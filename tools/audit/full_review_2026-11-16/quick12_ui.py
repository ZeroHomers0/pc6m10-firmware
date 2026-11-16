"""2026-11-16 独立只读审计；差异是审计发现，不是已通过的等价证明。"""
import os
from pathlib import Path
os.chdir(r'D:\code\LPC1765FBD100\PC12M-2')
import sys,struct
sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence_12 as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *
for menu,key,edit in [(1,14,0),(0x63,22,1),(0x63,33,1),(0x63,4,0)]:
 for full in (False,):
  pair=[]
  for new in (False,True):
   u=V.machine(new);V._seed_display_items(u)
   u.mem_write(0x10001724,bytes((menu,0,edit)))
   u.mem_write(0x10001690,struct.pack('<I',4000));u.mem_write(0x10001740,struct.pack('<III',27,0,73))
   u.mem_write(0x1000161c,b'\0'*4);u.mem_write(0x10001620,b'\0')
   for a in (0x2009c014,0x2009c034):u.mem_write(a,b'\xff'*4)
   if new:
    skip={V.SYMS['Delay'],V.SYMS['param_sync_live_to_eeprom']}
    if not full:skip|={a for n,a in V.SYMS.items() if n.startswith('disp_')}
   else:skip={0x758,0x3534}|(set() if full else {0xcec,0x40b0,0x942,0xe80,0xdf2,0x13ec})
   def hook(m,a,n,x):
    if a in skip:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
   u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_R0,key)
   u.emu_start((V.SYMS['state_machine'] if new else 0x4464)|1,0x3ff00,count=8000000)
   assert u.reg_read(UC_ARM_REG_PC)==0x3ff00
   pair.append((u.mem_read(0x10001724,1)[0],struct.unpack('<I',u.mem_read(0x10001690,4))[0],struct.unpack('<III',u.mem_read(0x10001740,12)),struct.unpack('<I',u.mem_read(0x10001758,4))[0]))
  print('menu/key/edit/full',menu,key,edit,full,'old/new (page,calibration,(countdown,idle,refresh),blink)',pair,flush=True)
