#ifndef CART_OPTIONS_H
#define CART_OPTIONS_H
#include <assert.h>
#include "fatfs/ff.h"


typedef enum
{
    CART_OPTIONS_SUCCESS        = 0,
    CART_OPTIONS_FILE_OPEN_FAIL = 1,
    CART_OPTIONS_READ_FAIL      = 2,
    // CART_OPTIONS_WRITE_FAIL     = 3, // we don't write cart_options files for now
    CART_OPTIONS_INVALID_PTR    = 4,
    CART_OPTIONS_FAIL           = 0xFFFFFFFF // 32-bit -1
} CartOptionsRetVal;

enum RAM_FLAGS {
    RAM_DISABLED = 0, // Disable RAM support
    RAM_ENABLED = 1,  // Enable RAM support
};

#define CART_OPTIONS_VERSION (1)
// VERSION 1
typedef struct {
    uint16_t size;           // size of this struct
    uint16_t ver;            // cart_options version
    uint8_t  ram_flags;      // RAM flags
    uint8_t  ram_start_addr; // 0x20 for 0x2000
    uint8_t  ram_end_addr;   // 0x27 for 0x27ff
    uint8_t  reserved[64-7];
} __attribute__((packed)) cart_options;
static_assert(sizeof(cart_options)==64, "cart_options should be 64 bytes, check the reserved field.");

// Prototypes
bool cartOptionsFileExists(const char* filename);
FRESULT cartOptionsOpenFile(const char* filename);
CartOptionsRetVal cartOptionsGet(const char* filename, cart_options* pCartOptions);
#endif // CART_OPTIONS_H