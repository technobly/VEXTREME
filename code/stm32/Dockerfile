FROM archlinux:20200605
LABEL Description="STM32 and libopencm3"

RUN pacman -Syu --noconfirm \
 && pacman -S make git python --noconfirm \
 && pacman -S arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-newlib --noconfirm

WORKDIR /build
RUN git clone https://github.com/libopencm3/libopencm3.git \
    && cd libopencm3 \
    && git checkout 90753950bbe10e87b8caabfb0bd6e1d195bb24b8 \
    && TARGETS=stm32/f4 make

WORKDIR /build/stm32
