;*******************************************************************************
; vxt_smart.asm - VXT toolkit: SmartList draw engine (6809 side)
;
; Based on the technique Malban documents in "VPatrol ingenuity part III"
; (vide.malban.de, 6 Feb 2018), attributed there to Kristof. Malban's measured
; numbers on the same 60-vector glider:
;       Draw_VL_Mode  7803 cycles   (~130/vector)
;       Draw_VLp      4825 cycles   (~80/vector)
;       drawSmart     2122 cycles   (~35/vector)   <-- this technique
; Our vxt_draw.asm costs ~77-98 cycles/record, so this is a ~2.2x win.
;
; How it works. The list is a string of (A, B, subroutine-address) triples.
; Every routine ends with `pulu a,b,pc`, which loads the NEXT record's A and B
; and jumps to the NEXT record's routine in one instruction - Malban: "for the
; cost of mere 9 cycles". That single instruction replaces vxt_draw's entire
; ~33-cycle interpretive dispatch (lda ,x+ / cmpa #$ff / bita / tsta / cmpa
; cache / branches). Same 4 bytes per record we already use.
;
; List format (4 bytes/record, U points at it):
;       db  y, x            ; -> A, B
;       dw  SM_<routine>    ; -> PC
; Terminate with:
;       db  0, 0
;       dw  SM_end
;
; Two further tricks, both from the post:
;   * The shift register is not touched between vectors, so consecutive draws
;     (or moves) chain without re-blanking - "the subroutine can be used for a
;     'continued' move or a 'continued' draw".
;   * No spin-wait loops. "Within all SmartList functions there is not one Wait
;     loop or interrupt flag interrogating - the timer is always expired before
;     the next relevant changes are made." vxt_draw's `bitb VIA_int_flags/beq`
;     loop disappears entirely.
;
; *** TIMING - read this ***
; Malban: the routines work out of the box "up to a scale of 9 (perhaps 10).
; Above that scale the routines have timing issues", because Timer 1 must be
; allowed to expire before anything new is done. His fix is to insert nops
; after each `clr <VIA_t1_cnt_hi`:
;       nop (SPRITE_SCALE-9)/2
; VXT_SM_SCALE below drives the ADD_NOPS macro to do exactly that. If lines
; come out short or partly drawn, the scale is outrunning the nop padding -
; raise VXT_SM_SCALE (which adds nops) or lower the scale.
;
; Honest note on provenance: the post shows SM_setScale in full, but the
; move/draw routine bodies are images that did not survive the PDF text
; extraction. The routines below are reconstructed from vxt_draw.asm's own
; (hardware-proven) VIA sequence, restructured into the pulu-dispatch shape.
; The dispatch mechanism and the no-wait/shift-register tricks are Malban's
; and are quoted above; the exact instruction bodies are ours and WILL need
; hardware iteration. vxt_draw.asm is untouched and remains the fallback.
;
; ENTRY: U = list base. Requires DP=$D0 and `setdp #$d0`.
; DESTROYS: A, B, U, and PC obviously. X is preserved.
;*******************************************************************************

VXT_SM_SCALE    EQU     $0C             ; must match the emitter's scale byte
                                        ; (0x0C = 12 decimal)

;-------------------------------------------------------------------------------
; PER-game switches . Three hardware-found
; hardware-found changes (2026-09-19) that older apps were never tested with. Each app
; must define all three (0 or 1) before including this file:
;   VXT_BLANK_BEFORE_ZERO  SM_recenter blanks the beam BEFORE asserting ZERO
;                          (same instructions, reordered: costs nothing)
;   VXT_BLANK_AT_EXPIRY    SM_startDrawHuge_d / SM_startDrawHuge16_d blank the
;                          beam when Timer 1 expires (+6 cycles per huge record)
;   VXT_MOVEHUGE           assemble SM_startMoveHuge_d (the app must also
;                          publish its address in the RPC 69 block)
; With all three 0 this file assembles byte-identical to VEXTREME's f67d300
; copy; all three 1 is the newer, recommended setting.
; asm6809 treats an UNDEFINED symbol in `if` as false without an error, so these
; equates exist to fail the build on a missing or misspelt switch instead.
VXT_SW_CHECK_1  equ     VXT_BLANK_BEFORE_ZERO
VXT_SW_CHECK_2  equ     VXT_BLANK_AT_EXPIRY
VXT_SW_CHECK_3  equ     VXT_MOVEHUGE

; Malban's ADD_NOPS: pad so Timer 1 has expired before the next change.
; `nop (VXT_SM_SCALE-9)/2` is invalid syntax - the 6809's
; NOP takes NO operand at all (it's not like x86 REP-prefixed instructions);
; there is no "repeat count" form. asm6809 has no REPT/repeat-block directive
; used anywhere else in this project either, so rather than guess at one,
; this emits the LITERAL nop count computed for our actual VXT_SM_SCALE:
;     (12 - 9) / 2 = 1     (integer division, truncated)
; If VXT_SM_SCALE changes, recompute (SCALE-9)/2 and adjust the nop count
; below by hand - this is a fixed constant, not a live macro parameter.
ADD_NOPS        MACRO
                nop                     ; 1 nop for VXT_SM_SCALE=$0C - see above
                ENDM

;-------------------------------------------------------------------------------
; drawSmart - kick the list off. Malban: "the drawSmart routine is called
; (which only does a pulu d,pc)". That is the entire dispatcher.
;-------------------------------------------------------------------------------
vxt_smart
        ldb     #$ce                    ; PUNZRO, unconditionally, once per
        stb     <VIA_cntl               ; frame
        pulu    a,b,pc                  ; -> first record's routine

;-------------------------------------------------------------------------------
; SM_setScale - B = scale. Verbatim from the post.
;-------------------------------------------------------------------------------
SM_setScale
        stb     <VIA_t1_cnt_lo
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_setIntensity - B = intensity (0-127, FULL range: no bit is stolen for a
; recenter flag here, unlike vxt_draw's packed byte0). Only emit this when the
; intensity changes - it is the one expensive routine (a BIOS call).
;-------------------------------------------------------------------------------
SM_setIntensity
        pshs    u                       ; not `pshu u`, which
                                        ; is invalid - PSHU pushes onto the
                                        ; stack pointed to by U, so U cannot
                                        ; push itself. S (the hardware stack,
                                        ; independent of U) is what protects
                                        ; U across the call.
        tfr     b,a                     ; Intensity_a's clobbers are not fully
        jsr     Intensity_a             ; characterized; U carries our list
        puls    u                       ; pointer, so protect it
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_recenter - zero the integrators. A, B ignored. Sequence per VOOM.
;-------------------------------------------------------------------------------
; CHANGED 2026-09-19 (a game port, hardware-found): the beam is now blanked BEFORE
; ZERO is asserted. It used to be ZERO first, blank a few cycles later. A lit line
; (SM_startDraw*) leaves the beam ON until something blanks it, so with ZERO asserted
; first the integrators swung the still-lit beam toward the screen centre for those
; cycles - a "tail toward the centre" on every lit line end followed by a Recenter,
; longer the farther from centre, and a double-dot on dots. Only the ORDER changed.
; (2026-10-24: behind VXT_BLANK_BEFORE_ZERO; 0 = the original order.)
SM_recenter
        if      VXT_BLANK_BEFORE_ZERO
        clr     <VIA_shift_reg          ; beam OFF first
        ldb     #$cc
        stb     <VIA_cntl               ; then ZERO
        else
        ldb     #$cc
        stb     <VIA_cntl
        clr     <VIA_shift_reg
        endif
        ldb     #$03
        clr     <VIA_port_a
        stb     <VIA_port_b
        ldb     #$02
        stb     <VIA_port_b
        stb     <VIA_port_b
        ldb     #$01
        stb     <VIA_port_b
        ldb     #$ce
        stb     <VIA_cntl
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_startMove_d - A=y, B=x. Blanks the beam, then ramps. Use for the FIRST
; move of a run; SM_continue_d chains after it with the beam still blanked.
;-------------------------------------------------------------------------------
SM_startMove_d
        sta     <VIA_port_a             ; y
        clr     <VIA_port_b             ; S/H strobe on (mux = y)
        nop                             ; settle (as vxt_draw does)
        inc     <VIA_port_b             ; S/H strobe off
        stb     <VIA_port_a             ; x
        clr     <VIA_shift_reg          ; beam OFF for the ramp
        clr     <VIA_t1_cnt_hi          ; start ramp
        ADD_NOPS
        pulu    a,b,pc


;-------------------------------------------------------------------------------
; SM_startMoveBig_d - identical to SM_startMove_d, but padded for scale $20 (32),
; not the project's default scale $0C (12). Exists because ADD_NOPS's
; padding is computed once, at assembly time, for VXT_SM_SCALE - it
; has no way to know what scale is loaded when a routine runs. Row
; repositioning (vxtSmartMoveBig, STM32 side) sets scale to POS_SCALE (32) but
; was dispatching through the same 1-nop-padded SM_startMove_d used for
; scale-12 box edges - Timer 1 had not expired before the next VIA write, the
; exact "drift madness" Malban's post warns about for under-padded high-scale
; operations. Malban's own formula, (SCALE-9)/2, gives 11 nops for scale 32 -
; and his post's own worked high-scale example uses this EXACT scale ($20),
; so this isn't extrapolated, it's his own tested value.
;
; Used only by vxtSmartMoveBig() (STM32 side) for row/position repositioning.
; Box edges continue through SM_startMove_d/SM_continue_d at scale 12, which
; were already correctly padded and are unaffected by this fix.
;-------------------------------------------------------------------------------
SM_startMoveBig_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        clr     <VIA_shift_reg
        clr     <VIA_t1_cnt_hi
        nop                     ; 11 nops for scale $20=32: (32-9)/2=11,
        nop                     ; Malban's own formula. not the shared
        nop                     ; ADD_NOPS macro (that stays tuned for the
        nop                     ; project's default scale, 12) - this
        nop                     ; routine exists SPECIFICALLY because one
        nop                     ; nop count cannot correctly serve two
        nop                     ; different scales at once.
        nop
        nop
        nop
        nop
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_startDrawBig_d - identical to SM_startDraw_d, but padded for scale $40
; (64), not the project's default scale $0C (12). Added when a caller
; needed long draws in ONE record with NO chaining at all - grid lines up
; to 6400 phys units, and a scale-32 version of this routine (first
; attempt) still needed 2 chained records (3200 each), leaving a visible
; mid-line joint that measurably affected the projected geometry once
; yaw/pitch rotation was applied. At scale 64, a rate of +-100 (this
; project's established chaining safety margin) covers +-6400 phys units
; in ONE record, zero joints.
;
; *** Unverified on hardware as of this writing - read before trusting ***
; The scale-32/11-nop pairing this routine originally used (still used by
; SM_startMoveBig_d) IS hardware-proven. This scale-64/27-nop pairing is not
; - it is Malban's own formula, (SCALE-9)/2, applied one step further than
; this project has ever gone (12 -> 32 -> 64), not a value anyone has
; confirmed on real Vectrex hardware yet. If lines still come out short,
; wavy, or misplaced at this scale, the formula itself may not extrapolate
; cleanly this far - do not assume linearity holds without seeing it draw
; correctly first.
;
; Used only by vxtSmartDrawBig() (STM32 side). Chains by repeating THIS
; routine for each step (never a separate "continue" variant) - mirroring
; vxtSmartMoveBig's own exact chaining pattern (vxt_smart.c: `while
; (ry!=0||rx!=0) sm_rec(sy,sx,sm_addrs.startMoveBig)`), since re-asserting
; VIA_shift_reg=$FF when it's already $FF is a harmless no-op. Existing
; scale-12 SM_startDraw_d/SM_continue_d and scale-32 SM_startMoveBig_d are
; UNCHANGED and remain correct for their own uses - this is a new, additive
; routine, not a replacement.
;-------------------------------------------------------------------------------
; SM_startDraw32_d - DRAW at scale $20 (32).
;
; The missing third draw scale. Until now draws existed only at 12
; (SM_startDraw_d, 1 nop) and 64 (SM_startDrawBig_d, 27 nops); scale 32 had
; a MOVE routine (SM_startMoveBig_d) but no draw counterpart.
;
; Why it is needed. SM_startDrawBig_d costs 97 6809 cycles, 52 of which are
; its 27 NOPs waiting out the scale-64 beam ramp - and Timer 1 counts down
; from the SCALE, not from the line's length, so a short edge at scale 64
; burns the full scale-64 wait for nothing. Dropping short edges to scale 12
; was tried first and was much faster but produced TWO hardware
; defects, both traceable to the scale itself:
;
;   1. CONVERGENCE. A draw lands on a multiple of its own scale, and
;      repositions land on multiples of the reposition grid's scale (32).
;      64 = 2*32 so
;      scale-64 landings sit on the reposition grid; 12 shares only a factor
;      of 4 with 32, so the two grids coincide just every 96 units. Vertices
;      reached by a short edge and by a reposition disagreed by up to ~22
;      units (~1.2px) - reported as "two separate dots" where mesh and hull
;      lines should meet. Scale 32 IS the reposition grid, so this term
;      vanishes by construction.
;   2. BRIGHTNESS. Dwell time is proportional to scale, so scale 12 is 5.3x
;      dimmer than 64 at any length - reported as a meter hash "not a
;      complete line". Scale 32 is 2x, not 5.3x.
;
; Cost: 12 nops, Malban's own (SCALE-9)/2 for scale 32 - the same count
; SM_startMoveBig_d already uses and which is hardware-proven at this
; scale, so the padding is not an extrapolation here (unlike
; SM_startDrawBig_d's 27, which is still flagged unverified in its own
; header). ~67 cycles vs 97: a 30-cycle saving per short edge with no
; grid or brightness regression.
;
; Identical to SM_startMoveBig_d except for the shift-register write that
; turns the beam ON - exactly the relationship SM_startDraw_d has to
; SM_startMove_d. Additive: every existing routine is untouched.
;-------------------------------------------------------------------------------
SM_startDraw32_d
        sta     <VIA_port_a             ; y rate
        clr     <VIA_port_b             ; S/H strobe on (mux = y)
        nop                             ; settle
        inc     <VIA_port_b             ; S/H strobe off
        stb     <VIA_port_a             ; x rate
        lda     #$ff
        sta     <VIA_shift_reg          ; beam ON for the ramp
        clr     <VIA_t1_cnt_hi          ; start ramp
        nop                             ; 12 nops for scale $20=32:
        nop                             ; (32-9)/2 = 11.5 -> 12, Malban's
        nop                             ; own formula. Same count
        nop                             ; SM_startMoveBig_d uses, and that
        nop                             ; pairing is hardware-proven.
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        pulu    a,b,pc

;-------------------------------------------------------------------------------
SM_startDrawBig_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        lda     #$ff
        sta     <VIA_shift_reg          ; beam ON for the ramp
        clr     <VIA_t1_cnt_hi
        nop                     ; 27 nops for scale $40=64: (64-9)/2=27,
        nop                     ; Malban's own formula extrapolated PAST the
        nop                     ; scale-32/11-nop pairing this project has
        nop                     ; verified on hardware - see the
        nop                     ; UNVERIFIED warning above. not the shared
        nop                     ; ADD_NOPS macro (tuned for scale 12) and
        nop                     ; not the 11-nop count SM_startMoveBig_d
        nop                     ; uses for scale 32 - a fixed constant for
        nop                     ; THIS specific scale.
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_startDrawHuge_d - identical to SM_startDrawBig_d, but with the fixed NOP
; block replaced by a GENUINE Timer 1 poll - the exact wait vxt_draw.asm uses
; (`ldb #$40 / wait: bitb VIA_int_flags / beq wait`), correct for any scale
; loaded into VIA_t1_cnt_lo, not just one pre-verified at assembly time.
;
; The scale-64/27-nop pairing above was flagged UNVERIFIED when written and
; never independently hardware-tested; any long-line draw depending on it
; (grid lines, complex models with many long edges) inherits that risk,
; worst on the models with the longest, most-chained edges. A model's
; longest edges need many chained BigScale steps (worse the closer it gets,
; since projected edge length grows toward the eye), each one a fresh
; dispatch through that unverified timing. This routine exists so the
; STM32 side can pick whatever scale minimizes chained steps for
; a given edge (see gamelibDrawHugeLine(), gamelib_beam.c) WITHOUT needing
; a matching hand-verified nop count for every scale it might choose - the
; poll is correct regardless.
;
; Cost: real, and paid on every use - `vxt_draw.asm`'s own header measured
; this same technique at ~2.2x a SmartList dispatch's cost. Deliberately
; scoped to only this one routine, used only where a caller explicitly opts
; into it (gamelibDrawHugeLine()) - every other SmartList path (small-scale
; chaining, the proven scale-32 MoveBig, and the still-unverified-but-
; unchanged scale-64 DrawBig) is untouched: a targeted exception, not a
; blanket default, applied here to engine choice instead of chaining
; technique.
;-------------------------------------------------------------------------------
; CHANGED 2026-09-19 (a game port, hardware-found): the beam is now BLANKED the moment the
; Timer 1 poll sees the ramp expire (`clr <VIA_shift_reg` before the dispatch). It used to stay
; lit until the NEXT record blanked it or re-lit it. On a chain of consecutive lit records that
; parked the integrators at every vertex, beam ON, for the whole time the next record needed to
; reload the DAC/S-H (~30 cycles - as long as a short segment's own ramp), painting a bright dot
; at every joint: the "connect the dots" look on the rings and the cannon. The BIOS never has
; this: every BIOS vector ends by blanking at timer expiry (F3F9 / draw_1_ring's `sta <$0A`).
; The next record's rate setup therefore runs dark and it re-lights only for its own ramp.
; Same change in SM_startDrawHuge16_d.
SM_startDrawHuge_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        lda     #$ff
        sta     <VIA_shift_reg          ; beam ON for the ramp
        clr     <VIA_t1_cnt_hi          ; start Timer 1
        ldb     #$40
SM_drawHuge_wait
        bitb    <VIA_int_flags          ; genuine hardware wait - see header
        beq     SM_drawHuge_wait        ; comment above, same technique as
                                        ; vxt_draw.asm's vxt_draw_wait
        if      VXT_BLANK_AT_EXPIRY     ; (2026-10-24: switch; 0 = the original)
        clr     <VIA_shift_reg          ; beam OFF at expiry (see the CHANGED note above)
        endif
        pulu    a,b,pc

        if      VXT_MOVEHUGE            ; (2026-10-24: switch; 0 = not assembled)
;-------------------------------------------------------------------------------
; SM_startMoveHuge_d - A BLANKED move of A=y, B=x rate
; units whose wait is a genuine Timer 1 poll, correct at any scale - the dark twin of
; SM_startDrawHuge_d. Needed because SM_startMoveBig_d is padded for scale 32 only, and
; one game draws its rings at 20/30/40 (and the expanding ones from 10 up): a destroyed
; ring segment is a blank move of the same vector at the same scale, and quantising it to
; the 32 grid would land every following segment off its neighbours. The beam is forced
; off before the ramp starts, so it is safe to follow a lit record whatever it left behind.
;-------------------------------------------------------------------------------
SM_startMoveHuge_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        clr     <VIA_shift_reg          ; beam OFF for the whole ramp
        clr     <VIA_t1_cnt_hi          ; start Timer 1
        ldb     #$40
SM_moveHuge_wait
        bitb    <VIA_int_flags          ; genuine poll, any scale
        beq     SM_moveHuge_wait
        pulu    a,b,pc
        endif                           ; VXT_MOVEHUGE

;-------------------------------------------------------------------------------
; SM_setScaleHi / SM_startDrawHuge16_d - widen the ramp DURATION itself past
; the 8-bit ceiling every routine above has always had, by using Timer 1's
; real 16-bit range instead of discarding its high byte.
;
; Answers the question "why can't one record just draw the whole line, the
; Vectrex can do that natively?" - it CAN. VIA_t1_cnt_hi ($D005) is the high byte
; of a genuine 16-bit down-counter (standard 6522 VIA Timer 1) - but every
; single existing routine in this file AND vxt_draw.asm unconditionally does
; `clr <VIA_t1_cnt_hi` when starting a ramp, discarding 248 of the 256
; possible high-byte values and capping every ramp's real duration (hence
; reach, since distance = coordinate-byte x duration) at whatever fits in the
; LOW byte alone (max 255). That 8-bit ceiling was never a Vectrex hardware
; limit - it's this project's own record format only ever having carried an
; 8-bit "scale" byte, inherited faithfully from VOOM's original design (which
; likely never needed more, since its per-segment adaptive scale keeps
; individual segments short). Loading a real high byte instead of hardcoding
; zero raises the max single-record reach from ~25,500 phys units (100*255)
; to ~6,553,500 (100*65535) - comfortably past any realistic single edge
; a game built on this toolkit is likely to draw, near-plane distortion
; included. No chaining needed at that point.
;
; The high byte can't be pre-loaded into VIA_t1_cnt_hi directly - writing it
; is what TRANSFERS the latched low byte + this high byte into the real
; counter and STARTS the ramp (standard 6522 semantics), so it must be
; staged somewhere else first and loaded from there at the moment the ramp
; starts. VXT_SM_SCALE_HI (Vectrex work RAM, not the VIA register)
; is that staging byte - outside the $D0xx direct page this file otherwise
; uses, same precedent as vxt_draw.asm's VXT_DRAW_LAST_I (also outside DP,
; accessed via plain extended addressing, no `<` prefix).
;
; Additive, not a replacement: SM_startDrawHuge_d above is untouched and
; stays the path for anything that already fits in 8 bits (the common,
; hardware-confirmed case) - gamelibDrawHugeLine() only escalates to
; this 16-bit path when the needed scale exceeds 255. Still a
; genuine Timer 1 poll, not nop-padding - correct at any 16-bit duration with
; no calibration, for the same reason SM_startDrawHuge_d needed none.
;-------------------------------------------------------------------------------
; Hardcoded to $C8A2 (= vxt_sound.asm's VXT_VARS_BASE+2), not referenced
; symbolically - same reason vxt_draw.asm's VXT_DRAW_LAST_I is hardcoded to
; $C8A1: apps that don't include vxt_sound.asm at all would fail to
; assemble against a symbol that doesn't exist. vxt_sound.asm's own
; header already reserves VXT_VARS_BASE+1..+15 for vxt_frame/vxt_draw;
; +1 is already VXT_DRAW_LAST_I, so this claims +2.
VXT_SM_SCALE_HI EQU     $C8A2

SM_setScaleHi
        stb     VXT_SM_SCALE_HI         ; extended addressing - not on the
                                        ; direct page, same as VXT_DRAW_LAST_I
        pulu    a,b,pc

SM_startDrawHuge16_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        lda     #$ff
        sta     <VIA_shift_reg          ; beam ON for the ramp
        lda     VXT_SM_SCALE_HI         ; the only difference from
        sta     <VIA_t1_cnt_hi          ; SM_startDrawHuge_d: a real high
                                        ; byte, not a hardcoded 0 - this is
                                        ; what starts the ramp (6522
                                        ; semantics) with the full 16-bit
                                        ; duration staged above
        ldb     #$40
SM_drawHuge16_wait
        bitb    <VIA_int_flags          ; same genuine poll as
        beq     SM_drawHuge16_wait      ; SM_startDrawHuge_d - correct at any
                                        ; duration, 8-bit or 16-bit alike
        if      VXT_BLANK_AT_EXPIRY     ; (2026-10-24: switch; 0 = the original)
        clr     <VIA_shift_reg          ; beam OFF at expiry (see SM_startDrawHuge_d)
        endif
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_startDraw_d - A=y, B=x. Lights the beam, then ramps.
;-------------------------------------------------------------------------------
SM_startDraw_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        lda     #$ff
        sta     <VIA_shift_reg          ; beam ON for the ramp
        clr     <VIA_t1_cnt_hi
        ADD_NOPS
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_continue_d - A=y, B=x. The workhorse: does not touch the shift register,
; so it continues whatever mode the last start_* set (draw or move). This is
; Malban's key saving: "the clever part here is that the shift register is not
; changed. This a) saves cycles b) the subroutine can be used for a 'continued'
; move or a 'continued' draw."
;-------------------------------------------------------------------------------
SM_continue_d
        sta     <VIA_port_a
        clr     <VIA_port_b
        nop
        inc     <VIA_port_b
        stb     <VIA_port_a
        clr     <VIA_t1_cnt_hi
        ADD_NOPS
        pulu    a,b,pc

;-------------------------------------------------------------------------------
; SM_end - stop. The list's last record must point here.
;-------------------------------------------------------------------------------
SM_end
        clr     <VIA_shift_reg          ; leave the beam blanked
        rts
