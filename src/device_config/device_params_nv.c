#include "device_params_nv.h"
#include "hal/nvm.h"
#include "nvm_items.h"

uint8_t  g_multi_press_reset_count = 10;
uint16_t g_overcurrent_limit_w     = 3680; // 16 A @ 230 V (device rating)
uint8_t  g_overcurrent_tripped     = 0;

void device_params_load_from_nv(void) {
    uint8_t          value;
    hal_nvm_status_t st =
        hal_nvm_read(NV_ITEM_MULTI_PRESS_RESET_COUNT, sizeof(value),
                     (uint8_t *)&value);

    if (st == HAL_NVM_SUCCESS) {
        g_multi_press_reset_count = value;
    }

    uint16_t limit;
    st = hal_nvm_read(NV_ITEM_OVERCURRENT_LIMIT, sizeof(limit),
                      (uint8_t *)&limit);
    if (st == HAL_NVM_SUCCESS) {
        g_overcurrent_limit_w = limit;
    }
}

void device_params_set_multi_press_reset_count(uint8_t value) {
    g_multi_press_reset_count = value;
    hal_nvm_write(NV_ITEM_MULTI_PRESS_RESET_COUNT, sizeof(value),
                  (uint8_t *)&value);
}

void device_params_set_overcurrent_limit(uint16_t value) {
    g_overcurrent_limit_w = value;
    hal_nvm_write(NV_ITEM_OVERCURRENT_LIMIT, sizeof(value),
                  (uint8_t *)&value);
}
