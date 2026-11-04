;*******************************************************************************
; Hello.asm - 6809 side of HELLO, the SDK template game for the VXE loader.
; See docs/vxe/SDK_GUIDE.md. The smallest working stub.
;
; Every frame the 6809 reads the controllers, asks the STM32 for the next frame (RPC)
; and draws the SmartList it composed. The game logic is in code/stm32/game/hello.c.
;
; To make your own game, change the title string, the two RPC IDs (inside
; 64-79, matching your <game>.h), and - if you want it - the high-score staging
; block (omitted here: HELLO keeps no high score).
;
; Two rules from bugs that froze the board (docs/vxe/CONVENTIONS.md, conventions 1 and 2):
; `jmp main` right after the header (do not fall through), and the RAM-resident RPC
; stub included last in the file.
;*******************************************************************************
        include "vectrex.i"

        ORG     0
        fcb     "g GCE 2026", $80        ; Required BIOS copyright string
        fdb     silent_music             ; Silent tune (fast exit)
        fcb     $F8, $50, $20, -$4E      ; height, width, rel y, rel x
        ; THE TITLE IS NOT COSMETIC. VEXTREME's per-game high score is keyed by the
        ; string at offset $11 (loadRomWithHighScore() -> highScoreGet(&romData[0x11])),
        ; and an EMPTY title collides with the default record. Keep it non-empty and
        ; unique among your games.
        fcb     "HELLO VXE", $80, 0

        jmp     main                     ; explicit, do not rely on fall-through

silent_music
        fdb     $FEE8                    ; ADSR pointer (BIOS default, any valid ptr)
        fdb     $FEB6                    ; TWANG pointer (BIOS default, any valid ptr)
        fcb     $00                      ; slice: rest (no notes)
        fcb     $80                      ; end of music

        setdp   #$d0

        include "vxt/vxt_rpc_macros.asm"
        include "vxt/vxt_input.asm"
; vxt_smart.asm per-game switches - every app must define all three (an undefined
; symbol in an asm6809 `if` is treated as false). New games: take all on; see docs/vxe/SDK_GUIDE.md §5.
VXT_BLANK_BEFORE_ZERO   equ     1
VXT_BLANK_AT_EXPIRY     equ     1
VXT_MOVEHUGE            equ     1
        include "vxt/vxt_smart.asm"
        include "vxt/vxt_sound.asm"

VXT_RPC_ID_SMART_ADDRS EQU 69           ; engine-level handshake, the same for every app
VXT_RPC_ID_HELLO_FRAME EQU 64           ; per-frame: advance a tick, compose the SmartList
VXT_RPC_ID_HELLO_INIT  EQU 65           ; once per 6809 boot (cold or warm reset alike)

;-------------------------------------------------------------------------------
main
        VXT_RPC_INIT
        VXT_SOUND_INIT
        VXT_INPUT_INIT

        ; --- one-shot address publish - identical in every vxt_smart app ---
        ldd     #SM_setScale
        std     $7f00
        ldd     #SM_setIntensity
        std     $7f02
        ldd     #SM_recenter
        std     $7f04
        ldd     #SM_startMove_d
        std     $7f06
        ldd     #SM_startDraw_d
        std     $7f08
        ldd     #SM_continue_d
        std     $7f0a
        ldd     #SM_end
        std     $7f0c
        ldd     #SM_startMoveBig_d
        std     $7f0e
        ldd     #SM_startDrawBig_d
        std     $7f10
        ldd     #SM_startDrawHuge_d
        std     $7f12
        ldd     #SM_setScaleHi
        std     $7f14
        ldd     #SM_startDrawHuge16_d
        std     $7f16
        ldd     #SM_startDraw32_d
        std     $7f18
        ldd     #SM_startMoveHuge_d
        std     $7f1a
        VXT_RPC #VXT_RPC_ID_SMART_ADDRS   ; blocks until the STM32 has stored them

        VXT_RPC #VXT_RPC_ID_HELLO_INIT    ; one-shot: the STM32 resets its state and does
                                          ; all its file reads here (not on the frame path)

frame_loop
        jsr     Wait_Recal

        VXT_INPUT_READ  Joy_Digital       ; digital stick + buttons -> parmRam

        VXT_RPC #VXT_RPC_ID_HELLO_FRAME   ; the STM32 composes the frame into $0800

        jsr     vxt_sound                 ; play any AY writes the STM32 queued

        ldu     #$0800
        jsr     vxt_smart                 ; play the SmartList

        bra     frame_loop

        include "vxt/vxt_rpc_stub.asm"    ; RAM-resident RPC stub, physically LAST
