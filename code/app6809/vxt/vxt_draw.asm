;*******************************************************************************
; vxt_draw.asm - VXT toolkit: draw-loop primitive (6809 side)
;
; Draws a vector list using the format confirmed directly from VOOM's
; original source (voom-voom_asm.txt) - the VIA sequence below is that
; source's, not a re-derivation from datasheets.
;
; Format, 4 bytes per vector record:
;   byte0 = intensity ($00-$7F). Bit 6 ($40) set = recenter integrators
;           first (A masked to $3F after, matching VOOM). $FF = end of list.
;           NEW (v2): byte0 == $00 (after any recenter masking) = BLANK MOVE
;           - the ramp runs with the beam blanked via VIA_shift_reg=$00, and
;           Intensity_a is not called at all. Backward compatible: intensity-0
;           records were always visually invisible; they are now also cheap.
;   byte1 = scale -> VIA_t1_cnt_lo (line length/duration).
;   byte2 = coord1 -> VIA_port_a. byte3 = coord2 (after S/H strobe).
;
; ENTRY: X = list base (#$0800). REQUIRES DP=$D0 + `setdp #$d0` before this
; include. DESTROYS A,B,X. Cart-resident (not RAM-stub) - include anywhere.
;
; v2 CHANGES (from flicker analysis on a multi-box test):
; The frame budget at the BIOS default 50Hz refresh is ~30,000 CPU cycles.
; v1 paid ~125-130 cycles of fixed overhead per record, including an
; unconditional `jsr Intensity_a` on every record - so ~280 records (30
; boxes + a text line) exceeded the whole budget before any line was even
; drawn, guaranteeing flicker regardless of the scale byte. Two changes:
;
;   1. BLANK-move fast path: intensity $00 skips Intensity_a entirely and
;      runs the ramp with VIA_shift_reg=$00 (beam off). Grounded in this
;      file's own existing code: the recenter block and the post-line
;      cleanup already blank the beam exactly this way (`clr VIA_shift_reg`).
;      Every invisible positioning move in every scene gets ~37 cycles
;      cheaper AND stops thrashing the intensity latch.
;
;   2. Intensity cache: for lit records, Intensity_a is called only when
;      the intensity differs from the last one applied (shadow at
;      VXT_DRAW_LAST_I). With blanks no longer thrashing the latch, a scene
;      drawn at one brightness makes ONE Intensity_a call per frame instead
;      of hundreds. Same seq-gate pattern as vxt_sound.asm.
;
; VXT_DRAW_LAST_I is hardcoded to $C8A1: vxt_sound.asm's VXT_VARS_BASE+1,
; the byte its header comment already reserves for vxt_draw. Hardcoded (not
; VXT_VARS_BASE+1 symbolically) because vxt_draw is included BEFORE
; vxt_sound in this project's apps, so the symbol wouldn't exist yet.
;
; NOTE: Intensity_a empirically preserves X (list pointer) - proven across
; Test 3/4/5 on hardware, where X was live across the call in v1.
;
; Bug note retained from v1: VIA_cntl=$ce (PUNZRO) is set unconditionally
; at entry, per VOOM, not only in the recenter branch.
;*******************************************************************************
VXT_DRAW_LAST_I EQU     $C8A1           ; = vxt_sound.asm's VXT_VARS_BASE+1

;*******************************************************************************
; VXT_DRAW_INIT - call once at startup, before the first vxt_draw call.
; Primes the intensity cache with $FF (no real intensity is $FF, so the
; first lit record always calls Intensity_a) - same init pattern as
; VXT_SOUND_INIT's sequence shadow.
;*******************************************************************************
VXT_DRAW_INIT   MACRO
        lda     #$FF
        sta     VXT_DRAW_LAST_I
        ENDM

vxt_draw
  ldb                  #$ce                 ; PUNZRO - unconditional, every
  stb                  VIA_cntl             ; call (Appendix I.3)

vxt_draw_next
  lda                  ,x+                  ; grab intensity
  cmpa                 #$ff                 ; end of list?
  beq                  vxt_draw_done
  bita                 #$80                 ; recenter-to-origin flag?
                                            ; bit 7, not bit 6 as VOOM used.
                                            ; VOOM packed intensity + recenter
                                            ; into one byte with bit 6 as the
                                            ; flag, which capped
                                            ; intensity at 0-63 (any value >=64
                                            ; sets bit 6 and is misread as a
                                            ; beam reset - this really bit us:
                                            ; a gradient using 80 and 127 made
                                            ; most boxes vanish). Bit 7 is
                                            ; unused by the BIOS Intensity_a
                                            ; range (0-127), so moving the flag
                                            ; there frees the FULL range.
                                            ; $FF still ends the list, so a
                                            ; recenter record must keep
                                            ; intensity <= 126.
  beq                  vxt_draw_chk

  ; Zero integrators: return beam to (0,0). Sequence per VOOM, confirmed.
  ldb                  #$cc
  stb                  VIA_cntl
  clr                  VIA_shift_reg
  ldb                  #$03
  clr                  VIA_port_a
  stb                  VIA_port_b
  ldb                  #$02
  stb                  VIA_port_b
  stb                  VIA_port_b
  ldb                  #$01
  stb                  VIA_port_b
  anda                 #$7f                 ; strip the flag; 0-127 now
  ldb                  #$ce
  stb                  VIA_cntl

vxt_draw_chk
  tsta                                      ; intensity 0 = blank move:
  beq                  vxt_draw_blank       ; no Intensity_a, beam stays off
  cmpa                 VXT_DRAW_LAST_I      ; same as last applied?
  beq                  vxt_draw_lit         ; yes - skip the BIOS call
  sta                  VXT_DRAW_LAST_I      ; no - cache, then apply
  jsr                  Intensity_a          ; (preserves X - see header)
vxt_draw_lit
  ldb                  #$ff                 ; beam ON during the ramp
  bra                  vxt_draw_go
vxt_draw_blank
  clrb                                      ; beam OFF during the ramp
vxt_draw_go
  lda                  ,x+                  ; scale
  sta                  VIA_t1_cnt_lo
  lda                  ,x+                  ; coord1
  sta                  VIA_port_a
  clr                  VIA_port_b           ; enable S/H strobe
  lda                  ,x+                  ; coord2  (6 cyc - same settle
  nop                                       ;  timing as v1's ldb ,x+ / nop)
  inc                  VIA_port_b           ; disable S/H strobe
  sta                  VIA_port_a
  stb                  VIA_shift_reg        ; $FF lit / $00 blank
  clr                  VIA_t1_cnt_hi        ; start Timer 1
  ldb                  #$40
vxt_draw_wait
  bitb                 VIA_int_flags
  beq                  vxt_draw_wait
  clr                  VIA_shift_reg        ; beam off between records

  bra                  vxt_draw_next

vxt_draw_done
  rts
