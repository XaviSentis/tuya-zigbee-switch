#include "oem_scanner.h"

#include <stddef.h>
#include <string.h>

#include "hal/flash.h"
#include "hal/printf_selector.h"

#define SCAN_START      0x00000UL
#define SCAN_END        0x100000UL
#define CHUNK           240
#define OVERLAP         16
#define BLOCK_MAX       (sizeof(((oem_dump_str_t *)0)->data))
#define BLOB_READ       400

oem_dump_str_t oem_dump_str = { 0, { 0 } };

static const char anchor_key[] = "rl1_pin";
#define ANCHOR_LEN (sizeof(anchor_key) - 1)

static const char *const wanted[] = {
    "ele_fun_en", "ele_pin", "vi_pin", "sel_pin", "sel_pin_lv",
    "resistor",   "vol_def", "chip",   "rl1_pin",
};
#define WANTED_N (sizeof(wanted) / sizeof(wanted[0]))

static void set_result(const char *s) {
    uint16_t n = (uint16_t)strlen(s);
    if (n > BLOCK_MAX) { n = BLOCK_MAX; }
    memcpy(oem_dump_str.data, s, n);
    oem_dump_str.length = (uint8_t)n;
}

static int32_t find_in(const uint8_t *buf, uint32_t len,
                       const char *pat, uint32_t patlen) {
    if (len < patlen) { return -1; }
    for (uint32_t i = 0; i + patlen <= len; i++) {
        if (buf[i] == pat[0] && memcmp(buf + i, pat, patlen) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static uint16_t extract_wanted(const uint8_t *blob, uint32_t blen) {
    uint16_t out = 0;
    for (uint32_t k = 0; k < WANTED_N; k++) {
        uint32_t klen = strlen(wanted[k]);
        uint32_t search_from = 0;
        int32_t  pos = -1;
        while (search_from < blen) {
            int32_t rel = find_in(blob + search_from, blen - search_from,
                                  wanted[k], klen);
            if (rel < 0) { break; }
            uint32_t abspos = search_from + (uint32_t)rel;
            uint32_t after  = abspos + klen;
            int prev_ok = (abspos == 0) || blob[abspos - 1] == '{' ||
                          blob[abspos - 1] == ',';
            int next_ok = (after < blen) && blob[after] == ':';
            if (prev_ok && next_ok) { pos = (int32_t)abspos; break; }
            search_from = abspos + 1;
        }
        if (pos < 0) { continue; }
        uint32_t after = (uint32_t)pos + klen;
        for (uint32_t c = 0; c < klen && out < BLOCK_MAX - 1; c++) {
            oem_dump_str.data[out++] = (char)wanted[k][c];
        }
        if (out < BLOCK_MAX - 1) { oem_dump_str.data[out++] = ':'; }
        uint32_t v = after + 1;
        while (v < blen && out < BLOCK_MAX - 1 &&
               ((blob[v] >= '0' && blob[v] <= '9') || blob[v] == '.')) {
            oem_dump_str.data[out++] = (char)blob[v++];
        }
        if (out < BLOCK_MAX - 1) { oem_dump_str.data[out++] = ','; }
    }
    if (out == 0) { return 0; }
    if (out > 120) {
        uint16_t cut = 120;
        while (cut > 0 && oem_dump_str.data[cut - 1] != ',') { cut--; }
        if (cut == 0) { cut = 120; }
        out = cut;
    }
    oem_dump_str.length = (uint8_t)out;
    return out;
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
        int32_t hit = find_in(buf, len, anchor_key, ANCHOR_LEN);
        if (hit < 0) { continue; }
        uint32_t hit_addr = addr + (uint32_t)hit;
        uint32_t win_start = (hit_addr > BLOB_READ / 2) ? hit_addr - BLOB_READ / 2 : 0;
        uint8_t blob[BLOB_READ];
        if (hal_flash_read(win_start, BLOB_READ, blob) != 0) { continue; }
        if (extract_wanted(blob, BLOB_READ) > 0) {
            printf("oem-scan: extracted keys near 0x%x\r\n", (unsigned int)hit_addr);
            return;
        }
        set_result("oem:present,no-wanted-keys");
        return;
    }
    set_result("oem-scan:not-found");
}
