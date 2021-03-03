; this file is part of Release, written by Malban in 2017
;
; The draw routines are "special" in the way, that they don't use the
; shift register for switching the light on and off, but rather do
; that manually using the VIA_cntl register
;
; also the following routines never "WAIT" in a loop for the drawing to be finished.
; the routines are all specialized for a certain scale (range)
; All handling internally is done in respect to the scale that they should be used with
; (if used with another scale the output is not garanteed)
; The higher scale versiona for that purpose MUST contain some sort of wait - here I used NOPs
; The wait is in general so short, that nothingmuch useful could be done.
;
; Also note some use register Y some register U internally (destroy!).
; It all depended where I used these routines, which register I had to keep save.
;
; Some functions that are only usied within "object" do not RETURN in the usual way
; but call the next object by pulling the U register with the next object data.
;

***********************************************************  
; draw bigger vlists than above one - scale 16
; input list in X
; destroys y
; 0 move
; negative use as shift
; positive end
myDraw_VL_mode2                             ; #isfunction 
  LDA                  #$80                 ;
  STA                  <VIA_aux_cntl        ; Shift reg mode = 000 free disable, T1 PB7 enabled 

next_line2 
  lda                  ,x+                  ;
  beq                  move_vl2             ;
  bmi                  draw_vl2             ;

done_vl2 
; VIA values back to default
  LDD                  #$98ce               ; [6] check 
  M_NOP_4                                   ;
  STa                  <VIA_aux_cntl        ; [4] Shift reg mode = 000 free disable, T1 PB7 enabled 
  STB                  <VIA_cntl            ; [4] $CC /BLANK low and /ZERO low 
  rts                  

draw_vl2 
  ldd                  ,x++ 
  ldy                  #$ce00 
  STA                  <VIA_port_a          ; (2) [4] Send Y to A/D 
  sty                  <VIA_cntl            ;
  CLR                  <VIA_port_b          ; (2) [4] enable mux, thus y integrators are set to Y 
  nop                                       ;
  INC                  <VIA_port_b          ; [6] Disable mux 
  STB                  <VIA_port_a          ; [6] Send X to A/D 
  ldb                  #$ee                 ; light ON, ZERO OFF 
  CLR                  <VIA_t1_cnt_hi       ; [4] enable timer 1 (6 cycles) 
; light on
  stb                  <VIA_cntl            ; [4] ZERO disabled, and BLANK disabled 
; 24 cycles -> light off
  lda                  ,x+                  ; [7] 
  bmi                  draw_vl2             ; [3] 
  beq                  move_vl2             ; [3] 
  bra                  done_vl2             ; [3] 

move_vl2 
  ldd                  ,x++ 
  ldy                  #$ce00 
  STA                  <VIA_port_a          ; (2) [4] Send Y to A/D 
  sty                  <VIA_cntl            ;
  CLR                  <VIA_port_b          ; (2) [4] enable mux, thus y integrators are set to Y 
  nop                                       ;
  INC                  <VIA_port_b          ; [6] Disable mux 
  STB                  <VIA_port_a          ; [6] Send X to A/D 
  CLR                  <VIA_t1_cnt_hi       ; [4] enable timer 1 (6 cycles) 
; light on
; stb                  <VIA_cntl            ; [4] ZERO disabled, and BLANK disabled 
; 24 cycles -> light off
  lda                  ,x+                  ; [7] 
  bmi                  draw_vl2             ; [3] 
  beq                  move_vl2             ; [3] 
  bra                  done_vl2             ; [3] 

***********************************************************  
; draw bigger vlists than above one - scale 24
; input list in X
; destroys u
; 0 move
; negative use as shift
; positive end
myDraw_VL_mode3                             ; #isfunction 
  LDA                  #$80                 ;
  STA                  <VIA_aux_cntl        ; Shift reg mode = 000 free disable, T1 PB7 enabled 

next_line3 
  lda                  ,x+                  ;
  beq                  move_vl3             ;
  bmi                  draw_vl3             ;

done_vl3 
; VIA values back to default
  M_NOP_8
  LDD                 #$98ce                ; [6] check 
  STA                 <VIA_aux_cntl         ; [4] Shift reg mode = 000 free disable, T1 PB7 enabled 
; nop 2 
  STB                 <VIA_cntl             ; [4] $CC /BLANK low and /ZERO low 
  rts      

draw_vl3 
  ldd                  ,x++ 
  ldu                  #$ce00               ; $ce is light off 
  STA                  <VIA_port_a          ; (2) [4] Send Y to A/D 
  M_NOP_2                                   ;
  bra                  wait_bra_1           ;

wait_bra_1 
  stu                  <VIA_cntl            ;
  CLR                  <VIA_port_b          ; (2) [4] enable mux, thus y integrators are set to Y 
  nop                                       ;
  INC                  <VIA_port_b          ; [6] Disable mux 
  STB                  <VIA_port_a          ; [6] Send X to A/D 
  ldb                  #$ee                 ; light ON, ZERO OFF 
  CLR                  <VIA_t1_cnt_hi       ; [4] enable timer 1 (6 cycles) 
; light on
  stb                  <VIA_cntl            ; [4] ZERO disabled, and BLANK disabled 
; 24 cycles -> light off
  lda                  ,x+                  ; [7] 
  bmi                  draw_vl3             ; [3] 
  beq                  move_vl3             ; [3] 
  bra                  done_vl3             ; [3] 

move_vl3 
  ldd                  ,x++                 ;
  ldu                  #$ce00               ;
  STA                  <VIA_port_a          ; (2) [4] Send Y to A/D 
; nop 4
  M_NOP_2                                   ;
  bra                  wait_bra_2           ;

wait_bra_2 
  stu                  <VIA_cntl 
  CLR                  <VIA_port_b          ; (2) [4] enable mux, thus y integrators are set to Y 
  nop                                       ;
  INC                  <VIA_port_b          ; [6] Disable mux 
  STB                  <VIA_port_a          ; [6] Send X to A/D 
; nop 4
  CLR                  <VIA_t1_cnt_hi       ; [4] enable timer 1 (6 cycles) 
; light on
  M_NOP_4
; stb <VIA_cntl ; [4] ZERO disabled, and BLANK disabled 
; 24 cycles -> light off
  lda                  ,x+                  ; [7] 
  bmi                  draw_vl3             ; [3] 
  beq                  move_vl3             ; [3] 
  bra                  done_vl3             ; [3] 
