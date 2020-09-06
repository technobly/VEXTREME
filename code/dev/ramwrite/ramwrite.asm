; Copyright (C) 2020 Brett Walach <technobly at gmail.com>
; --------------------------------------------------------------------------
; RAM WRITE and DUMP
;
; * This application demonstrates USB Developer Mode
; * It will enter Developer Mode automatically right now,
; * and it's up to you to load this app from a call to RPC ID 11, arg 0
; * See README.md for more details
;
                    include  "vectrex.i"
;***************************************************************************
; DEFINES SECTION
;***************************************************************************
TEXT_SIZE           equ      #$F850
;***************************************************************************
; USER RAM SECTION ($C880-$CBEA)
;***************************************************************************
                    bss
                    org      $c880
user_ram
mem_check_pass      rmb      1
temp_mem_a          rmb      1
temp_mem_b          rmb      1
show_check_cnt      rmb      1
show_check_true     rmb      1

last_user_ram       rmb      1                             ; unused in code
;***************************************************************************
; SYSTEM AREA of USER RAM SECTION
;***************************************************************************
rpcfn1              equ      last_user_ram
;***************************************************************************
; HEADER SECTION
;***************************************************************************
                    code
                    org      0
                    fcb      "g GCE 2020", $80        ; 'g' is copyright sign
                    fdb      mute1                    ; no sound
                    fcb      $F6, $60, $20, -$40
                    fcb      "RAM WRITE",$80          ; some game information ending with $80
                    fcb      0                        ; end of game header
;***************************************************************************
; PROGRAM STARTS HERE
;***************************************************************************
main
                    jsr      rpccopystart1       ; go copy the rpcfn1 into RAM
; init variables
                    lda      #0
                    sta      show_check_true
                    sta      show_check_cnt
                    sta      temp_mem_a
                    sta      temp_mem_b
                    sta      Vec_Joy_Mux_1_X     ; 0 disables Joy 1 X
                    sta      Vec_Joy_Mux_1_Y     ; 0 disables Joy 1 Y
                    sta      Vec_Joy_Mux_1_Y     ; 0 disables Joy 1 Y
                    sta      Vec_Joy_Mux_2_X     ; 0 disables Joy 2 X & Y
                    sta      Vec_Joy_Mux_2_Y     ; | saves a few hundred cycles
                    lda      #1
                    sta      mem_check_pass

;***************************************************************************
; MAIN LOOP
;***************************************************************************
main_loop
                    jsr      Wait_Recal         ; Recal and set intensity
                    jsr      Intensity_5F       ; |
print_something
                    ldd      #TEXT_SIZE         ; Set Dev Mode Text Size
                    std      Vec_Text_HW        ; |
                    ldu      #help1_str         ; | Print help string 1
                    ldd      #$2090             ; |
                    jsr      Print_Str_d        ; |
                    ldu      #help2_str         ; | Print help string 2
                    ldd      #$0090             ; |
                    jsr      Print_Str_d        ; |

                    lda      show_check_true    ; Check if we recently PASSED/FAILED
                    cmpa     #1                 ; |
                    bne      print_mem_idle     ; No, print idle
                                                ; Yes, print PASSED/FAILED
                    lda      Vec_Loop_Count+1   ; | after 5.12 seconds
                    anda     #$3f
                    suba     show_check_cnt     ; | Make sure we reset the status
                    beq      print_mem_idle     ; |
print_mem_status                                ; |
                    lda      mem_check_pass     ; Display PASSED or FAILED
                    cmpa     #1                 ; |
                    bne      print_mem_failed   ; |
print_mem_passed
                    ldu      #passed_str        ; | passed
                    bra      print_mem          ; |
print_mem_failed                                ; |
                    ldu      #failed_str        ; | failed
                    bra      print_mem          ; |
print_mem_idle                                  ; |
                    clra                        ; Disable show check
                    sta      show_check_true    ; |
                    ldu      #idle_str          ; | idle/reset
print_mem                                       ; |
                    ldd      #$D090             ; |
                    jsr      Print_Str_d        ; |

read_buttons
                    jsr      Read_Btns
                    anda     #$0F
                    cmpa     #$01
                    beq      do_button1
                    cmpa     #$02
                    beq      do_button2
                    cmpa     #$04
                    beq      do_button3
                    cmpa     #$08
                    beq      do_button4
no_button
                    bra      main_loop
do_button1
                    bra      ram_write_inc
do_button2
                    bra      ram_write_inc2
do_button3
                    bra      ram_write_99
do_button4
                    lbra     ram_dump
button_exit

; ------------------------------------------------------------------------------
; R/W using STA/LDA (incrementing bytes)
ram_write_inc
                    ldx      #$2000             ; Write RAM $2000 ~ $27ff
                    clra                        ; with 0x00 ~ 0xff, repeating
ram_write_loop1                                 ; |
                    sta      ,x+                ; |
                    inca                        ; |
                    cmpx     #$2800             ; |
                    bne      ram_write_loop1    ; |

ram_read_inc
                    ldx      #$2000             ; Read RAM $2000 ~ $27ff
                    clra                        ; expect 0x00 ~ 0xff, repeating
ram_read_loop1                                  ; |
                    sta      temp_mem_a         ; |
                    ldb      ,x+                ; |
                    cmpb     temp_mem_a         ; |
                    bne      mem_check_failed   ; | set failed flag
                                                ; |
                    inca                        ; |
                    cmpx     #$2800             ; |
                    bne      ram_read_loop1     ; |
                                                ; |
                    lda      #1                 ; | set passed flag
                    sta      mem_check_pass     ; |
                    jmp      start_timer        ; All done, let's get loopin'

mem_check_failed
                    lda      #0
                    sta      mem_check_pass
                    jmp      start_timer

; ------------------------------------------------------------------------------
; R/W using STD/LDD (incrementing bytes)
ram_write_inc2
                    ldx      #$2000             ; Write RAM $2000 ~ $27ff
                    clra                        ; with 0x0000, 0x0101, ~ 0xffff, repeating
                    clrb                        ; |
ram_write_loop3                                 ; |
                    std      ,x++               ; |
                    inca                        ; |
                    incb                        ; |
                    cmpx     #$2800             ; |
                    bne      ram_write_loop3    ; |

ram_read_inc2
                    ldx      #$2000             ; Read RAM $2000 ~ $27ff
                    clra                        ; expect 0x0000, 0x0101, ~ 0xffff, repeating
                    clrb                        ; |
ram_read_loop3                                  ; |
                    sta      temp_mem_a         ; |
                    stb      temp_mem_b         ; |
                    ldd      ,x++               ; |
                    cmpa     temp_mem_a         ; |
                    bne      mem_check_failed   ; | set failed flag
                    cmpb     temp_mem_b         ; |
                    bne      mem_check_failed   ; | set failed flag
                                                ; |
                    inca                        ; |
                    incb                        ; |
                    cmpx     #$2800             ; |
                    bne      ram_read_loop3     ; |
                                                ; |
                    lda      #1                 ; | set passed flag
                    sta      mem_check_pass     ; |
                    jmp      start_timer        ; All done, let's get loopin'

; ------------------------------------------------------------------------------
; R/W using STA/LDA (single byte)
ram_write_55
                    ldx      #$2000             ; Write RAM $2000 ~ $27ff
                    lda      #$55               ; with 0x55, repeating
                    bra      ram_write_loop2    ; |
ram_write_99
                    ldx      #$2000             ; Write RAM $2000 ~ $27ff
                    lda      #$AA               ; with 0xAA, repeating
ram_write_loop2                                 ; |
                    sta      ,x+                ; |
                    cmpx     #$2800             ; |
                    bne      ram_write_loop2    ; |

                    sta      temp_mem_a         ; | save reg a
                    ldx      #$2000             ; Read RAM $2000 ~ $27ff
                                                ; expect 0x55 of 0xAA, repeating
ram_read_loop2                                  ; |
                    ldb      ,x+                ; |
                    cmpb     temp_mem_a         ; | do we still match reg a?
                    bne      mem_check_failed   ; | set failed flag
                                                ; |
                    cmpx     #$2800             ; |
                    bne      ram_read_loop2     ; |
                                                ; |
                    lda      #1                 ; | set passed flag
                    sta      mem_check_pass     ; |
                    jmp      start_timer        ; All done, let's get loopin'

start_timer
                    lda      Vec_Loop_Count+1   ; Load the MSB of the loop counter
                    anda     #$3f
                    sta      show_check_cnt     ; save snapshot of loop timer
                    lda      #1                 ; enable show check
                    sta      show_check_true    ; |
                    jmp      main_loop

; ------------------------------------------------------------------------------

ram_dump
                    lda      #$1f               ; Dump RAM $1fe0 ~ $281f to TX output
                    sta      $7ff0              ; |
                    lda      #$e0               ; |
                    sta      $7ff1              ; |
                    lda      #$28               ; |
                    sta      $7ff2              ; |
                    lda      #$1f               ; |
                    sta      $7ff3              ; |
                    lda      #13                ; |
                    ldx      #main_loop         ; load x with return jmp address to main_loop
                    jmp      rpcfn1             ; Y'all come back now, ya hear?

;***************************************************************************
; RPC function for generic calls - will be copied to RAM - call as rpcfn1
;***************************************************************************
rpcfndat1
                    sta      $7fff
rpcwaitloop1
                    lda      $0
                    cmpa     # 'g'
                    bne      rpcwaitloop1
                    lda      $1
                    cmpa     # ' '
                    bne      rpcwaitloop1
                    jmp      ,x                 ; return address in x
rpcfndatend1
;***************************************************************************
; RPC FUNCTION END
;***************************************************************************
;***************************************************************************
; SUBROUTINE SECTION
;***************************************************************************
;***************************************************************************
; RPC1 COPY START - copy RPC function to Vectrex USER RAM
;***************************************************************************
rpccopystart1
                    ldx      #rpcfndat1
                    ldy      #rpcfn1
rpccopyloop1
                    lda      ,x+
                    sta      ,y+
                    cmpx     #rpcfndatend1
                    bne      rpccopyloop1
                    rts
;***************************************************************************
; RPC1 COPY END
;***************************************************************************
;***************************************************************************
; DATA SECTION
;***************************************************************************
VIBENL              fcb    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
FADE14              fdb    $0000,$2DDD,$DDDD,$B000,0,0,0,0
; No sound
mute1
                    fdb    FADE14
                    fdb    VIBENL
                    fcb    0,$80
help1_str
                    fcb    "1:STA INC 2:STD INC", $80
help2_str
                    fcb    "3:STA $99 4:DUMP", $80
passed_str
                    fcb    "   PASSED", $80
failed_str
                    fcb    "   FAILED", $80
idle_str
                    fcb    "   ------", $80
