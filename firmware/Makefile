# =============================================================================
# firmware/Makefile — LPC1765 固件骨架（目标B）构建
# 用法：make        （构建 firmware.elf/.hex/.bin）
#      make clean
# 说明：src/*.c 为按功能组织的活动固件源码。
# =============================================================================
TC := "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin"
CC      := $(TC)/arm-none-eabi-gcc
AS      := $(TC)/arm-none-eabi-gcc
OBJCOPY := $(TC)/arm-none-eabi-objcopy
OBJDUMP := $(TC)/arm-none-eabi-objdump
SIZE    := $(TC)/arm-none-eabi-size

COMMON_CFLAGS := $(strip $(file <compiler_flags.txt))
CFLAGS  := $(COMMON_CFLAGS)
ASFLAGS := $(COMMON_CFLAGS)
LDFLAGS := -T lpc1765.ld -nostdlib
LDLIBS  := -lgcc

TARGET  := firmware
OBJS    := startup.o data_image.o \
           $(patsubst src/%.c,src/%.o,$(wildcard src/*.c))

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin

$(TARGET).elf: $(OBJS) lpc1765.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) -Wl,--gc-sections -Wl,-Map,$(TARGET).map

# 汇编文件（startup.s / data_image.s）
%.o: %.s
	$(AS) $(ASFLAGS) -c -o $@ $<

# src/ 下 C 模块
src/%.o: src/%.c compiler_flags.txt inc/reg.h inc/types.h inc/firmware_types.h inc/firmware_api.h inc/firmware_state.h inc/firmware_parameters.h inc/firmware_display_strings.h inc/firmware_input_pins.h inc/consts.h
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
