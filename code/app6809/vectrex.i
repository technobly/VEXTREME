; this file is part of vectrex frogger, written by Christopher Salomon
; in March-April 1998
; all stuff contained here is public domain (?)
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; this file contains includes for vectrex BIOS functions and variables      ;
; it was written by Bruce Tomlin, slighte changed by Christopher Salomon    ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Vec_Snd_Shadow  equ     $C800   ;Shadow of sound chip registers (15 bytes)
Vec_Btn_State   equ     $C80F   ;Current state of all joystick buttons
Vec_Prev_Btns   equ     $C810   ;Previous state of all joystick buttons
Vec_Buttons     equ     $C811   ;Current toggle state of all buttons
Vec_Button_1_1  equ     $C812   ;Current toggle state of stick 1 button 1
Vec_Button_1_2  equ     $C813   ;Current toggle state of stick 1 button 2
Vec_Button_1_3  equ     $C814   ;Current toggle state of stick 1 button 3
Vec_Button_1_4  equ     $C815   ;Current toggle state of stick 1 button 4
Vec_Button_2_1  equ     $C816   ;Current toggle state of stick 2 button 1
Vec_Button_2_2  equ     $C817   ;Current toggle state of stick 2 button 2
Vec_Button_2_3  equ     $C818   ;Current toggle state of stick 2 button 3
Vec_Button_2_4  equ     $C819   ;Current toggle state of stick 2 button 4
Vec_Joy_Resltn  equ     $C81A   ;Joystick A/D resolution ($80=min $00=max)
Vec_Joy_1_X     equ     $C81B   ;Joystick 1 left/right
Vec_Joy_1_Y     equ     $C81C   ;Joystick 1 up/down
Vec_Joy_2_X     equ     $C81D   ;Joystick 2 left/right
Vec_Joy_2_Y     equ     $C81E   ;Joystick 2 up/down
Vec_Joy_Mux     equ     $C81F   ;Joystick enable/mux flags (4 bytes)
Vec_Joy_Mux_1_X equ     $C81F   ;Joystick 1 X enable/mux flag (=1)
Vec_Joy_Mux_1_Y equ     $C820   ;Joystick 1 Y enable/mux flag (=3)
Vec_Joy_Mux_2_X equ     $C821   ;Joystick 2 X enable/mux flag (=5)
Vec_Joy_Mux_2_Y equ     $C822   ;Joystick 2 Y enable/mux flag (=7)
Vec_Misc_Count  equ     $C823   ;Misc counter/flag byte, zero when not in use
Vec_0Ref_Enable equ     $C824   ;Check0Ref enable flag
Vec_Loop_Count  equ     $C825   ;Loop counter word (incremented in Wait_Recal)
Vec_Brightness  equ     $C827   ;Default brightness
Vec_Dot_Dwell   equ     $C828   ;Dot dwell time?
Vec_Pattern     equ     $C829   ;Dot pattern (bits)
Vec_Text_HW     equ     $C82A   ;Default text height and width
Vec_Text_Height equ     $C82A   ;Default text height
Vec_Text_Width  equ     $C82B   ;Default text width
Vec_Str_Ptr     equ     $C82C   ;Temporary string pointer for Print_Str
Vec_Counters    equ     $C82E   ;Six bytes of counters
Vec_Counter_1   equ     $C82E   ;First  counter byte
Vec_Counter_2   equ     $C82F   ;Second counter byte
Vec_Counter_3   equ     $C830   ;Third  counter byte
Vec_Counter_4   equ     $C831   ;Fourth counter byte
Vec_Counter_5   equ     $C832   ;Fifth  counter byte
Vec_Counter_6   equ     $C833   ;Sixth  counter byte
Vec_RiseRun_Tmp equ     $C834   ;Temp storage word for rise/run
Vec_Angle       equ     $C836   ;Angle for rise/run and rotation calculations
Vec_Run_Index   equ     $C837   ;Index pair for run
;*                       $C839   ;Pointer to copyright string during startup
Vec_Rise_Index  equ     $C839   ;Index pair for rise
;*                       $C83B   ;High score cold-start flag (=0 if valid)
Vec_RiseRun_Len equ     $C83B   ;length for rise/run
;*                       $C83C   ;temp byte
Vec_Rfrsh       equ     $C83D   ;Refresh time (divided by 1.5MHz)
Vec_Rfrsh_lo    equ     $C83D   ;Refresh time low byte
Vec_Rfrsh_hi    equ     $C83E   ;Refresh time high byte
Vec_Music_Work  equ     $C83F   ;Music work buffer (14 bytes, backwards?)
Vec_Music_Wk_A  equ     $C842   ;        register 10
;*                       $C843   ;        register 9
;*                       $C844   ;        register 8
Vec_Music_Wk_7  equ     $C845   ;        register 7
Vec_Music_Wk_6  equ     $C846   ;        register 6
Vec_Music_Wk_5  equ     $C847   ;        register 5
;*                       $C848   ;        register 4
;*                       $C849   ;        register 3
;*                       $C84A   ;        register 2
Vec_Music_Wk_1  equ     $C84B   ;        register 1
;*                       $C84C   ;        register 0
Vec_Freq_Table  equ     $C84D   ;Pointer to note-to-frequency table (normally $FC8D)
Vec_Max_Players equ     $C84F   ;Maximum number of players for Select_Game
Vec_Max_Games   equ     $C850   ;Maximum number of games for Select_Game
Vec_ADSR_Table  equ     $C84F   ;Storage for first music header word (ADSR table)
Vec_Twang_Table equ     $C851   ;Storage for second music header word ('twang' table)
Vec_Music_Ptr   equ     $C853   ;Music data pointer
Vec_Expl_ChanA  equ     $C853   ;Used by Explosion_Snd - bit for first channel used?
Vec_Expl_Chans  equ     $C854   ;Used by Explosion_Snd - bits for all channels used?
Vec_Music_Chan  equ     $C855   ;Current sound channel number for Init_Music
Vec_Music_Flag  equ     $C856   ;Music active flag ($00=off $01=start $80=on)
Vec_Duration    equ     $C857   ;Duration counter for Init_Music
Vec_Music_Twang equ     $C858   ;3 word 'twang' table used by Init_Music
Vec_Expl_1      equ     $C858   ;Four bytes copied from Explosion_Snd's U-reg parameters
Vec_Expl_2      equ     $C859   ;
Vec_Expl_3      equ     $C85A   ;
Vec_Expl_4      equ     $C85B   ;
Vec_Expl_Chan   equ     $C85C   ;Used by Explosion_Snd - channel number in use?
Vec_Expl_ChanB  equ     $C85D   ;Used by Explosion_Snd - bit for second channel used?
Vec_ADSR_Timers equ     $C85E   ;ADSR timers for each sound channel (3 bytes)
Vec_Music_Freq  equ     $C861   ;Storage for base frequency of each channel (3 words)
;*                       $C85E   ;Scratch 'score' storage for Display_Option (7 bytes)
Vec_Expl_Flag   equ     $C867   ;Explosion_Snd initialization flag?
;*               $C868...$C876   ;Unused?
Vec_Expl_Timer  equ     $C877   ;Used by Explosion_Snd
;*                       $C878   ;Unused?
Vec_Num_Players equ     $C879   ;Number of players selected in Select_Game
Vec_Num_Game    equ     $C87A   ;Game number selected in Select_Game
Vec_Seed_Ptr    equ     $C87B   ;Pointer to 3-byte random number seed (=$C87D)
Vec_Random_Seed equ     $C87D   ;Default 3-byte random number seed
                                ;
;*    $C880 - $CBEA is user RAM  ;
                                ;
Vec_Default_Stk equ     $CBEA   ;Default top-of-stack
Vec_High_Score  equ     $CBEB   ;High score storage (7 bytes)
Vec_SWI3_Vector equ     $CBF2   ;SWI2/SWI3 interrupt vector (3 bytes)
Vec_SWI2_Vector equ     $CBF2   ;SWI2/SWI3 interrupt vector (3 bytes)
Vec_FIRQ_Vector equ     $CBF5   ;FIRQ interrupt vector (3 bytes)
Vec_IRQ_Vector  equ     $CBF8   ;IRQ interrupt vector (3 bytes)
Vec_SWI_Vector  equ     $CBFB   ;SWI/NMI interrupt vector (3 bytes)
Vec_NMI_Vector  equ     $CBFB   ;SWI/NMI interrupt vector (3 bytes)
Vec_Cold_Flag   equ     $CBFE   ;Cold start flag (warm start if = $7321)
                                ;
VIA_port_b      equ     $D000   ;VIA port B data I/O register
;*       0 sample/hold (0=enable  mux 1=disable mux)
;*       1 mux sel 0
;*       2 mux sel 1
;*       3 sound BC1
;*       4 sound BDIR
;*       5 comparator input
;*       6 external device (slot pin 35) initialized to input
;*       7 /RAMP
VIA_port_a      equ     $D001   ;VIA port A data I/O register (handshaking)
VIA_DDR_b       equ     $D002   ;VIA port B data direction register (0=input 1=output)
VIA_DDR_a       equ     $D003   ;VIA port A data direction register (0=input 1=output)
VIA_t1_cnt_lo   equ     $D004   ;VIA timer 1 count register lo (scale factor)
VIA_t1_cnt_hi   equ     $D005   ;VIA timer 1 count register hi
VIA_t1_lch_lo   equ     $D006   ;VIA timer 1 latch register lo
VIA_t1_lch_hi   equ     $D007   ;VIA timer 1 latch register hi
VIA_t2_lo       equ     $D008   ;VIA timer 2 count/latch register lo (refresh)
VIA_t2_hi       equ     $D009   ;VIA timer 2 count/latch register hi
VIA_shift_reg   equ     $D00A   ;VIA shift register
VIA_aux_cntl    equ     $D00B   ;VIA auxiliary control register
;*       0 PA latch enable
;*       1 PB latch enable
;*       2 \                     110=output to CB2 under control of phase 2 clock
;*       3  > shift register control     (110 is the only mode used by the Vectrex ROM)
;*       4 /
;*       5 0=t2 one shot                 1=t2 free running
;*       6 0=t1 one shot                 1=t1 free running
;*       7 0=t1 disable PB7 output       1=t1 enable PB7 output
VIA_cntl        equ     $D00C   ;VIA control register
;*       0 CA1 control     CA1 -> SW7    0=IRQ on low 1=IRQ on high
;*       1 \
;*       2  > CA2 control  CA2 -> /ZERO  110=low 111=high
;*       3 /
;*       4 CB1 control     CB1 -> NC     0=IRQ on low 1=IRQ on high
;*       5 \
;*       6  > CB2 control  CB2 -> /BLANK 110=low 111=high
;*       7 /
VIA_int_flags   equ     $D00D   ;VIA interrupt flags register
;*               bit                             cleared by
;*       0 CA2 interrupt flag            reading or writing port A I/O
;*       1 CA1 interrupt flag            reading or writing port A I/O
;*       2 shift register interrupt flag reading or writing shift register
;*       3 CB2 interrupt flag            reading or writing port B I/O
;*       4 CB1 interrupt flag            reading or writing port A I/O
;*       5 timer 2 interrupt flag        read t2 low or write t2 high
;*       6 timer 1 interrupt flag        read t1 count low or write t1 high
;*       7 IRQ status flag               write logic 0 to IER or IFR bit
VIA_int_enable  equ     $D00E   ;VIA interrupt enable register
;*       0 CA2 interrupt enable
;*       1 CA1 interrupt enable
;*       2 shift register interrupt enable
;*       3 CB2 interrupt enable
;*       4 CB1 interrupt enable
;*       5 timer 2 interrupt enable
;*       6 timer 1 interrupt enable
;*       7 IER set/clear control
VIA_port_a_nohs equ     $D00F   ;VIA port A data I/O register (no handshaking)

Cold_Start      equ     $F000   ;
Warm_Start      equ     $F06C   ;
Init_VIA        equ     $F14C   ;
Init_OS_RAM     equ     $F164   ;
Init_OS         equ     $F18B   ;
Wait_Recal      equ     $F192   ;
Set_Refresh     equ     $F1A2   ;
DP_to_D0        equ     $F1AA   ;
DP_to_C8        equ     $F1AF   ;
Read_Btns_Mask  equ     $F1B4   ;
Read_Btns       equ     $F1BA   ;
Joy_Analog      equ     $F1F5   ;
Joy_Digital     equ     $F1F8   ;
Sound_Byte      equ     $F256   ;
Sound_Byte_x    equ     $F259   ;
Sound_Byte_raw  equ     $F25B   ;
Clear_Sound     equ     $F272   ;
Sound_Bytes     equ     $F27D   ;
Sound_Bytes_x   equ     $F284   ;
Do_Sound        equ     $F289   ;
Do_Sound_x      equ     $F28C   ;
Intensity_1F    equ     $F29D   ;
Intensity_3F    equ     $F2A1   ;
Intensity_5F    equ     $F2A5   ;
Intensity_7F    equ     $F2A9   ;
Intensity_a     equ     $F2AB   ;
Dot_ix_b        equ     $F2BE   ;
Dot_ix          equ     $F2C1   ;
Dot_d           equ     $F2C3   ;
Dot_here        equ     $F2C5   ;
Dot_List        equ     $F2D5   ;
Dot_List_Reset  equ     $F2DE   ;
Recalibrate     equ     $F2E6   ;
Moveto_x_7F     equ     $F2F2   ;
Moveto_d_7F     equ     $F2FC   ;
Moveto_ix_FF    equ     $F308   ;
Moveto_ix_7F    equ     $F30C   ;
Moveto_ix_a     equ     $F30E   ;
Moveto_ix       equ     $F310   ;
Moveto_d        equ     $F312   ;
Reset0Ref_D0    equ     $F34A   ;
Check0Ref       equ     $F34F   ;
Reset0Ref       equ     $F354   ;
Reset_Pen       equ     $F35B   ;
Reset0Int       equ     $F36B   ;
Print_Str_hwyx  equ     $F373   ;
Print_Str_yx    equ     $F378   ;
Print_Str_d     equ     $F37A   ;
Print_List_hw   equ     $F385   ;
Print_List      equ     $F38A   ;
Print_List_chk  equ     $F38C   ;
Print_Ships_x   equ     $F391   ;
Print_Ships     equ     $F393   ;
Mov_Draw_VLc_a  equ     $F3AD   ;count y x y x ...
Mov_Draw_VL_b   equ     $F3B1   ;y x y x ...
Mov_Draw_VLcs   equ     $F3B5   ;count scale y x y x ...
Mov_Draw_VL_ab  equ     $F3B7   ;y x y x ...
Mov_Draw_VL_a   equ     $F3B9   ;y x y x ...
Mov_Draw_VL     equ     $F3BC   ;y x y x ...
Mov_Draw_VL_d   equ     $F3BE   ;y x y x ...
Draw_VLc        equ     $F3CE   ;count y x y x ...
Draw_VL_b       equ     $F3D2   ;y x y x ...
Draw_VLcs       equ     $F3D6   ;count scale y x y x ...
Draw_VL_ab      equ     $F3D8   ;y x y x ...
Draw_VL_a       equ     $F3DA   ;y x y x ...
Draw_VL         equ     $F3DD   ;y x y x ...
Draw_Line_d     equ     $F3DF   ;y x y x ...
Draw_VLp_FF     equ     $F404   ;pattern y x pattern y x ... $01
Draw_VLp_7F     equ     $F408   ;pattern y x pattern y x ... $01
Draw_VLp_scale  equ     $F40C   ;scale pattern y x pattern y x ... $01
Draw_VLp_b      equ     $F40E   ;pattern y x pattern y x ... $01
Draw_VLp        equ     $F410   ;pattern y x pattern y x ... $01
Draw_Pat_VL_a   equ     $F434   ;y x y x ...
Draw_Pat_VL     equ     $F437   ;y x y x ...
Draw_Pat_VL_d   equ     $F439   ;y x y x ...
Draw_VL_mode    equ     $F46E   ;mode y x mode y x ... $01
Print_Str       equ     $F495   ;
Random_3        equ     $F511   ;
Random          equ     $F517   ;
Init_Music_Buf  equ     $F533   ;
Clear_x_b       equ     $F53F   ;
Clear_C8_RAM    equ     $F542   ;never used by GCE carts?
Clear_x_256     equ     $F545   ;
Clear_x_d       equ     $F548   ;
Clear_x_b_80    equ     $F550   ;
Clear_x_b_a     equ     $F552   ;
Dec_3_Counters  equ     $F55A   ;
Dec_6_Counters  equ     $F55E   ;
Dec_Counters    equ     $F563   ;
Delay_3         equ     $F56D   ;30 cycles
Delay_2         equ     $F571   ;25 cycles
Delay_1         equ     $F575   ;20 cycles
Delay_0         equ     $F579   ;12 cycles
Delay_b         equ     $F57A   ;5*B + 10 cycles
Delay_RTS       equ     $F57D   ;5 cycles
Bitmask_a       equ     $F57E   ;
Abs_a_b         equ     $F584   ;
Abs_b           equ     $F58B   ;
Rise_Run_Angle  equ     $F593   ;
Get_Rise_Idx    equ     $F5D9   ;
Get_Run_Idx     equ     $F5DB   ;
Get_Rise_Run    equ     $F5EF   ;
Rise_Run_X      equ     $F5FF   ;
Rise_Run_Y      equ     $F601   ;
Rise_Run_Len    equ     $F603   ;
Rot_VL_ab       equ     $F610   ;
Rot_VL          equ     $F616   ;
Rot_VL_Mode_a   equ     $F61F   ;
Rot_VL_Mode     equ     $F62B   ;
Rot_VL_dft      equ     $F637   ;
Xform_Run_a     equ     $F65B   ;
Xform_Run       equ     $F65D   ;
Xform_Rise_a    equ     $F661   ;
Xform_Rise      equ     $F663   ;
Move_Mem_a_1    equ     $F67F   ;
Move_Mem_a      equ     $F683   ;
Init_Music_chk  equ     $F687   ;
Init_Music      equ     $F68D   ;
Init_Music_x    equ     $F692   ;
Select_Game     equ     $F7A9   ;
Clear_Score     equ     $F84F   ;
Add_Score_a     equ     $F85E   ;
Add_Score_d     equ     $F87C   ;
Strip_Zeros     equ     $F8B7   ;
Compare_Score   equ     $F8C7   ;
New_High_Score  equ     $F8D8   ;
Obj_Will_Hit_u  equ     $F8E5   ;
Obj_Will_Hit    equ     $F8F3   ;
Obj_Hit         equ     $F8FF   ;
Explosion_Snd   equ     $F92E   ;
Draw_Grid_VL    equ     $FF9F   ;
                                ;
music1  equ $FD0D               ;
music2  equ $FD1D               ;
music3  equ $FD81               ;
music4  equ $FDD3               ;
music5  equ $FE38               ;
music6  equ $FE76               ;
music7  equ $FEC6               ;
music8  equ $FEF8               ;
music9  equ $FF26               ;
musica  equ $FF44               ;
musicb  equ $FF62               ;
musicc  equ $FF7A               ;
musicd  equ $FF8F               ;

;endif
