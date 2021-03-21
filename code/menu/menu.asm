; Copyright (C) 2020 Brett Walach <technobly at gmail.com>
; Copyright (C) 2021 Vasily Kiniv <vasily.kiniv at gmail.com>
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
  include  "macro.asm"

;***************************************************************************
; ASSEMBLY CONFIG SECTION
;***************************************************************************

; If enabled, settings data will be placed into RAM, instead of ROM, it is 
; useful for testing menu in emulators that doesn't allow to write data
; to ROM address space.
SETTINGS_IN_RAM        equ    #0

; 1 - scrollbar animation enabled, 0 - disabled
SCROLLBAR_ANIMATED     equ    #1

; If not zero, the value at the specified address will be printed at the 
; bottom of the screen. The value will be treated as unsigned 8bit number.
; For instance: 
;   DEBUG_VALUE            equ    m_repeat_counter
DEBUG_VALUE            equ    #0

;***************************************************************************
; DEFINES SECTION - ADDRESSES
;***************************************************************************
MENU_DATA_ADDR         equ    $4000
MENU_SYS_ADDR          equ    MENU_DATA_ADDR - $20
MENU_MEM_ADDR          equ    MENU_SYS_ADDR  - $400

; parmRam
RPC_ARG_ADDR           equ    $7f00
HIGHSCORE_WF_ADDR      equ    RPC_ARG_ADDR + $e0      ; IDLE:0x66, SAVE2STM:0x88
RPC_ID_ADDR            equ    $7fff

; sys
HIGHSCORE_RF_ADDR      equ    MENU_SYS_ADDR  + $0     ; IDLE:0x66, LOAD2VEC:0x77
HIGHSCORE_ADDR         equ    MENU_SYS_ADDR  + $10
USB_DEV_ADDR           equ    MENU_SYS_ADDR  + $1a
VUSB_ADDR              equ    MENU_SYS_ADDR  + $1b
RAMDISK_YIELD1_ADDR    equ    MENU_SYS_ADDR  + $1c
RAMDISK_YIELD2_ADDR    equ    MENU_SYS_ADDR  + $1d

;--------------------------------------------
;                  ROM MAP                  ;
;-------------------------------------------- 0 KB
; 0x0000                              CODE  ;
;                                           ;
;                                           ;
;                                           ;
; 0x3BDF                                    ;
;-------------------------------------------- 15 KB
; 0x3BE0                     MENU_MEM_ADDR  ; <- STM 1k bytes of user settings
; 0x3FE0                     MENU_SYS_ADDR  ; <- STM 32 bytes of system data
; 0x3FE0                 HIGHSCORE_RF_ADDR  ; <- STM 1 byte highscore read flag
; 0x3FE1                          RESERVED  ; 15 bytes
; 0x3FF0                    HIGHSCORE_ADDR  ; <- STM 7 bytes of highscore data
; 0x3FFA                      USB_DEV_ADDR  ; <- STM 1 byte developer mode flag
; 0x3FFB                         VUSB_ADDR  ; <- STM 1 byte usb ready flag
; 0x3FFC               RAMDISK_YIELD1_ADDR  ; <- STM 2 bytes ramdisk ping flag
; 0x3FFD               RAMDISK_YIELD2_ADDR  ;
; 0x3FFE                          RESERVED  ; 3 bytes
;-------------------------------------------- 16 KB
; 0x4000                    MENU_DATA_ADDR  ; <- STM current directory listing
;                                           ;
;                                           ;
;-------------------------------------------- 20 KB
; 0x5000              UNUSED ADDRESS SPACE  ;
;                                           ;
;                                           ;
;                                           ;
; 0x7EFF                                    ;
;-------------------------------------------- 31 KB
; 0x7F00                      RPC_ARG_ADDR  ; -> STM remote procedure arguments
; 0x7FFF                       RPC_ID_ADDR  ; -> STM remote procedure ID
;--------------------------------------------

;***************************************************************************
; DEFINES SECTION - CONSTANTS
;***************************************************************************

LIST_POS_X             equ    #-95          ; horizontal list pos(vertical is calculated based on max_linex)
LIST_STR_MAX           equ    #16           ; absolute max chars per line
LIST_WIDTH             equ    #90           ; define right border of the list

SCROLLBAR_POS_X        equ    #100
SCROLLBAR_POS_Y        equ    #-64          ; bottom border of the scrollbar(drawn from the bottom)
SCROLLBAR_TRACK_H      equ    #127          ; scrollbar track height
SCROLLBAR_TRACK_I      equ    #$2f          ; scrollbar track intensity
SCROLLBAR_THUMB_I      equ    #$4f          ; scrollbar thumb intensity
SCROLLBAR_SCALE        equ    #$80          ; scrollbar scale

LOGO_POS_X             equ    #-17          ; logo horizontal absolute pos
LOGO_OFFSET_Y          equ    #30           ; logo vertical offset from list

MOVE_SCALE             equ    #$80          ; scale used when moving while drawing vector string
SPACE_WIDTH            equ    #10           ; width of a space between letters

W_LINK                 equ    #$1C          ; control char for link input
W_NUM                  equ    #$1E          ; control char for number input
W_SELECT               equ    #$1F          ; control char for select input

BTN_LEFT               equ    #5            ; constants for joystick direction
BTN_RIGHT              equ    #6            ;
BTN_BOTTOM             equ    #7            ; 
BTN_TOP                equ    #8            ;

STARS_NUMBER           equ    #63           ; number of stars to be drawn in screensaver

DEVMODE_TEXT_SIZE      equ    #$F660

;***************************************************************************
; USER RAM SECTION ($C880-$CBEA)
;***************************************************************************
  bss
  org                  $c880
m_page                 rmb    1             ; current page index
m_cursor               rmb    1             ; list cursor index
m_curpos               rmb    1             ; used as item index while drawing list
m_waitjs               rmb    1             ; wait for joystick to return to zero
m_list_size            rmb    1             ; length of the list
m_romnumber            rmb    1             ; selected rom index
m_list_pos_y           rmb    1             ; menu Y pos, calculated based on user settings for max_lines
m_trim_at              rmb    1             ; used to set trim position for an item to be drawn
m_char_counter         rmb    1             ; used in vector string draw routines
m_move_vector          rmb    2             ; used to set draw position for vector string routines
m_list_ptr             rmb    2             ; pointer to current active list
m_input_sr_ptr         rmb    2             ; pointer to the button handler subroutine for an input widget
m_input_arg_ptr        rmb    2             ; pointer to the arguments for an input widget
m_repeat_counter       rmb    1             ; used as counter for button action repeat
m_cursor_intensity     rmb    1             ; cursor intensity animation
m_cursor_int_dir       rmb    1             ; cursor animation direction
m_list_anim_dir        rmb    1             ; list animation direction
m_list_anim_step       rmb    1             ; list animation step
m_sbt_height           rmb    1             ; scrollbar thumb height
m_sbt_offset           rmb    1             ; scrollbar thumb offset
m_sbt_offset_next      rmb    1             ; next scrollbar thumb offset(animation)
m_sbt_offset_step      rmb    1             ; scrollbar thumb offset step(animation)
m_idle_minutes         rmb    1             ; idling minutes counter
m_divide_tmp           rmb    1             ; used in division macros
m_tmp_d                rmb    2             ; 16bit general purpose
m_tmp_a                rmb    1             ; 8bit general purpose
m_tmp_b                rmb    1             ; 8bit general purpose
m_tmp_c                rmb    1             ; 8bit general purpose

; dev mode variables
m_start_dev_mode       rmb    1             ;
m_y_pos                rmb    1             ;
m_x_pos                rmb    1             ;
m_x_pos_cnt            rmb    1             ;
m_y_dir                rmb    1             ;
m_pressed_exit         rmb    1             ;
m_pressed_run          rmb    1             ;
m_pressed_none         rmb    1             ;
m_ramdisk_once         rmb    1             ;
m_vusb_once            rmb    1             ;

; settings (this will not be assembled in production build)
  IF                   SETTINGS_IN_RAM
m_max_lines            rmb    1             ;
m_max_chars            rmb    1             ;
m_led_mode             rmb    1             ;
m_led_luma             rmb    1             ;
m_led_red              rmb    1             ;
m_led_green            rmb    1             ;
m_led_blue             rmb    1             ;
m_ss_mode              rmb    1             ;
m_ss_delay             rmb    1             ;
m_last_cursor          rmb    1             ;
m_directory            rmb    1             ;
  ENDIF

;***************************************************************************
; SHARED AREA OF USER RAM
; The following area will be reused conditionally.
;***************************************************************************
m_shared_ram_begin     rmb    0             ; unused in code, serves as a marker

; variables used by screensaver
m_temp_b1              rmb    1
m_temp_w1              rmb    2
m_temp_w2              rmb    2
m_temp_w3              rmb    2
m_dot_x                rmb    1
m_dot_y                rmb    1
m_dot_z                rmb    2
m_dot_z_speed          rmb    2
m_dots_data            rmb    384

; area for RPC functions
m_rpcfn1               equ    m_shared_ram_begin
m_rpcfn2               equ    m_rpcfn1+(rpcfndatend1-rpcfndat1)
m_rpcfn3               equ    m_rpcfn1+(rpcfndatend2-rpcfndat1)

;***************************************************************************
; HEADER SECTION
;***************************************************************************
  code
  org                  0
  fcb                  "g GCE 2021", $80    ; 'g' is copyright sign
  fdb                  vextreme_tune1       ; catchy intro music to get stuck in your head
  fcb                  $F6, $60, $20, -$42  ;
  fcb                  "VEXTREME",$80       ; some game information ending with $80
  fcb                  $FC, $40, -$20, -$32 ;
  fcb                  "HW=11.22",$80       ; HW version info
  fcb                  $FC, $40, -$30, -$32 ;
  fcb                  "SW=33.44",$80       ; SW version info
  fcb                  0                    ; end of game header

;***************************************************************************
; CODE SECTION
;***************************************************************************
; here the cartridge program starts off
main
; TODO: we are running out of RAM, so conditionally copy the big ones when needed later
  M_COPY_ALL_RPC_2_RAM

  IF                   SETTINGS_IN_RAM
init_settings_in_ram
  lda                  #5                   ; | this section
  sta                  m_max_lines          ; | will be assembled
  sta                  m_ss_delay           ; | 
  lda                  #8                   ; | only when SETTINGS_IN_RAM
  sta                  m_max_chars          ; | is set to 1
  lda                  #1                   ; |
  sta                  m_ss_mode            ; |
  ENDIF

init_list_context
  ldd                  #filedata            ; menu_list_ptr = filedata
  std                  m_list_ptr           ; |
  ldd                  #0                   ; D = 0
  std                  m_input_sr_ptr       ; clear
  std                  m_input_arg_ptr      ; |
  lda                  #LIST_STR_MAX        ; trim_at = LIST_STR_MAX
  sta                  m_trim_at            ; |
  jsr                  init_page            ; init page, calculate scrollbar, etc.
  lda                  #95                  ; cursor_intensity = 95
  sta                  m_cursor_intensity   ; |
  clr                  m_list_anim_dir      ; clear
  clr                  m_list_anim_step     ; clear

init_dev_mode_vars
  lda                  #0                   ; A = 0
  sta                  m_y_dir              ; clear
  sta                  m_y_pos              ; |
  sta                  m_x_pos_cnt          ; |
  sta                  m_ramdisk_once       ; |
  sta                  m_vusb_once          ; |
  sta                  m_pressed_exit       ; |
  sta                  m_pressed_run        ; |
  sta                  m_pressed_none       ; |
  sta                  m_start_dev_mode     ; |
  lda                  #$90                 ; x_pos = 0x90
  sta                  m_x_pos              ; |

init_joystick
  clr                  m_repeat_counter     ; repeat_counter = 0
  lda                  #1                   ; Vec_Joy_Mux_1_X = 1
  sta                  Vec_Joy_Mux_1_X      ; |
  lda                  #3                   ; Vec_Joy_Mux_1_Y = 3
  sta                  Vec_Joy_Mux_1_Y      ; |
  ; 0 disables Joy 2 X & Y, saves a few hundred cycles
  lda                  #0                   ; 
  sta                  Vec_Joy_Mux_2_X      ; Vec_Joy_Mux_2_X = 0
  sta                  Vec_Joy_Mux_2_Y      ; Vec_Joy_Mux_2_Y = 0



;*******************************************************************************
; DEV MODE CHECK
;*******************************************************************************

load_dev_mode
  lda                  #1                   ; Load just one byte
  sta                  RPC_ARG_ADDR + 254   ; |
  lda                  #9                   ; From sysData[9], aka sys_opt.usb_dev
  sta                  RPC_ARG_ADDR + 253   ; |
  M_JSR_RPC            #12                  ; rpc call to read sys_opt.usb_dev into $3ff0

check_dev_mode
  lda                  USB_DEV_ADDR         ; Load sys_opt.usb_dev
  cmpa                 #0                   ; Is USB_DEV_DISABLED ?
  beq                  check_hs_clear       ;  Yes, continue normally
  lda                  #1                   ;  No, enable Dev Mode
  sta                  m_start_dev_mode     ;  |
  bra                  main_loop            ;  and skip over high score stuff for speed
; END OF DEV MODE CHECK



;*******************************************************************************
; HIGHTSCORE SAVE
;*******************************************************************************

check_hs_clear
  ldd                  #$7321               ; Check cold start flag
  cmpd                 Vec_Cold_Flag        ; |
  beq                  skip_hs_clear        ; | skip if warm boot
  lda                  #$66                 ; and only set to idle on cold start
  sta                  HIGHSCORE_WF_ADDR    ; | (s/b $66 already, but doesn't hurt to double check)

skip_hs_clear

load_hs_flag1
  lda                  #1                   ; Load just one byte
  sta                  RPC_ARG_ADDR + 254   ; |
  lda                  #$e0                 ; From parmRam[0xe0]
  sta                  RPC_ARG_ADDR + 253   ; |
  M_JSR_RPC            #16                  ; rpc call to load high_score_flag into $fe0

check_hs_flag1
  lda                  HIGHSCORE_RF_ADDR    ; Check high score flag
  cmpa                 #$88                 ; Is it SAVE2STM:0x88 ?
  bne                  hs_exit              ; No, skip saving high score to STM32, don't set to 0x66

store_hs_in_parmram
  ldd                  $cbeb                ; Save high score
  sta                  RPC_ARG_ADDR + 0     ; |
  stb                  RPC_ARG_ADDR + 1     ; |
  ldd                  $cbed                ; |
  sta                  RPC_ARG_ADDR + 2     ; |
  stb                  RPC_ARG_ADDR + 3     ; |
  ldd                  $cbef                ; |
  sta                  RPC_ARG_ADDR + 4     ; |
  stb                  RPC_ARG_ADDR + 5     ; |
  M_JSR_RPC            #14                  ; rpc call to highScoreSave()

hs_return
  lda                  #$66                 ; Set back to IDLE:0x66 after saving to STM32
  sta                  HIGHSCORE_WF_ADDR    ; |

hs_exit
; END OF HIGHTSCORE SAVE



;*******************************************************************************
; MAIN LOOP / ANIMATION, RECALIBRATION AND DEVMODE KICKOFF
;*******************************************************************************

main_loop
  lda                  m_ss_mode            ; if ss_mode != 1
  cmpa                 #1                   ; |
  bne                  cursor_animation     ; | screensaver is disabled, skip
  lda                  Vec_Loop_Count       ; if Vec_Loop_Count < 12 (~one minute)
  cmpa                 #12                  ; |
  blt                  cursor_animation     ; | less than minute passed, skip
  clr                  Vec_Loop_Count       ; clear 5s unit counter
  inc                  m_idle_minutes       ; idle_minutes++
  lda                  m_idle_minutes       ; if m_idle_minutes < m_ss_delay
  cmpa                 m_ss_delay           ; |
  blt                  cursor_animation     ; | not reached delay, skip starting screensaver
  jsr                  StarsInit            ; start screensaver
  ; when starsInit returns - user pressed a button
  M_RESET_IDLE                              ; reset idle counters
  M_COPY_ALL_RPC_2_RAM                      ; put back all RPCs into RAM

cursor_animation
  lda                  m_cursor_intensity   ; A = cursor_intensity
  ldb                  m_cursor_int_dir     ; if cursor_int_dir < 0
  bmi                  ca_decrease          ; | decrease intensity

ca_increase
  adda                 #1                   ; A(intensity) += 1
  cmpa                 #126                 ; A <= 126
  ble                  ca_done              ; | not rached end of animation, skip flipping
  ldb                  #-1                  ; cursor_int_dir = -1
  stb                  m_cursor_int_dir     ; | flip direction

ca_decrease
  suba                 #1                   ; A(intensity) -= 1
  cmpa                 #95                  ; A >= 95
  bge                  ca_done              ; | done
  ldb                  #1                   ; cursor_int_dir = 1
  stb                  m_cursor_int_dir     ; | flip direction

ca_done
  sta                  m_cursor_intensity   ; cursor_intensity = A

animate_led
; Rainbow Step LEDs
  lda                  m_led_mode           ; if led_mode != 1(not rainbow)
  cmpa                 #1                   ; |
  bne                  main_loop_recal      ; | skip to main loop
  lda                  #4                   ; rpc call to rainbowStep(4)
  sta                  RPC_ARG_ADDR + 254   ; |
  M_JSR_RPC            #5                   ; |

main_loop_recal
  jsr                  Wait_Recal           ; wait for recalibration

check_dev_mode_start
; Is Dev Mode enabled?
  lda                  m_start_dev_mode     ; if start_dev_mode == 1
  cmpa                 #1                   ; |
  lbeq                 loaddevmode          ; | load dev mode
; END OF MAIN LOOP / ANIMATION, RECALIBRATION AND DEVMODE KICKOFF



;*******************************************************************************
; MAIN LOOP / LOGO
;*******************************************************************************

draw_logo
  jsr                  Intensity_3F         ; set dim intensity
  lda                  m_cursor             ; if cursor >= 0
  bpl                  draw_logo2           ; | skip intensity animation
  lda                  m_cursor_intensity   ; INTENSITY(cursor_intensity)
  M_INTENSITY_A                             ; |

draw_logo2
  lda                  m_list_pos_y         ; move_vector = [LOGO_POS_X,
  adda                 #LOGO_OFFSET_Y       ; | list_pos_y + LOGO_OFFSET_Y]
  ldb                  #LOGO_POS_X          ; |
  std                  m_move_vector        ; |

draw_settings_logo
  M_IS_FILELIST                             ; if we are on main screen
  beq                  draw_main_logo       ; | skip to main logo
  ldx                  #LOGO_SETTINGS       ; X = settings logo vectorlist
  bra                  draw_logo3           ; skip to drawing

draw_main_logo
  ldx                  #LOGO_VECTREX        ; X = Vectrex logo vectorlist

draw_logo3
  M_DRAW_VLIST         3, 24, $80           ; draw vectorlist using mode3, scale 24
; END OF MAIN LOOP / LOGO

  ; draw debug value if set
  IF                   DEBUG_VALUE          ; conditional assembly
  lda                  #-127                ; move_vector = 0, -127
  ldb                  #0                   ; |
  std                  m_move_vector        ; |
  ldb                  DEBUG_VALUE          ; DrawVectorNumber(move_vector, DEBUG_VALUE)
  jsr                  DrawVectorNumber     ; |
  ENDIF                                     ; end of conditional assembly

;*******************************************************************************
; MAIN LOOP / LIST
;*******************************************************************************

menu_begin
  ldd                  #0                   ; cursor = 0
  sta                  m_curpos             ; |
  std                  m_input_sr_ptr       ; clear previously highlighted item handler

menu_loop

item_anim_visibility
  lda                  m_list_anim_dir      ; if list_anim_dir == 0
  beq                  cursor_begin         ; | skip animation and dim item
  lda                  m_curpos             ; if curpos == 0 
  beq                  item_fade_in         ; | do fade in animation
  cmpa                 m_max_lines          ; if curpos == max_lines
  beq                  item_fade_out        ; | do fade out animation
  bra                  cursor_begin         ; skip fade animation

; if animation direction is negative, 'fade in' becomes 'fade out' 
item_fade_in
  lda                  m_list_anim_step     ; A = list_anim_step * 8
  lsla                                      ; |
  lsla                                      ; |
  lsla                                      ; |
  bra                  item_anim_done       ; apply intensity

item_fade_out
  lda                  #8                   ; A = (8 - list_anim_step) * 8
  suba                 m_list_anim_step     ; |
  lsla                                      ; |
  lsla                                      ; |
  lsla                                      ; |

item_anim_done
  M_INTENSITY_A                             ; INTENSITY(A)
  bra                  cursor_trim          ; skip to drawing item

cursor_begin
  ldb                  m_list_anim_dir      ; if list_anim_dir != 0
  bne                  cursor_dim           ; | skip animation
  ldb                  m_cursor             ; if cursor != curpos
  cmpb                 m_curpos             ; |
  bne                  cursor_dim           ; | no highlight

cursor_highlight
  lda                  m_cursor_intensity   ; INTENSITY(cursor_intensity)
  M_INTENSITY_A                             ; |
  lda                  #LIST_STR_MAX        ; trim_at = LIST_STR_MAX
  sta                  m_trim_at            ; |
  bra                  cursor_end

cursor_dim
  jsr                  Intensity_3F         ; dim the item

cursor_trim
  M_IS_FILELIST                             ; if list == filelist
  bne                  cursor_end           ; | skip trimming(trim only on the main screen)
  lda                  m_max_chars          ; trim_at = m_max_chars
  sta                  m_trim_at            ; |

cursor_end

item_begin
; load address of next string title in reg U based on page + curpos
  ldu                  m_list_ptr           ; U = pointer to the current list
  ldb                  m_page               ; if page == 0
  beq                  item_load_addr       ; | skip shift
  lda                  m_list_anim_dir      ; if list_anim_dir <= 0
  ble                  item_load_addr       ; | skip shift
  subb                 #1                   ; page -= 1 (scrolling up - shift list by one item)

item_load_addr
  clra                                      ; D = curpos * 2
  addb                 m_curpos             ; |
  lslb                                      ; | (addresses are 2 bytes)
  rola                                      ; |
  ldu                  d,u                  ; U = U[D]
  cmpu                 #0                   ; if U == NULL (end of menu data)
  beq                  menu_end             ; | done drawing list

item_position
; adjust menu Y position in reg A
  lda                  m_curpos             ; A(Y axis) = -(curpos * 24)
  ldb                  #24                  ; |
  mul                                       ; |
  negb                                      ; |
  exg                  a, b                 ; |
  adda                 m_list_pos_y         ; A(Y axis) += list_pos_y
  ldb                  m_list_anim_dir      ; if list_anim_dir != 0
  bne                  item_animate_y_pos   ; | 
  suba                 #24                  ; shift item downward if not scrolling

item_draw
  ldb                  #LIST_POS_X          ; menu x offset from 0,0
  std                  m_move_vector        ; move_vector = D
  jsr                  DrawListItem         ; draw item
  lda                  m_curpos             ; curpos += 1
  inca                                      ; |
  sta                  m_curpos             ; |
  cmpa                 m_max_lines          ; if curpos < max_lines
  lblt                 menu_loop            ; | next item
  bgt                  menu_end             ; if curpos > max_lines: end of list
  ldb                  m_list_anim_dir      ; if list_anim_dir != 0 (scrolling up or down)
  lbne                 menu_loop            ; | extend list by one item for list animation
  bra                  menu_end

item_animate_y_pos
  suba                 m_list_anim_step     ; A -= m_list_anim_step * 3
  suba                 m_list_anim_step     ; | looks ugly but it will take
  suba                 m_list_anim_step     ; | less cycles than doing lsla + adda + sta
  bra                  item_draw            ; draw next item

menu_end

list_animation
  ldb                  m_list_anim_dir      ; if list_anim_dir == 0
  beq                  la_end               ; done
  blt                  la_decrease_step     ; if list_anim_dir < 0, increase

la_increase_step
  inc                  m_list_anim_step     ; list_anim_step++
  lda                  #8                   ; if 8 > list_anim_step
  cmpa                 m_list_anim_step     ; |
  bgt                  la_end               ; | step done
  clr                  m_list_anim_dir      ; stop animation
  dec                  m_page               ; page--
  bra                  la_end               ; animataion done

la_decrease_step
  dec                  m_list_anim_step     ; list_anim_step--
  bne                  la_end               ; | step done
  clr                  m_list_anim_dir      ; stop animation
  inc                  m_page               ; page--

la_end

scrollbar_begin
  lda                  m_sbt_height         ; if scroll_thumb == 0
  beq                  scrollbar_end        ; | done
  lda                  #SCROLLBAR_SCALE     ; SCALE(SCROLLBAR_SCALE)
  M_SCALE_A                                 ; |
  lda                  #SCROLLBAR_POS_Y     ; D = SCROLLBAR_POS
  ldb                  #SCROLLBAR_POS_X     ; |

scrollbar_draw_track     
  M_MOVE_TO_D_NT                            ; MOVE(D)
  lda                  #SCROLLBAR_TRACK_I   ; INTENSITY(SCROLLBAR_TRACK_I)
  M_INTENSITY_A                             ; |
  lda                  #SCROLLBAR_TRACK_H   ; D = track height
  ldb                  #0                   ; |
  jsr                  Draw_Line_d          ; Draw_Line_d(D)

scrollbar_draw_thumb
  lda                  m_sbt_offset         ; D = thumb offset
  ldb                  #0                   ; |
  nega                                      ; inverse Y (negative on Y is downward)
  M_MOVE_TO_D_NT                            ; MOVE(D)
  lda                  #SCROLLBAR_THUMB_I   ; INTENSITY(SCROLLBAR_THUMB_I)
  M_INTENSITY_A                             ; |
  lda                  m_sbt_height         ; D = thumb height
  ldb                  #0                   ; |
  nega                                      ; inverse Y (negative - downward)
  jsr                  Draw_Line_d          ; draw thumb

scrollbar_end

  IF                   SCROLLBAR_ANIMATED   ; conditional assembly
scrollbar_anim_begin
  ldb                  m_sbt_offset         ; B = sbt_offset (till the end of block)
  cmpb                 m_sbt_offset_next    ; if B == m_sbt_offset_next
  beq                  scrollbar_anim_end   ; | done
  bgt                  sa_decrease          ; sbt_offset > sbt_offset_next

sa_increase
  addb                 m_sbt_offset_step    ; B += sbt_offset_step
  cmpb                 m_sbt_offset_next    ; if B > sbt_offset_next
  bgt                  sa_fix_overflow      ; | fix
  bra                  sa_update            ; update

sa_decrease
  subb                 m_sbt_offset_step    ; B -= sbt_offset_step
  cmpb                 m_sbt_offset_next    ; if B < sbt_offset_next
  blt                  sa_fix_overflow      ; | fix
  bra                  sa_update            ; update

sa_fix_overflow
  ldb                  m_sbt_offset_next    ; B = sbt_offset_next

sa_update
  stb                  m_sbt_offset         ; sbt_offset = B

scrollbar_anim_end
  ENDIF                                     ; end of conditional assembly
; END OF MAIN LOOP / LIST



;*******************************************************************************
; MAIN LOOP / INPUT HANDLING
;*******************************************************************************

check_for_joy_zero
  lda                  m_waitjs             ; if waitjs != 0
  lbne                 dowaitjszero         ; | skip handling, wait for joystick to return to zero
  jsr                  Joy_Digital          ; read joystick state

handle_x
  M_IS_FILELIST                             ; if list == filelist
  beq                  handle_x_page        ; | switch page

handle_x_ih
  ; on settings screen left/right is routed to the input widget handler
  lda                  Vec_Joy_1_X          ; if Vec_Joy_1_X == 0
  beq                  handle_y             ; | skip to handle_y
  bpl                  handle_x_right       ; if Vec_Joy_1_X > 0

handle_x_left
  M_JSR_IH             #BTN_LEFT            ; | call [I]nput[H]andler with arg=5(stick left)
  bra                  handle_x_done        ; done

handle_x_right
  M_JSR_IH             #BTN_RIGHT           ; call [I]nput[H]andler with arg=6(stick left)

handle_x_done
  ldb                  #1                   ; waitjs = true
  stb                  m_waitjs             ; |
  bra                  handle_y             ; skip

handle_x_page
  ; on main screen left/right is for switching whole page(+-max_lines)
  lda                  Vec_Joy_1_X          ;
  jsr                  handlepage           ;

handle_y
  lda                  m_list_anim_dir      ; if list_anim_dir != 0
  lbne                 handle_y_end         ; | skip (page switching is already going on)
  lda                  Vec_Joy_1_Y          ; if Vec_Joy_1_Y == 0
  beq                  handle_y_zero        ; | skip
  bpl                  handle_y_up          ; if Vec_Joy_1_Y > 0, up

handle_y_down
  lda                  m_page               ; if page + cursor + 1 >= list_size
  adda                 m_cursor             ; |
  adda                 #1                   ; |
  cmpa                 m_list_size          ; |
  bge                  handle_y_end         ; | skip (reached the end of list)
  inc                  m_cursor             ; cursor++
  bra                  handle_y_done        ; done

handle_y_up
  dec                  m_cursor             ; cursor--

handle_y_done
  ldb                  #1                   ; waitjs = true
  stb                  m_waitjs             ; |
  lda                  m_cursor             ; if cursor < 0
  blt                  prev_page            ; | page--; cursor = 0
  cmpa                 m_max_lines          ; if cursor >= MENU_ITEMS_MAX
  bge                  next_page            ; | page++; cursor = 0
  bra                  handle_y_end         ; skip page switch handling

prev_page
  ldb                  m_page               ; if page <= 0
  ble                  prev_page_zero       ; | skip prev_page
  subb                 #1                   ; B = page - 1
  jsr                  CalcScrollOffset     ; B = CalcScrollOffset(B)
  IF                   SCROLLBAR_ANIMATED   ; 
  stb                  m_sbt_offset_next    ; sbt_offset_next = B
  ELSE                                      ; 
  stb                  m_sbt_offset         ; sbt_offset = B
  ENDIF                                     ; 
  lda                  #0                   ; list_anim_step = 0
  sta                  m_list_anim_step     ; |
  sta                  m_cursor             ; cursor = 0
  lda                  #1                   ; list_anim_dir = 1
  sta                  m_list_anim_dir      ; |
  bra                  handle_y_end         ; skip

prev_page_zero
  lda                  #-1                  ; if repeat_counter == 0
  ldb                  m_repeat_counter     ; |
  beq                  prev_page_done       ; | cursor = -1 (move cursor to logo)
  lda                  #0                   ; curosr = 0 (stay at first line while holding joystick)

prev_page_done
  sta                  m_cursor             ; 
  bra                  handle_y_end         ; 

next_page
  ldb                  m_page               ; if page + cursor >= list_size
  addb                 m_cursor             ; |
  cmpb                 m_list_size          ; |
  bge                  handle_y_end         ; | do not switch page
  ldb                  m_page               ; B = page + 1
  addb                 #1                   ; |
  jsr                  CalcScrollOffset     ; B = CalcScrollOffset(B)
  IF                   SCROLLBAR_ANIMATED   ; conditional assembly
  stb                  m_sbt_offset_next    ; sbt_offset_next = B
  ELSE                                      ; 
  stb                  m_sbt_offset         ; sbt_offset = B
  ENDIF                                     ; end of conditional assembly
  lda                  #8                   ; list_anim_step = 8
  sta                  m_list_anim_step     ; |
  lda                  #-1                  ; list_anim_dir = -1
  sta                  m_list_anim_dir      ; |
  dec                  m_cursor             ; cursor--
  bra                  handle_y_end         ; done

handle_y_zero
  lda                  Vec_Joy_1_X          ; if joy_X != 0
  bne                  handle_y_end         ; | skip
  clr                  m_repeat_counter     ; joy_Y and joy_X == 0, reset repeat counter

handle_y_end

check_buttons
  jsr                  Read_Btns            ; A = Read_Btns()
  anda                 #$0F                 ; A &= 0x0F (ignore joy2)
  beq                  check_buttons2       ; if A != 0
  M_RESET_IDLE                              ; | reset idle counter

check_buttons2
  lsla                                      ; convert to 2-byte index
  ldu                  #button_routines     ; map value using button_routines list
  leau                 a,u                  ; fetch addr of string, page
  pulu                 pc                   ;

dowaitjszero                                ; joystick has been touched
  M_RESET_IDLE                              ; reset idle counter
  inc                  m_repeat_counter     ; repeat_counter++
  lda                  m_repeat_counter     ; if repeat_counter > 24
  cmpa                 #24                  ; | (slightly delay repeating)
  bhs                  rmjsblocker          ; | remove joystick blocker
  ; ignore input until it returns to zero.
  jsr                  Joy_Digital          ; read joystick state
  lda                  Vec_Joy_1_X          ; if joy_X != 0
  lbne                 main_loop            ; | done
  lda                  Vec_Joy_1_Y          ; if joy_Y != 0
  lbne                 main_loop            ; | done

rmjsblocker
  lda                  m_repeat_counter     ; if btn_repeat_cnt & 0x03 != 0
  bita                 #3                   ; | (remove blocker only every 3d
  lbne                 main_loop            ; | loop cycle)
  lda                  #0                   ; waitjs = false
  sta                  m_waitjs             ; | remove joystick blocker
  jmp                  main_loop            ; jump back to main loop


button_routines
  fdb                  nobuttons            ; 0x00 no buttons
  fdb                  do1                  ; 0x01 b1
  fdb                  do2                  ; 0x02 b2
  fdb                  nobuttons            ; 0x03 b2+b1
  fdb                  do3                  ; 0x04 b3
  fdb                  nobuttons            ; 0x05 b3+b1
  fdb                  loaddevmode          ; 0x06 b3+b2
  fdb                  nobuttons            ; 0x07 b3+b2+b1
  fdb                  do4                  ; 0x08 b4
  fdb                  nobuttons            ; 0x09 b4+b1
  fdb                  nobuttons            ; 0x0A b4+b2
  fdb                  nobuttons            ; 0x0B b4+b2+b1
  fdb                  nobuttons            ; 0x0C b4+b3
  fdb                  nobuttons            ; 0x0D b4+b3+b1
  fdb                  nobuttons            ; 0x0E b4+b3+b2
  fdb                  nobuttons            ; 0x0F b4+b3+b2+b1

nobuttons
  jmp                  main_loop            ; skip button handling

do2
  M_IS_FILELIST                             ; if list == filelist
  beq                  pageleft             ; | previous page
  M_JSR_IH             #2                   ; call [I]nput[H]andler with arg=2(button 2)
  jmp                  main_loop            ; jump to main loop

pageleft
  lda                  #-1                  ; load arg for handlepage
  bra                  dopage               ; done

do3
  M_IS_FILELIST                             ; if list == filelist
  beq                  pageright            ; | next page
  M_JSR_IH             #3                   ; call [I]nput[H]andler with arg=3(button 3)
  jmp                  main_loop            ; jump to main loop

pageright
  lda                  #1                   ; load arg for handlepage

dopage
  jsr                  handlepage           ;
  jmp                  main_loop            ;

do4                                         ; start the game
  lda                  m_cursor             ; if cursor < 0
  bmi                  logo_select          ; | cursor is on the logo
  M_IS_FILELIST                             ; if list == filelist
  beq                  startgame            ; | start selected game
  M_JSR_IH             #4                   ; call [I]nput[H]andler with arg=4(button 4)
  jmp                  main_loop            ; jump to main loop

logo_select
  M_IS_FILELIST                             ; if we are on the main screen
  beq                  logo_select_sett     ; | go to settings
  M_JSR_RPC            #18                  ; RPC call to settingsSave()
  ldd                  #filedata            ; load address of main list
  bra                  logo_select_done     ; done

logo_select_sett
  ldd                  #settings_list       ; load address of settings list

logo_select_done
  std                  m_list_ptr           ; store sddress of a list in the current list pointer
  jsr                  init_page            ; reinitialize page after changing pointer
  jmp                  main_loop            ; jump to main loop

startgame
  lda                  m_page               ; Calculate the number of the ROM
  adda                 m_cursor             ; |
  sta                  m_romnumber          ; Stash the ROM number for later
  sta                  RPC_ARG_ADDR + 254   ; Store in special cart location
  M_JSR_RPC            #1                   ; rpc call to doChangeRom()
startgame_hs_cont

check_hs_flag2
  lda                  HIGHSCORE_RF_ADDR    ; Check high score flag
  cmpa                 #$77                 ; Is it LOAD2VEC:0x77 ?
  bne                  startgame_skip_hs    ; No, skip loading high score to Vectrex
                                            ; Yes, fall through and load high score
  ldd                  HIGHSCORE_ADDR + $0  ; 0x3FF0 - 0x3FF5 should have high score
  std                  $cbeb                ; |
  ldd                  HIGHSCORE_ADDR + $2  ; |
  std                  $cbed                ; |
  ldd                  HIGHSCORE_ADDR + $4  ; |
  std                  $cbef                ; |
  lda                  #$0x80               ; |
  sta                  $cbf1                ; |
                                            ; Set high score flag to SAVE2STM:0x88
  ldb                  #$88                 ; | letting the menu know the next
  stb                  HIGHSCORE_WF_ADDR    ; | time it loads to save the high score
  bra                  startgame_load       ; Jump to start game rom

startgame_skip_hs
  lda                  #$66                 ; Set back to IDLE:0x66
  sta                  HIGHSCORE_WF_ADDR    ; |

startgame_load
  lda                  m_romnumber          ; Load our stashed ROM number
  sta                  RPC_ARG_ADDR + 254   ; |
  lda                  #15                  ; rpc call to doStartRom()
  jmp                  m_rpcfn1             ; Call

loaddevmode
  ldd                  #DEVMODE_TEXT_SIZE   ; Set Dev Mode Text Size
  std                  Vec_Text_HW          ; |
  lda                  #5                   ; Load Dev Mode
  sta                  RPC_ARG_ADDR + 254   ; |
  lda                  #10                  ; rpc call initiate VUSB check
  jmp                  m_rpcfn3             ; Goodbye, we won't see you again hopefully!

do1
  M_IS_FILELIST                             ; if is_filelist
  beq                  dirup                ; | go one directory up
  ldx                  m_list_ptr

search_up_ptr_loop
  ldd                  ,x++                 ; loop through list
  bne                  search_up_ptr_loop   ; | find 0 (end of list)
  ldd                  ,x                   ; | after 0 there's pointer to the parent list
  std                  m_list_ptr           ; | (go up in settings)
  jsr                  init_page            ; initialize page
  M_IS_FILELIST                             ; if up is NOT filelist
  lbne                 main_loop            ; | done
  M_JSR_RPC            #18                  ; RPC call to settingsSave()
  jmp                  main_loop            ; done

dirup
  lda                  #3                   ; rpc call to change up a directory
  jmp                  m_rpcfn1             ; Call
; END OF MAIN LOOP / INPUT HANDLING
; END OF MAIN LOOP



;*******************************************************************************
; RPC function for Menu operation - will be copied to RAM - call as rpcfn1
;*******************************************************************************
rpcfndat1
  sta                  $7fff                ;

rpcwaitloop1
  lda                  $0                   ;
  cmpa                 # 'g'                ;
  bne                  rpcwaitloop1         ;
  lda                  $1                   ;
  cmpa                 # ' '                ;
  bne                  rpcwaitloop1         ;
  ldx                  #$11                 ; set up header comparison
  ldu #m_rpcfn1+(vextreme_marker-rpcfndat1) ; relative to ram

headerloop
  lda                  ,x+                  ;
  cmpa                 ,u+                  ;
  bne                  newrom               ;
  cmpx                 #$1A                 ;
  bne                  headerloop           ;
  jsr                  init_page            ;
  jmp                  main_loop            ;

newrom
  jmp                  $f000                ;

vextreme_marker
  fcb                  "VEXTREME",$80       ; for matching against cart header

rpcfndatend1

;***************************************************************************
; RPC function for generic calls - will be copied to RAM - call as rpcfn2
;***************************************************************************
; (needed to skip init_page conditionally)
rpcfndat2
  sta                  $7fff

rpcwaitloop2
  lda                  $0                   ;
  cmpa                 # 'g'                ;
  bne                  rpcwaitloop2         ;
  lda                  $1                   ;
  cmpa                 # ' '                ;
  bne                  rpcwaitloop2         ;
  jmp                  ,x                   ; return address in x

rpcfndatend2

;***************************************************************************
; RPC function for DEV MODE operation - will be copied to RAM - call as rpcfn3
;***************************************************************************
rpcfndat3
  sta                  $7fff                ;

rpcwaitloop3
  jsr                  Wait_Recal           ;
  lda                  Vec_Loop_Count+1     ; Load the MSB of the counter
  adda                 #$30                 ; Give it a bit of offset for first start
                                            ;  so we can see the text initially.
  sta                  m_y_pos              ; save it for text movement
  lda                  m_y_pos              ;

flip_start
  cmpa                 #$7f                 ; y_pos needs to be in reg a
  beq                  direction_flip       ;
  bra                  no_flip              ;

direction_flip
  inc                  m_y_dir              ; used with bita below
  lda                  m_x_pos_cnt          ; 0 - 15 counter
  inca                                      ; |
  anda                 #$f                  ; |
  sta                  m_x_pos_cnt          ; |

no_flip
move_x_a_bit
  lda                  m_x_pos_cnt          ;
  ldx      #m_rpcfn3+(y_pos_vals-rpcfndat3) ; relative to ram
  lda                  a,x                  ;
  sta                  m_x_pos              ;

up_or_down
  lda                  m_y_dir              ;
  bita                 #1                   ;
  beq                  going_up             ;

going_down
  lda                  #$ff                 ;
  suba                 m_y_pos              ;
  sta                  m_y_pos              ;

going_up
breath_with_me
  lda                  m_y_pos              ; reload y_pos
  lsra                                      ; count / 16 to slow down fade
  lsra                                      ; |
  lsra                                      ; |
  lsra                                      ; |
  anda                 #$f                  ; Mask off the value for indexing 16 vals
  ldx  #m_rpcfn3+(intensity_vals-rpcfndat3) ; relative to ram
  lda                  a,x                  ;
  suba                 #$20                 ;
  M_INTENSITY_A                             ;

read_buttons
  jsr                  Read_Btns            ;
  anda                 #$0F                 ;
  cmpa                 #$01                 ;
  beq                  do_button1           ;
  cmpa                 #$08                 ;
  beq                  do_button4           ;

no_button
  lda                  #1                   ;
  sta                  m_pressed_none       ;
  bra                  button_exit          ;

do_button1
  lda                  #1                   ;
  sta                  m_pressed_exit       ;
  sta                  m_ramdisk_once       ; short circuit these so we start looping
  sta                  m_vusb_once          ; |
  lda                  #0                   ;
  sta                  m_pressed_none       ;
  bra                  button_exit          ;

do_button4
  lda                  #1                   ;
  sta                  m_pressed_run        ;
  sta                  m_ramdisk_once       ; short circuit these so we start looping
  sta                  m_vusb_once          ; |
  lda                  #0                   ;
  sta                  m_pressed_none       ;

button_exit
check_vusb
  lda                  m_vusb_once          ; Have we completed vusb checks?
  cmpa                 #1                   ; |
  beq                  start_ramdisk        ; | Yes, proceed to starting the ramdisk
  lda                  VUSB_ADDR            ; | NO, read VUSB value ($99:high=USB, $66:low=NO-USB,
  cmpa                 #$66                 ; any other val and the RPC hasn't returned yet)
  lbeq                 rpcreadvusb          ; keep reading VUSB
  cmpa                 #$99                 ; Yes, we have VUSB!
  beq                  finish_vusb          ; | finish vusb checks
  jmp                  rpcreadvusb1         ; Else, unknown USB state

finish_vusb
  inc                  m_vusb_once          ;

start_ramdisk
  lda                  m_ramdisk_once       ; Have we loaded the ramdisk yet?
  cmpa                 #1                   ; |
  beq                  ramdisk_loaded       ; | Yes, just loop now
  jmp                  rpcdevwait           ; | No, let's get this ramdisk party started

ramdisk_loaded
  lda                  m_pressed_run        ;
  cmpa                 #1                   ;
  beq                  skip_cart_msg        ;
  lda                  m_pressed_exit       ;
  cmpa                 #1                   ;
  beq                  skip_cart_msg        ;

switch_to_help_msg
  lda                  m_y_dir              ; unrolled print_sub, because vec was crashing
  bita                 #1                   ;
  beq                  load_cart_msg        ;
  ldu        #m_rpcfn3+(help_str-rpcfndat3) ; relative to ram
  bra                  show_help_msg        ;

load_cart_msg
  ldu  #m_rpcfn3+(usbdevmode_str-rpcfndat3) ; needs to be relative to where it's copied in ram

show_help_msg
  lda                  m_y_pos              ;
  ldb                  m_x_pos              ;
  jsr                  Print_Str_d          ;

skip_cart_msg
ramdisk_yielded
  lda                  RAMDISK_YIELD1_ADDR  ; Every 1s, the ramdisk will yield
  cmpa                 # 'v'                ; to the Vectrex
  bne                  load_cart            ; so we can either let it know to
  lda                  RAMDISK_YIELD2_ADDR  ; keep ramdisk waiting / exit or run
  cmpa                 # 'x'                ; This byte sequence let's us know to
  bne                  load_cart            ; Give 1 of the 3 answers in response

act_on_buttons
  lda                  m_pressed_none       ;
  cmpa                 #1                   ;
  lbne                 rpcwaitloop3         ;
  lda                  m_pressed_run        ;
  cmpa                 #1                   ;
  beq                  rpcdevrun            ;
  lda                  m_pressed_exit       ;
  cmpa                 #1                   ;
  beq                  rpcdevexit           ;

rpcdevwait
  lda                  #1                   ;
  sta                  m_ramdisk_once       ;
  lda                  #0                   ;
  sta                  RPC_ARG_ADDR + 254   ;
  lda                  #10                  ;
  jmp                  m_rpcfn3             ;

rpcdevrun
  lda                  #4                   ;
  sta                  RPC_ARG_ADDR + 254   ;
  lda                  #10                  ;
  jmp                  m_rpcfn3             ;

rpcdevexit
  lda                  #1                   ;
  sta                  RPC_ARG_ADDR + 254   ;
  lda                  #10                  ;
  jmp                  m_rpcfn3             ;

load_cart
  lda                  $0                   ;
  cmpa                 # 'g'                ;
  lbne                 rpcwaitloop3         ;
  lda                  $1                   ;
  cmpa                 # ' '                ;
  lbne                 rpcwaitloop3         ;
  jmp                  $f000                ; warm boot address to reset the cart.bin that was just loaded

rpcreadvusb
  ldu   #m_rpcfn3+(pluginusb_str-rpcfndat3) ; relative to ram
  jsr                  print_sub            ;
  lda                  m_y_pos              ;
  anda                 #$10                 ;
  cmpa                 #$10                 ;
  bne                  rpcreadvusb1_exit    ; only read the VUSB every 320ms, no need to spam it
  lda                  #5                   ;
  sta                  RPC_ARG_ADDR + 254   ;
  lda                  #10                  ;
  jmp                  m_rpcfn3             ;

rpcreadvusb_exit
  jmp                  rpcwaitloop3         ;

rpcreadvusb1
  ldu  #m_rpcfn3+(unknownusb_str-rpcfndat3) ; relative to ram
  jsr                  print_sub            ;
  lda                  m_y_pos              ;
  anda                 #$10                 ;
  cmpa                 #$10                 ;
  bne                  rpcreadvusb1_exit    ; only read the VUSB every 320ms, no need to spam it
  lda                  #5                   ;
  sta                  RPC_ARG_ADDR + 254   ;
  lda                  #10                  ;
  jmp                  m_rpcfn3             ;

rpcreadvusb1_exit
  jmp                  rpcwaitloop3

; ====== RAM SUBROUTINES ======
print_sub
  lda                  m_y_dir              ;
  bita                 #1                   ;
  beq                  print_exit           ;
  ldu       #m_rpcfn3+(help_str-rpcfndat3)  ; relative to ram

print_exit
  lda                  m_y_pos              ;
  ldb                  m_x_pos              ;
  jsr                  Print_Str_d          ;
  rts

; ====== RAM DATA ======
usbdevmode_str
  fcb                  " LOAD CART.BIN", $80

pluginusb_str
  fcb                  "  PLUG IN USB", $80

unknownusb_str
  fcb                  " UNKNOWN USB", $80

help_str
  fcb                  "1: EXIT  4: RUN", $80

intensity_vals
  fcb                  $78,$68,$58,$48,$38,$28,$18,$18,$18,$18,$28,$38,$48,$58,$68,$78

y_pos_vals
  fcb                  $90,$80,$98,$8E,$8A,$9A,$88,$9C,$94,$8C,$92,$96,$86,$84,$82,$9D

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
  pshs                 a,b
  lbeq                 skipxmove
  bpl                  xneg
  tst                  m_page               ; if page < 0
  ble                  xmovedone            ; done
  lda                  m_page               ;
  suba                 m_max_lines          ; a = page - MENU_ITEMS_MAX
  tsta                                      ; if a < 0
  blt                  >                    ; page = 0, done
  sta                  m_page               ; page = a (decrease page)
  bra                  xmovedone            ; done
! clr                  m_page               ; 
  bra                  xmovedone
xneg
  lda                  m_page               ; a = page
  adda                 m_max_lines          ; a += MENU_ITEMS_MAX
  cmpa                 m_list_size          ; if a >= list_size
  bge                  xmovedone            ; done
  bra                  donextpage           ; else (increase page)

donextpage          
  lda                  m_max_lines          ;
  adda                 m_page               ;
  sta                  m_page               ;
  lda                  m_list_size          ;
  suba                 m_max_lines          ;
  cmpa                 m_page               ;
  bgt                  xmovedone            ;
  sta                  m_page               ;

xmovedone
  clr                  m_cursor             ;
  ldb                  m_page               ;
  jsr                  CalcScrollOffset     ;
  stb                  m_sbt_offset         ;
  stb                  m_sbt_offset_next    ;
  ldb                  #1                   ;
  stb                  m_waitjs             ;

skipxmove
  puls                 a,b                  ;
  rts                                       ;

;***************************************************************************
; handlepage END
;***************************************************************************
;***************************************************************************
; init_page START
;***************************************************************************
init_page
  clr                  m_page               ;
  M_IS_FILELIST                             ;
  bne                  init_page2           ;
  ldb                  m_last_cursor        ; page = last_cursor
  stb                  m_page               ; |

init_page2
  clr                  m_list_anim_dir      ; clear
  clr                  m_list_anim_step     ; |
  clr                  m_cursor             ; |
  clr                  m_list_size          ; |
  
  ldu                  m_list_ptr           ; U = list_ptr
  ; calc list Y pos
  lda                  m_max_lines          ; list_pos_y = (max_lines * 10) + 18
  ldb                  #10                  ; |
  mul                                       ; |
  addb                 #18                  ; |
  stb                  m_list_pos_y         ; |

is_not_last_page
  inc                  m_list_size          ;
  ldb                  ,u++                 ;
  cmpb                 #0                   ;
  bne                  is_not_last_page     ;
  dec                  m_list_size          ;

  M_IS_FILELIST                             ;
  bne                  calc_thumb_height    ;
  lda                  m_list_size          ; if list_size - max_lines >= page
  suba                 m_max_lines          ; |
  cmpa                 m_page               ; |
  bhs                  calc_thumb_height    ; | skip
  sta                  m_page               ; page = list_size - max_lines
  ldb                  m_last_cursor        ; cursor = last_cursor - page
  subb                 m_page               ; |
  stb                  m_cursor             ; |

calc_thumb_height
  clra                                      ;
  ldb                  m_max_lines          ;
  std                  m_tmp_d              ;
  clr                  m_sbt_height         ;
  clr                  m_sbt_offset         ;
  clr                  m_sbt_offset_next    ;
  lda                  #1                   ;
  sta                  m_sbt_offset_step    ; offset_st = 1
  lda                  #0                   ;
  ldb                  m_list_size          ;
  cmpb                 m_max_lines          ; if list_size < max_lines
  bgt                  more_than_one_page   ; | go to scrollbar calculation
  rts                                       ; | else done
                                            
more_than_one_page
  M_LSLD                                    ; list_size * 16
  M_LSLD                                    ; |
  M_LSLD                                    ; |
  M_LSLD                                    ; |
  M_DIV_D_TO_B         m_tmp_d              ; / max_lines
  lda                  #0                   ;
  std                  m_tmp_d              ;
  lda                  #0                   ;
  ldb                  #SCROLLBAR_TRACK_H   ;
  M_LSLD                                    ; SCROLLBAR_TRACK * 16
  M_LSLD                                    ; |
  M_LSLD                                    ; |
  M_LSLD                                    ; |
  M_DIV_D_TO_B         m_tmp_d              ; / (list_size / max_lines)
  stb                  m_sbt_height         ;
  ; calculate initial offset
  ldb                  m_page               ; B = CalcScrollOffset(page)
  jsr                  CalcScrollOffset     ; |
  stb                  m_sbt_offset         ; sbt_offset = B
  stb                  m_sbt_offset_next    ; sbt_offset_next = B
  ; Conditional assembly
  IF                   SCROLLBAR_ANIMATED   ;
  ; calculate offset animation step
  ; @TODO: possible bug in case if page == list_size
  ldb                  m_page               ; B = CalcScrollOffset(page + 1)
  addb                 #1                   ; |
  jsr                  CalcScrollOffset     ; |
  jsr                  CalcScrollOffsetStep ; sbt_offset_step = CalcScrollOffsetStep(B)
  ENDIF
  rts

;***************************************************************************
; init_page END
;***************************************************************************

;***************************************************************************
; CODE SECTION
;***************************************************************************
  include  "draw.asm"  
  include  "list.asm"  
  include  "stars.asm"  
;***************************************************************************
; DATA SECTION
;***************************************************************************
  include  "settings.i"
  include  "vector_font.i"
  include  "logo.i"

; VEXTREME Tune Notes
CS5                    equ  $1E
F5                     equ  $22
FS5                    equ  $23
GS5                    equ  $25
AS5                    equ  $27
RST                    equ  $3F
VIBENL                 fcb  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
FADE14                 fdb  $0000,$2DDD,$DDDD,$B000,0,0,0,0

; VEXTREME Intro Tune
vextreme_tune1
  fdb                  FADE14
  fdb                  VIBENL
  fcb                  FS5,8
  fcb                  FS5,8
  fcb                  FS5,8
  fcb                  AS5,16
  fcb                  GS5,16
  fcb                  FS5,16
  fcb                   F5,16
  fcb                  CS5,8
  fcb                  FS5,8
  fcb                  RST,8
  fcb                  0,$80                ; $80 is end marker for music, frequency is not played so 0


; STAR SCREENSAVER DATA
stars_speed_table
  fdb                  160,160,160,161,161,161,162,163,164,165,166,167,168,170
  fdb                  171,173,175,177,179,181,183,186,188,191,194,196,199,202
  fdb                  206,209,212,216,219,223,227,231,235,239,243,248,252,257
  fdb                  261,266,271,276,281,286,291,297,302,308,313,319,325,331
  fdb                  337,343,349,356,362,368,375,382,388,395,402,409,416,423
  fdb                  430,438,445,452,460,467,475,483,491,498,506,514,522,530
  fdb                  539,547,555,563,572,580,589,597,606,614,623,632,641,650
  fdb                  658,667,676,685,694,703,712,722,731,740,749,758,768,777
  fdb                  786,796,805,814,824,833,843,852,862,871,881,890,900,909
  fdb                  919,928

;***************************************************************************
; MENU DATA SECTION
;***************************************************************************
; Test menu list data. This gets overwritten by the firmware running in the
; STM with actual cartridge data.
  org                  MENU_MEM_ADDR
  IF                   !SETTINGS_IN_RAM
m_max_lines
  fcb                  5

m_max_chars
  fcb                  8

m_led_mode
  fcb                  1

m_led_luma
  fcb                  5

m_led_red
  fcb                  31

m_led_green
  fcb                  0

m_led_blue
  fcb                  31

m_ss_mode
  fcb                  1

m_ss_delay
  fcb                  1

m_last_cursor
  fcb                  0

m_directory
  fcb                  0
  ENDIF

  org                  MENU_DATA_ADDR
filedata
  fdb                  text0
  fdb                  text1
  fdb                  text2
  fdb                  text3
  fdb                  text4
  fdb                  text5
  fdb                  text6
  fdb                  text7
  fdb                  text8
  fdb                  text9
  fdb                  text10
  fdb                  text11
  fdb                  text12
  fdb                  text13
  fdb                  text14
  fdb                  text15
  fdb                  text16
  fdb                  text17
  fdb                  text18
  fdb                  text19
  fdb                  font1
  fdb                  font2
  fdb                  font3
  fdb                  font4
  fdb                  font5
  fdb                  font6
  fdb                  font7
  fdb                  font8
  fdb                  font9
  fdb                  font10
  fdb                  font11
  fdb                  font12
  fdb                  0

text0
  fcb                  "<CART 1>",$80
  
text1
  fcb                  "CART 2",$80
  
text2
  fcb                  "CART 3",$80
  
text3
  fcb                  "CART 4",$80
  
text4
  fcb                  "DE VIERDE UNIT",$80
  
text5
  fcb                  "EENTJE MET EEN LANGE NAAM DUS",$80
  
text6
  fcb                  "BEN IK HET AL ZAT?",$80
  
text7
  fcb                  "LALALA",$80
  
text8
  fcb                  "OMGWTFBB",$80
  
text9
  fcb                  "GNORK",$80
  
text10
  fcb                  "EENTJE MET EEN LANGE NAAM DUS",$80
  
text11
  fcb                  "BEN IK HET AL ZAT?",$80
  
text12
  fcb                  "LALALA OMG",$80
  
text13
  fcb                  "OMGWTFBBQ",$80
  
text14
  fcb                  "GNORK OMGWTFBBQ",$80
  
text15
  fcb                  "GNORK2 OMGWTFBBQ",$80
  
text16
  fcb                  "GNORK3 OMGWTFBBQ",$80
  
text17
  fcb                  "GNORK4 OMGWTFBBQ",$80
  
text18
  fcb                  "OMGWTFBBQ",$80
  
text19
  fcb                  "GNORK",$80
  
font1
  fcb                  "! \"#$%&",$80
  
font2
  fcb                  "`()*+,-./",$80
  
font3
  fcb                  "0123456789",$80
  
font4
  fcb                  ":;<=>?@",$80
  
font5
  fcb                  "ABCDEFGH",$80
  
font6
  fcb                  "IJKLMNOP",$80
  
font7
  fcb                  "QRSTUVW",$80
  
font8
  fcb                  "YZ[\]^_",$80
  
font9
  fcb                  "abcdefgh",$80
  
font10
  fcb                  "ijklmnop",$80
  
font11
  fcb                  "qrstuvw",$80
  
font12
  fcb                  "xyz{|}~",$80
  
