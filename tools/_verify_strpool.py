# -*- coding: utf-8 -*-
# _verify_strpool.py — 验证重链接产物 firmware.bin 的 strpool 簇字节 == 原 bin 字符串字节
# 2026-08-21 目标B W7a 验证。读 firmware.bin 中 strpool_clusters 表 + strpool_blob，
#   对每簇逐字节对比原 LPC1765.bin [base, base+len)。全 PASS 即端到端确认
#   （strpool_map 仅是 blob 内指针运算，簇内容对 = 显示字符串对）。
import struct, sys
sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"D:\code\LPC1765FBD100\decompiled"
FB = open(ROOT + r"\firmware\firmware.bin", "rb").read()
OB = open(ROOT + r"\LPC1765.bin", "rb").read()

BLOB_ADDR = 0xbcbc     # strpool_blob 符号 VMA（=firmware.bin 偏移，flash base 0）
TABLE_ADDR = 0xbbcc    # strpool_clusters 符号 VMA
BLOB_LEN = 2507        # gen_strpool 报告的 blob 字节数（含每簇尾 NUL）

def rd(a, n):
    if a + n > len(FB):
        return None
    return FB[a:a + n]

# 读簇表（每条 12 字节：base u32 / len u32 / blob_ptr u32）
nrec = 20   # gen_strpool 报告簇数；簇表结束于 0xbcbc（=strpool_blob 起点）
print("簇表条目数: %d" % nrec)

def u32(b, i):
    return struct.unpack("<I", b[i:i + 4])[0]

ok = 0; fail = 0
for i in range(nrec):
    base = u32(FB, TABLE_ADDR + i * 12)
    ln = u32(FB, TABLE_ADDR + i * 12 + 4)
    blobptr = u32(FB, TABLE_ADDR + i * 12 + 8)
    off = blobptr - BLOB_ADDR          # 该簇在 blob 内的偏移
    fw = rd(BLOB_ADDR + off, ln)       # firmware.bin 中该簇字节
    ob = OB[base:base + ln]            # 原 bin 对应字节
    if fw is None:
        print("  0x%04X len=%d 读取越界(firmware.bin 长度 %d)" % (base, ln, len(FB)))
        fail += 1
        continue
    if fw == ob:
        ok += 1
    else:
        fail += 1
        print("  FAIL 0x%04X len=%d 首词 fw=%02X%02X%02X%02X ob=%02X%02X%02X%02X"
              % (base, ln, fw[0], fw[1], fw[2], fw[3], ob[0], ob[1], ob[2], ob[3]))

print("簇字节对比: PASS=%d FAIL=%d （共 %d 簇）" % (ok, fail, nrec))

# 抽样：模拟 strpool_map(0x47dc) 读 "故障"，strpool_map(0x86e0) 读 '%'，RAM 原样
def map_addr(addr):
    for i in range(nrec):
        base = u32(FB, TABLE_ADDR + i * 12)
        ln = u32(FB, TABLE_ADDR + i * 12 + 4)
        blobptr = u32(FB, TABLE_ADDR + i * 12 + 8)
        if base <= addr < base + ln:
            return BLOB_ADDR + (blobptr - BLOB_ADDR) + (addr - base)
    return addr

def readc(a, n):
    return FB[a:a + n] if a + n <= len(FB) else b""

s = readc(map_addr(0x47dc), 8)     # "故障"?
print("map(0x47dc) -> '%s' (GBK %s)" % (s[:4].decode('gbk', errors='replace'), s[:4].hex()))
s = readc(map_addr(0x86e0), 4)     # '%'
print("map(0x86e0) -> [%s] '%s'" % (s[:1].hex(), s[:1].decode('latin1')))
ra = map_addr(0x100015cc)          # 应原样返回（RAM）
print("map(0x100015cc) -> 0x%04X (期望 0x100015cc %s)" % (ra, "OK" if ra == 0x100015cc else "FAIL"))
