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
; M_LSLD
;
; Shift register D one position to the left.
;*******************************************************************************
M_LSLD                 macro
  lsla                                      ; LSL A
  lslb                                      ; LSL B
  bcc                  >                    ; if no carry, done
  ora                  #1                   ; otherwise overflow from B to 0bit of A
!
  endm

;*******************************************************************************
; M_VS_ITOA #[font_table]
;
; Integer to char.
; Load vectorlist address into X for corresponding digit in ASCII font table.
; IN:
;   1 - font table address
;   m_input_sr_ptr - pointer to input handler
; OUT:
;   X - pointer to vectorlist for digit
; DESTROYS: B
;*******************************************************************************
M_VS_ITOA              macro
  addb                 #16                  ; add 16, so ASCII code has 0 offset
  lslb                                      ; * 2 (address is 16bit)
  ldx                  #\1                  ; and add the abc (table of vector list address of the ASCII chars)
  ldx                  b,x                  ; load glyph vectorlist into X
  endm

;*******************************************************************************
; M_B_MOD_A_TO_B
;
; Modulo: B = B % A
; IN:
;   A - divisor
;   B - divident
; OUT:
;   B - modulo
; DESTROYS: m_divide_tmp
;*******************************************************************************
M_B_MOD_A_TO_B         macro
  sta                  m_divide_tmp         ; tmp = divisor
  cmpb                 m_divide_tmp         ; if B < tmp(divisor)
  blo                  1F                   ; | done
! subb                 m_divide_tmp         ; B -= divisor
  cmpb                 m_divide_tmp         ; if B >= tmp(divisor)
  bhs                  <                    ; | repeat
1
                    endm

;*******************************************************************************
; M_IS_FILELIST
;
; Compare current list pointer with #filedata
; IN:
;   filedata
;   m_list_ptr
; OUT:
;   CC
; DESTROYS: D
;*******************************************************************************
M_IS_FILELIST          macro
  ldd                  #filedata            ; D = filelist pointer
  cmpd                 m_list_ptr           ; if list_ptr == D
  endm

;*******************************************************************************
; M_JSR_IH #[button_id]
;
; Call input handler.
; IN:
;   1 - button ID
;   m_input_sr_ptr - pointer to input handler
; DESTROYS: D
;*******************************************************************************
M_JSR_IH               macro
  ldd                  m_input_sr_ptr       ; if pointer == 0
  beq                  >                    ; | done
  lda                  #\1                  ; A = button ID
  jsr                  [m_input_sr_ptr]     ; execute handler
!
  endm

;*******************************************************************************
; M_JSR_RPC #[rpc_id]
;
; Execute remote procedure on STM as a subroutine.
; IN:
;   1 - remote procedure ID
; DESTROYS: A, X
;*******************************************************************************
M_JSR_RPC              macro
  lda                  #\1                  ; load macro argument(RPC ID)
  ldx                  #9F                  ; set return address to one line after this macro
  jmp                  m_rpcfn2             ; execute
9
  endm

;*******************************************************************************
; M_JSR_RPC_STR [value]
;
; Store one byte into ROM memory at specified address using RPC call to STM.
; IN:
;   1 - value
;   D - address
;   RPC_ARG_ADDR
; DESTROYS: A
;*******************************************************************************
M_JSR_RPC_STR          macro
  sta                  RPC_ARG_ADDR + $f0   ; ROM address high byte
  stb                  RPC_ARG_ADDR + $f1   ; ROM address low byte
  lda                  \1                   ; load value
  sta                  RPC_ARG_ADDR + $f2   ; store value in parmRam
  M_JSR_RPC            #17                  ; RPC call to storeToRom()
  endm

;*******************************************************************************
; NOP N for asm6809.
; Note that each NOP takes two cycles, so M_NOP_2 will take 4.
;*******************************************************************************
M_NOP_2                macro
  nop
  nop
  endm

M_NOP_4                macro
  nop
  nop
  nop
  nop
  endm

M_NOP_8                macro
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  endm

;*******************************************************************************
; M_DRAW_VLIST #[mode], #[draw_scale], #[move_scale]
;
; Move beam to vector stored in D and draw vectorlist in X using Malban's
; optimized routines.
; IN:
;   1             - mode (2,3 or 4)
;   2             - draw scale (16, 24)
;   3             - move scale (0-128)
;   X             - pointer to vectorlist
;   m_move_vector - draw at position
; DESTROYS: D, Y, U
;*******************************************************************************
M_DRAW_VLIST           macro
  lda                  #\3                  ; A = move_scale
  M_SCALE_A                                 ; SCALE(A)
  ldd                  m_move_vector        ; D = move_vector
  M_MOVE_TO_D_START                         ; MOVE(D)
  lda                  #\2                  ; A = draw_scale
  M_SCALE_A                                 ; SCALE(A)
  M_MOVE_END                                ; end move
  pshs                 u                    ; save U register
  jsr                  myDraw_VL_mode\1     ; draw
  puls                 u                    ; restore U register
  M_ZERO_VECTOR_BEAM                        ; zero beam
  endm

;*******************************************************************************
; M_MEMCPY #[destination], #[source_start], #[source_end]
;
; Copy chunk of memory.
; IN:
;   1 - destination address
;   2 - source start address
;   3 - source end address
; DESTROYS: A, X, Y
;*******************************************************************************
M_MEMCPY               macro
  ldx                  #\2                  ; source
  ldy                  #\1                  ; destination
! ; copy loop
  lda                  ,x+                  ; load from source
  sta                  ,y+                  ; store to destination
  cmpx                 #\3                  ; if X != source
  bne                  <                    ; | loop
  endm


;*******************************************************************************
; M_MEM_COPY_ALL_RPC_2_RAM
;
; Copy all RPCs into RAM.
;
; DESTROYS: A, X, Y
;*******************************************************************************
M_COPY_ALL_RPC_2_RAM   macro
  M_MEMCPY             #m_rpcfn1, #rpcfndat1, #rpcfndatend1
  M_MEMCPY             #m_rpcfn2, #rpcfndat2, #rpcfndatend2
  M_MEMCPY             #m_rpcfn3, #rpcfndat3, #rpcfndatend3
  endm

;*******************************************************************************
; M_RESET_IDLE
;
; Clear m_idle_minutes and Vec_Loop_Count.
;*******************************************************************************
M_RESET_IDLE           macro
  clr                  m_idle_minutes       ; clear minutes counter
  clr                  Vec_Loop_Count       ; clear 5s unit counter
  endm



; Copyright (c) 2017 Malban
; The following lines of code is part of Release.
; Modified by Vasily Kiniv in 2021.

;*******************************************************************************
; M_DIV_D_TO_B [divisor]
;
; Divide exact but slow. Sign is NOT handled.
; IN:
;   1 - divisor (in memory)
;   D - dividend
; OUT:
;   B - quotient
; DESTROYS: A, m_divide_tmp
;*******************************************************************************
M_DIV_D_TO_B           macro
  clr                  m_divide_tmp         ; tmp = 0
  tst                  \1 + 1               ; if lower byte is zero, we assume divisor is zero
  beq                  3F                   ; | divide_by_zero
  dec                  m_divide_tmp         ; tmp -= 1
  cmpd                 #0                   ; if D >= 0
  bpl                  >                    ; | divide_next
1 ; divide_next1
  inc                  m_divide_tmp         ; tmp += 1
  addd                 \1                   ; D += arg1
  bmi                  1B                   ; divide_next1
2 ; divide_by_zero1
  ldb                  m_divide_tmp         ; B = tmp
  negb                                      ; invert B
  bra                  4F                   ; divide_end
! ; divide_next
  inc                  m_divide_tmp         ; tmp += 1
  subd                 \1                   ; D -= arg1
  bpl                  <                    ; if >= 0 divide_next
3 ; divide_by_zero
  lda                  #0                   ; D = 0
  ldb                  m_divide_tmp         ; |
4 ;divide_end
  endm

;*******************************************************************************
; M_SCALE_A
;
; Set scale stored in register A.
; IN:
;   A - scale
;*******************************************************************************
M_SCALE_A              macro
  sta                  VIA_t1_cnt_lo        ; set move to time 1 lo (scaling)
  endm

;*******************************************************************************
; M_INTENSITY_A
;
; Set intensity stored in register A. Same as Intensity_a but saves a few cycles.
; IN:
;   A - intensity
; DESTROYS: D
;*******************************************************************************
M_INTENSITY_A          macro
  sta                  <VIA_port_a          ; store intensity in D/A
  ldd                  #$0504               ; A = 0x05, B = 0x04
  sta                  <VIA_port_b          ; mux disabled channel 2
  stb                  <VIA_port_b          ; mux enabled channel 2
  sta                  <VIA_port_b          ; turn off mux
  endm

;*******************************************************************************
; M_ZERO_VECTOR_BEAM
;
; Move beam to 0,0.
; DESTROYS: B
;*******************************************************************************
M_ZERO_VECTOR_BEAM     macro
  ldb                  #$CC                 ;
  stb                  VIA_cntl             ; ZERO = low, BLANK=low
  endm

;*******************************************************************************
; M_MOVE_TO_D_START
;
; Start movement to D, should be finished by M_MOVE_END.
; IN:
;   D - move vector
; DESTROYS: D
;*******************************************************************************
M_MOVE_TO_D_START      macro
  sta                  <VIA_port_a          ; VIA Y vector = A
  lda                  #$CE                 ; A = 0xCE: ZERO = high, BLANK=low
  sta                  <VIA_cntl            ; VIA control reg = A
  clra                                      ; A = 0
  sta                  <VIA_port_b          ; enable mux
  sta                  <VIA_shift_reg       ; clear shift register
  inc                  <VIA_port_b          ; disable mux
  stb                  <VIA_port_a          ; VIA X vector = B
  sta                  <VIA_t1_cnt_hi       ; enable timer
  endm

;*******************************************************************************
; M_MOVE_END
;
; End movement(wait for timer 1).
; DESTROYS: D
;*******************************************************************************
M_MOVE_END             macro
  ldb                  #$40                 ; B = bit6
! bitb                 <VIA_int_flags       ; while timer 1 interrupt flag != 1
  beq                  <                    ; |
  endm

;*******************************************************************************
; M_MOVE_END_ZERO
;
; End movement(wait for timer 1) and zero beam.
; DESTROYS: D
;*******************************************************************************
M_MOVE_END_ZERO        macro
  M_MOVE_END                                ;
  M_ZERO_VECTOR_BEAM                        ;
  endm

;*******************************************************************************
; M_MOVE_TO_D_NT
;
; Move beam to D.
; Optimzed, tweaked not perfect...
; IN:
;   D - move vector
; DESTROYS: D
;*******************************************************************************
M_MOVE_TO_D_NT         macro
  M_MOVE_TO_D_START                         ;
  M_MOVE_END                                ;
  endm
