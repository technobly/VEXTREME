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


BLOW_UP EQU 12                              ; Scale factor

HSFNT
  fdb    HSFNT_0                            ; space
  fdb    HSFNT_1                            ; !
  fdb    HSFNT_2                            ; "
  fdb    HSFNT_3                            ; #
  fdb    HSFNT_4                            ; $
  fdb    HSFNT_5                            ; %
  fdb    HSFNT_6                            ; &
  fdb    HSFNT_7                            ; '
  fdb    HSFNT_8                            ; (
  fdb    HSFNT_9                            ; )
  fdb    HSFNT_10                           ; *
  fdb    HSFNT_11                           ; +
  fdb    HSFNT_12                           ; ,
  fdb    HSFNT_13                           ; -
  fdb    HSFNT_14                           ; .
  fdb    HSFNT_15                           ; /
  fdb    HSFNT_16                           ; 0
  fdb    HSFNT_17                           ; 1
  fdb    HSFNT_18                           ; 2
  fdb    HSFNT_19                           ; 3
  fdb    HSFNT_20                           ; 4
  fdb    HSFNT_21                           ; 5
  fdb    HSFNT_22                           ; 6
  fdb    HSFNT_23                           ; 7
  fdb    HSFNT_24                           ; 8
  fdb    HSFNT_25                           ; 9
  fdb    HSFNT_26                           ; :
  fdb    HSFNT_27                           ; ;
  fdb    HSFNT_28                           ; <
  fdb    HSFNT_29                           ; =
  fdb    HSFNT_30                           ; >
  fdb    HSFNT_31                           ; ?
  fdb    HSFNT_32                           ; @
  fdb    HSFNT_33                           ; A
  fdb    HSFNT_34                           ; B
  fdb    HSFNT_35                           ; C
  fdb    HSFNT_36                           ; D
  fdb    HSFNT_37                           ; E
  fdb    HSFNT_38                           ; F
  fdb    HSFNT_39                           ; G
  fdb    HSFNT_40                           ; H
  fdb    HSFNT_41                           ; I
  fdb    HSFNT_42                           ; J
  fdb    HSFNT_43                           ; K
  fdb    HSFNT_44                           ; L
  fdb    HSFNT_45                           ; M
  fdb    HSFNT_46                           ; N
  fdb    HSFNT_47                           ; O
  fdb    HSFNT_48                           ; P
  fdb    HSFNT_49                           ; Q
  fdb    HSFNT_50                           ; R
  fdb    HSFNT_51                           ; S
  fdb    HSFNT_52                           ; T
  fdb    HSFNT_53                           ; U
  fdb    HSFNT_54                           ; V
  fdb    HSFNT_55                           ; W
  fdb    HSFNT_56                           ; X
  fdb    HSFNT_57                           ; Y
  fdb    HSFNT_58                           ; Z
  fdb    HSFNT_59                           ; [
  fdb    HSFNT_60                           ; \
  fdb    HSFNT_61                           ; ]
  fdb    HSFNT_62                           ; ^
  fdb    HSFNT_63                           ; _
  fdb    HSFNT_64                           ; `
  fdb    HSFNT_33                           ; a ; Reuse uppercase letters
  fdb    HSFNT_34                           ; b
  fdb    HSFNT_35                           ; c
  fdb    HSFNT_36                           ; d
  fdb    HSFNT_37                           ; e
  fdb    HSFNT_38                           ; f
  fdb    HSFNT_39                           ; g
  fdb    HSFNT_40                           ; h
  fdb    HSFNT_41                           ; i
  fdb    HSFNT_42                           ; j
  fdb    HSFNT_43                           ; k
  fdb    HSFNT_44                           ; l
  fdb    HSFNT_45                           ; m
  fdb    HSFNT_46                           ; n
  fdb    HSFNT_47                           ; o
  fdb    HSFNT_48                           ; p
  fdb    HSFNT_49                           ; q
  fdb    HSFNT_50                           ; r
  fdb    HSFNT_51                           ; s
  fdb    HSFNT_52                           ; t
  fdb    HSFNT_53                           ; u
  fdb    HSFNT_54                           ; v
  fdb    HSFNT_55                           ; w
  fdb    HSFNT_56                           ; x
  fdb    HSFNT_57                           ; y
  fdb    HSFNT_58                           ; z ; END Reuse uppercase letters
  fdb    HSFNT_65                           ; {
  fdb    HSFNT_66                           ; |
  fdb    HSFNT_67                           ; }
  fdb    HSFNT_68                           ; ~
  fdb    HSFNT_69                           ; short _

HSFNT_0   ; space
  fcb    $01

HSFNT_1   ; !
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_2   ; "
  fcb    $00, +$07*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $00, -$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_3   ; #
  fcb    $00, +$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$02*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_4   ; $
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, -$01*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_5   ; %
  fcb    $00, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_6   ; &
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, -$01*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_7   ; '
  fcb    $00, +$08*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_8   ; (
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)
  
HSFNT_9   ; )
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_10  ; *
  fcb    $00, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$06*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, -$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_11  ; +
  fcb    $00, +$01*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, -$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_12  ; ,
  fcb    $00, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_13  ; -
  fcb    $00, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_14  ; .
  fcb    $00, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_15  ; /
  fcb    $ff, +$08*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_16  ; 0
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_17  ; 1
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_18  ; 2
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_19  ; 3
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$04*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_20  ; 4
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_21  ; 5
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_22  ; 6
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_23  ; 7
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_24  ; 8
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_25  ; 9
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_26  ; :
  fcb    $00, +$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_27
  fcb    $00, +$02*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $00, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_28  ; <
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_29  ; =
  fcb    $00, +$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_30  ; >
  fcb    $ff, +$04*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_31  ; ?
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, -$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_32  ; @
  fcb    $00, +$06*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $ff, +$05*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_33  ; A
  fcb    $ff, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$03*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_34  ; B
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_35  ; C
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_36  ; D
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_37  ; E
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, -$04*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_38  ; F
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, -$04*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_39  ; G
  fcb    $00, +$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_40  ; H
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_41  ; I
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_42  ; J
  fcb    $00, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_43  ; K
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_44  ; L
  fcb    $00, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_45  ; M
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)
  
HSFNT_46  ; N
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_47  ; O
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_48  ; P
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_49  ; Q
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $00, +$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_50  ; R
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_51  ; S
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_52  ; T
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_53  ; U
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_54  ; V
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_55  ; W
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_56  ; X
  fcb    $ff, +$08*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_57  ; Y
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$05*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $00, +$03*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$03*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_58  ; Z
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, -$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_59  ; [
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_60  ; \
  fcb    $00, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$08*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_61  ; ]
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_62  ; ^
  fcb    $00, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_63  ; _
  fcb    $ff, +$00*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_64  ; `
  fcb    $00, +$08*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_65  ; {
  fcb    $00, +$08*BLOW_UP, +$04*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_66  ; |
  fcb    $00, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$08*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)


HSFNT_67  ; }
  fcb    $ff, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $00, +$00*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$01*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$01*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_68  ; ~
  fcb    $00, +$06*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_69  ; short _
  fcb    $ff, +$00*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_FOLDER
  fcb    $00, +$01*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$06*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$02*BLOW_UP    ; mode, y, x
  fcb    $ff, -$01*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$05*BLOW_UP    ; mode, y, x
  fcb    $ff, -$05*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, -$08*BLOW_UP    ; mode, y, x
  fcb    $00, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$00*BLOW_UP, +$08*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_ARR_LEFT
  fcb    $00, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, +$02*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)

HSFNT_ARR_RIGHT
  fcb    $00, +$02*BLOW_UP, +$01*BLOW_UP    ; mode, y, x
  fcb    $ff, +$04*BLOW_UP, +$00*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, +$03*BLOW_UP    ; mode, y, x
  fcb    $ff, -$02*BLOW_UP, -$03*BLOW_UP    ; mode, y, x
  fcb    $01                                ; endmarker (1)
