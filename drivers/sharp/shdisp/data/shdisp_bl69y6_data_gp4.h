/* drivers/sharp/shdisp/data/shdisp_bl69y6_data_gp4.h  (Display Driver)
 *
 * Copyright (C) 2011-2012 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* ------------------------------------------------------------------------- */
/* SHARP DISPLAY DRIVER FOR KERNEL MODE                                      */
/* ------------------------------------------------------------------------- */

#ifndef SHDISP_BL69Y6_DATA_GP4_H
#define SHDISP_BL69Y6_DATA_GP4_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include "../shdisp_bl69y6.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define SHDISP_BKL_FIX_TBL_NUM          23
#define SHDISP_BKL_AUTO_TBL_NUM         16
#define SHDISP_TRI_LED_COLOR_TBL_NUM    8
#define NUM_SHDISP_BKL_TBL_MODE         (SHDISP_BKL_TBL_MODE_CHARGE + 1)

#define SHDISP_INT_ENABLE_GFAC          0x002C0308
#define SHDISP_LUX_CHANGE_LEVEL1        0x0D
#define SHDISP_LUX_CHANGE_LEVEL2        0x01

#define CABC_LUX_LEVEL_LUT0_1           0x01
#define CABC_LUX_LEVEL_LUT1_2           0x02
#define CABC_LUX_LEVEL_LUT2_3           0x03
#define CABC_LUX_LEVEL_LUT3_4           0x04
#define CABC_LUX_LEVEL_LUT4_5           0x05


/* ------------------------------------------------------------------------- */
/* MACROS(Register Value)                                                    */
/* ------------------------------------------------------------------------- */

#define BDIC_REG_SYSTEM4_VAL            0x28
#define BDIC_REG_SLOPE_VAL              0xCB
#define BDIC_REG_DCDC1_VLIM_VAL         0xC0
#define BDIC_REG_DCDC_SYS_VAL           0x20
#define BDIC_REG_DCDC2_VO_VAL           0xE8
#define BDIC_REG_SYSTEM2_BKL            0x03
#define BDIC_REG_ALS_ADJ0_L_DEFAULT     0x5C
#define BDIC_REG_ALS_ADJ0_H_DEFAULT     0x3F
#define BDIC_REG_ALS_ADJ1_L_DEFAULT     0xAA
#define BDIC_REG_ALS_ADJ1_H_DEFAULT     0x4C
#define BDIC_REG_ALS_SHIFT_DEFAULT      0x03
#define BDIC_REG_CLEAR_OFFSET_DEFAULT   0x00
#define BDIC_REG_IR_OFFSET_DEFAULT      0x00

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static const unsigned char shdisp_main_bkl_tbl[SHDISP_BKL_FIX_TBL_NUM][NUM_SHDISP_BKL_TBL_MODE] = {
    { 0x00,   0x00,   0x00,   0x00 },
    { 0x04,   0x04,   0x04,   0x04 },
    { 0x08,   0x05,   0x05,   0x08 },
    { 0x09,   0x08,   0x08,   0x09 },
    { 0x0d,   0x0b,   0x0b,   0x0d },
    { 0x10,   0x0d,   0x0d,   0x10 },
    { 0x13,   0x0f,   0x0f,   0x13 },
    { 0x16,   0x11,   0x11,   0x16 },
    { 0x1a,   0x12,   0x12,   0x1a },
    { 0x1e,   0x14,   0x14,   0x1e },
    { 0x22,   0x16,   0x16,   0x22 },
    { 0x26,   0x18,   0x18,   0x26 },
    { 0x2c,   0x1a,   0x1a,   0x2c },
    { 0x32,   0x1d,   0x1a,   0x32 },
    { 0x39,   0x20,   0x1a,   0x39 },
    { 0x42,   0x24,   0x1a,   0x42 },
    { 0x4b,   0x29,   0x1a,   0x4b },
    { 0x56,   0x2f,   0x1a,   0x56 },
    { 0x63,   0x36,   0x1a,   0x63 },
    { 0x71,   0x3f,   0x1a,   0x71 },
    { 0x81,   0x49,   0x1a,   0x81 },
    { 0x94,   0x54,   0x1a,   0x94 },
    { 0xa9,   0x61,   0x1a,   0xa9 }
};

static const unsigned char shdisp_main_dtv_bkl_tbl[SHDISP_BKL_FIX_TBL_NUM][NUM_SHDISP_BKL_TBL_MODE] = {
    { 0x00,   0x00,   0x00,   0x00 },
    { 0x07,   0x05,   0x05,   0x07 },
    { 0x07,   0x05,   0x05,   0x07 },
    { 0x08,   0x08,   0x08,   0x08 },
    { 0x0b,   0x0a,   0x0a,   0x0b },
    { 0x0f,   0x0d,   0x0d,   0x0f },
    { 0x13,   0x0f,   0x0f,   0x13 },
    { 0x18,   0x12,   0x12,   0x18 },
    { 0x1d,   0x14,   0x14,   0x1d },
    { 0x23,   0x17,   0x17,   0x23 },
    { 0x29,   0x1a,   0x1a,   0x29 },
    { 0x30,   0x1d,   0x1d,   0x30 },
    { 0x38,   0x21,   0x21,   0x38 },
    { 0x40,   0x25,   0x21,   0x40 },
    { 0x49,   0x29,   0x21,   0x49 },
    { 0x52,   0x2e,   0x21,   0x52 },
    { 0x5c,   0x33,   0x21,   0x5c },
    { 0x67,   0x39,   0x21,   0x67 },
    { 0x73,   0x40,   0x21,   0x73 },
    { 0x7f,   0x47,   0x21,   0x7f },
    { 0x8c,   0x4f,   0x21,   0x8c },
    { 0x9a,   0x58,   0x21,   0x9a },
    { 0xa9,   0x61,   0x21,   0xa9 }
};



static const unsigned char shdisp_main_bkl_opt_low_tbl[SHDISP_BKL_AUTO_TBL_NUM][1+NUM_SHDISP_BKL_TBL_MODE] = {
    { BDIC_REG_OPT0,    0x08,   0x04,   0x04,   0x0a },
    { BDIC_REG_OPT1,    0x0a,   0x06,   0x06,   0x0d },
    { BDIC_REG_OPT2,    0x0e,   0x08,   0x08,   0x11 },
    { BDIC_REG_OPT3,    0x12,   0x0b,   0x0B,   0x18 },
    { BDIC_REG_OPT4,    0x17,   0x0d,   0x0D,   0x1f },
    { BDIC_REG_OPT5,    0x1c,   0x10,   0x10,   0x28 },
    { BDIC_REG_OPT6,    0x21,   0x14,   0x14,   0x31 },
    { BDIC_REG_OPT7,    0x27,   0x17,   0x17,   0x3c },
    { BDIC_REG_OPT8,    0x2c,   0x1a,   0x1A,   0x47 },
    { BDIC_REG_OPT9,    0x32,   0x1d,   0x1A,   0x53 },
    { BDIC_REG_OPT10,   0x37,   0x20,   0x1A,   0x60 },
    { BDIC_REG_OPT11,   0x3d,   0x24,   0x1A,   0x6e },
    { BDIC_REG_OPT12,   0x43,   0x27,   0x1A,   0x7e },
    { BDIC_REG_OPT13,   0x49,   0x2b,   0x1A,   0x8f },
    { BDIC_REG_OPT14,   0x4f,   0x2e,   0x1A,   0x9b },
    { BDIC_REG_OPT15,   0xa9,   0x3a,   0x1A,   0xa9 }
};
static const unsigned char shdisp_main_dtv_bkl_opt_low_tbl[SHDISP_BKL_AUTO_TBL_NUM][1+NUM_SHDISP_BKL_TBL_MODE] = {
    { BDIC_REG_OPT0,    0x0a,   0x06,   0x06,   0x0a },
    { BDIC_REG_OPT1,    0x0d,   0x08,   0x08,   0x0d },
    { BDIC_REG_OPT2,    0x11,   0x0a,   0x0a,   0x11 },
    { BDIC_REG_OPT3,    0x17,   0x0d,   0x0d,   0x18 },
    { BDIC_REG_OPT4,    0x1d,   0x11,   0x11,   0x1f },
    { BDIC_REG_OPT5,    0x23,   0x15,   0x15,   0x28 },
    { BDIC_REG_OPT6,    0x2a,   0x19,   0x19,   0x31 },
    { BDIC_REG_OPT7,    0x31,   0x1d,   0x1d,   0x3c },
    { BDIC_REG_OPT8,    0x38,   0x21,   0x21,   0x47 },
    { BDIC_REG_OPT9,    0x3f,   0x25,   0x21,   0x53 },
    { BDIC_REG_OPT10,   0x46,   0x29,   0x21,   0x60 },
    { BDIC_REG_OPT11,   0x4d,   0x2d,   0x21,   0x6e },
    { BDIC_REG_OPT12,   0x55,   0x32,   0x21,   0x7e },
    { BDIC_REG_OPT13,   0x5e,   0x37,   0x21,   0x8f },
    { BDIC_REG_OPT14,   0x65,   0x3b,   0x21,   0x9b },
    { BDIC_REG_OPT15,   0xa9,   0x49,   0x21,   0xa9 }
};

static const unsigned char shdisp_main_bkl_opt_high_tbl[SHDISP_BKL_AUTO_TBL_NUM][1+NUM_SHDISP_BKL_TBL_MODE] = {
    { BDIC_REG_OPT0,    0x44,   0x28,   0x1a,   0x81 },
    { BDIC_REG_OPT1,    0x49,   0x2b,   0x1a,   0x8e },
    { BDIC_REG_OPT2,    0x4d,   0x2d,   0x1a,   0x98 },
    { BDIC_REG_OPT3,    0x50,   0x2f,   0x1a,   0x9c },
    { BDIC_REG_OPT4,    0x54,   0x31,   0x1a,   0xa0 },
    { BDIC_REG_OPT5,    0x58,   0x33,   0x1a,   0xa3 },
    { BDIC_REG_OPT6,    0x5c,   0x36,   0x1a,   0xa6 },
    { BDIC_REG_OPT7,    0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT8,    0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT9,    0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT10,   0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT11,   0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT12,   0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT13,   0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT14,   0xa9,   0x3a,   0x1a,   0xa9 },
    { BDIC_REG_OPT15,   0xa9,   0x3a,   0x1a,   0xa9 }
};
static const unsigned char shdisp_main_dtv_bkl_opt_high_tbl[SHDISP_BKL_AUTO_TBL_NUM][1+NUM_SHDISP_BKL_TBL_MODE] = {
    { BDIC_REG_OPT0,    0x57,   0x33,   0x21,   0x81 },
    { BDIC_REG_OPT1,    0x5d,   0x36,   0x21,   0x8e },
    { BDIC_REG_OPT2,    0x62,   0x39,   0x21,   0x98 },
    { BDIC_REG_OPT3,    0x67,   0x3c,   0x21,   0x9c },
    { BDIC_REG_OPT4,    0x6c,   0x3f,   0x21,   0xa0 },
    { BDIC_REG_OPT5,    0x70,   0x41,   0x21,   0xa3 },
    { BDIC_REG_OPT6,    0x76,   0x44,   0x21,   0xa6 },
    { BDIC_REG_OPT7,    0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT8,    0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT9,    0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT10,   0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT11,   0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT12,   0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT13,   0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT14,   0xa9,   0x49,   0x21,   0xa9 },
    { BDIC_REG_OPT15,   0xa9,   0x49,   0x21,   0xa9 }
};

static const unsigned char shdisp_main_bkl_adj_tbl[NUM_SHDISP_MAIN_BKL_ADJ] = {
    0x00,
    0x02,
    0x04,
    0x06
};

static const unsigned char shdisp_triple_led_tbl[SHDISP_TRI_LED_COLOR_TBL_NUM][3] = {
    { 0x00, 0x00, 0x00 },
    { 0x38, 0x00, 0x00 },
    { 0x00, 0x18, 0x00 },
    { 0x38, 0x28, 0x00 },
    { 0x00, 0x00, 0x40 },
    { 0x30, 0x00, 0x18 },
    { 0x00, 0x20, 0x20 },
    { 0x30, 0x28, 0x20 }
};

static const unsigned char shdisp_triple_led_anime_tbl[2][SHDISP_TRI_LED_COLOR_TBL_NUM][3] = {
    {
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00 }
    },
    {
        { 0x00, 0x00, 0x00 },
        { 0x38, 0x00, 0x00 },
        { 0x00, 0x18, 0x00 },
        { 0x38, 0x28, 0x00 },
        { 0x00, 0x00, 0x40 },
        { 0x30, 0x00, 0x18 },
        { 0x00, 0x20, 0x20 },
        { 0x30, 0x28, 0x20 }
    }
};

static const unsigned char shdisp_main_bkl_chg_high_tbl[14][2] = {
    { 0x00, 0xad },
    { 0x00, 0xa8 },
    { 0x00, 0xb8 },
    { 0x00, 0xb2 },
    { 0x00, 0xc3 },
    { 0x00, 0xbf },
    { 0x00, 0xca },
    { 0x00, 0xc7 },
    { 0x00, 0xd5 },
    { 0x00, 0xd1 },
    { 0x00, 0xdd },
    { 0x00, 0xdb },
    { 0x00, 0xe9 },
    { 0x00, 0xe5 }
};

static const struct shdisp_bdic_bkl_lux_str shdisp_bdic_bkl_lux_tbl[2][16] = {
    {
        { 0, 0x000F,      3 },
        { 0, 0x001F,      6 },
        { 0, 0x002F,     12 },
        { 0, 0x003F,     23 },
        { 0, 0x004F,     42 },
        { 0, 0x005F,     78 },
        { 0, 0x006F,    146 },
        { 0, 0x007F,    270 },
        { 0, 0x008F,    500 },
        { 0, 0x009F,    950 },
        { 0, 0x00AF,   1800 },
        { 0, 0x00BF,   3300 },
        { 0, 0x00CF,   6300 },
        { 0, 0x00DF,  11000 },
        { 1, 0x00EF,  27000 },
        { 1, 0x00FF,  60000 }
    },
    {
        { 0, 0x008F,   4800 },
        { 1, 0x009A,   7800 },
        { 1, 0x00A5,  11500 },
        { 1, 0x00AD,  16500 },
        { 1, 0x00B7,  24000 },
        { 1, 0x00C0,  34000 },
        { 1, 0x00CB,  51500 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 },
        { 1, 0x00FF,  65000 }
    }
};

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */



/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */



#endif /* SHDISP_BL69Y6_DATA_GP4_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
