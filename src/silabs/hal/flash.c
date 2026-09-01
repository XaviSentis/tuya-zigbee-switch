#include "hal/flash.h"

// OEM config scanning targets Tuya Telink devices; not supported here.
int8_t hal_flash_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    (void)addr; (void)len; (void)buf;
    return -1;
}
