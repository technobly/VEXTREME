; Copyright (C) 2020 Brett Walach <technobly at gmail.com>
; --------------------------------------------------------------------------
; LED TEST demo v2.0
;
; This application demonstrates how to control the addressable LEDs
; by seeding $7ff0 through $7ff9 with the LED colors, then calling
; RPC function ID 7 (updateMulti) which writes out the data to all LEDs
;
; Also demonstrates RPC ID 8 for LED brightness control
;
; LED Colors
; ----------
; 0 = off
; 1 = red
; 2 = orange
; 3 = yellow
; 4 = green
; 5 = cyan
; 6 = blue
; 7 = pink
; 8 = magenta
; 9 = white (Try not to use this one... it kind of draws a lot of power!)
;
; LED brightness
; --------------
; 0 = off
; 1..30
; 31 = max brightness
;
; When sending brightness value, first multiply value (0 - 31) by 8,
; such that sent value in RPC function $7ffe = 0 - 248.
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

;***************************************************************************
; USER RAM SECTION ($C880-$CBEA)
;***************************************************************************
user_ram            equ      $c880
led_color           equ      $c880
led_num             equ      $c881 ; counter to know when to switch colors (20ms * 5 = 100ms)
led_brightness      equ      $c882 ; led brightness (0-31 values) << 3 bits (x8)
led_bright_true     equ      $c883 ; set if we just updated led_brightness
led0_addr           equ      $7ff0 ; start address of LED0
ledb_addr           equ      $7ffe ; led brightness RPC value address
;***************************************************************************
; SYSTEM AREA of USER RAM SECTION ($CB00-$CBEA)
;***************************************************************************
rpcfn               equ      $cb00
;***************************************************************************
; HEADER SECTION
;***************************************************************************
                    org      0
                    fcb      "g GCE 2020", $80            ; 'g' is copyright sign
                    fdb      vextreme_tune1               ; catchy intro music to get stuck in your head
                    fcb      $F6, $60, $20, -$60
                    fcb      "LED TEST V2.0",$80          ; some game information ending with $80
                    fcb      0                            ; end of game header
;***************************************************************************
; PROGRAM STARTS HERE
;***************************************************************************
main
;***************************************************************************
; RPC COPY START
;***************************************************************************
rpccopystart
                    ldx      #rpcfndat
                    ldy      #rpcfn
rpccopyloop
                    lda      ,x+
                    sta      ,y+
                    cmpx     #rpcfndatend
                    bne      rpccopyloop
;***************************************************************************
; RPC COPY END
;***************************************************************************
init_vars
                    lda      #1
                    sta      led_color
                    sta      led_bright_true
                    lda      #15
                    sta      led_brightness

                    lda      #0                        ; 0 disables Joy 1 X
                    sta      Vec_Joy_Mux_1_X           ;  |
                    lda      #0                        ; 0 disables Joy 1 Y
                    sta      Vec_Joy_Mux_1_Y           ;  |
                    lda      #0                        ; 0 disables Joy 2 X & Y
                    sta      Vec_Joy_Mux_2_X           ;  | saves a few hundred cycles
                    sta      Vec_Joy_Mux_2_Y           ;  |
;***************************************************************************
; LED UPDATE FUNCTION START
;***************************************************************************
; update LED brightness from 0 to led_brightness (max 31), but this value must also be multiplied by 8
led_brightness_start
                    lda      led_brightness            ; LED desired brightness
                    lsla                               ; (0-31) x8
                    lsla                               ;  |
                    lsla                               ;  |
                    ldx      #ledb_addr                ; Load start address for LED0
                    sta      ,x+                       ; Write brightness to LEDb memory
                    lda      #8                        ; rpc call to update LED bightness, saved in $7ffe
                    jmp      rpcfn                     ; Call, this will return to loop
led_brightness_exit
; update LEDs from 0 to led_num as led_color desired, off for remainder up to 9 (10 LEDs)
led_color_start
                    lda      #10                       ; Update all 10 LEDs
                    sta      led_num                   ;  |
                    clrb                               ; LED index counter
                    lda      led_color                 ; LED desired color
                    ldx      #led0_addr                ; Load start address for LED0
led_color_loop
                    cmpb     led_num                   ; Reached 0 - N LEDs yet?
                    beq      led_off_start             ;  Yes, turn off the rest
                    sta      ,x+                       ;  No, Write color to LED0 memory
                    incb                               ; inc LEDx index counter
                    bra      led_color_loop            ; Keep writing LEDx color
led_off_start
                    clra                               ; LED color that indicates OFF
led_off_loop
                    cmpb     #10                       ; Already wrote to last LED9 ?
                    beq      led_color_exit            ;  Yes, all done, exit
                    sta      ,x+                       ;  No, keep writing LEDx off color
                    incb                               ; inc LEDx index counter
                    bra      led_off_loop
led_color_exit
led_update
                    lda      #7                        ; rpc call to update multiple LEDs, color saved in $7ff0 - $7ff9
                    jmp      rpcfn                     ; Call
led_update_exit
;***************************************************************************
; MAIN LOOP
;***************************************************************************
loop
; Recal video stuff
                    jsr      Wait_Recal
                    jsr      Intensity_1F
; Input handling
                    jsr      Read_Btns
                    anda     #$0F                      ; ignore joy2
                    lsla                               ; convert to 2-byte index
                    ldu      #button_routines
                    leau     a,u                       ;fetch addr of string, page
                    pulu     pc

button_routines
                    fdb      nobuttons                 ; 0x00 no buttons
                    fdb      button1                   ; 0x01 b1
                    fdb      button2                   ; 0x02 b2
                    fdb      nobuttons                 ; 0x03 b2+b1
                    fdb      button3                   ; 0x04 b3
                    fdb      nobuttons                 ; 0x05 b3+b1
                    fdb      nobuttons                 ; 0x06 b3+b2
                    fdb      nobuttons                 ; 0x07 b3+b2+b1
                    fdb      button4                   ; 0x08 b4
                    fdb      nobuttons                 ; 0x09 b4+b1
                    fdb      nobuttons                 ; 0x0A b4+b2
                    fdb      nobuttons                 ; 0x0B b4+b2+b1
                    fdb      nobuttons                 ; 0x0C b4+b3
                    fdb      nobuttons                 ; 0x0D b4+b3+b1
                    fdb      nobuttons                 ; 0x0E b4+b3+b2
                    fdb      nobuttons                 ; 0x0F b4+b3+b2+b1

nobuttons
                    lda      led_bright_true           ; Did we just update brightness?
                    cmpa     #1                        ;  |
                    bne      nobuttons_exit            ;  No, just loop again
                    clra                               ;  |
                    sta      led_bright_true           ;  Yes, force a color update to refresh the LEDs
                    jmp      led_color_start           ;  |
nobuttons_exit
                    jmp      loop

button1
                    lda      led_color
                    cmpa     #0
                    beq      button1_exit
                    deca
                    sta      led_color
button1_exit
                    jmp      led_color_start
button2
                    lda      led_color
                    cmpa     #9
                    beq      button2_exit
                    inca
                    sta      led_color
button2_exit
                    jmp      led_color_start
button3
                    lda      led_brightness
                    cmpa     #0
                    beq      led_brightness_jump
                    deca
                    sta      led_brightness
                    bra      led_brightness_jump
button4
                    lda      led_brightness
                    cmpa     #31
                    beq      led_brightness_jump
                    inca
                    sta      led_brightness
led_brightness_jump
                    lda      #1
                    sta      led_bright_true
                    jmp      led_brightness_start

;***************************************************************************
; RPC FUNCTION START
;***************************************************************************
; Rpc function. Because this makes the cart unavailable, this
; will copied to SRAM. Call as rpcfn.
rpcfndat
                    sta      $7fff
rpcwaitloop
                    lda      $0
                    cmpa     # 'g'
                    bne      rpcwaitloop
                    lda      $1
                    cmpa     # ' '
                    bne      rpcwaitloop
                    jmp      loop ;start address
rpcfndatend
;***************************************************************************
; RPC FUNCTION END
;***************************************************************************

;***************************************************************************
; SUBROUTINE SECTION
;***************************************************************************

;***************************************************************************
; DATA SECTION
;***************************************************************************

; VEXTREME Tune Notes
CS5                 equ      $1E
F5                  equ      $22
FS5                 equ      $23
GS5                 equ      $25
AS5                 equ      $27
RST                 equ      $3F
VIBENL              fcb      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
FADE14              fdb      $0000,$2DDD,$DDDD,$B000,0,0,0,0
; VEXTREME Intro Tune
vextreme_tune1
                    fdb      FADE14
                    fdb      VIBENL
                    fcb      FS5,8
                    fcb      FS5,8
                    fcb      FS5,8
                    fcb      AS5,16
                    fcb      GS5,16
                    fcb      FS5,16
                    fcb       F5,16
                    fcb      CS5,8
                    fcb      FS5,8
                    fcb      RST,8
                    fcb      0,$80 ; $80 is end marker for music, frequency is not played so 0
