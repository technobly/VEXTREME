# Lazy Developer Feature

>= v0.4 hardware has a diode and extra line going to PA10 from Vectrex 5V, which allows you to know when the Vectrex is on.  PA9 let's you similarly know when the USB is plugged in.

This feature is coined, "Lazy Developer" because it allows you to have the USB drive connected, and without ejecting the drive, you can just turn on the Vectrex and it will detect that and eject the drive for you.  I can't tell you how many times I've forgotten to eject the drive first and the Vectrex just boots to Mine Storm.  You're (we're) welcome!

Similarly, you can use Vide to compile and flash a cart.bin to VEXTREME (see [Developer Mode](../developer-mode.md), then just turn the Vectrex on and boot right into it.

Also when flashing STM32 code, you can leave everything plugged in (Vectrex & USB). Just have the Vectrex off or held in reset, and reset the STM32 out of DFU mode ...and because the Vectrex is on... it will skip mounting the USB drive.  Boom, faster testing!
