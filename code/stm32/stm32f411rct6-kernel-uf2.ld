/* stm32f411rct6-kernel-uf2.ld - the KERNEL half of a split build, UF2 layout. The UF2 bootloader owns
 * sector 0 (0x08000000-0x08003FFF), so the kernel is linked to run from 0x08004000 (the bootloader jumps
 * there) and may use everything below the payload slot at 0x08020000 (vxe/vxe_payload_uf2.ld): 112 KB,
 * i.e. sectors 1-3 plus all of sector 4. RAM ends at 0x2001C000 (0x20000400 + 111K), where the payload's
 * 16K window starts; the stack (_stack) therefore tops out just below the payload's RAM. */

/* Define memory regions. */
MEMORY
{
rom (rx) : ORIGIN = 0x08004000, LENGTH = 112K
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

