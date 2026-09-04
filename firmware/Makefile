# =============================================================================
# firmware/Makefile — LPC1765 固件骨架（目标B）构建
# 用法：make        （构建 firmware.elf/.hex/.bin）
#      make clean
# 说明：src/*.c 为反编译源码的可编译化副本（阶段4 逐模块修复后加入）。
# =============================================================================
TC := "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin"
CC      := $(TC)/arm-none-eabi-gcc
AS      := $(TC)/arm-none-eabi-gcc
OBJCOPY := $(TC)/arm-none-eabi-objcopy
OBJDUMP := $(TC)/arm-none-eabi-objdump
SIZE    := $(TC)/arm-none-eabi-size

CPU     := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
CFLAGS  := $(CPU) -Os -ffreestanding -fno-builtin -Wall -I. -Iinc
CFLAGS  += -Wno-unused-but-set-variable -Wno-unused-variable -Wno-pointer-sign
ASFLAGS := $(CPU)
LDFLAGS := -T lpc1765.ld -nostdlib
LDLIBS  := -lgcc

TARGET  := firmware
OBJS    := startup.o data_image.o stub.o \
           $(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin

$(TARGET).elf: $(OBJS) lpc1765.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) -Wl,--gc-sections -Wl,-Map,$(TARGET).map

# 汇编文件（startup.s / data_image.s）
%.o: %.s
	$(AS) $(ASFLAGS) -c -o $@ $<

# src/ 下 C 模块
src/%.o: src/%.c inc/reg.h inc/types.h inc/firmware_types.h inc/firmware_api.h inc/firmware_state.h inc/firmware_parameters.h inc/consts.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 根目录 C（stub.c）
%.o: %.c inc/reg.h inc/types.h inc/firmware_types.h inc/firmware_api.h inc/firmware_state.h inc/firmware_parameters.h inc/consts.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

%.bin: %.elf
	$(OBJCOPY) --gap-fill 0xFF -O binary --only-section=.isr_vector --only-section=.crp --only-section=.text --only-section=.fw_image $< $@

size: $(TARGET).elf
	$(SIZE) $<

dump: $(TARGET).elf
	$(OBJDUMP) -D $< > $(TARGET).lst

clean:
	rm -f *.o src/*.o $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).map $(TARGET).lst

.PHONY: all clean size dump
