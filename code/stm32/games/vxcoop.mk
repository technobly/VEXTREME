# games/vxcoop.mk - VX-COOP, the toolkit demonstration cart: input, text, sound, music, 3D drawing and
# calibration, with the STM32 computing each frame and the 6809 drawing it. Read it as a worked example.
GAME_TITLE     := VXCOOP       # the bundle is named <GAME_TITLE>.vxe
GAME_OBJS      := game/vxcoop_handler.o game/vxcoop_music_example.o game/vxcoop_vxe.o
GAME_CFLAGS    :=
GAME_6809_DIR  := VXCOOP       # code/app6809/<dir>/<asm>.asm
GAME_6809_ASM  := VXCOOP
