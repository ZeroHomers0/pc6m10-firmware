import sys,importlib.util,struct,json,subprocess
from pathlib import Path
ROOT=Path.cwd();sys.path.insert(0,str(ROOT/'tools/verification'))
import verify_firmware_equivalence as V
from unicorn.arm_const import UC_ARM_REG_PC

def mod(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
A=mod('review_a','test/emulation/test_audit_fixes_2.py');D=mod('review_d','test/emulation/test_display_call_equivalence.py')
orig_run=V.run
# 所有调用必须正常返回，禁止把预算耗尽视为比较成功。
def checked(*args,**kwargs):
 orig_run(*args,**kwargs);assert args[0].reg_read(UC_ARM_REG_PC)==0x3ff00,'未完成执行'
V.run=checked;D.run=checked
protocol=[];outputs=[];traces=[];states=[]
orig_modbus=A.run_modbus;orig_output=A.run_output_stage
A.run_modbus=lambda frame,seed=None:(protocol.append((frame,seed)) or orig_modbus(frame,seed))
A.run_output_stage=lambda seed:(outputs.append(seed) or orig_output(seed))
D.check_trace=lambda cases:(traces.extend(cases) or 0)
D.check_state=lambda cases:(states.extend(cases) or 0)
A.main();D.main()
# 新增导航帧、非法页号和状态监视页的边界样本。
for menu in (0x14,0,0x64,0xff):
 for idle in (0,4999,5000):
  for refresh in (249,250,251):
   p=dict(D.BASE);p.update({0x10001764:idle,0x10001778:refresh,0x10001624:0})
   states.append((menu,0,p))
for item in range(16):
 for key in (2,3):
  for view in (0,1):
   p=dict(D.BASE);traces.append((3,item,view,500,p,key))
for sel in (1,2,3,4):
 p=dict(D.BASE);p[0x10001710]=sel;traces.append((2,4,0,250,p,1))
for reg in range(1,64):
 for count in sorted(set((1,64-reg))):protocol.append(((1,3,16,reg,0,count),None))
for fc in (3,6,16):
 for page in (0,1,15,17,255):
  protocol.append(((1,fc,page,1,0,1,2,0,1) if fc==16 else (1,fc,page,1,0,1),None))


# 原回归第二路闭环样本未显式设置 ADC 给定，补足进入主控制块的前提。
base=list(outputs[-1])
for counter in (0,49,50,51):
 for flag in (0,1,2):
  for measured in (1,3001):
   outputs.append(base+[(0x100015a8,struct.pack('<I',100)),(0x10002018,struct.pack('<I',counter)),(0x10002020,struct.pack('<I',flag)),(0x100015bc,struct.pack('<I',measured))])

latest=(V.FW,dict(V.SYMS),V.SECS,V.NEW)
records={}
for label in ('latest','before'):
 if label=='before':
  V.FW=ROOT/'tmp/review_latest_two/before/firmware'
  symbols=V.symbols();V.SYMS.clear();V.SYMS.update(symbols);V.SECS=V.sections();V.NEW=(V.FW/'firmware.bin').read_bytes()
 bad={};total={}
 for name,cases,func in [('protocol',protocol,orig_modbus),('output',outputs,orig_output)]:
  failures=[]
  for i,case in enumerate(cases):
   pair=func(*case) if name=='protocol' else func(case)
   if pair[0]!=pair[1]:failures.append(i)
  total[name]=len(cases);bad[name]=failures
 for name,cases,func in [('display',traces,D.trace),('state',states,D.state_snapshot)]:
  failures=[]
  for i,case in enumerate(cases):
   if name=='state':
    menu,key,extra=case;p=dict(D.BASE);p.update(extra);case=(menu,key,p)
   if func(False,*case)!=func(True,*case):failures.append(i)
  total[name]=len(cases);bad[name]=failures
 records[label]={'total':total,'bad':bad};print(label,json.dumps(records[label]),flush=True)
V.FW=latest[0];V.SYMS.clear();V.SYMS.update(latest[1]);V.SECS=latest[2];V.NEW=latest[3]
Path('tmp/review_latest_two/results.json').write_text(json.dumps(records,indent=2))
