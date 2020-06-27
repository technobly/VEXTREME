FROM alpine:3.9.4

RUN apk add --no-cache --virtual .build-deps build-base wget \
    && wget -O asm6809.tar.gz http://www.6809.org.uk/asm6809/dl/asm6809-2.12.tar.gz \
    && tar -xvf asm6809.tar.gz -C /tmp \
    && rm asm6809.tar.gz \
    && cd /tmp/asm6809-2.12 \
    && ./configure \
    && make \
    && make install \
    && rm -rf /tmp/asm6809-2.12 \
    && apk del .build-deps \
    && apk add --no-cache make

WORKDIR /build
