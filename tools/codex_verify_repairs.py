"""独立修复验证：直接执行原固件与新 ELF 的关键函数，不调用 test/ 现有套件。"""
from pathlib import Path
import re
import struct
import subprocess

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_SP, UC_ARM_REG_R0,
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / "firmware"
TC = Path(r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin")
NM = TC / "arm-none-eabi-nm.exe"
OBJDUMP = TC / "arm-none-eabi-objdump.exe"


def symbols():
    output = subprocess.check_output([NM, "-n", FW / "firmware.elf"], text=True)
    return {m.group(2): int(m.group(1), 16) for line in output.splitlines()
            if (m := re.match(r"([0-9a-fA-F]+)\s+\w\s+(\S+)$", line))}


def sections():
    output = subprocess.check_output([OBJDUMP, "-h", FW / "firmware.elf"], text=True)
    result = {}
    for line in output.splitlines():
        m = re.match(r"\s*\d+\s+(\.\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            result[m.group(1)] = tuple(int(m.group(i), 16) for i in range(2, 5))
    return result


SYMS = symbols()
SECS = sections()
ORIGINAL = (ROOT / "LPC1765.bin").read_bytes()
NEW = (FW / "firmware.bin").read_bytes()


def machine(new: bool):
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    for base, size in ((0, 0x40000), (0x10000000, 0x10000), (0x2007C000, 0x4000),
                       (0x2009C000, 0x1000), (0x40008000, 0x1000),
                       (0x40090000, 0x1000), (0x4009C000, 0x1000),
                       (0x400FC000, 0x2000), (0xE000E000, 0x2000)):
        uc.mem_map(base, size)
    uc.mem_write(0, NEW if new else ORIGINAL)
    image = (ROOT / "docs/_data_image.bin").read_bytes()
    uc.mem_write(0x10000000, image)
    if new:
        data_size, data_vma, data_lma = SECS[".data"]
        uc.mem_write(data_vma, NEW[data_lma:data_lma + data_size])
    uc.reg_write(UC_ARM_REG_SP, 0x10007000)
    uc.reg_write(UC_ARM_REG_LR, 0x3FF01)
    return uc


def run(uc, entry, writes=None, max_insn=2_000_000):
    def stop_at_return(machine, address, size, user):
        if address == 0x3FF00:
            machine.emu_stop()
    uc.hook_add(UC_HOOK_CODE, stop_at_return)
    if writes is not None:
        uc.hook_add(UC_HOOK_MEM_WRITE,
                    lambda machine, access, address, size, value, user: writes.append((address, size, value)),
                    begin=0x2009C000, end=0x2009CFFF)
    uc.emu_start(entry | 1, 0, count=max_insn)


def verify_uart_rx():
    for state in (0, 1):
        snapshots = []
        for is_new, entry in ((False, 0xAED0), (True, SYMS["func_0x0000aed0"])):
            uc = machine(is_new)
            uc.mem_write(0x10001790, bytes((state, 7, 3)))
            uc.mem_write(0x4009C000, b"\xA5")
            run(uc, entry, max_insn=500)
            snapshots.append(uc.mem_read(0x10001790, 3) + uc.mem_read(0x100022A4, 8))
        assert snapshots[0] == snapshots[1], f"UART RX state={state} mismatch"
    print("UART_RX: PASS")


def verify_gpio_trace(old_entry, new_name, label):
    traces = []
    for is_new, entry in ((False, old_entry), (True, SYMS[new_name])):
        uc = machine(is_new)
        # 非零种子可同时检查 RMW 掩码与写序。
        for offset in range(0, 0xA0, 4):
            uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x01010101 ^ offset))
        trace = []
        run(uc, entry, trace)
        traces.append(trace)
    assert traces[0] == traces[1], f"{label} MMIO trace mismatch: {len(traces[0])}/{len(traces[1])}"
    print(f"{label}: PASS writes={len(traces[0])}")


def verify_interrupt_trace(old_entry, new_name, label):
    traces = []
    states = []
    ranges = ((0x2009C000, 0x2009CFFF), (0x40008000, 0x40008FFF),
              (0x40090000, 0x40090FFF), (0x400FC000, 0x400FDFFF),
              (0xE000E000, 0xE000FFFF))
    for is_new, entry in ((False, old_entry), (True, SYMS[new_name])):
        uc = machine(is_new)
        trace = []
        callback = lambda machine, access, address, size, value, user: trace.append((address, size, value))
        for begin, end in ranges:
            uc.hook_add(UC_HOOK_MEM_WRITE, callback, begin=begin, end=end)
        run(uc, entry, max_insn=2_000_000)
        traces.append(trace)
        states.append(bytes(uc.mem_read(0x10000000, 0x2200)))
    assert traces[0] == traces[1], f"{label} peripheral trace mismatch: {len(traces[0])}/{len(traces[1])}"
    assert states[0] == states[1], f"{label} SRAM mismatch"
    print(f"{label}: PASS writes={len(traces[0])}")


def execute_pair(old_entry, new_name, args=(), setup=None, max_insn=2_000_000):
    results = []
    regs = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
    for is_new, entry in ((False, old_entry), (True, SYMS[new_name])):
        uc = machine(is_new)
        if setup:
            setup(uc)
        for reg, value in zip(regs, args):
            uc.reg_write(reg, value & 0xFFFFFFFF)
        run(uc, entry, max_insn=max_insn)
        snapshot = bytes(uc.mem_read(0x10000000, 0x2200)) + bytes(uc.mem_read(0x10003000, 0x100))
        results.append((uc.reg_read(UC_ARM_REG_R0), snapshot))
    return results


def verify_timer1_matrix():
    cases = 0
    for scan in (0, 1, 38, 39, 40, 41, 78, 79, 80, 118, 119, 120,
                 158, 159, 160, 198, 199, 200, 238, 239, 240):
        for mode in (0, 1, 2):
            for freq in (50, 60):
                traces = []
                states = []
                for is_new, entry in ((False, 0xFF6C), (True, SYMS["TIMER1_IRQHandler"])):
                    uc = machine(is_new)
                    uc.mem_write(0x10002074, bytes((scan, mode)))
                    uc.mem_write(0x10001FF8, bytes((freq,)))
                    for offset in range(0, 0xA0, 4):
                        uc.mem_write(0x2009C000 + offset, struct.pack("<I", 0x13570000 ^ offset))
                    trace = []
                    run(uc, entry, trace)
                    traces.append(trace)
                    states.append(bytes(uc.mem_read(0x10002074, 2)))
                assert traces[0] == traces[1], f"TIMER1 trace mismatch scan={scan} mode={mode} freq={freq}"
                assert states[0] == states[1], f"TIMER1 state mismatch scan={scan} mode={mode} freq={freq}"
                cases += 1
    print(f"TIMER1_MATRIX: PASS cases={cases}")


def verify_crc_matrix():
    vectors = (b"", b"\x00", b"\x01\x03\x00\x00\x00\x0a", bytes(range(32)),
               bytes((i * 73 + 19) & 0xFF for i in range(255)))
    for payload in vectors:
        def setup(uc, data=payload):
            uc.mem_write(0x10003000, data or b"\x00")
        results = execute_pair(0xAF64, "crc16", (0x10003000, len(payload)), setup)
        assert results[0][0] == results[1][0], f"CRC mismatch len={len(payload)}"
    print(f"CRC_MATRIX: PASS cases={len(vectors)}")


def verify_modbus_regs():
    read_cases = 0
    for reg in range(65):
        def setup(uc):
            uc.mem_write(0x10003000, b"\xA5\x5A\xC3\x3C")
        results = execute_pair(0xAF94, "modbus_read_reg", (0x10003000, reg), setup)
        assert results[0][0] == results[1][0], f"read return mismatch reg={reg}"
        assert results[0][1] == results[1][1], f"read RAM mismatch reg={reg}"
        read_cases += 1

    write_cases = 0
    for reg in range(64):
        for value in (0, 1, 0x55, 0x1234, 0xFFFF):
            def setup(uc, v=value):
                uc.mem_write(0x10003000, struct.pack("<I", v))
            results = execute_pair(0xB2E0, "modbus_write_multi", (0x10003000, reg), setup)
            assert results[0][0] == results[1][0], f"write return mismatch reg={reg} value={value}"
            assert results[0][1] == results[1][1], f"write RAM mismatch reg={reg} value={value}"
            write_cases += 1
    print(f"MODBUS_REGS: PASS read={read_cases} write={write_cases}")


def verify_closed_loop():
    cases = ((0, 0, 1, 1), (100, 90, 2, 3), (90, 100, 2, 3),
             (0x7FFFFFFF, 1, 7, 11), (1, 0x7FFFFFFF, 7, 11),
             (5000, 4999, 0x100, 0x200))
    for args in cases:
        results = execute_pair(0x108B0, "closed_loop_integral", args, max_insn=500_000)
        assert results[0][0] == results[1][0], f"closed-loop return mismatch args={args}"
        assert results[0][1] == results[1][1], f"closed-loop RAM mismatch args={args}"
    print(f"CLOSED_LOOP: PASS cases={len(cases)}")


def verify_vector():
    words = struct.unpack_from("<8I", NEW)
    assert sum(words) & 0xFFFFFFFF == 0
    assert words[0] == 0x100029C8
    print("VECTOR: PASS")


if __name__ == "__main__":
    verify_vector()
    verify_uart_rx()
    verify_gpio_trace(0xE5A8, "pin_config", "PIN_CONFIG")
    verify_gpio_trace(0x1064C, "gpio2_init", "GPIO2_INIT")
    verify_gpio_trace(0x106A0, "auth_challenge", "AUTH_CHALLENGE")
    verify_gpio_trace(0xFF6C, "TIMER1_IRQHandler", "TIMER1_ISR")
    verify_interrupt_trace(0xF9E8, "EINT1_IRQHandler", "EINT1_ISR")
    verify_interrupt_trace(0xFA0A, "EINT2_IRQHandler", "EINT2_ISR")
    verify_interrupt_trace(0xFF48, "TIMER2_IRQHandler", "TIMER2_ISR")
    verify_interrupt_trace(0xFA2C, "EINT3_IRQHandler", "EINT3_ISR")
    verify_timer1_matrix()
    verify_crc_matrix()
    verify_modbus_regs()
    verify_closed_loop()
