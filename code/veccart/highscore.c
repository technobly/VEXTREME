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

#include "highscore.h"

// Externs

// Globals

static FIL fileHighScore;   // Highscore file pointer
static FRESULT fResult;     // Highscore file return results

// Local variables
static GameFileRecord activeGameData;

// Global Varablies
GameFileRecord * pActiveGameData = &activeGameData;


// Local functions
static int highScoreGetFileNameSize(const unsigned char * pString);

/**
 * Open High score file locted on flash file system.  If it doesn't exist
 * create the file.
 */
FRESULT highScoreOpenFile(void)
{
    // Create/Open highscore file
    fResult = f_open(&fileHighScore, "/hs.bin", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);

    return fResult;
}

/**
 * Search for the game by name in the highscore file, and if it exists, 
 * read the game record associated with the name and store it in the
 * location pointed to by the second parameter (pGameRecord).
 *
 * @param[in] - pGame - Points to a 0x80 terminated game name
 * @param[out] - pGameRecord - Location to store game data record, or NULL
 *                             to store in out active game location
 *
 * @return - HIGH_SCORE_SUCCESS or error code for failure
 *           pGameRecord = Game record is stored here on success
 *           fileHighScore->fptr = EOF or start of record for the game
 */
HighScoreRetVal highScoreGet(const unsigned char * pGameName, GameFileRecord * pGameRecord)
{
    int retVal = HIGH_SCORE_GAME_NOT_FOUND;
    unsigned int bytesRead = 0;
    bool exit = false;

    /**
     * Check for invalid parameter for game name
     */
    if (pGameName == NULL)
    {
	return (HIGH_SCORE_GAME_NAME_INVALID_PTR);
    }
    
    /**
     * Check to make sure file was opened successfully
     */
    if (fResult != FR_OK)
    {
        return (HIGH_SCORE_FILE_OPEN_FAIL);
    }

    /**
     * Calculate size of the 0x80 terminated string passed into function
     */
    int gameNameSize = highScoreGetFileNameSize(pGameName);

    /**
     * Return failure if name of game is 0
     */
    if (gameNameSize == 0)
    {
        return (HIGH_SCORE_GAME_NAME_SIZE_ZERO);
    }

    /**
     * Exit if the game name passed to this function is larger than we allow
     */
    if (gameNameSize > (MAX_GAME_NAME_SIZE))
    {
        return (HIGH_SCORE_GAME_NAME_TOO_LONG);
    }

    /**
     * Set game record pointer to the active local game data pointer
     * if user passed in NULL
     */
    if (pGameRecord == NULL)
    {
        pGameRecord = pActiveGameData;
    }

    /**
     * Move file pointer to beginning of the file
     */
    f_lseek(&fileHighScore, 0);
    
    /**
     * Search for game in file
     */
    do
    {
        /**
         * Read a high score record from file
         */
        fResult = f_read(&fileHighScore, pGameRecord, sizeof(GameFileRecord), &bytesRead);

        /**
         * Exit if EOF or another read issue
         */
        if ((fResult != FR_OK) || (bytesRead == 0))
        {
            exit = true;    
        }

        /**
         * Compare passed in game name to game name read from file
         */
        if ( (0 == strncmp((char *)pGameName, (char *)pGameRecord->name, gameNameSize))  &&
	     (exit == false) )
        {
	    /**
	     * Move file pointer back to the beginning of the record for 
	     * the game, to prepare writing of the new high score if it
	     * is beat.
	     */
	    f_lseek(&fileHighScore, f_tell(&fileHighScore) - sizeof(GameFileRecord));

     	    /**
	     * Found matching game
	     */
	    exit = true;
	    retVal = HIGH_SCORE_SUCCESS;
	}
    } while (exit == false);

    return retVal;
}

/**
 * Initialize a game record to the defaults and add the name pointed to by pGameName
 * @param[in] - pGameName - Pointer to the game name in the ROM - 0x80 terminated
 * @param[in] - pGameRecord - Pointer to a game record, or NULL (If NULL, use active)
 *
 * @return - HIGH_SCORE_SUCCESS, or error code
 */
HighScoreRetVal highScoreSetGameRecordToDefaults(const unsigned char * pGameName, GameFileRecord * pGameRecord)
{
    int retVal = HIGH_SCORE_SUCCESS;
    char defaultScore[] = {' ', ' ', ' ', ' ', ' ', '0', '\0'};

    /**
     * Check for invalid parameter for game name
     */
    if (pGameName == NULL)
    {
        return (HIGH_SCORE_GAME_NAME_INVALID_PTR);
    }

    /**
     * Set game record pointer to the active local game data pointer
     * if user passed in NULL
     */
    if (pGameRecord == NULL)
    {
        pGameRecord = pActiveGameData;
    }

    /**
     * Calculate size of the 0x80 terminated string passed into function
     */
    int gameNameSize = highScoreGetFileNameSize(pGameName);

    /**
     * Exit if the game name passed to this function is larger than we allow
     */
    if (gameNameSize > (MAX_GAME_NAME_SIZE - 1))
    {
        return (HIGH_SCORE_GAME_NAME_TOO_LONG);
    }

    /**
     * Return failure if name of game is 0
     */
    if (gameNameSize == 0)
    {
        return (HIGH_SCORE_GAME_NAME_SIZE_ZERO);
    }

    /**
     * Initialize the high score to default of zero
     */
    strncpy((char *)pGameRecord->maxScore, defaultScore, MAX_GAME_SCORE_SIZE);

    /**
     * Move name into active game record name field, followed by a NULL byte
     */
    int count = 0;
    for (count = 0; (count < MAX_GAME_NAME_SIZE) && (count < gameNameSize); count++)
    {
        pGameRecord->name[count] = pGameName[count];
    }

    /**
     * NULL terminate the string for storing in our game records
     */

    for (; count < MAX_GAME_NAME_SIZE; count++)
    {
        pGameRecord->name[count] = '\0';
    }

    return retVal;
}

/**
 * Return the size of the games name string, excluding the trailing 0x80
 *
 * param[in] - pString - 0x80 terminated game name string as found in 
 * cartridge.
 *
 * TODO - Update to grab the second portion of the string.  Look up to 0x80 0x00
 * then back up to get both strings
 */
static int highScoreGetFileNameSize(const unsigned char * pString)
{
    int count = 0;

    for (count = 0; count < MAX_GAME_NAME_SIZE; count++)
    {
	if (pString[count] == 0x80)
	{
	    break;
	}
    }
    return count;
}

/**
 * If the score pointed to by pScore is larger than the current maximum
 * score stored in the active game data record, then update it and
 * write it to the high score file.
 *
 * @param[in] - Pointer to 0x80 terminated high score
 */
void highScoreSave(unsigned char * pScore)
{
    int i = 0;

    /**
     * Compare this new high score to see if it is greater
     * than the overall high score for this game stored in 
     * the highScore file.
     */
    HighScoreCompare retVal = highScoreCompare(pActiveGameData->maxScore, pScore);

    /**
     * If the new score in pScore is greater than the current max score we read from
     * the file, then we need to update the active game data record with the new
     * high score so it will be saved to the file.
     */
    if (retVal == HIGH_SCORE1_LESS_SCORE2)
    {
        /**
         * Move new high score into global current game record
         */
        for (i = 0; i < (MAX_GAME_SCORE_SIZE - 1); i++)
        {
           pActiveGameData->maxScore[i]  = pScore[i];
        }

        /**
         * Add '\0' to end of string
         */
        pActiveGameData->maxScore[i]  = '\0';

	/**
	 * Write the updated game record to the file
	 * Note: The write pointer should be correct, either the end
	 * of the file for a new game, or the beginning of the game
	 * record where the update needs to happen.
	 */
	(void)highScoreStore(pActiveGameData);
    }
}

/**
 * Store the game record into the fileHighScore at the current write
 * pointer.
 *
 * @param[in] - Pointer to the game record to store
 *
 * @return - HIGH_SCORE_SUCCESS or HIGH_SCORE_WRITE_FAIL
 */
HighScoreRetVal highScoreStore(GameFileRecord * pGameRecord)
{
    unsigned int bytesWrote = 0;

    /**
     * Set game record pointer to the active local game data pointer
     * if user passed in NULL
     */
    if (pGameRecord == NULL)
    {
        pGameRecord = pActiveGameData;
    }

    if (pGameRecord->name[0] == 0x00)
    {
        return HIGH_SCORE_INVALID_NAME;
    }

    /**
     * Store a high score record to the file
     */
    fResult = f_write(&fileHighScore, pGameRecord, sizeof(GameFileRecord), &bytesWrote);

    if ( (bytesWrote < sizeof(GameFileRecord)) || (fResult != FR_OK) )
    {
        return HIGH_SCORE_WRITE_FAIL;
    }

    /**
     * Flush the file data
     */
    f_sync(&fileHighScore);

    /**
     * Flush record to flash device
     */
    flashDoWriteback();

    return HIGH_SCORE_SUCCESS;
}

/**
 * Compare two scores in string BCD format with preceding
 * spaces as kept by the vectrex. 
 * Note: A maximum of six bytes are compared. 
 *
 * @param[in] - pScore1 - Pointer to first score
 * @param[in] - pScore2 - Pointer to second score
 *
 * @return - HIGH_SCORES_EQUAL, HIGH_SCORE1_LESS_SCORE2,
 * HIGH_SCORE2_LESS_SCORE1
 *
 */
HighScoreCompare highScoreCompare(const unsigned char * pScore1, const unsigned char * pScore2)
{
    HighScoreCompare retVal = HIGH_SCORES_EQUAL;

    int i = 0;
    for (i = 0; i < (MAX_GAME_SCORE_SIZE - 1); i++)
    {
        if (pScore1[i] < pScore2[i])
	{
            retVal = HIGH_SCORE1_LESS_SCORE2;
	    break;
	}
	else if (pScore2[i] < pScore1[i])
	{
            retVal = HIGH_SCORE2_LESS_SCORE1;
	    break;
	}
    }
    return (retVal);
}
