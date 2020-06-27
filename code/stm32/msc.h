#ifndef MSC_H
#define MSC_H

enum RAMDISK_TYPE {
    RAMDISK_NON_BLOCKING,
    RAMDISK_BLOCKING
};

int ramdiskmain(int blocking);

#endif
