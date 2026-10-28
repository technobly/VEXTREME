# games/hello.mk - registers the SDK template game "hello" with the VXE build.
# A copy-and-rename starting point: see docs/vxe/SDK_GUIDE.md §7.
GAME_TITLE     := HELLO        # the bundle is named <GAME_TITLE>.vxe
GAME_OBJS      := game/hello.o game/hello_vxe.o
GAME_CFLAGS    := -DVXT_GAME_HELLO=1
GAME_6809_DIR  := Hello        # code/app6809/<dir>/<asm>.asm
GAME_6809_ASM  := Hello
