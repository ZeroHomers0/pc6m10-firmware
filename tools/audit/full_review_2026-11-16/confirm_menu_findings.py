"""2026-11-16 独立只读审计；差异是审计发现，不是已通过的等价证明。"""
import os
from pathlib import Path
os.chdir(Path(__file__).resolve().parents[3])
import sys,struct
sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *
for menu,key,edit in [(1,14,0),(0x63,22,1),(0x63,33,1),(0x63,4,0)]:
 for full in (False,True):
  pair=[]
  for new in (False,True):
   u=V.machine(new);V._seed_display_items(u)
   u.mem_write(0x10001744,bytes((menu,0,edit)))
   u.mem_write(0x10001698,struct.pack('<I',4000));u.mem_write(0x10001760,struct.pack('<III',27,0,73))
   u.mem_write(0x10001624,b'\0'*4);u.mem_write(0x10001628,b'\0')
   for a in (0x2009c014,0x2009c034):u.mem_write(a,b'\xff'*4)
   if new:
    skip={V.SYMS['Delay'],V.SYMS['param_sync_live_to_eeprom']}
    if not full:skip|={a for n,a in V.SYMS.items() if n.startswith('disp_')}
   else:skip={0x7a8,0x35f2}|(set() if full else {0xd3c,0x427c,0x992,0xed0,0xe42,0x143c})
   def hook(m,a,n,x):
    if a in skip:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
   u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_R0,key)
   u.emu_start((V.SYMS['state_machine'] if new else 0x458c)|1,0x3ff00,count=8000000)
   assert u.reg_read(UC_ARM_REG_PC)==0x3ff00
   pair.append((u.mem_read(0x10001744,1)[0],struct.unpack('<I',u.mem_read(0x10001698,4))[0],struct.unpack('<III',u.mem_read(0x10001760,12)),struct.unpack('<I',u.mem_read(0x10001778,4))[0]))
  print('menu/key/edit/full',menu,key,edit,full,'old/new (page,calibration,(countdown,idle,refresh),blink)',pair,flush=True)
