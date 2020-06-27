#define CM3_ASSERT_VERBOSE
#include <libopencm3/cm3/assert.h>
#include <libopencm3/cm3/dwt.h>
#include "delay.h"
#include "xprintf.h"

#ifdef NDEBUG
    #error "asserts disabled, waaaaa!!"
#endif

__attribute__((__noreturn__))
void cm3_assert_failed_verbose(const char *file, int line,
    const char *func, const char *assert_expr) {
    xprintf("[%s:%d,%s] assert failed:(%s)\n",
        file, line, func, assert_expr);
    while(1); // die forever
}

void delay(uint32_t wait) {
    cm3_assert(wait < (0xFFFFFFFF / 120000)); // delay must be less than ~35.8s
    uint32_t start = dwt_read_cycle_counter() / 120000;
    while (millis() - start <= wait);
}

uint32_t millis(void) {
    return dwt_read_cycle_counter() / 120000;
}