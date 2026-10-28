;*******************************************************************************
; vxt_rpc_stub.asm - VXT toolkit: RPC stub body only (6809 side)
;
; *** Must be included last in any application file ***
;
; These are real, assembled instruction bytes - not macro definitions. This
; file was split out from vxt_rpc.asm specifically so it can be positioned
; physically last, after every application loop and branch target, so
; normal control flow can never fall into it by accident - defense in
; depth alongside the explicit `jmp main` every application must also
; place immediately after its cartridge header. This exact combination
; was learned the hard way from a power-on fall-through freeze on real
; hardware, and from reviewing how Sprite_tm's own VOOM/multicart source
; avoided the same hazard structurally.
;
; Body = menu.asm rpcfndat2, with explicit extended addressing forced (the
; addresses used here - $0, $1, $7FFF - are not in page $D0, so this is
; correct regardless of any setdp state elsewhere in the file).
; Entry: A = RPC ID, X = return address (see vxt_rpc_macros.asm's VXT_RPC).
;*******************************************************************************
vxt_rpcfndat
  sta                  >RPC_ID_ADDR
!
  lda                  >$0000
  cmpa                 #'g'
  bne                  <
  lda                  >$0001
  cmpa                 #' '
  bne                  <
  jmp                  ,x
vxt_rpcfndat_end
