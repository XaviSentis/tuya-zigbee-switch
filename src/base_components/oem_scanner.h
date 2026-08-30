#ifndef _OEM_SCANNER_H_
#define _OEM_SCANNER_H_

#include <stdint.h>

/* ZCL char string layout: uint8 length + data (must match ZCL_DATA_TYPE_CHAR_STR) */
typedef struct {
    uint8_t length;
    char    data[250];
} oem_dump_str_t;

extern oem_dump_str_t oem_dump_str;

/** Scan flash for the Tuya factory OEM config JSON (runs once at boot) */
void oem_scanner_run(void);

#endif
