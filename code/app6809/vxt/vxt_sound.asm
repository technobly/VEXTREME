;*******************************************************************************
; vxt_sound.asm — VXT Toolkit sound-service module (6809 side)
;
; STM32 computes, 6809 services, using the
; BIOS PSG primitive Sound_Byte (WRREG, $F256) rather than raw VIA writes.
;
; Requires: DP = $D0 (toolkit standing rule)
;           vectrex.i included
;
; NOTE: badapple.asm's `doaudio` is not a PSG
; write example — it drives the analog mux DAC (Port B = $06 leaves BC1/BDIR
; both low, i.e. PSG bus inactive; mux sel = 3 = audio line). It is a PCM
; reference, not a direct-VIA-PSG reference. Do not reuse its 6-nop pad here.
;*******************************************************************************

;-------------------------------------------------------------------------------
; Shared block layout (STM32 -> 6809, in the served image)
;-------------------------------------------------------------------------------
; MOVED from $1000 -> $2000: the vector list at $0800 grows past $1000 at
; high object counts and would overwrite this block, permanently killing
; the PSG. must stay in step with vxt_sound.h's VXT_SND_OFFSET.
VXT_SND_BLOCK   EQU     $2000           ; byte 0 = seq, then [reg][val] pairs,
                                        ; terminated by $FF in the reg position.
VXT_SND_END     EQU     $FF             ; project-defined terminator (not the
                                        ; undocumented Sound_Bytes terminator)

;-------------------------------------------------------------------------------
; Toolkit RAM (proposal to freeze Appendix G.4 — see notes)
;-------------------------------------------------------------------------------
; VXT_STUB_BASE   EQU   $C880           ; (defined in vxt_rpc.asm)
; VXT_STUB_SIZE   EQU   32              ; frozen, padded
VXT_VARS_BASE   EQU     $C8A0           ; = VXT_STUB_BASE + VXT_STUB_SIZE
VXT_SND_SEQ     EQU     VXT_VARS_BASE+0 ; 1 byte  <-- vxt_sound's only RAM need
;               (VXT_VARS_BASE+1 .. +15 reserved for vxt_frame/vxt_draw)
;               +1 = VXT_DRAW_LAST_I (vxt_draw.asm)
;               +2 = VXT_SM_SCALE_HI (vxt_smart.asm - staged Timer 1 high
;                    byte for SM_startDrawHuge16_d)
; VXT_APP_BASE    EQU   $C8B0           ; applications start here

;*******************************************************************************
; VXT_SOUND_INIT — call once, after VXT_RPC_INIT. Silences the PSG and puts the
; sequence shadow into a state that guarantees the first block is applied.
;*******************************************************************************
VXT_SOUND_INIT  MACRO
        jsr     Clear_Sound             ; $F272 (INTPSG) — silence PSG + mirrors
        lda     #$FF                    ; seq shadow != any STM32 seq (0,1,2...)
        sta     VXT_SND_SEQ
        ENDM

;*******************************************************************************
; vxt_sound — call once per frame, AFTER the frame's RPC returns and BEFORE
; vxt_draw. (Amendment to E.5.3 pt.3: PSG registers are latched, so discrete
; sound needs ONE hook, not two. Streaming audio would need many — see H.1.)
;
; Entry:  DP = $D0
; Exit:   A,B,X destroyed (Sound_Byte returns X = $C800). U preserved by pshs.
; Cost:   ~12 cycles + 2 loads when seq is unchanged.
;
; Placement rationale: Vol.2 lists Sound_Byte's control-register modifications
; as CNTRL + DAC — it stomps Port B mux state and Port A. Calling it between
; vector records would corrupt the beam. Post-RPC / pre-draw is the one slot
; where nothing is mid-flight, and vxt_draw re-asserts VIA_cntl at loop entry
; anyway (the Appendix I.3 fix).
;*******************************************************************************
vxt_sound
        pshs    u
        ldu     #VXT_SND_BLOCK
        lda     ,u+                     ; sequence number from STM32
        cmpa    VXT_SND_SEQ
        beq     vxt_snd_done            ; nothing new this frame
        sta     VXT_SND_SEQ             ; latch it

vxt_snd_loop
        lda     ,u+                     ; PSG register number
        cmpa    #VXT_SND_END
        beq     vxt_snd_done
        cmpa    #$0D                    ; belt-and-braces: never write reg $0E.
        bhi     vxt_snd_done            ; Reg 14 is the AY's I/O port A = the
                                        ; BUTTONS. Sound_Byte's documented range
                                        ; is $00-$0D; vecx.c refuses reg 14 too.
        ldb     ,u+                     ; value
        jsr     Sound_Byte              ; $F256 WRREG: A=reg, B=data, DP=$D0.
                                        ; Also maintains the $C800 shadow mirror.
                                        ; Clobbers A,B,X — U is safe.
        bra     vxt_snd_loop

vxt_snd_done
        puls    u
        rts

;*******************************************************************************
; vxt_sound_panic — silence everything. Call on app exit / error path.
;*******************************************************************************
vxt_sound_panic
        jsr     Clear_Sound
        lda     #$FF
        sta     VXT_SND_SEQ
        rts
