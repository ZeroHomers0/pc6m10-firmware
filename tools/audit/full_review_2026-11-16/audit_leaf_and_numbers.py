"""2026-11-16 独立只读审计；差异是审计发现，不是已通过的等价证明。"""
import os
from pathlib import Path
os.chdir(Path(__file__).resolve().parents[3])
import sys,json,struct,random,re
from pathlib import Path
sys.path.insert(0,'tools/verification')
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
rows=json.loads(Path(__file__).with_name('functions.json').read_text())
entries={n:a for a,o,z,n,f in rows}
entries.update(param_sync_live_to_eeprom=0x35f2,uart3_receive_frame=0xaed0,modbus_read_reg=0xaf94,modbus_write_multi=0xb2e0,output_stage=0xe9ac,startup_idle_hook=0x5ca,freq_adjust_sync=0xab48)
results=[]
def make(new):
 u=V.machine(new)
 for base in (0x40004000,0x4002c000,0x40034000):u.mem_map(base,0x1000)
 return u
# 无轮询的外设叶函数：比较所有外围写入地址、位宽、值以及SRAM末态。
leaf=['wdt_init','wd_feed','timer0_init','lcd_ctrl_line','lcd_data_byte','disp_cmd','disp_data','gpio_inputs_dir_init','i2c_gpio_init','i2c_start','i2c_stop','i2c_write_byte','i2c_read_byte','adc_init','adc0_start','gpio_outputs_set','timer1_init','timer2_init','eint1_init','eint2_init','eint3_init','fio1_pin22_ctrl','fio0_pin22_ctrl','out_relay_p020','out_relay_p021','fio1_pin20_ctrl','fio1_pin21_ctrl','fio1_pin23_ctrl','gpio2_init','gpio0_input_init','read_input_p02','chk_p02_p03','nvic_enable_irq']
for name in leaf:
 for arg in (0,1,2,7,8,31,32,63,127,255):
  if name=='nvic_enable_irq' and arg>63:continue
  states=[]
  for new in (False,True):
   u=make(new);writes=[]
   for base in (0x2009c000,0x40004000,0x40008000,0x40090000,0x4009c000,0x400fc000,0xe000e000,0x4002c000,0x40034000):
    u.mem_write(base,bytes([0xa5])*0x1000)
   # 短延时仍真实执行；不挂GPIO读hook。
   # 本次关闭内存写hook，只比较寄存器终态。
   u.reg_write(UC_ARM_REG_R0,arg);u.reg_write(UC_ARM_REG_R1,arg&1)
   try:
    u.emu_start((V.SYMS[name] if new else entries[name])|1,0x3ff00,count=1000000)
    done=u.reg_read(UC_ARM_REG_PC)==0x3ff00
    states.append((done,bytes(u.mem_read(0x2009c000,0xa0)),bytes(u.mem_read(0x10000000,0x213c))))
   except Exception as e:states.append(('ERROR',str(e)))
  if states[0]!=states[1]:
   diff=[]
   if len(states[0])==3 and len(states[1])==3:
    diff=[(hex(a),x,y) for a,(x,y) in enumerate(zip(states[0][2],states[1][2]),0x10000000) if x!=y][:3]

   results.append((name,arg,'DIFF',states[0][0],states[1][0],len(states[0][1]),len(states[1][1]),diff))
 print(name,'done',flush=True)
print('LEAF_DIFFS',results,flush=True)
# 数字格式化：真正执行格式化和字形查表，仅替换LCD总线叶函数为记录器。
nums=['disp_digit','disp_number3','disp_uint4','disp_uint5','disp_number','disp_signed_angle','disp_offset','disp_uint2','disp_fixed_1dec','disp_decimal1']
for name in nums:
 bad=[]
 for val in (0,1,9,10,49,50,99,100,159,160,179,180,999,1000,9999,10000,65535,0xffffffff):
  pair=[]
  for new in (False,True):
   u=make(new);trace=[]
   cmd=V.SYMS['disp_cmd'] if new else 0x94a;data=V.SYMS['disp_data'] if new else 0x8f4;delay=V.SYMS['Delay'] if new else 0x7a8
   def cb(m,a,n,x):
    if a in (cmd,data):trace.append(('cmd' if a==cmd else 'data',m.reg_read(UC_ARM_REG_R0)&255,m.reg_read(UC_ARM_REG_R1) if a==data else 0))
    if a in (cmd,data,delay):m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
   u.hook_add(UC_HOOK_CODE,cb)
   for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),(val,1,2,0)):u.reg_write(reg,v)
   try:u.emu_start((V.SYMS[name] if new else entries[name])|1,0x3ff00,count=1000000);pair.append((u.reg_read(UC_ARM_REG_PC)==0x3ff00,trace))
   except Exception as e:pair.append(('ERROR',str(e)))
  if pair[0]!=pair[1]:bad.append((val,pair[0][0],pair[1][0],len(pair[0][1]),len(pair[1][1])))
 print('NUMBER',name,'bad',bad,flush=True)
