SPRITE_SCALE        equ      #34
;/* HIGHEST SCALE FOR SMARTLIST + CONTINUE is 16!

;***************************************************************************
; ROUTINE SECTION
;***************************************************************************
                    jsr      DP_to_D0
;***************************************************************************
ADD_NOPS
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
                    rts
ADD_NOPS_NOU
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
                    rts
drawSmart                                                 ;#isfunction
                    pulu     d,pc
;***************************************************************************
SM_setScale                                               ;#isfunction
                    stb      <VIA_t1_cnt_lo
                    pulu     a,b,pc
SM_setIntensityScale                                      ;#isfunction
                    stb      <VIA_t1_cnt_lo
SM_setIntensity                                           ;#isfunction
                    sta      <VIA_port_a
                    ldd      #$0401
                    sta      <VIA_port_b
                    stb      <VIA_port_b
                    pulu     a,b,pc
; continue uses same shift
SM_continue_d7                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b
SM_continue_d6                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b
SM_continue_d5                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b
SM_continue_d4                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b
SM_continue_d3                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b
; two consecutive continues, most common case,
; than save a pulu
SM_continue_d2                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b
; continue uses same shift
SM_continue_d                                             ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    clra                                  ; special case entry. new y = old X, saves 4 cycles!
; y is inherently known to be == old_x, y was set to 0 by generator
SM_continue_newY_eq_oldX                                  ;#isfunction
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_continue_d_x0                                          ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    stb      <VIA_port_b
                    lda      #1
                    std      <VIA_port_b
                    stb      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_continue_d_y0                                          ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
; continue uses same shift
; y is inherently known to be == x, y was set to 0 by generator
SM_continue_yEqx                                          ;#isfunction
                    std      <VIA_port_b                  ; shift not changed, move might also be a draw
                    inc      <VIA_port_b
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_continue_d_double                                      ;#isfunction
                    sta      <VIA_port_a
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    bra      SM_repeat_same

SM_continue_d_double_x0                                   ;#isfunction
                    sta      <VIA_port_a
                    stb      <VIA_port_b
                    lda      #1
                    std      <VIA_port_b
                    stb      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    bra      SM_repeat_same

SM_continue_d_double_y0                                   ;#isfunction
                    sta      <VIA_port_a
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    bra      SM_repeat_same

SM_repeat_same                                            ;#isfunction
                    pulu     a,b
                    clr      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     pc
SM_startMove_d                                            ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_shift_reg               ; this should be done 2-3 cycles earlier!
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_startMove_d_sj                                         ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_shift_reg               ; this should be done 2-3 cycles earlier!
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS_NOU                          ; reduced by ldu ,u - 5 cycles
                    ldu      ,u
                    pulu     a,b,pc
SM_startMove_d_double_sj                                     ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    bra      SM_continue_sj

SM_continue_sj                                               ;#isfunction
                    ldu      ,u
                    pulu     a,b
                    clr      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     pc

SM_continue_d_x0_sj                                       ;#isfunction
                    sta      <VIA_port_a                  ; shift not changed, move might also be a draw
                    stb      <VIA_port_b
                    lda      #1
                    std      <VIA_port_b
                    stb      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS_NOU                          ; reduced by ldu ,u - 5 cycles
                    ldu      ,u
                    pulu     a,b,pc
SM_startMove_d_double                                     ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    bra      SM_continue

SM_startMove_d_x0_sj                                      ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    stb      <VIA_port_b
                    lda      #$01
                    std      <VIA_port_b
                    stb      <VIA_shift_reg
                    stb      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS_NOU                          ; reduced by ldu ,u - 5 cycles
                    ldu      ,u
                    pulu     a,b,pc
SM_startMove_d_x0                                         ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    stb      <VIA_port_b
                    lda      #$01
                    std      <VIA_port_b
                    stb      <VIA_shift_reg
                    stb      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_continue                                               ;#isfunction
                    pulu     a,b
                    clr      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     pc
SM_startMove_d_y0_sj                                      ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS_NOU                          ; reduced by ldu ,u - 5 cycles
                    ldu      ,u
                    pulu     a,b,pc
SM_startMove_d_y0                                         ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light off
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    clra
                    sta      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_startDraw_d                                            ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light on
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    ldd      #$00FF
                    stb      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_continue_double                                        ;#isfunction
                    clr      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
SM_startDraw_d_double                                     ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light on
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    ldd      #$00FF
                    stb      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    bra      SM_continue

SM_startDraw_d_x0                                         ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light on
                    stb      <VIA_port_b
                    lda      #$01
                    std      <VIA_port_b
                    lda      #$FF
                    sta      <VIA_shift_reg
                    stb      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_startDraw_d_y0                                         ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift light on
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    ldd      #$00FF
                    stb      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
SM_lastDraw_rts                                           ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift to e0, dies slowly
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    ldd      #$40E0
                    stb      <VIA_shift_reg
SM_rts                                                    ;#isfunction
                    ldb      gameScale
                    stB      VIA_t1_cnt_lo
                    LDA      #$CC
                    STA      VIA_cntl                     ;/BLANK low and /ZERO low
; and ensures integrators are clean (good positioning!)
                    ldd      #0
                    std      <VIA_port_b
                    rts

SM_lastDraw_rts_stay                                      ;#isfunction
                    sta      <VIA_port_a                  ; leaves with shift to e0, dies slowly
                    clra
                    sta      <VIA_port_b
                    inca
                    std      <VIA_port_b
                    ldd      #$40E0
                    clr      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
SM_FlagWait3                                              ;#isfunction
                    bita     <VIA_int_flags
                    beq      SM_FlagWait3
                    stb      <VIA_shift_reg
                    rts

SM_lastDraw_rts2                                          ;#isfunction
                    ldb      gameScale
                    lda      #$f0
                    stB      VIA_t1_cnt_lo
                    sta      <VIA_shift_reg
SM_rts2                                                   ;#isfunction
                    LDa      #$CC
                    STA      VIA_cntl                     ;/BLANK low and /ZERO low
; and ensures integrators are clean (good positioning!)
                    ldd      #0
                    std      <VIA_port_b
                    puls     d,pc                         ; (D = y,x, pc = next object)
SM_draw_only_XChanges_double                              ;#isfunction
                    stb      <VIA_port_a
                    ldd      #$00FF
                    stb      <VIA_shift_reg
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    ;brn      0
                    NOP 
                    ;NOP
                    pulu     a,b
                    clr      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     pc
; y is inherently known to be == old_y, y was set to 0 by generator
SM_draw_only_XChanges                                     ;#isfunction
                    stb      <VIA_port_a
                    sta      <VIA_t1_cnt_hi
                    jsr      ADD_NOPS
                    pulu     a,b,pc
calibrationZero                                           ;#isfunction
                    ldb      #$CC
                    stb      <VIA_cntl
                    ldd      #$8100
                    std      <VIA_port_b
                    dec      <VIA_port_b
                    ldb      >calibrationValue
                    lda      #$82
                    std      <VIA_port_b
                    ldd      #$83FF
                    stb      <VIA_port_a
                    sta      <VIA_port_b
                    rts

;***************************************************************************
; DATA SECTION
;***************************************************************************
_SM_vextreme_logo

                    fdb ScenList_0 ; list of all single vectorlists in this
                    fdb ScenList_1
                    fdb ScenList_2
                    fdb ScenList_3
                    fdb ScenList_4
                    fdb ScenList_5
                    fdb ScenList_6
                    fdb ScenList_7
                    fdb 0

ScenList_0
                    fcb  $49, -$51
                    fdb  SM_continue_d_double
                    fcb  $49, -$51
                    fdb  SM_continue_d_double
                    fcb  $29,  $28
                    fdb  SM_startDraw_d
                    fcb  $00, -$63
                    fdb  SM_continue_d_y0
                    fcb -$4F,  $48
                    fdb  SM_continue_d2
                    fcb  $4F,  $42
                    fcb  $00, -$19
                    fdb  SM_continue_d_y0
                    fcb -$2F, -$2E
                    fdb  SM_continue_d2
                    fcb  $08, -$08
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_1
                    fcb  $7E, -$78
                    fdb  SM_continue_d_double
                    fcb  $4F,  $00
                    fdb  SM_startDraw_d_x0
                    fcb  $00,  $55
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$1C, -$17
                    fdb  SM_continue_d
                    fcb  $00, -$24
                    fdb  SM_continue_d_y0
                    fcb -$03,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $24
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$13, -$12
                    fdb  SM_continue_d
                    fcb  $00, -$12
                    fdb  SM_continue_d_y0
                    fcb -$05,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $28
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$17,  $13
                    fdb  SM_continue_d
                    fcb  $00, -$55
                    fdb  SM_continue_d_y0
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_2
                    fcb  $7E, -$4B
                    fdb  SM_continue_d_double
                    fcb  $28,  $24
                    fdb  SM_startDraw_d
                    fcb  $27, -$24
                    fdb  SM_continue_d
                    fcb  $00,  $3B
                    fdb  SM_continue_d_y0
                    fcb -$1C, -$12
                    fdb  SM_continue_d3
                    fcb -$03,  $04
                    fcb  $1F,  $1B
                    fcb  $00,  $1A
                    fdb  SM_continue_d_y0
                    fcb -$27, -$1E
                    fdb  SM_continue_d2
                    fcb -$28,  $1E
                    fcb  $00, -$39
                    fdb  SM_continue_d_y0
                    fcb  $17,  $16
                    fdb  SM_continue_d3
                    fcb  $05, -$09
                    fcb -$1C, -$1B
                    fcb  $00, -$1B
                    fdb  SM_continue_d_y0
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_3
                    fcb  $7E, -$09
                    fdb  SM_continue_d_double
                    fcb  $33,  $00
                    fdb  SM_startDraw_d_x0
                    fcb -$0B, -$1B
                    fdb  SM_continue_d
                    fcb  $27,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $4E
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$27,  $00
                    fdb  SM_continue_d_x0
                    fcb  $0B, -$15
                    fdb  SM_continue_d
                    fcb -$33,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00, -$1D
                    fdb  SM_continue_newY_eq_oldX
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_4
                    fcb  $7E,  $3A
                    fdb  SM_continue_d_double
                    fcb  $13, -$0E
                    fdb  SM_startDraw_d
                    fcb  $0C,  $0E
                    fdb  SM_continue_d
                    fcb  $1C,  $00
                    fdb  SM_continue_d_x0
                    fcb  $13, -$0E
                    fdb  SM_continue_d
                    fcb  $00, -$43
                    fdb  SM_continue_d_y0
                    fcb -$4F,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $28
                    fdb  SM_continue_newY_eq_oldX
                    fcb  $1C, -$16
                    fdb  SM_continue_d
                    fcb  $00,  $09
                    fdb  SM_continue_d_y0
                    fcb -$1C,  $16
                    fdb  SM_continue_d
                    fcb  $00,  $20
                    fdb  SM_continue_d_y0
                    fcb  $2F, -$17
                    fdb  SM_startMove_d
                    fcb  $03, -$04
                    fdb  SM_startDraw_d
                    fcb  $00, -$07
                    fdb  SM_continue_d_y0
                    fcb -$03, -$06
                    fdb  SM_continue_d
                    fcb -$07,  $00
                    fdb  SM_continue_d_x0
                    fcb -$03,  $06
                    fdb  SM_continue_d
                    fcb  $00,  $07
                    fdb  SM_continue_d_y0
                    fcb  $03,  $04
                    fdb  SM_continue_d
                    fcb  $07,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_5
                    fcb  $7E,  $69
                    fdb  SM_continue_d_double
                    fcb  $17, -$11
                    fdb  SM_startDraw_d
                    fcb  $00, -$28
                    fdb  SM_continue_d_y0
                    fcb  $05,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $11
                    fdb  SM_continue_newY_eq_oldX
                    fcb  $13,  $0D
                    fdb  SM_continue_d
                    fcb  $00, -$1E
                    fdb  SM_continue_d_y0
                    fcb  $03,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $23
                    fdb  SM_continue_newY_eq_oldX
                    fcb  $1C,  $16
                    fdb  SM_continue_d
                    fcb  $00, -$54
                    fdb  SM_continue_d_y0
                    fcb -$4F,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $54
                    fdb  SM_continue_newY_eq_oldX
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_6
                    fcb  $7E,  $69
                    fdb  SM_continue_d_double
                    fcb  $4F,  $0F
                    fdb  SM_startDraw_d
                    fcb  $00,  $0C
                    fdb  SM_continue_d_y0
                    fcb -$1C,  $1C
                    fdb  SM_continue_d
                    fcb  $00,  $16
                    fdb  SM_continue_newY_eq_oldX
                    fcb  $00,  $0D
                    fdb  SM_continue_d_y0
                    fcb -$4F,  $0E
                    fdb  SM_continue_d
                    fcb  $00, -$20
                    fdb  SM_continue_d_y0
                    fcb  $20,  $05
                    fdb  SM_continue_d
                    fcb  $00, -$09
                    fdb  SM_continue_d_y0
                    fcb -$20, -$05
                    fdb  SM_continue_d
                    fcb  $00, -$1B
                    fdb  SM_continue_d_y0
                    fcb  $20,  $05
                    fdb  SM_continue_d
                    fcb  $00, -$09
                    fdb  SM_continue_d_y0
                    fcb -$20, -$05
                    fdb  SM_continue_d
                    fcb  $00, -$1B
                    fdb  SM_continue_d_y0
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
ScenList_7
                    fcb  $3F,  $4F
                    fdb  SM_continue_d_double
                    fcb  $3F,  $4F
                    fdb  SM_continue_d_double
                    fcb  $4F,  $00
                    fdb  SM_startDraw_d_x0
                    fcb  $00,  $51
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$1C, -$16
                    fdb  SM_continue_d
                    fcb  $00, -$24
                    fdb  SM_continue_d_y0
                    fcb -$03,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $1F
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$13, -$0D
                    fdb  SM_continue_d
                    fcb  $00, -$12
                    fdb  SM_continue_d_y0
                    fcb -$05,  $00
                    fdb  SM_continue_d_x0
                    fcb  $00,  $28
                    fdb  SM_continue_newY_eq_oldX
                    fcb -$17,  $12
                    fdb  SM_continue_d
                    fcb  $00, -$51
                    fdb  SM_continue_d_y0
                    fcb  $00,  $00
                    fdb  SM_lastDraw_rts
