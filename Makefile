
# Project name
TARGET = stm32f4-rtos-datalogger

# Toolchain
CC = arm-none-eabi-gcc
AS = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Sources
CORE_SOURCES   = $(wildcard Src/*.c)
DRIVER_SOURCES = $(wildcard Src/drivers/*.c)
APP_SOURCES    = $(wildcard Src/app/*.c)
LIB_SOURCES    = Lib/fatfs/ff.c
KERNEL_DIR     = Lib/FreeRTOS-Kernel
KERNEL_PORT    = $(KERNEL_DIR)/portable/GCC/ARM_CM4F
KERNEL_SOURCES = $(addprefix $(KERNEL_DIR)/,tasks.c queue.c list.c stream_buffer.c) $(KERNEL_PORT)/port.c
C_SOURCES   = $(CORE_SOURCES) $(DRIVER_SOURCES) $(APP_SOURCES) $(LIB_SOURCES) $(KERNEL_SOURCES)
ASM_SOURCES = Startup/startup_stm32f407vgtx.s

# Linker script
LDSCRIPT = STM32F407VGTX_FLASH.ld

# MCU flags
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Compiler flags
CFLAGS = $(MCU) -Wall -g -O0 -std=gnu11
CFLAGS += -DSTM32F407xx -I./Inc -I./Lib/fatfs -I$(KERNEL_DIR)/include -I$(KERNEL_PORT)
CFLAGS += -MMD -MP

# Linker flags
LDFLAGS = $(MCU) -T$(LDSCRIPT) --specs=nosys.specs -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# Build directory
BUILD_DIR = build

# Object files mirror the source tree under build/
OBJECTS = $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(MCU) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f board/stm32f4discovery.cfg -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)

.PHONY: all flash clean
