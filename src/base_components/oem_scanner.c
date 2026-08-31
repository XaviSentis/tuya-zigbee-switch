#include "oem_scanner.h"

#include <stddef.h>
#include <string.h>

#include "hal/flash.h"
#include "hal/printf_selector.h"

#define SCAN_START      0x00000UL
#define SCAN_END        0x100000UL
#define CHUNK           240
#define OVERLAP         16
#define CHUNK_BYTES     118
#define BLOB_MAX        640
#define N_CHUNKS        5

oem_dump_str_t oem_dump_str  = { 0, { 0 } };
oem_dump_str_t oem_dump_str2 = { 0, { 0 } };
oem_dump_str_t oem_dump_str3 = { 0, { 0 } };
oem_dump_str_t oem_dump_str4 = { 0, { 0 } };
oem_dump_str_t oem_dump_str5 = { 0, { 0 } };

static oem_dump_str_t *const chunks[N_CHUNKS] = {
    &oem_dump_str, &oem_dump_str2, &oem_dump_str3, &oem_dump_str4, &oem_dump_str5,
};

static const char anchor_key[] = "rl1_pin";
#define ANCHOR_LEN (sizeof(anchor_key) - 1)

static void clear_chunks(void) {
    for (int i = 0; i < N_CHUNKS; i++) { chunks[i]->length = 0; }
}

static void set_sentinel(const char *s) {
    clear_chunks();
    uint16_t n = (uint16_t)strlen(s);
    if (n > CHUNK_BYTES) { n = CHUNK_BYTES; }
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

static uint8_t write_total_prefix(int32_t total) {
    char tmp[8];
    uint8_t t = 0;
    tmp[t++] = '[';
    char digs[6]; int d = 0; int v = total;
    if (v == 0) { digs[d++] = '0'; }
    while (v > 0 && d < 5) { digs[d++] = (char)('0' + v % 10); v /= 10; }
    while (d > 0) { tmp[t++] = digs[--d]; }
    tmp[t++] = ']';
    memcpy(oem_dump_str.data, tmp, t);
    return t;
}

void oem_scanner_run(void) {
    static uint8_t buf[CHUNK + OVERLAP];

    if (hal_flash_read(0, OVERLAP, buf) != 0) {
        set_sentinel("oem-scan:unsupported");
        return;
    }

    for (uint32_t addr = SCAN_START; addr < SCAN_END; addr += CHUNK) {
        uint32_t len = CHUNK + OVERLAP;
        if (addr + len > SCAN_END) { len = SCAN_END - addr; }
        if (hal_flash_read(addr, len, buf) != 0) { break; }

        int32_t hit = find_in(buf, len, anchor_key, ANCHOR_LEN);
        if (hit < 0) { continue; }

        uint32_t hit_addr = addr + (uint32_t)hit;
        uint32_t back = 500;
        uint32_t win_start = (hit_addr > back) ? hit_addr - back : 0;
        static uint8_t  blob[BLOB_MAX];
        if (hal_flash_read(win_start, BLOB_MAX, blob) != 0) { continue; }

        int32_t rel = (int32_t)(hit_addr - win_start);
        int32_t s = rel;
        while (s > 0 && blob[s] != '{') { s--; }
        int32_t e = rel;
        while (e < BLOB_MAX - 1 && blob[e] != '}') { e++; }
        if (blob[s] != '{' || blob[e] != '}') {
            set_sentinel("oem:bounds-error");
            return;
        }

        int32_t total = e - s + 1;
        const uint8_t *bp = &blob[s];

        clear_chunks();

        uint8_t pfx = write_total_prefix(total);
        int32_t c0_room = CHUNK_BYTES - pfx;
        int32_t c0_n = (total < c0_room) ? total : c0_room;
        memcpy(oem_dump_str.data + pfx, bp, (size_t)c0_n);
        oem_dump_str.length = (uint8_t)(pfx + c0_n);

        int32_t off = c0_n;
        for (int i = 1; i < N_CHUNKS && off < total; i++) {
            int32_t remain = total - off;
            int32_t n = (remain > CHUNK_BYTES) ? CHUNK_BYTES : remain;
            memcpy(chunks[i]->data, bp + off, (size_t)n);
            chunks[i]->length = (uint8_t)n;
            off += n;
        }

        printf("oem-scan: raw block %d bytes at 0x%x\r\n",
               (int)total, (unsigned int)(win_start + (uint32_t)s));
        return;
    }

    set_sentinel("oem-scan:not-found");
}
