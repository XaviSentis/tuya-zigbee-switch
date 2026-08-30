#include "hal/flash.h"

// Provided by ota_reformating/ram_code_flash.c
extern void ram_code_flash_read_page(unsigned long addr, unsigned long len,
                                     unsigned char *buf);

int8_t hal_flash_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    ram_code_flash_read_page(addr, len, buf);
    return 0;
}
