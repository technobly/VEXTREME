;*******************************************************************************
; vxt_rpc_macros.asm - VXT toolkit: RPC equates and macros only (6809 side)
;
; Split out from the original vxt_rpc.asm specifically so this file
; contains NO real instruction bytes - only equates and macro/template
; definitions, which asm6809 does not emit until invoked. This makes it
; safe to include immediately after a cartridge header, before `main`,
; without risking the power-on fall-through hazard that froze real
; hardware early in this toolkit's development. The RAM-only stub body
; itself now lives in the companion file vxt_rpc_stub.asm, which must be
; included last in any application - defense in depth alongside an
; explicit `jmp main` after the header.
;
; USAGE (from an application main.asm):
;     include "vectrex.i"
;     org 0
;     [cartridge header]
;     jmp main
;     setdp #$d0                       ; assembler directive - see note below
;     include "../vxt/vxt_rpc_macros.asm"
;     include "../vxt/vxt_draw.asm"     ; or other toolkit modules, as needed
;   main
;     VXT_RPC_INIT                     ; copies stub to RAM, sets runtime DP
;     ...
;     VXT_RPC #66
;     ...
;     include "../vxt/vxt_rpc_stub.asm" ; must be last in the file
;
; Note on setdp: `setdp #$d0` is a pure assembler directive (no emitted
; bytes) telling asm6809 to assume DP=$D0 when choosing between direct-page
; (2-byte) and extended (3-byte) addressing for subsequent source lines -
; confirmed by cross-checking VOOM's identical use of it. It is a
; compile-time, source-POSITION-dependent effect, distinct from the runtime
; `lda #$d0 / tfr a,dp` instructions (in VXT_RPC_INIT below) that set the
; CPU's actual DP register at execution time. Both are required; place the
; `setdp` directive early in the file (before any VIA-register-touching
; code you want assembled compactly), regardless of where VXT_RPC_INIT is
; invoked at runtime.
;*******************************************************************************

RPC_ARG_ADDR           equ    $7f00
RPC_ID_ADDR            equ    $7fff
VXT_ARG                equ    $7ffe

VXT_RAM_BASE           equ    $c880          ; verified free-RAM boundary -
                                             ; not inherited from VOOM
                                             ; ($ca00/$cb00) or copied from
                                             ; the menu's internal layout;
                                             ; chosen on its own merits.

vxt_rpcfn              equ    VXT_RAM_BASE
VXT_RAM_END            equ    VXT_RAM_BASE+(vxt_rpcfndat_end-vxt_rpcfndat)
                                             ; first free RAM byte after the
                                             ; stub. This remains a COMPUTED
                                             ; value rather than a frozen
                                             ; offset, since other modules
                                             ; (vxt_sound.asm, vxt_frame.asm)
                                             ; may still claim RAM after it.

;*******************************************************************************
; VXT_RPC_INIT - copy the RPC stub into RAM and set the runtime DP register.
; Call once at startup, before any BIOS call or VIA access.
; DESTROYS: A, X, Y
;*******************************************************************************
VXT_RPC_INIT           macro
  ldx                  #vxt_rpcfndat
  ldy                  #vxt_rpcfn
!
  lda                  ,x+
  sta                  ,y+
  cmpx                 #vxt_rpcfndat_end
  bne                  <

  lda                  #$d0                 ; runtime DP set - required by
  tfr                  a,dp                 ; every frame-loop BIOS call
                                             ; and by vxt_draw.asm's compact
                                             ; VIA addressing (this file's
                                             ; header note above).
  endm

;*******************************************************************************
; VXT_RPC #[rpc_id] / VXT_RPC_A - trigger STM32 RPC; returns when done.
; DESTROYS: A, X (A already caller-loaded for the _A form)
;*******************************************************************************
VXT_RPC                macro
  lda                  #\1
  ldx                  #9F
  jmp                  vxt_rpcfn
9
  endm

VXT_RPC_A              macro
  ldx                  #9F
  jmp                  vxt_rpcfn
9
  endm
