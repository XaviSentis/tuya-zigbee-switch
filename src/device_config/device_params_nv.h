#ifndef DEVICE_CONFIG_DEVICE_PARAMS_NV_H_
#define DEVICE_CONFIG_DEVICE_PARAMS_NV_H_

#include <stdint.h>

/*
 * Global device parameter: multi-press reset count.
 *   0  => multi-press factory reset disabled
 *   N  => factory-reset after N consecutive presses
 * Default: 10, persisted in NVM.
 */

extern uint8_t g_multi_press_reset_count;

/*
 * Overcurrent (over-power) soft limit, in watts.
 *   0  => disabled
 *   W  => trip (open relay) after a few consecutive windows above W
 * Default: 3680 W (= 16 A @ 230 V, the device rating). Persisted in NVM.
 * NOTE: this is a thermal/anomaly limit with a 4-6 s delay, NOT a substitute
 * for the physical circuit breaker.
 */
extern uint16_t g_overcurrent_limit_w;

/* Transient tripped state (not persisted). 1 = tripped, relay latched off. */
extern uint8_t g_overcurrent_tripped;

void device_params_load_from_nv(void);
void device_params_set_multi_press_reset_count(uint8_t value);
void device_params_set_overcurrent_limit(uint16_t value);

#endif /* DEVICE_CONFIG_DEVICE_PARAMS_NV_H_ */
