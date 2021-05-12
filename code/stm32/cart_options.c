/*
 *  Copyright (C) 2021
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

#include "cart_options.h"

/**
 * Set DEBUG_SETTINGS to 1 to enable cart_options.c debugging, 0 to disable.
 */
#define DEBUG_SETTINGS (1)

#if (DEBUG_SETTINGS == 1)
    #define CART_OPT_XPRINTF(F, ...)  xprintf(F, ##__VA_ARGS__)
#else
    #define CART_OPT_XPRINTF(F, ...)
#endif

// Externs

// Globals

static FIL fileCartOptions;             // cart_options file pointer
static FILINFO fileInfoCartOptions;     // cart_options file info pointer
static FRESULT fResult = FR_NO_FILE;    // cart_options file return results

// Local functions

/**
 * Check if cart_options file exists on flash file system.
 */
bool cartOptionsFileExists(const char* filename)
{
    return f_stat(filename, &fileInfoCartOptions) == FR_OK;
}

/**
 * Open cart_options file located on flash file system.
 */
FRESULT cartOptionsOpenFile(const char* filename)
{
    fResult = FR_NO_FILE;

    if (cartOptionsFileExists(filename)) {
        CART_OPT_XPRINTF("%s exists, opening!\n", filename);
        fResult = f_open(&fileCartOptions, filename, FA_READ);
    }

    return fResult;
}

/**
 * Load the cart_options from file to memory structure
 *
 * @param[in] - filename - Points to game.ini file (cart_options file)
 * @param[in] - pCartOptions - Points to cart_options structure
 *
 * @return - CART_OPTIONS_SUCCESS or error code for failure
 */
CartOptionsRetVal cartOptionsGet(const char* filename, cart_options* pCartOptions)
{
    unsigned int bytesRead = 0;

    /**
     * Check for invalid parameter for cart_options address
     */
    if (pCartOptions == NULL)
    {
        CART_OPT_XPRINTF("ERROR: no cart_options pointer passed!\n");
        return (CART_OPTIONS_INVALID_PTR);
    }

    /**
     * If the file is not open already, open it!
     */
    if (fResult != FR_OK)
    {
        // Try to open the file...
        CART_OPT_XPRINTF("Attempting to open %s file...\n", filename);
        fResult = cartOptionsOpenFile(filename);
        if (fResult != FR_OK) {
            return CART_OPTIONS_FILE_OPEN_FAIL;
        }
    }

    /**
     * Move file pointer to beginning of the file
     */
    f_lseek(&fileCartOptions, 0);

     /**
     * Read a cart_options record from file
     */
    fResult = f_read(&fileCartOptions, pCartOptions, sizeof(cart_options), &bytesRead);

    /**
     * Exit if EOF or another read issue
     */
    if ((fResult != FR_OK) || (bytesRead != sizeof(cart_options)))
    {
        CART_OPT_XPRINTF("ERROR: f_read(): %u bytesRead: %u\n", fResult, bytesRead);
        return CART_OPTIONS_READ_FAIL;
    }

    if (fResult != FR_NO_FILE) {
        f_close(&fileCartOptions);
        fResult = FR_NO_FILE;
    }

    return CART_OPTIONS_SUCCESS;
}
