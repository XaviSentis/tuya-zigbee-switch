#ifndef _OEM_SCANNER_H_
#define _OEM_SCANNER_H_

#include <stdint.h>

/* ZCL long char string layout: uint16 length + data */
typedef struct {
    uint16_t size;
    char     data[250];
} oem_dump_str_t;

extern oem_dump_str_t oem_dump_str;

/** Scan flash for the Tuya factory OEM config JSON (runs once at boot) */
void oem_scanner_run(void);

#endif
