;*******************************************************************************
; vxt_input.asm - VXT toolkit: controller input reporting (6809 side)
;
; Reads the joystick(s) and buttons via BIOS and reports them to the STM32 as
; a fixed block of RAW bytes in parmRam. The STM32 owns all game logic; this
; module only reports (the VOOM model).
;
; DESIGN: RAW, not pre-DIGITIZED.
; Vol. 2 confirms Joy_Digital (JOYBIT, $F1F8) and Joy_Analog (JOYSTK, $F1F5)
; are drop-in interchangeable from the caller's side: identical entry values
; (DP=$D0, EPOT0-3 enables, POTRES) and identical result locations
; (POT0-3 = Vec_Joy_1_X/_1_Y/_2_X/_2_Y, $C81B-$C81E). JOYSTK even lists JOYBIT
; among the subroutines it uses. Only the VALUES differ: JOYBIT yields a coarse
; direction, JOYSTK the absolute position.
;
; Therefore this module reports Vec_Joy_* verbatim as SIGNED bytes and lets the
; caller choose which BIOS routine fills them. A digital game tests the
; sign on the STM32 side; an analog game uses the magnitude. Same wire format,
; no information discarded. (An earlier draft packed direction into flag bits -
; that would have made analog games impossible to build on this module.)
;
; parmRam input block (6809 -> STM32; write-only from the 6809).
; Mirrored by code/stm32/vxt/vxt_input.h - keep the two in step.
;   $7FFE parmRam[254] = Vec_Joy_1_X   (SIGNED)
;   $7FFD parmRam[253] = Vec_Joy_1_Y   (SIGNED)
;   $7FFC parmRam[252] = Vec_Joy_2_X   (SIGNED; only meaningful if ctrl 2 enabled)
;   $7FFB parmRam[251] = Vec_Joy_2_Y   (SIGNED; ditto)
;   $7FFA parmRam[250] = Vec_Button_1_1
;   $7FF9 parmRam[249] = Vec_Button_1_2
;   $7FF8 parmRam[248] = Vec_Button_1_3
;   $7FF7 parmRam[247] = Vec_Button_1_4
;   $7FF6 parmRam[246] = Vec_Btn_State (raw bitmask, all buttons)
;   $7FF5 parmRam[245] = EDGE (newly-pressed transitions; Read_Btns' A return)
; The per-button bytes AND the raw bitmask are both reported deliberately:
; vectrex.i names the per-button bytes unambiguously, whereas the bit positions
; within Vec_Btn_State are not established in the available documentation - so
; the bitmask is offered without any claim about which bit is which button.
; Does not collide with upstream's parmRam usage (main.c reads [254], [253] and
; [240]-[243], and only within its own RPC handlers, which apps never invoke).
;
; USAGE:
;     include "../vxt/vxt_input.asm"     ; anywhere after setdp #$d0
;     ...
;   main
;     VXT_RPC_INIT
;     VXT_INPUT_INIT                     ; once at startup
;   loop
;     jsr Wait_Recal
;     VXT_INPUT_READ Joy_Digital         ; ...or: VXT_INPUT_READ Joy_Analog
;     VXT_RPC <id>                       ; STM32 reads the block, writes a frame
;     ldx #$0800
;     jsr vxt_draw
;     bra loop
;
; Ordinary cart-resident code - not copied to RAM. Requires DP=$D0 at runtime
; (set by VXT_RPC_INIT) - every BIOS routine used here documents DP=$D0 as an
; entry value (Vol. 2: JOYBIT, JOYSTK, INPUT).
;*******************************************************************************

; --- parmRam input block ------------------------------------------------------
VXT_IN_JOY1X           equ    $7ffe
VXT_IN_JOY1Y           equ    $7ffd
VXT_IN_JOY2X           equ    $7ffc
VXT_IN_JOY2Y           equ    $7ffb
VXT_IN_BTN1_1          equ    $7ffa
VXT_IN_BTN1_2          equ    $7ff9
VXT_IN_BTN1_3          equ    $7ff8
VXT_IN_BTN1_4          equ    $7ff7
VXT_IN_BTNS            equ    $7ff6
VXT_IN_EDGE            equ    $7ff5

;*******************************************************************************
; VXT_INPUT_INIT - set the BIOS joystick enable/mux bytes and resolution.
; Call once at startup (after VXT_RPC_INIT, which establishes DP=$D0).
;
; Defaults: controller 1 enabled, controller 2 DISABLED (saves the BIOS two pot
; reads per frame), POTRES = $80 (minimum resolution = fastest read).
; These are plain BIOS RAM bytes - an app needing controller 2, or finer analog
; resolution, overwrites Vec_Joy_Mux_2_X/_2_Y (to $05/$07) or
; Vec_Joy_Resltn (toward $00 = maximum resolution) after calling this.
; Values per Vol. 2's JOYBIT/JOYSTK entry requirements; $80/$00 resolution
; semantics per vectrex.i's Vec_Joy_Resltn comment.
;
; NOTE: VOOM never sets any of these and still works, so the BIOS power-up
; defaults are evidently usable - but Vol. 2 documents them as required entry
; values, so the toolkit sets them explicitly rather than relying on defaults.
; DESTROYS: A
;*******************************************************************************
VXT_INPUT_INIT         macro
  lda                  #$01
  sta                  Vec_Joy_Mux_1_X      ; EPOT0 = $01 : ctrl 1 Right/Left ON
  lda                  #$03
  sta                  Vec_Joy_Mux_1_Y      ; EPOT1 = $03 : ctrl 1 Up/Down    ON
  clr                  Vec_Joy_Mux_2_X      ; EPOT2 = $00 : ctrl 2            OFF
  clr                  Vec_Joy_Mux_2_Y      ; EPOT3 = $00 : ctrl 2            OFF
  lda                  #$80
  sta                  Vec_Joy_Resltn       ; POTRES: $80 = min res (fastest)
  endm

;*******************************************************************************
; VXT_INPUT_READ <joy_routine> - read all inputs and report them to parmRam.
;   \1 = Joy_Digital ($F1F8) or Joy_Analog ($F1F5). Interchangeable (see header).
; Call once per frame, before the RPC that asks the STM32 for the next frame.
; DESTROYS: A, B, X (all three BIOS routines destroy registers; Read_Btns
;           returns X = $C81A, which we don't use)
;*******************************************************************************
VXT_INPUT_READ         macro
  jsr                  \1                   ; fills Vec_Joy_1_X/_1_Y (/_2_* if
                                             ; controller 2 is enabled)
  lda                  Vec_Joy_1_X
  sta                  VXT_IN_JOY1X
  lda                  Vec_Joy_1_Y
  sta                  VXT_IN_JOY1Y
  lda                  Vec_Joy_2_X
  sta                  VXT_IN_JOY2X
  lda                  Vec_Joy_2_Y
  sta                  VXT_IN_JOY2Y

  jsr                  Read_Btns            ; A = EDGE; fills Vec_Btn_State and
                                             ; the Vec_Button_1_* bytes
  sta                  VXT_IN_EDGE          ; A still holds EDGE here
  lda                  Vec_Button_1_1
  sta                  VXT_IN_BTN1_1
  lda                  Vec_Button_1_2
  sta                  VXT_IN_BTN1_2
  lda                  Vec_Button_1_3
  sta                  VXT_IN_BTN1_3
  lda                  Vec_Button_1_4
  sta                  VXT_IN_BTN1_4
  lda                  Vec_Btn_State
  sta                  VXT_IN_BTNS

  ; Read_Btns is documented (Vol. 2, INPUT) to modify DDAC - i.e. VIA_DDR_a
  ; ($D003), the register that must have Port A as OUTPUT for the DAC to drive
  ; beam coordinates. VOOM never calls Read_Btns, so we have no proven
  ; precedent that the BIOS restores it. Stock games read buttons then draw
  ; every frame, so it very likely does - but the toolkit forces it back rather
  ; than depend on an unverified assumption. Cheap, and it means every app
  ; using this module is safe to draw immediately afterwards.
  lda                  #$ff
  sta                  VIA_DDR_a
  endm
