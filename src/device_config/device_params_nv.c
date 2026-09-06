#include "device_params_nv.h"
#include "hal/nvm.h"
#include "nvm_items.h"

// Overcurrent soft limit hard ceiling: physical device rating 16 A @ 230 V.
// 0 = disabled; values above this are clamped (see device_params_set_overcurrent_limit).
#define OVERCURRENT_LIMIT_MAX_W 3680

uint8_t  g_multi_press_reset_count = 10;
uint16_t g_overcurrent_limit_w     = OVERCURRENT_LIMIT_MAX_W; // 16 A @ 230 V (device rating)
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
    // Clamp to the physical rating: 0 = disabled, max OVERCURRENT_LIMIT_MAX_W (3680 W).
    // Prevents setting a "soft" limit above what the plug can carry (it would never trip in time).
    if (value > OVERCURRENT_LIMIT_MAX_W) {
        value = OVERCURRENT_LIMIT_MAX_W;
    }
    g_overcurrent_limit_w = value;
    hal_nvm_write(NV_ITEM_OVERCURRENT_LIMIT, sizeof(value),
                  (uint8_t *)&value);
}
