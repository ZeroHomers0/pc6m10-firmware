"""八类审计修复的原 BIN 执行回归；不连接硬件。"""
import contextlib
import io
from pathlib import Path
import re
import runpy
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R3

_PASSWORD_INNER_SITES={}

def completed_menu_machine(base_machine, new):
    """只加速密码延时内层；保留10000轮外层、真实喂狗与最终计数。"""
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
    u = base_machine(new)
    if new in _PASSWORD_INNER_SITES:
        sites=_PASSWORD_INNER_SITES[new]
    elif new:
        dis = subprocess.check_output([V.OBJDUMP, '-d', ROOT/'firmware/firmware.elf'], text=True)
        block = dis.split('<delay_for_password_feedback>:', 1)[1].split('\n\n', 1)[0]
        match = re.search(r'^\s*([0-9a-f]+):.*cmp\.w\s+r3, #1000', block, re.M)
        assert match, '密码延时编译布局变化'
        sites = {int(match[1],16)}
    else:
        sites = {a-0x18 for a in (0x549e,0x55e0,0x8c88,0x8d40,0x8d8a,0x8dd4,0x8e56,0x8f0c,0x8fda,0x9026)}
        cs = Cs(CS_ARCH_ARM,CS_MODE_THUMB)
        for a in sites:
            ins = next(cs.disasm(V.ORIGINAL[a:a+4],a))
            assert ins.mnemonic.startswith('cmp') and ins.op_str in ('r0, r1','r0, #0x3e8'), ('原机内层比较点变化',hex(a),ins.mnemonic,ins.op_str)
    _PASSWORD_INNER_SITES[new]=sites
    def inner(m,a,n,user):
        if a in sites:
            m.mem_write(0x10001620,struct.pack('<I',1000))
            m.reg_write(UC_ARM_REG_R3 if new else UC_ARM_REG_R0,1000)
    u.hook_add(UC_HOOK_CODE,inner)
    start = u.emu_start
    def bounded(*args,**kwargs):
        kwargs['count'] = max(kwargs.get('count',0),2_000_000)
        return start(*args,**kwargs)
    u.emu_start = bounded
    return u


def main():
    audit = ROOT / 'tools/audit/full_review_2026-11-16'
    # 把审计器的结果转为真正的断言，非返回命令单独验证。
    for script, bad, pending in [('audit_modbus_single_write.py', 'bad', 2),
                                 ('audit_all_menu_states.py', 'failed', 0)]:
        base_machine=V.machine
        if script=='audit_all_menu_states.py':
            V.machine=lambda new: completed_menu_machine(base_machine,new)
        with contextlib.redirect_stdout(io.StringIO()):
            try: result = runpy.run_path(str(audit / script))
            finally: V.machine=base_machine
        assert not result[bad], result[bad][:5]
        incomplete = result.get('incomplete', result.get('unfinished'))
        assert len(incomplete) == pending, incomplete
        assert all(x[-2:] == (False, False) for x in incomplete), incomplete
        if pending:
            assert all(x[0] in (0x25,0x26) and x[1]==0 for x in incomplete),incomplete
            print(script, 'PASS_EQ正常返回；EXPECTED_NONRETURN复位命令', pending)
        else:
            print(script, '所有有界菜单实际返回；原 BIN 差分无差异')

    # 只加速纯空转内层，外层10000次和每次喂狗仍真实执行。
    dis = subprocess.check_output([V.OBJDUMP, '-d', ROOT/'firmware/firmware.elf'], text=True)
    block = dis.split('<delay_for_password_feedback>:', 1)[1].split('\n\n', 1)[0]
    match = re.search(r'^\s*([0-9a-f]+):.*cmp\.w\s+r3, #1000', block, re.M)
    assert match, '编译器指令变化，需重新核对纯空转内层加速点'
    compare = int(match[1], 16)
    for menu in (0xa, 0x62):
        pair = []
        for new in (False, True):
            u = V.machine(new)
            u.mem_write(0x10001744, bytes((menu, 0, 0)))
            u.mem_write(0x100015f2, b'\xff'*6)
            u.mem_write(0x1000161c, struct.pack('<II', 77, 88))
            feeds = []
            feed = V.SYMS['wd_feed'] if new else 0x238
            skip = {a for n, a in V.SYMS.items() if n.startswith('disp_')} if new else {0x992, 0xd3c, 0x427c}
            def hook(m, a, size, user):
                if a == (compare if new else (0x5486 if menu == 0xa else 0x55c8)):
                    m.mem_write(0x10001620, struct.pack('<I', 1000))
                    m.reg_write(UC_ARM_REG_R3 if new else UC_ARM_REG_R0, 1000)
                if a == feed:
                    feeds.append(1)
                if a in skip or a == feed:
                    m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
            u.hook_add(UC_HOOK_CODE, hook)
            u.reg_write(UC_ARM_REG_R0, 1)
            u.emu_start((V.SYMS['state_machine'] if new else 0x458c)|1, 0x3ff00, count=2000000)
            assert u.reg_read(UC_ARM_REG_PC) == 0x3ff00
            pair.append((bytes(u.mem_read(0x1000161c, 8)), len(feeds), u.mem_read(0x10001744, 1)[0]))
        assert pair[0] == pair[1] == (struct.pack('<II', 10000, 1000), 10000, 1), pair
        print('密码错误页', hex(menu), '原 BIN/ELF 外层10000、内层1000、喂狗10000一致')

    # 两条命令必须先应答，再按原顺序写EEPROM，并保持非返回。
    for reg in (0x25, 0x26):
        pair = []
        for new in (False, True):
            u = V.machine(new)
            body = bytes((1, 6, 0x10, reg, 0, 0))
            crc = 0xffff
            for byte in body:
                crc ^= byte
                for _ in range(8):
                    crc = (crc >> 1)^0xa001 if crc & 1 else crc >> 1
            u.mem_write(0x100016ff, b'\1')
            u.mem_write(0x10001790, b'\5')
            u.mem_write(0x10001792, b'\10')
            u.mem_write(0x100022a4, body+struct.pack('<H', crc))
            tx = V.SYMS['uart3_tx_byte'] if new else 0xae0c
            i2c = V.SYMS['i2c_write_reg'] if new else 0x1e88
            events = []
            def hook(m, a, size, user):
                if a == tx:
                    events.append(('tx', bytes(m.mem_read(0x1000236c, m.reg_read(UC_ARM_REG_R0)))))
                if a == i2c:
                    events.append(('eeprom', m.reg_read(UC_ARM_REG_R0), m.reg_read(UC_ARM_REG_R1)))
                if a in (tx, i2c):
                    m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
            u.hook_add(UC_HOOK_CODE, hook)
            u.emu_start((V.SYMS['modbus_dispatch'] if new else 0xb642)|1, 0x3ff00, count=10000)
            assert u.reg_read(UC_ARM_REG_PC) != 0x3ff00
            # 自跳指令固定点，排除仅因普通长路径用尽预算。
            pc = u.reg_read(UC_ARM_REG_PC)
            assert bytes(u.mem_read(pc, 2)) == b'\xfe\xe7'
            pair.append(events)
        expected = [('tx', body+struct.pack('<H', crc))]
        if reg == 0x26:
            expected += [('eeprom', 0, 5), ('eeprom', 0, 6)]
        assert pair[0] == pair[1] == expected, pair
        print('复位命令', hex(reg), '响应/EEPROM顺序/自跳等待与原 BIN 一致')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
