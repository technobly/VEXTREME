# games/cal.mk - the calibration rig as a VXE game: measures this console's beam errors and writes
# /calmeas.csv at the card root, which every other game's boot-time loaders read (vxt_cal_load.h).
GAME_TITLE      := CAL         # the bundle is named <GAME_TITLE>.vxe
GAME_OBJS       := game/cal_vxe.o
GAME_ENABLE_CAL := 1           # link the rig and register RPC 75/76 (see the Makefile)
GAME_CFLAGS     :=
GAME_6809_DIR   := Cal         # code/app6809/<dir>/<asm>.asm
GAME_6809_ASM   := Cal
