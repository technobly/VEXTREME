FROM frolvlad/alpine-glibc:glibc-2.33

WORKDIR /build

RUN apk --update --no-cache upgrade && \
    # Base utils \
    apk --update --no-cache add make && \
    apk --update --no-cache add python2 && \
    apk --update --no-cache add python3 && \
    apk --update --no-cache add bash && \
    apk --update --no-cache add --virtual build-dependencies wget openssl ca-certificates git && \
    # Install arm-none-eabi-gcc \
    wget -O /tmp/gcc-arm-none-eabi.tar.bz2 "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2" && \
    tar xvf /tmp/gcc-arm-none-eabi.tar.bz2 --strip-components=1 -C /usr/local && \
    rm -rf /tmp/gcc-arm-none-eabi.tar.bz2 && \
    rm -rf /usr/local/share/doc && \
    # Install libopencm3 \
    git clone https://github.com/libopencm3/libopencm3.git && \
    cd libopencm3 && \
    git checkout 90753950bbe10e87b8caabfb0bd6e1d195bb24b8 && \
    TARGETS=stm32/f4 make && \
    # Install uf2conf.py \
    wget -O /tmp/uf2conv.py https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2conv.py && \
    mv /tmp/uf2conv.py /usr/local/bin/uf2conv.py && \
    chmod +x /usr/local/bin/uf2conv.py && \
    wget -O /usr/local/bin/uf2families.json https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2families.json && \
    # Cleanup \
    apk del build-dependencies

WORKDIR /build/stm32
