# Cart Options

FYI: I ran into these things recently!

@phillipeaton
> "The downside of it all being RAM is that some bad code might try and write
to the ROM address space, if it's ROM, it's a harmless write, if not...
Do any programs do that? Probably not, but who knows? I guess it would be
OK to make all the non-ROM address space work as RAM up to the 48k limit."

@malbanGit
> "But there are some games that actually (buggy!) write to ROM and destroy the running game (some of pcJohn's games)"

Gravitrex is!  ...and also Vec-Man 🙊

---

So I implemented the RAM cart specific options tonight :D

- I first modified the romemu.S to allow for variables to be used for RAM enable, start, and end address
- Created a Cart Options structure
- Created a Cart Options check/load/get/close function
- For every file loaded, it also checks if there is a matching *.ini file... if so it attempts to load it.  The options get parsed and the game is executed with RAM supported at the desired location, or not.  It works great!

Now RAM will be disabled unless we request it by adding a matching game.ini file.  The ini file is binary data, but super simple to create by hand, and rarely used.

E.g. If you have Animaction.bin, you would add the following Animation.ini to enable RAM support for Animation.bin:

```
40 00 01 00 01 20 7F 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

Data is encoded in the file in little endian format

0x0040 = 64 bytes for size
0x0001 = Version 1 Cart Options format
0x01 = bit 0 set, RAM enabled (other bits not used currently)
0x20 - high byte of start address (aka 0x2000)
0x27 - high byte of end address (aka 0x27FF)
This is for Animation of course

If you wanted to only allow 0x2000 to 0x20FF, you would specify it like this
0x20 - high byte of start address (aka 0x2000)
0x20 - high byte of end address (aka 0x20FF)

Logo Lite would be
0x60 - high byte of start address (aka 0x6000)
0x7F - high byte of end address (aka 0x7Fxx)

Technically Logo needs RAM all of the way to 7FFF, but we will run into a problem with that if it ever reaches the top, because that's how we call our RPC functions.  So, we'll need to move those to >=0x8000 ;-) But for now you'll be hard pressed to reach 0x7FFF with Logo Lite I think.  Let me know if you do!
