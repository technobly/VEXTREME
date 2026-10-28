;*******************************************************************************
; Cal.asm - 6809 side of the VXT CALIBRATION RIG.
;
; A STANDALONE TOOLKIT APP, deliberately not a mode inside any one game:
; calibration serves every game built on this toolkit, so the measurement
; rig is its own cart.
;
; This file follows the same structure as any toolkit game cart -
; address-publish handshake, Wait_Recal/read-input/RPC/vxt_smart frame
; loop, analog-joystick setup. The STM32 side holds the rig's logic
; (vxt/vxt_cal.c), and this file should stay a plain executor that does not need editing.
;
; CONVENTIONS THIS FILE FOLLOWS (both came from
; a board freeze; do not "tidy" either away):
;   1. Explicit `jmp main` immediately after the cartridge header. Do not
;      rely on fall-through.
;   2. The RAM-resident RPC stub is included last in the file, past every
;      branch target - mid-STM32-computation the normal ROM bytes are not
;      served, so 6809 code still fetching from "ROM" can crash.
;
; Uses Joy_Analog (not Joy_Digital) because the rig's measuring caret needs
; proportional stick response - Joy_Digital only ever reports -1/0/1,
; which cannot position a caret finely enough to read
; an error off in physical units.
;*******************************************************************************
        include "vectrex.i"

        ORG     0
        fcb     "g GCE 2026", $80
        fdb     music1
        fcb     $F8, $50, $20, -$10
        fcb     "VXT CAL", $80
        fcb     0

        jmp     main                    ; convention 1 - explicit, do not rely on fall-through
                                        ; fall-through

        setdp   #$d0

        include "vxt/vxt_rpc_macros.asm"
        include "vxt/vxt_input.asm"
; vxt_smart.asm per-game switches - every app defines all three. The rig measures the
; engine as it was first tested, so all three are off.
VXT_BLANK_BEFORE_ZERO   equ     0
VXT_BLANK_AT_EXPIRY     equ     0
VXT_MOVEHUGE            equ     0
        include "vxt/vxt_smart.asm"

VXT_RPC_ID_SMART_ADDRS EQU 69           ; engine-level handshake, same ID
                                        ; every vxt_smart-using app publishes
                                        ; its routine addresses against
VXT_RPC_ID_CAL_FRAME   EQU 75           ; this rig's own ID, inside the
                                        ; 64-79 toolkit block
VXT_RPC_ID_CAL_INIT    EQU 76           ; one-shot, fired once per 6809 boot
                                        ; (cold or warm alike) - resets the
                                        ; STM32-side rig state, which a 6809
                                        ; warm reset alone does not do (it does
                                        ; not reset the STM32)

;-------------------------------------------------------------------------------
main
        VXT_RPC_INIT
        VXT_INPUT_INIT

        ; Maximum joystick A/D resolution. VXT_INPUT_INIT defaults
        ; Vec_Joy_Resltn to $80 (fastest read, coarsest value) - fine for
        ; Joy_Digital, too coarse for a measuring caret. Any app that reads
        ; the analog stick (test4_input, test5_sound) must set it
        ; explicitly - omitting it degrades to coarse readings.
        clr     Vec_Joy_Resltn          ; POTRES = $00 : maximum resolution

        ; --- one-time address publish - identical to every vxt_smart app ---
        ; The rig exercises every draw path (that is what the scale-band
        ; probe is for), so it must publish the full table -
        ; including startDrawBig, startDrawHuge, setScaleHi and
        ; startDrawHuge16. An unpublished routine is a silent no-op on the
        ; STM32 side, which here would read as "that draw path measured
        ; perfectly" - the single most misleading failure this rig could have.
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
        ldd     #SM_startDrawBig_d      ; the scale-64 / 27-nop pairing that
        std     $7f10                   ; is UNVERIFIED on hardware - the rig's
                                        ; scale-band screen exists to measure
                                        ; exactly this (roadmap item 10a)
        ldd     #SM_startDrawHuge_d     ; genuine Timer-1 poll, correct at any
        std     $7f12                   ; scale - the known-good reference the
                                        ; scale-band screen compares against
        ldd     #SM_setScaleHi
        std     $7f14
        ldd     #SM_startDrawHuge16_d
        std     $7f16
        ; startDraw32 must be published even though this cart may never
        ; choose it: the STM32 handler reads all 13 slots, and the
        ; dispatcher - not this cart - selects Draw32 for short lines.
        ; Left unpublished, $7F18 holds whatever the PREVIOUS cart wrote,
        ; which passes vxtSmartHasDraw32()'s range check because every
        ; cart links the same engine, and short lines then jump into
        ; unrelated code - an invisible measuring caret, depending only
        ; on which cart ran before this one.
        ldd     #SM_startDraw32_d
        std     $7f18
        VXT_RPC #VXT_RPC_ID_SMART_ADDRS   ; blocks until the STM32 has stored them

        VXT_RPC #VXT_RPC_ID_CAL_INIT      ; one-shot: resets rig state on the
                                        ; STM32 - see the EQU comment above

frame_loop
        jsr     Wait_Recal

        VXT_INPUT_READ  Joy_Analog      ; proportional - the caret needs it

        VXT_RPC #VXT_RPC_ID_CAL_FRAME    ; STM32 runs vxt_cal_handler():
                                        ; advances screen/caret state and
                                        ; composes the whole frame's
                                        ; SmartList into $0800

        ldu     #$0800
        jsr     vxt_smart

        bra     frame_loop

        include "vxt/vxt_rpc_stub.asm"  ; convention 2 - PHYSICALLY LAST,
                                        ; past every branch target above
