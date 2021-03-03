.code 16
.syntax unified

.global menuData
.global romData
.global parmRam
.global doHandleEvent
.global doDbgHook
.global readcnt
.global sysData // usb_dev is offset of 9 bytes
.global checkDevMode

.section .data

.global romemu
// Main rom emulation code

#if HW_VER == 255
    #error "USE_HW hardware version not specified, please specify e.g. USE_HW=v0.2 or USE_HW=v0.3"
#endif

romemu:
initonce:
    ldr   r8,=0                 // Cycle count since last event
initloop:
    // Initialization
    ldr   r1,=0x40020800        // GPIOC base (idr offset = 0x10 = input)
    ldr   r0,=romData           // Load ROM data array addr in scratch r0
    ldr   r2,[r0]               // romData pointer deferenced,
                                // do not clobber r2 unless returning through initloop
    ldr   r3,=0x40020000        // GPIOA base (bsrr offset = 0x18 = output)
    ldr   r4,=0x40020400        // GPIOB base (idr offset = 0x10 = input)
                                // r5 is another scratch register for data
                                // r6 is another scratch register for data manipulation
    ldr   r7,=0xfffe            // reset vector constant
    ldr   r9,=0x40011004        // USART1_DR for quick addr pointer to Serial Debugging TX
                                // USART1_SR = 0x40011000

wloop:
    // Check for 6809 reset vector (0xFFFE)
    // FIXME: Currently on v0.2, we are not reading A15 for MSB, but rather PB6.
    //        This works though, because PB6 stays HIGH, but it might break down
    //        with games that toggle PB6.  Not sure what PB6 will do if LOW, when
    //        the Vectrex is reset.  Change this for v0.3 hardware so A15 is on PC15,
    //        unless something like Bad Apple or Voom uses PB6, although in testing
    //        it doesn't seem like it.  Bad Apple can be reset now with this method.
    ldr   r0,[r1, 0x10]         // Load addr pins
    cmp   r0,r7                 // Compare to 6809 reset vector
    bne   clearcount            // Matched? clearcount if not

#if (HW_VER < 3)
// ===========================================================
    add   r8,#1                 // Increase reset vector count
    cmp   r8,#0x700000          // 8 cycles * 0x170000 is about 100ms
                                //   0x700000 is is more like 700ms (quick tap resets game, long tap back to menu)
                                //   Found that no-buzz Vectrex has a min 300ms reset pulse, so 700ms is about right.
    bne   put_menu_back         // Keep the increased count, but exit without returning to menu
    b     ledscyan              // Make all LEDs CYAN to help indicate when the cart swaps, then jump to initloop

put_menu_back:
    cmp   r8,#0x710000
    blo   resetexit
    bne   waitreset

    // Reset vector detected for 700ms, exchange the rom and menu data
    ldr   r0,=romData
    ldr   r2,=menuData
    str   r2,[r0]
    mov   r0,# 'g'              // Restore copyright bytes just in case we got reset in Dev Mode
    strb  r0,[r2, #0]           // |
    mov   r0,# ' '              // |
    strb  r0,[r2, #1]           // |

#elif (HW_VER >= 3)
// -----------------------------------------------------------
    ldr   r0,[r4, 0x10]         // Load addr pins (for /CE)
    lsls  r0,#(31-15)           // Shift bit 15 into sign flag
    bpl   clearcount            // Bit 15 clear? clearcount if so
    lsls  r0,#5                 // Shift bit 10 into sign flag (for /OE which is /E)
    bmi   resetexit             // Bit 10 high? skip this one (no countinc or clearcount)

countinc:
    add   r8,#1                 // Increase reset vector count

    // DEBUG code to turn RED LED on
    //mov   r5,#0x00000001        // Just set LED(GPIO0) in BSRRL(set) and ensure BSRRH(reset) is clear
    //str   r5,[r4, 0x18]         // Output to GPIOB_BSRR

check_put_menu_back:
    cmp   r8,#0x4000
    bne   check_put_cart_back

    ldr   r0,=checkDevMode      // Have we checked Dev Mode yet? We care about this on first boot.
    ldrb  r5,[r0]               // |
    cmp   r5,#2                 //  No, fall through to check if we are in Dev Mode
    beq   put_menu_back         //  Yes, swap the menu
    add   r5,#1                 // Increase checkDevMode count (allow two resets on cold boot:
    str   r5,[r0]               //  1 initial turn on, and 2 user pressing reset to get to the cart.bin faster)

    ldr   r0,=sysData           // Is Dev Mode active at first power up?
    ldr   r0,[r0]               // | dereference sysData pointer
    ldrb  r5,[r0, #9]           // | usb_dev is 1 byte at offset 9
    cmp   r5,#0                 // | DISABLED = 0
    bne   skip_put_menu_back    //  Yes, skip menu swap

put_menu_back:
    ldr   r0,=romData           // Reset vector detected for more than 300ms, set romData to menuData
    ldr   r2,=menuData          // |
    str   r2,[r0]               // |
    mov   r0,# 'g'              // Restore copyright bytes just in case we got reset in Dev Mode
    strb  r0,[r2, #0]           // |
    mov   r0,# ' '              // |
    strb  r0,[r2, #1]           // |

skip_put_menu_back:

check_put_cart_back:
    cmp   r8,#0x200000          // This is set to about 700ms
                                //  quick tap < 700ms resets to the Menu, long tap >= 700ms resets to the Game
                                //  Most Vectrex should have 250-300ms minimum reset delay, so 700ms is about right.
    bne   put_cart_back         // Keep the increased count, but exit without returning to menu
    b     ledscyan              // Make all LEDs CYAN to help indicate when the cart swaps, then jump to initloop

put_cart_back:
    cmp   r8,#0x210000
    blo   resetexit
    bne   waitreset

    // DEBUG code to turn RED LED off
    //mov   r5,#0x00010000        // Just clear LED(GPIO0) in BSRRH(clear) and ensure BSRRL(set) is clear
    //str   r5,[r4, 0x18]         // Output to GPIOB_BSRR

    // Reset vector detected for more than 700ms, set romData to cartData
    ldr   r0,=romData
    ldr   r2,=cartData
    ldr   r2,[r2]               // dereference cartData pointer
    str   r2,[r0]

#endif // HW_VER == 3
// ===========================================================

waitreset:
    ldr   r0,[r1, 0x10]         // Load addr pins
    cmp   r0,r7                 // Compare to 6809 reset vector
    beq   waitreset             // Wait for reset vector to clear
    and   r8,#0                 // clear count to reset counter
    b     restoreled            // set the LEDs back to rainbow and jump back to initloop

clearcount:
    and   r8,#0
resetexit:

    // Wait for /CE (A15) to become active
    ldr   r0,[r4, 0x10]
    lsls  r0,#(31-15)           // Shift bit 15 into sign flag
    bmi   wloop                 // Bit 15 set? Loop if so

    // Check for a write
    ldr   r0,[r4, 0x10]
    lsls  r0,#(31-1)            // Shift bit 1 into sign flag
    bpl   write                 // Bit 1 clear? Is write!

    // Handle read: load address, get byte, send to Vectrex
    ldr   r0,[r1, 0x10]         // Load addr pins
    eor   r0, r0, #0x8000       // Invert software-controlled line: it is high per default

    // IF WE CLOBBER R2, RELOAD IT
    //ldr   r6,=romData           // Load ROM data array addr in scratch r0
    //ldr   r2,[r6]               // romData pointer deferenced in r2
    ldrb  r5,[r0, r2]           // Load corresponding byte in rom
    orr.w r5,r5,#0xff0000       // Set shit (this clears 0-7, but then the set register overrides the cleared bits)
    str   r5,[r3, 0x18]         // Output

    b     wloop

write:
    // The Vec writes to the cart. Seemingly, we need to do something
    ldr   r6,[r3, 0]            // Load input/output defs of portA
    mov   r7,r6                 // Save old values
    lsr   r6,#16                // Nuke bottom 16 bits...
    lsl   r6,#16                // ...by shifting them out and zeroes back.
    str   r6,[r3, 0]            // Set bottom 8 pins of gpioa to input
    ldr   r0,[r1, 0x10]         // Load addr word from gpioc
    ubfx  r0,r0,#0,#15          // and with 0x7fff

#if (HW_VER < 3)
// ===========================================================
// We need to delay a while because the written data is only stable in the 2nd half of the cycle.
// FIXME: This will become a problem with more code added below.
    mov   r6,#10                // <- Oscillator frequency dependent magic number here!!
waitdataloop:                   // Wait a bit
    subs  r6,#1                 // |
    bne   waitdataloop          // |

#elif (HW_VER >= 3)
// -----------------------------------------------------------
// Wait until the /E goes low (data valid), REQUIRES v0.3 HW
waitdataloop:
    ldr   r6,[r4, 0x10]         // Load addr pins (for /E)
    lsls  r6,#(31-10)           // Shift bit 10 into sign flag (for /OE which is /E)
    bmi   waitdataloop          // Bit 10 high? wait for low if so

#endif // HW_VER == 3
// ===========================================================

    ldr   r5,[r3, 0x10]         // Load data
    and   r5,#0xff
    str   r7,[r3, 0]            // Restore bottom half of gpioa to output

    // From here on:
    // r0 is addr
    // r5 is data
    // Writes to 0x7f00-0x7ffe are to set parameter values for a future event (rpc function ID)
    // Writes to 0x7fff are the rpc function ID's and kick off the function
    // Writes to 0x0000 will have the data byte immediately forwarded to the serial TX pin for debugging
    // ------------------------------------------------------------------------
    // Is it a write to addr #$0000 = serial port addr
    // TODO: avoid addr 0 because Polar Rescue writes there occassionally
    //       avoid addr 1 because the Bad Apple player writes there a lot!
    // cmp   r0,#0
    // beq   outserial

    // See if it is a write to 7Fxx (this is extra insurance against odd Polar Rescue writes)
    lsr   r9,r0,#8
    cmp   r9,#0x7f
    bne   writecart             // If not, must be a general cart RAM write

    // See if it is a write to xxFF
    and   r0,#0xff
    cmp   r0,#0xff
    beq   handleevent

    // Nope, it is probably a parameter (0x7ff0 ~ 0x7ffe)
    ldr   r7,=parmRam
    strb  r5,[r7, r0]

    writecart:
    lsr   r9,r0,#8              // See if it is a write to 2000 - 27ff
    cmp   r9,#0x20              // | TODO: Make this range two variables
    blo   wrdone                // below the range we want, bail
    cmp   r9,#0x27              // |
    bhi   wrdone                // above the range we want, bail

    ldr   r9,=cartData          // Write to cartData[r0] = r5
    ldr   r9,[r9]               // |
    strb  r5,[r9, r0]           // |
    // fall through to wrdone

#if (HW_VER < 3)
// ===========================================================
// Wait until the /wr line goes high again
wrdone:
    ldr   r0,[r4, 0x10]         // Load inputs
    lsls  r0,#(31-1)            // Shift bit 1 into sign flag
    bmi   initloop              // bit 1 set? Return if so (was jumping back to wloop, but seemed
                                //  to not return to menu unless registers re-init with initloop jump
                                //  when playing Polar Rescue, after sub launches and Write to cart occur)
    b     wrdone                // Wr still is low

#elif (HW_VER >= 3)
// -----------------------------------------------------------
// Wait until the /E goes high again, REQUIRES v0.3 HW
wrdone:
    ldr   r6,[r4, 0x10]         // Load addr pins (for /E)
    lsls  r6,#(31-10)           // Shift bit 10 into sign flag (for /OE which is /E)
    bpl   wrdone                // Bit 10 low? wait for high if so
    b     initloop              // otherwise all done here

#endif // HW_VER == 3
// ===========================================================

// This sub-routine is after wrdone, and lets the parameter write fall through to wrdone
// to save a few instructions on average and force more cycles to be wasted only when using serial
outserial:
    // Output byte to serial port (TX pin on debugging header)
    ldr   r0,=0x40011004
    str   r5,[r0]
    b     wrdone

dbg:
    mov   r1,r5
    ldr   r2,=doDbgHook
    blx   r2
    b     initloop

ledscyan:
    ldr   r1,=ledsCyan
    blx   r1
    b     initloop

ledsmagenta:
    ldr   r1,=ledsMagenta
    blx   r1
    b     initloop

restoreled:
    mov   r1,#1
    ldr   r2,=applyLedSettings
    blx   r2
    b     initloop

ledsoff:
    ldr   r1,=ledsOff
    blx   r1
    b     initloop

logR5:
    mov   r0,r5
    ldr   r1,=doLog
    blx   r1
    b     initloop

dumpmem:
    ldr   r1,=dumpMemory
    blx   r1
    b     initloop

handleevent:
    mov   r0,r5
    ldr   r1,=doHandleEvent
    blx   r1
    b     initloop
