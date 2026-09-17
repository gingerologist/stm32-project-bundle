/*
 * sidekick.c
 *
 *  Created on: 2026年6月2日
 *      Author: matia
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "stm32g4xx_hal.h"
#include "task.h"

#include "main.h"
#include "dac53202.h"
#include "embedded_cli.h"
#include "sidekick.h"

/**
 * current 8 bit
 * period  10 bit
 * width   9 bit
 */

pulse_config_t pulse_config[2] = {
    {.current = 0, .period = 0, .width = 0, .interval = 0},
    {.current = 0, .period = 0, .width = 0, .interval = 0}};

EmbeddedCliConfig *cli_config = NULL;
EmbeddedCli *cli              = NULL;

extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim2;

extern QueueHandle_t configPendingQueueHandle;
extern QueueHandle_t configIdleQueueHandle;

int _write(int file, char *ptr, int len) {
  // Redirect stdout (file=1) and stderr (file=2) to UART2
  if (file == 1 || file == 2) {
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
  }

  errno = EBADF;
  return -1;
}

static void cli_writeChar(EmbeddedCli *embeddedCli, char c) {
  uint8_t chr = c;
  HAL_UART_Transmit(&huart1, &chr, 1, HAL_MAX_DELAY);
}

/**
 * gemini advise to write to nop as nop
 */
uint8_t wnop[3] = {0x00, 0x00, 0x00};
uint8_t rnop[3] = {0x80, 0x00, 0x00};

void read_reg(regaddr_t addr, uint8_t data[2]) {
  uint8_t out[3] = {0x80 | addr, 0, 0};
  HAL_SPI_Transmit(&hspi1, out, 3, 10);
}

void SPI1_Set_CPOL_1(void) {
  while ((SPI1->SR & SPI_SR_FTLVL) || (SPI1->SR & SPI_SR_BSY))
    ;
  SPI1->CR1 &= ~SPI_CR1_SPE;
  SPI1->CR1 |= SPI_CR1_CPOL;
  SPI1->CR1 |= SPI_CR1_SPE;
}

void SPI1_Set_CPOL_0(void) {
  while ((SPI1->SR & SPI_SR_FTLVL) || (SPI1->SR & SPI_SR_BSY))
    ;
  SPI1->CR1 &= ~SPI_CR1_SPE;
  SPI1->CR1 &= ~SPI_CR1_CPOL;
  SPI1->CR1 |= SPI_CR1_SPE;
}

HAL_StatusTypeDef dac_write(uint8_t reg, uint16_t val) {
  HAL_StatusTypeDef status;

  uint8_t mo[3] = {reg, (val >> 8), val};

  SPI1_Set_CPOL_1();
  HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_RESET);
  status = HAL_SPI_Transmit(&hspi1, mo, 3, 10);
  HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_SET);
  if (status != HAL_OK) {
    printf("fail to write %d %d %d\r\n", mo[0], mo[1], mo[2]);
  }

  return status;
}

HAL_StatusTypeDef dac_read(uint8_t reg, uint16_t *val) {
  HAL_StatusTypeDef status;

  uint8_t mo[3] = {reg | RN_READ, 0x00, 0x00};
  uint8_t mi[3] = {0};

  SPI1_Set_CPOL_1();
  HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_RESET);
  status = HAL_SPI_Transmit(&hspi1, mo, 3, 10);
  HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_SET);
  if (status != HAL_OK) {
    printf("fail to write %d %d %d\r\n", mo[0], mo[1], mo[2]);
    return status;
  }

  SPI1_Set_CPOL_0();
  HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_RESET);
  status = HAL_SPI_Receive(&hspi1, mi, 3, 10);
  HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_SET);
  if (status != HAL_OK) {
    printf("fail to read 0x%02x\r\n", reg);
  }

  if (val) {
    *val = (((uint16_t)mi[1]) << 8) + mi[2];
  }
  return status;
}

/**
 * INTERFACE-CONFIG 7.6.16 INTERFACE-CONFIG Register (address = 26h)
 * [reset = 0000h] p66, enable SDO-EN (bit 0), enable FSDO (bit 2) optional
 *
 * FSDO = 0, 1.25MHz (6.13 Timing Requirements: SPI Read and Daisy Chain
 * Operation (FSDO = 0) FSDO = 1, 2.50MHz (6.14 Timing Requirements: SPI Read
 * and Daisy Chain Operation (FSDO = 1
 */
void dac_enable_sdo() {
  uint16_t val;
  dac_write(RN_INTERFACE_CONFIG, 0x0005);
  dac_read(RN_INTERFACE_CONFIG, &val);

  printf("interface-config: %d\r\n", val);
}

void read_general_status() {
  general_status_t status;

  dac_read(RN_GENERAL_STATUS, &status.val);
  printf("general-status: %u, device-id %u, version-id %u\r\n", status.val,
         status.device_id, status.version_id);
}

const char *iout_range_strs[] = {"-25u to 25u",   "-50u to 50u",
                                 "-125u to 125u", "-250u to 250u",
                                 "invalid",       NULL};

void dump_dac_registers(void) {

  dac_x_iout_misc_config_t iout_misc_config;
  dac_x_cmp_mode_config_t cmp_mode_config;

  dac_x_data_t data;

  common_config_t common_config;

  interface_config_t interface_config;

  unsigned int range, index;

  printf("------- dump-begin --------\r\n");
#if (DUMP_DAC_X_MARGIN == 1)
  dac_x_margin_t margin;
#endif

#if (DUMP_DAC_X_VOUT_CMP_CONFIG == 1)
  dac_x_vout_cmp_config_t vout_cmp_config;
#endif

#if (DUMP_DAC_X_FUNC_CONFIG == 1)
  dac_x_func_config_t func_config;
#endif

  // dac 1
#if (DUMP_DAC_X_MARGIN == 1)
  dac_x_margin_t margin;
  dac_read(RN_DAC_1_MARGIN_HIGH, &margin.val);
  printf("dac-1-margin-high %d\r\n", margin.dac53202.dac_x_margin);

  dac_read(RN_DAC_1_MARGIN_LOW, &margin.val);
  printf("dac-1-margin-low %d\r\n", margin.dac53202.dac_x_margin);
#endif

#if (DUMP_DAC_X_VOUT_CMP_CONFIG == 1)
  dac_read(RN_DAC_1_VOUT_CMP_CONFIG, &vout_cmp_config.val);
  printf("dac-1-vout-cmp-config\r\n");
  printf("  cmp-1-en: %u\r\n", vout_cmp_config.cmp_x_en);
  printf("  cmp-1-inv-en: %u\r\n", vout_cmp_config.cmp_x_inv_en);
  printf("  cmp-1-hiz-in-dis: %u\r\n", vout_cmp_config.cmp_x_hiz_in_dis);
  printf("  cmp-1-out-en: %u", vout_cmp_config.cmp_x_out_en);
  printf("  cmp-1-od-en: %u", vout_cmp_config.cmp_x_od_en);
  printf("  vout-gain-1: %u", vout_cmp_config.vout_gain_x);
#endif

  dac_read(RN_DAC_1_IOUT_MISC_CONFIG, &iout_misc_config.val);
  printf("dac-1-iout-misc-config\r\n");
  range = iout_misc_config.iout_range_x;
  index = (range < 8 || range > 11) ? 4 : range - 8;
  printf("  iout-range-1: %u, %s\r\n", range, iout_range_strs[index]);

  dac_read(RN_DAC_1_CMP_MODE_CONFIG, &cmp_mode_config.val);
  printf("dac-1-cmp-mode-config, cmp-1-mode: %u\r\n",
         cmp_mode_config.cmp_x_mode);

#if (DUMP_DAC_X_FUNC_CONFIG == 1)
  dac_read(RN_DAC_1_FUNC_CONFIG, &func_config.val);
  printf("dac-1-func-config\r\n");
  if (func_config.linear.log_slew_en_x) {
    printf("  fall-slew-1: %u\r\n", func_config.logarithmic.fall_slew_x);
    printf("  rise-slew-1: %u\r\n", func_config.logarithmic.rise_slew_x);
    printf("  log-slew-en-1: %u\r\n", func_config.logarithmic.log_slew_en_x);
    printf("  func-config-1: %u\r\n", func_config.logarithmic.func_config_x);
    printf("  phase-sel-1: %u\r\n", func_config.logarithmic.phase_sel_x);
    printf("  brd-config-1: %u\r\n", func_config.logarithmic.brd_config_x);
    printf("  sync-config-1: %u\r\n", func_config.logarithmic.sync_config_x);
    printf("  clr-sel-1: %u\r\n", func_config.logarithmic.clr_sel_x);
  } else {
    printf("  slew-rate-1: %u\r\n", func_config.linear.slew_rate_x);
    printf("  code-step-1: %u\r\n", func_config.linear.code_step_x);
    printf("  log-slew-en-1: %u\r\n", func_config.linear.log_slew_en_x);
    printf("  func-config-1: %u\r\n", func_config.linear.func_config_x);
    printf("  phase-sel-1: %u\r\n", func_config.linear.phase_sel_x);
    printf("  brd-config-1: %u\r\n", func_config.linear.brd_config_x);
    printf("  sync-config-1: %u\r\n", func_config.linear.sync_config_x);
    printf("  clr-sel-1: %u\r\n", func_config.linear.clr_sel_x);
  }
#endif

  dac_read(RN_DAC_1_DATA, &data.val);
  printf("dac-1-data: %u", data.current.dac_x_data);

  //  dac 0
#if (DUMP_DAC_X_MARGIN == 1)
  dac_read(RN_DAC_0_MARGIN_HIGH, &margin.val);
  printf("dac-0-margin-high %d\r\n", margin.dac53202.dac_x_margin);

  dac_read(RN_DAC_0_MARGIN_LOW, &margin.val);
  printf("dac-0-margin-low %d\r\n", margin.dac53202.dac_x_margin);
#endif

#if (DUMP_DAC_X_VOUT_CMP_CONFIG == 1)
  dac_read(RN_DAC_0_VOUT_CMP_CONFIG, &vout_cmp_config.val);
  printf("dac-0-vout-cmp-config\r\n");
  printf("  cmp-0-en: %u\r\n", vout_cmp_config.cmp_x_en);
  printf("  cmp-0-inv-en: %u\r\n", vout_cmp_config.cmp_x_inv_en);
  printf("  cmp-0-hiz-in-dis: %u\r\n", vout_cmp_config.cmp_x_hiz_in_dis);
  printf("  cmp-0-out-en: %u", vout_cmp_config.cmp_x_out_en);
  printf("  cmp-0-od-en: %u", vout_cmp_config.cmp_x_od_en);
  printf("  vout-gain-0: %u", vout_cmp_config.vout_gain_x);
#endif

  dac_read(RN_DAC_0_IOUT_MISC_CONFIG, &iout_misc_config.val);
  printf("dac-0-iout-misc-config\r\n");
  range = iout_misc_config.iout_range_x;
  index = (range < 8 || range > 11) ? 4 : range - 8;
  printf("  iout-range-0: %u, %s\r\n", range, iout_range_strs[index]);

  dac_read(RN_DAC_0_CMP_MODE_CONFIG, &cmp_mode_config.val);
  printf("dac-0-cmp-mode-config, cmp-1-mode: %u\r\n",
         cmp_mode_config.cmp_x_mode);

#if (DUMP_DAC_X_FUNC_CONFIG == 1)
  dac_read(RN_DAC_0_FUNC_CONFIG, &func_config.val);
  printf("dac-0-func-config\r\n");
  if (func_config.linear.log_slew_en_x) {
    printf("  fall-slew-0: %u\r\n", func_config.logarithmic.fall_slew_x);
    printf("  rise-slew-0: %u\r\n", func_config.logarithmic.rise_slew_x);
    printf("  log-slew-en-0: %u\r\n", func_config.logarithmic.log_slew_en_x);
    printf("  func-config-0: %u\r\n", func_config.logarithmic.func_config_x);
    printf("  phase-sel-0: %u\r\n", func_config.logarithmic.phase_sel_x);
    printf("  brd-config-0: %u\r\n", func_config.logarithmic.brd_config_x);
    printf("  sync-config-0: %u\r\n", func_config.logarithmic.sync_config_x);
    printf("  clr-sel-0: %u\r\n", func_config.logarithmic.clr_sel_x);
  } else {
    printf("  slew-rate-0: %u\r\n", func_config.linear.slew_rate_x);
    printf("  code-step-0: %u\r\n", func_config.linear.code_step_x);
    printf("  log-slew-en-0: %u\r\n", func_config.linear.log_slew_en_x);
    printf("  func-config-0: %u\r\n", func_config.linear.func_config_x);
    printf("  phase-sel-0: %u\r\n", func_config.linear.phase_sel_x);
    printf("  brd-config-0: %u\r\n", func_config.linear.brd_config_x);
    printf("  sync-config-0: %u\r\n", func_config.linear.sync_config_x);
    printf("  clr-sel-0: %u\r\n", func_config.linear.clr_sel_x);
  }
#endif

  dac_read(RN_DAC_0_DATA, &data.val);
  printf("dac-0-data: %u", data.current.dac_x_data);

  dac_read(RN_COMMON_CONFIG, &common_config.val);
  printf("common-config\r\n");
  printf("  iout-pdn-1: %u\r\n", common_config.iout_pdn_1);
  printf("  vout-pdn-1: %u\r\n", common_config.vout_pdn_1);
  printf("  iout-pdn-0: %u\r\n", common_config.iout_pdn_0);
  printf("  vout-pdn-0: %u\r\n", common_config.vout_pdn_0);
  printf("  en-int-ref: %u\r\n", common_config.en_int_ref);
  printf("  ee-read-addr: %u\r\n", common_config.ee_read_addr);
  printf("  dev-lock: %u\r\n", common_config.dev_lock);
  printf("  win-latch-en: %u\r\n", common_config.win_latch_en);

#if (DUMP_COMMON_TRIGGER == 1)
  common_trigger_t common_trigger;
  dac_read(RN_COMMON_TRIGGER, &common_trigger.val);
  printf("common-trigger\r\n");
  printf("  nvm-reload: %u\r\n", common_trigger.nvm_reload);
  printf("  nvm-prog: %u\r\n", common_trigger.nvm_prog);
  printf("  read-one-trig: %u\r\n", common_trigger.read_one_trig);
  printf("  protect: %u\r\n", common_trigger.protect);
  printf("  fault-dump: %u\r\n", common_trigger.fault_dump);
  printf("  clr: %u\r\n", common_trigger.clr);
  printf("  ldac: %u\r\n", common_trigger.ldac);
  printf("  reset: %u\r\n", common_trigger.reset);
  printf("  dev-lock: %u\r\n", common_trigger.dev_unlock);
#endif

#if (DUMP_COMMON_DAC_TRIG == 1)
  common_dac_trig_t common_dac_trig;
  dac_read(RN_COMMON_DAC_TRIG, &common_dac_trig.val);
  printf("common-dac-trig\r\n");
  printf("  start-func-0: %u\r\n", common_dac_trig.start_func_0);
  printf("  trig-mar-hi-0: %u\r\n", common_dac_trig.trig_mar_hi_0);
  printf("  trig-mar-lo-0: %u\r\n", common_dac_trig.trig_mar_lo_0);
  printf("  reset-cmp-flag-0: %u\r\n", common_dac_trig.reset_cmp_flag_0);
  printf("  start-func-1: %u\r\n", common_dac_trig.start_func_1);
  printf("  trig-mar-hi-1: %u\r\n", common_dac_trig.trig_mar_hi_1);
  printf("  trig-mar-lo-1: %u\r\n", common_dac_trig.trig_mar_lo_1);
  printf("  reset-cmp-flag-1: %u\r\n", common_dac_trig.reset_cmp_flag_1);
#endif

#if (DUMP_GENERAL_STATUS == 1)
  general_status_t general_status;
  dac_read(RN_GENERAL_STATUS, &general_status.val);
  printf("general-status\r\n");
  printf("  version-id: %u\r\n", general_status.version_id);
  printf("  device-id: %u\r\n", general_status.device_id);
  printf("  dac-1-busy: %u\r\n", general_status.dac_1_busy);
  printf("  dac-0-busy: %u\r\n", general_status.dac_0_busy);
  printf("  nvm-crc-fail-user: %u\r\n", general_status.nvm_crc_fail_user);
  printf("  nvm-crc-fail-int: %u\r\n", general_status.nvm_crc_fail_int);
#endif

#if (DUMP_CMP_STATUS == 1)
  cmp_status_t cmp_status;
  dac_read(RN_CMP_STATUS, &cmp_status.val);
  printf("cmp-status\r\n");
  printf("  cmp-flag-1: %u\r\n", cmp_status.cmp_flag_1);
  printf("  cmp-flag-0: %u\r\n", cmp_status.cmp_flag_0);
  printf("  win-cmp-1: %u\r\n", cmp_status.win_cmp_1);
  printf("  win-cmp-0: %u\r\n", cmp_status.win_cmp_0);
  printf("  protect-flag: %u\r\n", cmp_status.protect_flag);
#endif

#if (DUMP_GPIO_CONFIG == 1)
  gpio_config_t gpio_config;
  dac_read(RN_GPIO_CONFIG, &gpio_config.val);
  printf("gpio-config\r\n");
  printf("  gpi-en: %u\r\n", gpio_config.gpi_en);
  printf("  gpi-config: %u\r\n", gpio_config.gpi_config);
  printf("  gpi-ch-sel: %u\r\n", gpio_config.gpi_ch_sel);
  printf("  gpo-config: %u\r\n", gpio_config.gpo_config);
  printf("  gpo-en: %u\r\n", gpio_config.gpo_en);
  printf("  gf-en: %u\r\n", gpio_config.gf_en);
#endif

#if (DUMP_DEVICE_MODE_CONFIG == 1)
  device_mode_config_t device_mode_config;
  dac_read(RN_DEVICE_MODE_CONFIG, &device_mode_config.val);
  printf("device-mode-config\r\n");
  printf("  protect-config: %u\r\n", device_mode_config.protect_config);
  printf("  dis-mode-in: %u\r\n", device_mode_config.dis_mode_in);
#endif

  dac_read(RN_INTERFACE_CONFIG, &interface_config.val);
  printf("interface-config, sdo-en: %u, fsdo-en: %u, en-pmbus: %u, timeout-en: "
         "%u\r\n",
         interface_config.sdo_en, interface_config.fsdo_en,
         interface_config.en_pmbus, interface_config.timeout_en);

#if (DUMP_SRAM == 1)
  sram_config_t sram_config;
  sram_data_t sram_data;

  dac_read(RN_SRAM_CONFIG, &sram_config.val);
  printf("sram-config\r\n");
  printf("  sram-addr: %u\r\n", sram_config.sram_addr);

  dac_read(RN_SRAM_DATA, &sram_data.val);
  printf("sram-data\r\n");
  printf("  sram-data: %u\r\n", sram_data.sram_data);
#endif

#if (DUMP_BRDCAST_DATA == 1)
  brdcast_data_t brdcast_data;
  dac_read(RN_BRDCAST_DATA, &brdcast_data.val);
  printf("brdcast-data\r\n");
  printf("  brdcast-data [dac53202] : %u\r\n",
         brdcast_data.dac53202.brdcast_data);
  printf("  brdcast-data [dac63202] : %u\r\n",
         brdcast_data.dac63202.brdcast_data);
#endif

  printf("-------  dump-end  --------\r\n");
}

extern TIM_HandleTypeDef htim2;

static pulse_config_t current_config = {0};
static bool current_config_valid     = false;
static pulse_config_t running_config = {0};
static bool running_config_valid     = false;

static void CLI_CMD_Clear(EmbeddedCli *cli, char *args, void *context) {
  // printf("\033[2J\033[H"); This does not work
  // printf("\x1B[2J\x1B[H"); This does not work either
  HAL_UART_Transmit(&huart1, (uint8_t *)"\x1B[2J\x1B[H", 7,
                    HAL_MAX_DELAY); // this works :)
}

CliCommandBinding cli_cmd_clear_binding = {"clear", "Clear screen.", false,
                                           NULL, CLI_CMD_Clear};

static void CLI_CMD_Reboot(EmbeddedCli *cli, char *args, void *context) {
  NVIC_SystemReset();
}

CliCommandBinding cli_cmd_reboot_binding = {"reboot", "Reboot", false, NULL,
                                            CLI_CMD_Reboot};

static int parse_max5_digits(const char *str) {
  if (str == NULL)
    return -1;

  int len = strlen(str);
  if (len == 0 || len > 5)
    return -1;

  if (len > 1 && str[0] == '0')
    return -1;

  int sum = 0;
  for (int i = 0; i < len; i++) {
    if (str[i] < '0' || str[i] > '9') {
      return -1;
    }
    sum = sum * 10 + (str[i] - '0');
  }

  return sum;
}

static void print_config(const char *name, bool *valid,
                         pulse_config_t *config) {
  if (!(*valid)) {
    printf("no %s config\r\n", name);
    return;
  }

  printf("%s config\r\n", name);
  printf("  current: %u steps\r\n", config->current);
  printf("    width: %u us\r\n", config->width);
  if (config->period > 0) {
    printf("   period: %u ms\r\n", config->period);
  } else {
    printf("   period: 0, predefined 6s/4s pattern for 4 (5-min) periods\r\n");
    printf("           with 3Hz/3Hz/10Hz/20Hz respectively.\r\n");
  }
}

// set <current> <pulse width> <period>
static void CLI_CMD_Set(EmbeddedCli *cli, char *args, void *context) {
  int current, width, period;

  uint16_t count = embeddedCliGetTokenCount(args);
  if (count != 3) {
    printf("count %u\r\n", count);
    return;
  }

  // current
  current = parse_max5_digits(embeddedCliGetToken(args, 1));
  if (current < 1 || current > 128) {
    printf("invalid current\r\n");
    return;
  }

  width = parse_max5_digits(embeddedCliGetToken(args, 2));
  if (width < 100 || width > 1000) {
    printf("invalid width\r\n");
    return;
  }

  period = parse_max5_digits(embeddedCliGetToken(args, 3));
  if (period == 0) {
  } else if (period < 50 || period > 500) {
    printf("invalid period\r\n");
    return;
  }

  current_config.current = current;
  current_config.width   = width;
  current_config.period  = period;
  current_config_valid   = true;
}

CliCommandBinding cli_cmd_set_binding = {
    "set",
    "set pulse parameters\r\n"
    "        set <current:1-128 steps> <pulse width: 100-1000 micro-seconds> "
    "<period: 50-500 milli-seconds, or 0 for predefined daily pattern>\r\n"
    "        example: set 64 200 100\r\n",
    true, NULL, CLI_CMD_Set};

static void CLI_CMD_Show(EmbeddedCli *cli, char *args, void *context) {
  bool saved_config_valid;
  pulse_config_t saved_config;

  print_config("current", &current_config_valid, &current_config);
  print_config("running", &running_config_valid, &running_config);

  saved_config_valid = load_config(&saved_config, false);
  print_config("saved", &saved_config_valid, &saved_config);
}

CliCommandBinding cli_cmd_show_binding = {"show", "show config and simulation",
                                          false, NULL, CLI_CMD_Show};

static void start_stimulation(void) {
  if (!current_config_valid) {
    printf("no config\r\n");
    return;
  }

  pulse_config_t *pcfg;

  xQueueReceive(configIdleQueueHandle, &pcfg, portMAX_DELAY);

  pcfg->current        = current_config.current;
  pcfg->width          = current_config.width;
  pcfg->period         = current_config.period;
  running_config       = current_config;
  running_config_valid = true;

  xQueueSend(configPendingQueueHandle, &pcfg, portMAX_DELAY);
  printf("stimulation started\r\n");
}

static void CLI_CMD_Start(EmbeddedCli *cli, char *args, void *context) {
  start_stimulation();
}

CliCommandBinding cli_cmd_start_binding = {"start", "start stimulation", false,
                                           NULL, CLI_CMD_Start};

static void CLI_CMD_Stop(EmbeddedCli *cli, char *args, void *context) {
  uint32_t *p = NULL;
  xQueueSend(configPendingQueueHandle, &p, portMAX_DELAY);
  running_config_valid = false;
}

CliCommandBinding cli_cmd_stop_binding = {"stop", "stop simulation", false,
                                          NULL, CLI_CMD_Stop};

static void CLI_CMD_Save(EmbeddedCli *cli, char *args, void *context) {
  if (!current_config_valid) {
    printf("no config\r\n");
    return;
  }
  save_config(&current_config);
}

CliCommandBinding cli_cmd_save_binding = {
    "save",
    "save config to flash and start stimulation automatically after power up",
    false, NULL, CLI_CMD_Save};

static void CLI_CMD_Delete(EmbeddedCli *cli, char *args, void *context) {
  delete_config();
}

CliCommandBinding cli_cmd_delete_binding = {"delete", "delete saved config",
                                            false, NULL, CLI_CMD_Delete};

void StartDefaultTask(void const *argument) {
  cli_config     = embeddedCliDefaultConfig();
  cli            = embeddedCliNew(cli_config);
  cli->writeChar = cli_writeChar;

  embeddedCliAddBinding(cli, cli_cmd_clear_binding);
  embeddedCliAddBinding(cli, cli_cmd_reboot_binding);
  embeddedCliAddBinding(cli, cli_cmd_set_binding);
  embeddedCliAddBinding(cli, cli_cmd_show_binding);
  embeddedCliAddBinding(cli, cli_cmd_start_binding);
  embeddedCliAddBinding(cli, cli_cmd_stop_binding);
  embeddedCliAddBinding(cli, cli_cmd_save_binding);
  embeddedCliAddBinding(cli, cli_cmd_delete_binding);

  embeddedCliReceiveChar(cli, 9);
  embeddedCliProcess(cli);

  if (load_config(&current_config, true)) {
    current_config_valid = true;
    print_config("current (autoload)", &current_config_valid, &current_config);
    printf("stimulation will start in 10 seconds\r\n");
    vTaskDelay(10000);
    start_stimulation();
  }

  /* Infinite loop */
  for (;;) {
    uint8_t c;
    HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, &c, 1, 10);
    if (status == HAL_OK) {
      embeddedCliReceiveChar(cli, c);
      embeddedCliProcess(cli);
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

/**
 * To enter current-output mode for each DAC channel, disable the respective
 * IOUT-PDN-X bits in the COMMONCONFIG register, and set the respective
 * VOUT-PDN-X bits in the same register to Hi-Z power-down mode. Select the
 * desired current-output range by writing to the IOUT-RANGE-X bit in the
 * DAC-X-IOUT-MISC-CONFIG register. To minimize leakage in current-output mode,
 * disconnect the FBx pin. For the best power-on glitch performance, program the
 * NVM with IOUT mode using the smallest output range before powering on the
 * output channel, and then immediately program the DAC code and desired output
 * range. The transfer function of the output current is shown in formula 4.
 */
static void init_current_sink(void) {
  common_config_t common_config = {
      .iout_pdn_1   = 1, // power down iout1
      .vout_pdn_1   = 3, // power down vout1 with Hi-Z to AGND
      .iout_pdn_0   = 1, // power down iout0
      .vout_pdn_0   = 3, // power down vout0 with Hi-Z to AGND
      .en_int_ref   = 1, // enable internal reference.
      .ee_read_addr = 0, // irrelevant
      .dev_lock     = 0, // irrelevant
      .win_latch_en = 0, // irrelevant
  };
  dac_write(RN_COMMON_CONFIG, common_config.val);

  dac_x_iout_misc_config_t iout_misc_config = {
      .iout_range_x = 10,
  };
  dac_write(RN_DAC_0_IOUT_MISC_CONFIG, iout_misc_config.val);
  dac_write(RN_DAC_1_IOUT_MISC_CONFIG, iout_misc_config.val);
}

/**
 * original equation
 *
 * Iout = DAC_DATA * 250u / 256 - 125u
 * when DAC_DATA = 0,   Iout = -125u
 * when DAC_DATA = 64,  Iout = -62.5u
 * when DAC_DATA = 128, Iout = 0u
 *
 * step is defined as 128 - DAC_DATA
 */
static void start_current_sink(uint8_t step) {
  if (step > 128)
    step = 128;

  common_config_t common_config = {
      .iout_pdn_1   = 0, // power on iout1
      .vout_pdn_1   = 3, // power down vout1 with Hi-Z to AGND
      .iout_pdn_0   = 0, // power on iout0
      .vout_pdn_0   = 3, // power down vout0 with Hi-Z to AGND
      .en_int_ref   = 1, // enable internal reference.
      .ee_read_addr = 0, // irrelevant
      .dev_lock     = 0, // irrelevant
      .win_latch_en = 0, // irrelevant
  };
  dac_write(RN_COMMON_CONFIG, common_config.val);

  dac_x_data_t dac_data = {.current = {.dac_x_data = 128 - step}};
  dac_write(RN_DAC_0_DATA, dac_data.val);
  dac_write(RN_DAC_1_DATA, dac_data.val);
}

static void stop_current_sink() {
  common_config_t common_config = {
      .iout_pdn_1   = 1, // power down iout1
      .vout_pdn_1   = 3, // power down vout1 with Hi-Z to AGND
      .iout_pdn_0   = 1, // power down iout0
      .vout_pdn_0   = 3, // power down vout0 with Hi-Z to AGND
      .en_int_ref   = 1, // enable internal reference.
      .ee_read_addr = 0, // irrelevant
      .dev_lock     = 0, // irrelevant
      .win_latch_en = 0, // irrelevant
  };
  dac_write(RN_COMMON_CONFIG, common_config.val);
}

// active low
static void start_opamp() {
  HAL_GPIO_WritePin(CH0_SD_GPIO_Port, CH0_SD_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CH1_SD_GPIO_Port, CH1_SD_Pin, GPIO_PIN_RESET);
}

static void stop_opamp() {
  HAL_GPIO_WritePin(CH0_SD_GPIO_Port, CH0_SD_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CH1_SD_GPIO_Port, CH1_SD_Pin, GPIO_PIN_SET);
}

// active high
static void switch_sourcing(void) {
  HAL_GPIO_WritePin(MUX_SEL1_GPIO_Port, MUX_SEL1_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(MUX_SEL2_GPIO_Port, MUX_SEL2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(MUX_SEL3_GPIO_Port, MUX_SEL3_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(MUX_SEL4_GPIO_Port, MUX_SEL4_Pin, GPIO_PIN_SET);
}

static void switch_sinking(void) {
  HAL_GPIO_WritePin(MUX_SEL1_GPIO_Port, MUX_SEL1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MUX_SEL2_GPIO_Port, MUX_SEL2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MUX_SEL3_GPIO_Port, MUX_SEL3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MUX_SEL4_GPIO_Port, MUX_SEL4_Pin, GPIO_PIN_RESET);
}

// Active low logic enable; has internal pull-down resistor. The SELx logic
// inputs determine switch connections when this pin is low (see 节 7.5)
static void switch_on(void) {
  HAL_GPIO_WritePin(MUX_EN_GPIO_Port, MUX_EN_Pin, GPIO_PIN_RESET);
}

static void switch_off(void) {
  HAL_GPIO_WritePin(MUX_EN_GPIO_Port, MUX_EN_Pin, GPIO_PIN_SET);
}

static void bleed_on(void) {
  HAL_GPIO_WritePin(BLEED_EN_GPIO_Port, BLEED_EN_Pin, GPIO_PIN_RESET);
}

static void bleed_off(void) {
  HAL_GPIO_WritePin(BLEED_EN_GPIO_Port, BLEED_EN_Pin, GPIO_PIN_SET);
}

static void start_timer() {
  __HAL_TIM_SET_AUTORELOAD(&htim2, 0xffffffff);
  HAL_TIM_Base_Start(&htim2);
}

static void stop_timer() { HAL_TIM_Base_Stop(&htim2); }

static inline uint32_t tim_counter() { return __HAL_TIM_GET_COUNTER(&htim2); }

static void tim_delay_us(unsigned int us) {
  uint32_t mark = tim_counter();
  while (htim2.State == HAL_TIM_STATE_BUSY && tim_counter() - mark < us)
    ;
}

static void enable_hv_power(void) {
  HAL_GPIO_WritePin(EN_POS_HV_GPIO_Port, EN_POS_HV_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(EN_NEG_HV_GPIO_Port, EN_NEG_HV_Pin, GPIO_PIN_SET);
}

static void disable_hv_power(void) {
  HAL_GPIO_WritePin(EN_POS_HV_GPIO_Port, EN_POS_HV_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(EN_NEG_HV_GPIO_Port, EN_NEG_HV_Pin, GPIO_PIN_RESET);
}

// __HAL_TIM_SET_AUTORELOAD(&htim2, 1999);
// HAL_TIM_Base_Start_IT(&htim2);

const uint8_t steps[4] = {0, 32, 64, 96};

static void do_pulse(unsigned int width, bool print) {
  // static int roll = 0;

  //  dac_x_data_t dac_data = {.current = {.dac_x_data = steps[roll]}};
  //  dac_write(RN_DAC_0_DATA, dac_data.val);
  //  dac_write(RN_DAC_1_DATA, dac_data.val);
  //
  //  roll = (roll + 1) & 0x03;

  uint32_t timestamps[6];

  timestamps[0] = tim_counter();
  start_opamp();
  tim_delay_us(250);
  timestamps[1] = tim_counter();

  switch_sourcing();
  switch_on();
  tim_delay_us(width);
  timestamps[2] = tim_counter();

  switch_off();
  switch_sinking();
  tim_delay_us(10);
  timestamps[3] = tim_counter();

  switch_on();
  tim_delay_us(width);
  timestamps[4] = tim_counter();

  switch_off();
  stop_opamp();

  tim_delay_us(1);

  bleed_on();
  tim_delay_us(10);
  bleed_off();

  timestamps[5] = tim_counter();

  if (print) {
    printf("pulse timing: \r\n");
    for (int i = 1; i < 6; i++) {
      printf("  %lu\r\n", timestamps[i] - timestamps[i - 1]);
    }
  }
}

typedef enum {
  SWTST_ALL_OFF,
  SWTST_SOURCING,
  SWTST_SINKING
} switch_test_case_t;

void switch_test(switch_test_case_t test) {
  switch_off();

  switch (test) {
  case SWTST_ALL_OFF:
    break;
  case SWTST_SOURCING:
    switch_sourcing();
    switch_on();
    break;
  case SWTST_SINKING:
    switch_sinking();
    switch_on();
    break;
  }
}

#if 0
void StartPulseTask(void const *argument) {
  for (int i = 0; i < 2; i++) {
    pulse_config_t *p = &pulse_config[i];
    xQueueSend(configIdleQueueHandle, &p, portMAX_DELAY);
  }

  TickType_t ticks_to_wait = portMAX_DELAY;
  pulse_config_t config    = {.width = 500};

  enable_hv_power();

  dac_enable_sdo();
  dump_dac_registers();

  printf("\r\n\r\n"
         "start pulse task\r\n");

// #if 0
//  switch_test(SWTST_ALL_OFF);
//  vTaskDelay(portMAX_DELAY);
// #endif

  init_current_sink();

  for (;;) {
    pulse_config_t *pcfg;
    if (pdPASS ==
        xQueueReceive(configPendingQueueHandle, &pcfg, ticks_to_wait)) {

      if (pcfg == NULL) {
        // if NULL, stop
        stop_current_sink();
        stop_timer();
        ticks_to_wait = portMAX_DELAY;
      } else {
        config        = *pcfg;
        ticks_to_wait = config.period;
        start_timer();
        start_current_sink(config.current);
        xQueueSend(configIdleQueueHandle, &pcfg, portMAX_DELAY);
      }

      continue;
    } else {
      do_pulse(config.width, false);
    }
  }
}
#endif

void run_endless_mode(pulse_config_t *config) {
  // --- Mode A: Endless Running Mode ---
  do_pulse(config->width, false);

  // Use vTaskDelay for period spacing, but break early if config changes
  TickType_t start_tick  = xTaskGetTickCount();
  TickType_t delay_ticks = pdMS_TO_TICKS(config->period);
  while ((xTaskGetTickCount() - start_tick) < delay_ticks) {
    if (uxQueueMessagesWaiting(configPendingQueueHandle) > 0) {
      break; // Exit delay early to process new config/stop command
             // immediately
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/**
 * @brief Runs a 10-second pattern: 6 seconds active pulsing, 4 seconds idle.
 *
 * @param config Pointer to configuration struct containing pulse width
 * @param period Stimulation period in milliseconds for active pulses
 * @return true if interrupted by a new queue message, false if 10s elapses
 * fully
 */
bool run_10s(pulse_config_t *config, int period) {
  TickType_t start_tick   = xTaskGetTickCount();
  TickType_t active_ticks = pdMS_TO_TICKS(6000);  // 6 seconds active
  TickType_t total_ticks  = pdMS_TO_TICKS(10000); // 10 seconds total

  // --- Phase 1: 6-second Active Phase ---
  while ((xTaskGetTickCount() - start_tick) < active_ticks) {
    if (uxQueueMessagesWaiting(configPendingQueueHandle) > 0) {
      return true; // Interrupted
    }

    do_pulse(config->width, false);

    // Delay for 'period' ms while checking for queue interruption
    TickType_t pulse_start  = xTaskGetTickCount();
    TickType_t period_ticks = pdMS_TO_TICKS(period);

    while ((xTaskGetTickCount() - pulse_start) < period_ticks) {
      if (uxQueueMessagesWaiting(configPendingQueueHandle) > 0) {
        return true; // Interrupted
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  // --- Phase 2: 4-second Idle Phase ---
  while ((xTaskGetTickCount() - start_tick) < total_ticks) {
    if (uxQueueMessagesWaiting(configPendingQueueHandle) > 0) {
      return true; // Interrupted
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return false; // Completed full 10s without interruption
}

void StartPulseTask(void const *argument) {
  for (int i = 0; i < 2; i++) {
    pulse_config_t *p = &pulse_config[i];
    xQueueSend(configIdleQueueHandle, &p, portMAX_DELAY);
  }

  pulse_config_t config = {.width = 500};
  bool is_running       = false;

  enable_hv_power();
  dac_enable_sdo();
  dump_dac_registers();

  printf("\r\n\r\nstart pulse task\r\n");
  init_current_sink();

  for (;;) {
    // 1. Non-blocking check for new configuration or stop command
    pulse_config_t *pcfg;
    if (xQueueReceive(configPendingQueueHandle, &pcfg, 0) == pdPASS) {
      if (pcfg == NULL) {
        // Stop command received
        stop_current_sink();
        stop_timer();
        is_running = false;
        continue;
      } else {
        // New configuration loaded
        config = *pcfg;
        start_timer();
        start_current_sink(config.current);
        is_running = true;
        xQueueSend(configIdleQueueHandle, &pcfg, portMAX_DELAY);
      }
    }

    if (!is_running) {
      // Idle state: block briefly until a command arrives to save CPU cycles
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // 2. Execution Mode Handler
    if (config.period > 0) {
      run_endless_mode(&config);
    } else {
      // --- Mode B: Predefined / Fixed Pattern Mode ---
      bool interrupted    = false;
      int phase_period[4] = {333, 333, 100, 50}; // 3Hz, 3Hz, 10Hz, 20Hz

      for (int phase = 0; phase < 4; phase++) {
        printf("daily pattern phase %i: %ims period for 5 minutes\r\n", phase,
               phase_period[phase]);
        for (int times = 0; times < 5 * 6; times++) { // Fixed typo here
          interrupted = run_10s(&config, phase_period[phase]);
          if (interrupted)
            break;
        }
        if (interrupted)
          break;
      }

      // Only shut down if the sequence completed naturally
      if (!interrupted) {
        stop_current_sink();
        stop_timer();
        is_running = false;
        printf("daily pattern finished\r\n");
      }
    }
  }
}
