#include "oem_scanner.h"

#include <stddef.h>
#include <string.h>

#include "hal/flash.h"
#include "hal/printf_selector.h"

#define SCAN_START      0x00000UL
#define SCAN_END        0x100000UL /* 1 MB */
#define CHUNK           240
#define OVERLAP         16
#define BLOCK_MAX       (sizeof(((oem_dump_str_t *)0)->data))

oem_dump_str_t oem_dump_str = { 0, { 0 } };

static const char needle[] = "rl1_pin";
#define NEEDLE_LEN (sizeof(needle) - 1)

static void set_result(const char *s) {
    uint16_t n = (uint16_t)strlen(s);
    if (n > BLOCK_MAX) { n = BLOCK_MAX; }
    memcpy(oem_dump_str.data, s, n);
    oem_dump_str.size = n;
}

static int32_t find_needle(const uint8_t *buf, uint32_t len) {
    if (len < NEEDLE_LEN) { return -1; }
    for (uint32_t i = 0; i + NEEDLE_LEN <= len; i++) {
        if (buf[i] == needle[0] && memcmp(buf + i, needle, NEEDLE_LEN) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static uint8_t extract_block(uint32_t anchor_addr) {
    uint32_t start = anchor_addr;
    uint32_t back_limit = (anchor_addr > 1024) ? anchor_addr - 1024 : 0;
    uint8_t  b = 0;
    while (start > back_limit) {
        if (hal_flash_read(start, 1, &b) != 0) { return 0; }
        if (b == '{') { break; }
        start--;
    }
    if (b != '{') { return 0; }
    uint16_t n = 0;
    uint32_t addr = start;
    while (n < BLOCK_MAX) {
        if (hal_flash_read(addr, 1, &b) != 0) { return 0; }
        oem_dump_str.data[n++] = (char)b;
        if (b == '}') { break; }
        addr++;
    }
    oem_dump_str.size = n;
    printf("oem-scan: OEM config at 0x%x (%d bytes)\r\n",
           (unsigned int)start, (int)n);
    return 1;
}

void oem_scanner_run(void) {
    uint8_t buf[CHUNK + OVERLAP];
    if (hal_flash_read(0, OVERLAP, buf) != 0) {
        set_result("oem-scan:unsupported");
        return;
    }
    for (uint32_t addr = SCAN_START; addr < SCAN_END; addr += CHUNK) {
        uint32_t len = CHUNK + OVERLAP;
        if (addr + len > SCAN_END) { len = SCAN_END - addr; }
        if (hal_flash_read(addr, len, buf) != 0) { break; }
        int32_t hit = find_needle(buf, len);
        if (hit < 0) { continue; }
        if (extract_block(addr + (uint32_t)hit)) { return; }
    }
    set_result("oem-scan:not-found");
    printf("oem-scan: no OEM config found in 1MB\r\n");
}
