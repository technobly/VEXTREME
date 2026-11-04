# vxe_rules.mk - included at the END of the Makefile when VXE=1: the payload side (toolkit + a game), the game registry
# and the vxe-* targets. See docs/vxe/SDK_GUIDE.md.

# ---- the game registry: each game is ONE file, games/<name>.mk (GAME_TITLE, GAME_OBJS, GAME_CFLAGS, GAME_6809_DIR,
# GAME_6809_ASM, optionally GAME_ENABLE_CAL := 1), so adding a game never edits a Makefile. Use `make vxe-<name>`.
VXT_GAME ?= hello
VXE_GAMES := $(patsubst games/%.mk,%,$(wildcard games/*.mk))
ifeq ($(wildcard games/$(VXT_GAME).mk),)
$(error VXT_GAME='$(VXT_GAME)' has no games/$(VXT_GAME).mk - registered games: $(VXE_GAMES))
endif
include games/$(VXT_GAME).mk
# a trailing comment on a `:=` line leaves its trailing spaces in the value
GAME_TITLE    := $(strip $(GAME_TITLE))
GAME_6809_DIR := $(strip $(GAME_6809_DIR))
GAME_6809_ASM := $(strip $(GAME_6809_ASM))
# Which flash layouts the game's payload is built for and packed into its .vxe: both by default, so one file runs on
# either kernel. A game too big for the 128 KB uf2 slot says `GAME_LAYOUTS := dfu`, and a uf2 kernel then refuses it
# with a message instead of failing to install it.
GAME_LAYOUTS  := $(strip $(if $(strip $(GAME_LAYOUTS)),$(GAME_LAYOUTS),uf2 dfu))
$(foreach l,$(GAME_LAYOUTS),$(if $(filter $(l),uf2 dfu),,$(error games/$(VXT_GAME).mk: GAME_LAYOUTS has '$(l)', use uf2 and/or dfu)))
ifeq ($(strip $(GAME_OBJS)),)
$(error games/$(VXT_GAME).mk must set GAME_OBJS)
endif
# A game can ask for the calibration rig (vxt/vxt_cal.c) with GAME_ENABLE_CAL := 1 (about 11KB flash + 13.5KB RAM).
ifeq ($(strip $(GAME_ENABLE_CAL)),1)
VXT_ENABLE_CAL = 1
CAL_OBJS = vxt/vxt_cal.o
else
VXT_ENABLE_CAL = 0
CAL_OBJS =
endif

# ---- the payload: the VXT toolkit + gamelib + the game. Every object is compiled with vxe/vxe_redirect.h force-included,
# which routes its kernel calls through the VxeKernelApi table; the shim holds the table stand-ins (no redirect).
PAYLOAD_OBJS = vxe/vxe_payload.o vxt/vxt_rpc.o vxt/vxt_frame.o vxt/vxt_sound.o vxt/vxt_music.o \
		vxt/vxt_smart.o $(CAL_OBJS) vxt/vxt_smart_text.o vxt/vxt_font_western.o vxt/vxt_cal_load.o \
		vxt/vxt_bounds.o gamelib/gamelib_beam.o gamelib/gamelib_proj3d.o gamelib/gamelib_clip.o \
		gamelib/gamelib_cull.o $(GAME_OBJS)
SHIM_OBJS = vxe/vxe_shim.o
PAYLOAD_CFLAGS = -O2 -DVXT_ENABLE_CAL=$(VXT_ENABLE_CAL) -DVXT_ENABLE_GAME=1 $(GAME_CFLAGS)
$(PAYLOAD_OBJS): CFLAGS += $(PAYLOAD_CFLAGS) -include vxe/vxe_redirect.h
$(SHIM_OBJS): CFLAGS += $(PAYLOAD_CFLAGS)

PAYLOAD_LDSCRIPT = vxe/vxe_payload_$(VXE_LAYOUT).ld
LDFLAGS_PAYLOAD = -nostartfiles -T$(PAYLOAD_LDSCRIPT) -Wl,--gc-sections -Xlinker -Map=payload_$(VXT_GAME).map \
		$(ARCH_FLAGS) -lm -Wl,--start-group -lc -lgcc -Wl,--end-group

# The payload links on its own (no kernel objects, libc/libm only), so a kernel symbol the table does not carry fails
# the build here, not at run time.
payload_$(VXT_GAME).elf: $(PAYLOAD_OBJS) $(SHIM_OBJS) $(PAYLOAD_LDSCRIPT)
	$(LD) -o $@ $(PAYLOAD_OBJS) $(SHIM_OBJS) $(LDFLAGS_PAYLOAD)
	$(PREFIX)-size $@

docker-payload:
	${STM_DOCKER} make payload_$(VXT_GAME).bin $(HW_VER_ARGS)

# ---- `make vxe-<name> USE_HW=v0.3` (or `make vxe GAME=<name>`, or `make vxe-all`): the whole pipeline for one game -
# the kernel for $(VXE_LAYOUT), the payload for each layout in GAME_LAYOUTS, the 6809 stub and the .vxe - into
# $(VXE_OUT)/ together with the kernel (stm32_kernel.bin and, for the uf2 layout, VEXTREME.uf2). The .vxe carries one
# STM32 payload per layout, so the same file runs on a uf2 or a dfu kernel. Each target cleans first, so `vxe-all`
# builds them one after another (never use -j). Payload objects are position-dependent, so they are rebuilt for each
# layout (the objects do not track CFLAGS).
VXE_OUT ?= ../../out/vxe-$(VXE_LAYOUT)
ASM6809 = docker run --rm -v "$(CURDIR)/../app6809":/build -w /build -u `id -u`:`id -g` asm6809 asm6809 -B
vxe-%: games/%.mk
	$(MAKE) clean
	$(MAKE) menu.ld
	$(MAKE) docker USE_HW=$(USE_HW) VXT_GAME=$*
	$(MAKE) vxe-payloads VXT_GAME=$* USE_HW=$(USE_HW)
	$(MAKE) vxe-pack VXT_GAME=$* USE_HW=$(USE_HW)
# (internal) build payload_<game>_<layout>.bin for every layout the game supports
vxe-payloads:
	@for l in $(GAME_LAYOUTS); do \
		rm -f $(PAYLOAD_OBJS) $(SHIM_OBJS) payload_$(VXT_GAME).elf payload_$(VXT_GAME).bin || exit 1; \
		$(MAKE) docker-payload USE_HW=$(USE_HW) VXT_GAME=$(VXT_GAME) VXE_LAYOUT=$$l || exit 1; \
		mv payload_$(VXT_GAME).bin payload_$(VXT_GAME)_$$l.bin || exit 1; \
	done
vxe:
	@test -n "$(GAME)" || { echo "usage: make vxe GAME=<name> USE_HW=v0.3   (registered: $(VXE_GAMES))"; exit 1; }
	$(MAKE) vxe-$(GAME) USE_HW=$(USE_HW)
vxe-all:
	@for g in $(VXE_GAMES); do $(MAKE) vxe-$$g USE_HW=$(USE_HW) || exit 1; done
	@ls -l $(VXE_OUT)
# (internal) assemble the 6809 stub and pack, using the games/$(VXT_GAME).mk variables
vxe-pack:
	@for l in $(GAME_LAYOUTS); do test -f payload_$(VXT_GAME)_$$l.bin || { echo "vxe-pack: run make vxe-$(VXT_GAME) first"; exit 1; }; done
	mkdir -p $(VXE_OUT)
	$(ASM6809) -o $(GAME_6809_DIR)/$(GAME_6809_ASM).bin $(GAME_6809_DIR)/$(GAME_6809_ASM).asm
	python3 -I ../../tools/make_vxe.py pack $(VXE_OUT)/$(GAME_TITLE).vxe \
		--m6809 ../app6809/$(GAME_6809_DIR)/$(GAME_6809_ASM).bin --name $(GAME_TITLE) \
		$(foreach l,$(GAME_LAYOUTS),--stm-$(l) payload_$(VXT_GAME)_$(l).bin)
	cp $(BINARY).bin $(VXE_OUT)/stm32_kernel.bin
ifeq ($(VXE_LAYOUT),uf2)
	${STM_DOCKER} uf2conv.py -b 0x08004000 -o $(BINARY).uf2 -c $(BINARY).bin
	cp $(BINARY).uf2 $(VXE_OUT)/VEXTREME.uf2
endif

# Also remove every payload object and dependency file, not just the kernel's (upstream's clean only knows $(OBJS)).
vxe-clean:
	rm -f vxe/*.o vxe/*.d vxt/*.o vxt/*.d gamelib/*.o gamelib/*.d game/*.o game/*.d payload_*.elf payload_*.bin payload_*.map
clean: vxe-clean

.PHONY: vxe vxe-all vxe-pack vxe-payloads vxe-clean docker-payload
