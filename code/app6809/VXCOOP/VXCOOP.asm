;*******************************************************************************
; VXCOOP.asm - VX-COOP: VXT toolkit reference application, 6809 side.
; Copyright (C) 2026 Caelotronics.
;
; This file is the complete 6809 half of a cooperative dual-CPU application,
; and it consists of approximately sixty instructions. The division of labor
; is the reason: the STM32 owns all application state, geometry, arithmetic,
; sound composition and persistence, while the 6809 reports the controller,
; requests a frame, services the PSG, and draws. No code in this file knows
; what is being drawn.
;
; Read alongside docs/VX-COOP/VX-COOP_Reference_Guide.md, which derives every
; mechanism used here, and code/stm32/vxt/vxcoop_handler.c, the STM32 side
; this file drives.
;
; File ordering is significant. Both of the following defences were adopted
; after a power-on failure on hardware:
;
;   1. An explicit `jmp main` immediately after the cartridge header. The
;      Vectrex BIOS convention is to fall through to whatever follows the
;      header's terminating zero byte, so without this jump execution enters
;      whatever the assembler placed next.
;   2. The RAM-resident RPC stub include placed physically last, after every
;      loop and branch target. vxt_rpc_stub.asm contains assembled instruction
;      bytes intended only to be copied into RAM and executed there. When that
;      include was placed immediately after the header, the 6809 executed the
;      stub in place at power-on with undefined values in A and X, stored an
;      undefined RPC identifier to $7FFF, and then executed `jmp ,x` to an
;      undefined address.
;
; Retain both. Sprite_tm's 2015 VOOM source already used the second
; structurally; the first was added independently.
;
; A macros-only include (vxt_rpc_macros.asm) may safely be placed early,
; because it emits no bytes until a macro is invoked. Only the stub body must
; be placed last.
;*******************************************************************************
        include "vectrex.i"

;-------------------------------------------------------------------------------
; Cartridge header.
; The title string at offset $11 is also the key the VEXTREME firmware uses to
; locate this cartridge's saved high score: loadRomWithHighScore() calls
; highScoreGet(&romData[0x11]). It is therefore functional, not decorative, and
; an empty title disables per-cartridge score persistence.
;-------------------------------------------------------------------------------
        ORG     0
        fcb     "g GCE 2026", $80        ; required BIOS copyright string
        fdb     silent_music             ; silent tune: ~1-frame title bypass
        fcb     $F8, $50, $20, -$56      ; height, width, rel y, rel x
        fcb     "VX-COOP", $80, 0        ; title string and high-score key

        jmp     main                     ; defence 1; see the header note

;-------------------------------------------------------------------------------
; Minimal silent tune, so the BIOS exits its title sequence in approximately
; one frame rather than playing the full jingle. One rest slice followed
; immediately by the terminator.
;-------------------------------------------------------------------------------
silent_music
        fdb     $FEE8                    ; ADSR pointer  (any valid BIOS pointer)
        fdb     $FEB6                    ; TWANG pointer (any valid BIOS pointer)
        fcb     $00                      ; slice: rest, no notes
        fcb     $80                      ; end of music

;-------------------------------------------------------------------------------
; `setdp` is an assembler directive, not an instruction, and emits no bytes. It
; instructs asm6809 to assume DP=$D0 when selecting between direct-page
; (two-byte) and extended (three-byte) addressing for each subsequent SOURCE
; LINE, which is what allows the draw engine's VIA accesses to assemble
; compactly. Its effect depends on source position and is entirely distinct
; from the runtime `lda #$d0 / tfr a,dp` inside VXT_RPC_INIT, which sets the
; CPU's actual DP register. Both are required.
;-------------------------------------------------------------------------------
        setdp   #$d0

        include "vxt/vxt_rpc_macros.asm"   ; equates and macros only; no bytes
        include "vxt/vxt_input.asm"        ; controller state -> parmRam
; vxt_smart.asm per-game switches - every app defines all three (an undefined symbol in an asm6809
; `if` is false). This cart was written against the engine before they existed: all off.
VXT_BLANK_BEFORE_ZERO   equ     0
VXT_BLANK_AT_EXPIRY     equ     0
VXT_MOVEHUGE            equ     0
        include "vxt/vxt_smart.asm"        ; the SmartList draw engine
        include "vxt/vxt_sound.asm"        ; PSG command-block service

;-------------------------------------------------------------------------------
; RPC IDENTIFIERS.
;
; Identifier space, per vxt_rpc.h: 0-18 belong to the upstream multicart
; firmware, 64-79 are reserved for the toolkit, and vxtRpcRegister() refuses
; any identifier at or below 18. Identifier 69 is the engine-level SmartList
; address handshake shared by every application using vxt_smart. 78 and 79 are
; this application's own, being the last two free identifiers in the reserved
; block.
;
; Before selecting your own: main.c's dispatch hook currently routes only 64-79
; to vxtRpcDispatch(), so the 19-63 and 80+ ranges documented in that header
; are unreachable until the test is widened.
;-------------------------------------------------------------------------------
VXT_RPC_ID_SMART_ADDRS  EQU     69      ; publish SmartList routine addresses
VXT_RPC_ID_VXCOOP       EQU     78      ; per frame: compute and compose a frame
VXT_RPC_ID_VXCOOP_INIT  EQU     79      ; one-shot, once per 6809 boot

;-------------------------------------------------------------------------------
main
        VXT_RPC_INIT                    ; copy the stub to RAM and set DP=$D0.
                                        ; Must be first: all subsequent code
                                        ; depends on DP, and the stub cannot be
                                        ; called before it has been copied.
        VXT_SOUND_INIT                  ; silence the PSG and set the sequence
                                        ; shadow to a value no STM32 sequence
                                        ; number can match, so that the first
                                        ; command block is always applied
        VXT_INPUT_INIT                  ; enable controller 1's two pots and
                                        ; disable controller 2, which saves the
                                        ; BIOS two pot reads per frame

        ; Maximum joystick A/D resolution. VXT_INPUT_INIT sets POTRES to $80,
        ; the minimum resolution and fastest read, which is appropriate for
        ; Joy_Digital and too coarse for the proportional response required by
        ; the projection pages.
        clr     Vec_Joy_Resltn

;-------------------------------------------------------------------------------
; The address handshake. One-shot, and the mechanism by which the STM32 is able
; to emit code addresses it cannot know at compile time.
;
; A SmartList record has the form (A, B, subroutine-address): the 6809's
; `pulu a,b,pc` loads the next record's two data bytes and jumps to its routine
; in a single instruction. The STM32 must therefore write genuine 6809
; addresses into the list, but those routines are located wherever this
; cartridge's assembler placed them. The cartridge consequently publishes them
; into parmRam once at startup, and the STM32 caches the resulting table.
;
; The order and offsets below must match the vxtSmartAddrs structure unpacked
; by vxt_smart_addrs_handler() on RPC 69. Emitting records before the table is
; populated is a no-op, which fails as a blank screen rather than by jumping
; the 6809 to an arbitrary address.
;
; An application may publish a subset. Routines it never uses may be omitted,
; and the emitter functions requiring them are never called. The single
; exception is startDraw32, which the dispatcher rather than the application
; elects to use; vxtSmartHasDraw32() range-checks it before any record is
; emitted through it.
;-------------------------------------------------------------------------------
        ldd     #SM_setScale
        std     $7f00
        ldd     #SM_setIntensity
        std     $7f02
        ldd     #SM_recenter
        std     $7f04
        ldd     #SM_startMove_d         ; blanked run, scale 12  ( 1 nop)
        std     $7f06
        ldd     #SM_startDraw_d         ; lit run,     scale 12  ( 1 nop)
        std     $7f08
        ldd     #SM_continue_d          ; continues whichever mode is open; it
        std     $7f0a                   ; never touches the shift register, and
                                        ; is the least expensive record available
        ldd     #SM_end
        std     $7f0c
        ldd     #SM_startMoveBig_d      ; blanked run, scale 32  (12 nops)
        std     $7f0e
        ldd     #SM_startDrawBig_d      ; lit run,     scale 64  (27 nops)
        std     $7f10
        ldd     #SM_startDrawHuge_d     ; lit run, any scale; polls Timer 1
        std     $7f12                   ; rather than padding, at approximately
                                        ; 2.2 times the dispatch cost
        ldd     #SM_setScaleHi          ; stages Timer 1's real high byte
        std     $7f14
        ldd     #SM_startDrawHuge16_d   ; consumes that high byte: the full
        std     $7f16                   ; 16-bit ramp duration, removing the
                                        ; 8-bit reach ceiling
        ldd     #SM_startDraw32_d       ; lit run,     scale 32  (12 nops);
        std     $7f18                   ; the draw-scale ladder's fast tier
        VXT_RPC #VXT_RPC_ID_SMART_ADDRS ; blocks until the STM32 has stored them

        VXT_RPC #VXT_RPC_ID_VXCOOP_INIT ; one-shot: reset demo state and load
                                        ; calibration from the SD card. This is
                                        ; the only point at which blocking file
                                        ; I/O is permitted; see the reference guide's
                                        ; calibration chapter.

;-------------------------------------------------------------------------------
; The frame loop. Four steps, each of whose position was established by
; hardware measurement rather than preference.
;-------------------------------------------------------------------------------
frame_loop
        jsr     Wait_Recal              ; BIOS frame synchronisation and
                                        ; integrator zero. This is the frame
                                        ; clock.

        VXT_INPUT_READ  Joy_Analog      ; Joy_Analog reports absolute stick
                                        ; position; Joy_Digital reports only
                                        ; -1, 0 or +1. The two are drop-in
                                        ; interchangeable, taking the same
                                        ; entry values and returning results in
                                        ; the same locations, so substituting
                                        ; one for the other is a single-token
                                        ; change requiring no STM32 edit: the
                                        ; toolkit reports the axes raw and
                                        ; leaves their interpretation to the
                                        ; STM32.
                                        ;
                                        ; This macro also restores VIA_DDR_a
                                        ; after Read_Btns, which is documented
                                        ; to modify it. Port A must be an
                                        ; output for the DAC to drive the beam,
                                        ; so drawing after a button read
                                        ; without that restore is unsafe. The
                                        ; restore is not redundant.

        VXT_RPC #VXT_RPC_ID_VXCOOP      ; The handoff.
                                        ; Writing the identifier to $7FFF
                                        ; causes the STM32 to leave its
                                        ; bus-servicing loop and execute the
                                        ; handler. ROM is not served for that
                                        ; entire duration, which is why the
                                        ; trigger-and-wait sequence executes
                                        ; from RAM. On return, this frame's
                                        ; SmartList is present at $0800 and its
                                        ; PSG commands at $2000.

        jsr     vxt_sound               ; Service any PSG register/value pairs
                                        ; queued by the STM32. After the RPC,
                                        ; since there is nothing to service
                                        ; before it, and before the draw:
                                        ; Sound_Byte modifies Port A and Port
                                        ; B's multiplexer state, so calling it
                                        ; between vector records would corrupt
                                        ; the beam. This is the only point in
                                        ; the frame at which no operation is in
                                        ; flight. The call costs approximately
                                        ; 12 cycles when the sequence byte is
                                        ; unchanged.

        ldu     #$0800                  ; U = the SmartList base. The engine
        jsr     vxt_smart               ; consumes the list via `pulu a,b,pc`
                                        ; and returns at the SM_end record.

        bra     frame_loop

;-------------------------------------------------------------------------------
; Defence 2: the RAM-resident stub body, physically last, after every branch
; target above. See this file's header.
;-------------------------------------------------------------------------------
        include "vxt/vxt_rpc_stub.asm"
