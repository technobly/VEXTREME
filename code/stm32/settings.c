/*
 *  Copyright (C) 2020
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
// Includes
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "flash.h"
#include "xprintf.h"
#include "fatfs/ff.h"

#include "settings.h"

/**
 * Set DEBUG_SETTINGS to 1 to enable settings.c debugging, 0 to disable.
 */
#define DEBUG_SETTINGS (0)

#if (DEBUG_SETTINGS == 1)
    #define SET_XPRINTF(F, ...)  xprintf(F, ##__VA_ARGS__)
#else
    #define SET_XPRINTF(F, ...)
#endif

// Externs

// Globals

static FIL fileSettings;    // settings file pointer
static FILINFO fileInfoSettings;        // settings file info pointer
static FRESULT fResult = FR_NO_FILE;     // settings file return results

// Local functions

/**
 * Check if Settings file exists on flash file system.
 */
bool settingsFileExists(void)
{
    return f_stat("/settings.bin", &fileInfoSettings) == FR_OK;
}

/**
 * Open Settings file located on flash file system.  If it doesn't exist
 * create the file.
 */
FRESULT settingsOpenFile(void)
{
    fResult = f_open(&fileSettings, "/settings.bin", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);

    return fResult;
}

/**
 * Load the settings from file to memory structure
 *
 * @param[in] - pSettingsInRom - Points to settings structure
 *
 * @return - SETTINGS_SUCCESS or error code for failure
 */
SettingsRetVal settingsGet(SettingsRecord* pSettingsInRom)
{
    unsigned int bytesRead = 0;

    /**
     * Check for invalid parameter for settings address
     */
    if (pSettingsInRom == NULL)
    {
        SET_XPRINTF("ERROR: no settings pointer passed!\n");
        return (SETTINGS_INVALID_PTR);
    }

    /**
     * Check to make sure file was opened successfully
     */
    if (fResult != FR_OK)
    {
        // Try again!
        SET_XPRINTF("ERROR: retrying settings.bin file open!\n");
        fResult = settingsOpenFile();
        if (fResult != FR_OK) {
            return SETTINGS_FILE_OPEN_FAIL;
        }
    }

    /**
     * Move file pointer to beginning of the file
     */
    f_lseek(&fileSettings, 0);

     /**
     * Read a settings record from file
     */
    fResult = f_read(&fileSettings, pSettingsInRom, sizeof(SettingsRecord), 
                     &bytesRead);

    /**
     * Exit if EOF or another read issue
     */
    if ((fResult != FR_OK) || (bytesRead != sizeof(SettingsRecord)))
    {
        SET_XPRINTF("ERROR: f_read(): %u bytesRead: %u\n", fResult, bytesRead);
        return SETTINGS_READ_FAIL;
    }

    return SETTINGS_SUCCESS;
}

/**
 * Store the passed memory chunk into the fileSettings at the current write
 * pointer.
 *
 * @param[in] - Pointer to the game record to store
 *
 * @return - SETTINGS_SUCCESS or SETTINGS_WRITE_FAIL
 */
static SettingsRetVal settingsStore(const SettingsRecord* pSettingsInRom, size_t size)
{
    unsigned int bytesWrote = 0;

    /**
     * Check to make sure file was opened successfully
     */
    if (fResult != FR_OK)
    {
        // Try again!
        SET_XPRINTF("ERROR: retrying settings.bin file open!\n");
        fResult = settingsOpenFile();
        if (fResult != FR_OK) {
            return (SETTINGS_FILE_OPEN_FAIL);
        }
    }

    f_lseek(&fileSettings, 0);

    /**
     * Store settings memory chunk to file
     */
    fResult = f_write(&fileSettings, pSettingsInRom, size, &bytesWrote);

    if ( (bytesWrote < size) || (fResult != FR_OK) )
    {
        return SETTINGS_WRITE_FAIL;
    }

    /**
     * Flush the file data
     */
    f_sync(&fileSettings);

    /**
     * Flush record to flash device
     */
    flashDoWriteback();

    return SETTINGS_SUCCESS;
}


/**
 * Store settings to fs
 * @param[in] - Pointer to memory chunk containing settings
 */
void settingsSave(const SettingsRecord* pSettingsInRom)
{
    SettingsRetVal store_ret_val = settingsStore(pSettingsInRom, 
                                                 sizeof(SettingsRecord));
    SET_XPRINTF("settingsStore result: %d\n", store_ret_val);
    (void) store_ret_val;
}