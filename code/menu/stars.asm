;*******************************************************************************
; FAKE 3D STARFIELD by C.MALCOLM
;*******************************************************************************
; A quick and dirty pseudo-3D starfield
;*******************************************************************************
; Adapted as screensavaer by Vasily Kiniv
;*******************************************************************************

StarsInit
  jsr                  dotsInit

stars_main_loop
  jsr                  Wait_Recal           ; Vectrex BIOS recalibration
  jsr                  Reset0Ref            ;
  jsr                  Intensity_7F         ; set beam intensity 
  jsr                  Joy_Analog           ; read analog stick
  jsr                  dotsDraw             ; draw stars
  jsr                  dotsMove             ; move stars
  ; exit from loop and go to menu if user pressed button 1-4
  jsr                  Read_Btns            ; 
  anda                 #$0F                 ; ignore second joystick
  beq                  stars_main_loop      ; no button pressed, loop

;*******************************************************************************
;this procudeure initialises the star data
dotsInit
  ;init loop
  lda                  #STARS_NUMBER
  sta                  m_temp_b1
  ldx                  #m_dots_data

dotsInitLp
  ; random star position, speed
  jsr                  dotRandom

  ; Y
  lda                  m_dot_y
  sta                  ,x+

  ; X
  lda                  m_dot_x
  sta                  ,x+

  ; Z (Random 0-127)
  jsr                  Random
  anda                 #127
  ldb                  #0
  std                  ,x++

  ; ZSpd
  ldd                  m_dot_z_speed
  std                  ,x++

  ;loop?
  dec                  m_temp_b1
  lda                  m_temp_b1
  cmpa                 #0
  bne                  dotsInitLp

  rts

;*******************************************************************************
;draw our stars
dotsDraw
  jsr                  Reset0Ref            ; reset beam
  jsr                  Intensity_7F         ; set beam intensity 

  ;init loop
  lda                  #STARS_NUMBER
  sta                  m_temp_b1
  ldx                  #m_dots_data

dotsDrawLp
  ;load bytes
  lda                  ,x+
  sta                  m_dot_y
  lda                  ,x+
  sta                  m_dot_x
  ldd                  ,x++
  std                  m_dot_z
  ldd                  ,x++
  std                  m_dot_z_speed

  jsr                  dotDraw

  ;loop?
  dec                  m_temp_b1
  lda                  m_temp_b1
  cmpa                 #0
  bne                  dotsDrawLp

  rts

;*******************************************************************************
; draw each star
;*******************************************************************************
dotDraw
  ; avoids a cluster of dots appearing at the center of the screen - adjust to taste
  lda                  m_dot_z
  cmpa                 #15
  ble                  dotDrawExit  
  jsr                  Reset0Ref            ; goto 0,0

  ; set scale factor (this scales the coordinates, giving the appearance of 3D)
  lda                  m_dot_z
  sta                  VIA_t1_cnt_lo

  ;set dot intensity - dots closer to the player (higher Z value) get brighter - adjust to taste 
  lda                  m_dot_z
  jsr                  Intensity_a          ; set beam intensity 

  ;move to position
  lda                  m_dot_y              ; to 0 (y)
  ldb                  m_dot_x              ; to 0 (x)
  jsr                  Moveto_d             ; move the vector beam the
  jsr                  Dot_here             ; draw dot

dotDrawExit
  rts


;*******************************************************************************
; move our stars
;*******************************************************************************
dotsMove
  jsr                  Reset0Ref            ;
  jsr                  Intensity_7F         ; set beam intensity 
  jsr                  dotsJoytoW2          ; get speed from joystick

  ; init loop
  lda                  #STARS_NUMBER
  sta                  m_temp_b1
  ldx                  #m_dots_data
  stx                  m_temp_w1

dotsMoveLp
  ldx                  m_temp_w1            ; restore pointer

  ;load variables with data bytes
  lda                  ,x+
  sta                  m_dot_y
  lda                  ,x+
  sta                  m_dot_x
  ldd                  ,x++
  std                  m_dot_z
  ldd                  ,x++
  std                  m_dot_z_speed

  jsr                  dotMove              ; perform move
  ldx                  m_temp_w1            ; restore pointer + overwrite data

  ; save
  lda                  m_dot_y
  sta                  ,x+
  lda                  m_dot_x
  sta                  ,x+
  ldd                  m_dot_z
  std                  ,x++
  ldd                  m_dot_z_speed
  std                  ,x++
  stx                  m_temp_w1

  ;loop?
  dec                  m_temp_b1
  lda                  m_temp_b1
  cmpa                 #0
  bne                  dotsMoveLp

  rts

;*******************************************************************************
; move a star
;*******************************************************************************
dotMove
  jsr                  dotMoveLR            ; move left/right - depending on joystick position
  jsr                  dotZtoW3             ; get z axis speed modifier from table (speeds up the star as it moves towards the 'player' on the Z axis)
  ;moving backwards?
  lda                  Vec_Joy_1_Y
  cmpa                 #0
  blt                  dotMoveNeg

dotMovePos
  ldd                  m_dot_z              ; move
  addd                 m_temp_w3            ; z axis speed modifier
  addd                 m_dot_z_speed        ; star speed
  addd                 m_temp_w2            ; add joystick y pos
  std                  m_dot_z

dotMove2
  ;outside screen? - add new star
  lda                  m_dot_z
  cmpa                 #0
  blt                  dotRandom
  rts

dotMoveNeg
  ; move (reverse)
  ldd                  m_dot_z
  subd                 m_temp_w3            ; z axis speed modifier
  subd                 m_dot_z_speed        ; star speed
  subd                 m_temp_w2            ; subtract joystick y pos
  std                  m_dot_z
  bra                  dotMove2

;*******************************************************************************
; scroll starfield left/right
;*******************************************************************************
dotMoveLR
  ;moving left?
  lda                  Vec_Joy_1_X
  cmpa                 #-32
  blt                  dotMoveL
  lda                  Vec_Joy_1_X
  cmpa                 #32
  bgt                  dotMoveR
  rts

dotMoveR
  lda                  m_dot_x
  suba                 #8
  sta                  m_dot_x
  rts

dotMoveL
  lda                  m_dot_x
  adda                 #8
  sta                  m_dot_x
  rts

;*******************************************************************************
; random star
;*******************************************************************************
dotRandom
  jsr                  dotRandomPos

  ; moving backwards?
  lda                  Vec_Joy_1_Y
  cmpa                 #0
  blt                  dotRandomZRev

  ; reset Z pos
  ldd                  #0
  std                  m_dot_z

dotRandomSpd
  ; Random Z Spd (-128to127)
  jsr                  Random
  tfr                  a,b
  sex
  std                  m_dot_z_speed
  rts

dotRandomZRev
  ; reset Z pos
  lda                  #127
  sta                  m_dot_z
  ; random speed
  bra                  dotRandomSpd

;*******************************************************************************
dotRandomPos
  ; 50/50 chance of being 'true' random pos or random at edge (gives better screen coverage) - adjust to taste
  jsr                  Random
  cmpa                 #0
  blt                  dotRandomPosNew

  ; truly random pos
  jsr                  Random_3
  sta                  m_dot_y
  jsr                  Random_3
  sta                  m_dot_x
  rts

;*******************************************************************************
; random pos (at edge of screen)
;*******************************************************************************
dotRandomPosNew
  ;random side of screen
  jsr                  Random
  anda                 #3
  cmpa                 #1
  beq                  dotRandomPosTop
  cmpa                 #2
  beq                  dotRandomPosLeft
  cmpa                 #3
  beq                  dotRandomPosRight

dotRandomPosBottom
  lda                  #-127
  sta                  m_dot_y
  jsr                  Random
  sta                  m_dot_x
  rts

dotRandomPosTop
  lda                  #127
  sta                  m_dot_y
  jsr                  Random
  sta                  m_dot_x
  rts

dotRandomPosLeft
  lda                  #-127
  sta                  m_dot_x
  jsr                  Random
  sta                  m_dot_y
  rts

dotRandomPosRight
  lda                  #127
  sta                  m_dot_x
  jsr                  Random
  sta                  m_dot_y
  rts

;*******************************************************************************
; get z speed modifier from table and store in m_temp_w3
;*******************************************************************************
dotZtoW3
  ldx                  #stars_speed_table
  lda                  #2
  ldb                  m_dot_z
  mul
  abx
  ldd                  ,x
  std                  m_temp_w3
  rts

dotsJoytoW2
; get speed modifier from joystick y axis and store in m_temp_w2
  ; calc joystick speed
  lda                  Vec_Joy_1_Y
  ldb                  #4
  mul
  ; store
  std                  m_temp_w2
  rts