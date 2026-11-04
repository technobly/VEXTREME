/*
 * vxe_redirect.h - force-included (-include) into every PAYLOAD object: the
 * game, vxt/ and gamelib/ (Makefile: PAYLOAD_OBJS). Do not include it by hand
 * or into a kernel file or vxe_shim.c.
 *
 * It renames every kernel symbol the payload uses to a vxe_* stand-in defined
 * in vxe_shim.c, which forwards through VxeKernelApi. The game sources stay
 * as copied from their own trees.
 *
 * Functions are renamed with OBJECT-like macros (f_open -> vxe_f_open), so the
 * kernel headers' own prototypes still parse - they declare the stand-in.
 * Variables use the pointer form (romData -> (*vxe_romData_pp)): an existing
 * `extern char* romData;` in a payload file then reads
 * `extern char* (*vxe_romData_pp);`, which is a valid declaration of the same
 * char** declared below, and `romData[i]` reads through the kernel's live
 * pointer (the kernel repoints romData between menu and cart).
 *
 * The list is exactly what the payloads need, measured with nm over the objects of
 * the games built so far; `make` re-checks it on
 * every build (payload-check). Add to it - and to VxeKernelApi - only when
 * that check names a new symbol.
 */
#ifndef VXE_REDIRECT_H
#define VXE_REDIRECT_H

#include "vxe_api.h"            /* pulls in ff.h and highscore.h, real names */

/* WHAT IS DECLARED HERE, AND WHAT IS NOT. ff.h and highscore.h are parsed
 * above with their REAL names, so their stand-ins (vxe_f_*, vxe_highScore*)
 * are declared below - the payload's own `#include "ff.h"` is then a guarded
 * no-op. Every other renamed symbol is declared by the payload itself, under
 * the new name, when it includes the kernel header it always included
 * (flash.h, led.h, delay.h, xprintf.h, libopencm3's dwt.h) or writes its own
 * `extern` (vxt_frame.c's romData/cartData, vxt_rpc.c's parmRam, a game's
 * pActiveGameData or doLedOn). Declaring those here too would only
 * duplicate them (-Wredundant-decls); a payload file that used one without
 * declaring it would already have failed to compile before VXE. */

/* ---- shared variables (vxe_shim.c holds the pointers) ---- */
#define romData          (*vxe_romData_pp)
#define cartData         (*vxe_cartData_pp)
#define parmRam          (*vxe_parmRam_p)
#define pActiveGameData  (*vxe_pActiveGameData_pp)

/* ---- functions (vxe_shim.c) ---- */
FRESULT vxe_f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT vxe_f_close(FIL *fp);
TCHAR  *vxe_f_gets(TCHAR *buff, int len, FIL *fp);
FRESULT vxe_f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);
FRESULT vxe_f_sync(FIL *fp);
FRESULT vxe_f_rename(const TCHAR *path_old, const TCHAR *path_new);
FRESULT vxe_f_unlink(const TCHAR *path);
HighScoreRetVal vxe_highScoreGet(const unsigned char *pGame, GameFileRecordV1 *pGameRecord);
HighScoreRetVal vxe_highScoreSetGameRecordToDefaults(const unsigned char *pGame, GameFileRecordV1 *pGameRecord);
void     vxe_highScoreSave(unsigned char *pScore);
void     vxe_menuScoreSet(const unsigned char score7[7]);   /* no kernel header declares it */

#define f_open                            vxe_f_open
#define f_close                           vxe_f_close
#define f_gets                            vxe_f_gets
#define f_write                           vxe_f_write
#define f_sync                            vxe_f_sync
#define f_rename                          vxe_f_rename
#define f_unlink                          vxe_f_unlink
#define flashDoWriteback                  vxe_flashDoWriteback
#define doLedOn                           vxe_doLedOn
#define ledsClear                         vxe_ledsClear
#define ledsUpdate                        vxe_ledsUpdate
#define rainbowStep                       vxe_rainbowStep
#define millis                            vxe_millis
#define dwt_read_cycle_counter            vxe_dwt_read_cycle_counter
#define xprintf                           vxe_xprintf
#define highScoreGet                      vxe_highScoreGet
#define highScoreSetGameRecordToDefaults  vxe_highScoreSetGameRecordToDefaults
#define highScoreSave                     vxe_highScoreSave

#endif /* VXE_REDIRECT_H */
