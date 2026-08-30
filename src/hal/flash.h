#ifndef _HAL_FLASH_H_
#define _HAL_FLASH_H_

#include <stdint.h>

/**
 * Read raw bytes from the device flash.
 * @return 0 on success, -1 if unsupported on this platform
 */
int8_t hal_flash_read(uint32_t addr, uint32_t len, uint8_t *buf);

#endif
