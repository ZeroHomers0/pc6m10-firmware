#ifndef FIRMWARE_INPUT_PINS_H
#define FIRMWARE_INPUT_PINS_H

/*
 * 输入扫描使用的 GPIO 位掩码。掩码值来自原板级连接和当前固件的访问
 * 顺序；业务判断使用这些名称，避免在去抖逻辑中重复出现裸十六进制数。
 */

/* 旋转编码器六相输入 */
#define INPUT_ENCODER_A_FIO1_MASK           0x00080000u
#define INPUT_ENCODER_B_FIO1_MASK           0x00040000u
#define INPUT_ENCODER_A_FIO0_MASK           0x40000000u
#define INPUT_ENCODER_B_FIO0_MASK           0x20000000u
#define INPUT_ENCODER_A_FIO3_MASK           0x02000000u
#define INPUT_ENCODER_B_FIO3_MASK           0x04000000u

/* 启停按键 */
#define INPUT_RUN_FIO0_MASK                 0x10000000u
#define INPUT_STOP_FIO0_MASK                0x08000000u

/* 其他面板输入 */
#define INPUT_P09_FIO0_MASK                 0x00000200u
#define INPUT_P06_FIO0_MASK                 0x00000040u
#define INPUT_P116_FIO1_MASK                0x00010000u
#define INPUT_P117_FIO1_MASK                0x00020000u
#define INPUT_P02_FIO0_MASK                 0x00000004u
#define INPUT_P03_FIO0_MASK                 0x00000008u

#endif /* FIRMWARE_INPUT_PINS_H */
