BINARY=veccart
OBJS=main.o menu.o romemu.o msc.o xprintf.o usb_msc.o ff_diskio.o \
		fatfs/ff.o fatfs/option/unicode.o flash.o led.o delay.o \
		highscore.o

LIBNAME = opencm3_stm32f4
ARCH_FLAGS = -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -mcpu=cortex-m4 -DSTM32F4

PREFIX	= arm-none-eabi
CC		= $(PREFIX)-gcc
LD		= $(PREFIX)-gcc
OBJCOPY		= $(PREFIX)-objcopy
OBJDUMP		= $(PREFIX)-objdump
# Uncomment this line if you want to use the installed (not local) library.
#TOOLCHAIN_DIR = `dirname \`which $(CC)\``/../$(PREFIX)
TOOLCHAIN_DIR=../libopencm3
CFLAGS		+= -O3 -g -Wextra -Wshadow -Wimplicit-function-declaration -Wredundant-decls \
		-Wmissing-prototypes -Wstrict-prototypes -fno-common -ffunction-sections -fdata-sections \
		-MD -Wall -Wundef -I$(TOOLCHAIN_DIR)/include $(ARCH_FLAGS)

LDSCRIPT	= stm32f411rct6.ld
#../libopencm3/lib/lib$(LIBNAME).ld

LDFLAGS		+= --static -nostartfiles -L$(TOOLCHAIN_DIR)/lib  \
		   -T$(LDSCRIPT) -Wl,--gc-sections -mthumb $(CFLAGS) -lm \
			-Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group -l$(LIBNAME)

#include libopencm3/ld/Makefile.linker

.SUFFIXES: .elf .bin .hex .srec .list .images
.SECONDEXPANSION:
.SECONDARY:

# Unzipped release files to code/releases/
# Specify release version like so
# make flash-bin VER=v0.1
flash-release:
	dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D ../releases/$(VER)/stm32/veccart.bin

# Specify bin like so
# make flash-bin BIN=../releases/v0.1/stm32/veccart.bin
flash-bin:
	dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D $(BIN)

all: docker

docker-build: Dockerfile
	docker build . -t stm32-build

docker:
	docker run --rm -t -v `pwd`:/build/veccart -u `id -u ${USER}`:`id -g ${USER}` stm32-build make images

images: $(BINARY).images
flash: $(BINARY).flash

rom.o: rom.inc rom.c rom.h
	$(CC) $(CFLAGS) -o $@ -c rom.c

%.images: %.bin %.hex %.srec %.list
	@#echo "*** $* images generated ***"

%.bin: %.elf
	$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.hex: %.elf
	$(OBJCOPY) -Oihex $(*).elf $(*).hex

%.srec: %.elf
	$(OBJCOPY) -Osrec $(*).elf $(*).srec

%.list: %.elf
	$(OBJDUMP) -S $(*).elf > $(*).list

%.elf: $(OBJS) $(LDSCRIPT)
	$(LD) -o $(*).elf $(OBJS) $(LDFLAGS)
	$(PREFIX)-size $(*).elf

%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS)
	rm -f *.elf
	rm -f *.bin
	rm -f *.hex
	rm -f *.srec
	rm -f *.list
	find . -name '*.d' -delete

#%.flash: %.hex
#	stm32flash -w "$(*).hex" -b 460800 -i -rts,dtr,-dtr -v -g 0x0 /dev/ttyUSB0

%.flash: %.bin
	dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D $(*).bin

.PHONY: images clean flash dummy
