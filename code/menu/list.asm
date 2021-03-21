; Copyright (C) 2021 Vasily Kiniv <vasily.kiniv at gmail.com>
;
; This library is free software: you can redistribute it and/or modify
; it under the terms of the GNU Lesser General Public License as published b
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU Lesser General Public License for more details.
; You should have received a copy of the GNU Lesser General Public License
; along with this library.  If not, see <http://www.gnu.org/licenses/>.


;*******************************************************************************
; CalcScrollOffset
;
; Calculate scrollbar thumb offset.
; IN:
;   B - page
;   m_list_size
; OUT:
;   B - scrollbar thumb offset
; DESTROYS:
;   A
;   m_tmp_d
;   m_tmp_a
;*******************************************************************************
CalcScrollOffset
  lda                  #0                   ; A = 0
  std                  m_tmp_d              ; tmp_d = D (page)
  sta                  m_tmp_a              ; tmp_a = A (0)
  ldb                  m_list_size          ; B = list_size
  cmpb                 m_max_lines          ; if B (list_size) <= max_lines
  lble                 cso_zero_done        ; | scroll_offset = 0, exit
  ldb                  m_tmp_d+1            ; B = tmp_d[1] (page)
  cmpb                 #8                   ; if B (page) <= 8
  ble                  cso_div1             ; | disable extra precision
  inc                  m_tmp_a              ; tmp_a += 1 (enable extra precision)

; (SCROLLBAR_TRACK_H - m_sbt_height) / ((list_size - MENU_ITEMS_MAX) / page)
cso_div1
  ldb                  m_list_size          ; B = list_size
  subb                 m_max_lines          ; B -= max_lines
  M_LSLD                                    ; D (list_size) *= 8
  M_LSLD                                    ; |
  M_LSLD                                    ; |
  tst                  m_tmp_a              ; if tmp_a (extra precision) == 0
  beq                  cso_div2             ; | skip following 2 lines
  M_LSLD                                    ; D *= 4 (extra precision)
  M_LSLD                                    ; |

cso_div2
  M_DIV_D_TO_B         m_tmp_d              ; B = D / tmp_d (page)
  std                  m_tmp_d              ; tmp_d = D (keep result in m_tmp_d)
  clra                                      ; A = 0
  ldb                  #SCROLLBAR_TRACK_H   ; B = SCROLLBAR_TRACK_H
  subb                 m_sbt_height         ; B -= scroll_thumb (offset)
  M_LSLD                                    ; D *= 8
  M_LSLD                                    ; |
  M_LSLD                                    ; |
  tst                  m_tmp_a              ; if tmp_a == 0
  beq                  cso_div3             ; | skip following 2 lines
  M_LSLD                                    ; D *= 4 (extra precision)
  M_LSLD                                    ; |

cso_div3
  M_DIV_D_TO_B         m_tmp_d              ; B = D / tmp_d(previous result)

cso_done
  rts                                       ; exit

cso_zero_done
  ldb                  #0                   ; B = 0
  rts                                       ; exit

;*******************************************************************************
; CalcScrollOffsetStep
;
; Calculate scrollbar thumb offset animation step.
; IN:
;   B - next scroll offset
; OUT:
;   m_sbt_offset_step  - scrollbar thumb offset step used in animation
;*******************************************************************************
CalcScrollOffsetStep
  subb                 m_sbt_offset         ; B -= scroll_offset
  bpl                  res_is_positive      ; if B >= 0
  negb                                      ; | inverse B

res_is_positive
  cmpb                 #8                   ; if B >= 8
  bgt                  delta_is_large       ; | skip following 3 lines
  ldb                  #1                   ; B = 1
  stb                  m_sbt_offset_step    ; scroll_offset_st = B
  bra                  csos_done            ; exit

delta_is_large
  ldb                  #2                   ; B = 2
  stb                  m_sbt_offset_step    ; scroll_offset_st = B

csos_done
  rts                                       ; exit

;*******************************************************************************
; DrawVectorNumber
;
; Draw number from right to left.
; IN:
;   B             - 8bit number to print
;   m_move_vector - draw start position
;   MOVE_SCALE
;   SPACE_WIDTH
;   HSFNT
; DESTROYS: D, Y, U, m_tmp_a, m_tmp_d
;*******************************************************************************
DrawVectorNumber
  ; draw digit (number % 10)
  stb                  m_tmp_a              ; tmp_a = B (number)
  lda                  #10                  ; A = 10
  M_B_MOD_A_TO_B                            ; B = B % A
  M_VS_ITOA            #HSFNT               ; digit to char(glyph vectorlist addr into X)
  M_DRAW_VLIST         2,16,#MOVE_SCALE     ; draw vectorlist
  ; move draw position to the left
  ldd                  m_move_vector        ; D = m_move_vector
  subb                 #SPACE_WIDTH         ; B -= SPACE_WIDTH
  std                  m_move_vector        ; move_vector = D
  ; number / 10
  clra                                      ; D = 10
  ldb                  #10                  ; |
  std                  m_tmp_d              ; m_tmp_d (divisor) = 10
  ldb                  m_tmp_a              ; B = tmp_a(dividend)
  M_DIV_D_TO_B         m_tmp_d              ; D / tmp_d
  cmpb                 #0                   ; if B > 0
  lbhi                 DrawVectorNumber     ; draw next digit
  rts

;*******************************************************************************
; DrawListItem

; Draw string in U register
; IN:
;   m_cursor
;   m_curpos
;   MOVE_SCALE
;   HSFNT
;   HSFNT_FOLDER
; OUT:
;   m_char_counter
; DESTROYS:
;   m_tmp_a
;   m_tmp_b
;*******************************************************************************
DrawListItem
  clr                  m_char_counter       ; char_counter = 0
  clr                  m_tmp_a              ; m_tmp_a('is folder' marker) = 0
  ldb                  ,u                   ; B = U[0]
  cmpb                 # '<'                ; if B (char) != '<'
  bne                  draw_next_char       ; | draw char
  lda                  #1                   ; A = 1
  sta                  m_tmp_a              ; tmp_a('is folder' marker) = A
  ; shift folder label to the left to align with file labels
  ldd                  m_move_vector        ; D = move_vector
  subb                 #20                  ; B -= 24 (X axis)
  std                  m_move_vector        ; move_vector = D

draw_next_char
  lda                  #MOVE_SCALE          ; A = MOVE_SCALE
  M_SCALE_A                                 ; SCALE(A)
  ldd                  m_move_vector        ; D = move_vector
  addb                 #SPACE_WIDTH         ; B += SPACE_WIDTH (X axis)
  std                  m_move_vector        ; move_vector = D
  M_MOVE_TO_D_START                         ; MOVE(D)
  inc                  m_char_counter       ; char_counter++
  lda                  m_char_counter       ; A = m_char_counter
  cmpa                 #1                   ; if A == 1
  bne                  load_next_char       ; | skip check for '<', no need to replace any subsequent '<' chars with a folder icon
  ldb                  ,u+                  ; B = U++ (next char)
  lda                  m_tmp_a              ; if tmp_a(is folder) == 0
  beq                  is_not_folder        ; | skip following 5 lines
  ; additional spacing because folder icon is twice larger in width
  lda                  m_move_vector+1      ; A = move_vector[1] (X axis)
  adda                 #SPACE_WIDTH         ; A += SPACE_WIDTH
  sta                  m_move_vector+1      ; move_vector[1] (X axis) = A

draw_folder
  ldx                  #HSFNT_FOLDER        ; X = pointer to folder icon vectorlist
  lbra                 cont_vp2             ; draw and finish moveto

load_next_char
  LDB                  ,u+                  ; B = U++ (next char)

is_not_folder
  cmpb                 #W_NUM               ; if B != control char(num input)
  lbne                 is_not_num_input     ; | check for another control char

; ------------------------------------------------------------------------ W_NUM
  M_MOVE_END_ZERO                           ; end moveto
  ; set start point at the right of the screen, following routines will draw from right to left
  ldd                  m_move_vector        ; D = move_vector
  ldb             #LIST_WIDTH - SPACE_WIDTH ; B (X axis) = right end - space
  std                  m_move_vector        ; move_vector = D
  ; draw '>'
  ldx                  #HSFNT_ARR_RIGHT     ; X = pointer to vectorlist
  M_DRAW_VLIST         2, 16, #MOVE_SCALE   ; draw '>' at move_vector using mode2, scale 16
  ldd                  m_move_vector        ; D = move_vector
  subb                 #SPACE_WIDTH         ; B -= SPACE_WIDTH
  std                  m_move_vector        ; move_vector = D
  ; draw number
  ldx                  2,u                  ; X = U[2] (3rd arg - pointer to value)
  ldb                  ,x                   ; B = X[0] (value itself)
  jsr                  DrawVectorNumber     ; draw number from right to left
  ; draw '<'
  ldx                  #HSFNT_ARR_LEFT      ; X = pointer to vectorlist
  M_DRAW_VLIST         2, 16, #MOVE_SCALE   ; draw '<' at move_vector using mode2, scale 16
  ; setup button handler if highlighted
  lda                  m_cursor             ; if cursor != curpos (not highlighted)
  cmpa                 m_curpos             ; |
  bne                  input_num_draw_done  ; | exit
  stu                  m_input_arg_ptr      ; U = pointer to the start of args sequence
  ldd                  #input_num_bh        ; D = address of num button handler
  std                  m_input_sr_ptr       ; input_sr_ptr = D

input_num_draw_done
  rts                                       ; exit

input_num_bh
  ldx                  m_input_arg_ptr      ; X = pointer to the args
  ldy                  2,x                  ; Y = X[2] (3rd arg - pointer to value)
  ldb                  ,y                   ; B = Y[0] (value itself)
  cmpa                 #2                   ; if button == 2
  beq                  input_num_bh_dec     ; | decrease
  cmpa                 #3                   ; if button == 3
  beq                  input_num_bh_inc     ; | increase
  cmpa                 #BTN_LEFT            ; if button == left
  beq                  input_num_bh_dec     ; | decrease
  cmpa                 #BTN_RIGHT           ; if button == right
  beq                  input_num_bh_inc     ; | increase
  bra                  input_num_bh_store   ; done

input_num_bh_inc
  cmpb                 1, x                 ; if B (value) >= X[1] (max)
  bhs                  input_num_bh_store   ; | done
  addb                 #1                   ; increase
  bra                  input_num_bh_store   ; done

input_num_bh_dec
  cmpb                 ,x                   ; if B (value) <= X[0] (min)
  bls                  input_num_bh_store   ; | done
  subb                 #1                   ; decrease

input_num_bh_store
  IF                   SETTINGS_IN_RAM      ; STORE IN RAM
  stb                  ,y                   ; | store changed value at Y (pointer to input value)
  ELSE                                      ; STORE IN ROM
  stb                  m_tmp_b              ; | load value into m_tmp_b for M_JSR_RPC_STR
  tfr                  y, d                 ; | load address of value in ROM to D for M_JSR_RPC_STR
  M_JSR_RPC_STR        m_tmp_b              ; | call storeToRom(), store m_tmp_b in
  ENDIF                                     ;
  ; call external handler if it's not zero
  ldy                  m_input_arg_ptr      ; Y = pointer to input argument
  ldd                  4,y                  ; if Y[4] == 0
  cmpd                 #0                   ; |
  beq                  input_num_bh_done    ; | exit
  jsr                  [4,y]                ; call handler

input_num_bh_done
  rts                                       ; exit
; END -------------------------------------------------------------------- W_NUM

is_not_num_input
  cmpb                 #W_LINK              ; if B != control char(link input)
  bne                  is_not_input_link    ; | check for another control char

; ----------------------------------------------------------------------- W_LINK
  M_MOVE_END_ZERO                           ; end moveto

input_link_draw
  ; setup button handler if highlighted
  lda                  m_cursor             ; if cursor != curpos (not highlighted)
  cmpa                 m_curpos             ; |
  bne                  input_link_draw_done ; | exit
  ldd                  ,u                   ; D = U[0] (pointer to a submenu)
  std                  m_input_arg_ptr      ; input_arg_ptr = D
  ldd                  #input_link_bh       ; D = pointer to the button handler
  std                  m_input_sr_ptr       ; input_sr_ptr = D

input_link_draw_done
  rts                                       ; exit

input_link_bh
  cmpa                 #4                   ; if a != 4
  bne                  input_link_bh_done   ; | exit
  ldd                  m_input_arg_ptr      ; D = another list pointer stored in input arguments
  std                  m_list_ptr           ; list_ptr = D
  jsr                  init_page            ; initialize page
  M_IS_FILELIST                             ; if up is NOT filelist
  bne                  input_link_bh_done   ; | done
  M_JSR_RPC            #18                  ; RPC call to settingsSave()

input_link_bh_done
  rts                                       ; exit
; END ------------------------------------------------------------------- W_LINK

is_not_input_link
  cmpb                 #W_SELECT            ; if B != control char(select input)
  lbne                 is_not_input         ; | check for another control char
; --------------------------------------------------------------------- W_SELECT
  M_MOVE_END_ZERO                           ; end moveto

  ; draw '>'
  ldd                  m_move_vector        ; D = move_vector
  ldb             #LIST_WIDTH - SPACE_WIDTH ; B = right end - space
  std                  m_move_vector        ; move_vector = D
  ldx                  #HSFNT_ARR_RIGHT     ; X = pointer to vectorlist
  M_DRAW_VLIST         2, 16, #MOVE_SCALE   ; draw '>' at move_vector using mode2, scale 16
  ; count chars in option string
  lda                  [,u]                 ; A = *U[0] (selected option index)
  adda                 #1                   ; A += 1 (options starts at index 1, not 0)
  ldb                  #2                   ; B = 2
  mul                                       ; D = A * B (address is 16bit)
  ldx                  b,u                  ; X = U[B] (option string address)
  clra                                      ; A = 0

input_sel_ch_cnt_loop
  inca                                      ; A++
  ldb                  ,x+                  ; B = X[0], X++
  cmpb                 #$80                 ; if B != 0x80 (end of option string)
  bne                  input_sel_ch_cnt_loop; | repeat
  adda                 #1                   ; A += 1 (account for '>' char)
  ; shift left SPACE_WIDTH * chars_num
  ldb                  #SPACE_WIDTH         ; B = SPACE_WIDTH
  mul                                       ; D = A * B
  negb                                      ; B -= B
  addb                 #LIST_WIDTH          ; B += LIST_WIDTH
  stb                  m_move_vector+1      ; move_vector[1] (X axis) = B
  ; draw '<'
  ldx                  #HSFNT_ARR_LEFT      ; X = pointer to vectorlist
  M_DRAW_VLIST         2, 16, #MOVE_SCALE   ; draw '<' at move_vector using mode2, scale 16
  ; setup button handler if highlighted
  lda                  m_cursor             ; if cursor != curpos (not highlighted)
  cmpa                 m_curpos             ; |
  bne                  input_select_load_str; | print option string
  stu                  m_input_arg_ptr      ; U = pointer to the start of args sequence
  ldd                  #input_select_bh     ; D = address of num button handler
  std                  m_input_sr_ptr       ; input_sr_ptr = D

input_select_load_str
  lda                  [,u]                 ; A = *U[0] (selected option)
  adda                 #1                   ; A += 1 (options starts at index 1, not 0)
  ldb                  #2                   ; B = 2
  mul                                       ; D = A * B (address is 16bit)
  ldu                  b,u                  ; U = U[B] (option string address)
  ; proceed to printing option string
  lbra                 draw_next_char       ; proceed to drawing option string
  rts                                       ; exit

input_select_bh
  ldx                  [m_input_arg_ptr]    ; X = pointer to value (option index)
  ldy                  m_input_arg_ptr      ; Y = pointer to input arguments
  leay                 4, y                 ; Y += forward past the first option
  ; find last option index, so we have max value to compare
  ldb                  #-1                  ; B(counter) = -1, because we need last index, not size

input_sel_op_loop
  incb                                      ; B++
  tst                  ,y++                 ; if Y != 0(end of list)
  bne                  input_sel_op_loop    ; | advance pointer and test again
  stb                  m_tmp_b              ; keep last index in tmp_b
  ; handle button
  ldb                  ,x                   ; load current value
  cmpa                 #2                   ; if A(button) == 2
  beq                  input_select_bh_dec  ; | decrease
  cmpa                 #3                   ; if A == 3
  beq                  input_select_bh_inc  ; | increase
  cmpa                 #BTN_LEFT            ; if A == LEFT
  beq                  input_select_bh_dec  ; | decrease
  cmpa                 #BTN_RIGHT           ; if A == RIGHT
  beq                  input_select_bh_inc  ; | increase
  bra                  input_select_bh_str  ; done, print option string

input_select_bh_inc
  cmpb                 m_tmp_b              ; if B(value) >= max
  bhs                  input_select_bh_str  ; | done, print option string
  addb                 #1                   ; B += 1 (increase option index)
  bra                  input_select_bh_str  ; done

input_select_bh_dec
  cmpb                 #0                   ; if B(value) <= min
  bls                  input_select_bh_str  ; | done, print option string
  subb                 #1                   ; B -= 1 (decrease option index)

input_select_bh_str
  IF                   SETTINGS_IN_RAM      ; STORE IN RAM
  stb                  ,x                   ; store value at X(pointer to input value in RAM)
  ELSE                                      ; STORE IN ROM
  stb                  m_tmp_b              ; load value into m_tmp_b for M_JSR_RPC_STR
  tfr                  x, d                 ; load address of value in ROM to D for M_JSR_RPC_STR
  M_JSR_RPC_STR        m_tmp_b              ; call storeToRom()
  ENDIF                                     ;
  rts                                       ; exit
; END ----------------------------------------------------------------- W_SELECT

is_not_input
  cmpb                 # ' '-1              ; if B(char) > space
  bgt                  is_not_lower         ; | compare next char
  bra                  draw_not_supported   ; draw placeholder for unsupported char

is_not_lower
  CMPB                 # '~'+1              ; if B(char) < tilda
  blt                  draw_actual_char     ; | draw
  bra                  draw_not_supported   ; draw placeholder for unsupported char

draw_not_supported
  ldx                  #HSFNT_1             ; X = pointer to vectorlist('!')
  bra                  cont_vp2             ; draw vectorlist

draw_actual_char
  lda                  #2                   ; A = 2 (multiplier)
  SUBB                 #' '                 ; B -= 32 (subtract space, so ASCII code has 0 offset)
  mul                                       ; D = A * B (address is 16bit)
  ldx                  #HSFNT               ; X = pointer to the start of the ASCII font table
  LDX                  d,x                  ; X = X[B] (glyph vectorlist)

cont_vp2
  lda                  #16                  ; A = 16 (draw scale)
  M_SCALE_A                                 ; SCALE(A)
  M_MOVE_END                                ; end moveto
  pshs                 u                    ; save U register
  jsr                  myDraw_VL_mode2      ; draw using mode2, scale 16
  puls                 u                    ; restore U register
  M_ZERO_VECTOR_BEAM                         ; draw each letter with a move from zero, more stable
  lda                  m_tmp_a              ; if tmp_a == 0 (is not a folder)
  beq                  cont_vp2_not_folder  ; | skip following 3 lines
  ldb                  ,u                   ; else
  cmpb                 #'>'                 ; if B(char) == '>' (end of folder label)
  beq                  cont_vp2_exit        ; | exit

cont_vp2_not_folder
  lda                  m_char_counter       ; if m_char_counter >= m_trim_at
  cmpa                 m_trim_at            ; |
  bge                  cont_vp2_trim        ; trim string
  LDB                  ,u                   ; else
  lbpl                 draw_next_char       ; draw next char

cont_vp2_exit
  rts                                       ; exit

cont_vp2_trim
  lda                  m_char_counter       ; A = char_counter
  cmpa                 m_trim_at            ; if A > trim_at
  bgt                  cont_vp2_exit        ; | skip '...' (already done)
  LDB                  ,u                   ; B = U[0](next char)
  cmpb                 #$80                 ; if B == $80
  beq                  cont_vp2_exit        ; | skip '...' (string length == trim_at)
  lda                  #MOVE_SCALE          ; A = MOVE_SCALE
  M_SCALE_A                                 ; SCALE(A)
  ldd                  m_move_vector        ; D = move_vector
  addb                 #SPACE_WIDTH         ; B += SPACE_WIDTH (X axis)
  std                  m_move_vector        ; move_vector = D
  M_MOVE_TO_D_START                         ; start movement to D
  ldx                  #HSFNT_69            ; X = vectorlist ('...' char)
  inc                  m_char_counter       ; char_counter++ (prevent another '...')
  bra                  cont_vp2             ; draw '...'
