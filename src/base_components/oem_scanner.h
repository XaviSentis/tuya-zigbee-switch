#ifndef _OEM_SCANNER_H_
#define _OEM_SCANNER_H_

#include <stdint.h>

typedef struct {
    uint8_t length;
    char    data[250];
} oem_dump_str_t;

extern oem_dump_str_t oem_dump_str;
extern oem_dump_str_t oem_dump_str2;
extern oem_dump_str_t oem_dump_str3;
extern oem_dump_str_t oem_dump_str4;
extern oem_dump_str_t oem_dump_str5;
extern oem_dump_str_t oem_dump_str6;
extern oem_dump_str_t oem_dump_str7;
extern oem_dump_str_t oem_dump_str8;

void oem_scanner_run(void);

#endif
