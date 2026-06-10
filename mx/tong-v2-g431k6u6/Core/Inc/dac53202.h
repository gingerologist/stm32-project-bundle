/*
 * dac53202.h
 *
 *  Created on: 2026年6月5日
 *      Author: matia
 */

#ifndef INC_DAC53202_H_
#define INC_DAC53202_H_

#include <stdint.h>
#include <assert.h>

#define NOP                    (0x00)
#define DAC_1_MARGIN_HIGH      (0x01)
#define DAC_1_MARGIN_LOW       (0x02)
#define DAC_1_VOUT_CMP_CONIFG  (0x03)
#define DAC_1_IOUT_MISC_CONIFG (0x04)
#define DAC_1_CMP_MODE_CONFIG  (0x05)
#define DAC_1_FUNC_CONFIG      (0x06)

#define DAC_0_MARGIN_HIGH      (0x13)
#define DAC_0_MARGIN_LOW       (0x14)
#define DAC_0_VOUT_CMP_CONIFG  (0x15)
#define DAC_0_IOUT_MISC_CONIFG (0x16)
#define DAC_0_CMP_MODE_CONFIG  (0x17)
#define DAC_0_FUNC_CONFIG      (0x18)

#define DAC_1_DATA             (0x19)
#define DAC_0_DATA             (0x1C)

#define COMMON_CONFIG          (0x1F)
#define COMMON_TRIGGER         (0x20)
#define COMMON_DAC_TRIG        (0x21)
#define GENERAL_STATUS         (0x22)
#define CMP_STATUS             (0x23)
#define GPIO_CONFIG            (0x24)
#define DEVICE_MODE_CONFIG     (0x25)
#define INTERFACE_CONFIG       (0x26)
#define SRAM_CONFIG            (0x2B)
#define SRAM_DATA              (0x2C)
#define BRDCAST_DATA           (0x50)

typedef enum {
  RN_NOP                    = 0x00,
  RN_DAC_1_MARGIN_HIGH      = 0x01,
  RN_DAC_1_MARGIN_LOW       = 0x02,
  RN_DAC_1_VOUT_CMP_CONFIG  = 0x03,
  RN_DAC_1_IOUT_MISC_CONFIG = 0x04,
  RN_DAC_1_CMP_MODE_CONFIG  = 0x05,
  RN_DAC_1_FUNC_CONFIG      = 0x06,

  RN_DAC_0_MARGIN_HIGH      = 0x13,
  RN_DAC_0_MARGIN_LOW       = 0x14,
  RN_DAC_0_VOUT_CMP_CONFIG  = 0x15,
  RN_DAC_0_IOUT_MISC_CONFIG = 0x16,
  RN_DAC_0_CMP_MODE_CONFIG  = 0x17,
  RN_DAC_0_FUNC_CONFIG      = 0x18,

  RN_DAC_1_DATA             = 0x19,
  RN_DAC_0_DATA             = 0x1C,

  RN_COMMON_CONFIG          = 0x1F,
  RN_COMMON_TRIGGER         = 0x20,
  RN_COMMON_DAC_TRIG        = 0x21,
  RN_GENERAL_STATUS         = 0x22,
  RN_CMP_STATUS             = 0x23,
  RN_GPIO_CONFIG            = 0x24,
  RN_DEVICE_MODE_CONFIG     = 0x25,
  RN_INTERFACE_CONFIG       = 0x26,
  RN_SRAM_CONFIG            = 0x2B,
  RN_SRAM_DATA              = 0x2C,
  RN_BRDCAST_DATA           = 0x50,

  RN_READ                   = 0x80,
} regaddr_t;

// DAC-X-MARGIN-HIGH 13h, 01h
// DAC-X-MARGIN-LOW  14h, 02h
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int r1 : 4;
    unsigned int dac_x_margin : 12;

  } dac63202;
  struct __attribute__((packed)) {
    unsigned int r1 : 6;
    unsigned int dac_x_margin : 10;
  } dac53202;
} dac_x_margin_t;

_Static_assert(sizeof(dac_x_margin_t) == 2, "");

// DAC-X-VOUT-CMP-CONFIG 15h, 03h
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int cmp_x_en : 1;
    unsigned int cmp_x_inv_en : 1;
    unsigned int cmp_x_hiz_in_dis : 1;
    unsigned int cmp_x_out_en : 1;
    unsigned int cmp_x_od_en : 1;
    unsigned int r1 : 5;
    unsigned int vout_gain_x : 3;
    unsigned int r2 : 3;
  };
} dac_x_vout_cmp_config_t;

_Static_assert(sizeof(dac_x_vout_cmp_config_t) == 2, "");

// DAC-X-IOUT-MISC-CONFIG Register (address = 16h, 04h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int r1 : 9;
    unsigned int iout_range_x : 4;
    unsigned int r2 : 3;
  };
} dac_x_iout_misc_config_t;

_Static_assert(sizeof(dac_x_iout_misc_config_t) == 2, "");

// DAC-X-CMP-MODE-CONFIG Register (address = 17h, 05h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int r1 : 10;
    unsigned int cmp_x_mode : 2;
    unsigned int r2 : 4;
  };
} dac_x_cmp_mode_config_t;

_Static_assert(sizeof(dac_x_cmp_mode_config_t) == 2, "");

// DAC-X-FUNC-CONFIG Register (address = 18h, 06h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int slew_rate_x : 4;
    unsigned int code_step_x : 3;
    unsigned int log_slew_en_x : 1; // 0 for linear
    unsigned int func_config_x : 3;
    unsigned int phase_sel_x : 2;
    unsigned int brd_config_x : 1;
    unsigned int sync_config_x : 1;
    unsigned int clr_sel_x : 1;
  } linear;
  struct __attribute__((packed)) {
    unsigned int r1 : 1;
    unsigned int fall_slew_x : 3;
    unsigned int rise_slew_x : 3;
    unsigned int log_slew_en_x : 1; // 1 for logarithmic
    unsigned int func_config_x : 3;
    unsigned int phase_sel_x : 2;
    unsigned int brd_config_x : 1;
    unsigned int sync_config_x : 1;
    unsigned int clr_sel_x : 1;
  } logarithmic;
} dac_x_func_config_t;

_Static_assert(sizeof(dac_x_func_config_t) == 2, "");

// DAC-X-DATA Register (address = 1Ch, 19h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int r1 : 4;
    unsigned int dac_x_data : 12;
  } dac63202;
  struct __attribute__((packed)) {
    unsigned int r1 : 6;
    unsigned int dac_x_data : 10;
  } dac53202;
} dac_x_data_t;

_Static_assert(sizeof(dac_x_data_t) == 2, "");

// COMMON-CONFIG Register (address = 1Fh)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int iout_pdn_1 : 1;
    unsigned int vout_pdn_1 : 2;
    unsigned int r1 : 6;
    unsigned int iout_pdn_0 : 1;
    unsigned int vout_pdn_0 : 2;
    unsigned int en_int_ref : 1;
    unsigned int ee_read_addr : 1;
    unsigned int dev_lock : 1;
    unsigned int win_latch_en : 1;
  };
} common_config_t;

_Static_assert(sizeof(common_config_t) == 2, "");

// COMMON-TRIGGER Register (address = 20h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int nvm_reload : 1;
    unsigned int nvm_prog : 1;
    unsigned int read_one_trig : 1;
    unsigned int protect : 1;
    unsigned int fault_dump : 1;
    unsigned int r1 : 1;
    unsigned int clr : 1;
    unsigned int ldac : 1;
    unsigned int reset : 4;
    unsigned int dev_unlock : 4;
  };
} common_trigger_t;

_Static_assert(sizeof(common_trigger_t) == 2, "");

// COMMON-DAC-TRIG Register (address = 21h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int start_func_0 : 1;
    unsigned int trig_mar_hi_0 : 1;
    unsigned int trig_mar_lo_0 : 1;
    unsigned int reset_cmp_flag_0 : 1;
    unsigned int r1 : 8;
    unsigned int start_func_1 : 1;
    unsigned int trig_mar_hi_1 : 1;
    unsigned int trig_mar_lo_1 : 1;
    unsigned int reset_cmp_flag_1 : 1;
  };
} common_dac_trig_t;

_Static_assert(sizeof(common_dac_trig_t) == 2, "");

// GENERAL-STATUS Register (address = 22h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int version_id : 2;
    unsigned int device_id : 6;
    unsigned int r3 : 1;
    unsigned int dac_1_busy : 1;
    unsigned int r2 : 2;
    unsigned int dac_0_busy : 1;
    unsigned int r1 : 1;
    unsigned int nvm_crc_fail_user : 1;
    unsigned int nvm_crc_fail_int : 1;
  };
} general_status_t;

_Static_assert(sizeof(general_status_t) == 2, "");

// CMP-STATUS Register (address = 23h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int cmp_flag_1 : 1;
    unsigned int r1 : 2;
    unsigned int cmp_flag_0 : 1;
    unsigned int win_cmp_1 : 1;
    unsigned int r2 : 2;
    unsigned int win_cmp_0 : 1;
    unsigned int protect_flag : 1;
    unsigned int r3 : 7;
  };
} cmp_status_t;

_Static_assert(sizeof(cmp_status_t) == 2, "");

// GPIO-CONFIG Register (address = 24h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int gpi_en : 1;
    unsigned int gpi_config : 4;
    unsigned int gpi_ch_sel : 4;
    unsigned int gpo_config : 4;
    unsigned int gpo_en : 1;
    unsigned int r1 : 1;
    unsigned int gf_en : 1;
  };
} gpio_config_t;

_Static_assert(sizeof(gpio_config_t) == 2, "");

// DEVICE-MODE-CONFIG Register (address = 25h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int r1 : 5;
    unsigned int z1 : 3;
    unsigned int protect_config : 2;
    unsigned int z2 : 3;
    unsigned int dis_mode_in : 1;
    unsigned int z3 : 2;
  };
} device_mode_config_t;

_Static_assert(sizeof(device_mode_config_t) == 2, "");

// INTERFACE-CONFIG Register (address = 26h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int sdo_en : 1;
    unsigned int r1 : 1;
    unsigned int fsdo_en : 1;
    unsigned int r2 : 5;
    unsigned int en_pmbus : 1;
    unsigned int r3 : 3;
    unsigned int timeout_en : 1;
    unsigned int r4 : 3;
  };
} interface_config_t;

_Static_assert(sizeof(interface_config_t) == 2, "");

// SRAM-CONFIG Register (address = 2Bh)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int sram_addr : 8;
    unsigned int r1 : 8;
  };
} sram_config_t;

_Static_assert(sizeof(sram_config_t) == 2, "");

// SRAM-DATA Register (address = 2Ch)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int sram_data : 16;
  };
} sram_data_t;

_Static_assert(sizeof(sram_data_t) == 2, "");

// BRDCAST-DATA Register (address = 50h)
typedef union {
  uint16_t val;
  struct __attribute__((packed)) {
    unsigned int r1 : 4;
    unsigned int brdcast_data : 12;
  } dac63202;
  struct __attribute__((packed)) {
    unsigned int r1 : 6;
    unsigned int brdcast_data : 10;
  } dac53202;
} brdcast_data_t;

_Static_assert(sizeof(brdcast_data_t) == 2, "");

#define DUMP_DAC_X_MARGIN          0
#define DUMP_DAC_X_VOUT_CMP_CONFIG 0
#define DUMP_DAC_X_FUNC_CONFIG     0

#define DUMP_COMMON_TRIGGER        0
#define DUMP_COMMON_DAC_TRIG       0
#define DUMP_GENERAL_STATUS        0
#define DUMP_CMP_STATUS            0
#define DUMP_GPIO_CONFIG           0
#define DUMP_DEVICE_MODE_CONFIG    0 // low power and protect gpi
#define DUMP_SRAM                  0 // fault dump, not used
#define DUMP_BRDCAST_DATA          0 // i2c/pmbus only, not used

#endif /* INC_DAC53202_H_ */
