; Copyright (C) 2020 Brett Walach <technobly at gmail.com>
; --------------------------------------------------------------------------
; USB Developer Mode
;
; * This application demonstrates USB Developer Mode
; * It will enter Developer Mode automatically right now,
; * and it's up to you to load this app from a call to RPC ID 11, arg 0
; * See README.md for more details
;
;
; Original header follows:
; --------------------------------------------------------------------------
; Copyright (C) 2015 Jeroen Domburg <jeroen at spritesmods.com>
;
; This library is free software: you can redistribute it and/or modify
; it under the terms of the GNU Lesser General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public License
; along with this library.  If not, see <http://www.gnu.org/licenses/>.
;
                    include  "vectrex.i"
;***************************************************************************
; DEFINES SECTION
;***************************************************************************
DEVMODE_TEXT_SIZE   equ      #$F660
usb_dev_addr        equ      $ff0
vusb_addr           equ      $ffb
yield_byte1         equ      $ffc
yield_byte2         equ      $ffd
;***************************************************************************
; USER RAM SECTION ($C880-$CBEA)
;***************************************************************************
                    bss
                    org      $c880
user_ram

y_pos               rmb      1
x_pos               rmb      1
x_pos_cnt           rmb      1
y_dir               rmb      1
pressed_exit        rmb      1
pressed_run         rmb      1
pressed_none        rmb      1
ramdisk_once        rmb      1
vusb_once           rmb      1

last_user_ram       rmb      1                             ; unused in code
;***************************************************************************
; SYSTEM AREA of USER RAM SECTION ($CB00-$CBEA)
;***************************************************************************
rpcfn3              equ      last_user_ram
;***************************************************************************
; HEADER SECTION
;***************************************************************************
                    code
                    org      0
                    fcb      "g GCE     ", $80        ; 'g' is copyright sign
                    fdb      mute1                    ; no sound
                    fcb      $F6, $60, $20, -$40
                    fcb      "DEV MODE",$80           ; some game information ending with $80
                    fcb      0                        ; end of game header
;***************************************************************************
; PROGRAM STARTS HERE
;***************************************************************************
main
                    jsr      rpccopystart3        ; go copy the rpcfn into RAM
; init dev mode vars
                    lda      #0
                    sta      y_dir
                    sta      y_pos
                    sta      x_pos_cnt
                    sta      ramdisk_once
                    sta      vusb_once
                    sta      pressed_exit
                    sta      pressed_run
                    sta      pressed_none
                    lda      #$90
                    sta      x_pos
; init joystic
                    lda      #0
                    sta      Vec_Joy_Mux_1_X     ; 0 disables Joy 1 X
                    sta      Vec_Joy_Mux_1_Y     ; 0 disables Joy 1 Y
                    sta      Vec_Joy_Mux_1_Y     ; 0 disables Joy 1 Y
                    sta      Vec_Joy_Mux_2_X     ; 0 disables Joy 2 X & Y
                    sta      Vec_Joy_Mux_2_Y     ; | saves a few hundred cycles

;***************************************************************************
; MAIN LOOP
;***************************************************************************
loop
; Recal video stuff
                    jsr      Wait_Recal
                    jsr      Intensity_7F

loaddevmode
                    ldd      #DEVMODE_TEXT_SIZE ; Set Dev Mode Text Size
                    std      Vec_Text_HW        ; |
                    lda      #5                 ; Load Dev Mode
                    sta      $7ffe              ; |
                    lda      #10                ; rpc call initiate VUSB check
                    jmp      rpcfn3             ; Goodbye, we won't see you again hopefully!

                    jmp      loop               ; Should never get here

;***************************************************************************
; RPC function for DEV MODE operation - will be copied to RAM - call as rpcfn3
;***************************************************************************
rpcfndat3
                    sta      $7fff
rpcwaitloop3
                    jsr      Wait_Recal
                    lda      Vec_Loop_Count+1   ; Load the MSB of the counter
                    adda     #$30               ; Give it a bit of offset for first start
                                                ;  so we can see the text initially.
                    sta      y_pos              ; save it for text movement
                    lda      y_pos
flip_start
                    cmpa     #$7f               ; y_pos needs to be in reg a
                    beq      direction_flip
                    bra      no_flip

direction_flip
                    inc      y_dir              ; used with bita below
                    lda      x_pos_cnt          ; 0 - 15 counter
                    inca                        ; |
                    anda     #$f                ; |
                    sta      x_pos_cnt          ; |
no_flip
move_x_a_bit
                    lda      x_pos_cnt
                    ldx      #rpcfn3+(y_pos_vals-rpcfndat3) ; relative to ram
                    lda      a,x
                    sta      x_pos
up_or_down
                    lda      y_dir
                    bita     #1
                    beq      going_up
going_down
                    lda      #$ff
                    suba     y_pos
                    sta      y_pos
going_up
breath_with_me
                    lda      y_pos              ; reload y_pos
                    lsra                        ; count / 16 to slow down fade
                    lsra                        ; |
                    lsra                        ; |
                    lsra                        ; |
                    anda     #$f                ; Mask off the value for indexing 16 vals
                    ldx      #rpcfn3+(intensity_vals-rpcfndat3) ; relative to ram
                    lda      a,x
                    suba     #$20
                    jsr      Intensity_a
read_buttons
                    ;clr      Vec_Btn_State
                    ;jsr      Read_Btns_Mask
                    jsr      Read_Btns
                    anda     #$0F
                    cmpa     #$01
                    beq      do_button1
                    cmpa     #$08
                    beq      do_button4
no_button
                    lda      #1
                    sta      pressed_none
                    bra      button_exit

do_button1
                    lda      #1
                    sta      pressed_exit
                    sta      ramdisk_once       ; short circuit these so we start looping
                    sta      vusb_once          ; |
                    lda      #0
                    sta      pressed_none
                    bra      button_exit

do_button4
                    lda      #1
                    sta      pressed_run
                    sta      ramdisk_once       ; short circuit these so we start looping
                    sta      vusb_once          ; |
                    lda      #0
                    sta      pressed_none
button_exit
check_vusb
                    lda      vusb_once          ; Have we completed vusb checks?
                    cmpa     #1                 ; |
                    beq      start_ramdisk      ; | Yes, proceed to starting the ramdisk
                    lda      vusb_addr          ; | NO, read VUSB value ($99:high=USB, $66:low=NO-USB,
                    cmpa     #$66               ; any other val and the RPC hasn't returned yet)
                    lbeq     rpcreadvusb        ; keep reading VUSB
                    cmpa     #$99               ; Yes, we have VUSB!
                    beq      finish_vusb        ; | finish vusb checks
                    jmp      rpcreadvusb1       ; Else, unknown USB state

finish_vusb
                    inc      vusb_once
start_ramdisk
                    lda      ramdisk_once       ; Have we loaded the ramdisk yet?
                    cmpa     #1                 ; |
                    beq      ramdisk_loaded     ; | Yes, just loop now
                    jmp      rpcdevwait         ; | No, let's get this ramdisk party started

ramdisk_loaded
                    lda      pressed_run
                    cmpa     #1
                    beq      skip_cart_msg
                    lda      pressed_exit
                    cmpa     #1
                    beq      skip_cart_msg

switch_to_help_msg
                    lda      y_dir              ; unrolled print_sub, because vec was crashing
                    bita     #1
                    beq      load_cart_msg
                    ldu      #rpcfn3+(help_str-rpcfndat3) ; relative to ram
                    bra      show_help_msg
load_cart_msg
                    ldu      #rpcfn3+(usbdevmode_str-rpcfndat3) ; needs to be relative to where it's copied in ram
show_help_msg
                    lda      y_pos
                    ldb      x_pos
                    jsr      Print_Str_d
skip_cart_msg

ramdisk_yielded
                    lda      yield_byte1        ; Every 1s, the ramdisk will yield
                    cmpa     # 'v'              ; to the Vectrex
                    bne      load_cart          ; so we can either let it know to
                    lda      yield_byte2        ; keep ramdisk waiting / exit or run
                    cmpa     # 'x'              ; This byte sequence let's us know to
                    bne      load_cart          ; Give 1 of the 3 answers in response
act_on_buttons
                    lda      pressed_none
                    cmpa     #1
                    lbne     rpcwaitloop3
                    lda      pressed_run
                    cmpa     #1
                    beq      rpcdevrun
                    lda      pressed_exit
                    cmpa     #1
                    beq      rpcdevexit
rpcdevwait
                    lda      #1
                    sta      ramdisk_once
                    lda      #0
                    sta      $7ffe
                    lda      #10
                    jmp      rpcfn3

rpcdevrun
                    lda      #4
                    sta      $7ffe
                    lda      #10
                    jmp      rpcfn3

rpcdevexit
                    lda      #1
                    sta      $7ffe
                    lda      #10
                    jmp      rpcfn3

load_cart
                    lda      $0
                    cmpa     # 'g'
                    lbne     rpcwaitloop3
                    lda      $1
                    cmpa     # ' '
                    lbne     rpcwaitloop3

                    jmp      $f000              ; warm boot address to reset the cart.bin that was just loaded

rpcreadvusb
                    ldu      #rpcfn3+(pluginusb_str-rpcfndat3) ; relative to ram
                    jsr      print_sub
                    lda      y_pos
                    anda     #$10
                    cmpa     #$10
                    bne      rpcreadvusb1_exit  ; only read the VUSB every 320ms, no need to spam it
                    lda      #5
                    sta      $7ffe
                    lda      #10
                    jmp      rpcfn3

rpcreadvusb_exit
                    jmp      rpcwaitloop3

rpcreadvusb1
                    ldu      #rpcfn3+(unknownusb_str-rpcfndat3) ; relative to ram
                    jsr      print_sub
                    lda      y_pos
                    anda     #$10
                    cmpa     #$10
                    bne      rpcreadvusb1_exit  ; only read the VUSB every 320ms, no need to spam it
                    lda      #5
                    sta      $7ffe
                    lda      #10
                    jmp      rpcfn3

rpcreadvusb1_exit
                    jmp      rpcwaitloop3

; ====== RAM SUBROUTINES ======
print_sub
                    lda      y_dir
                    bita     #1
                    beq      print_exit
                    ldu      #rpcfn3+(help_str-rpcfndat3) ; relative to ram
print_exit
                    lda      y_pos
                    ldb      x_pos
                    jsr      Print_Str_d
                    rts

; ====== RAM DATA ======
usbdevmode_str
                    fcb      " LOAD CART.BIN", $80
pluginusb_str
                    fcb      "  PLUG IN USB", $80
unknownusb_str
                    fcb      " UNKNOWN USB", $80
help_str
                    fcb      "1: EXIT  4: RUN", $80
intensity_vals
                    fcb      $78,$68,$58,$48,$38,$28,$18,$18,$18,$18,$28,$38,$48,$58,$68,$78
y_pos_vals
                    fcb      $90,$80,$98,$8E,$8A,$9A,$88,$9C,$94,$8C,$92,$96,$86,$84,$82,$9D
rpcfndatend3
;***************************************************************************
; RPC FUNCTION END
;***************************************************************************
;***************************************************************************
; SUBROUTINE SECTION
;***************************************************************************
;***************************************************************************
; RPC3 COPY START - copy DEV MODE function to Vectrex USER RAM
;***************************************************************************
rpccopystart3
                    ldx      #rpcfndat3
                    ldy      #rpcfn3
rpccopyloop3
                    lda      ,x+
                    sta      ,y+
                    cmpx     #rpcfndatend3
                    bne      rpccopyloop3
                    rts
;***************************************************************************
; RPC3 COPY END
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
