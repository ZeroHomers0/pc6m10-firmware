"""PID分段边界：只读执行原BIN/当前ELF。"""
from pathlib import Path
import os,sys,struct,itertools
os.chdir(Path(__file__).resolve().parents[3]);sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence as V
from unicorn.arm_const import *
errors=(0,1,9,10,11,29,30,31,219,220,221,548,549,550,999,1000,1001,1499,1500,1501,1999,2000,2001,2499,2500,2501,2999,3000,3001,3999,4000,4001,4999,5000,5001,5999,6000)
bad=[];count=0
for error,reverse,mode,coeff in itertools.product(errors,(False,True),(0,1,2),((1,1),(5,3),(128,128))):
 args=(error if not reverse else 0,0 if not reverse else error,*coeff);pair=[]
 for new in (False,True):
  u=V.machine(new);u.mem_write(0x10001634,bytes([mode]))
  for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):u.reg_write(r,v)
  V.run(u,V.SYMS['closed_loop_integral'] if new else 0x108b0,max_insn=100000)
  pair.append((u.reg_read(UC_ARM_REG_PC)==0x3ff00,u.reg_read(UC_ARM_REG_R0),bytes(u.mem_read(0x10000000,0x2200))))
 count+=1
 if pair[0]!=pair[1]:bad.append((error,reverse,mode,coeff))
print('PID_BOUNDARY',count,'DIFF',len(bad),bad[:20])
