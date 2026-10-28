# vxe_config.mk - included by the Makefile when VXE=1, right after OBJS is defined. It only changes what the KERNEL
# (the firmware itself) is built from; the payload side is in vxe_rules.mk. With VXE=0 neither file is read and the
# Makefile is upstream's, unchanged.
#
#   VXE_LAYOUT  uf2 (default): the UF2 bootloader owns sector 0; the kernel runs from 0x08004000 (up to 112 KB) and is
#                              built with upstream's own flags (-O3); the payload slot is sector 5, 128 KB.
#               dfu          : no bootloader (flashed with dfu-util); the kernel owns sectors 0-3 (64 KB) and is built
#                              -Os; the payload slot is sectors 4-5, 192 KB.
#   KERNEL_OPT  extra optimisation flag for the kernel objects only (default follows the layout; set it in
#               config/profile.mk). The payload is always built -O2.

VXE_LAYOUT ?= uf2
ifeq ($(filter $(VXE_LAYOUT),uf2 dfu),)
$(error VXE_LAYOUT must be uf2 or dfu, not '$(VXE_LAYOUT)')
endif
ifeq ($(VXE_LAYOUT),uf2)
VXE_LAYOUT_UF2 = 1
KERNEL_OPT ?=
else
VXE_LAYOUT_UF2 = 0
KERNEL_OPT ?= -Os
endif

# The kernel = the regular firmware's objects plus the payload loader.
OBJS += vxe/vxe_kernel.o vxe/vxe_install.o
LDSCRIPT := stm32f411rct6-kernel-$(VXE_LAYOUT).ld
CFLAGS += -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=$(VXE_LAYOUT_UF2) -DVXE_SPLIT=1
$(OBJS): CFLAGS += $(KERNEL_OPT)

# The container's make reads these from its environment, so the same choices apply inside docker.
DOCKER_ARGS += -e VXE_LAYOUT=$(VXE_LAYOUT) -e VXT_GAME=$(VXT_GAME)

# A bare `make` would otherwise run upstream's first rule, flash-release.
.DEFAULT_GOAL := all
