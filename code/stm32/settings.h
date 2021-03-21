#ifndef SETTINGS_H
#define SETTINGS_H

// Includes
#include "fatfs/ff.h"
#include <assert.h>

typedef enum
{
    SETTINGS_SUCCESS = 0,
    SETTINGS_FILE_OPEN_FAIL = 1,
    SETTINGS_READ_FAIL = 2,
    SETTINGS_WRITE_FAIL = 3,
    SETTINGS_INVALID_PTR = 4,
    SETTINGS_FAIL    = 0xFFFFFFFF // 32-bit -1
} SettingsRetVal;

// Sructures
#define SETTINGS_RECORD_VERSION (1)
typedef struct
{
    uint8_t max_lines;
    uint8_t max_chars;
    uint8_t led_mode; 
    uint8_t led_luma; 
    uint8_t led_red;  
    uint8_t led_green;
    uint8_t led_blue; 
    uint8_t ss_mode; 
    uint8_t ss_delay; 
    uint8_t last_cursor;
    char    directory[_MAX_LFN + 1];
    uint16_t size;                      // size of this struct
    uint16_t ver;                       // SettingsRecord version
    uint8_t reserved[1024 - (_MAX_LFN+1) - 14];
} __attribute__((packed))SettingsRecord;
static_assert(sizeof(SettingsRecord)==1024, "SettingsRecord should be 1024 bytes, check the reserved field.");

// Prototypes
bool settingsFileExists(void);
FRESULT settingsOpenFile(void);
SettingsRetVal settingsGet(SettingsRecord* pSettingsInRom);
void           settingsSave(const SettingsRecord* pSettingsInRom);
#endif // SETTINGS_H

