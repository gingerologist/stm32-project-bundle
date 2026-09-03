/*
 * flash.c
 *
 *  Created on: 2026年6月8日
 *      Author: matia
 */
#include <stdio.h>
#include <stdbool.h>

#include "stm32g4xx_hal.h"
#include "sidekick.h"

#define MAGIC                 0xDEADBEEF

#define FLASH_LAST_PAGE       63
#define FLASH_LAST_PAGE_ADDR0 0x0801F800
#define FLASH_LAST_PAGE_ADDR1 0x0801F808

HAL_StatusTypeDef save_config(pulse_config_t *pcfg) {
  HAL_StatusTypeDef status;
  uint32_t page_error = 0;

  if (pcfg == NULL) {
    status = HAL_ERROR;
    goto exit0;
  }

  persistent_config_t duo[2];
  duo[0].config          = *pcfg;
  duo[1].config          = *pcfg;
  duo[0].config.interval = MAGIC;
  duo[1].config.interval = MAGIC;

  // 2. unlock flase
  status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    goto exit0;
  }

  // 3. clear pending error flags
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  // 4. set up erase configuration for last page
  FLASH_EraseInitTypeDef erase_config = {.TypeErase = FLASH_TYPEERASE_PAGES,
                                         .Banks     = FLASH_BANK_1,
                                         .Page      = FLASH_LAST_PAGE,
                                         .NbPages   = 1};

  status = HAL_FLASHEx_Erase(&erase_config, &page_error);
  if (status != HAL_OK)
    goto exit1;

  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                             FLASH_LAST_PAGE_ADDR0, duo[0].data);
  if (status != HAL_OK)
    goto exit1;

  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                             FLASH_LAST_PAGE_ADDR1, duo[1].data);
exit1:
  HAL_FLASH_Lock();

exit0:
  if (status == HAL_OK) {
    printf("config saved\r\n");
  } else {
    printf("failed to save config\r\n");
  }
  return status;
}

bool load_config(pulse_config_t *config, bool print) {
  persistent_config_t per1, per2;
  per1.data = *(volatile uint64_t *)(FLASH_LAST_PAGE_ADDR0);
  per2.data = *(volatile uint64_t *)(FLASH_LAST_PAGE_ADDR1);

  if (per1.data != per2.data || per1.config.interval != MAGIC) {
    if (print)
      printf("no saved config or corrupted\r\n");
    return false;
  }

  if (config) {
    config->current  = per1.config.current;
    config->period   = per1.config.period;
    config->width    = per1.config.width;
    config->interval = 0;
  }
  if (print)
    printf("config loaded\r\n");
  return true;
}

HAL_StatusTypeDef delete_config(void) {
  HAL_StatusTypeDef status;
  uint32_t page_error;

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK)
    goto fail0;

  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  FLASH_EraseInitTypeDef erase_config = {.TypeErase = FLASH_TYPEERASE_PAGES,
                                         .Banks     = FLASH_BANK_1,
                                         .Page      = FLASH_LAST_PAGE,
                                         .NbPages   = 1};

  status = HAL_FLASHEx_Erase(&erase_config, &page_error);
  HAL_FLASH_Lock();

fail0:
  if (status == HAL_OK) {
    printf("saved config deleted\r\n");
  } else {
    printf("failed to delete config, %u\r\n", status);
  }
  return status;
}
