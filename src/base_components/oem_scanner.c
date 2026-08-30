#include "oem_scanner.h"

#include <string.h>

#include "hal/flash.h"
#include "hal/printf_selector.h"

/*
 * Tuya factory OEM config scanner.
 *
 * Stock Tuya firmware stores a per-device JSON config in flash, written
 * at the factory, with the metering pin mapping and calibration
 * (ele_pin / vi_pin / sel_pin_pin / resistor / vol_def ...). An OTA
 * conversion replaces the application area but normally leaves that
 * region untouched, so on converted devices the JSON is still there.
 *
 * This scanner runs once at boot, searches the flash for the "ele_pin"
 * key, extracts the enclosing JSON object and exposes it through a
 * read-only Basic cluster attribute (0xFF01) so the pin mapping can be
 * recovered over the air — no teardown needed.
 */

#define SCAN_START      0x00000UL
#define SCAN_END        0x80000UL /* 512 KB */
#define CHUNK           240
#define OVERLAP         16
#define WINDOW          640

oem_dump_str_t oem_dump_str = { 0, { 0 } };

static const char needle[] = "ele_pin";
#define NEEDLE_LEN (sizeof(needle) - 1)

static void set_result(const char *s) {
    uint16_t n = (uint16_t)strlen(s);
    if (n > sizeof(oem_dump_str.data)) {
        n = sizeof(oem_dump_str.data);
    }
    memcpy(oem_dump_str.data, s, n);
    oem_dump_str.size = n;
}

static int32_t find_needle(const uint8_t *buf, uint32_t len) {
    if (len < NEEDLE_LEN) {
        return -1;
    }
    for (uint32_t i = 0; i + NEEDLE_LEN <= len; i++) {
        if (buf[i] == 'e' && memcmp(buf + i, needle, NEEDLE_LEN) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

void oem_scanner_run(void) {
    uint8_t buf[CHUNK + OVERLAP];

    if (hal_flash_read(0, OVERLAP, buf) != 0) {
        set_result("oem-scan:unsupported");
        return;
    }

    for (uint32_t addr = SCAN_START; addr < SCAN_END; addr += CHUNK) {
        uint32_t len = CHUNK + OVERLAP;
        if (addr + len > SCAN_END) {
            len = SCAN_END - addr;
        }
        if (hal_flash_read(addr, len, buf) != 0) {
            set_result("oem-scan:unsupported");
            return;
        }
        int32_t hit = find_needle(buf, len);
        if (hit < 0) {
            continue;
        }

        // Read a window around the hit and extract the JSON object
        uint32_t hit_addr = addr + (uint32_t)hit;
        uint32_t win_start = (hit_addr > WINDOW / 2) ? hit_addr - WINDOW / 2 : 0;
        uint8_t  win[WINDOW];
        if (hal_flash_read(win_start, WINDOW, win) != 0) {
            set_result("oem-scan:unsupported");
            return;
        }
        int32_t rel = (int32_t)(hit_addr - win_start);

        // Backtrack to the opening brace
        int32_t start = rel;
        while (start > 0 && win[start] != '{' &&
               (rel - start) < (int32_t)(WINDOW / 2)) {
            start--;
        }
        if (win[start] != '{') {
            // Key found without JSON context: likely the firmware string
            // pool of a stock image in the other OTA slot. Keep scanning.
            continue;
        }
        // Forward to the matching closing brace (flat objects expected)
        int32_t end = rel;
        while (end < (int32_t)WINDOW - 1 && win[end] != '}') {
            end++;
        }
        if (win[end] != '}') {
            continue;
        }

        uint32_t n = (uint32_t)(end - start + 1);
        if (n > sizeof(oem_dump_str.data)) {
            n = sizeof(oem_dump_str.data);
        }
        memcpy(oem_dump_str.data, &win[start], n);
        oem_dump_str.size = (uint16_t)n;
        printf("oem-scan: config found at 0x%x (%d bytes)\r\n",
               (unsigned int)(win_start + (uint32_t)start), (int)n);
        return;
    }

    set_result("oem-scan:not-found");
    printf("oem-scan: no OEM config found\r\n");
}
