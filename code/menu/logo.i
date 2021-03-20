LOGO_BLOW_UP EQU 12

LOGO_VECTREX
  fcb    $00, +$00*LOGO_BLOW_UP, +$07*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*LOGO_BLOW_UP, +$07*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$03*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*LOGO_BLOW_UP, -$04*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$01*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*LOGO_BLOW_UP, +$04*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$0A*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*LOGO_BLOW_UP, +$07*LOGO_BLOW_UP    ; mode, y, x
  fcb    $01                                          ; endmarker (1)

LOGO_SETTINGS
  fcb    $00, +$02*LOGO_BLOW_UP, +$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, +$06*LOGO_BLOW_UP    ; mode, y, x
  fcb    $00, +$00*LOGO_BLOW_UP, +$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, +$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $00, +$04*LOGO_BLOW_UP, +$00*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$06*LOGO_BLOW_UP    ; mode, y, x
  fcb    $00, +$00*LOGO_BLOW_UP, -$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $00, -$01*LOGO_BLOW_UP, +$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, +$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*LOGO_BLOW_UP, +$00*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*LOGO_BLOW_UP, +$00*LOGO_BLOW_UP    ; mode, y, x
  fcb    $00, -$02*LOGO_BLOW_UP, +$04*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*LOGO_BLOW_UP, +$00*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, +$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*LOGO_BLOW_UP, +$00*LOGO_BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*LOGO_BLOW_UP, -$02*LOGO_BLOW_UP    ; mode, y, x
  fcb    $01                                          ; endmarker (1)
