#include "oem_scanner.h"

#include <stddef.h>
#include <string.h>

#include "hal/flash.h"
#include "hal/printf_selector.h"

/* ------------------------------------------------------------------ *
 * OEM scanner v6 — empaquetado para el limite de frame ZCL
 *
 * Igual que v5 en la BUSQUEDA (misma ancla ofuscada, mismo escaneo de toda
 * la flash con a0..a3, mismas pasadas de delimitadores, misma ventana de
 * fallback). Solo cambia el EMPAQUETADO:
 *  - Un read ZCL sin fragmentacion transporta ~74 B de string; los chunks
 *    de 118 B de v5 daban timeout. Nuevo CHUNK_LEN = 64.
 *  - La cabecera [h=...;len=L] va SOLA en 0xFF03 (sin datos de bloque).
 *  - El bloque se reparte en 0xFF04..0xFF0A (7 chunks x 64 = 448 B).
 *  - Total 8 atributos (0xFF03..0xFF0A / 65283..65290).
 *  - Solo lectura de flash. C99 estricto (sin _Static_assert).
 * ------------------------------------------------------------------ */

#define SCAN_END        0x100000UL
#define ANCHOR_LO       0x000F0000UL   /* ignorar cualquier hit por debajo */
#define ANCHOR_LEN      7
#define SCAN_STEP       256
#define DELIM_MAX       2048
#define WIN_BACK        96
#define WIN_FWD         480
#define NF_START        0x000FF800UL
#define NF_END          0x000FFFFFUL

#define N_CHUNKS        8
#define CHUNK_LEN       64
#define HDR_CAP         128
#define BODY_CAP        ((N_CHUNKS - 1) * CHUNK_LEN)   /* 7 * 64 = 448 */

oem_dump_str_t oem_dump_str  = { 0, { 0 } };
oem_dump_str_t oem_dump_str2 = { 0, { 0 } };
oem_dump_str_t oem_dump_str3 = { 0, { 0 } };
oem_dump_str_t oem_dump_str4 = { 0, { 0 } };
oem_dump_str_t oem_dump_str5 = { 0, { 0 } };
oem_dump_str_t oem_dump_str6 = { 0, { 0 } };
oem_dump_str_t oem_dump_str7 = { 0, { 0 } };
oem_dump_str_t oem_dump_str8 = { 0, { 0 } };

/* chunk 0 = cabecera; chunks 1..7 = bloque */
static oem_dump_str_t *const chunks[N_CHUNKS] = {
    &oem_dump_str,  &oem_dump_str2, &oem_dump_str3, &oem_dump_str4,
    &oem_dump_str5, &oem_dump_str6, &oem_dump_str7, &oem_dump_str8,
};

/* "rl1_pin" con cada byte +1 -> se decodifica a RAM en tiempo de ejecucion */
static const char anchor_obf[ANCHOR_LEN + 1] = { 's', 'm', '2', '`', 'q', 'j', 'o', 0 };

/* ------------------------- flash byte cache ----------------------- */

static uint8_t  fc_buf[SCAN_STEP];
static uint32_t fc_base;
static uint8_t  fc_valid;

/* Lee un byte de flash usando una cache de bloque de 256 B (solo lectura). */
static int8_t flash_byte(uint32_t addr, uint8_t *out) {
    uint32_t base;
    if (addr >= SCAN_END) { return -1; }
    base = addr & ~((uint32_t)(SCAN_STEP - 1));
    if (!fc_valid || base != fc_base) {
        if (hal_flash_read(base, SCAN_STEP, fc_buf) != 0) {
            fc_valid = 0;
            return -1;
        }
        fc_base  = base;
        fc_valid = 1;
    }
    *out = fc_buf[addr - base];
    return 0;
}

/* ------------------------- output builders ------------------------ */

static void clear_chunks(void) {
    int i;
    for (i = 0; i < N_CHUNKS; i++) { chunks[i]->length = 0; }
}

static void out_putc(uint8_t *out, uint16_t *n, uint16_t cap, char c) {
    if (*n < cap) { out[(*n)++] = (uint8_t)c; }
}

static void out_puts(uint8_t *out, uint16_t *n, uint16_t cap, const char *s) {
    while (*s) { out_putc(out, n, cap, *s++); }
}

/* Hex en minusculas, rellenado a un minimo de 5 digitos. */
static void out_hex5(uint8_t *out, uint16_t *n, uint16_t cap, uint32_t v) {
    char t[9];
    int  d = 0;
    if (v == 0) { t[d++] = '0'; }
    while (v > 0 && d < 8) {
        uint32_t r = v & 0xF;
        t[d++] = (char)(r < 10 ? ('0' + r) : ('a' + (r - 10)));
        v >>= 4;
    }
    while (d < 5) { t[d++] = '0'; }
    while (d > 0) { out_putc(out, n, cap, t[--d]); }
}

static void out_dec(uint8_t *out, uint16_t *n, uint16_t cap, uint32_t v) {
    char t[12];
    int  d = 0;
    if (v == 0) { t[d++] = '0'; }
    while (v > 0 && d < 11) {
        t[d++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (d > 0) { out_putc(out, n, cap, t[--d]); }
}

/* [h=N;a0=XXXXX;a1=XXXXX;a2=XXXXX;a3=XXXXX;a=XXXXX;s=XXXXX;e=XXXXX;len=L] */
static void build_header(uint8_t *out, uint16_t *n, uint16_t cap, uint16_t h,
                         uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                         uint32_t a, uint32_t s, uint32_t e, uint32_t len) {
    out_puts(out, n, cap, "[h=");   out_dec(out, n, cap, h);
    out_puts(out, n, cap, ";a0=");  out_hex5(out, n, cap, a0);
    out_puts(out, n, cap, ";a1=");  out_hex5(out, n, cap, a1);
    out_puts(out, n, cap, ";a2=");  out_hex5(out, n, cap, a2);
    out_puts(out, n, cap, ";a3=");  out_hex5(out, n, cap, a3);
    out_puts(out, n, cap, ";a=");   out_hex5(out, n, cap, a);
    out_puts(out, n, cap, ";s=");   out_hex5(out, n, cap, s);
    out_puts(out, n, cap, ";e=");   out_hex5(out, n, cap, e);
    out_puts(out, n, cap, ";len="); out_dec(out, n, cap, len);
    out_putc(out, n, cap, ']');
}

/* Copia [start, end_incl] de flash al buffer de salida; opcionalmente
 * sustituye los bytes no imprimibles por '.'. Para cuando el buffer se llena. */
static void out_region(uint8_t *out, uint16_t *n, uint16_t cap,
                       uint32_t start, uint32_t end_incl, int sanitize) {
    uint32_t a;
    if (start > end_incl) { return; }
    for (a = start; a <= end_incl && *n < cap; a++) {
        uint8_t b;
        if (flash_byte(a, &b) != 0) { break; }
        if (sanitize && (b < 0x20 || b > 0x7E)) { b = (uint8_t)'.'; }
        out[(*n)++] = b;
    }
}

/* chunk 0 = cabecera sola; chunks 1..N_CHUNKS-1 = bloque (CHUNK_LEN B cada uno). */
static void emit_chunks(const uint8_t *hdr, uint16_t hlen,
                        const uint8_t *body, uint16_t blen) {
    uint16_t pos = 0;
    uint16_t hn;
    int      i;
    clear_chunks();
    hn = hlen;
    if (hn > 250) { hn = 250; }
    memcpy(chunks[0]->data, hdr, hn);
    chunks[0]->length = (uint8_t)hn;
    for (i = 1; i < N_CHUNKS && pos < blen; i++) {
        uint16_t nn = (uint16_t)(blen - pos);
        if (nn > CHUNK_LEN) { nn = CHUNK_LEN; }
        memcpy(chunks[i]->data, body + pos, nn);
        chunks[i]->length = (uint8_t)nn;
        pos += nn;
    }
}

/* ----------------------------- run -------------------------------- */

void oem_scanner_run(void) {
    static uint8_t hdr[HDR_CAP];
    static uint8_t body[BODY_CAP];
    static uint8_t sbuf[SCAN_STEP + 8];

    char     anchor[ANCHOR_LEN + 1];
    uint32_t hits[4];
    uint8_t  nstored = 0;
    uint16_t h       = 0;
    uint32_t anchor_addr = 0;
    int      have_anchor = 0;
    uint16_t hlen = 0, blen = 0;
    uint32_t addr;
    uint32_t a0, a1, a2, a3;
    uint32_t s_addr = 0, e_addr = 0;
    int      have_s = 0, have_e = 0;
    uint32_t off;
    uint8_t  b;
    int      i;

    fc_valid = 0;

    /* Decodificar el ancla (ofuscada, cada byte +1) a RAM. */
    for (i = 0; i < ANCHOR_LEN; i++) {
        anchor[i] = (char)((unsigned char)anchor_obf[i] - 1);
    }
    anchor[ANCHOR_LEN] = 0;

    /* Pasada 1: escanear TODA la flash. Se registran los 4 primeros hits
     * (incluidos los < ANCHOR_LO, p.ej. el literal en .rodata) para a0..a3;
     * el ancla es el hit MAS ALTO que sea >= ANCHOR_LO. */
    for (addr = 0; addr < SCAN_END; addr += SCAN_STEP) {
        uint32_t len = SCAN_STEP + (ANCHOR_LEN - 1);
        uint32_t j;
        if (addr + len > SCAN_END) { len = SCAN_END - addr; }
        if (hal_flash_read(addr, len, sbuf) != 0) { break; }
        for (j = 0; j + ANCHOR_LEN <= len; j++) {
            if (sbuf[j] == (uint8_t)anchor[0] &&
                memcmp(sbuf + j, anchor, ANCHOR_LEN) == 0) {
                uint32_t hit = addr + j;
                h++;
                if (nstored < 4) { hits[nstored++] = hit; }
                if (hit >= ANCHOR_LO && hit > anchor_addr) {
                    anchor_addr = hit;
                    have_anchor = 1;
                }
            }
        }
    }

    a0 = (nstored > 0) ? hits[0] : 0;
    a1 = (nstored > 1) ? hits[1] : 0;
    a2 = (nstored > 2) ? hits[2] : 0;
    a3 = (nstored > 3) ? hits[3] : 0;

    /* Sin ancla valida (ningun hit >= ANCHOR_LO): volcar la ventana
     * 0xFF800-0xFFFFF con centinela, PERO mostrando a0..a3 reales. */
    if (!have_anchor) {
        build_header(hdr, &hlen, HDR_CAP, h, a0, a1, a2, a3, 0,
                     NF_START, NF_END, NF_END - NF_START + 1);
        out_puts(body, &blen, BODY_CAP, "oem:not-found");
        out_region(body, &blen, BODY_CAP, NF_START, NF_END, 1);
        emit_chunks(hdr, hlen, body, blen);
        printf("oem-scan v6: not-found (h=%d, sin ancla >=0xf0000)\r\n", (int)h);
        return;
    }

    /* Pasada 2: delimitadores por bloques de 256 B (via flash_byte). */
    for (off = 0; off <= DELIM_MAX; off++) {
        if (off > anchor_addr) { break; }
        if (flash_byte(anchor_addr - off, &b) != 0) { break; }
        if (b == '{') { s_addr = anchor_addr - off; have_s = 1; break; }
    }
    for (off = 0; off <= DELIM_MAX; off++) {
        uint32_t a = anchor_addr + off;
        if (a >= SCAN_END) { break; }
        if (flash_byte(a, &b) != 0) { break; }
        if (b == '}') { e_addr = a; have_e = 1; break; }
    }

    if (have_s && have_e && s_addr < e_addr) {
        uint32_t bl = e_addr - s_addr + 1;
        build_header(hdr, &hlen, HDR_CAP, h, a0, a1, a2, a3, anchor_addr,
                     s_addr, e_addr, bl);
        out_region(body, &blen, BODY_CAP, s_addr, e_addr, 0);  /* bloque en crudo */
        emit_chunks(hdr, hlen, body, blen);
        printf("oem-scan v6: block %d B at 0x%x (h=%d anchor=0x%x)\r\n",
               (int)bl, (unsigned int)s_addr, (int)h, (unsigned int)anchor_addr);
    } else {
        uint32_t ws = (anchor_addr > WIN_BACK) ? (anchor_addr - WIN_BACK) : 0;
        uint32_t we = anchor_addr + WIN_FWD;
        if (we >= SCAN_END) { we = SCAN_END - 1; }
        build_header(hdr, &hlen, HDR_CAP, h, a0, a1, a2, a3, anchor_addr,
                     ws, we, we - ws + 1);
        out_region(body, &blen, BODY_CAP, ws, we, 1);          /* ventana saneada */
        emit_chunks(hdr, hlen, body, blen);
        printf("oem-scan v6: window (no delims) anchor=0x%x h=%d\r\n",
               (unsigned int)anchor_addr, (int)h);
    }
}
