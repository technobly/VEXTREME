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

#include "wifi.h"

/**
 * Set DEBUG_WIFI to 1 to enable wifi.c debugging, 0 to disable.
 */
#define DEBUG_WIFI (0)

#if (DEBUG_WIFI == 1)
    #define WIFI_XPRINTF(F, ...)  xprintf(F, ##__VA_ARGS__)
#else
    #define WIFI_XPRINTF(F, ...)
#endif

// Externs

// Globals
#define DEFAULT_WIFI_FILE "# Add your Wi-Fi credentials\n" \
        "# For AUTH, enter 1, 2 or 3 based on WEP(1) WPA(2) WPA2(3)\n" \
        "# For CIPHER, enter 1, 2 or 3 based on AES(1) TKIP(2) AES&TKIP(3)\n" \
        "# Note 1: If you don't know your auth & cipher, it's likely WPA2=3 & AES=1\n" \
        "# Note 2: Please do not add any spaces around the =\n" \
        "# Note 3: To clear Wi-Fi credentials, simply clear the value for SSID or\n" \
        "#         PASSWORD below, e.g. PARAM=\n" \
        "#         without anything after the =, and then save the wifi.txt file.\n" \
        "# Note 4: Your credentials are consumed and encrypted by VEXTREME,\n" \
        "#         and this file will be restored to default settings.\n" \
        "\n" \
        "SSID=MyNetworkName\n" \
        "PASSWORD=MyPassword12345\n" \
        "AUTH=3\n" \
        "CIPHER=1\n" \
        "\n" \
        "# END OF FILE\n"
#define DEFAULT_WIFI_FILE_SIZE (669)

static FIL     fileWifi;                // wifi file pointer
static FILINFO fileInfoWifi;            // wifi file info pointer
static FRESULT fResult = FR_NO_FILE;    // wifi file return results

// Local functions

/**
 * Check if wifi file exists on flash file system.
 */
bool wifiFileExists(void)
{
    return f_stat("/wifi.txt", &fileInfoWifi) == FR_OK;
}

/**
 * Open wifi file located on flash file system.  If it doesn't exist
 * create the file.
 */
FRESULT wifiOpenFile(void)
{
    fResult = FR_NO_FILE;
    unsigned int bytesWrote = 0;

    bool exist = wifiFileExists();
    fResult = f_open(&fileWifi, "/wifi.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    if (!exist) {
        /**
         * Create a default wifi.txt file since one doesn't exist
         */
        fResult = f_write(&fileWifi, DEFAULT_WIFI_FILE, DEFAULT_WIFI_FILE_SIZE, &bytesWrote);

        if (bytesWrote < DEFAULT_WIFI_FILE_SIZE || fResult != FR_OK)
        {
            return fResult;
        }

        /**
         * Flush the file data
         */
        f_sync(&fileWifi);

        /**
         * Flush record to flash device
         */
        flashDoWriteback();
    }

    return fResult;
}

/**
 * Load the wifi credentials from file and send to Photon
 *
 * @return - WIFI_SUCCESS or error code for failure
 */
WifiRetVal wifiSetup(void)
{
    unsigned int bytesRead = 0;
    char wifiData[DEFAULT_WIFI_FILE_SIZE + 64 + 64 + 2 + 2] = {0};
    char wifiSsid[64] = {0};
    char wifiPassword[64] = {0};
    char wifiAuth[2] = {0};
    char wifiCipher[2] = {0};

    /**
     * Check to make sure file was opened successfully
     */
    if (fResult != FR_OK)
    {
        WIFI_XPRINTF("Attempting to open wifi.txt file...\n");
        fResult = wifiOpenFile();
        if (fResult != FR_OK) {
            return WIFI_FILE_OPEN_FAIL;
        }
    }

    /**
     * Move file pointer to beginning of the file
     */
    f_lseek(&fileWifi, 0);

     /**
     * Read a wifi info from file
     */
    fResult = f_read(&fileWifi, wifiData, sizeof(wifiData), &bytesRead);

    /**
     * Exit if EOF or another read issue
     */
    if ((fResult != FR_OK) || (bytesRead == 0))
    {
        WIFI_XPRINTF("ERROR: f_read(): %u bytesRead: %u\n", fResult, bytesRead);
        return WIFI_READ_FAIL;
    }

    if (fResult != FR_NO_FILE) {
        f_close(&fileWifi);
        fResult = FR_NO_FILE;
    }

    // WIFI_XPRINTF("READ OK: f_read(): %u bytesRead: %u\n", fResult, bytesRead);
    // WIFI_XPRINTF("WIFI DATA:%s\n", wifiData);

    char* ptr1 = strstr(wifiData, "SSID=");
    char* ptr2 = strstr(wifiData, "PASSWORD=");
    char* ptr3 = strstr(wifiData, "AUTH=");
    char* ptr4 = strstr(wifiData, "CIPHER=");
    if (ptr1 && ptr2 && ptr3 && ptr4) {
        WIFI_XPRINTF("ssid\n");
        ptr1 += 5; // skip past "ssid="
        unsigned int bytes = ptr1 - wifiData;
        for (unsigned int x = 0; (*ptr1 != '\r') && (*ptr1 != '\n') && (*ptr1 != ' ') && (x < sizeof(wifiSsid)-1) && (bytes < bytesRead); bytes++) {
            WIFI_XPRINTF("x:%d-%c\n", x, *ptr1);
            wifiSsid[x++] = *ptr1++;
        }
        WIFI_XPRINTF("password\n");
        ptr2 += 9; // skip past "password="
        bytes = ptr2 - wifiData;
        for (unsigned int x = 0; (*ptr2 != '\r') && (*ptr2 != '\n') && (*ptr2 != ' ') && (x < sizeof(wifiPassword)-1) && (bytes < bytesRead); bytes++) {
            WIFI_XPRINTF("x:%d-%c\n", x, *ptr2);
            wifiPassword[x++] = *ptr2++;
        }
        WIFI_XPRINTF("auth\n");
        ptr3 += 5; // skip past "auth="
        bytes = ptr3 - wifiData;
        for (unsigned int x = 0; (*ptr3 != '\r') && (*ptr3 != '\n') && (*ptr3 != ' ') && (x < sizeof(wifiAuth)-1) && (bytes < bytesRead); bytes++) {
            WIFI_XPRINTF("x:%d-%c\n", x, *ptr3);
            wifiAuth[x++] = *ptr3++;
        }
        WIFI_XPRINTF("cipher\n");
        ptr4 += 7; // skip past "cipher="
        bytes = ptr4 - wifiData;
        for (unsigned int x = 0; (*ptr4 != '\r') && (*ptr4 != '\n') && (*ptr4 != ' ') && (x < sizeof(wifiCipher)-1) && (bytes < bytesRead); bytes++) {
            WIFI_XPRINTF("x:%d-%c\n", x, *ptr4);
            wifiCipher[x++] = *ptr4++;
        }
        // Only set Wi-Fi credentials if Network or Password is not the default.
        if (strcmp(wifiSsid,"MyNetworkName")!=0 || strcmp(wifiPassword,"MyPassword12345")!=0) {
            xprintf("WIFI_SSID[%s]PASSWORD[%s]AUTH[%s]CIPHER[%s]END\n", wifiSsid, wifiPassword, wifiAuth, wifiCipher);

            f_unlink("/wifi.txt");

            wifiOpenFile();

            if (fResult != FR_NO_FILE) {
                f_close(&fileWifi);
                fResult = FR_NO_FILE;
            }
        }
    } else {
        WIFI_XPRINTF("no valid Wi-Fi credentials, or not entered\n");
    }

    return WIFI_SUCCESS;
}
