;*******************************************************************************
; VxeMsg.asm - the message cartridge the VXE kernel serves when it refuses a
; .vxe file (wrong flash layout, damaged file, kernel too old or too new, install
; failure), so the user sees WHY on the Vectrex screen instead of the menu just
; coming back. The kernel copies this image into the cart buffer and writes the
; text into the eight fixed 20-byte slots at $0100 (chars, then $80); nothing else
; in the image changes. Assembled by tools/gen_vxe_msgcart.sh into
; code/stm32/vxe/vxe_msgcart.h, which is what the kernel actually includes.
;
; No STM32 cooperation: it only prints. Press RESET on the Vectrex to return to the menu.
;*******************************************************************************
        include "vectrex.i"

        ORG     0
        fcb     "g GCE 2026", $80        ; BIOS copyright string
        fdb     silent_music
        fcb     $F8, $50, $20, -$4E      ; height, width, rel y, rel x
        fcb     "VXE MESSAGE", $80, 0    ; title (also the high-score key; this cart has no scores)
        jmp     main                     ; explicit, do not rely on fall-through

silent_music
        fdb     $FEE8
        fdb     $FEB6
        fcb     $00
        fcb     $80

        setdp   #$d0

; The eight text slots: fixed address, 20 bytes each (19 characters + $80).
        ORG     $0100
msg_lines
        rmb     160

        ORG     $0200
main
frame   jsr     Wait_Recal
        lda     #$5F
        jsr     Intensity_a
        ldu     #msg_lines+0*20
        lda     #80
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+1*20
        lda     #50
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+2*20
        lda     #30
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+3*20
        lda     #10
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+4*20
        lda     #-20
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+5*20
        lda     #-40
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+6*20
        lda     #-70
        ldb     #-90
        jsr     Print_Str_d
        ldu     #msg_lines+7*20
        lda     #-90
        ldb     #-90
        jsr     Print_Str_d
        bra     frame
