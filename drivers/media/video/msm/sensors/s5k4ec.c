/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"
#include <linux/gpio.h>
#include <sharp/sh_smem.h>
#include "s5k4ec.h"
#include "msm_camera_i2c.h"

DEFINE_MUTEX(s5k4ec_mut);
static struct msm_sensor_ctrl_t s5k4ec_s_ctrl;
#define SENSOR_NAME "s5k4ec"

static unsigned char s5k4ec_otp_data[1024];
static unsigned char s5k4ec_diag_data[3];
static unsigned short s5k4ec_af_macro_position = S5K4EC_FOCUS_POSITION_MACRO;

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define Q8    0x00000100

/* New 1.2M CMOS product ID register address */
#define REG_S5K8AAYX_MODEL_ID                   0x01A4
#define S5K4EC_I2C_RETRY_TIMES                  10

#define S5K4EC_FOCUS_LOW_LIGHT              0x002E86F1

#define S5K8AAYX_MODEL_ID                       0x4EC0
/* New 1.2M CMOS product ID */

/* Time in milisecs for waiting for the sensor to reset */
#define S5K8AAYX_RESET_DELAY_MSECS    66
#define S5K8AAYX_DEFAULT_CLOCK_RATE   24000000
/* Registers*/

/* Color bar pattern selection */
#define S5K8AAYX_COLOR_BAR_PATTERN_SEL_REG     0x82
/* Color bar enabling control */
#define S5K8AAYX_COLOR_BAR_ENABLE_REG           0x601
/* Time in milisecs for waiting for the sensor to reset*/
#define S5K8AAYX_RESET_DELAY_MSECS    66

#define S5K4EC_FOCUS_POSITION_MIN           0x0000
#define S5K4EC_FOCUS_POSITION_MAX           0x00FF

/*============================================================================
							DATA DECLARATIONS
============================================================================*/
static struct msm_camera_i2c_conf_array s5k4ec_prev_conf[] = {
	{&s5k4ec_capture_tbl[0],
	ARRAY_SIZE(s5k4ec_capture_tbl), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k4ec_preview_tbl[0],
	ARRAY_SIZE(s5k4ec_preview_tbl), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_sensor_output_info_t s5k4ec_dimensions[] = {
	{
		.x_output = 0xA00, /* 2560 */
		.y_output = 0x780, /* 1920 */
		.line_length_pclk = 0xA00, /* 1402 */
		.frame_length_lines = 0x780, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x500, /* 1280 */
		.y_output = 0x3C0, /*  960 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	}
};

static struct msm_camera_csid_vc_cfg s5k4ec_cid_cfg[] = {
	{0, 0x1E, CSI_DECODE_8BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params s5k4ec_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 2,
		.lut_params = {
			.num_cid = ARRAY_SIZE(s5k4ec_cid_cfg),
			.vc_cfg = s5k4ec_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 2,
		.settle_cnt = 0x06,
	},
};

static struct msm_camera_csi2_params *s5k4ec_csi_params_array[] = {
	&s5k4ec_csi_params,
	&s5k4ec_csi_params,
};

int32_t s5k4ec_camera_i2c_txdata(struct msm_camera_i2c_client *dev_client,
				unsigned char *txdata, int length)
{
	int32_t rc = 0;
	uint16_t saddr = dev_client->client->addr >> 1;
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	rc = i2c_transfer(dev_client->client->adapter, msg, 1);
	if (rc < 0)
		CDBG("msm_camera_i2c_txdata faild 0x%x\n", saddr);
	return rc;
}

int32_t s5k4ec_camera_i2c_write(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	int trytimes)
{
	int32_t rc = -EFAULT;
	unsigned char buf[client->addr_type+MSM_CAMERA_I2C_WORD_DATA];
	uint8_t len = 0;
	int32_t s5k4ec_counter=0;

	CDBG("%s addr=0x%04x: data=0x%04x\n", __func__, addr, data);

	buf[0] = addr >> BITS_PER_BYTE;
	buf[1] = addr;
	len = 2;
	buf[len] = data >> BITS_PER_BYTE;
	buf[len+1] = data;
	len += 2;

	while ((s5k4ec_counter < trytimes) && (rc < 0)) {
		rc = s5k4ec_camera_i2c_txdata(client, buf, len);
		if (rc < 0) {
			s5k4ec_counter++;
			CDBG("%s failed rc=%d s5k4ec_counter=%d\n", __func__, rc, s5k4ec_counter);
			usleep_range(20000,20000);
		}
	}
	return rc;
}

int32_t s5k4ec_sensor_write_seq(struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_i2c_reg_conf *settings_array, uint16_t settings_array_size)
{
	int32_t rc=0;
	int32_t i;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;
	uint16_t addr_pos,burst_num;
	unsigned char buf[128];
	
	reg_conf_tbl = settings_array;
	size = settings_array_size;
	addr_pos = 0;
	burst_num = 0;
	for (i = 0; i < size; i++) {
		if(reg_conf_tbl->reg_addr == 0x0F12){
			buf[burst_num] = (unsigned char) ((reg_conf_tbl->reg_data) >> 8);
			burst_num++;
			buf[burst_num] = (unsigned char) ((reg_conf_tbl->reg_data) & 0x00FF);
			burst_num++;
			
			if(burst_num >= 128){
				addr_pos = reg_conf_tbl->reg_addr;
				CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
				rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
																addr_pos,
																buf,
																burst_num);
				burst_num = 0;
			}
		} else {
			if(burst_num > 0){
				addr_pos = 0x0F12;
				CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
				rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
																addr_pos,
																buf,
																burst_num);
			}
			addr_pos = reg_conf_tbl->reg_addr;
			burst_num = 2;
			buf[0] = (unsigned char) ((reg_conf_tbl->reg_data) >> 8);
			buf[1] = (unsigned char) ((reg_conf_tbl->reg_data) & 0x00FF);
			CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
			rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
															addr_pos,
															buf,
															burst_num);
			addr_pos = 0;
			burst_num = 0;
		}
		reg_conf_tbl++;
	}
	if(burst_num > 0){
		addr_pos = 0x0F12;
		CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
		rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
														addr_pos,
														buf,
														burst_num);
	}
	usleep_range(0,0);
	return rc;
}

int32_t s5k4ec_camera_i2c_read(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t *data)
{
	int32_t rc = -EFAULT;
	unsigned short work = 0;

	rc = s5k4ec_camera_i2c_write(client, 0x002C, 0x7000, S5K4EC_I2C_RETRY_TIMES);
	if (rc < 0) {
		pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
		return rc;
	}

	rc = s5k4ec_camera_i2c_write(client, 0x002E, addr, S5K4EC_I2C_RETRY_TIMES);
	if (rc < 0) {
		pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
		return rc;
	}

	rc = msm_camera_i2c_read(client, 0x0F12, &work, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
		return rc;
	}
	*data = work;
	CDBG("%s addr=0x%04x: data=0x%04x\n", __func__, addr, *data);

	return 0;
}

static int s5k4ec_get_otpdata(struct msm_camera_i2c_client *client, uint8_t * buf)
{
	int i = 0;
	int32_t rc;

    do {

		/*-------- s/w core reset --------*/
		rc = s5k4ec_camera_i2c_write(client, 0x0028, 0xD000, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = s5k4ec_camera_i2c_write(client, 0x002A, 0x0012, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = s5k4ec_camera_i2c_write(client, 0x0F12, 0x0001, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}

		/*-------- clock enable to control block --------*/
		rc = s5k4ec_camera_i2c_write(client, 0x002A, 0x007A, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = s5k4ec_camera_i2c_write(client, 0x0F12, 0x0000, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}

        /*-------- make initial state --------*/
		rc = s5k4ec_camera_i2c_write(client, 0x002A, 0xA000, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = s5k4ec_camera_i2c_write(client, 0x0F12, 0x0004, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}

		/*-------- read by page --------*/
		for(i = 0; i < 16; i++) {
			/*-------- set page --------*/
			rc = s5k4ec_camera_i2c_write(client, 0x002A, 0xA002, S5K4EC_I2C_RETRY_TIMES);
			if (rc < 0) {
				pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}
			rc = s5k4ec_camera_i2c_write(client, 0x0F12, i, S5K4EC_I2C_RETRY_TIMES);
			if (rc < 0) {
				pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}

			/*-------- start read mode --------*/
			rc = s5k4ec_camera_i2c_write(client, 0x002A, 0xA000, S5K4EC_I2C_RETRY_TIMES);
			if (rc < 0) {
				pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}
			rc = s5k4ec_camera_i2c_write(client, 0x0F12, 0x0001, S5K4EC_I2C_RETRY_TIMES);
			if (rc < 0) {
				pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}

			udelay(100);

			rc = s5k4ec_camera_i2c_write(client, 0x002C, 0xD000, S5K4EC_I2C_RETRY_TIMES);
			if (rc < 0) {
				pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}

			rc = s5k4ec_camera_i2c_write(client, 0x002E, 0xA006, S5K4EC_I2C_RETRY_TIMES);
			if (rc < 0) {
				pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}

			rc = msm_camera_i2c_read_seq(client, 0x0F12, &buf[(i * 64)], 64);
			if (rc < 0) {
				pr_err("%s:%d i2c_multipleRead failed!. rc=%d\n", __func__, __LINE__, rc);
				return rc;
			}

			{
				int k;
				for(k = 0; k < 8; k++) {
					CDBG("%s buf[%d]=0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __func__, (i * 64) + (k * 8), 
							(unsigned int)buf[((i * 64) + (k * 8) + 0)],
							(unsigned int)buf[((i * 64) + (k * 8) + 1)],
							(unsigned int)buf[((i * 64) + (k * 8) + 2)],
							(unsigned int)buf[((i * 64) + (k * 8) + 3)],
							(unsigned int)buf[((i * 64) + (k * 8) + 4)],
							(unsigned int)buf[((i * 64) + (k * 8) + 5)],
							(unsigned int)buf[((i * 64) + (k * 8) + 6)],
							(unsigned int)buf[((i * 64) + (k * 8) + 7)]);
				}
			}
		} /* for(i = 0; i < 16; i++) */


		/*-------- make initial state --------*/
		rc = s5k4ec_camera_i2c_write(client, 0x002A, 0xA000, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = s5k4ec_camera_i2c_write(client, 0x0F12, 0x0004, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}

		/*-------- interface off --------*/
		rc = s5k4ec_camera_i2c_write(client, 0x002A, 0xA000, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = s5k4ec_camera_i2c_write(client, 0x0F12, 0x0000, S5K4EC_I2C_RETRY_TIMES);
		if (rc < 0) {
			pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
			return rc;
		}

	} while (0);

	return 0;
}


static int s5k4ec_mode_upadate_chk(struct msm_sensor_ctrl_t *s_ctrl, unsigned short raddr)
{
	int rc = 0;
	int cnt = 0;
	unsigned short work = 0;

	CDBG("%s Check start. addr=0x%04x\n", __func__, raddr);
	while(1) {
		work = 0;
		rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, raddr, &work);
		if(0 != rc) {
			return rc;
		}
		else {
			if(work != 0x0000)
			{
				if(cnt == 2000){
					pr_err("s5k4ec update timeout error!! reg=0x%x\n", work);
					return -ETIMEDOUT;
				}
				cnt++;
				usleep_range(2000,2000);
				continue;
			}
			else
			{
				CDBG("%s Check OK. cnt=%d\n", __func__, cnt);
				break;
			}
		}
	}
	return 0;
}

#define S5K4EC_FOCUS_FULL_START  0x2A
#define S5K4EC_FOCUS_FULL_STEPS  12
#define S5K4EC_FOCUS_MACRO_STEPS 10
#define SENSOR_DEF_100_UP	0x64

static int32_t s5k4ec_set_af_table(struct msm_sensor_ctrl_t *s_ctrl, unsigned char *diag_data)
{
	int rc = 0;
	uint32_t af_default_in_macro;
	uint32_t l_full_stop, l_macro_start, l_macro_stop;
	uint16_t full_step, full_start, full_stop;
	uint16_t macro_step, macro_start, macro_stop;
	uint16_t lens_position = 0;
	int table_index = 0;
	int cnt = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	if(diag_data[0] == 0x55){
		af_default_in_macro = ((unsigned short)diag_data[1] << 8) + (unsigned short)diag_data[2];
	} else {
		af_default_in_macro = SENSOR_DEF_100_UP;
	}

	CDBG("%s af_default_in_macro=%0x\n", __func__, af_default_in_macro);

	l_full_stop   = af_default_in_macro * 1100;
	l_macro_start = af_default_in_macro * 650;
	l_macro_stop  = af_default_in_macro * 1200;

	full_step      = S5K4EC_FOCUS_FULL_STEPS;
	full_start     = S5K4EC_FOCUS_FULL_START;
	full_stop      = (unsigned short)((l_full_stop + 500)/1000);
	CDBG("%s full_step=%0x, full_start=%0x, full_stop=%0x\n", __func__, full_step, full_start, full_stop);

	macro_step     = S5K4EC_FOCUS_MACRO_STEPS;
	macro_start    = (unsigned short)((l_macro_start + 500)/1000);
	macro_stop     = (unsigned short)((l_macro_stop + 500)/1000);
	CDBG("%s macro_step=%0x, macro_start=%0x, macro_stop=%0x\n", __func__, macro_step, macro_start, macro_stop);

	if(full_start < full_stop ) {

		table_index = 2;
		s5k4ec_af_lens_position_normal_tbl[table_index].reg_data = full_step - 1;
		s5k4ec_af_lens_position_caf_tbl[table_index].reg_data = full_step - 1;
		CDBG("%s s5k4ec_af_lens_position_normal_tbl[%d].reg_data = 0x%04x !!\n",
				__func__, table_index, s5k4ec_af_lens_position_normal_tbl[table_index].reg_data);

		for(cnt = 0; cnt < 32; cnt++) {
			table_index++;
			if (cnt < full_step) {
				lens_position = (full_start * 10) + (cnt * (full_stop - full_start) * 10 / (full_step - 1));
				lens_position = (lens_position + 5) / 10;
				s5k4ec_af_lens_position_normal_tbl[table_index].reg_data = lens_position;
				s5k4ec_af_lens_position_caf_tbl[table_index].reg_data = lens_position;
			} else {
				s5k4ec_af_lens_position_normal_tbl[table_index].reg_data = 0x00;
				s5k4ec_af_lens_position_caf_tbl[table_index].reg_data =0x00;
			}
			CDBG("%s s5k4ec_af_lens_position_normal_tbl[%d].reg_data = 0x%04x !!\n",
					__func__, table_index, s5k4ec_af_lens_position_normal_tbl[table_index].reg_data);
		}
	} else {
		CDBG("%s Used Default s5k4ec_af_lens_position_normal_tbl !!\n", __func__);
	}

	if((macro_start < macro_stop) ) {

		table_index = 2;
		s5k4ec_af_lens_position_macro_tbl[table_index].reg_data = macro_step - 1;
		s5k4ec_af_mode_macro_tbl[10].reg_data = macro_step - 1;
		CDBG("%s s5k4ec_af_lens_position_macro_tbl[%d].reg_data = 0x%04x !!\n",
				__func__, table_index, s5k4ec_af_lens_position_macro_tbl[table_index].reg_data);

		for(cnt = 0; cnt < 32; cnt++) {
			table_index++;
			if (cnt < full_step) {
				lens_position = (macro_start * 10) + (cnt * (macro_stop - macro_start) * 10 / (macro_step - 1));
				lens_position = (lens_position + 5) / 10;
				s5k4ec_af_lens_position_macro_tbl[table_index].reg_data = lens_position;
			} else {
				s5k4ec_af_lens_position_macro_tbl[table_index].reg_data = 0x00;
			}
			CDBG("%s s5k4ec_af_lens_position_macro_tbl[%d].reg_data = 0x%04x !!\n",
					__func__, table_index, s5k4ec_af_lens_position_macro_tbl[table_index].reg_data);
		}
	} else {
		CDBG("%s Used Default s5k4ec_af_lens_position_macro_tbl !!\n", __func__);
	}

	reg_conf_tbl = &s5k4ec_af_lens_position_normal_tbl[0];
	size = ARRAY_SIZE(s5k4ec_af_lens_position_normal_tbl);

	rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
	if (rc < 0) {
		pr_err("%s s5k4ec_af_lens_position_normal_tbl write error !!\n", __func__);
		return rc;
	}

	return 0;

}

static int s5k4ec_sensor_start_af(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	unsigned short read_data  = 0;
	uint32_t lei = 0;
	unsigned short SingleAfFlags = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	CDBG("%s start\n", __func__);

	do{
		reg_conf_tbl = &s5k4ec_init_af_tbl[0];
		size = ARRAY_SIZE(s5k4ec_init_af_tbl);

		rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
		if(rc < 0) {
			pr_err("[%d]:write s5k4ec_init_af_tbl failed!.\n", __LINE__);
			return rc;
		}

		rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AF_search_usStatus, &read_data);
		if(rc >= 0) {
			CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
		} else {
			pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
					__LINE__, Mon_AF_stat_ulCurStat_0_);
			return rc;
		}

		if(read_data != 0x00){
			usleep_range(1000,1000);
		}
	} while(read_data != 0x00);

	read_data  = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AAIO_ulOptimalLei_0_, &read_data);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
		lei = (read_data & 0x0000FFFF);
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}

	read_data  = 0;
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x0F12, &read_data, MSM_CAMERA_I2C_WORD_DATA);
	if(rc >= 0) {
		lei |= ((read_data << 16) & 0xFFFF0000);
		CDBG("%s:lei = 0x%08lx\n", __func__, (unsigned long)lei);
	} else {
		pr_err("[%s]:s5k4ec i2c_read failed!.\n", __func__);
		return rc;
	}

	read_data  = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, af_search_usSingleAfFlags, &read_data);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
		SingleAfFlags = read_data;
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}
	if (lei >= S5K4EC_FOCUS_LOW_LIGHT) {
		if ((SingleAfFlags & 0x000F) != 0x0000) {
			SingleAfFlags &= 0xFFF0;
		}
	} else {
		if ((SingleAfFlags & 0x0002) != 0x0002) {
			SingleAfFlags |= 0x0002;
		}
	}
	if (SingleAfFlags != read_data) {
		CDBG("%s:Write SingleAfFlags = 0x%04x\n", __func__, SingleAfFlags);
		s5k4ec_pre_single_af_tbl[2].reg_data = (unsigned short)SingleAfFlags;
		reg_conf_tbl = &s5k4ec_pre_single_af_tbl[0];
		size = ARRAY_SIZE(s5k4ec_pre_single_af_tbl);

		rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
		if(rc < 0) {
			pr_err("[%d]:write s5k4ec_pre_single_af_tbl failed!.\n", __LINE__);
			return rc;
		}
	}

	reg_conf_tbl = &s5k4ec_single_af_tbl[0];
	size = ARRAY_SIZE(s5k4ec_single_af_tbl);

	rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
	if(rc < 0) {
		pr_err("[%d]:write s5k4ec_single_af_tbl failed!.\n", __LINE__);
		return rc;
	}
	CDBG("%s end rc = %d\n", __func__, rc);
	return rc;
}

static int s5k4ec_sensor_start_manual_focus(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;
	int16_t af_position = cdata->cfg.focus_info.af_position;

	CDBG(" %s af_position =%d\n", __func__, af_position);
	if ((af_position >= S5K4EC_FOCUS_POSITION_MIN) || (af_position <= S5K4EC_FOCUS_POSITION_MAX)) {
		s5k4ec_manual_af_tbl[2].reg_data = (unsigned short)af_position;
	} else {
		pr_err("%s af_position(=%d) invalid \n", __func__, af_position);
		return -EINVAL;
	}

	reg_conf_tbl = &s5k4ec_manual_af_tbl[0];
	size = ARRAY_SIZE(s5k4ec_manual_af_tbl);

	rc = s5k4ec_sensor_write_seq(s_ctrl, s5k4ec_manual_af_tbl, size);
	if (rc < 0) {
		pr_err("%s s5k4ec_manual_af_tbl write error !!\n", __func__);
		return rc;
	}

	CDBG("%s rc = %d\n", __func__, rc);

	return 0;

}

static int s5k4ec_get_af_search_status(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	unsigned short read_data  = 0;

	/* status read */
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AF_search_usStatus, &read_data);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}

	cdata->cfg.focus_info.af_1st_status = read_data;

	read_data = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_SKL_AfOutput_bChangeCfgeCfgDisable, &read_data);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}
	cdata->cfg.focus_info.af_2nd_status = read_data & 0x00FF;

	read_data = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AF_stat_ulCurStat_0_, &read_data);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}
	cdata->cfg.focus_info.af_integrated_value = read_data & 0x0000FFFF;

	read_data = 0;
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x0F12, &read_data, MSM_CAMERA_I2C_WORD_DATA);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}
	cdata->cfg.focus_info.af_integrated_value |= ((((uint32_t)read_data) << 16) & 0xFFFF0000);

	if(cdata->cfg.focus_info.af_stage == SHCAM_FOCUS_MODE_PRODUCT){
		s5k4ec_sensor_start_manual_focus(s_ctrl, cdata);
	}

	read_data = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AF_pos_usCurPos, &read_data);
	if(rc >= 0) {
		CDBG("%s:read_data = 0x%04x\n", __func__, read_data);
	} else {
		pr_err("[%d]:s5k4ec i2c_read failed!. addr=0x%04x\n",
				__LINE__, Mon_AF_stat_ulCurStat_0_);
		return rc;
	}
	cdata->cfg.focus_info.af_position = read_data;

	CDBG("%s cdata->cfg.focus_info.af_1st_status = 0x%0x\n", __func__, cdata->cfg.focus_info.af_1st_status);
	CDBG("%s cdata->cfg.focus_info.af_2nd_status = 0x%0x\n", __func__, cdata->cfg.focus_info.af_2nd_status);
	CDBG("%s cdata->cfg.focus_info.af_position = 0x%0x\n", __func__, cdata->cfg.focus_info.af_position);
	CDBG("%s cdata->cfg.focus_info.af_integrated_value = 0x%0x\n", __func__, cdata->cfg.focus_info.af_integrated_value);

	CDBG("%s end rc = %d\n", __func__, rc);
	return rc;
}

static int s5k4ec_set_focus_mode(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint8_t focus_mode = cdata->cfg.focus_info.focus_mode;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	CDBG("%s focus_mode=%d\n", __func__, focus_mode);
	switch (focus_mode) {
		case SHCAM_FOCUS_MODE_NORMAL:
			reg_conf_tbl = &s5k4ec_af_lens_position_normal_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_lens_position_normal_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_lens_position_normal_tbl write error !!\n", __func__);
				return rc;
			}

			reg_conf_tbl = &s5k4ec_af_mode_normal_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_mode_normal_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_mode_normal_tbl write error !!\n", __func__);
				return rc;
			}
			break;
		case SHCAM_FOCUS_MODE_MACRO:
			reg_conf_tbl = &s5k4ec_af_lens_position_macro_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_lens_position_macro_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_lens_position_macro_tbl write error !!\n", __func__);
				return rc;
			}

			s5k4ec_af_mode_macro_tbl[2].reg_data = s5k4ec_af_macro_position;

			reg_conf_tbl = &s5k4ec_af_mode_macro_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_mode_macro_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_mode_macro_tbl write error !!\n", __func__);
				return rc;
			}
			break;
		case SHCAM_FOCUS_MODE_INFINITY:
			cdata->cfg.focus_info.af_position = S5K4EC_FOCUS_FULL_START;
			rc = s5k4ec_sensor_start_manual_focus(s_ctrl, cdata);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_mode_macro_tbl write error !!\n", __func__);
				return rc;
			}
			break;
		case SHCAM_FOCUS_MODE_PRODUCT:
			reg_conf_tbl = &s5k4ec_af_mode_product_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_mode_product_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_mode_product_tbl write error !!\n", __func__);
				return rc;
			}
			break;
		case SHCAM_FOCUS_MODE_CAF:
			reg_conf_tbl = &s5k4ec_af_lens_position_caf_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_lens_position_caf_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_lens_position_caf_tbl write error !!\n", __func__);
				return rc;
			}

			reg_conf_tbl = &s5k4ec_af_mode_continuous_tbl[0];
			size = ARRAY_SIZE(s5k4ec_af_mode_continuous_tbl);

			rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
			if (rc < 0) {
				pr_err("%s s5k4ec_af_mode_continuous_tbl write error !!\n", __func__);
				return rc;
			}
			break;
		default:
			pr_err("[%s] Invalid focus mode !!\n", __func__);
		return -EINVAL;
	}

	CDBG("%s rc = %d\n", __func__, rc);
	return 0;

}

static int s5k4ec_set_focus_area(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	CDBG("%s focus_area.x=%d focus_area.dx=%d focus_area.y=%d focus_area.dy=%d\n", __func__, cdata->cfg.focus_area.x, cdata->cfg.focus_area.dx, cdata->cfg.focus_area.y, cdata->cfg.focus_area.dy);

	s5k4ec_af_area_tbl[2].reg_data = (unsigned short)cdata->cfg.focus_area.x;
	s5k4ec_af_area_tbl[3].reg_data = (unsigned short)cdata->cfg.focus_area.y;
	s5k4ec_af_area_tbl[4].reg_data = (unsigned short)cdata->cfg.focus_area.dx;
	s5k4ec_af_area_tbl[5].reg_data = (unsigned short)cdata->cfg.focus_area.dy;
	s5k4ec_af_area_tbl[6].reg_data = (unsigned short)cdata->cfg.focus_area.x;
	s5k4ec_af_area_tbl[7].reg_data = (unsigned short)cdata->cfg.focus_area.y;
	s5k4ec_af_area_tbl[8].reg_data = (unsigned short)cdata->cfg.focus_area.dx;
	s5k4ec_af_area_tbl[9].reg_data = (unsigned short)cdata->cfg.focus_area.dy;

	reg_conf_tbl = &s5k4ec_af_area_tbl[0];
	size = ARRAY_SIZE(s5k4ec_af_area_tbl);

	rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
	if (rc < 0) {
		pr_err("%s s5k4ec_af_lens_position_normal_tbl write error !!\n", __func__);
		return rc;
	}

	CDBG("%s rc = %d\n", __func__, rc);
	return 0;

}

static int s5k4ec_set_focus_cancel(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	reg_conf_tbl = &s5k4ec_cancel_single_af_tbl[0];
	size = ARRAY_SIZE(s5k4ec_cancel_single_af_tbl);

	rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
	if(rc < 0) {
		pr_err("[%d]:write s5k4ec_cancel_single_af_tbl failed!.\n", __LINE__);
		return rc;
	}

	CDBG("%s rc = %d\n", __func__, rc);
	return 0;

}

static int s5k4ec_get_snapshot_info(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	unsigned short read_data  = 0;
	unsigned short work  = 0;

	CDBG("%s start\n", __func__);

	read_data  = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AAIO_PrevAcqCtxt_ME_LEI_Exp, &read_data);
	if (rc < 0) {
		pr_err("[%d]:s5k4ec read failed!. addr=0x%04x\n", __LINE__,
				Mon_AAIO_PrevAcqCtxt_ME_LEI_Exp);
		return rc;
	}

	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x0F12, &work, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s:%d s5k4ec_i2c_write failed!. rc=%d\n", __func__, __LINE__, rc);
		return rc;
	}

	cdata->cfg.sensor_snapshot_info.exp_value = ((uint32_t)read_data & 0xFFFF) + ((uint32_t)work << 16);

	read_data  = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, Mon_AAIO_PrevAcqCtxt_ME_AGain, &read_data);
	if (rc < 0) {
		pr_err("[%d]:s5k4ec read failed!. addr=0x%04x\n", __LINE__,
				Mon_AAIO_PrevAcqCtxt_ME_AGain);
		return rc;
	}
	cdata->cfg.sensor_snapshot_info.a_gain = read_data;

	read_data  = 0;
	rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client,Mon_AAIO_PrevAcqCtxt_ME_DGain, &read_data);
	if (rc != 0) {
		pr_err("[%d]:s5k4ec read failed!. addr=0x%04x\n", __LINE__,
				Mon_AAIO_PrevAcqCtxt_ME_DGain);
		return rc;
	}
	cdata->cfg.sensor_snapshot_info.d_gain = read_data;

	CDBG("%s exp_value= %0x\n", __func__, cdata->cfg.sensor_snapshot_info.exp_value);
	CDBG("%s a_gain= %0x\n", __func__, cdata->cfg.sensor_snapshot_info.a_gain);
	CDBG("%s d_gain= %0x\n", __func__, cdata->cfg.sensor_snapshot_info.d_gain);

	return rc;

}


static int s5k4ec_set_effect(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	switch(cdata->cfg.effect){
		case CAMERA_EFFECT_OFF:
			CDBG("%s CAMERA_EFFECT_OFF\n", __func__);
			s5k4ec_effect_tbl[EFFECT_IDX].reg_data = 0;
		break;
		case CAMERA_EFFECT_MONO:
			CDBG("%s CAMERA_EFFECT_MONO\n", __func__);
			s5k4ec_effect_tbl[EFFECT_IDX].reg_data = 1;
		break;
		case CAMERA_EFFECT_SEPIA:
			CDBG("%s CAMERA_EFFECT_SEPIA\n", __func__);
			s5k4ec_effect_tbl[EFFECT_IDX].reg_data = 4;
		break;
		case CAMERA_EFFECT_AQUA:
			CDBG("%s CAMERA_EFFECT_AQUA\n", __func__);
			s5k4ec_effect_tbl[EFFECT_IDX].reg_data = 5;
		break;
		case CAMERA_EFFECT_EMBOSS:
			CDBG("%s CAMERA_EFFECT_EMBOSS\n", __func__);
			s5k4ec_effect_tbl[EFFECT_IDX].reg_data = 11;
		break;
		case CAMERA_EFFECT_SKETCH:
			CDBG("%s CAMERA_EFFECT_SKETCH\n", __func__);
			s5k4ec_effect_tbl[EFFECT_IDX].reg_data = 9;
		break;
		default:
			return -EFAULT;
		break;
	}

	reg_conf_tbl = &s5k4ec_effect_tbl[0];
	size = ARRAY_SIZE(s5k4ec_effect_tbl);

	rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
	if(rc < 0) {
		pr_err("[%d]:write s5k4ec_effect_tbl failed!.\n", __LINE__);
		return rc;
	}

	CDBG("%s rc = %d\n", __func__, rc);
	return 0;

}

static int s5k4ec_set_bestshot(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	switch(cdata->cfg.bestshot_mode){
		case SHCAMERA_BESTSHOT_OFF:
			CDBG("%s SHCAMERA_BESTSHOT_OFF\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_off_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_off_tbl);
		break;
		case SHCAMERA_BESTSHOT_PORTRAIT:
			CDBG("%s SHCAMERA_BESTSHOT_POTRAIT\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_portrait_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_portrait_tbl);
		break;
		case SHCAMERA_BESTSHOT_NIGHT_PORTRAIT:
			CDBG("%s SHCAMERA_BESTSHOT_NIGHT_PORTRAIT\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_portraitnight_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_portraitnight_tbl);
		break;
		case SHCAMERA_BESTSHOT_LANDSCAPE:
			CDBG("%s SHCAMERA_BESTSHOT_LANDSCAPE\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_landscape_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_landscape_tbl);
		break;
		case SHCAMERA_BESTSHOT_NIGHT:
			CDBG("%s SHCAMERA_BESTSHOT_NIGHT\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_nightshot_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_nightshot_tbl);
		break;
		case SHCAMERA_BESTSHOT_COOKING:
			CDBG("%s SHCAMERA_BESTSHOT_COOKING\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_food_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_food_tbl);
		break;
		case SHCAMERA_BESTSHOT_TEXT:
			CDBG("%s SHCAMERA_BESTSHOT_TEXT\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_text_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_text_tbl);
		break;
		case SHCAMERA_BESTSHOT_SPORTS:
			CDBG("%s SHCAMERA_BESTSHOT_SPORTS\n", __func__);
			reg_conf_tbl = &s5k4ec_scene_sports_tbl[0];
			size = ARRAY_SIZE(s5k4ec_scene_sports_tbl);
		break;
		default:
			return -EFAULT;
		break;
	}

	rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
	if(rc < 0) {
		pr_err("[%d]:write s5k4ec_effect_tbl failed!.\n", __LINE__);
		return rc;
	}
	usleep_range(1000,1000);

	s5k4ec_mode_upadate_chk(s_ctrl,REG_TC_GP_NewConfigSync);
	if(rc != 0) {
		pr_err("%s s5k4ec_mode_upadate_chk err\n",__func__);
		return -EFAULT;
	}

	CDBG("%s rc = %d\n", __func__, rc);
	return 0;

}

static int s5k4ec_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&s5k4ec_mut);
	CDBG("s5k4ec_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SHDIAG_GET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				return -EFAULT;
			}
			CDBG("%s:%d i2c_read addr=0x%0x\n",__func__,__LINE__,cdata.cfg.i2c_info.addr);
			rc = s5k4ec_camera_i2c_read(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr, data);
			CDBG("%s:%d i2c_read data=0x%0x\n",__func__,__LINE__,*(unsigned short *)data);
			if(rc < 0){
				kfree(data);
				rc = -EFAULT;
				break;
			}
			if (copy_to_user((void *)cdata.cfg.i2c_info.data,
				data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				rc = -EFAULT;
				break;
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SHDIAG_SET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				rc = -EFAULT;
				break;
			}
			if (copy_from_user(data,
				(void *)cdata.cfg.i2c_info.data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				rc = -EFAULT;
				break;
			}
			CDBG("%s addr = 0x%x\n",__func__, cdata.cfg.i2c_info.addr);
			rc = s5k4ec_camera_i2c_write(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr, *(uint16_t *)data, S5K4EC_I2C_RETRY_TIMES);
			CDBG("%s *(uint16_t *)data = 0x%x\n",__func__, *(uint16_t *)data);
			if(rc < 0){
				kfree(data);
				rc = -EFAULT;
				break;
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SHDIAG_GET_OTP_DATA:
		{
			if(cdata.cfg.otp_info.length > 1024){
				rc = -EFAULT;
				break;
			}
			if (copy_to_user((void *)cdata.cfg.otp_info.data,
				s5k4ec_otp_data,
				cdata.cfg.otp_info.length)){
				CDBG("%s copy_to_user error\n",__func__);
				rc = -EFAULT;
				break;
			}
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SHDIAG_SET_SMEM:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				rc = -EFAULT;
				break;
			}
			if (copy_from_user(data,
				(void *)cdata.cfg.i2c_info.data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_from_user error\n",__func__);
				rc = -EFAULT;
				break;
			}
			CDBG("%s cdata.cfg.i2c_info.length = 0x%x\n",__func__, cdata.cfg.i2c_info.length);
			if(cdata.cfg.i2c_info.length <= 3){
				memcpy(&s5k4ec_diag_data[0], data, sizeof(s5k4ec_diag_data));
				CDBG("s5k4ec_diag_data[0]: %0x\n", s5k4ec_diag_data[0]);
				CDBG("s5k4ec_diag_data[1]: %0x\n", s5k4ec_diag_data[1]);
				CDBG("s5k4ec_diag_data[2]: %0x\n", s5k4ec_diag_data[2]);
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SET_AUTO_FOCUS:
		{
			rc = s5k4ec_sensor_start_af(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SH_GET_AF_SEARCH_STATUS:
		{
			rc = s5k4ec_get_af_search_status(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SH_SET_FOCUS_MODE:
		{
			rc = s5k4ec_set_focus_mode(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SH_GET_SNAPSHOT_INFO:
		{
			rc = s5k4ec_get_snapshot_info(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SH_SET_FOCUS_AREA:
		{
			rc = s5k4ec_set_focus_area(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SH_SET_FOCUS_CANCEL:
		{
			rc = s5k4ec_set_focus_cancel(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SET_EFFECT:
		{
			rc = s5k4ec_set_effect(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	case CFG_SH_SET_BESTSHOT_MODE:
		{
			rc = s5k4ec_set_bestshot(s_ctrl, &cdata);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
			if(rc < 0){
				rc = -EFAULT;
				break;
			}
			rc = 0;
		}
			break;
	default:
		mutex_unlock(&s5k4ec_mut);
		return msm_sensor_config(s_ctrl, argp);
		break;
	}
	
	mutex_unlock(&s5k4ec_mut);
	return rc;

}

int32_t s5k4ec_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc=0;
	int i=0,retryend=0;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	do{
		reg_conf_tbl = &s5k4ec_init_1_tbl[0];
		size = ARRAY_SIZE(s5k4ec_init_1_tbl);

		rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
		if (rc < 0) {
			pr_err("s5k4ec: sensor s5k4ec_init_1_tbl setting failed!.\n");
			retryend = 1;
			break;
		}
		CDBG("%s %d rc=%d", __func__, __LINE__, rc);

		usleep_range(10000,10000);

		reg_conf_tbl = &s5k4ec_init_2_tbl[0];
		size = ARRAY_SIZE(s5k4ec_init_2_tbl);

		rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
		if (rc < 0) {
			pr_err("s5k4ec: sensor s5k4ec_init_2_tbl setting failed!.\n");
			retryend = 1;
			break;
		}
		CDBG("%s %d rc=%d", __func__, __LINE__, rc);

		usleep_range(50,50);

//		reg_conf_tbl = &s5k4ec_init_notUseOtp_tbl[0];
//		size = ARRAY_SIZE(s5k4ec_init_notUseOtp_tbl);
		reg_conf_tbl = &s5k4ec_init_useOtp_tbl[0];
		size = ARRAY_SIZE(s5k4ec_init_useOtp_tbl);
		rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
		if (rc < 0) {
			pr_err("s5k4ec: sensor s5k4ec_init_notUseOtp_tbl setting failed!.\n");
			retryend = 1;
			break;
		}
		CDBG("%s %d rc=%d", __func__, __LINE__, rc);

		usleep_range(50,50);

		reg_conf_tbl = &s5k4ec_init_3_tbl[0];
		size = ARRAY_SIZE(s5k4ec_init_3_tbl);
		rc = s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);
		if (rc < 0) {
			pr_err("s5k4ec: sensor init_3_tbl setting failed!.\n");
			retryend = 1;
			break;
		}
		CDBG("%s %d rc=%d", __func__, __LINE__, rc);
		usleep_range(1000,1000);

		rc = s5k4ec_mode_upadate_chk(s_ctrl,REG_TC_GP_NewConfigSync);
		if(rc == -ETIMEDOUT) {
			pr_err("s5k4ec:[%d] s5k4ec_mode_upadate_chk retry %d\n",
					__LINE__, i);
			i++;
		}
		else if(rc != 0) {
			pr_err("s5k4ec:[%d] s5k4ec_mode_upadate_chk err\n",__LINE__);
			retryend = 1;
			break;
		}
	} while(rc < 0 && i < 5 && retryend == 0);

	s5k4ec_set_af_table(s_ctrl, s5k4ec_diag_data);

	return rc;

}

void s5k4ec_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	if(s_ctrl->curr_res == MSM_SENSOR_RES_FULL){

		reg_conf_tbl = s5k4ec_cols_qtr_tbl;
		size = ARRAY_SIZE(s5k4ec_cols_qtr_tbl);
		s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);

		reg_conf_tbl = s5k4ec_preview_start_tbl;
		size = ARRAY_SIZE(s5k4ec_preview_start_tbl);
		s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);

		s5k4ec_mode_upadate_chk(s_ctrl,REG_TC_GP_NewConfigSync);

	} if(s_ctrl->curr_res == MSM_SENSOR_RES_QTR){

		reg_conf_tbl = s5k4ec_cols_normal_tbl;
		size = ARRAY_SIZE(s5k4ec_cols_normal_tbl);
		s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);

		reg_conf_tbl = s5k4ec_capture_start_tbl;
		size = ARRAY_SIZE(s5k4ec_capture_start_tbl);
		s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);

		s5k4ec_mode_upadate_chk(s_ctrl,REG_TC_GP_NewConfigSync);

	} else {
		CDBG("%s s_ctrl->curr_res=%d\n", __func__, s_ctrl->curr_res);
	}
}

void s5k4ec_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;

	if(s_ctrl->curr_res == MSM_SENSOR_RES_FULL){
		reg_conf_tbl = s5k4ec_capture_stop_tbl;
		size = ARRAY_SIZE(s5k4ec_capture_stop_tbl);
		s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);

//		s5k4ec_mode_upadate_chk(s_ctrl,REG_TC_GP_NewConfigSync);

	} if(s_ctrl->curr_res == MSM_SENSOR_RES_QTR){
		reg_conf_tbl = s5k4ec_preview_stop_tbl;
		size = ARRAY_SIZE(s5k4ec_preview_stop_tbl);
		s5k4ec_sensor_write_seq(s_ctrl, reg_conf_tbl, size);

//		s5k4ec_mode_upadate_chk(s_ctrl,REG_TC_GP_NewConfigSync);

	} else {
		CDBG("%s s_ctrl->curr_res=%d\n", __func__, s_ctrl->curr_res);
	}
}

static int32_t s5k4ec_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		s5k4ec_sensor_write_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
//		msm_sensor_write_res_settings(s_ctrl, res);
		if (s_ctrl->curr_csi_params != s_ctrl->csi_params[res]) {
			s_ctrl->curr_csi_params = s_ctrl->csi_params[res];
			s_ctrl->curr_csi_params->csid_params.lane_assign =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_assign;
			s_ctrl->curr_csi_params->csiphy_params.lane_mask =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_mask;
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSID_CFG,
				&s_ctrl->curr_csi_params->csid_params);
			mb();
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIPHY_CFG,
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
	}
	return rc;
}

static struct v4l2_subdev_info s5k4ec_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", S5K8AAYX_DEFAULT_CLOCK_RATE},
};

static int32_t s5k4ec_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	CDBG("%s: %d\n", __func__, __LINE__);
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	if (IS_ERR(s_ctrl->reg_ptr[0])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[0].reg_name);
		goto enable_vreg_failed;
	}
	rc = regulator_enable(s_ctrl->reg_ptr[0]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[0].reg_name);
		goto enable_vreg_failed;
	}
	usleep_range(200,1200);
	if (IS_ERR(s_ctrl->reg_ptr[1])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[1].reg_name);
		goto enable_vreg_failed;
	}
	rc = regulator_enable(s_ctrl->reg_ptr[1]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[1].reg_name);
		goto enable_vreg_failed;
	}
	usleep_range(300,1300);
	if (IS_ERR(s_ctrl->reg_ptr[2])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[2].reg_name);
		goto enable_vreg_failed;
	}
	rc = regulator_enable(s_ctrl->reg_ptr[2]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[2].reg_name);
		goto enable_vreg_failed;
	}
	usleep_range(200,1200);
	if (IS_ERR(s_ctrl->reg_ptr[3])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[3].reg_name);
		goto enable_vreg_failed;
	}
	rc = regulator_enable(s_ctrl->reg_ptr[3]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[3].reg_name);
		goto enable_vreg_failed;
	}
	usleep_range(100,1100);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	CDBG("%s: %d\n", __func__, __LINE__);
	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	usleep_range(100,1100);

//	if (data->sensor_platform_info->ext_power_ctrl != NULL)
//		data->sensor_platform_info->ext_power_ctrl(1);

	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;

    CDBG("%s: rc value %d\n", __func__, rc);
    CDBG("%s: %d\n", __func__, __LINE__);

	return rc;
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);
enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
    CDBG("%s: rc value 2nd time %d\n", __func__, rc);
	CDBG("%s: %d\n", __func__, __LINE__);
	return rc;

}

static int32_t s5k4ec_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct msm_camera_gpio_conf *gpio_conf = data->sensor_platform_info->gpio_conf;
	int32_t rc = 0;
	CDBG("%s\n", __func__);

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);

	gpio_set_value_cansleep(gpio_conf->cam_gpio_set_tbl[2].gpio, GPIOF_OUT_INIT_LOW);

	usleep_range(100,1100);

	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	gpio_set_value_cansleep(gpio_conf->cam_gpio_set_tbl[0].gpio, GPIOF_OUT_INIT_LOW);

	msm_camera_config_gpio_table(data, 0);

	if (IS_ERR(s_ctrl->reg_ptr[3])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[3].reg_name);
	}
	rc = regulator_disable(s_ctrl->reg_ptr[3]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[3].reg_name);
	}
	usleep_range(10000,11000);
	if (IS_ERR(s_ctrl->reg_ptr[2])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[2].reg_name);
	}
	rc = regulator_disable(s_ctrl->reg_ptr[2]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[2].reg_name);
	}
	usleep_range(1000,2000);
	if (IS_ERR(s_ctrl->reg_ptr[0])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[0].reg_name);
	}
	rc = regulator_disable(s_ctrl->reg_ptr[0]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[0].reg_name);
	}
	usleep_range(1500,2500);
	if (IS_ERR(s_ctrl->reg_ptr[1])) {
		pr_err("%s: %s null regulator\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[1].reg_name);
	}
	rc = regulator_disable(s_ctrl->reg_ptr[1]);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n",
			__func__, s_ctrl->sensordata->sensor_platform_info->cam_vreg[1].reg_name);
	}

	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);

	msm_camera_request_gpio_table(data, 0);

	kfree(s_ctrl->reg_ptr);

	return 0;
}

static int32_t s5k4ec_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	sharp_smem_common_type *p_sh_smem_common_type = NULL;

	rc = s5k4ec_camera_i2c_write(
			s_ctrl->sensor_i2c_client,
			0xFCFC, 0xD000,
			S5K4EC_I2C_RETRY_TIMES);
	if (rc < 0) {
		pr_err("%s: %s: write id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	rc = s5k4ec_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("msm_sensor id: %d\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}

	memset(s5k4ec_otp_data, 0 ,sizeof(s5k4ec_otp_data));
	rc = s5k4ec_get_otpdata(s_ctrl->sensor_i2c_client, s5k4ec_otp_data);

	memset(s5k4ec_diag_data, 0 ,sizeof(s5k4ec_diag_data));
	p_sh_smem_common_type = sh_smem_get_common_address();
	if(p_sh_smem_common_type != NULL){
		memcpy(&s5k4ec_diag_data[0], &p_sh_smem_common_type->sh_camOtpData[0], sizeof(s5k4ec_diag_data));
		CDBG("s5k4ec_diag_data[0]: %0x\n", s5k4ec_diag_data[0]);
		CDBG("s5k4ec_diag_data[1]: %0x\n", s5k4ec_diag_data[1]);
		CDBG("s5k4ec_diag_data[2]: %0x\n", s5k4ec_diag_data[2]);
	}

	return rc;
}

static int32_t s5k4ec_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (on) {
		rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		if (rc < 0) {
			pr_err("%s: %s power_up failed rc = %d\n", __func__,
				s_ctrl->sensordata->sensor_name, rc);
		}
	} else {
		rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

extern int shcamled_pmic_set_torch_led_1_current(unsigned mA);

int s5k4ec_pmic_set_flash_led_current(enum pmic8058_leds id, unsigned mA)
{
	int ret = 0;
	ret = shcamled_pmic_set_torch_led_1_current(mA);
	return ret;
}
EXPORT_SYMBOL(s5k4ec_pmic_set_flash_led_current);


static struct msm_sensor_id_info_t s5k4ec_id_info = {
	.sensor_id_reg_addr = REG_S5K8AAYX_MODEL_ID,
	.sensor_id = S5K8AAYX_MODEL_ID,
};


static const struct i2c_device_id s5k4ec_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k4ec_s_ctrl},
	{ }
};

static struct i2c_driver s5k4ec_i2c_driver = {
	.id_table = s5k4ec_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4ec_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k4ec_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k4ec_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = s5k4ec_sensor_power,
};

static struct v4l2_subdev_video_ops s5k4ec_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k4ec_subdev_ops = {
	.core = &s5k4ec_subdev_core_ops,
	.video  = &s5k4ec_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k4ec_func_tbl = {
	.sensor_start_stream = s5k4ec_sensor_start_stream,
	.sensor_stop_stream = s5k4ec_sensor_stop_stream,
	.sensor_group_hold_on = NULL,
	.sensor_group_hold_off = NULL,
	.sensor_set_fps = NULL,
	.sensor_write_exp_gain = NULL,
	.sensor_write_snapshot_exp_gain = NULL,
	.sensor_setting = s5k4ec_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = s5k4ec_sensor_config,
	.sensor_power_up = s5k4ec_sensor_power_up,
	.sensor_power_down = s5k4ec_sensor_power_down,
	.sensor_match_id = s5k4ec_sensor_match_id,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t s5k4ec_regs = {
	.default_data_type = MSM_CAMERA_I2C_WORD_DATA,
	.start_stream_conf = NULL,
	.start_stream_conf_size = 0,
	.stop_stream_conf = NULL,
	.stop_stream_conf_size = 0,
	.group_hold_on_conf = NULL,
	.group_hold_on_conf_size = 0,
	.group_hold_off_conf = NULL,
	.group_hold_off_conf_size = 0,
	.init_settings = NULL,
	.init_size = 0,
	.mode_settings = &s5k4ec_prev_conf[0],
	.output_settings = &s5k4ec_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k4ec_prev_conf),
};

static struct msm_sensor_ctrl_t s5k4ec_s_ctrl = {
	.msm_sensor_reg = &s5k4ec_regs,
	.sensor_i2c_client = &s5k4ec_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.sensor_output_reg_addr = NULL,
	.sensor_id_info = &s5k4ec_id_info,
	.sensor_exp_gain_info = NULL,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &s5k4ec_csi_params_array[0],
	.msm_sensor_mutex = &s5k4ec_mut,
	.sensor_i2c_driver = &s5k4ec_i2c_driver,
	.sensor_v4l2_subdev_info = s5k4ec_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4ec_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k4ec_subdev_ops,
	.func_tbl = &s5k4ec_func_tbl,
	.clk_rate = S5K8AAYX_DEFAULT_CLOCK_RATE,
};

module_init(msm_sensor_init_module);

MODULE_DESCRIPTION("S5K4EC YUV 5M sensor driver");
MODULE_LICENSE("GPL v2");
