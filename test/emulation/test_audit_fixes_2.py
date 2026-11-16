"""2026-11-16 第二轮审计（F1-F4）的原 BIN 执行回归；不连接硬件。"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1

FIO_RANGES = ((0x2009C000, 0x2009CFFF), (0x40008000, 0x40008FFF),
              (0x40090000, 0x40090FFF), (0x400FC000, 0x400FDFFF))


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def run_modbus(frame, seed=None):
    """返回 (响应字节, 0x100017A8 末值)；响应为空表示未发送。"""
    body = bytes(frame)
    wire = body + struct.pack('<H', crc16(body))
    results = []
    for new in (False, True):
        uc = V.machine(new)
        if seed:
            for address, data in seed:
                uc.mem_write(address, data)
        uc.mem_write(0x100016FF, b'\x01')
        uc.mem_write(0x10001790, b'\x05')
        uc.mem_write(0x10001792, bytes((len(wire),)))
        uc.mem_write(0x100022A4, wire)
        sent = []
        tx = V.SYMS['uart3_tx_byte'] if new else 0xAE0C
        def hook(m, address, size, user):
            if address == tx:
                sent.append(bytes(m.mem_read(0x1000236C, m.reg_read(UC_ARM_REG_R0))))
                m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
        uc.hook_add(UC_HOOK_CODE, hook)
        V.run(uc, V.SYMS['modbus_dispatch'] if new else 0xB642, max_insn=200000)
        assert uc.reg_read(UC_ARM_REG_PC) == 0x3FF00, '通信路径未正常返回'
        results.append((sent[0] if sent else b'', bytes(uc.mem_read(0x100017A8, 1))))
    return results


def run_output_stage(seed):
    """返回 (外设写迹, 0x10000000..0x10002200 快照)。"""
    results = []
    for new in (False, True):
        uc = V.machine(new)
        for address, data in seed:
            uc.mem_write(address, data)
        uc.mem_write(0x10002078, b'\x0A')
        trace = []
        callback = lambda m, access, address, size, value, user: trace.append((address, size, value))
        for begin, end in FIO_RANGES:
            uc.hook_add(UC_HOOK_MEM_WRITE, callback, begin=begin, end=end)
        V.run(uc, V.SYMS['output_stage'] if new else 0xE9AC, max_insn=3000000)
        assert uc.reg_read(UC_ARM_REG_PC) == 0x3FF00, '输出级路径未正常返回'
        results.append((trace, bytes(uc.mem_read(0x10000000, 0x2200))))
    return results


def word(address):
    return struct.pack('<I', address)


def main():
    # F1：功能码正确但寄存器页不是 0x10xx 时，原 BIN 必须发异常响应，不能静默丢帧。
    for name, frame, code in (
            ('0x06', (0x01, 0x06, 0x00, 0x10, 0x00, 0x01), 0x86),
            ('0x10', (0x01, 0x10, 0x00, 0x10, 0x00, 0x01, 0x02, 0x00, 0x01), 0x90),
            ('0x03', (0x01, 0x03, 0x00, 0x10, 0x00, 0x01), 0x83)):
        old, new = run_modbus(frame)
        assert old[0] == new[0], f'{name} frame[2]!=0x10 响应不同 old={old[0].hex()} new={new[0].hex()}'
        assert old[0][:3] == bytes((0x01, code, 0x02)), f'{name} 异常码不是 02: {old[0].hex()}'
        print(f'F1 {name} 非 0x10 页异常响应一致 {old[0].hex()}')

    # F2：0x03 读循环原 BIN 迭代 count+2 次，末次暂存索引为 start+count+1。
    for start, count in ((1, 1), (0x0D, 3), (0x3E, 1)):
        frame = (0x01, 0x03, 0x10, start, 0x00, count)
        seed = [(0x10001000 + 4 * i, struct.pack('<I', 0x30 + i)) for i in range(0x40)]
        old, new = run_modbus(frame, seed)
        assert old[0] == new[0], f'0x03 响应不同 start={start} count={count}'
        assert old[1] == new[1], f'0x03 暂存索引不同 start={start} count={count} old={old[1].hex()} new={new[1].hex()}'
        assert old[1][0] == (start + count + 1) & 0xFF, f'0x03 末次索引 {old[1].hex()}'
        print(f'F2 0x03 start={start} count={count} 响应与末次索引一致')

    # F2b：全寄存器读矩阵。给参数区每个 4 字节字灌入互不相同的值，逐一起始地址比对，
    # 可同时验证「寄存器号基线 = start-1」「迭代 count+2 次」「暂存索引末值」三件事。
    read_seed = [(0x10001500 + 4 * i, struct.pack('<H', ((0x10001500 + 4 * i) * 2654435761 ^ ((0x10001500 + 4 * i) >> 3)) & 0xFFFF))
                 for i in range((0x10002000 - 0x10001500) // 4)]
    read_seed.append((0x10001710, b'\x01'))
    read_bad = []
    for start in range(1, 0x40):
        old, new = run_modbus((0x01, 0x03, 0x10, start, 0x00, 0x01), read_seed)
        if old != new:
            read_bad.append((start, old[0].hex(), new[0].hex(), old[1].hex(), new[1].hex()))
    assert not read_bad, f'0x03 读矩阵不一致 {read_bad[:3]}'
    print('F2b 0x03 读矩阵 start=1..0x3F 全部一致')

    # F3：运行联锁复位判据是 0x1000177D(run_request)，不是 0x1000203D(fault_state)。
    for request in (0, 1):
        seed = [(0x10001628, b'\x00'), (0x10001634, b'\x00'), (0x100015CE, word(0)),
                (0x10001660, word(90)), (0x10001650, word(0)), (0x10001782, b'\x00'),
                (0x10002000, word(1)), (0x1000203D, b'\x00'), (0x1000177D, bytes((request,))),
                (0x1000203C, b'\x00'), (0x10001624, word(0))]
        old, new = run_output_stage(seed)
        assert old[0] == new[0], f'F3 run_request={request} 外设写迹不同 {len(old[0])}/{len(new[0])}'
        assert old[1] == new[1], f'F3 run_request={request} SRAM 不同'
        assert (len(old[0]) == 28) == bool(request), f'F3 run_request={request} 复位写数 {len(old[0])}'
        print(f'F3 run_request={request} 写数={len(old[0])} 一致')

    # F4：恒压源分支用 0x1000202C，不得覆盖 aux_b(0x10002018)。
    # 原 BIN 0xEA50..0xEA5E 要求 output_mode==1 && adc_output_reference>=10 才进入
    # 0xEA60..0xF6B2 这一整块（保护 + control_mode 0/1/2 三支），故此处 output_mode=1。
    seed = [(0x10001628, b'\x01'), (0x10001634, b'\x02'), (0x100015CE, word(0)),
            (0x10001660, word(90)), (0x10001650, word(0)), (0x10001782, b'\x00'),
            (0x10002000, word(0)), (0x1000203D, b'\x00'), (0x1000177D, b'\x00'),
            (0x1000203C, b'\x01'), (0x100015A8, word(1234)), (0x1000164C, b'\x01'),
            (0x10002018, word(7)), (0x10002068, word(0)), (0x1000206C, word(0)),
            (0x10002070, word(0)), (0x10001FFC, word(0)), (0x10002004, word(90)),
            (0x10002008, word(180)), (0x10001624, word(0)),
            (0x1000163C, word(5000)), (0x10001638, word(5000)), (0x10001644, word(1000)),
            (0x10001648, word(600)), (0x100016C0, word(0)), (0x100016C4, word(0)),
            (0x100016C8, word(0)), (0x100016CC, word(0)), (0x100016D0, word(0)),
            (0x100016D4, b'\x00'), (0x1000207C, word(0)), (0x100020BC, word(0))]
    old, new = run_output_stage(seed)
    assert old[0] == new[0], f'F4 外设写迹不同 {len(old[0])}/{len(new[0])}'
    assert old[1] == new[1], 'F4 SRAM 不同'
    assert struct.unpack('<I', old[1][0x202C:0x2030])[0] == 1234, 'F4 未按原厂写 0x1000202C'
    assert struct.unpack('<I', old[1][0x2018:0x201C])[0] == 7, 'F4 aux_b 被覆盖'
    print('F4 恒压源分支写 0x1000202C、aux_b 保持初值，一致')

    # 第二路闭环：进入条件为 control_mode==1（原 BIN 0xF30C），计数用 aux_b(0x10002018)、
    # 标志用 aux_a(0x10002020)；反馈寄存器是 0x100015BC，限幅时从 0x100015B8 复制且不覆盖源。
    seed = [(0x100015A8, word(100)), (0x10001628, b'\x01'), (0x10001634, b'\x01'), (0x100015CE, word(0)),
            (0x10001660, word(90)), (0x10001650, word(0)), (0x10001782, b'\x00'),
            (0x10002000, word(5)), (0x1000203D, b'\x00'), (0x1000177D, b'\x00'),
            (0x1000203C, b'\x00'), (0x10002075, b'\x02'), (0x10002077, b'\x01'),
            (0x10002080, b'\x01'), (0x10002018, word(50)), (0x1000201C, word(1)),
            (0x10002020, word(2)), (0x10001590, word(3000)), (0x10001594, word(1)),
            (0x100015B8, word(3000)), (0x100015BC, word(1)), (0x100015D0, word(0)),
            (0x10001648, word(526)), (0x10001638, word(5000)), (0x1000163C, word(5000)),
            (0x10001644, word(1)), (0x10002058, word(6000)), (0x10001659, b'\x01'),
            (0x100015AC, word(0)), (0x10001624, word(0)), (0x1000207C, word(0)),
            (0x100016C0, word(0)), (0x100016C4, word(0)), (0x100020BC, word(0)),
            (0x100016D0, word(0)), (0x100016D4, b'\x00')]
    old, new = run_output_stage(seed)
    assert old[0] == new[0], f'第二路闭环外设写迹不同 {len(old[0])}/{len(new[0])}'
    assert old[1] == new[1], '第二路闭环 SRAM 不同'
    aux_b = struct.unpack('<I', old[1][0x2018:0x201C])[0]
    aux_a = struct.unpack('<I', old[1][0x2020:0x2024])[0]
    voltage_scaled = struct.unpack('<I', old[1][0x15B8:0x15BC])[0]
    assert aux_a in (1, 2), f'第二路闭环标志值异常 aux_a={aux_a}'
    assert aux_b <= 0x33, f'第二路闭环计数器越界 aux_b={aux_b}'
    assert voltage_scaled == 3000, f'第二路闭环误写 0x100015B8={voltage_scaled}'
    print(f'第二路闭环一致 aux_b={aux_b} aux_a={aux_a} 0x100015B8 未被改写')

    # 实际进入第二路闭环，逐条证明三组跨反馈单元复制均执行。
    scenarios = [
        ('量程比较路径', {0x10001638: 4000, 0x10001594: 100, 0x100015D0: 1000}, 0xF450),
        ('反馈比较路径', {0x1000163C: 5000, 0x10001594: 100, 0x100015D0: 4000}, 0xF4B6),
        ('低反馈路径', {0x1000163C: 5000, 0x10001594: 1, 0x100015D0: 0}, 0xF50A),
    ]
    for label, extra, store_pc in scenarios:
        for feedback in (1, 3001):
            for counter in (0, 49):
                initial = dict(seed)
                initial.update({0x100015A8: word(100), 0x10002018: word(counter),
                                0x10002020: word(1), 0x100015BC: word(feedback)})
                initial.update({address: word(value) for address, value in extra.items()})
                pair = []
                for new in (False, True):
                    uc = V.machine(new)
                    for address, data in initial.items():
                        uc.mem_write(address, data)
                    uc.mem_write(0x10002078, b'\x0A')
                    calls, stores = [], []
                    pid = V.SYMS['closed_loop_wrapper'] if new else 0x10F0A
                    def hook(m, address, size, user):
                        if address == pid:
                            calls.append((m.reg_read(UC_ARM_REG_R0), m.reg_read(UC_ARM_REG_R1)))
                        if not new and address == store_pc:
                            stores.append(address)
                    uc.hook_add(UC_HOOK_CODE, hook)
                    V.run(uc, V.SYMS['output_stage'] if new else 0xE9AC, max_insn=3000000)
                    assert uc.reg_read(UC_ARM_REG_PC) == 0x3FF00
                    assert calls, f'{label}未进入PID分支'
                    if not new:
                        assert stores, f'{label}原BIN未执行预期反馈复制'
                    assert calls[-1] == (526, 3000), (label, calls)
                    assert struct.unpack('<I', uc.mem_read(0x100015BC, 4))[0] == 3000
                    pair.append((calls, bytes(uc.mem_read(0x10000000, 0x2200))))
                assert pair[0] == pair[1], f'{label}反馈复制/PID/SRAM不同'
        print(f'{label}：4例实际执行原BIN复制指令，反馈/PID/SRAM一致')

    print('AUDIT_FIXES_2: 全通过')
    return 0


if __name__ == '__main__':
    sys.exit(main())
