
/* Define memory regions. */
MEMORY
{
rom (rx) : ORIGIN = 0x08000000 + 32K, LENGTH = 256K - 32K
ram (rwx) : ORIGIN = 0x20000400, LENGTH = 127K
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

