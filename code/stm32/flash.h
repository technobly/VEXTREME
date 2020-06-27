#ifndef FLASH_H
#define FLASH_H

void flashTick(void);
void flashInit(void);
void flashDoWriteback(void);
int flashReadBlk(uint32_t lba, uint8_t *copy_to);
int flashWriteBlk(uint32_t lba, const uint8_t *copy_from);

#endif
