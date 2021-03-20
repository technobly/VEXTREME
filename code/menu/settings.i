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
; LIST OF INPUTS EXAMPLE
;*******************************************************************************
;list_name
; fdb                  pointer              <- pointer to input widget
; fdb                  pointer              <- |
; fdb                  ...                  <- |
; fdb                  0                    <- end of list marker

;*******************************************************************************
; NUMBER INPUT EXAMPLE
;*******************************************************************************
;numeric_input_name
; fcb                  "LABEL FOR INPUT"
; fcb                  W_NUM                <- number input marker
; fcb                  0                    <- minimal value
; fcb                  255                  <- maximum value
; fdb                  pointer              <- pointer to the 8bit number in 
;                                              RAM or ROM to be changed by 
;                                              input widget
; fdb                  0                    <- pointer to the subroutine which will
;                                              be called after value was changed
;                                              (must be set to 0 if not used)

;*******************************************************************************
; SELECT INPUT EXAMPLE
;*******************************************************************************
;select_input_name
; fcb                  "LABEL FOR INPUT"
; fcb                  W_SELECT             <- select input marker
; fdb                  pointer              <- pointer to the 8bit index of the
;                                              option in RAM or ROM to be changed
;                                              by input widget
; fdb                  option0              <- pointer to the $80 terminated string
; fdb                  option1              <- |
; fdb                  ...                  <- |
; fdb                  0                    <- end of options list marker

;*******************************************************************************
; LINK INPUT EXAMPLE
;*******************************************************************************
;link_input_name
; fcb                  "LABEL FOR INPUT"
; fcb                  W_LINK               <- link input marker
; fdb                  pointer              <- pointer to the another list of inputs



;*******************************************************************************
; SETTINGS SCREEN
;*******************************************************************************
settings_list
  fdb                  sl_lines
  fdb                  sl_chars
  fdb                  sl_screensaver
  fdb                  sl_led
  fdb                  sl_back
  fdb                  0
  fdb                  filedata

sl_lines
  fcb                  "LINES", W_NUM, 3, 6
  fdb                  #m_max_lines
  fdb                  #init_page

sl_chars
  fcb                  "CHARACTERS", W_NUM, 4, 16
  fdb                  #m_max_chars
  fdb                  0

sl_led
  fcb                  "LED", W_LINK
  fdb                  #settings_led_list

sl_screensaver
  fcb                  "SCREEN SAVER", W_LINK
  fdb                  #settings_ss_list

sl_back
  fcb                  "EXIT",  W_LINK
  fdb                  #filedata

;*******************************************************************************
; SETTINGS->SCREENSAVER
;*******************************************************************************
settings_ss_list
  fdb                  ssl_mode
  fdb                  ssl_delay
  fdb                  ssl_back
  fdb                  0
  fdb                  settings_list

ssl_mode
  fcb                  "MODE", W_SELECT
  fdb                  #m_ss_mode
  fdb                  ssl_mode_off
  fdb                  ssl_mode_stars
  fdb                  0

ssl_mode_off       
  fcb                  "OFF", $80

ssl_mode_stars         
  fcb                  "STARS", $80

ssl_delay
  fcb                  "MINUTES", W_NUM, 1, 120
  fdb                  #m_ss_delay
  fdb                  0

ssl_back
  fcb                  "BACK", W_LINK
  fdb                  settings_list

;*******************************************************************************
; SETTINGS->LED
;*******************************************************************************
settings_led_list
  fdb                  sll_mode
  fdb                  sll_luma
  fdb                  sll_color
  fdb                  sll_back
  fdb                  0
  fdb                  settings_list

sll_mode
  fcb                  "MODE", W_SELECT
  fdb                  #m_led_mode
  fdb                  sll_mode_off
  fdb                  sll_mode_rainbow
  fdb                  sll_mode_color
  fdb                  0

sll_mode_off           
  fcb                  "OFF", $80

sll_mode_rainbow       
  fcb                  "RAINBOW", $80

sll_mode_color         
  fcb                  "COLOR", $80

sll_luma
  fcb                  "LUMA", W_NUM, 0, 31
  fdb                  #m_led_luma
  fdb                  0

sll_color
  fcb                  "COLOR", W_LINK
  fdb                  settings_led_color_list

sll_back
  fcb                  "BACK", W_LINK
  fdb                  settings_list

;*******************************************************************************
; SETTINGS->LED->COLOR
;*******************************************************************************
settings_led_color_list
  fdb                  slcl_red
  fdb                  slcl_green
  fdb                  slcl_blue
  fdb                  slcl_back
  fdb                  0
  fdb                  settings_led_list

slcl_red
  fcb                  "RED", W_NUM, 0, 31
  fdb                  #m_led_red
  fdb                  0

slcl_green
  fcb                  "GREEN", W_NUM, 0, 31
  fdb                  #m_led_green
  fdb                  0

slcl_blue
  fcb                  "BLUE", W_NUM, 0, 31
  fdb                  #m_led_blue
  fdb                  0

slcl_back
  fcb                  "BACK", W_LINK
  fdb                  settings_led_list