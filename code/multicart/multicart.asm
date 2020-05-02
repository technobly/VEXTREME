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
;***************************************************************************
; USER RAM SECTION ($C880-$CBEA)
;***************************************************************************
user_ram            equ      $c880
page                equ      $c880
cursor              equ      $c881
curpos              equ      $c882
waitjs              equ      $c883
lastpage            equ      $c884
logo_scale          equ      $c885
logo_dir            equ      $c886
page_label          equ      $c887
page_label_end      equ      $c888
calibrationValue    equ      $c889
gameScale           equ      $c890
romnumber           equ      $c88a
highScoreSendFlag   equ      $c88b


;***************************************************************************
; SYSTEM AREA of USER RAM SECTION ($CB00-$CBEA)
;***************************************************************************
rpcfn               equ      $cb00
rpcfn2              equ      rpcfn+(rpcfndatend-rpcfndat)
;***************************************************************************
; HEADER SECTION
;***************************************************************************
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
; copy menu RPC function to Vectrex USER RAM
rpccopystart
                    ldx      #rpcfndat
                    ldy      #rpcfn
rpccopyloop
                    lda      ,x+
                    sta      ,y+
                    cmpx     #rpcfndatend
                    bne      rpccopyloop

; copy LED RPC function to Vectrex USER RAM
rpccopystart2
                    ldx      #rpcfndat2
                    ldy      #rpcfn2
rpccopyloop2
                    lda      ,x+
                    sta      ,y+
                    cmpx     #rpcfndatend2
                    bne      rpccopyloop2

init_vars
; init menu var
                    jsr      init_page_cursor
; init vextreme logo vars
                    lda      #1
                    sta      logo_dir
                    lda      #20
                    sta      logo_scale
                    lda      #1
                    sta      calibrationValue
                    lda      #1
                    sta      gameScale
;Check cold start flag and only clear on cold start
                    ldd      #$7321
                    cmpd     Vec_Cold_Flag
                    beq      loop
                    lda      #0
                    sta      highScoreSendFlag
loop
; Send high score flag to STM
                    lda      highScoreSendFlag
                    beq      loop_cont1             ; Skip saving high score if flag is not 0xA5
                    lda      #0
                    sta      highScoreSendFlag      ; Clear high score flag
store_hs_in_parmram
                    ldd      $cbeb                  ; Create a subroutine to save highs score
                    sta      $7f00
                    stb      $7f01
                    ldd      $cbed
                    sta      $7f02
                    stb      $7f03
                    ldd      $cbef
                    sta      $7f04
                    stb      $7f05
                    lda      #10                          ; rpc call to saveHighScore()
                    ldx      #loop_cont1                  ; Load return point for jump instruction
                    jmp      rpcfn2                       ; Call
loop_cont1
; Rainbow Step LEDs
                    lda      #4                           ; LED step rate
                    sta      $7ffe                        ; Store in parmRam[254]
                    lda      #5                           ; rpc call to rainbowStep()
                    ldx      #loop_cont_main              ; load x with return jmp address
                    jmp      rpcfn2                       ; Call
loop_cont_main
				
; Recal video stuff
do_recal
                    jsr      Wait_Recal
                    jsr      Intensity_5F
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
; update joystick
                    lda      #1
                    sta      Vec_Joy_Mux_1_X
                    lda      #3
                    sta      Vec_Joy_Mux_1_Y
                    lda      #0
                    sta      Vec_Joy_Mux_2_X
                    sta      Vec_Joy_Mux_2_Y
                    lda      #0
                    sta      curpos
nameloop
                    ldb      cursor						  ;Handle highlighting of active entry
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
                    sta      lastpage                     ;  |
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
                    bne      dowaitjszero
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
                    jsr      Read_Btns
                    anda     #$0F                         ; ignore joy2
                    lsla                                  ; convert to 2-byte index
                    ldu      #button_routines
                    leau     a,u                          ;fetch addr of string, page
                    pulu     pc

;Js has been touched. Ignore input until it returns.
;(ToDo: input repeating?)
dowaitjszero
                    jsr      Joy_Digital
                    lda      Vec_Joy_1_X
                    bne      nozero
                    lda      Vec_Joy_1_Y
                    bne      nozero
                    sta      waitjs
nozero
                    jmp      loop

button_routines
                    fdb      nobuttons                    ; 0x00 no buttons
                    fdb      dirup                        ; 0x01 b1
                    fdb      pageleft                     ; 0x02 b2
                    fdb      nobuttons                    ; 0x03 b2+b1
                    fdb      pageright                    ; 0x04 b3
                    fdb      nobuttons                    ; 0x05 b3+b1
                    fdb      nobuttons                    ; 0x06 b3+b2
                    fdb      nobuttons                    ; 0x07 b3+b2+b1
                    fdb      startgame                    ; 0x08 b4
                    fdb      nobuttons                    ; 0x09 b4+b1
                    fdb      nobuttons                    ; 0x0A b4+b2
                    fdb      nobuttons                    ; 0x0B b4+b2+b1
                    fdb      nobuttons                    ; 0x0C b4+b3
                    fdb      nobuttons                    ; 0x0D b4+b3+b1
                    fdb      nobuttons                    ; 0x0E b4+b3+b2
                    fdb      nobuttons                    ; 0x0F b4+b3+b2+b1

nobuttons
                    jmp      loop

pageleft
                    lda      #-1
                    bra      dopage
pageright
                    lda      #1
dopage
                    jsr      handlepage
                    jmp      loop

startgame                                                 ;Start the game.
                    lda      page                         ;Calculate the number of the ROM
                    lsla                                  ; (page * 4) + cursor
                    lsla                                  ;  |
                    adda     cursor                       ;  |
                    sta      romnumber                    ;Store ROM number
                    sta      $7ffe                        ;Store in special cart location
                    lda      #1                           ;rpc call to doChangeRom()
                    ldx      #startgame_hs_cont           ; Load return point for jump instruction
                    jmp      rpcfn2                       ;Call
startgame_hs_cont
; 0x7F00 - 0x7F05 should have high score.  Note we must do byte reads to memory from 0x0000 to 0x7FFF
                    lda      $ff7                         ; Should be a 0x77 of this is a game
                    cmpa     #$77
                    bne      startgame_hs_done            ; TODO - Jump location??? 

                    lda      $ff0
                    ldb      $ff1
                    std      $cbeb
                    lda      $ff2
                    ldb      $ff3
                    std      $cbed
                    lda      $ff4
                    ldb      $ff5
                    std      $cbef
                    lda      #$0x80
                    sta      $cbf1

                    ldb      #1
                    stb      highScoreSendFlag            ; Set high score flag to 0x01
                    stb      $ff7                         ; Set flag at 0xFF7 to 0x01

startgame_hs_done
                    lda      romnumber
                    sta      $7ffe                        ;Store romnumber in special cart location
                    lda      #11                          ;rpc call to doStartRom()
                    jmp      rpcfn                        ;Call

dirup
                    lda      #3                           ;rpc call to change up a directory
                    jmp      rpcfn                        ;Call

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

init_page_cursor
                    lda      lastselcart
                    anda     #3
                    sta      cursor
                    lda      lastselcart                  ; (lastselcart / 4) = page
                    lsra                                  ;  |
                    lsra                                  ;  |
                    sta      page                         ;  |
                    rts

;Rpc function. Because this makes the cart unavailable, this
;will copied to SRAM. Call as rpcfn.
rpcfndat
                    sta      $7fff						; Trigger doChangeRom() in romemu.S
rpcwaitloop
                    lda      $0
                    cmpa     # 'g'
                    bne      rpcwaitloop
                    lda      $1
                    cmpa     # ' '
                    bne      rpcwaitloop
                    ; set up header comparison
                    ldx      #$11
                    ldu      #rpcfn+(vextreme_marker-rpcfndat) ; needs to be relative to where it's copied in ram
headerloop
                    lda      ,x+
                    cmpa     ,u+	
                    bne      newrom
                    cmpx     #$1A
                    bne      headerloop
                    jmp      loop_cont_main ;start address
newrom
                    ldu      #$f000
                    pshs     u
                    rts
vextreme_marker
                    fcb      "VEXTREME",$80   ; for matching against cart header
rpcfndatend


; LED RPC2 function (needed to skip init_page_cursor conditionally)
; This will block here because when we do reads on the 6809 the romemu.S
; code is supposed to detect the read and then inject the correct data
; so that it is successfull.  However, since romemu.S is off handling the
; function the the rpc told it to perform, the reads will not be handled
; properly and bad data will be give to the 6809.
; Input: X register points to addres for return JMP instruction
rpcfndat2
                    sta      $7fff
rpcwaitloop2
                    lda      $0
                    cmpa     # 'g'
                    bne      rpcwaitloop2
                    lda      $1
                    cmpa     # ' '
                    bne      rpcwaitloop2
                    jmp      ,x
rpcfndatend2

;***************************************************************************
; SUBROUTINE SECTION
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
;Test multicart list data. This gets overwritten by the firmware running in the
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
