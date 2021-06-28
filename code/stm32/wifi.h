#ifndef WIFI_H
#define WIFI_H

// Includes
#include "fatfs/ff.h"
#include <assert.h>

typedef enum
{
    WIFI_SUCCESS        = 0,
    WIFI_FILE_OPEN_FAIL = 1,
    WIFI_READ_FAIL      = 2,
    WIFI_WRITE_FAIL     = 3,
    WIFI_INVALID_PTR    = 4,
    WIFI_FAIL           = 0xFFFFFFFF // 32-bit -1
} WifiRetVal;

// Prototypes
bool wifiFileExists(void);
FRESULT wifiOpenFile(void);
WifiRetVal wifiSetup(void);
#endif // WIFI_H

