"""2026-11-16 独立只读审计；差异是审计发现，不是已通过的等价证明。"""
import os
from pathlib import Path
os.chdir(Path(__file__).resolve().parents[3])
import sys,struct
sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *
for menu in (0xa,0x62):
 out=[]
 for new in (False,True):
  u=V.machine(new);u.mem_map(0x40000000,0x1000)
  u.mem_write(0x10001744,bytes((menu,0,0)));u.mem_write(0x100015f2,b'\xff'*6)
  u.mem_write(0x1000161c,struct.pack('<II',77,88))
  u.mem_write(0x10001624,b'\0'*4);u.mem_write(0x10001628,b'\0')
  for a in (0x2009c014,0x2009c034):u.mem_write(a,b'\xff'*4)
  feed=V.SYMS['wd_feed'] if new else 0x238;feeds=[]
  skip={a for n,a in V.SYMS.items() if n.startswith('disp_')} if new else {0x992,0xd3c,0x427c}
  def hook(m,a,n,x):
   # 仅省略OLD的空转内层，仍逐次执行外层计数与喂狗。
   if not new and a in (0x5486,0x55c8):
    m.reg_write(UC_ARM_REG_R0,1000);m.mem_write(0x10001620,struct.pack('<I',1000))
   if a==feed:feeds.append(1)
   if a in skip or a==feed:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
  u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_R0,1)
  u.emu_start((V.SYMS['state_machine'] if new else 0x458c)|1,0x3ff00,count=2000000)
  out.append((hex(u.reg_read(UC_ARM_REG_PC)),u.mem_read(0x10001744,1)[0],struct.unpack('<II',u.mem_read(0x1000161c,8)),len(feeds)))
 print(hex(menu),'OLD/NEW (PC,page,(outer,inner),feed_count)',out)
