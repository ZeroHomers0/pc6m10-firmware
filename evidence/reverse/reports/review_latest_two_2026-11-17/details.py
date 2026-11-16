from pathlib import Path
s=Path('tmp/review_latest_two/review.py').read_text(encoding='utf-8').split('latest=(V.FW,dict(V.SYMS),V.SECS,V.NEW)')[0]
exec(s)
from unicorn import UC_HOOK_CODE
for i in (3,4,6,7,12,13,16):
 pair=[]
 for new in (False,True):
  u=V.machine(new)
  for a,data in outputs[i]:u.mem_write(a,data)
  u.mem_write(0x10002078,b'\x0a');seen=[]
  def hook(m,a,n,x):
   if a in (0xf30e,0xf33c,0xf35a,0xf4e6,0xf51a,0xf556,0xf58a):seen.append(hex(a))
  if not new:u.hook_add(UC_HOOK_CODE,hook)
  V.run(u,V.SYMS['output_stage'] if new else 0xe9ac,max_insn=3000000)
  pair.append(bytes(u.mem_read(0x10000000,0x2200)))
  print('case',i,'new',new,'branches',seen)
 print('NO_MEM_HOOK_DIFF',i,[(hex(0x10000000+j),a,b) for j,(a,b) in enumerate(zip(*pair)) if a!=b][:20])
