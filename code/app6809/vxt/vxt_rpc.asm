;*******************************************************************************
; vxt_rpc.asm - VXT toolkit: RPC trigger primitives (6809 side)
;
; Part of the VEXTREME cooperative-multitasking toolkit ("VXT").
; License: GPLv3 (derivative of VEXTREME menu.asm / macro.asm patterns,
;          Copyright (C) 2020 Brett Walach, (C) 2021 Vasily Kiniv)
;
;   - Protocol: a 6809 write to $7FFF triggers doHandleEvent(data) on the STM32
;     (romemu.S write path). Writes to $7F00-$7FFE
;     land in STM32 parmRam[low_byte] (romemu.S; parmRam is WRITE-only from the
;     6809 side - reads are always served from romData).
;   - While the STM32 executes the RPC handler, ROM serving is PAUSED
;     (romemu.S handleevent: blx doHandleEvent from inside the polling loop).
;     Therefore the trigger/wait code below must execute from Vectrex RAM.
;   - Ready detection: poll cart addresses $0000/$0001 until they read
;     'g',' ' (the cartridge header signature) again - the exact mechanism
;     shipping today in menu.asm rpcfndat2/rpcwaitloop2.
;   - Stub body is menu.asm's rpcfndat2 verbatim in structure, with explicit
;     '>' (extended addressing) forced so correctness does not depend on
;     assembler DP assumptions (no setdp is used anywhere in the menu build).
;   - Call convention mirrors macro.asm M_JSR_RPC: ID in A, return addr in X.
;   - Default RAM placement $C880 = start of USER RAM SECTION ($C880-$CBEA),
;     per menu.asm memory map.
;
; USAGE (from an application main.asm):
;     VXT_RAM_BASE equ $c880        ; optional override BEFORE include
;     include "vxt/vxt_rpc.asm"
;     ...
;     VXT_RPC_INIT                  ; once at startup: copy stub to RAM
;     ...
;     lda #42
;     sta VXT_ARG                   ; parmRam[254] - the "primary arg" slot
;     VXT_RPC #66                   ; trigger STM32 RPC 66, returns when done
;
; RPC ID space:
;   1-18 upstream firmware; 64-79 reserved for VXT toolkit (66 = frame render);
;   app-specific IDs: 19-63 or 80+.
;*******************************************************************************

; --- Protocol addresses (menu.asm equates, verbatim) --------------------------
RPC_ARG_ADDR           equ    $7f00          ; parmRam[0] window base
RPC_ID_ADDR            equ    $7fff          ; write here = doHandleEvent(byte)
VXT_ARG                equ    $7ffe          ; parmRam[254]: primary arg slot
                                             ; (same slot the joystick byte and
                                             ; upstream cases 1/5/8 use)

; --- RAM placement -------------------------------------------------------------
; NOTE: kept as a single unconditional equate (only IF/ENDIF conditional
; assembly is confirmed in the in-repo asm6809 usage, menu.asm; ifndef is not).
; Change this one line if an application needs the stub elsewhere.
VXT_RAM_BASE           equ    $c880          ; USER RAM SECTION start (menu.asm)

vxt_rpcfn              equ    VXT_RAM_BASE   ; RAM address of the copied stub
VXT_RAM_END            equ    VXT_RAM_BASE+(vxt_rpcfndat_end-vxt_rpcfndat)
                                             ; first free RAM byte after stub;
                                             ; app variables may start here

;*******************************************************************************
; VXT_RPC_INIT - copy the RPC stub into Vectrex RAM. Call once at startup.
; DESTROYS: A, X, Y
; (Copy loop pattern = macro.asm M_MEMCPY)
;*******************************************************************************
VXT_RPC_INIT           macro
  ldx                  #vxt_rpcfndat        ; source (in cart image)
  ldy                  #vxt_rpcfn           ; destination (Vectrex RAM)
!
  lda                  ,x+
  sta                  ,y+
  cmpx                 #vxt_rpcfndat_end
  bne                  <
  endm

;*******************************************************************************
; VXT_RPC #[rpc_id] - trigger STM32 RPC by immediate ID; returns when the
;                     STM32 has finished the handler and resumed ROM serving.
; DESTROYS: A, X
; (Convention = macro.asm M_JSR_RPC: ID in A, return address in X)
;*******************************************************************************
VXT_RPC                macro
  lda                  #\1
  ldx                  #9F                  ; return address = just past macro
  jmp                  vxt_rpcfn
9
  endm

;*******************************************************************************
; VXT_RPC_A - same, but RPC ID is already in A (computed at runtime).
; DESTROYS: X (A already caller-loaded)
;*******************************************************************************
VXT_RPC_A              macro
  ldx                  #9F
  jmp                  vxt_rpcfn
9
  endm

;*******************************************************************************
; The stub itself (copied to RAM by VXT_RPC_INIT; never executed in place).
; Body = menu.asm rpcfndat2, with explicit extended addressing.
; Entry: A = RPC ID, X = return address.
;
; *** Warning to includers ***
; These are real, assembled instruction bytes - not macro definitions. If this
; file is included directly after a cartridge header (or anywhere execution
; could fall through to by accident), the 6809 will execute this code in place
; instead of your app's entry point, using whatever garbage is in A/X at that
; moment, and will eventually jump to a garbage address. This exact mistake
; caused a real freeze/reboot on hardware - the fix was an explicit
; `jmp main` placed immediately after the header, before this include.
; Always do the same in any new app.
;*******************************************************************************
vxt_rpcfndat
  sta                  >RPC_ID_ADDR         ; trigger doHandleEvent(A) on STM32
!                                           ; ROM serving now paused; we are
                                            ; safe only because this runs in RAM
  lda                  >$0000               ; header byte 0 ...
  cmpa                 #'g'                 ; ... reads 'g' only when the STM32
  bne                  <                    ;     is serving ROM again
  lda                  >$0001               ; header byte 1 ...
  cmpa                 #' '                 ; ... second check defeats bus noise
  bne                  <
  jmp                  ,x                   ; return
vxt_rpcfndat_end
