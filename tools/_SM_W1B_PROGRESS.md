# W1b 进度 —— state_machine(0x458C) 还原（任务 #36）

> 本文件是 W1b（把 state_machine 从 stub 还原为真实 C）的**防丢失进度档**。
> 恢复顺序：读本文件 → 继续读剩余 case 反汇编 → 写 `firmware/src/07_state_machine.c`。
> 相关文件（tools/）：`_sm_entry_0x458C_0x4B16.txt`、`_sm_case1_0x4B16_0x541C.txt`、
> `_sm_caseA_0x541C_0x5572.txt`、`_sm_case62_0x5572_0x5748.txt`、`_sm_case63_0x5748_0x6134.txt`、
> `_sm_case2_0x6134_0x69D6.txt`、`_sm_case3_0x69D6_0x7C1A.txt`、`_sm_case4_0x7C1A_0x8780.txt`、
> `_sm_lits_all.txt`（pool 表）、`_case2_str_utf8.txt`/`_case34_str2.txt`/`_pid_str.txt`/`_case14_etc_str.txt`（字符串）。

---

## 1. 状态总览（2026-08-21 会话 2 结束点）

| 段 | 地址 | 反汇编 | 解码 | C 写入 |
|---|---|---|---|---|
| entry | 0x458C-0x4B16 | ✅ | ✅ 完整 | ❌ |
| case1 | 0x4B16-0x541C | ✅ | ✅ 完整 | ❌ |
| caseA | 0x541C-0x5572 | ✅ | ✅ 完整 | ❌ |
| case62 | 0x5572-0x5748 | ✅ | ✅ 完整 | ❌ |
| case63 | 0x5748-0x6134 | ✅ | ✅ 完整 | ❌ |
| case2 | 0x6134-0x69D6 | ✅ | ✅ 完整 | ✅ |
| case3 | 0x69D6-0x7C1A | ✅ | ✅ 已写入 07_state_machine.c（含 sm3_draw_item 重构，零警告编译）| ✅ |
| case4 | 0x7C1A-0x8780 | ✅ | ✅ 已解码（10 项 3 页，sm4_draw_value/sm4_draw_page）+已写入 | ✅ |
| case5 | 0x8780-0x8C1A | ✅ | ✅ 已解码（4 项单页，sm5_draw_value/sm5_draw_page）+已写入 | ✅ |

**下一步**：① 写 case6/7/8/B/9/5A/C/14/1E（§11 已全解码，池已验证）→ ② build.sh 验证 → ③ 更新本文件状态 + 记忆 + 任务 #36 completed。

**反汇编分散链**：entry 无显式跳转落入 case1；case1→A→62→63→2→3→4→5→6→7→8→B→9→5A→C→14→1E→0xAB44 返回。
C 中用顺序 if 级联等价，遇 `return;` 即返回。**0x69CE→0x4BA8 是 case1 段内返回点**（非函数外）。

---

## 2. 全局约定

- **r4 = key**：1=确认、2=DOWN/减、3=UP/加、4=SET/退出、5=启动、6=停机、0x16=快加、0x21=快减、
  0x17=统计清零、0xe=初始参数密码、数字键 0-9 输密码。
- **显示函数**（已 Ghidra 确认）：
  `disp_string(r0=str,r1=row,r2=col,r3=attr)`@0xd3c、`disp_uint4`@0xed0、`disp_uint5`@0xf8c、
  `disp_number`@0x1092、`disp_offset`@0x12b0、`disp_uint2`@0x13e8、`disp_signed_angle`@0x11bc、
  `disp_render_char8`@0xb44、`disp_number3`@0xe42、`disp_clear`@0x992、`disp_splash_screen`@0x427c、
  `disp_screen_static`@0x448a、`disp_screen_calib`@0x44c2、`lcd_ctrl_line`@0x7b6。
- **其他 bl**：`fio0_pin22_ctrl`@0xe966、`fio1_pin22_ctrl`@0xe946、`fio1_pin20_ctrl`@0x105c8、
  `fio1_pin21_ctrl`@0x105e8、`fio1_pin23_ctrl`@0x10608、`out_relay_p020`@0x10588、`out_relay_p021`@0x105a8、
  `param_sync_live_to_eeprom`@0x35f2、`wd_feed`@0x238、`debounce_p09`@0x1ab8、`debounce_p116`@0x1ae6、
  `debounce_p117`@0x1b3e、`debounce_p06`@0x1b96、`scan_run_stop`@0x19c6、`i2c_write_reg`@0x1e88、
  `0xe79a`（故障处理）、`0xf9aa`（run_stop_preset?）。
- **attr**：1=高亮、0=正常。
- **adr**=字符串字节本身（C 用 `(uint8_t*)0xXXXX`）；**ldr**=pool 指针。
- **编码风格**（W1a 参照 `08_modbus_dispatch.c`）：头注释大块 + `#include "inc/types.h"`/`"inc/globals.h"`
  + 依赖函数前向声明带 `/* 来源模块.c */` 注释 + `#define XXX ((volatile uint8_t*)0x1000xxxx)`（byte）/
  `((volatile uint32_t*)...)`（word）宏 + `(void)key;`。

---

## 3. entry 段完整解码（0x458C-0x4B16）→ C 草图

```c
void state_machine(int key) {
  (*TIMEOUT4)++;                                      // 0x10001770
  if (key > 0) { *TIMEOUT4 = 0; lcd_ctrl_line(1); }
  if (*TIMEOUT4 > 0x1388) { *TIMEOUT4 = 0; lcd_ctrl_line(0); }
  if (debounce_p09() == 1) {                          // P0.9 高电平：4 组 live→shadow + EEPROM
    if (*HOUR_TOTAL != *0x1000160c) { *0x1000160c=*HOUR_TOTAL;
      i2c_write_reg((*0x1000160c>>8)&0xff,0x97); i2c_write_reg(*0x1000160c&0xff,0x98); }
    if (*MIN_TOTAL != *0x10001610) { *0x10001610=*MIN_TOTAL;
      i2c_write_reg((*0x10001610>>8)&0xff,0x99); i2c_write_reg(*0x10001610&0xff,0x9a); }
    if (*0x10001614 != *0x10001618) { *0x10001618=*0x10001614;
      i2c_write_reg((*0x10001618>>8)&0xff,0x9b); i2c_write_reg(*0x10001618&0xff,0x9c); }
    if (*MANUAL != *0x100015dc) { *0x100015dc=*MANUAL;
      i2c_write_reg((*0x100015dc>>8)&0xff,0x1d); i2c_write_reg(*0x100015dc&0xff,0x1e); }
  }
  if (*FAULT != 0) {                                  // 0x10001624 word
    EVCODE=0;                                         // 0x10001748
    if (*FAULT&0x4) EVCODE=1; if (*FAULT&0x2) EVCODE=1; if (*FAULT&0x1) EVCODE=1;
    if (*FAULT&0x8) EVCODE=2; if (*FAULT&0x200) EVCODE=3; if (*FAULT&0x40) EVCODE=4;
    if (*FAULT&0x400) EVCODE=5; if (*FAULT&0x10) EVCODE=6; if (*FAULT&0x20) EVCODE=7;
    if (*FAULT&0x100) EVCODE=8; if (*FAULT&0x80) EVCODE=9; if (*FAULT&0x4000) EVCODE=0xa;
    if (*FAULT&0x8000) EVCODE=0xb; if (*FAULT&0x800) EVCODE=0xc;
    if (*FAULT&0x2000) EVCODE=0xd; if (*FAULT&0x1000) EVCODE=0xe;
    fio0_pin22_ctrl(0); fio1_pin22_ctrl(0); out_relay_p021(1); fio1_pin23_ctrl(1);
    *0x10001785=0; *STOP_PEND=1; *STOP_REQ=0; *RUN=0;         // strb
    *0x10001ffc=0; *0x10002068=0; *0x1000206c=0; *0x10002070=0; *0x10002000=0;  // word
    bl 0xe79a;
  } else {
    out_relay_p021(0); fio1_pin23_ctrl(0); *EVCODE=0;
  }
  if (*RUN != 0) {                                    // 0x10001628 byte
    fio0_pin22_ctrl(1); fio1_pin22_ctrl(1);
    (*TICK)++;
    if (*TICK > 0x7530) { *TICK=0; (*MIN_NOW)++; (*MIN_TOTAL)++;
      if (*MIN_NOW>=0x3c){*MIN_NOW=0; (*HOUR_NOW)++;}
      if (*MIN_TOTAL>=0x3c){*MIN_TOTAL=0; (*HOUR_TOTAL)++; (*0x10001614)++;}
      if (*0x10001614>=0x140) *0x10001614=0; }
    if (*0x10001614==0x78 && *SYNC_30==0) { *SYNC_30=1; *SYNC_34=0; param_sync_live_to_eeprom(); }
    if (*0x10001614==0x12c && *SYNC_30==2) { *SYNC_30=0; *SYNC_34=1; param_sync_live_to_eeprom(); }
  }
  if (*SYNC_2C != 1) { out_relay_p020(1); out_relay_p021(1); fio0_pin22_ctrl(1); }  // 0x1000172c
  *DB_116 = debounce_p116();
  if (*FAULT==0 && *DB_116==2) *FAULT |= 0x4000;
  (*0x10001750)++;
  if (*0x10001750 <= 0x64) goto case1;                // 每 100 次才去抖
  *0x10001750=0;
  if (*0x100020c0==0 && *0x100016dd>0) { (*0x10001754)++; if(*0x10001754==5){*0x10001754=0;*FAULT|=0x1;} }
  else { *0x10001754=0; *FAULT&=~0x1; }
  if (*0x100020c1==0 && *0x100016dd>0) { (*0x10001758)++; if(*0x10001758==5){*0x10001758=0;*FAULT|=0x2;} }
  else { *0x10001758=0; *FAULT&=~0x2; }
  if (*0x100020c2==0 && *0x100016dd>0) { (*0x1000175c)++; if(*0x1000175c==5){*0x1000175c=0;*FAULT|=0x4;} }
  else { *0x1000175c=0; *FAULT&=~0x4; }
  *0x100020c0=0; *0x100020c1=0; *0x100020c2=0;        // strb
  /* fallthrough → case1 */
}
```

**entry pool 映射**：0x48b8→TIMEOUT4(0x10001770) 0x48d8→EVCODE(0x10001748) 0x47d8→FAULT(0x10001624)
0x47e4→RUN(0x10001628) 0x4cfc→RUN 0x4cf8→EVCODE 0x4d00→TICK(0x10001600) 0x4d04→MIN_NOW(0x100015fc)
0x4d08→MIN_TOTAL(0x10001608) 0x4d0c→HOUR_NOW(0x100015f8) 0x4d10→HOUR_TOTAL(0x10001604)
0x4d14→0x10001614 0x4d18→SYNC_30(0x10001730) 0x4d1c→SYNC_34(0x10001734) 0x4d20→SYNC_2C(0x1000172c)
0x4d24→DB_116(0x10001780) 0x4d28→FAULT 0x4d2c→0x10001750 0x4d30→0x100020c0 0x4d34→0x100016dd
0x4d38→0x10001754 0x4d3c→0x100020c1 0x4d40→0x10001758 0x4d44→0x100020c2 0x4d48→0x1000175c
0x48bc→HOUR_TOTAL 0x48c0→0x1000160c 0x48c4→MIN_TOTAL 0x48c8→0x10001610 0x48cc→0x10001614
0x48d0→0x10001618 0x47cc→MANUAL 0x48d4→0x100015dc 0x48dc→0x10001785 0x48e0→STOP_PEND(0x1000177f)
0x48e4→STOP_REQ(0x1000177e) 0x48e8→0x10001ffc 0x48ec→0x10002068 0x48f0→0x1000206c
0x48f4→0x10002070 0x4cf4→0x10002000

---

## 4. case1 完整解码（0x4B16-0x541C，menu==1 运行状态屏）

```c
// menu==1
if (key==0x17 && *RUN==0) { *MENU=0xc; return; }             // 统计清零屏
if (key==1 && *RUN==0) { *MENU=0xa;                            // 参数密码屏
  disp_string((uint8_t*)0x4d9c,1,0,0); disp_string((uint8_t*)0x4dac,3,7,0);
  *TIMEOUT2=0x3c; *IDLE=0; return; }
if (key==0xe) { *MENU=0x62;                                    // 初始密码屏
  disp_string((uint8_t*)0x4db4,0,0,0); disp_string((uint8_t*)0x4dc8,1,0,0);
  disp_string((uint8_t*)0x4dac,3,7,0); return; }
if (key==4 && *RUN==0 && *FAULT!=0) { *MENU=0x14; *TIMEOUT3=0x1f4; return; } // 故障记录
(*IDLE)++;
if (*IDLE >= 0x15e) {                                         // 刷新显示
  *IDLE=0;
  if (*DISP_SEL==0) disp_fixed_1dec(*FREQ,0,9,0);            // 0x10001655==0
  else if (*DISP_SEL==1) disp_uint4(*TARGET,0,9,0);           // 0x100015a8
  else disp_uint4(*MANUAL,0,9,0);                             // 0x100015d8
  disp_uint4(*0x10001590,1,9,0); disp_uint4(*0x10001594,2,9,0);
  if (*FAULT!=0) { *STATUS=0; disp_string((uint8_t*)0x47dc,3,0,0); }   // '故障'
  else if (*RUN==0 && *STATUS!=1) { *STATUS=1; disp_string((uint8_t*)0x47e8,3,0,0); } // '停机'
  else if (*RUN!=0 && *STATUS!=2) { *STATUS=2; disp_string((uint8_t*)0x47f0,3,0,0); } // '运行'
  if (*CTRL_MODE==0 && *DISP_MODE!=1) { *DISP_MODE=1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0);
    disp_string((uint8_t*)0x47fc,0,0,0); }                    // '恒压'
  else if (*CTRL_MODE==1 && *DISP_MODE!=2) { *DISP_MODE=2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
    disp_string((uint8_t*)0x4804,0,0,0); }                    // '恒流'
  else if (*CTRL_MODE==2 && *DISP_MODE!=3) { *DISP_MODE=3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0);
    disp_string((uint8_t*)0x480c,0,0,0); }                    // '开环'
  *DB_117 = debounce_p117();
  if (*FAULT!=0) {
    if (*DB_117==2 && *RESET_MODE==0) {                       // 复位流程
      *FAULT=0; *RUN=0; *STOP_PEND=1; *STOP_REQ=0;
      disp_string((uint8_t*)0x522c,0,0,0);                    // '复位'
      延时 0xbb8;                                             // LATCH_IN 0→0x7d0 内循环+wd_feed+LATCH_OUT++
      disp_string((uint8_t*)0x523c,0,0,0);                    // '重启'
      延时 0xbb8; while(1); }
  } else {
    if (*RESET_MODE==1) {
      if (*DB_117!=2 && *DISP_MODE!=1) { *DISP2=1; *CTRL_MODE=0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
        disp_string((uint8_t*)0x47fc,0,0,0); }
      if (*DB_117==2 && *DISP_MODE!=2) { *DISP2=2; *CTRL_MODE=1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
        disp_string((uint8_t*)0x4804,0,0,0); }
    }
    if (*RESET_MODE==2) {
      if (*DB_117!=2) *RESET2=0;
      else *RESET2=1;
    }
    *DB_117 = debounce_p06();
    if (*DB_117==2 && *ESTOP==0) {                            // 启动按钮（无故障）
      *RUN_REQ=1; *RUN=0; *STOP_PEND=1; *STOP_REQ=0;
      if (*STAT1==0) { disp_string((uint8_t*)0x47e8,3,0,0); *STAT1=1; }  // '停机'
      return; }
    if (*ESTOP==1) { /* 恒压/恒流切换（逻辑同 RESET_MODE==1，对应复位键常按=急停恢复） */ }
    if (*ESTOP==2) { /* RESET2 设置（同 RESET_MODE==2） */ }
    if (*RESET_MODE!=2 && *ESTOP!=2) *RESET2=0;
    *SCAN_STOP = scan_run_stop();
    if (*FAULT==0 && *STOP_REQ==0 && *0x10001785==1 && *DISP_SEL==0) { // 启动
      *0x10001785=1; *STOP_REQ=1; *STOP_PEND=0; *RUN_REQ=0; *RUN=1; *STAT1=0;
      *TICK=0; *MIN_NOW=0; *HOUR_NOW=0;
      disp_string((uint8_t*)0x47f0,3,0,0); }                  // '运行'
    if (*FAULT==0 && *STOP_PEND==0 && *0x10001785==0 && *DISP_SEL==0) { // 停机
      *STOP_PEND=1; *STOP_REQ=0; *RUN=0;
      disp_string((uint8_t*)0x47e8,3,0,0); }
    if (*RUN==0 && *DISP_SEL!=0) *0x10001785=0;
    if (*FAULT==0 && *STOP_REQ==0 && (*key==5 || *SCAN_STOP==7)) {      // 启动方式0
      if (*启动方式==0) { 公共启动; }
      else if (*SCAN_STOP==7 && *启动方式==1 && *DISP_SEL!=0) { 公共启动; }
    }
    if (*FAULT==0 && *STOP_PEND==0 && (*key==6 || *SCAN_STOP==8)) {     // 停机方式0
      if (*启动方式==0) { 公共停机; }
      else if (*SCAN_STOP==8 && *启动方式==1 && *DISP_SEL!=0) { 公共停机; }
    }
    // 公共启动：*0x10001785=1; *STOP_REQ=1; *STOP_PEND=0; *RUN_REQ=0; *RUN=1; *STAT1=0;
    //           *TICK=0; *MIN_NOW=0; *HOUR_NOW=0; disp 0x47f0
    // 公共停机：*0x10001785=0; *STOP_PEND=1; *STOP_REQ=0; *RUN=0; disp 0x47e8
    if (*DISP_SEL==0) {                                       // 恒压/恒流目标生成
      *TARGET=*FREQ;
      if (*CTRL_MODE==0) *TARGET_AMP=(*FREQ * *V_RANGE)/1000;    // 0x10001774
      else *TARGET_AMP=(*FREQ * *A_RANGE)/1000;                  // 0x10001638
      *V_AMP=*TARGET_AMP; *V_AMP2=*TARGET_AMP;
    } else if (*DISP_SEL==1) { *V_AMP=*V_AMP2; }
    else {                                                    // DISP_SEL==2 手动
      if (key==2 || key==0x16) { (*MANUAL)++; if (*MANUAL>0x3e8)*MANUAL=0x3e8;
        if (*MANUAL<0xa)*MANUAL=0xa; disp_fixed_1dec(*MANUAL,0,9,0); }
      if (key==3 || key==0x21) {
        if (*MANUAL>0xa) (*MANUAL)--;
        else { *MANUAL=1; (*MANUAL)--; }                      // 注意 case1 与 case1E 不同：有二次 --
        disp_fixed_1dec(*MANUAL,0,9,0); }
      *TARGET=*MANUAL;
      if (*CTRL_MODE==0) *0x100015d4=(*MANUAL * *V_RANGE)/1000;   // 手动 TARGET_AMP 存 0x100015d4
      else *0x100015d4=(*MANUAL * *A_RANGE)/1000;
    }
  }
  // case1 尾部（0x541e，手动段）
  *V_AMP=*0x100015d4; *V_AMP2=*0x100015d4; return;
}
```
> ⚠ **注意**：case1 手动减键 `*MANUAL>0xa?(*MANUAL)--:*MANUAL=1,(*MANUAL)--`；case1E 减键不同（无二次--，见下）。
> 手动尾部 0x541E 在 DISP_SEL==0/1 时也会执行（V_AMP/V_AMP2=*0x100015d4）——精确分支需对照反汇编再校一次。

**case1 key pool**：0x4d4c→MENU(0x10001744) 0x4d50→MENU2 0x4d54→TIMEOUT(0x10001764) 0x4d94→TIMEOUT2(0x10001760)
0x4d98→IDLE(0x10001768) 0x4ddc→TIMEOUT3(0x10001778) 0x4de0→DISP_SEL(0x10001655) 0x4de4→FREQ(0x10001788)
0x4de8→TARGET(0x100015a8) 0x4dec→MANUAL(0x100015d8) 0x4df0→0x10001590 0x4df4→0x10001594
0x4df8→STATUS(0x100015cc) 0x5208→RUN 0x520c→STATUS 0x5210→CTRL_MODE(0x10001634) 0x5214→DISP_MODE(0x100015cd)
0x5218→DB_117(0x1000177c) 0x521c→FAULT 0x5220→RESET_MODE(0x10001658) 0x5224→STOP_PEND(0x1000177f)
0x5228→STOP_REQ(0x1000177e) 0x5234→LATCH_OUT(0x1000161c) 0x5238→LATCH_IN(0x10001620)
0x5244→DISP2(0x100015cf) 0x5248→RESET2(0x10001782) 0x524c→ESTOP(0x10001657) 0x5250→RUN_REQ(0x1000177d)
0x5254→STAT1(0x10001781) 0x5258→SCAN_STOP(0x10001747) 0x525c→0x10001785 0x5260→DISP_SEL 0x5264→TICK
0x5268→MIN_NOW 0x526c→HOUR_NOW 0x5270→启动方式(0x10001656) 0x5670→STOP_PEND 0x5674→RUN_REQ 0x5678→RUN
0x567c→STAT1 0x5680→TICK 0x5684→MIN_NOW 0x5688→HOUR_NOW 0x568c→FAULT 0x5690→SCAN_STOP
0x5694→启动方式 0x5698→DISP_SEL 0x569c→0x10001785 0x56a0→STOP_REQ 0x56a4→FREQ 0x56a8→TARGET
0x56ac→CTRL_MODE 0x56b0→V_RANGE(0x1000163c) 0x56b4→TARGET_AMP(0x10001774) 0x56b8→A_RANGE(0x10001638)
0x56bc→V_AMP(0x100015d0) 0x56c0→V_AMP2(0x100015b4) 0x56c4→MANUAL 0x56c8→0x100015d4

---

## 5. caseA 完整解码（0x541C-0x5572，menu==0xa 参数密码屏）

```c
// menu!=0xa → case62
// menu==0xa
if (key==1) {
  *MENU2=0;
  while (*MENU2<6) {
    if (PWD_BUF[*MENU2] != PWD_A[*MENU2]) {             // 0x100015f2 vs 0x100015e0
      disp_clear();
      disp_string((uint8_t*)0x56dc,1,4,0);              // '密码错误'
      延时 0x2710;                                      // LATCH_IN 0→0x3e8 内循环+wd_feed+LATCH_OUT++
      *MENU=1; disp_splash_screen(); return;
    }
    PWD_BUF[*MENU2]=0; (*MENU2)++;
  }
  *MENU=2; *MENU2=0; disp_screen_static(); return;      // →主菜单页1
}
if (key==4) { *MENU=1; disp_splash_screen(); return; }
if (key>0) {
  if (*MENU2<6) { PWD_BUF[*MENU2]=key; disp_render_char8('*',1,*MENU2+7,0); (*MENU2)++; }
  return;
}
(*IDLE)++;
if (*IDLE>=0x1f4) { *IDLE=0; (*TIMEOUT2)--; disp_number3(*TIMEOUT2,3,6,0);
  if (*TIMEOUT2==0) { *MENU=1; disp_splash_screen(); return; } }
return;
```
**caseA pool**：0x56cc→MENU 0x56d0→MENU2 0x56d4→PWD_BUF(0x100015f2) 0x56d8→PWD_A(0x100015e0)
0x56dc→'密码错误' 0x56e8→LATCH_OUT 0x56ec→LATCH_IN 0x56f0→IDLE 0x56f4→TIMEOUT2 0x56f8→PWD_C(0x100015ec)

---

## 6. case62 完整解码（0x5572-0x5748，menu==0x62 初始密码屏）

```c
// menu==0x62
if (key==1) {
  *MENU2=0;
  while (*MENU2<6) {
    if (PWD_BUF[*MENU2] != PWD_C[*MENU2]) {             // 0x100015ec
      disp_clear(); disp_string((uint8_t*)0x56dc,1,4,0);  // '密码错误'
      延时 0x2710; *MENU=1; disp_splash_screen(); return;
    }
    PWD_BUF[*MENU2]=0; (*MENU2)++;
  }
  *MENU=0x63; *MENU2=0; disp_clear(); disp_screen_calib(); return;  // →初始参数
}
if (key==4) { *MENU=1; disp_splash_screen(); return; }
if (key>0) { if (*MENU2<6) { PWD_BUF[*MENU2]=key; disp_render_char8('*',1,*MENU2+7,0); (*MENU2)++; } return; }
(*IDLE)++;                                              // 0x5afc
if (*IDLE>=0x1f4) { *IDLE=0; (*TIMEOUT2)--; disp_number3(*TIMEOUT2,3,6,0);
  if (*TIMEOUT2==0) { *MENU=1; disp_splash_screen(); return; } }
return;
```
**case62 pool**：0x5afc→IDLE 0x5b00→TIMEOUT2 0x5b04→MENU

---

## 7. case63 完整解码（0x5748-0x6134，menu==0x63 初始参数，MENU2=项0-10，MENU3=0导航/1编辑）

```c
// menu==0x63
if (key==1) {                                           // 导航/编辑切换
  *TIMEOUT=0; (*MENU3)++; if (*MENU3>1) *MENU3=0;
  if (*MENU3==0) *TIMEOUT3=0xfa; else *TIMEOUT3=0x1f4;
}
if (key==4) { *TIMEOUT=0; param_sync_live_to_eeprom(); *MENU=1; disp_splash_screen(); return; }
if (key==2 || key==3) {
  if (*MENU3==0) {                                      // 导航
    *TIMEOUT=0;
    if (key==3) { (*MENU2)++; if (*MENU2>0xa) *MENU2=0xa; }
    if (key==2) { if (*MENU2>0) (*MENU2)--; }
    if (*MENU2<4)    disp 0x4854/0x4868/0x487c/0x4890 rows0-3;    // '1.U 相电流'/'2.V 相电流'/'3.W 相电流'/'4.输出电流'
    if (4<=*MENU2<8) disp 0x5b18/0x5b2c/0x5b40/0x5b54 rows0-3;    // '5.输出电压'/'6.急停设置'/'7.复位设置'/'8.反馈设置'
    if (8<=*MENU2<0xc) disp 0x5b68/0x5b7c/0x5b90/0x5ba4 rows0-3;  // '9.输入设置'/'10控制方式'/'11起始相位'/空白
    *TIMEOUT3=0xfa;
  } else {                                              // 编辑
    if (key==2 || key==0x16) {                          // 加
      *TIMEOUT=0;
      if (*MENU2==0)   { (*0x10001698)++; if (*0x10001698>0x1194) *0x10001698=0x1194; }  // 4500
      if (*MENU2==1)   { (*0x100016a0)++; if>0x1194 =0x1194; }
      if (*MENU2==2)   { (*0x100016a8)++; if>0x1194 =0x1194; }
      if (*MENU2==3)   { (*0x100016b0)++; if>0x1194 =0x1194; }
      if (*MENU2==4)   { (*0x100016b8)++; if>0x1194 =0x1194; }
      if (*MENU2==5)   { (*ESTOP=0x10001657)++; if>2 =2; }
      if (*MENU2==6)   { (*RESET_MODE=0x10001658)++; if>2 =2; }
      if (*MENU2==7)   { (*FEEDBACK=0x10001659)++; if>1 =1; }
      if (*MENU2==8)   { (*INPUT_SEL=0x1000165a)++; if>1 =1; }
      if (*MENU2==9)   { (*0x1000165b)++; if>1 =1; }
      if (*MENU2==0xa) { (*START_PHASE=0x10001660)++; if>0xb4 =0xb4; }  // 180
    }
    if (key==3 || key==0x21) {                          // 减
      *TIMEOUT=0;
      if (*MENU2==0)   { if (*0x10001698>0xdac) (*0x10001698)--; }   // 3500
      if (*MENU2==1)   { if (*0x100016a0>0xdac) (*0x100016a0)--; }
      if (*MENU2==2)   { if (*0x100016a8>0xdac) (*0x100016a8)--; }
      if (*MENU2==3)   { if (*0x100016b0>0xdac) (*0x100016b0)--; }
      if (*MENU2==4)   { if (*0x100016b8>0xdac) (*0x100016b8)--; }
      if (*MENU2==5)   { if (*0x10001657>0) (*0x10001657)--; }
      if (*MENU2==6)   { if (*0x10001658>0) (*0x10001658)--; }
      if (*MENU2==7)   { if (*0x10001659>0) (*0x10001659)--; }
      if (*MENU2==8)   { if (*0x1000165a>0) (*0x1000165a)--; }
      if (*MENU2==9)   { if (*0x1000165b>0) (*0x1000165b)--; }
      if (*MENU2==0xa) { if (*0x10001660>0) (*0x10001660)--; }
    }
    *TIMEOUT3=0xfa;
  }
}
(*TIMEOUT3)++;
if (*TIMEOUT3==0xfb) {                                  // 刷新显示（高亮当前项）
  if (*MENU2<4) {                                       // 项0-3 电流值 disp_uint4(0x10001698/a0/a8/b0, row, 0xb, 高亮?)
    ... }
  else if (*MENU2<8) {                                  // 项5-7 枚举显示
    if (*MENU2==5) { if (*0x10001657==0)disp 0x6018,1,0xb,1; ==1→0x6020; ==2→0x6028; }  // '急停'/'外控'/'限相'
    ...项6=0x10001658 (0x6030'复位'/0x6020/0x6028)、项7=0x10001659 (0x6038'检测'/0x6040'关闭')
  }
  else if (*MENU2<0xc) {                                // 项8-10
    ...项8=0x1000165a (0x6048/0x6050)、项9=0x1000165b (0x6058'全控'/0x6060'半控')、项10=disp_number3(*0x10001660,2,0xb,1)
  }
}
if (*TIMEOUT3>0x1f4) { *TIMEOUT3=0;
  if (*MENU3==0) return;                                // 导航超时直接返回
  // 编辑模式清高亮：按 MENU2 对应行 disp 0x6474('     ')/0x647c, row, 0xb, 0
  (*TIMEOUT)++;
  if (*TIMEOUT>=0x1388) { *TIMEOUT=0; param_sync_live_to_eeprom(); *MENU=1; disp_splash_screen(); return; }
  return;
}
```
**case63 pool**：0x5b08→TIMEOUT 0x5b0c→MENU3(0x10001746) 0x5b10→TIMEOUT3 0x5b14→MENU2
0x5bb8→0x10001698 0x5bbc→0x100016a0 0x5bc0→0x100016a8 0x5bc4→0x100016b0 0x5bc8→0x100016b8（word）
0x5bcc→0x10001657(ESTOP) 0x5bd0→0x10001658(RESET_MODE) 0x5bd4→0x10001659(FEEDBACK)
0x5bd8→0x1000165a(INPUT_SEL) 0x5bdc→0x1000165b 0x5be0→0x10001660(START_PHASE word)
0x5fe4→0x10001657 0x5fe8→MENU2 0x5fec→0x10001658 0x5ff0→0x10001659 0x5ff4→0x1000165a
0x5ff8→0x1000165b 0x5ffc→0x10001660 0x6000→TIMEOUT3 0x6004→0x10001698 0x6008→0x100016a0
0x600c→0x100016a8 0x6010→0x100016b0 0x6014→0x100016b8 0x6068→MENU3 0x6470→MENU2
0x6474→'     ' 0x647c→'      ' 0x6480→TIMEOUT 0x6484→MENU
字符串：0x4854 '1.U 相电流' / 0x4868 '2.V 相电流' / 0x487c '3.W 相电流' / 0x4890 '4.输出电流'
0x5b18 '5.输出电压' / 0x5b2c '6.急停设置' / 0x5b40 '7.复位设置' / 0x5b54 '8.反馈设置'
0x5b68 '9.输入设置' / 0x5b7c '10控制方式' / 0x5b90 '11起始相位' / 0x5ba4 空白
0x6018 ' 急停' / 0x6020 ' 外控' / 0x6028 ' 限相' / 0x6030 ' 复位' / 0x6038 ' 检测' / 0x6040 ' 关闭'
0x6058 ' 全控' / 0x6060 ' 半控' / 0x6048 / 0x6050 待解码（项8 输入设置枚举）

---

## 8. case2 完整解码（0x6134-0x69D6，menu==2 主菜单页1，MENU2=选项0-8）

```c
// menu==2
if (key==4) { *TIMEOUT=0; *MENU=1; disp_splash_screen(); return; }
if (key==2 || key==3) {                                 // 导航
  *TIMEOUT=0;
  if (key==3) { (*MENU2)++; if (*MENU2>7) *MENU2=7; }
  if (key==2) { if (*MENU2>0) (*MENU2)--; }
  if (*MENU2<4)    disp 0x6488/0x649c/0x64b0/0x64c4 rows0-3;    // '1.基本参数设置'/'2.保护参数设置'/'3.通讯参数设置'/'4.恢复出厂参数'
  if (4<=*MENU2<8) disp 0x64d8/0x64ec/0x6500/0x6514 rows0-3;    // '5.PID 参数设置'/'6.相位参数校准'/'7.运行时间查询'/'8.产品版本信息'
  if (8<=*MENU2<0xc) disp 0x6528/0x5ba4/0x5ba4/0x5ba4 rows0-3;  // '9.电流手动平衡'/空白
  if (*MENU2==0) disp 0x6488,0,0,1;   if (*MENU2==1) disp 0x649c,1,0,1;
  if (*MENU2==2) disp 0x64b0,2,0,1;   if (*MENU2==3) disp 0x64c4,3,0,1;
  if (*MENU2==4) disp 0x64d8,0,0,1;   if (*MENU2==5) disp 0x64ec,1,0,1;
  if (*MENU2==6) disp 0x6500,2,0,1;   if (*MENU2==7) disp 0x6514,3,0,1;
  if (*MENU2==8) disp 0x6528,0,0,1;
}
if (key==1) {                                           // 确认选中项 → 分发
  *TIMEOUT=0; *MENU3=0;                                  // 0x653c→MENU3
  if (*MENU2==0) {                                       // 基本参数
    *MENU=3; *MENU2=0;
    disp 0x6540/0x6554/0x6568/0x657c rows0-3;            // '1.运行模式'/'2.电压量程'/'3.电流量程'/'4.互感器比'
    if (*CTRL_MODE==0) { disp 0x6594,0,0xb,1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); } // '恒压'
    if (*CTRL_MODE==1) { disp 0x659c,0,0xb,1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); } // '恒流'
    if (*CTRL_MODE==2) { disp 0x65a4,0,0xb,1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); } // '开环'
    disp_uint4(*V_RANGE,1,0xb,0); disp_uint4(*A_RANGE,2,0xb,0); disp_uint4(*0x10001640,3,0xb,0);
    *TIMEOUT3=0xfa;
  }
  if (*MENU2==1) {                                       // 保护参数
    *MENU=4; *MENU2=0;
    disp 0x65bc/0x65d0/0x65e4/0x65f8 rows0-3;            // '1.过压保护'/'2.过压时间'/'3.欠压保护'/'4.欠压时间'
    disp_uint4(*0x100016c0,0,0xb,1); disp_uint4(*0x100016c4,1,0xb,0);
    disp_uint4(*0x100016c8,2,0xb,0); disp_uint4(*0x100016cc,3,0xb,0);
    *TIMEOUT3=0xfa;
  }
  if (*MENU2==2) {                                       // 通讯参数
    *MENU=5; *MENU2=0;
    disp 0x6a18/0x6a2c/0x6a40/0x6a54 rows0-3;            // '1.通讯地址'/'2.波特率'/'3.校验位'/'4.通讯检测'
    disp_uint5(*COM_ADDR,0,0xb,1);                        // 0x100016ff
    disp_number(BAUD_TBL[*BAUD_IDX],1,0xa,0);             // 0x100017bc[index]; index=0x10001700
    if (*PARITY==0) disp 0x6a78,2,0xa,0; if (==1) disp 0x6a80; if (==2) disp 0x6a88;  // 无/奇/偶校验
    if (*COM_CHK==0) disp 0x6038,3,0xb,0; else disp 0x6a94,3,0xb,0;  // ' 检测'/' 开启'
    *TIMEOUT3=0xfa;
  }
  if (*MENU2==3) {                                       // 恢复出厂
    *MENU=6; *MENU2=0; *LATCH_OUT=0; disp_clear();
    disp 0x6aa0,1,0,0;                                    // '  密码:------' ？见注
  }
  if (*MENU2==4) {                                       // PID
    *MENU=7; *MENU2=0;
    disp 0x6aa4/0x6ab8/0x6acc/0x6ae0 rows0-3;            // '1.PID 模式'/'2.P 参数'/'3.I 参数'/'4.D 参数'
    if (*PID_MODE==1) { disp 0x6af8,0,0xb,1; disp_uint2(*0x10001711,1,0xb,0); disp_uint2(*0x10001712,2,0xb,0); } // ' 快速'
    if (*PID_MODE==2) { disp 0x6b08,0,0xb,1; disp_uint2(*0x10001713,1,0xb,0); disp_uint2(*0x10001712,2,0xb,0); } // ' 中速'
    if (*PID_MODE==3) { disp 0x6b14,0,0xb,1; disp_uint2(*0x10001715,1,0xb,0); disp_uint2(*0x10001716,2,0xb,0); } // ' 慢速'
    if (*PID_MODE==4) { disp 0x6b24,0,0xb,1; disp_uint2(*0x10001717,1,0xb,0); disp_uint2(*0x10001718,2,0xb,0); } // ' 自定'
    *TIMEOUT3=0xfa;
  }
  if (*MENU2==5) {                                       // 相位校准
    *MENU=8; *MENU2=0; disp_clear();
    disp 0x6b34,0,4,0; disp 0x6b40,1,2,0; disp 0x6b4c,2,2,0; disp_offset(*PHASE_OFF,2,7,1); disp 0x6b58,3,0,0;
  }
  if (*MENU2==6) {                                       // 运行时间
    *MENU=0xb; *MENU2=0; *LATCH_OUT=0; disp_clear();
    disp (uint8_t*)0x4d58,0,0,0;  disp (uint8_t*)0x4d6c,1,0,0;
    disp (uint8_t*)0x4d80,2,0,0;  disp (uint8_t*)0x4d6c,3,0,0;   // 注：0x6aa0 池值=0x4d9c；-0x44/-0x30/-0x1c
    disp_uint5(*HOUR_NOW,1,3,0);  disp_uint2(*MIN_NOW,1,0xa,0);   // 本次 时/分
    disp_uint5(*HOUR_TOTAL,3,3,0); disp_uint2(*MIN_TOTAL,3,0xa,0); // 累计 时/分
  }
  if (*MENU2==7) {                                       // 版本信息
    *MENU=9; *MENU2=0; disp_clear();
    disp 0x6b78/0x6b84/0x6b94/0x6ba4 rows0-3;            // '型号:ST33C'/'版本:V2.0.2016'/'厂商:SINEP0WER'/'电话:18938061832'
  }
  if (*MENU2==8) {                                       // 电流平衡
    *MENU=0x5a; *MENU2=0; disp_clear();
    disp 0x6bb8,0,4,0; disp 0x6b40,1,2,0; disp 0x6b4c,2,2,0; disp_signed_angle(*BAL_ANG,2,7,1); disp 0x6b58,3,0,0;
  }
}
(*TIMEOUT)++;                                           // 0x6bc8→TIMEOUT(0x10001764)
if (*TIMEOUT>=0x1388) { *TIMEOUT=0; *MENU=1; disp_splash_screen(); }
return;                                                  // 回到 case1
```
> 注：0x6aa0 池值 = FLASH 0x4d9c（'  密码:------'）；-0x44=0x4d58('1.本次运行时间:')、-0x30=0x4d6c('   60000时30分  ')、-0x1c=0x4d80('2.累计运行时间:')。恢复出厂显示 0x6aa0 字符串。

**case2 pool**：0x6470→MENU2 0x6480→TIMEOUT(0x10001764) 0x6484→MENU 0x653c→MENU3(0x10001746)
0x6590→CTRL_MODE(0x10001634) 0x65ac→V_RANGE(0x1000163c) 0x65b0→A_RANGE(0x10001638)
0x65b4→0x10001640 0x65b8→TIMEOUT3(0x10001778) 0x660c→0x100016c0 0x6610→0x100016c4
0x6614→0x100016c8 0x6618→0x100016cc 0x6a0c→TIMEOUT3 0x6a10→MENU2 0x6a14→MENU
0x6a68→COM_ADDR(0x100016ff byte) 0x6a6c→BAUD_TBL(0x100017bc 7 word) 0x6a70→BAUD_IDX(0x10001700)
0x6a74→PARITY(0x10001704) 0x6a90→COM_CHK(0x10001705) 0x6a9c→LATCH_OUT(0x1000161c)
0x6aa0→FLASH 0x4d9c 0x6af4→PID_MODE(0x10001710) 0x6b00→0x10001711 0x6b04→0x10001712
0x6b10→0x10001713 0x6b1c→0x10001715 0x6b20→0x10001716 0x6b2c→0x10001717 0x6b30→0x10001718
0x6b54→PHASE_OFF(0x1000162c word) 0x6b68→HOUR_NOW(0x100015f8) 0x6b6c→MIN_NOW(0x100015fc)
0x6b70→HOUR_TOTAL(0x10001604) 0x6b74→MIN_TOTAL(0x10001608) 0x6bc4→BAL_ANG(0x10001694 byte)
0x6bc8→TIMEOUT(0x10001764)
字符串：0x6488'1.基本参数设置' 0x649c'2.保护参数设置' 0x64b0'3.通讯参数设置' 0x64c4'4.恢复出厂参数'
0x64d8'5.PID 参数设置' 0x64ec'6.相位参数校准' 0x6500'7.运行时间查询' 0x6514'8.产品版本信息'
0x6528'9.电流手动平衡' 0x6540'1.运行模式' 0x6554'2.电压量程' 0x6568'3.电流量程' 0x657c'4.互感器比'
0x6594'恒压' 0x659c'恒流' 0x65a4'开环' 0x65bc'1.过压保护' 0x65d0'2.过压时间' 0x65e4'3.欠压保护'
0x65f8'4.欠压时间' 0x6a18'1.通讯地址' 0x6a2c'2.波特率' 0x6a40'3.校验位' 0x6a54'4.通讯检测'
0x6a78'无校验' 0x6a80'奇校验' 0x6a88'偶校验' 0x6a94' 开启' 0x6aa4'1.PID 模式' 0x6ab8'2.P 参数'
0x6acc'3.I 参数' 0x6ae0'4.D 参数' 0x6af8' 快速' 0x6b08' 中速' 0x6b14' 慢速' 0x6b24' 自定'
0x6b34'相位校准' 0x6b40'输出电压50%' 0x6b4c'参数:' 0x6b58'工作状态: 停机' 0x6b78'型号:ST33C'
0x6b84'版本:V2.0.2016' 0x6b94'厂商:SINEP0WER' 0x6ba4'电话:18938061832' 0x6bb8'电流平衡'
0x4d58'1.本次运行时间:' 0x4d6c'   60000时30分  ' 0x4d80'2.累计运行时间:' 0x4d9c'  密码:------'
0x4814'1.基本参数设置'（case3 回主菜单用）

---

## 9. case3 已解码（0x69D6-0x7C1A，menu==3 基本参数 16 项，MENU3=0导航/1编辑）

**16 项（MENU2=0-15）与参数地址（已确认 pool）**：

| 项 | 字符串 | 参数 SRAM | 宽度 | 加上限 | 减下限 |
|---|---|---|---|---|---|
| 0 | '1.运行模式'(0x6540) | 0x10001634 CTRL_MODE | byte | >2 回绕0 | 0→3 再-- |
| 1 | '2.电压量程'(0x6554) | 0x1000163c V_RANGE | word | 0x1770(6000) | 0xa(10) |
| 2 | '3.电流量程'(0x6568) | 0x10001638 A_RANGE | word | 0x1770 | 0xa |
| 3 | '4.互感器比'(0x657c) | 0x10001640 | word | 0x1770 | 0xa |
| 4 | '5.电压限制'(0x6fe4) | 0x10001648 | word | ≤V_RANGE+1 | 0xa |
| 5 | '6.电流限制'(0x6ff8) | 0x10001644 | word | ≤A_RANGE+1 | 0xa |
| 6 | '7.软起时间'(0x700c) | 0x1000164c | byte | 0xc8(200) | >0 |
| 7 | '8.软停时间'(0x7020) | 0x1000164d | byte | 0xc8 | >0 |
| 8 | '9.相位限制'(0x7034) | 0x10001650 | word | 0xb4(180) | >0 |
| 9 | '10主从偏移'(0x7048) | 0x10001654 | byte | 0xa0(160) | >0x28(40) |
| 10 | '11控制方式'(0x705c) | 0x10001655 | byte | 0x2 | >0 |
| 11 | '12启动方式'(0x7070) | 0x10001656 | byte | 0x1 | >0 |
| 12 | '13急停设置'(0x7084) | 0x10001657 | byte | 0x2 | >0 |
| 13 | '14反馈设置'(0x7098) | 0x10001659 | byte | 0x1 | >0 |
| 14 | '15输入设置'(0x70ac) | 0x1000165a | byte | 0x1 | >0 |
| 15 | '16起始相位'(0x70c0) | 0x10001660 | word | 0xb4 | >0 |

```c
// menu==3
if (key==1) { *TIMEOUT=0; (*MENU3)++; if (*MENU3>1)*MENU3=0;
  if (*MENU3==0) *TIMEOUT3=0xfa; else *TIMEOUT3=0x1f4; }
if (key==4) { *TIMEOUT=0; *MENU=2; *MENU2=0; param_sync_live_to_eeprom(); disp_clear();
  disp 0x4814,0,0,1; disp 0x4824,1,0,0; disp 0x4834,2,0,0; disp 0x4844,3,0,0; return; }  // 主菜单页1
if (key==2 || key==3) {
  if (*MENU3==0) {                                  // 导航
    *TIMEOUT=0;
    if (key==3) { (*MENU2)++; if (*MENU2>0xf)*MENU2=0xf; }
    if (key==2) { if (*MENU2>0)(*MENU2)--; }
    if (*MENU2<4)     disp 0x6540/0x6554/0x6568/0x657c rows0-3;
    if (4<=*MENU2<8)  disp 0x6fe4/0x6ff8/0x700c/0x7020 rows0-3;
    if (8<=*MENU2<0xc) disp 0x7034/0x7048/0x705c/0x7070 rows0-3;
    if (0xc<=*MENU2<0x10) disp 0x7084/0x7098/0x70ac/0x70c0 rows0-3;
    *TIMEOUT3=0xfa;
  } else {                                          // 编辑 MENU3==1
    if (key==2 || key==0x16) {                      // 加
      *TIMEOUT=0;
      if (*MENU2==0)   { if (++*0x10001634>2) *0x10001634=0; }
      if (*MENU2==1)   { if (++*V_RANGE>0x1770) *V_RANGE=0x1770; }         // 0x7100
      if (*MENU2==2)   { if (++*A_RANGE>0x1770) *A_RANGE=0x1770; }         // 0x7104
      if (*MENU2==3)   { if (++*0x10001640>0x1770) *0x10001640=0x1770; }   // 0x7108
      if (*MENU2==4)   { if (++*0x10001648 > *V_RANGE+1) *0x10001648=*V_RANGE+1; }   // 0x710c
      if (*MENU2==5)   { if (++*0x10001644 > *A_RANGE+1) *0x10001644=*A_RANGE+1; }   // 0x7110
      if (*MENU2==6)   { if (++*0x1000164c>0xc8) *0x1000164c=0xc8; }
      if (*MENU2==7)   { if (++*0x1000164d>0xc8) *0x1000164d=0xc8; }
      if (*MENU2==8)   { if (++*0x10001650>0xb4) *0x10001650=0xb4; }
      if (*MENU2==9)   { if (++*0x10001654>0xa0) *0x10001654=0xa0; }
      if (*MENU2==0xa) { if (++*0x10001655>2) *0x10001655=2; }
      if (*MENU2==0xb) { if (++*0x10001656>1) *0x10001656=1; }
      if (*MENU2==0xc) { if (++*0x10001657>2) *0x10001657=2; }
      if (*MENU2==0xd) { if (++*0x10001659>1) *0x10001659=1; }
      if (*MENU2==0xe) { if (++*0x1000165a>1) *0x1000165a=1; }
      if (*MENU2==0xf) { if (++*0x10001660>0xb4) *0x10001660=0xb4; }
      if (key==0x16) {                                // 快加 +5（仅项1-5）
        if (*MENU2==1) { *V_RANGE+=5; if(*V_RANGE>0x1770)*V_RANGE=0x1770; }
        ... 2/3 同理；4: *0x10001648+=5; if>*V_RANGE+1=*V_RANGE+1; 5: *0x10001644+=5; if>*A_RANGE+1=*A_RANGE+1;
      }
    } else {                                          // 减 key==3 || key==0x21
      *TIMEOUT=0;
      if (*MENU2==0)   { if (*0x10001634!=0) { if (*0x10001634==3)*0x10001634=0; (*0x10001634)--; } }
      if (*MENU2==6)   { if (*0x1000164c>0) (*0x1000164c)--; }
      if (*MENU2==7)   { if (*0x1000164d>0) (*0x1000164d)--; }
      if (*MENU2==8)   { if (*0x10001650>0) (*0x10001650)--; }
      if (*MENU2==9)   { if (*0x10001654>0x28) (*0x10001654)--; }        // 下限40
      if (*MENU2==0xa) { if (*0x10001655>0) (*0x10001655)--; }
      if (*MENU2==0xb) { if (*0x10001656>0) (*0x10001656)--; }
      if (*MENU2==0xc) { if (*0x10001657>0) (*0x10001657)--; }
      if (*MENU2==0xd) { if (*0x10001659>0) (*0x10001659)--; }
      if (*MENU2==0xe) { if (*0x1000165a>0) (*0x1000165a)--; }
      if (*MENU2==0xf) { if (*0x10001660>0) (*0x10001660)--; }
      if (key==3) {                                    // 单按 -1（项1-5，下限0xa）
        if (*MENU2==1) { if (*V_RANGE>0xa) (*V_RANGE)--; }
        ... 2/3/4/5 同理 };
      if (key==0x21) {                                 // 快减 -5（项1-5）
        if (*MENU2==1) { if (*V_RANGE<0x10) *V_RANGE=0xf; *V_RANGE-=5; }
        ... 2/3/4/5 同理 };
    }
    *TIMEOUT3=0xfa;
  }
}
(*TIMEOUT3)++;
if (*TIMEOUT3==0xfb) {                                // 刷新显示（项0-7 已解，项8-15 在 0x772A-0x7C1A 未读完）
  if (*MENU2<4) {
    if (*MENU2==0) { if (*CTRL_MODE==0)disp 0x6594,0,0xb,1; ==1→0x659c; ==2→0x65a4; + fio1 引脚切换 }
    else { disp 0x6594/0x659c/0x65a4,0,0xb,0; + fio1 引脚 }
    if (*MENU2==1) disp_uint4(*V_RANGE,1,0xb,1); else disp_uint4(*V_RANGE,1,0xb,0);
    if (*MENU2==2) disp_uint4(*A_RANGE,2,0xb,1); else ...attr0;
    if (*MENU2==3) disp_uint4(*0x10001640,3,0xb,1); else ...attr0;
  }
  else if (*MENU2<8) {                                // 项4-7
    if (*MENU2==4) { if (*0x10001648 >= *V_RANGE) disp_uint4(*V_RANGE,0,0xb,1); else disp 0x6038,0,0xb,1; }
    else { 同上 attr0; }
    if (*MENU2==5) { if (*0x10001644 >= *A_RANGE) disp_uint4(*A_RANGE,1,0xb,1); else disp 0x6038,1,0xb,1; }
    ...
    if (*MENU2==6) disp_uint4(*0x1000164c,2,0xb,1); else ...attr0;
    if (*MENU2==7) disp_uint4(*0x1000164d,3,0xb,1); else ...attr0;
  }
  // 项8-15 刷新：0x772A-0x7C1A ⚠️ 未读完，见 0x7998'通讯'/0x79a0'本地'/0x79a8'定值'/0x79b4'点动'/0x79bc'自锁'
  // 0x7974池值='V'(0x56)、0x7980池值='A'(0x41)、0x7978池值=FLASH 0x6038(' 检测')
}
if (*TIMEOUT3>0x1f4) { *TIMEOUT3=0;
  if (*MENU3==0) return;
  // 清高亮（编辑模式超时）：disp 0x6474('     ')/0x647c, 对应行, 0xb, 0
  (*TIMEOUT)++;
  if (*TIMEOUT>=0x1388) { *TIMEOUT=0; param_sync_live_to_eeprom(); *MENU=1; disp_splash_screen(); return; }
  return;
}
```
**case3 pool**：0x6fcc→TIMEOUT3(0x10001778) 0x6fd0→MENU3(0x10001746) 0x6fd4→TIMEOUT(0x10001764)
0x6fd8→MENU(0x10001744) 0x6fdc→MENU2(0x10001745) 0x6fe0→FLASH 0x4814 0x70d4→0x10001634
0x70d8→0x1000164c 0x70dc→0x1000164d 0x70e0→0x10001650 0x70e4→0x10001654 0x70e8→0x10001655
0x70ec→0x10001656 0x70f0→0x10001657 0x70f4→0x10001659 0x70f8→0x1000165a 0x70fc→0x10001660
0x7100→0x1000163c 0x7104→0x10001638 0x7108→0x10001640 0x710c→0x10001648 0x7110→0x10001644
0x7510→TIMEOUT 0x7514→MENU2 0x7518→0x1000163c 0x751c→0x10001638 0x7520→0x10001640
0x7524→0x10001648 0x7528→0x10001644 0x752c→0x10001634 0x7530→0x1000164c 0x7534→0x1000164d
0x7538→0x10001650 0x753c→0x10001654 0x7540→0x10001655 0x7544→0x10001656 0x7548→0x10001657
0x754c→0x10001659 0x7550→0x1000165a 0x7554→0x10001660 0x7558→TIMEOUT3(0x10001778)
0x795c→0x10001634 0x7960→MENU2 0x7964→0x1000163c 0x7968→0x10001638 0x796c→0x10001640
0x7970→0x10001648 0x7974→FLASH 'V'(0x56) 0x7978→FLASH 0x6038 0x797c→0x10001644
0x7980→FLASH 'A'(0x41) 0x7984→0x1000164c 0x7988→0x1000164d 0x798c→0x10001650
0x7990→0x10001654 0x7994→0x10001655 0x7998→FLASH ' 通讯' 0x79a0→' 本地' 0x79a8→' 定值'
0x79b0→0x10001656 0x79b4→' 点动' 0x79bc→' 自锁' 0x79c4→0x10001657 0x79c8→0x10001659 0x79cc→0x1000165a

---

## 10. SRAM 宏清单（写码时直接使用）

**菜单/状态**：
MENU=0x10001744(byte) MENU2=0x10001745(byte) MENU3=0x10001746(byte) SCAN_STOP=0x10001747(byte)
EVCODE=0x10001748(word) TIMEOUT=0x10001764(word) TIMEOUT2=0x10001760(word) TIMEOUT3=0x10001778(word)
TIMEOUT4=0x10001770(word) IDLE=0x10001768(word) LATCH_OUT=0x1000161c(word) LATCH_IN=0x10001620(word)

**运行/故障**：
FAULT=0x10001624(word) RUN=0x10001628(byte) RUN_REQ=0x1000177d STOP_REQ=0x1000177e STOP_PEND=0x1000177f
STAT1=0x10001781 RESET2=0x10001782 DB_116=0x10001780 DB_117=0x1000177c STATUS=0x100015cc
DISP_MODE=0x100015cd DISP2=0x100015cf 0x10001785(byte) 0x10001750(word) 0x10001754/58/5c(word)
0x100020c0/c1/c2(byte) 0x100016dd(byte) 0x10002000/0x10001ffc/0x10002068/6c/70(word)

**模式/参数**：
CTRL_MODE=0x10001634(byte) DISP_SEL=0x10001655(byte) 启动方式=0x10001656(byte) ESTOP=0x10001657
RESET_MODE=0x10001658 FEEDBACK=0x10001659 INPUT_SEL=0x1000165a 0x1000165b 0x10001660(word START_PHASE)
V_RANGE=0x1000163c(word) A_RANGE=0x10001638(word) 0x10001640(word) 0x10001644(word) 0x10001648(word)
0x1000164c(byte) 0x1000164d(byte) 0x10001650(word) 0x10001654(byte)
TARGET=0x100015a8 MANUAL=0x100015d8 0x100015d4(word 手动TARGET_AMP) TARGET_AMP=0x10001774(word)
V_AMP=0x100015d0 V_AMP2=0x100015b4 FREQ=0x10001788(word) 0x10001590(word) 0x10001594(word)

**计时**：
TICK=0x10001600 MIN_NOW=0x100015fc HOUR_NOW=0x100015f8 HOUR_TOTAL=0x10001604 MIN_TOTAL=0x10001608
0x1000160c/0x10001610/0x10001614/0x10001618(word shadow) 0x100015dc(word shadow)
SYNC_30=0x10001730 SYNC_34=0x10001734 SYNC_2C=0x1000172c

**密码**：PWD_BUF=0x100015f2(6B) PWD_A=0x100015e0(6B) PWD_B=0x100015e6(6B) PWD_C=0x100015ec(6B)

**通讯**：COM_ADDR=0x100016ff BAUD_IDX=0x10001700 PARITY=0x10001704 COM_CHK=0x10001705
BAUD_TBL=0x100017bc(7 word)

**PID**：PID_MODE=0x10001710 0x10001711-0x10001718(byte 槽)

**case63 word 参数**：0x10001698/0x100016a0/0x100016a8/0x100016b0/0x100016b8(word)

**其他**：PHASE_OFF=0x1000162c(word) BAL_ANG=0x10001694(byte) 0x100016c0/0xc4/0xc8/0xcc(保护参数)

---

## 11. case6-1E 解码（2026-08-21 本轮补完，已精读全部 18 case）

> 本轮完成 case6/7/8/B/9/5A/C/14/1E 精读。至此 **18 个 case 段全部读过**。pool 表
> `_pool_case5to1E.txt`(0x8f00-0xac00,174条) + `_pool_case6to1E.txt`(同,0x8f00-0xac00)。
> 各 case 用不同 pool 地址引用同一系统变量：MENU=0x10001744、MENU2=0x10001745、
> MENU3(编辑/导航)=0x10001746、TIMEOUT=0x10001764、TIMEOUT3=0x10001778。

### case6 (MENU==6, 0x8C1A-0x910C)：运行时间查询 + 初始参数密码
pool：0x8f50=MENU(0x10001744) 0x8f3c=MENU2(0x10001745) 0x8f54=0x100015f2(6B数组)
0x8f58=0x100015e6(6B数组) 0x8f5c=0x56dc(FLASH) 0x8f60=0x1000161c(word 计数)
0x8f64=0x10001620(word 计数) 0x8f68=0x4814(FLASH title)
- **key==1**(0x8c22)：MENU2=0；循环 0x8c2e：idx=MENU2；若 0x8f54[idx]==0x8f58[idx]→0x8cda(清零);
  否则显示 0x8f5c(0x56dc)页 + 0x8f60/0x8f64 计数循环到 0x3e8(1000)/0x2710(10000)；
  然后 MENU(0x8f50)=2、MENU2=3、显示 0x8f68(0x4814)4行 → return 0x85d8
- **0x8cda**：0x8f54[MENU2]=0；MENU2++；若<6 循环；清完显示 0x8f6c(空白)页、0x8f60=0
- **key==0xe**(0x8df0)：初始密码流程。MENU2=0；0x8dfc: 0x8f54[idx]==0x8f98(0x100015ec)？
  若等→0x8ea8 清零；不等→显示+计数循环；MENU=2、MENU2=2? ... 0x8ea4 return 0x877a
- **0x9052 (key==4)**：0x93b0(0x10001764)==0；0x93b4(0x10001744)=2(byte)；0x93b8(0x10001745)=3(byte)；
  param_sync(0x35f2)；disp_clear；显示 0x93bc(0x4814)4行 → return 0x8b7c
- **0x90aa (key 数字/0)**：密码数字输入。0x90ae: if(0x93b8(0x10001745)<6){
  0x93c0(0x100015f2)[0x93b8]=key; 显示 char'*'(0x2a) via disp_render_char8(0xb44); 0x93b8++ }
- **0x90de**：0x93b0(0x10001764)++ 超 0x1388(5000) → 0x93b0=0; 0x93b4(0x10001744)=1(byte); splash(0x427c) → return

### case7 (MENU==7, 0x910C-0x9A84)：PID 参数设置（子项 0-8）
> 复杂，需进一步核对项序号。pool：0x93b4=MENU(0x10001744) 0x93b8=MENU3=... 0x93b0=TIMEOUT
> 0x93c4=MENU3(0x10001746) 0x93c8=TIMEOUT3(0x10001778) 0x93cc=0x64d8('5.PID 参数设置')
> 0x93d0=0x10001710(PID_MODE) 0x93d4=0x10001711 0x93d8=0x1000170e 0x93dc=0x10001712
> 0x93e0=0x1000170f 0x93e4=0x10001713 0x93e8=0x10001714 0x93ec=0x10001715 0x93f0=0x10001716
> 0x93f4=0x10001717 0x93f8=0x10001718 0x93fc=0x6aa4(FLASH) 0x9864=0x10001745(MENU2)
> 0x9868=0x10001722 0x986c=0x10001723 0x9870=0x10001724 0x9874=0x10001725 0x9878=0x10001726
> 0x987c=0x10001764 0x9880=0x10001710 0x9884=0x10001717 0x9888=0x10001718 0x988c=0x10001778
> 0x9890=0x6af8(' 快速'.. 4槽) 0x9894=0x10001711 0x9898=0x10001713 0x989c=0x10001715
> 0x98a0=0x10001712 0x98a4=0x10001714 0x98a8=0x10001716
- **key==1**(0x9114)：MENU3(0x93c4) 0/1 切换；==0→TIMEOUT3(0x93c8)=0xfa(250)、==1→0x1f4(500)
- **key==4**(0x9150)：TIMEOUT=0；MENU(0x93b4)=2；MENU_1(0x93b8)=4；param_sync；显示 0x93cc(0x64d8)4行
- **0x9216-0x931c**：PID_MODE(0x93d0) switch 0..4 → 各分支复制 0x93d4/0x93dc/0x93e4/0x93ec/0x93f4
  → 0x93d8/0x93e0（显示缓冲）；0x92ce/0x92e6 显示 PID 子项页（0x93fc 0x6aa4 / 0x9464 0x5ba4 / adr 0x9400 系 GBK）
- **0x931c-0x93a4**：key==2/3/0x16/0x21 导航编辑 PID_MODE(0x93d0) 0..4；随 MENU3 编辑；超时清高亮 0x93c8
- **0x9468-0x95f0**：PID 各子项(0x9864 0x10001745 的 0-7) 加减 + 上限(0x80/0xfa 等)，
  显示子项值 disp_uint4(0xed0)；**包含 PID 系数联动**（0x95f0 用 0x9868/0x986c/0x9874 互相 clamp）
- **0x9614-0x99a0**：TIMEOUT3(0x988c)++ 超 0xfb → 刷新显示 + 清高亮；MENU3==0 且超时 → return case1
- **0x999c-0x9a80**：TIMEOUT(0x9ccc)++ 超 0xc350(50000) → MENU=1 + splash

### case8 (MENU==8, 0x9A84-0x9C5C)：相位参数校准
pool：0x9cd0=MENU(0x10001744) 0x9cd4=0x100015ce 0x9cd8=0x1000162c(PHASE_OFF word)
0x9cdc=0x10001624 0x9ce0=0x100015cc 0x9ce4=0x47dc(FLASH) 0x9ce8=0x10001628
0x9cec=0x10001630 0x9ca8=MENU2(0x10001745) 0x9cc8=0x6474(FLASH 16空格)
0x9ccc=0x10001764(TIMEOUT)
- **0x9a92**：key==3|0x21 → PHASE_OFF(0x9cd8)++ 上限 0x2b0(688)；key==2|0x16 → -- 下限 0x45(69)；
  显示 disp_offset(0x12b0)(PHASE_OFF, 行2, 列7, 高亮1)
- **0x9afc-0x9b7e**：0x9cdc(0x10001624)!=0 或 0x9ce8 切换 0x100015cc 状态(0/1/2) + 显示 0x9ce4(0x47dc)+偏移；
  key==5 启动(0x9ce8=1)、key==6 停机(0x9ce8=0 + 故障处理 0xe79a)
- **0x9b8a (key==4)**：TIMEOUT=0；MENU(0x9cd0)=2；MENU1(0x9ca8)=5(byte)；disp_clear；
  显示 0x9cc8(0x6474)+0x64/0x78/0x8c/0xa0 4行 + 0x9cd8 vs 0x9cec 写 EEPROM reg 0xc9/0xca(0x1e88)
- **0x9c1e**：TIMEOUT(0x9ccc)++ 超 0x3a98(15000) → MENU=1 + splash

### caseB (MENU==0xb, 0x9C5C-0x9D86)：运行时间查询（含 恢复出厂/清零）
pool：0x9cd0=MENU 0x9ccc=TIMEOUT 0x9ca8=MENU2 0x9cc8=0x6474 0x9cc4=MENU3(0x10001746)
0x9cd8=0x1000162c 0x9cec=0x10001630 0xa0f8=0x6514('8.产品版本信息'——复用标题)
0xa0fc=0x100015f8(HOUR_NOW) 0xa100=0x100015fc(MIN_NOW) 0xa104=0x10001604(HOUR_TOTAL)
0xa108=0x10001608(MIN_TOTAL) 0xa10c=0x10001764(TIMEOUT) 0xa110=MENU(0x10001744)
- **key==4**(0x9c64)：TIMEOUT=0；MENU(0x9cd0)=2；6→0x9ca8(0x10001745)=6(byte)；param_sync；
  显示 0x9cc8+0x64/0x78/0x8c 和 0xa0f8(0x6514) 4行 → return
- **key==0x17**(0x9d06)：清零 4 个时间 word(0xa0fc/0xa100/0xa104/0xa108)=0；显示 disp_uint5/0x13e8 清零后时间
- **超时**(0x9d54)：TIMEOUT(0xa10c)++ 超 0x1388 → MENU=1 + splash

### case9 (MENU==9, 0x9D86-0x9E14)：产品版本信息
pool：0xa110=MENU 0xa10c=TIMEOUT 0xa0f8=0x6514('8.产品版本信息') 0xa114=MENU2(0x10001745)
- **key==4**(0x9d8e)：TIMEOUT=0；MENU=2；MENU2(0xa114)=7(byte)；param_sync；disp_clear；
  显示 0xa0f8-0x3c/0x28/0x14/0x00 4行（版本信息 '型号:ST33C'/'版本:V2.0'/'厂商:'/...）
  → return 0x999c
- **超时**(0x9de6)：TIMEOUT(0xa10c)++ 超 0x3a98(15000) → MENU=1 + splash

### case5A (MENU==0x5a=90, 0x9E14-0x9FB8)：电流手动平衡
pool：0xa110=MENU 0xa118=0x100015ce 0xa10c=TIMEOUT 0xa11c=0x10001694(BAL_ANG byte)
0xa120=0x10001624 0xa124=0x47dc 0xa128=0x10001628 0xa12c=0x100015cc 0xa150=0x10001695
0xa114=MENU2(0x10001745)
- **0x9e22**：key==2|0x16 → *0xa11c(0x10001694)++ 上限 0xc7(199)；key==3|0x21 → -- 下限 2；
  显示 disp_signed_angle(0x11bc)(*0x10001694, 行2, 列7, 高亮1)——平衡角
- **0x9e88-0x9f00**：0xa120(0x10001624)!=0 或 0xa128 切换 0x100015cc 状态 + 显示 0xa124(0x47dc)+偏移；
  key==5 启动、key==6 停机(0xe79a)
- **0x9f08 (key==4)**：TIMEOUT=0；MENU=2；8→MENU2(0xa114)=8(byte)；disp_clear；
  显示 adr 0xa130/0xa140(空白) + 0xa11c(0x10001694) vs 0xa150(0x10001695) 写 EEPROM reg 0x1c(0x1e88)
- **0x9f82**：TIMEOUT(0xa10c)++ 超 0x3a98 → MENU=1 + splash

### caseC (MENU==0xc, 0x9FB8-0xA04E)：？(某功能页，较小)
pool：0xa110=MENU 0xa10c=TIMEOUT 0xa0fc/0xa100/0xa104/0xa108=时间4 word
- **key==4**(0x9fc0)：MENU(0xa110)=1(byte)；splash → return
- **key==0x17**(0x9fd2)：清零 4 时间 word + disp_uint5/0x13e8 显示
- **超时**(0xa020)：TIMEOUT++ 超 0x1388 → MENU=1 + splash

### case14 (MENU==0x14=20, 0xA04E-0xA2C8)：运行状态监控页（位标志→状态行）
pool：0xa110=MENU 0xa154=TIMEOUT3(0x10001778) 0xa120=0x10001624 0xa58c=0x10001624(word 位域)
0xa680=0x10001764(TIMEOUT) 0xa684=MENU(0x10001744)
- **key==4**(0xa056)：MENU=1；splash → return
- **0xa068 (非4)**：TIMEOUT3(0xa154)++ 超 0xfa(250) → 显示 0xa158(adr) + 按 0xa120(0x10001624)
  位(bit4/2/1/8) 显示 0xa178(adr)+偏移；0xa58c(0x10001624) word 位域(0x10/0x20/0x40/0x80/
  0x100/0x200/0x400/0x800/0x1000/0x4000/0x8000/0x2000) 各标志位 → 显示对应状态行 0xa590/0xa5a4/
  0xa5b8/0xa5cc/0xa5e0/0xa5f4/0xa608/0xa61c/0xa630/0xa644/0xa658/0xa66c（GBK 状态字符串）
- **0xa29a**：TIMEOUT(0xa680)++ 超 0x1388 → MENU(MENU 0xa684)=1 + splash → return 0x9c1c

### case1E (MENU==0x1e=30, 0xA2C8-0xAB46)：运行主界面（启动/停机/运行模式/实时值）
> **最复杂**。pool：0xa684=MENU(0x10001744) 0xa680=TIMEOUT(0x10001764)
> 0xa688=0x10001628 0xa68c=MENU2(0x10001745) 0xa690=0x10001760 0xa694=0x10001768
> 0xa698=0x4d9c(' 密码:------') 0xa69c=0x10001598 0xa6a0=0x1000159c 0xa6a4=0x100015a0
> 0xa58c=0x10001624 0xa6a8=0x100015cc 0xa6ac=0x47dc 0xa6b0=0x10001634(CTRL_MODE)
> 0xa6b4=0x100015cd 0xa6b8=0x1000177c 0xa6bc=0x10001658 0xa6c0=0x1000177f 0xa6c4=0x1000177e
> 0xa6c8=0x522c 0xa6cc=0x1000161c 0xa6d0=0x10001620 0xa6d4=0x100015cf
> 0xaac4=0x4804 0xaac8=0x10001658 0xaacc=0x1000177c 0xaad0=0x10001782 0xaad4=0x10001624
> 0xaad8=0x10001657 0xaadc=0x1000177d 0xaae0=0x10001628 0xaae4=0x1000177f 0xaae8=0x1000177e
> 0xaaec=0x10001781 0xaaf0=0x100015cd 0xaaf4=0x100015cf 0xaaf8=0x10001634(CTRL_MODE)
> 0xaafc=0x10001747 0xab00=0x10001785 0xab04=0x10001655 0xab08=0x10001600
> 0xab0c=0x100015fc 0xab10=0x100015f8 0xab14=0x10001656 0xab18=0x10001788
> 0xab1c=0x100015a8 0xab20=0x1000163c 0xab24=0x10001774 0xab28=0x10001638
> 0xab2c=0x100015d0 0xab30=0x100015b4 0xab34=0x100015d8 0xab38=0x100015d4
> 0xab3c=0x10001764(TIMEOUT) 0xab40=MENU(0x10001744)
- **key==4**(0xa2d0)：MENU=1；splash；TIMEOUT=0 → return 0x9c1c
- **key==1 且 *0xa688(0x10001628)==0**(0xa2e6)：进入密码页。MENU=0xa(10)；MENU2=0；TIMEOUT=0；
  *0xa690(0x10001760)=0x3c(60)；*0xa694(0x10001768)=0；显示 0xa698(0x4d9c) 密码提示2行 → return
- **0xa32e**：*0xa694(0x10001768)++ 超 0x15e(350) → 显示实时 3 值(0xa69c/0xa6a0/0xa6a4)=0x10001598/9c/a0(disp_uint4)
- **0xa3ac-0xa43c**：CTRL_MODE(0xa6b0) 0/1/2 → 设 0x100015cd 模式(1/2/3) + fio1_pin20_ctrl(0x105c8)/
  fio1_pin21_ctrl(0x105e8) 驱动 LED + 显示 0xa6ac(0x47dc)+偏移(运行模式字符串)
- **0xa43c-0xa50c**：*0xa6b8(0x1000177c)=debounce_p117(0x1b3e)(停机键去抖)；
  若 *0xa58c(0x10001624)!=0(运行中) 且 *0xa6b8==2 → 停机流程（清运行+输出关+等待循环）
- **0xa50c-0xa574**：停机分支：*0xa6bc(0x10001658)!=0 → 清 0x100015cf、CTRL_MODE、设 0x1000177f/
  0x1000177e、fio1_pin2x 驱动、显示停机字符串 0xa6ac+0x20
- **0xa6ea-0xa9c4**：运行状态机（大量位标志 0xaacc/0x1000177c、0xaad8/0x10001657、
  0xaaec/0x10001781 等）+ scan_run_stop(0x19c6)/debounce_p06(0x1b96)/0x1b3e；
  输出控制、实时 V_AMP/FREQ 计算（TARGET_AMP=0x10001774 = 0x100015d8*系数/0x3e8 联动）
- **0xa9d4-0xaa9a**：CTRL_MODE==2 时加减 0x100015d8(TARGET_AMP) 上限 0x3e8/下限 0xa，
  显示 disp_0x143c(新显示函数，3参)；联动 0x100015d4/0x100015d0
- **0xaa9a**：TIMEOUT(0xab3c)++ 超 0x1388 → MENU=0xab40=1 + splash → return 0xa2e4

### case1E 新显示函数
**0x143c**（0xa350/0xaa16/0xaa44 调用，参数 (val,row,col,attr) 如 disp_uint4 风格）——
待确认功能（可能=disp_uint4@0xed0 的等价或带符号显示）。**写码时用 `extern void disp_0x143c(...)` 前向声明 + 占位注释，或确认后替换名**。

## 12. 待办清单

- [x] 补读 case3 刷新显示尾（0x772A-0x7C1A）→ 已读全 0x69D6-0x7C1A（见 §13）
- [x] 精读 case4（保护参数 10 项）
- [x] 精读 case6-1E（全部 18 case 已解码）
- [x] 确认 0x143c / 0x105c8 / 0x105e8 / 0xe42 / 0x11bc / 0x992 身份（§13 表；0x143c 仍待 Ghidra）
- [x] case3 池映射 bin 验证（§13；旧 §9 项8-15 地址已核对为另一池，语义以 §13 为准）
- [x] 写 case3/4/5 到 `firmware/src/07_state_machine.c`（§14；src 已含 entry+case1/A/62/63/2+case3/4/5）
- [x] 改 `firmware/stub.c`：移除 `void state_machine(int param_1){}` 占位；**保留** freq_adjust_sync(0xAB48) 与 func_0x0000aed0 骨架；更新头注释
- [x] `bash build.sh` 编译零错误零警告（text 50184 data 3388 bss 2188）
- [x] case7(PID)/case1E(运行界面) 复杂分支精确核对（case7 → text 54772；case1E → text 56516，均零警告）
- [x] 写 case6/7/8/B/9/5A/C/14/1E（§11 已全解码，池已验证；§14 写码记录）
- [x] 更新本文件状态 + 记忆 + 任务 #36 completed（达成 18/18 case 全部写码）

---

## 13. case3/4/5 池映射核对（2026-08-21，bin 逐字面量验证）

> 本段把 case3/4/5 用到的**全部字面量→SRAM/FLASH 映射**从 `LPC1765.bin` 直接读出，
> **取代 §9 中部分曾漂移的旧注**（旧注把 case3 项8-15 记成 0x10001650/54 等，
> 实测 case3 编辑/刷新用的是另一组 0x70xx/0x75xx/0x79xx 字面量——两者都对，
> 因为 case3 底部池 + 顶部池分别指向不同偏移）。以下为权威映射，写 C 以此为准。

### case3 底部池（0x79xx，刷新显示用）→ SRAM
```
0x795c->0x10001634 0x7960->0x10001745(MENU2) 0x7964->0x1000163c 0x7968->0x10001638
0x796c->0x10001640 0x7970->0x10001648 0x7974->0x56('V') 0x7978->0x6038(' 关闭')
0x797c->0x10001644 0x7980->0x41('A') 0x7984->0x1000164c 0x7988->0x1000164d
0x798c->0x10001650 0x7990->0x10001654 0x7994->0x10001655 0x7998/0x79a0/0x79a8=FLASH(GBK ' 通讯'/' 本地'/' 定值')
0x79b0->0x10001656 0x79b4/0x79bc=FLASH(' 点动'/' 自锁') 0x79c4->0x10001657 0x79c8->0x10001659 0x79cc->0x1000165a
```
### case3 顶部池（0x70xx/0x75xx，编辑用）→ SRAM
```
0x70d4->0x10001634 0x70d8->0x1000164c 0x70dc->0x1000164d 0x70e0->0x10001650
0x70e4->0x10001654 0x70e8->0x10001655 0x70ec->0x10001656 0x70f0->0x10001657
0x70f4->0x10001659 0x70f8->0x1000165a 0x70fc->0x10001660
0x7100->0x1000163c 0x7104->0x10001638 0x7108->0x10001640 0x710c->0x10001648 0x7110->0x10001644
0x7510->0x10001764 0x7514->0x10001745(MENU2) 0x7518->0x1000163c 0x751c->0x10001638
0x7520->0x10001640 0x7524->0x10001648 0x7528->0x10001644 0x752c->0x10001634(CTRL_MODE)
0x7530->0x1000164c 0x7534->0x1000164d 0x7538->0x10001650 0x753c->0x10001654
0x7540->0x10001655 0x7544->0x10001656 0x7548->0x10001657 0x754c->0x10001659
0x7550->0x1000165a 0x7554->0x10001660 0x7558->0x10001778(TIMEOUT3)
```
### case3 控制变量
```
0x6a14->0x10001744(MENU) 0x6bcc->0x10001746(MENU3) 0x6bc8->0x10001764(TIMEOUT)
0x6fcc->0x10001778 0x6fd0->0x10001746 0x6fd4->0x10001764 0x6fd8->0x10001744 0x6fdc->0x10001745
0x7dcc->0x6048 0x7dd0->0x1000165a 0x7dd4->0x10001745 0x7dd8->0x10001660 0x7ddc->0x10001778
0x7de0->0x10001746 0x7de4->0x6474 0x7de8->0x1000163c 0x7dec->0x10001648 0x7df0->0x10001638
0x7df4->0x10001644 0x7df8->0x10001764 0x7dfc->0x10001634 0x7e00->0x1000164c 0x7e04->0x10001744
```

### case3 16 项结构（bas_param，4 屏×4 项，MENU2=项号 0-15）
|项|SRAM|宽|上限|刷新显示|
|--|----|--|--|--------|
|0|0x10001634 CTRL_MODE|byte|2|enum ' 通讯'(6594)/' 本地'(659c)/' 定值'(65a4), 高亮行0 + fio1_pin20/21|
|1|0x1000163c V_RANGE|word|0x1770|disp_uint4 行1|
|2|0x10001638 A_RANGE|word|0x1770|行2|
|3|0x10001640|word|0x1770|行3|
|4|*0x1000163c 对照 *0x10001648|—|—|val>=lim→disp_uint4(lim)+'V'; 否则' 关闭'|
|5|*0x10001638 对照 *0x10001644|—|—|val>=lim→disp_uint4(lim)+'A'; 否则' 关闭'|
|6|0x1000164c|byte|0xc8(200)|disp_uint4 行2? (item6)|
|7|0x1000164d|byte|0xc8|item7|
|8|0x10001650|word|0xb4(180)|item8 (相位限制)|
|9|0x10001654|byte|0xa0(160)|disp_signed_angle (主从偏移 100±60°)|
|10|0x10001655 DISP_SEL|byte|2|' 通讯'/' 本地'/' 定值'|
|11|0x10001656|byte|1|' 点动'/' 自锁'|
|12|0x10001657 ESTOP|byte|2|0x7978-0x20/-0x18/-0x10 三值|
|13|0x10001659 FEEDBACK|byte|1|0x7978/+0x8 二值|
|14|0x1000165a INPUT_SEL|byte|1|0x6048/+0x8 |
|15|0x10001660|word|0xb4|item15 (起始相位)|

**case3 流程**：
1. `if(*MENU!=3) return`（0x69da）
2. **navigation（编辑关，*MENU3==0，key 2/3）**：MENU2 0-15 游标；按 MENU2 范围输出 4 行屏标题
   （0-3→0x6540/6554/6568/657c；4-7→0x6fe4/6ff8/700c/7020；8-11→0x7034/7048/705c/7070；12-15→0x7084/7098/70ac/70c0），
   并高亮当前项行。
3. **编辑（*MENU3==1，key 2/3/0x16/0x21）**：按当前项增/减值（上限见上表；项1-5 word 上限 0x1770，
   项4/5 钳到 V_RANGE/A_RANGE+1）。
4. **刷新**：按 MENU2 分段显示当前页 4 项数值 + 单位（'V'/'A'/' 关闭'/' 开启'），高亮当前项。
5. **key==1**：切换编辑模式（MENU3 0↔1，并设 TIMEOUT3=0xfa / 0x1f4）。**key==4**：TIMEOUT=0；
   MENU=2；MENU2=0；param_sync_live_to_eeprom()；disp_clear()；显示 0x4814 回主菜单 4 行。
6. **超时尾（0x7b2e-0x7c1a）**：TIMEOUT3>0x1f4→清0；MENU3==0→return；否则 MENU2 当前页清高亮
   （disp 0x6474/0x7068 对应行）；TIMEOUT++ 超 0x1388→MENU=1+splash。 **注意 MENU3==0 时** `b 0x4ba8`（回 case2 恢复），
   MENU3!=0 时走 `b 0x4a4a`（分发重入）。

### case4（0x7C1A-0x8780，保护参数 10 项）与 case5（0x8780-0x8C1A，通讯 4 项）
两 case **已完整解码**（见 §4 之前的进度记录）：case4 pool 0x7dd4/0x7ddc/0x7de0/0x7df8/0x7e04/0x8284-0x8690、
case5 pool 0x8b00-0x8f40；项：
- case4：item0-3=过压/过压时间/欠压/欠压时间；item4-7=IF/CT 过载；item8=缺相；item9=三相平衡。
  导航/编辑/刷新三态，刷新单元 'V'/'S'/'A'/'%/开关'，字符串 0x6a94(' 开启')/0x6038(' 关闭')。
- case5：item0=通讯地址(COM_ADDR 0x100016ff)、item1=波特率(BAUD_IDX 0x10001700→BAUD_TBL 0x100017bc 查表)、
  item2=校验(PARITY 0x10001704)、item3=校验开关(COM_CHK 0x10001705)。

### 辅助函数身份（本轮确认，消除"臆造"风险）
|BL 目标|真实函数|来源|
|------|--------|-----|
|0x992|`disp_clear(void)`|02_lcd_display.c:136|
|0xd3c|`disp_string(int,undef4,uint,undef4)`|:300|
|0xe42|`disp_number3(int,undef4,int,undef4)` 3位 |:364|
|0xed0|`disp_uint4(uint,undef4,int,undef4)` 4位 |:383|
|0x11bc|`disp_signed_angle(int,undef4,int,undef4)` |:476|
|0x105c8|`fio1_pin20_ctrl(int)`|10_relay_led.c:49|
|0x105e8|`fio1_pin21_ctrl(int)`|:61|

### 写码待决（不臆造，标注后确认）
- **item1 与 item4 都读 0x1000163c**：语义歧义——是"量程/当前值"在 2 个菜单复用，还是项号映射需微调。
  写 C 时按字面量照抄两处（忠实），语义标签留 W7 核对。
- **key 2/3 与递增/递减实际方向**：反汇编 key==2/0x16→某段递增、key==3/0x21→另一段递减，与全局
  "2=减/3=加"语义相反；写 C 一律按反汇编方向（`if(key==2||key==0x16){...++ } if(key==3||key==0x21){...--}`），
  不因语义名"修正"。行为等价以 W7 为准。


---

## 14. case3 写码完成（2026-08-21，07_state_machine.c）
> 本轮把 case3 从"已解码待写"落到 `firmware/src/07_state_machine.c` 并编译通过。素材全部来自
> 已落盘 bin/反汇编（未使用 MCP 臆造——MCP 8080 全 404，改用 tools/ 落盘 txt）。

**写入内容（entry+case1/A/62/63/2 已有；追加 case3）**：
- 顶部（`void state_machine` 之前）新增 static `sm3_draw_item(uint32_t it, uint32_t row)`：
  按项号渲染当前项值/枚举到 `(row,0xb)`。item0 含 fio1_pin20/21_ctrl 副作用的三种控制方式选择。
  所有值串地址/枚举宽度 bin 校验（见 §13）。
- case3 结构（`if (*MENU==3)`）：
  1. `key==1`：翻转 MENU3（0↔1，编辑态切换）+ 置 `*TIMEOUT3`。
  2. `key==4`：同步 live→EEPROM、disp_clear、画 4 行 type2 子菜单（0x4814..0x4844）、`*MENU=2`。
  3. `key==2/3 && *MENU3==0`：导航 MENU2(0..15)，重绘新页 4 行标题（0x6540/0x6fe4/0x7034/0x7084 组）
     + `sm3_draw_item`。
  4. `key in {2,0x16,3,0x21} && *MENU3==1`：修改。item1-5 数字项（key==0x16 快加+5 / 0x21 快减-5 /
     2 +1 / 3 -1，上限 0x1770 或 *V/A_RANGE+1，下限 0xb/0xf）；item0,6-15 枚举/数项（2,0x16 加 /
     3,0x21 减，各带 clamp）。改动后 `sm3_draw_item(it,row)`。
  5. 公共尾：`(*TIMEOUT)++; if(*TIMEOUT>=0x1388){*TIMEOUT=0;*MENU=1;disp_splash_screen();} return;`
- **重构修订**：移除最初"无条件标题绘制"（会抹掉 col0xb 数值）；标题绘制下沉到 nav 分支，值绘制统一
  收敛到 sm3_draw_item。消除了 key==1/4 时重绘标题导致数值栏消失的缺陷。
- **stub.c**：删除 `void state_machine(int param_1){}` 占位（保留 freq_adjust_sync + func_0x0000aed0）；
  头注释更新为"二者均已由 src 侧还原"。
- **编译**：`bash build.sh` 零错误零警告产出 firmware.elf/hex/bin（text 47568 data 3388 bss 2188）。
  （编译期修：case3 中 3 处裸 `TIMEOUT3 =` 改为 `*TIMEOUT3 =`；sm3_draw_item 的 `else...;break;`
  误缩进分号告警由 break 独立行消除。）

**遗留（W7 行为等价核对项）**：
- 页切换只重绘当前项值，同页其它 3 项值依赖其先前显示（未强制全页刷新）。
- §9 尾部两条语义歧义（item1/item4 均读 0x1000163c；key 2/3 与增减方向）——本实现按反汇编
  字面量/方向照抄，语义标签留 W7 核对。

---

## 14. case4 + case5 写码记录（2026-08-21 会话 3，build.sh 零警告）

**case4（0x7C1A-0x8780）保护参数屏**（`if (*MENU == 4)`）：10 项（MENU2=0-9，3 页：页0=0-3/页1=4-7/页2=8-9），MENU3=0导航/1编辑。
- **辅助**：`sm4_draw_value(it,row,attr)`（单项目值/单位渲染）+ `sm4_draw_page(it)`（整页 10 项 3 页，高亮当前项）。
- **entry**：`if(*MENU!=4)` 下一 case。
- **key==1**：`(*MENU3)++ clamp1；*TIMEOUT3=(*MENU3==0)?0xfa:0x1f4`。
- **key==4**：`*MENU=2;*MENU2=0;param_sync;disp_clear;disp 0x65bc/...（按页）+ 0x5ba4 空格页尾`。
- **nav**(key2/3,MENU3==0)：MENU2±，按页画标题（页0=0x65bc/0x65d0/0x65e4/0x65f8；页1=0x7e10/0x7e24/0x7e38/0x7e4c；页2=0x7e60/0x7e74/0x5ba4/0x5ba4）。
- **edit**(key{2,0x16,3,0x21},MENU3==1)：改 10 项。word 项(0/2/4/6) key==0x16/0x21 走 ±5，byte 项恒 ±1；item0/2/4/6 为 0 时显示 ' 关闭'(0x6038)；item8 缺相开/关(0x6a94/0x6038)；item9 三相平衡 ≥0xa 显数值+'%'(0x25) 否则 ' 关闭'。
- **公共尾**：`(*TIMEOUT3)++; if==0xfb sm4_draw_page(*MENU2); if>0x1f4{*TIMEOUT3=0;if(*MENU3==0)return;} (*TIMEOUT)++; if>=0x1388{...splash} return;`

**case5（0x8780-0x8C1A）通讯屏**（`if (*MENU == 5)`）：4 项单页（MENU2=0-3），MENU3=0导航/1编辑。
- **辅助**：`sm5_draw_value(it,attr)`（row=it）+ `sm5_draw_page(it)`（4 项循环）。
- **key==1**：`(*MENU3)++; if(*MENU3>1)*MENU3=0; *TIMEOUT3=(*MENU3==0)?0xfa:0x1f4;`
- **key==4**：`*TIMEOUT=0;*MENU=2;*MENU2=2;param_sync_live_to_eeprom();disp_clear();disp 0x4814(0)/0x4824(1)/0x4834(2,attr1)/0x4844(3)`。
- **nav**(key2/3,MENU3==0)：MENU2± clamp[0,3]；`disp 0x6a18(0)/0x6a2c(1)/0x6a40(2)/0x6a54(3)` 全 attr0；`*TIMEOUT3=0xfa`。
- **edit**(key{2,0x16,3,0x21},MENU3==1)：
  - **item0 COM_ADDR**(0x100016ff,byte)：inc `if(>=0xf6)=0xf6; ++`；dec `if(>1)--`。
  - **item1 BAUD_IDX**(0x10001700,word)：inc `if(>=7)=6; ++`；dec `if(!=0)--`。查 **BAUD_TBL**(0x100017bc)=[2400..115200] 8 表项。
  - **item2 PARITY**(0x10001704,byte)：inc `++ if>3=3`；dec `if(>0)--`。显 0x6a78/0x6a80/0x6a88/0x8b2c('1 ST0P')。
  - **item3 COM_CHK**(0x10001705,byte)：inc=1；dec=0。显 0x6a94(' 开启')/0x6038(' 关闭')。
- **公共尾**：`(*TIMEOUT3)++; if==0xfb && *MENU2<4 sm5_draw_page(*MENU2); if>0x1f4{*TIMEOUT3=0; if(*MENU3==0)return; 清当前行(0x6474/0x8f44 空格 按 MENU2);} (*TIMEOUT)++; if>=0x1388{*TIMEOUT=0;*MENU=1;splash;} return;`

**构建**：`bash build.sh` 零错误零警告——text 50184 data 3388 bss 2188（case4/5 相较 case3 态 47568 增 ~2600B）。
**宏新增**（第 110-115 行）：`COM_ADDR(0x100016ff,u8*)/BAUD_IDX(0x10001700,u32*)/PARITY(0x10001704,u8*)/COM_CHK(0x10001705,u8*)/BAUD_TBL(0x100017bc,u32*)/PID_MODE(0x10001710,u32*)`。

**遗留（W7 行为等价核对项）**：
- 编辑超时尾的清行逻辑（case5 0x8b7e-0x8bec，按 MENU2 清当前行：0/4→row0、1/5→row1、2/6→row2、3/7→row3；
  0x6474=行满空格、0x8f44=4 空格）作"闪烁"处理照抄；case4 尾未含此清行，二者差异留 W7。
- case4/5 的 flash 字符串（0x4814/0x6a18/0x6038/0x6a94/0x8b2c 等）以**原始 flash 地址**直传 disp_string，
  在 GCC 重定位下字串表需 W7 建立（地址→C 串映射）后才能正确显示。当前按 case1-3 同模式（零警告编译，行为待验）。

## 14. case6-1E 全部写码完成（2026-08-21 会话，build.sh 零警告）——**18/18 case 齐，W1b 收口**

> 本轮把剩余 case 逐段对照 `tools/_sm_case*.txt` 写入 `firmware/src/07_state_machine.c` 并编译通过。
> 分布（§8 待办 813/814 均已达成）：先 case6/7/8/B/9/5A/C/14（text 54772），最后 case1E（text 56516，零警告）。
> 每 case 分支方向一律以反汇编 `bcc`/`bne`/`beq`/`cbnz` 目标为准（bcc=无符号<跳，故 C 用 `>=` 进入分支）。

**无新增宏**——case6-1E 全部复用已有宏（MENU/MENU2/MENU3/TIMEOUT/TIMEOUT2/TIMEOUT3/IDLE/FAULT/RUN/
RUN_REQ/STOP_REQ/STOP_PEND/STAT1/RESET2/DB_117/STATUS/DISP_MODE/DISP2/CTRL_MODE/DISP_SEL/RESET_MODE/
ESTOP/V_RANGE/A_RANGE/TARGET/MANUAL/TARGET_AMP/V_AMP/V_AMP2/FREQ/TICK/MIN_NOW/HOUR_NOW/LATCH_OUT/LATCH_IN/SCAN_STOP）。
未定义地址在 case1E 内联（见下）。**关键修正**：case7 的 PID 槽（0x10001710-18）用 SM7B 字节宏内联，
**禁止用 `*PID_MODE`**（PID_MODE 是 u32*，会用 word 写 0x10001710 污染相邻 PID 槽）。

**case6（MENU==6，0x8C1A-0x910C，运行时间 + 初始参数密码）**：key1 校验 PWD_B(0x100015e6) vs PWD_BUF(0x100015f2)，
错误→disp_string(0x56dc,1,4,0)+sm6_delay_loop+回菜单 4 行；成功→三个延时+0x8f6c/8f78/8f88 提示+i2c_write_reg(0,5)/(0,6)+`for(;;){}`。
key0xe 校验 PWD_C(0x100015ec)，成功→i2c_write_reg(0,5/6/7/8)+`for(;;){}`。key4→TIMEOUT=0,MENU=2,MENU2=3,param_sync,回菜单。
key>0→PWD_BUF[MENU2]=key,disp_render_char8(0x2a,1,*,0),MENU2++。超时尾 0x1388→MENU=1+splash。

**case7（MENU==7，0x910C-0x9A84，PID 参数设置，最复杂）**：MENU3 0/1 翻编辑；key4 回菜单+按 PIDMODE 复制当前槽到 0x1000170e/0f；
key∈{2,3,0x16,0x21} 则 MENU3==1→编辑（增：PIDMODE--/PID_G/H++clamp0x80/0x10001722-26 各自++clamp 联动；减：PIDMODE++clamp4/
0x10001722-26 各自--）；MENU3==0&&PIDMODE==4→导航。刷新 TIMEOUT3==0xfb：MENU2<4 画 PID 模式名+PI/D 值（at行=PIDMODE 槽）；
MENU2>=4&&<8 画 0x10001722-25；>=8&&<0xc 画 0x10001726。超时清 >0x1f4。超时尾 0xc350→MENU=1+splash。

**case8（MENU==8，0x9A84-0x9C5C，相位校准）**、**case5A（MENU==0x5a，0x9E14-0x9FB8，电流手动平衡）**：
0x100015ce=1；PHASE_OFF/BAL_ANG 增(clamp0x2b0/0xc7)减(clamp0x45/0x2)显示；故障/运行状态行；key5/6 启停机；
key4 回菜单+写 EEPROM reg 0xc9/0xca（case8）/0x1c（case5A 存 0x10001695）；超时尾 0x3a98。

**caseB（MENU==0xb，0x9C5C-0x9D86）**、**caseC（MENU==0xc，0x9FB8-0xA04E）**：key4 回菜单；key0x17 统计清零
（HOUR/MIN_NOW/TOTAL=0）；超时尾 0x1388。

**case9（MENU==9，0x9D86-0x9E14，版本信息）**：显示 0x6514-0x3c/0x28/0x14/0x6514 四行；key4 回菜单；超时尾 0x3a98。

**case14（MENU==0x14，0xA04E-0xA2C8，运行状态监控）**：key4 回菜单；TIMEOUT3++，>0xfa 重绘（标题 0xa158 + FAULT 位与各故障串 0xa164/0xa178-0xa66c row2）；超时尾 0x1388。

**case1E（MENU==0x1e，0xA2C8-0xAB44，运行主界面）**——最后写入（text 56516）：
- 新增 6 个内联地址：s17=0x10001785(byte,STAT2 类)/s656=0x10001656(byte,启动使能)/v98=0x10001598/v9c=0x1000159c/
  va0=0x100015a0(均 u32)/v5d4=0x100015d4(u32)。
- key4→MENU=1+splash+TIMEOUT=0；key1&&RUN==0→MENU=0xa,MENU2=0,TIMEOUT2=0x3c,IDLE=0+0x4d9c 两行。
- IDLE++：>=0x15e 重显 0x10001598/9c/a0 三值(row0-2 col9)；FAULT→STATUS=0+0x47dc；RUN==0&&STATUS!=1→STATUS=1+0x47dc+0xc；
  CTRL_MODE==0/1/2 切 DISP_MODE 1/2/3+继电器 fio1_pin20/21_ctrl+0x47dc+0x20/0x28/0x30。
- 状态机：DB_117=debounce_p117()（0x1b3e）；FAULT&&DB_117==2&&RESET_MODE==0→停机斜坡（LATCH_OUT 双层延时 0xbb8/0x7d0 喂狗）+
  0x522c/0x523c+`for(;;){}` 锁定；RESET_MODE==1→STOP 段（DISP2/CTRL_MODE 切换+继电器+0x47dc+0x20/0x4804）；RESET_MODE==2→RESET2 按 DB_117 置。
  DB_117=debounce_p06()（0x1b96，急停）；FAULT==0&&DB_117==2&&ESTOP==0→RUN_REQ=1...return；ESTOP==1→断（0x47fc/0x4804）；ESTOP==2→RESET2；
  RESET_MODE!=2&&ESTOP!=2→RESET2=0；SCAN_STOP=scan_run_stop()（0x19c6）；启停条件 key5/6 + SCAN_STOP==7/8 + s656 使能。
- 幅值：DISP_SEL==0→TARGET=FREQ,TARGET_AMP=FREQ×V/A_RANGE/1000,V_AMP=V_AMP2=TARGET_AMP；==1→V_AMP=V_AMP2；
  ==2→MANUAL 增(0x3e8/0xa clamp)减(0xa→0x1)disp_fixed_1dec(MANUAL,0,9,0)+TARGET=MANUAL+0x100015d4=MANUAL×V/A_RANGE/1000。
- 超时尾 0x1388→MENU=1+splash；return。

**函数身份确认（§8 line 808 遗留闭合）**：0x143c=disp_fixed_1dec(uint,undef4,int,undef4)（02_lcd_display.c:557，
固定 1 位小数 XX.X）；0x1b3e=debounce_p117()（03_input_debounce.c:313，P1.17 复位去抖）；0x1b96=debounce_p06()
（P0.6 急停去抖）；0x105c8=fio1_pin20_ctrl（10_relay_led.c:33，P1.20 控制输出）；0x105e8=fio1_pin21_ctrl（P1.21）。
均已在 src 有实现，case1E 直接调用，无需新定义。

**18/18 全部写码**：entry+case1/A/62/63/2/3/4/5/6/7/8/B/9/5A/C/14/1E。`bash build.sh` **零错误零警告** text 56516。

**遗留核对（W7 行为等价，非 W1b 阻塞）**：
- case1E/6/7/8/B/9/5A/C/14 的 flash 字符串均以**原始 flash 地址**直传 disp_string，GCC 重定位后需 W7 字串表（地址→C 串）才能正确显示。
- case1E 故障停机斜坡的 `for(;;){}` 锁定、case7 的 SM7B 字节指针，均按反汇编照抄，语义留 W7/W8 硬件实测。
- case1E 中 0x10001785/0x10001656/0x100015d4 无命名宏，用内联指针（宽度按 ldrb/ldr 区分），若后续要用可升为宏。
