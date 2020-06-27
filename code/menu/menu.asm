; Copyright (C) 2020 Brett Walach <technobly at gmail.com>
; --------------------------------------------------------------------------
; VEXTREME Menu
;
; This application demonstrates performs all menu functions for VEXTREME.
;
; Assembler manual: https://www.6809.org.uk/asm6809/doc/asm6809.shtml
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
MENU_POS_X          equ      #-110
MENU_POS_Y          equ      #50
MENU_ITEMS_MAX      equ      #4

PAGE_POS_X          equ      #-10
PAGE_POS_Y          equ      #-70
BUTTONS_POS_X       equ      #-120
BUTTONS_POS_Y       equ      #-80

DEVMODE_TEXT_SIZE   equ      #$F660
usb_dev_addr        equ      $ff0 ; used for sys_opt.usb_dev[9]
high_score_flag_rd  equ      $fe0 ; (menuData[] read:  IDLE:0x66, LOAD2VEC:0x77)
high_score_flag_wr  equ      $7fe0 ; (parmRam[] write: IDLE:0x66, SAVE2STM:0x88)
vusb_addr           equ      $ffb
yield_byte1         equ      $ffc
yield_byte2         equ      $ffd
;***************************************************************************
; USER RAM SECTION ($C880-$CBEA)
;***************************************************************************
                    bss
                    org      $c880
user_ram
page                rmb      1
cursor              rmb      1
curpos              rmb      1
waitjs              rmb      1
lastpage            rmb      1
logo_scale          rmb      1
logo_dir            rmb      1
page_label          rmb      1
page_label_end      rmb      1
calibrationValue    rmb      1
gameScale           rmb      1
jump_to_buttons     rmb      1
start_dev_mode      rmb      1
romnumber           rmb      1

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
rpcfn1              equ      last_user_ram
rpcfn2              equ      rpcfn1+(rpcfndatend1-rpcfndat1)
rpcfn3              equ      rpcfn1+(rpcfndatend2-rpcfndat1)
;***************************************************************************
; HEADER SECTION
;***************************************************************************
                    code
                    org      0
                    fcb      "g GCE 2020", $80            ; 'g' is copyright sign
                    fdb      vextreme_tune1               ; catchy intro music to get stuck in your head
                    fcb      $F6, $60, $20, -$42
                    fcb      "VEXTREME",$80               ; some game information ending with $80
                    fcb      $FC, $40, -$20, -$32
                    fcb      "HW=11.22",$80               ; HW version info
                    fcb      $FC, $40, -$30, -$32
                    fcb      "SW=33.44",$80               ; SW version info
                    fcb      0                            ; end of game header
;***************************************************************************
; CODE SECTION
;***************************************************************************
; here the cartridge program starts off
main
; TODO: we are running out of RAM, so conditionally copy the big ones when needed later
                    jsr      rpccopystart1            ; go copy the rpcfn into RAM
                    jsr      rpccopystart2            ; copy the LED routine
                    jsr      rpccopystart3            ; and finally the Dev Mode (hope we have RAM left)
initVars
; init menu var
                    jsr      init_page_cursor
; init vextreme logo vars
                    lda      #0
                    sta      jump_to_buttons
                    lda      #1
                    sta      logo_dir
                    sta      calibrationValue
                    sta      gameScale
                    lda      #20
                    sta      logo_scale
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
                    sta      start_dev_mode
                    lda      #$90
                    sta      x_pos
; init joystick
                    lda      #1
                    sta      Vec_Joy_Mux_1_X
                    lda      #3
                    sta      Vec_Joy_Mux_1_Y
                    lda      #0
                    sta      Vec_Joy_Mux_2_X              ; 0 disables Joy 2 X & Y
                    sta      Vec_Joy_Mux_2_Y              ; | saves a few hundred cycles
load_dev_mode
                    lda      #1                           ; Load just one byte
                    sta      $7ffe                        ; |
                    lda      #9                           ; From sysData[9], aka sys_opt.usb_dev
                    sta      $7ffd                        ; |
                    lda      #12                          ; rpc call to read sys_opt.usb_dev into $ff0
                    ldx      #check_dev_mode              ; load x with return jmp address
                    jmp      rpcfn2                       ; Call
check_dev_mode                                            ;
                    lda      usb_dev_addr                 ; Load sys_opt.usb_dev
                    cmpa     #0                           ; Is USB_DEV_DISABLED ?
                    beq      check_hs_clear               ;  Yes, continue normally
                    lda      #1                           ;  No, enable Dev Mode
                    sta      start_dev_mode               ;  |
                    bra      loop_led                     ;  and skip over high score stuff for speed

check_hs_clear
                    ldd      #$7321                       ; Check cold start flag
                    cmpd     Vec_Cold_Flag                ; |
                    beq      skip_hs_clear                ; | skip if warm boot
                    lda      #$66                         ; and only set to idle on cold start
                    sta      high_score_flag_wr           ; | (s/b $66 already, but doesn't hurt to double check)
skip_hs_clear

load_hs_flag1
                    lda      #1                           ; Load just one byte
                    sta      $7ffe                        ; |
                    lda      #$e0                         ; From parmRam[0xe0]
                    sta      $7ffd                        ; |
                    lda      #16                          ; rpc call to load high_score_flag into $fe0
                    ldx      #check_hs_flag1              ; load x with return jmp address
                    jmp      rpcfn2                       ; Call
check_hs_flag1                                            ;
                    lda      high_score_flag_rd           ; Check high score flag
                    cmpa     #$88                         ; Is it SAVE2STM:0x88 ?
                    bne      hs_exit                      ; No, skip saving high score to STM32, don't set to 0x66
store_hs_in_parmram                                       ;
                    ldd      $cbeb                        ; Save high score
                    sta      $7f00                        ; |
                    stb      $7f01                        ; |
                    ldd      $cbed                        ; |
                    sta      $7f02                        ; |
                    stb      $7f03                        ; |
                    ldd      $cbef                        ; |
                    sta      $7f04                        ; |
                    stb      $7f05                        ; |
                    lda      #14                          ; rpc call to saveHighScore()
                    ldx      #hs_return                   ; Load return point for jump instruction
                    jmp      rpcfn2                       ; Call
hs_return                                                 ;
                    lda      #$66                         ; Set back to IDLE:0x66 after saving to STM32
                    sta      high_score_flag_wr           ; |
hs_exit                                                   ;

loop_led
; Rainbow Step LEDs
                    lda      #4                           ; LED step rate
                    sta      $7ffe                        ; Store in parmRam[254]
                    lda      #5                           ; rpc call to rainbowStep()
                    ldx      #loop_main                   ; load x with return jmp address
                    jmp      rpcfn2                       ; Call
loop_main
; Recal video stuff
                    jsr      Wait_Recal
                    jsr      Intensity_5F
; Is Dev Mode enabled?
                    lda      start_dev_mode
                    cmpa     #1
                    lbeq     loaddevmode
; Jump to button read routine first time through, so we can skip logo drawing if 2 & 3 held
                    lda      jump_to_buttons
                    cmpa     #1
                    beq      drawLogo
                    lda      #1
                    sta      jump_to_buttons
                    jmp      check_buttons
; display vextreme logo
drawLogo
                    ldx      #_SM_vextreme_logo
nextLogoPart
                    lda      logo_scale
                    sta      VIA_t1_cnt_lo
                    lda      #$CE                          ;Blank low, zero high?
                    sta      <VIA_cntl
                    ldu      ,x++
                    beq      logoDone
                    jsr      drawSmart
                    bra      nextLogoPart
logoDone
; logo zoom in animation
                    lda      logo_scale
                    cmpa     #34
                    beq      logoZoomDone
                    inca
                    sta      logo_scale
logoZoomDone
; menu font settings
                    ldd      #$f160
                    std      Vec_Text_HW
                    lda      #$80
                    sta      <VIA_t1_cnt_lo
; menu init
                    lda      #0
                    sta      curpos
nameloop
                    ldb      cursor                       ;Handle highlighting of active entry
                    cmpb     curpos
                    bne      nohighlight
                    jsr      Intensity_7F
                    bra      hlend

nohighlight
                    jsr      Intensity_5F
hlend
; load address of next string title in reg U based on (page * 4) + curpos
                    ldu      #filedata                    ;data of pointer to filenames
                    ldb      page                         ;load page no
                    lda      #0
                    lslb                                  ; *4
                    rola                                  ; |
                    lslb                                  ; |
                    rola                                  ; |
                    addb     curpos                       ;add in current pos
                    adca     #0
                    lslb                                  ;because the addresses are 2 bytes
                    rola
                    leau     d,u                          ;fetch addr of string, page
                    ldu      a,u                          ;add pos
; adjust menu Y position in reg A
                    lda      curpos
                    nega                                  ;because y decreases downward
                    lsla                                  ; *8
                    lsla                                  ; |
                    lsla                                  ; | (fine tunes Y line spacing, add more lsla's to inc.)
                    adda     #MENU_POS_Y                  ; menu y offset from 0,0
; check if we hit the end of the list within this page
                    cmpu     #0                           ; compare reg U to null pointer, which signifies end of menu data
                    bne      showtitle                    ; if not the end, showtitle
                    lda      #1                           ; else bail out and set lastpage = 1 to avoid lengthy test later
                    sta      lastpage                     ; |
                    bra      menuend

showtitle
                    ldb      #MENU_POS_X                  ; menu x offset from 0,0
                    jsr      sync_Print_Str_d             ;print string
                    lda      curpos
                    inca
                    sta      curpos
                    cmpa     #MENU_ITEMS_MAX              ; number of menu items
                    bne      nameloop
                    lda      #0
                    sta      lastpage
menuend
                    jsr      Intensity_5F
;                    ldb      #$80
;                    stb      page_label_end
;                    ldb      page
;                    addb     #'1'
;                    stb      page_label
;                    ldu      #page_label
;                    lda      #PAGE_POS_Y
;                    ldb      #PAGE_POS_X
;                    jsr      sync_Print_Str_d             ;print string

                    ldu      #buttons_label
                    lda      #BUTTONS_POS_Y
                    ldb      #BUTTONS_POS_X
                    jsr      sync_Print_Str_d             ;print string
gfxend
;End of gfx routines.
;Input handling
                    lda      waitjs
                    lbne     dowaitjszero
                    jsr      Joy_Digital
;X handling
                    lda      Vec_Joy_1_X
                    jsr      handlepage
;Y handling
                    lda      Vec_Joy_1_Y
                    beq      skipymove
                    bpl      yneg
                    inc      cursor
                    bra      ymovedone
yneg
                    dec      cursor
ymovedone
                    lda      #MENU_ITEMS_MAX              ; index of menu items (0 to N-1)
                    deca                                  ; |
                    anda     cursor
                    sta      cursor
                    ldb      1
                    stb      waitjs
skipymove
check_buttons
                    jsr      Read_Btns
                    anda     #$0F                         ; ignore joy2
                    lsla                                  ; convert to 2-byte index
                    ldu      #button_routines
                    leau     a,u                          ;fetch addr of string, page
                    pulu     pc

dowaitjszero                                              ; Joystick has been touched,
                    jsr      Joy_Digital                  ; ignore input until it returns.
                    lda      Vec_Joy_1_X                  ; (TODO: input repeating?)
                    bne      nozero                       ; |
                    lda      Vec_Joy_1_Y                  ; |
                    bne      nozero                       ; |
                    sta      waitjs                       ; |
nozero                                                    ; |
                    jmp      loop_led                     ; jump back to main loop

button_routines
                    fdb      nobuttons                    ; 0x00 no buttons
                    fdb      dirup                        ; 0x01 b1
                    fdb      pageleft                     ; 0x02 b2
                    fdb      nobuttons                    ; 0x03 b2+b1
                    fdb      pageright                    ; 0x04 b3
                    fdb      nobuttons                    ; 0x05 b3+b1
                    fdb      loaddevmode                  ; 0x06 b3+b2
                    fdb      nobuttons                    ; 0x07 b3+b2+b1
                    fdb      startgame                    ; 0x08 b4
                   ;fdb      loadapp                      ; 0x09 b4+b1
                    fdb      nobuttons                    ; 0x09 b4+b1
                    fdb      nobuttons                    ; 0x0A b4+b2
                    fdb      nobuttons                    ; 0x0B b4+b2+b1
                    fdb      nobuttons                    ; 0x0C b4+b3
                    fdb      nobuttons                    ; 0x0D b4+b3+b1
                    fdb      nobuttons                    ; 0x0E b4+b3+b2
                    fdb      nobuttons                    ; 0x0F b4+b3+b2+b1

nobuttons
                    jmp      loop_led

pageleft
                    lda      #-1
                    bra      dopage
pageright
                    lda      #1
dopage
                    jsr      handlepage
                    jmp      loop_led

startgame                                                 ; Start the game
                    lda      page                         ; Calculate the number of the ROM
                    lsla                                  ; (page * 4) + cursor
                    lsla                                  ;  |
                    adda     cursor                       ;  |
                    sta      romnumber                    ; Stash the ROM number for later
                    sta      $7ffe                        ; Store in special cart location
                    lda      #1                           ; rpc call to doChangeRom()
                    ldx      #startgame_hs_cont           ; Load return point for jump instruction
                    jmp      rpcfn2                       ; Call
startgame_hs_cont

load_hs_flag2
                    lda      #1                           ; Load just one byte
                    sta      $7ffe                        ; |
                    lda      #$e0                         ; From parmRam[0xe0]
                    sta      $7ffd                        ; |
                    lda      #16                          ; rpc call to load high_score_flag into $fe0
                    ldx      #check_hs_flag2              ; load x with return jmp address
                    jmp      rpcfn2                       ; Call
check_hs_flag2                                            ;
                    lda      high_score_flag_rd           ; Check high score flag
                    cmpa     #$77                         ; Is it LOAD2VEC:0x77 ?
                    bne      startgame_skip_hs            ; No, skip loading high score to Vectrex
                                                          ; Yes, fall through and load high score
                    ldd      $ff0                         ; 0xFF0 - 0xFF5 should have high score
                    std      $cbeb                        ; |
                    ldd      $ff2                         ; |
                    std      $cbed                        ; |
                    ldd      $ff4                         ; |
                    std      $cbef                        ; |
                    lda      #$0x80                       ; |
                    sta      $cbf1                        ; |
                                                          ; Set high score flag to SAVE2STM:0x88
                    ldb      #$88                         ; | letting the menu know the next
                    stb      high_score_flag_wr           ; | time it loads to save the high score
                    bra      startgame_load               ; Jump to start game rom
                                                          ;
startgame_skip_hs                                         ;
                    lda      #$66                         ; Set back to IDLE:0x66
                    sta      high_score_flag_wr           ; |
startgame_load                                            ;
                    lda      romnumber                    ; Load our stashed ROM number
                    sta      $7ffe                        ; |
                    lda      #15                          ; rpc call to doStartRom()
                    jmp      rpcfn1                       ; Call

;loadapp
;                    lda      #0                           ; Load system app (hard coded to 0 for now: dev mode)
;                    sta      $7ffe                        ; |
;                    lda      #11                          ; rpc call to load a system rom
;                    ldx      #$f000                       ; load x with return jmp address
;                    jmp      rpcfn2                       ; Goodbye, we won't see you again hopefully!

loaddevmode
                    ldd      #DEVMODE_TEXT_SIZE           ; Set Dev Mode Text Size
                    std      Vec_Text_HW                  ; |
                    lda      #5                           ; Load Dev Mode
                    sta      $7ffe                        ; |
                    lda      #10                          ; rpc call initiate VUSB check
                    jmp      rpcfn3                       ; Goodbye, we won't see you again hopefully!

dirup
                    lda      #3                           ;rpc call to change up a directory
                    jmp      rpcfn1                       ;Call

;***************************************************************************
; RPC function for Menu operation - will be copied to RAM - call as rpcfn1
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
                    ldx      #$11                                ; set up header comparison
                    ldu      #rpcfn1+(vextreme_marker-rpcfndat1) ; relative to ram
headerloop
                    lda      ,x+
                    cmpa     ,u+
                    bne      newrom
                    cmpx     #$1A
                    bne      headerloop
                    jsr      init_page_cursor
                    jmp      loop_led
newrom
                    jmp      $f000
vextreme_marker
                    fcb      "VEXTREME",$80   ; for matching against cart header
rpcfndatend1

;***************************************************************************
; RPC function for generic calls - will be copied to RAM - call as rpcfn2
;***************************************************************************
; (needed to skip init_page_cursor conditionally)
rpcfndat2
                    sta      $7fff
rpcwaitloop2
                    lda      $0
                    cmpa     # 'g'
                    bne      rpcwaitloop2
                    lda      $1
                    cmpa     # ' '
                    bne      rpcwaitloop2
                    jmp      ,x                 ; return address in x
rpcfndatend2
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
; handlepage START
;***************************************************************************
handlepage
                    pshs     a,b
                    beq      skipxmove
                    bpl      xneg
                    lda      page
                    beq      xmovedone
                    dec      page
                    bra      xmovedone

xneg
                    lda      lastpage                     ; test if lastpage is already set
                    bne      xmovedone                    ; skip following test if so
; test for last page
                    ldu      #filedata                    ; data of pointer to filenames
                    ldb      page                         ; load page no
                    incb                                  ; temporarily move forward one page
                    lda      #0
                    lslb                                  ; *4
                    rola                                  ; |
                    lslb                                  ; |
                    rola                                  ; |
                                                          ; first curpos (no addb curpos)
                    adca     #0
                    lslb                                  ; because the addresses are 2 bytes
                    rola
                    leau     d,u                          ; fetch addr of string, page
                    ldu      a,u                          ; add pos
                                                          ;
                    cmpu     #0                           ; see if we're at the end of the list
                    bne      donextpage                   ; if not, do next page
                    lda      #1                           ; else set lastpage = 1
                    sta      lastpage                     ;  |
                    bra      xmovedone
donextpage
                    inc      page
xmovedone
                    ldb      1
                    stb      waitjs
skipxmove

                    puls     a,b
                    rts
;***************************************************************************
; handlepage END
;***************************************************************************
;***************************************************************************
; init_page_cursor START
;***************************************************************************
init_page_cursor
                    lda      lastselcart
                    anda     #3
                    sta      cursor
                    lda      lastselcart                  ; (lastselcart / 4) = page
                    lsra                                  ;  |
                    lsra                                  ;  |
                    sta      page                         ;  |
                    rts
;***************************************************************************
; init_page_cursor END
;***************************************************************************
;***************************************************************************
; RPC COPY START - copy menu RPC function to Vectrex USER RAM
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
; RPC COPY END
;***************************************************************************
;***************************************************************************
; RPC2 COPY START - copy LED RPC function to Vectrex USER RAM
;***************************************************************************
rpccopystart2
                    ldx      #rpcfndat2
                    ldy      #rpcfn2
rpccopyloop2
                    lda      ,x+
                    sta      ,y+
                    cmpx     #rpcfndatend2
                    bne      rpccopyloop2
                    rts
;***************************************************************************
; RPC2 COPY END
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
                    include  "printStringSync.asm"
;***************************************************************************
; DATA SECTION
;***************************************************************************
                    include  "font_5_fixed.asm"
                    include  "vextremeLogoSM.asm"
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

buttons_label
                    fcb      "(BK) (<) (>) (SEL)",$80               ; some game information ending with $80
;***************************************************************************
; MENU SECTION
;***************************************************************************
;Test menu list data. This gets overwritten by the firmware running in the
;STM with actual cartridge data.
                    org      $fff
lastselcart
                    fcb      0
                    org      $1000
filedata
                    fdb      text0
                    fdb      text1
                    fdb      text2
                    fdb      text3
                    fdb      text4
                    fdb      text5
                    fdb      text6
                    fdb      text7
                    fdb      text8
                    fdb      text9
                    fdb      text10
                    fdb      text11
                    fdb      text12
                    fdb      text13
                    fdb      text14
                    fdb      text15
                    fdb      text16
                    fdb      text17
                    fdb      text18
                    fdb      text19
                    fdb      font1
                    fdb      font2
                    fdb      font3
                    fdb      font4
                    fdb      font5
                    fdb      font6
                    fdb      font7
                    fdb      font8
                    fdb      font9
                    fdb      font10
                    fdb      font11
                    fdb      font12
                    fdb      0
text0
                    fcb      "CART 1",$80
text1
                    fcb      "CART 2",$80
text2
                    fcb      "CART 3",$80
text3
                    fcb      "CART 4",$80
text4
                    fcb      "DE VIERDE UNIT",$80
text5
                    fcb      "EENTJE MET EEN LANGE NAAM DUS",$80
text6
                    fcb      "BEN IK HET AL ZAT?",$80
text7
                    fcb      "LALALA",$80
text8
                    fcb      "OMGWTFBBQ",$80
text9
                    fcb      "GNORK",$80
text10
                    fcb      "EENTJE MET EEN LANGE NAAM DUS",$80
text11
                    fcb      "BEN IK HET AL ZAT?",$80
text12
                    fcb      "LALALA",$80
text13
                    fcb      "OMGWTFBBQ",$80
text14
                    fcb      "GNORK",$80
text15
                    fcb      "GNORK2",$80
text16
                    fcb      "GNORK3",$80
text17
                    fcb      "GNORK4",$80
text18
                    fcb      "OMGWTFBBQ",$80
text19
                    fcb      "GNORK",$80
font1
                    fcb      "! \"#$%&",$80
font2
                    fcb      "`()*+,-./",$80
font3
                    fcb      "0123456789",$80
font4
                    fcb      ":;<=>?@",$80
font5
                    fcb      "ABCDEFGH",$80
font6
                    fcb      "IJKLMNOP",$80
font7
                    fcb      "QRSTUVW",$80
font8
                    fcb      "YZ[\]^_",$80
font9
                    fcb      "abcdefgh",$80
font10
                    fcb      "ijklmnop",$80
font11
                    fcb      "qrstuvw",$80
font12
                    fcb      "xyz{|}~",$80
