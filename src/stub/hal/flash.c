#include "hal/flash.h"

#include <stdio.h>
#include <string.h>

// Stub: backed by ./stub_flash.bin when present, zeros otherwise
int8_t hal_flash_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    memset(buf, 0xFF, len);
    FILE *f = fopen("stub_flash.bin", "rb");
    if (f == NULL) {
        return 0;
    }
    if (fseek(f, addr, SEEK_SET) == 0) {
        size_t got = fread(buf, 1, len, f);
        (void)got;
    }
    fclose(f);
    return 0;
}
