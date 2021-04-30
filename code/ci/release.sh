#!/bin/bash

# Usage:
# $ USE_SW=<tagged-version-to-release> ./release.sh
# Example: USE_SW=v0.24 ./release.sh

# we are going for the 3 fingered claw approach (try <task>)
yell() { echo "$0: $*" >&2; }
die() { yell "$*"; fail; exit 111; }
try() { "$@" || die "cannot $*"; }

GRN="\033[32m"
RED="\033[31m"
NOC="\033[0m"

# Execute in ci/ folder
STM32_DIR=../stm32
MENU_DIR=../menu
RELEASE_DIR=../releases
SOURCE_RELEASE=MAIN
SOURCE_RELEASE_URL=https://github.com/technobly/VEXTREME/releases/download/v0.23/VEXTREME-v0.23.zip
: ${USE_SW:?"REQUIRED! Usage: USE_VER=v0.24 ./release.sh"}

main() {
    try rm -rf $RELEASE_DIR/$USE_SW
    try rm -f $RELEASE_DIR/vextreme-$USE_SW.zip
    if [ ! -d "$RELEASE_DIR/$SOURCE_RELEASE" ]; then
        if [ ! -e "$RELEASE_DIR/VEXTREME-v0.23.zip" ]; then
            cd $RELEASE_DIR
            curl -L "$SOURCE_RELEASE_URL" > "VEXTREME-v0.23.zip"
            unzip VEXTREME-v0.23.zip
        fi
    fi

    USE_HW=v0.2
    try cd $STM32_DIR
    try make clean all USE_HW=$USE_HW -s #&> /dev/null
    try mkdir -p $RELEASE_DIR
    try mkdir -p $RELEASE_DIR/$USE_SW
    try mkdir -p $RELEASE_DIR/$USE_SW/stm32
    try mkdir -p $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW
    try cp stm32.bin $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.bin
    try cp stm32.elf $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.elf
    try cp stm32.hex $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.hex
    try cp stm32.list $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.list

    USE_HW=v0.3
    try make clean all -s USE_HW=$USE_HW #&> /dev/null
    try mkdir -p $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW
    try cp stm32.bin $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.bin
    try cp stm32.elf $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.elf
    try cp stm32.hex $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.hex
    try cp stm32.list $RELEASE_DIR/$USE_SW/stm32/hardware-$USE_HW/stm32-sw.$USE_SW.hw.$USE_HW.list

    try cd $MENU_DIR
    try make clean all #&> /dev/null
    try mkdir -p $RELEASE_DIR/$USE_SW/VEXTREME
    try cp menu.bin $RELEASE_DIR/$USE_SW/VEXTREME/menu.bin
    try cp $RELEASE_DIR/$SOURCE_RELEASE/VEXTREME/vec.bin $RELEASE_DIR/$USE_SW/VEXTREME/vec.bin
    try cp -R $RELEASE_DIR/$SOURCE_RELEASE/VEXTREME/roms $RELEASE_DIR/$USE_SW/VEXTREME/roms

    try cd $RELEASE_DIR
    try zip -vr VEXTREME-$USE_SW.zip $USE_SW -x "*.DS_Store" #&> /dev/null

    pass
    return 0;
}

pass() {
    echo -e ${GRN}' 8888888b.     d8888  .d8888b.   .d8888b. '${NOC}
    echo -e ${GRN}' 888   Y88b   d88888 d88P  Y88b d88P  Y88b '${NOC}
    echo -e ${GRN}' 888    888  d88P888 Y88b.      Y88b. '${NOC}
    echo -e ${GRN}' 888   d88P d88P 888  "Y888b.    "Y888b. '${NOC}
    echo -e ${GRN}' 8888888P" d88P  888     "Y88b.     "Y88b. '${NOC}
    echo -e ${GRN}' 888      d88P   888       "888       "888 '${NOC}
    echo -e ${GRN}' 888     d8888888888 Y88b  d88P Y88b  d88P '${NOC}
    echo -e ${GRN}' 888    d88P     888  "Y8888P"   "Y8888P" '${NOC}
}

fail() {
    echo -e ${RED}' 8888888888     d8888 8888888 888      888 '${NOC}
    echo -e ${RED}' 888           d88888   888   888      888 '${NOC}
    echo -e ${RED}' 888          d88P888   888   888      888 '${NOC}
    echo -e ${RED}' 8888888     d88P 888   888   888      888 '${NOC}
    echo -e ${RED}' 888        d88P  888   888   888      888 '${NOC}
    echo -e ${RED}' 888       d88P   888   888   888      Y8P '${NOC}
    echo -e ${RED}' 888      d8888888888   888   888       "  '${NOC}
    echo -e ${RED}' 888     d88P     888 8888888 88888888 888 '${NOC}
}

main
