/*
 * vxe_shim.c - PAYLOAD side: the vxe_* stand-ins that vxe_redirect.h points
 * the game and toolkit at, each forwarding through the VxeKernelApi table.
 * Compiled WITHOUT vxe_redirect.h (it must see the real kernel types and must
 * not rename itself).
 *
 * Nothing here may run before vxe_shim_bind() - vxe_payload_init() calls it
 * first, and the kernel calls nothing else in the payload before that.
 */
#include <stdarg.h>
#include "vxe_api.h"

/* Declarations of the stand-ins and pointers (the same prototypes the payload
 * sees through vxe_redirect.h; repeated here because this file must not
 * include that header). */
extern char           **vxe_romData_pp;
extern char           **vxe_cartData_pp;
extern unsigned char  (*vxe_parmRam_p)[256];
extern GameFileRecordV1 **vxe_pActiveGameData_pp;
FRESULT vxe_f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT vxe_f_close(FIL *fp);
TCHAR  *vxe_f_gets(TCHAR *buff, int len, FIL *fp);
FRESULT vxe_f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);
FRESULT vxe_f_sync(FIL *fp);
FRESULT vxe_f_rename(const TCHAR *path_old, const TCHAR *path_new);
FRESULT vxe_f_unlink(const TCHAR *path);
void     vxe_flashDoWriteback(void);
void     vxe_doLedOn(int on);
void     vxe_ledsClear(void);
void     vxe_ledsUpdate(void);
void     vxe_rainbowStep(uint8_t step);
uint32_t vxe_millis(void);
uint32_t vxe_dwt_read_cycle_counter(void);
void     vxe_xprintf(const char *fmt, ...);
HighScoreRetVal vxe_highScoreGet(const unsigned char *pGame, GameFileRecordV1 *pGameRecord);
HighScoreRetVal vxe_highScoreSetGameRecordToDefaults(const unsigned char *pGame, GameFileRecordV1 *pGameRecord);
void     vxe_highScoreSave(unsigned char *pScore);
void     vxe_menuScoreSet(const unsigned char score7[7]);

static const VxeKernelApi *K;

char           **vxe_romData_pp;
char           **vxe_cartData_pp;
unsigned char  (*vxe_parmRam_p)[256];
GameFileRecordV1 **vxe_pActiveGameData_pp;

void vxe_shim_bind(const VxeKernelApi *api)
{
    K = api;
    vxe_romData_pp         = api->k_romData_pp;
    vxe_cartData_pp        = api->k_cartData_pp;
    vxe_parmRam_p          = api->k_parmRam_p;
    vxe_pActiveGameData_pp = api->k_pActiveGameData_pp;
}

FRESULT vxe_f_open(FIL *fp, const TCHAR *path, BYTE mode)             { return K->k_f_open(fp, path, mode); }
FRESULT vxe_f_close(FIL *fp)                                         { return K->k_f_close(fp); }
TCHAR  *vxe_f_gets(TCHAR *buff, int len, FIL *fp)                    { return K->k_f_gets(buff, len, fp); }
FRESULT vxe_f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)  { return K->k_f_write(fp, buff, btw, bw); }
FRESULT vxe_f_sync(FIL *fp)                                          { return K->k_f_sync(fp); }
FRESULT vxe_f_rename(const TCHAR *path_old, const TCHAR *path_new)   { return K->k_f_rename(path_old, path_new); }
FRESULT vxe_f_unlink(const TCHAR *path)                              { return K->k_f_unlink(path); }
void     vxe_flashDoWriteback(void)        { K->k_flashDoWriteback(); }
void     vxe_doLedOn(int on)               { K->k_doLedOn(on); }
void     vxe_ledsClear(void)               { K->k_ledsClear(); }
void     vxe_ledsUpdate(void)              { K->k_ledsUpdate(); }
void     vxe_rainbowStep(uint8_t step)     { K->k_rainbowStep(step); }
uint32_t vxe_millis(void)                  { return K->k_millis(); }
uint32_t vxe_dwt_read_cycle_counter(void)  { return K->k_dwt_read_cycle_counter(); }

void vxe_xprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    K->k_xvprintf(fmt, ap);
    va_end(ap);
}

HighScoreRetVal vxe_highScoreGet(const unsigned char *pGame, GameFileRecordV1 *pGameRecord)
{
    return K->k_highScoreGet(pGame, pGameRecord);
}
HighScoreRetVal vxe_highScoreSetGameRecordToDefaults(const unsigned char *pGame, GameFileRecordV1 *pGameRecord)
{
    return K->k_highScoreSetGameRecordToDefaults(pGame, pGameRecord);
}
void vxe_highScoreSave(unsigned char *pScore)              { K->k_highScoreSave(pScore); }
void vxe_menuScoreSet(const unsigned char score7[7])       { K->k_menuScoreSet(score7); }
