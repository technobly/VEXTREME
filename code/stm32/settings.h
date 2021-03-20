#ifndef SETTINGS_H
#define SETTINGS_H

// Includes
#include "fatfs/ff.h"

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
} __attribute__((packed))SettingsRecord;

// settings file format reflect the chunk of memory in ROM dedicated to settings vars
// @TODO: detect settings structure change and wipe settings file

// Prototypes
SettingsRetVal settingsGet(char * pSettingsInRom);
void           settingsSave(const char * pSettingsInRom);
#endif // SETTINGS_H

