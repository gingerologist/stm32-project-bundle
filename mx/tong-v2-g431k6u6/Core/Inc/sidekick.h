/*
 * sidekick.h
 *
 *  Created on: 2026年6月8日
 *      Author: matia
 */

#ifndef INC_SIDEKICK_H_
#define INC_SIDEKICK_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"

typedef struct __attribute__((packed)) {
  unsigned int current : 8;
  unsigned int width : 12;
  unsigned int period : 12;
  unsigned int interval;
} pulse_config_t;

_Static_assert(sizeof(pulse_config_t) == 8, "");

typedef union __attribute__((packed)) {
  uint64_t data;
  pulse_config_t config;
} persistent_config_t;

_Static_assert(sizeof(persistent_config_t) == 8, "");

HAL_StatusTypeDef save_config(pulse_config_t *pcfg);
bool load_config(pulse_config_t *config, bool print);
HAL_StatusTypeDef delete_config(void);

#endif /* INC_SIDEKICK_H_ */
