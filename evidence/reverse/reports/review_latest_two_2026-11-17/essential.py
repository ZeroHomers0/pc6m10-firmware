from pathlib import Path
s=Path('tmp/review_latest_two/review.py').read_text(encoding='utf-8').split('latest=(V.FW,dict(V.SYMS),V.SECS,V.NEW)')[0]
exec(s)
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_R1,UC_ARM_REG_R0
latest=(V.FW,dict(V.SYMS),V.SECS,V.NEW)
for label in ('original','latest','candidate'):
 if label=='candidate':
  V.FW=ROOT/'tmp/review_latest_two/candidate/firmware';sym=V.symbols();V.SYMS.clear();V.SYMS.update(sym);V.SECS=V.sections();V.NEW=(V.FW/'firmware.bin').read_bytes()
 new=label!='original';u=V.machine(new)
 for a,d in outputs[6]:u.mem_write(a,d)
 u.mem_write(0x10002078,b'\x0a');calls=[];addr=V.SYMS['closed_loop_wrapper'] if new else 0x10f0a
 def hook(m,a,n,x):
  if a==addr:calls.append((m.reg_read(UC_ARM_REG_R0),m.reg_read(UC_ARM_REG_R1)))
 u.hook_add(UC_HOOK_CODE,hook);V.run(u,V.SYMS['output_stage'] if new else 0xe9ac,max_insn=3000000)
 print('PID_INPUT',label,calls,'field_feedback',struct.unpack('<I',u.mem_read(0x100015bc,4))[0])
V.FW=latest[0];V.SYMS.clear();V.SYMS.update(latest[1]);V.SECS=latest[2];V.NEW=latest[3]
# 直接执行原始eint3_init，确认main调用UART前的r0残留。
u=V.machine(False);u.mem_map(0x4002c000,0x1000);V.run(u,0xe908);print('ORIGINAL_EINT3_RETURN_R0',hex(u.reg_read(UC_ARM_REG_R0)))
for idx in (0,7,8,255):
 pair=[]
 for new in (False,True):
  u=V.machine(new);u.mem_map(0x4002c000,0x1000);u.mem_write(0x10001700,struct.pack('<I',idx));u.reg_write(UC_ARM_REG_R0,21)
  V.run(u,V.SYMS['uart3_init'] if new else 0xac24)
  pair.append(bytes(u.mem_read(0x4009c000,0x30)))
 assert pair[0]==pair[1]
 print('UART_FALLBACK',idx,'MATCH')
