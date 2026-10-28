/*
 * vxe_payload_dfu.ld - linker script for a VXE PAYLOAD image (the NO-BOOTLOADER layout: sectors 4-5).
 * The payload is the game + the VXT
 * toolkit + its own libc/libm, linked to run from flash sectors 4-5 with its
 * variables in a fixed RAM window at the top of RAM. Must agree with
 * vxe/vxe_api.h (VXE_SLOT_ADDR/SIZE, VXE_PAYLOAD_RAM/_SZ) and with
 * stm32f411rct6-kernel.ld, which stops the kernel's RAM where this starts.
 *
 * There is no vector table and no reset handler: the kernel finds the
 * VxePayloadHeader at the very start of the slot and calls its `start`
 * (vxe_payload_start(), vxe/vxe_payload.c), which copies .data, zeroes .bss
 * and runs any init_array before vxe_payload_init().
 */
MEMORY
{
    prom (rx)  : ORIGIN = 0x08010000, LENGTH = 192K
    pram (rwx) : ORIGIN = 0x2001C000, LENGTH = 16K
}

ENTRY(vxe_payload_start)

SECTIONS
{
    .vxe_header : {
        KEEP(*(.vxe_header))            /* VxePayloadHeader - MUST be first */
    } >prom

    .text : {
        *(.text*)
        . = ALIGN(4);
        *(.rodata*)
        . = ALIGN(4);
    } >prom

    .init_array : {
        . = ALIGN(4);
        _vxe_init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        _vxe_init_array_end = .;
    } >prom

    .ARM.extab : { *(.ARM.extab*) } >prom
    .ARM.exidx : { *(.ARM.exidx*) } >prom

    . = ALIGN(4);

    .data : {
        _vxe_data = .;
        *(.data*)
        . = ALIGN(4);
        _vxe_edata = .;
    } >pram AT >prom
    _vxe_data_load = LOADADDR(.data);

    .bss : {
        _vxe_bss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _vxe_ebss = .;
    } >pram

    _vxe_image_end = LOADADDR(.data) + SIZEOF(.data);

    /DISCARD/ : { *(.eh_frame) *(.preinit_array*) *(.fini_array*) }
}
