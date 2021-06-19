
# make output a little less noisy
# use with "make clean all -s" for maximum impact
VERBOSE=@
VERBOSE_REDIRECT= > /dev/null 2>&1

DOCKER_ARGS=--rm -t -v `pwd`:/build/stm32 -u `id -u ${USER}`:`id -g ${USER}`
STM_DOCKER=docker run ${DOCKER_ARGS} stm32-build
STM_DOCKER_I=docker run -i ${DOCKER_ARGS} stm32-build

# define a symbol MAKE_OS for the current operating system
# WINDOWS | LINUX | OSX
ifeq ($(OS),Windows_NT)
    MAKE_OS = WINDOWS
    ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
        CPU=AMD64
    endif
    ifeq ($(PROCESSOR_ARCHITECTURE),x86)
        CPU=IA32
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        MAKE_OS=LINUX
    endif
    ifeq ($(UNAME_S),Darwin)
        MAKE_OS=OSX
    endif
endif

# HW Version Info
# USAGE: make clean all USE_HW=v0.2 for v0.2 VEXTREME build
# USAGE: make clean all USE_HW=v0.3 for v0.3 VEXTREME build
ifndef HW_VER_VAL
HW_VER_VAL?=255
endif
ifeq ($(USE_HW),v0.1)
HW_VER_VAL = 1
HW_VER_ARGS="USE_HW=v0.1"
else ifeq ($(USE_HW),v0.2)
HW_VER_VAL = 2
HW_VER_ARGS="USE_HW=v0.2"
else ifeq ($(USE_HW),v0.3)
HW_VER_VAL = 3
HW_VER_ARGS="USE_HW=v0.3"
endif
CFLAGS += -DHW_VER=$(HW_VER_VAL)
ASFLAGS += -DHW_VER=$(HW_VER_VAL)

ifeq ($(USE_UF2),1)
UF2_VAL=1
UF2_ARGS="USE_UF2=1"
LDSCRIPT=stm32f411rct6-uf2.ld
else
UF2_VAL=0
UF2_ARGS="USE_UF2=0"
LDSCRIPT=stm32f411rct6.ld
endif

BINARY=stm32
OBJS=main.o menu.o romemu.o msc.o xprintf.o usb_msc.o ff_diskio.o \
		fatfs/ff.o fatfs/option/unicode.o flash.o led.o delay.o \
		highscore.o settings.o

LIBNAME = opencm3_stm32f4
ARCH_FLAGS = -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -mcpu=cortex-m4 -DSTM32F4

PREFIX	= arm-none-eabi
CC		= $(PREFIX)-gcc
LD		= $(PREFIX)-gcc
OBJCOPY	= $(PREFIX)-objcopy
OBJDUMP	= $(PREFIX)-objdump
# Uncomment this line if you want to use the installed (not local) library.
#TOOLCHAIN_DIR = `dirname \`which $(CC)\``/../$(PREFIX)
TOOLCHAIN_DIR=../libopencm3
CFLAGS		+= -O3 -g -Wextra -Wshadow -Wimplicit-function-declaration -Wredundant-decls \
		-Wmissing-prototypes -Wstrict-prototypes -fno-common -ffunction-sections -fdata-sections \
		-MD -Wall -Wundef -I$(TOOLCHAIN_DIR)/include $(ARCH_FLAGS)
ASFLAGS	+= $(ARCH_FLAGS)

LDFLAGS		+= --static -nostartfiles -L$(TOOLCHAIN_DIR)/lib  \
		   -Xlinker -Map=$(BINARY).map -T$(LDSCRIPT) -Wl,--gc-sections -mthumb $(CFLAGS) -lm \
			-Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -l$(LIBNAME)

#include libopencm3/ld/Makefile.linker

.SUFFIXES: .elf .bin .hex .srec .list .images
.SECONDEXPANSION:
.SECONDARY:

# Unzipped release files to code/releases/
# Specify release version like so
# make flash-bin HW_VER=v0.3 SW_VER=v0.24
flash-release:
	dfu-util -a 0 -d 0483:df11 -s 0x08000000 -D ../releases/$(SW_VER)/stm32/hardware-$(HW_VER)/stm32-sw.$(SW_VER)-test.hw.$(HW_VER).bin

# Specify bin like so
# make flash-bin BIN=../releases/v0.24/stm32/hardware-v0.3/stm32-sw.v0.24-test.hw.v0.3.bin
flash-bin:
	dfu-util -a 0 -d 0483:df11 -s 0x08000000 -D $(BIN)

all: menu.ld docker

docker-build: Dockerfile
	docker build . -t stm32-build

docker:
	${STM_DOCKER} make images $(HW_VER_ARGS) $(UF2_ARGS)

uf2: $(BINARY).bin
	${STM_DOCKER} uf2conv.py -b 0x08008000 -o $(BINARY).uf2 -c $(BINARY).bin

# '$ make bash' puts you in the docker container interactive shell for debugging
bash:
	${STM_DOCKER_I} bash

images: $(BINARY).images
flash: $(BINARY).flash

rom.o: rom.inc rom.c rom.h
	$(CC) $(CFLAGS) -o $@ -c rom.c

%.images: %.bin %.hex %.srec %.list
	@#echo "*** $* images generated ***"

%.bin: %.elf
	$(VERBOSE)$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.hex: %.elf
	$(VERBOSE)$(OBJCOPY) -Oihex $(*).elf $(*).hex

%.srec: %.elf
	$(VERBOSE)$(OBJCOPY) -Osrec $(*).elf $(*).srec

%.list: %.elf
	$(VERBOSE)$(OBJDUMP) -S $(*).elf > $(*).list

%.elf: $(OBJS) $(LDSCRIPT) menu.ld
	$(LD) -o $(*).elf $(OBJS) $(LDFLAGS)
	$(PREFIX)-size $(*).elf

%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

menu.bin:
	make -C ../menu/ clean docker-build all
	cp ../menu/menu.bin .

menu.ld: menu.bin
	cat $< | hexdump -v -e '"BYTE(0x" 1/1 "%02X" ")\n"' > $@

clean:
	rm -f menu.ld
	rm -f $(OBJS)
	rm -f *.elf
	rm -f *.bin
	rm -f *.hex
	rm -f *.srec
	rm -f *.list
	rm -f *.map
	rm -rf *.d

%.flash: %.bin
	dfu-util -a 0 -d 0483:df11 -s 0x08000000 -D $(*).bin

.PHONY: images clean flash dummy
