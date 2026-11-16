"""2026-11-16 独立只读审计；差异是审计发现，不是已通过的等价证明。"""
import os
from pathlib import Path
os.chdir(Path(__file__).resolve().parents[3])
import sys,struct,itertools,json
from pathlib import Path
sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *
oldnames={n:a for a,o,z,n,f in json.loads(Path(__file__).with_name('functions.json').read_text())}
oldnames.update(param_sync_live_to_eeprom=0x35f2,output_stage=0xe9ac)
menus=(1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0x14,0x1e,0x5a,0x62,0x63)
keys=(0,1,2,3,4,5,6,0xb,0xe,0x16,0x17,0x21)
failed=[];unfinished=[];total=0
for menu,item,edit,key in itertools.product(menus,(0,1,3),(0,1),keys):
 pair=[]
 for new in (False,True):
  u=V.machine(new)
  for base in (0x40000000,0x40004000,0x4002c000,0x40034000):u.mem_map(base,0x1000)
  u.mem_write(0x40034004,struct.pack("<I",0x80000000))
  V._seed_display_items(u)
  u.mem_write(0x10001744,bytes((menu,item,edit)))
  u.mem_write(0x10001624,b"\0"*4);u.mem_write(0x10001628,b"\0")
  u.mem_write(0x10001660,struct.pack('<I',90))
  for a in (0x2009c014,0x2009c034):u.mem_write(a,b"\xff"*4)
  names=[n for n in oldnames if n.startswith('disp_')]+['Delay','param_sync_live_to_eeprom','i2c_write_reg','i2c_delay','auth_challenge','auth_retry','adc0_scan_channels']
  skip={(V.SYMS[n] if new else oldnames[n]) for n in names if n in V.SYMS and n in oldnames}
  def hook(m,a,n,x):
   if a in skip:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
  u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_R0,key)
  try:
   u.emu_start((V.SYMS['state_machine'] if new else 0x458c)|1,0x3ff00,count=200000)
   pair.append((u.reg_read(UC_ARM_REG_PC)==0x3ff00,bytes(u.mem_read(0x10000000,0x29c8))))
  except Exception as e:
   pair.append(('ERROR',str(e)))
   if total<500:print('EXEC_ERROR',menu,item,edit,key,new,str(e),flush=True)
 total+=1
 if pair[0][0]!=True or pair[1][0]!=True:unfinished.append((menu,item,edit,key,pair[0][0],pair[1][0]));continue
 if pair[0]!=pair[1]:
  diffs=[(hex(i+0x10000000),a,b) for i,(a,b) in enumerate(zip(pair[0][1],pair[1][1])) if a!=b]
  failed.append((menu,item,edit,key,diffs[:8]))
 if total%72==0:print('menu',hex(menu),'cases',total,'failed',len(failed),'unfinished',len(unfinished),flush=True)
print('TOTAL',total,'FAIL',len(failed),'UNFINISHED',len(unfinished))
print('DIFFS',failed)
print('UNFINISHED_CASES',unfinished)
