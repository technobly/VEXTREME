/* stm32f411rct6-kernel-dfu.ld - the KERNEL half of a split build, NO-BOOTLOADER layout. Flash is cut to
 * sectors 0-3 (64 KB; the payload owns 0x08010000+, vxe/vxe_payload_dfu.ld) and RAM ends at 0x2001C000
 * (0x20000400 + 111K), where the payload's 16K window starts. The stack (_stack) therefore tops out just
 * below the payload's RAM. */

/* Define memory regions. */
MEMORY
{
rom (rx) : ORIGIN = 0x08000000, LENGTH = 64K
ram (rwx) : ORIGIN = 0x20000400, LENGTH = 111K
}
/* Include the common ld script. */
INCLUDE cortex-m-generic.ld

SECTIONS {
    .text : {

        /* ... */

        menu_start = .;
        INCLUDE "menu.ld" ;
        menu_end = .;

       /* ... */
    }
}

