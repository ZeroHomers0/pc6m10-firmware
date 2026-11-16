"""2026-11-16 独立只读审计；差异是审计发现，不是已通过的等价证明。"""
import os
from pathlib import Path
os.chdir(r'D:\code\LPC1765FBD100\PC12M-2')
import sys,struct,itertools
sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence_12 as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *
def crc(b):
 v=0xffff
 for x in b:
  v^=x
  for _ in range(8):v=(v>>1)^0xa001 if v&1 else v>>1
 return v
values=(0,1,2,3,4,5,7,8,9,10,40,99,100,101,127,128,129,159,160,161,179,180,181,247,248,255,256,3499,3500,4499,4500,4501,5999,6000,6001,65535)
bad=[];incomplete=[];count=0
for reg,val in itertools.product((0x18,0x19,0x1b,0x1c,0x1d,0x25,0x26,0x28,0x33,0x34,0x35,0x36,0x37),values):
 body=bytes((1,6,0x10,reg,val>>8,val&255));c=crc(body);frame=body+bytes((c&255,c>>8));pair=[]
 for new in (False,True):
  u=V.machine(new);u.mem_write(0x100016f7,b'\1');u.mem_write(0x10001770,b'\5');u.mem_write(0x10001772,b'\10');u.mem_write(0x10002278,frame)
  tx=V.SYMS['uart3_tx_byte'] if new else 0xab7c;sync=V.SYMS['param_sync_live_to_eeprom'] if new else 0x3534;i2c=V.SYMS['i2c_write_reg'] if new else 0x1e38;counts=[]
  def hook(m,a,n,x):
   if a==tx:counts.append(m.reg_read(UC_ARM_REG_R0))
   if a in (tx,sync,i2c):m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
  u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_ARM_REG_R0,0)
  try:
   u.emu_start((V.SYMS['modbus_dispatch'] if new else 0xb3b2)|1,0x3ff00,count=100000)
   pair.append((u.reg_read(UC_ARM_REG_PC)==0x3ff00,counts,bytes(u.mem_read(0x1000161c,0x108))+bytes(u.mem_read(0x1000175c,0x10))+bytes(u.mem_read(0x10002340,counts[-1] if counts else 0))))
  except Exception as e:pair.append(('ERROR',str(e)))
 count+=1
 if pair[0][0]!=True or pair[1][0]!=True:incomplete.append((reg,val,pair[0][0],pair[1][0]));continue
 if pair[0]!=pair[1]:
  diff=[(hex(0x10000000+i),a,b) for i,(a,b) in enumerate(zip(pair[0][2],pair[1][2])) if a!=b][:5]
  bad.append((reg,val,pair[0][1],pair[1][1],diff))
 if reg%8==7 and val==65535:print('progress',count,'bad',len(bad),'incomplete',len(incomplete),flush=True)

from collections import Counter
print('REG_GROUPS',dict(Counter(x[0] for x in bad)))
print('PER_REG_EXAMPLE',[next(x for x in bad if x[0]==r) for r in sorted(set(x[0] for x in bad))])
print('TOTAL',count,'BAD',len(bad),'INCOMPLETE',len(incomplete));print('DIFFS',bad[:30]);print('INCOMPLETE_CASES',incomplete)
