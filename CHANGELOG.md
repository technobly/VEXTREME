# Changelog

## v0.24

Hardware Changes

- rename 'veccart' to 'vextreme' for hardware
- add image for v0.3 wire mod
- Fixes #17, make all pads on addressable LEDS the same size
- Fixes #10 removes soldermask ink around PCB fingers
- Fixes #11 Edge of PCB fingers should be 18.0mm from center of 3.52mm mounting hole and 1.2mm from edge, and slightly enlarged (0.2mm oversized)
- Also aligns holes, fingers and board outline on 0.1mm grid

Software Changes

- Added Turkey Pop.bin to release
- Added HACKS/LEVEL SELECT/Mine Storm 1.bin to release
- Fixed GCE/Mine Storm 1.bin in release
- rename 'veccart' to 'stm32' for firmware, and rename 'multicart' to 'menu'
- Embedded menu binary in STM32 flash image
- Update Dockerfile distro to Arch w/ newer arm-gcc (10.1)
- add .map file to build output
- Dev Mode (cart.bin) and Menu (multicart.bin) should not load a high score
- made high_score_flag readable from parmRam with new RPC ID 16, and speeds memory r/w for highscore
- Implements #55 High Score (by @TylerBrymer PR #56)
- Truncate filenames displayed to 16 characters and fix long filename bug
- Reduced rainbow mode colors so that no-buzz Vectrex owners don't hear any LED modulation
- implemented Animaction support for v0.3 hardware
- made sys_opt.hw_ver usage clearer
- added RAM Write debugging application and new dumpMemory() RPC ID 13
- added a release.sh script
- added standalone Developer Mode app (note: this is for reference only, see readme.md)
- added Develop Mode docs
- added Developer Mode (DM) to Menu.  Press button 2 & 3 at any time to enter DM, or hold them down when starting the Menu.  DM will also automatic re-enter when it's active.  DM is active when a cart.bin is present, and DM can be disabled by removing cart.bin manually, or through the DM menu button 1: Exit.
- add USE_HW compile option, which must now be set.  It will not default to a specific version.  Right now specify it as USE_HW=v0.2 or USE_HW=v0.3 when running 'make clean all flash'.  It can also be exported in your environment if you wish, but don't forget about it if you update your HW ;-)
- removed :leave from DFU commands since after flashing the stm32 binary, we are not really ready to reset and run.  The boot0 jumper must be removed before then.
- minor changes to improve the Makefile for Windows (still WIP)
- changed delay() and millis() to use dwt_read_cycle_counter() instead of the sys_tick_handler() because romemu.S didn't like us randomly leaving to handle interrupts, and we need these functions while we are emulating ROM.  There is a ~35s delay limit because of this.  I added an assert so I don't forget!
- started adding some changes in fatfs to enable writes to flash memory since it was currently read-only, but ended up not needing them.  No worries though, the highscore feature will require these.  These changes thanks to @tbrymer!
- VEXTREME now also prints it's own HW and SW revision in logs at boot time.
- Moved start of rainbow LED code to after we are done with USB activities, so as not to load down the USB port more than necessary.  Hopefully this fixes a reported bug with Windows.
- USB MSC can now be entered while the Vectrex is off, Ejected and left plugged in and the Vectrex can be turned on and boot a cart.bin for Developer Mode, or the VEXTREME menu.
- USB MSC can be started and stopped from the Vectrex now, which is necessary for Developer Mode.
- An improved reset function was added after feedback from users.  Now in v0.3 hardware you will be able to press reset at any time to get back to the Menu.  If you hold reset for ~700ms, it will reset the current running ROM/game.  In the case of booting into Developer Mode (cart.bin already on the drive root), a first reset will not go to the Menu, but rather the cart.bin.. allowing you to skip the cold boot sequence. You may modify v0.2 hardware to act like v0.3 hardware for this feature.  For now that means you will need to jumper V-OE pin 12 of the cart fingers (or U3 pin 9) to STM32 pin 29 (PB10).
- For v0.2 hardware, you can still compile in the old functionality were a short reset would reset the running ROM/game, and if you hold reset for ~700ms you will get the Menu.
- Also added to the reset sequence for both v0.2/v0.3 HW is that the LEDs light up CYAN after the "long" reset delay of 700ms, and stay lit as long as you are holding reset.  When you let go, they return to rainbow.  This helps you gage just how long you need to wait to hold the button for long reset.
- Added "scsi_command: %02X\n" which logs USB MSC commands to log output, this might seem chatty, but for now let's see what we see ;-)
- Set USB power to 500mA (was set to 100mA)

## v0.23

- Fixes issue where VEXTREME takes several minutes to mount as USB drive on Windows #53

## v0.22

- ignore all file extensions that are not .bin or .vec (case insensitive) #49
- increased quick reset delay to 700ms #45 
- adds HW/SW version to VEXTREME menu and games #48 
  - see example in LED Test v1.1.bin

## v0.21

- :warning: Added NOTICE of Current Development
- Fixes Polar Rescue and implements a new Reset Heurisic #37
- Adds addressable LED control and demo #41
    - added 3 functions to control the LEDs from a Vectrex program
        - updateOne() - RPC 6
        - updateAll() - RPC 4
        - updateMulti() - RPC 7
    - added RPC function 5 to step through rainbow control of LEDs in menu
    - added "LED TEST" which uses updateMulti to show how to finely control
    all of the LEDs with a test program.  Press UP on the joystick, try it slowly as well :D
    - added addressable LED control to Veccy Bird v1.5

## v0.2

- Replaced USB-B-mini with USB-C and centered USB on cart
- Extended the PCB by 1.7 mm to get the USB-C connector as close to the case exterior as possible.
- Widened the PCB to 48.0 mm so that there’s less side to side play in the cartridge slot.
- Moved outer mounting holes into the proper locations
- Reversed D3 and R4 order to get D3 closer to edge of the PCB, but it turned out we added something else there!
- Added 10 APA102-2020 addressable RGB LED lights (max. draw 245mA) … qualify with USB-IN (PA9) for 100% brightness, else 50%
- Added 47uF cap for peak currents required for RGB LEDs.
- Added V-IRQ connection to PB9 for 128KB bank-switching
- Grounded unused floating inputs on U3 to reduce current consumption
- Adjusted Y1 to use 12pF load crystal resonator with 18pF load caps (see equation)
- Lots of clean up and tweaking all over PCB
- Added revision to PCB
- Added “VEXTREME” to PCB
- Added a ground pour under the STM32
- Made the contacts on the edge connector thinner (1.52mm) and made sure they were spaced properly (2.54mm).
- Started moving footprints to the libs/veccart.pretty library so you know where they will be!
- Add basic folder support #1
- Ignore dotfiles generated by MacOS
- Add software driven addressible LED support
- improve USB write time for a single game by 3x (32KB in 1 second)
- adds 'clean' and 'all' rules for multicart build
- create 4KB of space for menu (currently about 2KB), and reserve 60KB for menu data.
- Added a mostly optimized 100 vector logo for VEXTREME
- Added a much improved version of Malban's 5 line font (everything except lowercase, don't use lower)
- Reduced menu to 4 items at a time
- Added a special intro jingle that's been stuck in my head (enjoy!)
- temp increase time to reset to allow Fortress of Narzod to play
- Extract menu code from main code and optimize menu code
- Add basic menu labels/function-per-button support #4
    - At the bottom of the menu 1 = (BACK), 2 = PAGE LEFT, 3 = PAGE RIGHT, 4 = SELECT
- Ignore lst files #5
- adds ISSUE_TEMPLATE.md and PULL_REQUEST_TEMPLATE.md for contributions from the community
- Update to latest (pinned) libopencm3 #6
- Fixed lastselectcart on wrong game after reset
- Fixed do not inc page if next one will be blank issue
- Converts logo from sync to smart list, thanks Malban!
    - Note: Had to replace NOP MACROs with JSR of approx equivalent value
    - Converted fcb hi(label),lo(label) into fdb label
    - Reduces cycle count by 8 - 10k!

## v0.1

- Mostly unchanged from Sprite_tm's original HW/SW design
- Mostly unchanged from Rattboi's original PCB design
- fixes error opening PCB with latest KiCaD
- add gerbers for easy PCB ordering at many places
- update README.md and add LICENSE
- fixes premature automatic reset delay on GCE Vectrex
- fixes menu builder missing filename in path
- pretty up the menu by removing the .BIN file extension, and other cleanup
- let's call it the VEXTREME cart, for now. (original name was EXTREME)
