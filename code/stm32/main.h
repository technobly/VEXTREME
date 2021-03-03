#ifndef MAIN_H
#define MAIN_H

void uart_output_func(unsigned char c);
void loadMenu(void);
void loadRom(char *fn);
void loadRomWithHighScore(char *fn, bool load_hs_mode, bool use_embedded_menu);
void loadStreamData(int addr, int len);
void doUpDir(void);
void doChangeDir(char* dirname);
void doChangeRom(char* basedir, int i);
void doHandleEvent(int data);
void doDbgHook(int adr, int data);
void doLog(int data);
void updateAll(void);
void updateOne(void);
void updateMulti(void);
void loadVersions(void);
void doRamDisk(void);
void loadApp(void);
void doLedOn(int on);
void ledsCyan(void);
void ledsMagenta(void);
void ledsOff(void);
void loadSysOpt(void);
void dumpMemory(void);
void loadParmRam(void);
void storeToRom(void);
void syncSettings(void);
void applyLedSettings(bool initial);

#endif
