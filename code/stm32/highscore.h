#ifndef HIGHSCORE_H
#define HIGHSCORE_H

// Includes
#include "fatfs/ff.h"

// Constants
#define MAX_GAME_NAME_SIZE	(40 + 1) // Max of 40 ASCII characters + NULL
#define MAX_GAME_SCORE_SIZE (6 + 1)  // Max of 6 ASCII characters + NULL

typedef enum
{
    HIGH_SCORE_SUCCESS               = 0,
    HIGH_SCORE_FILE_OPEN_FAIL        = 1,
    HIGH_SCORE_GAME_NOT_FOUND        = 2,
    HIGH_SCORE_GAME_NAME_TOO_LONG    = 3,
    UNUSED1                          = 4,
    HIGH_SCORE_GAME_NAME_SIZE_ZERO   = 5,
    HIGH_SCORE_GAME_NAME_INVALID_PTR = 6,
    UNUSED2                          = 5,
    HIGH_SCORE_WRITE_FAIL            = 7,
    HIGH_SCORE_INVALID_NAME          = 8,
    HIGH_SCORE_FAIL                  = 0xFFFFFFFF // 32-bit -1
} HighScoreRetVal;

typedef enum
{
    HIGH_SCORES_EQUAL       = 6,
    HIGH_SCORE1_LESS_SCORE2 = 7,
    HIGH_SCORE2_LESS_SCORE1 = 8
} HighScoreCompare;

// Sructures

// A fame file record is exactly 48 bytes
typedef struct
{
    unsigned char name[MAX_GAME_NAME_SIZE];  // NULL terminated name string
    unsigned char maxScore[MAX_GAME_SCORE_SIZE]; // NULL terminated high score string
} __attribute__((packed))GameFileRecord;


// The format of the highscore file will be:
// GameName, HighScore
// GameName will be the same format used in a ROM game cartridge: ASCII bytes followed by NULL
// HighScore will also the same format used at location 0xCBEB: 6 ACII bytes followed by NULL
// The file can hold as many games as we have space on the file system

// Prototypes
FRESULT highScoreOpenFile(void);
HighScoreRetVal highScoreGet(const unsigned char * pGame, GameFileRecord * pGameRecord);
HighScoreRetVal highScoreSetGameRecordToDefaults(const unsigned char * pGame, GameFileRecord * pGameRecord);
void highScoreSave(unsigned char * pScore);
HighScoreCompare highScoreCompare(const unsigned char * pScore1, const unsigned char * pScore2);
HighScoreRetVal highScoreStore(GameFileRecord * pGameRecord);
#endif // HIGHSCORE_H

