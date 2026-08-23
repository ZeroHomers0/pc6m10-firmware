"""独立修复验证：直接执行原固件与新 ELF 的关键函数，不调用 test/ 现有套件。"""
from pathlib import Path
import re
import struct
import subprocess

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_SP

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
                       (0x40090000, 0x1000), (0x4009C000, 0x1000)):
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
