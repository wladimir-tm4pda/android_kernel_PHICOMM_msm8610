/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*-------------------------------------------------------------------------------------------
 *when          who                     why                           comment_tag
 *-------------------------------------------------------------------------------------------
 *2014-03-12    Xinwang.Jiang       add the file
 */

#include <linux/module.h>
#include <linux/export.h>
#include "msm_led_flash.h"

/* ADP1650 Registers */
#define ADP1650_REG_VERSION			0x00
#define ADP1650_REG_TIMER_IOCFG		0x02
#define ADP1650_REG_CURRENT_SET		0x03
#define ADP1650_REG_OUTPUT_MODE		0x04
#define ADP1650_REG_FAULT			0x05
#define ADP1650_REG_CONTROL			0x06
#define ADP1650_REG_AD_MODE			0x07
#define ADP1650_REG_ADC				0x08
#define ADP1650_REG_BATT_LOW		0x09

/* ADP1650_REG_TIMER_IOCFG Bits and Masks */
#define ADP1650_IOCFG_IO2_HIGH_IMP	(0 << 6) /* High Impedance */
#define ADP1650_IOCFG_IO2_IND_LED	(1 << 6) /* Indicator LED */
#define ADP1650_IOCFG_IO2_TXMASK2	(2 << 6) /* TxMASK2 operation mode */
#define ADP1650_IOCFG_IO2_AIN		(3 << 6) /* ADC analog input */
#define ADP1650_IOCFG_IO1_HIGH_IMP	(0 << 4) /* High Impedance */
#define ADP1650_IOCFG_IO1_TORCH		(1 << 4) /* Torch mode */
#define ADP1650_IOCFG_IO1_TXMASK1	(2 << 4) /* TxMASK1 operation mode */
#define ADP1650_FL_TIMER_ms(x)		((((x) - 100) / 100) & 0xF) /* Timer */

/* ADP1650_REG_CURRENT_SET Bits and Masks */
#define ADP1650_I_FL_mA(x)		((((x) - 300) / 50) << 3)
#define ADP1650_I_TOR_mA(x)		((((x) - 25) / 25) & 0x7)

/* ADP1650_REG_OUTPUT_MODE Bits and Masks */
#define ADP1650_IL_PEAK_1A75			(0 << 6)
#define ADP1650_IL_PEAK_2A25			(1 << 6)
#define ADP1650_IL_PEAK_2A75			(2 << 6)
#define ADP1650_IL_PEAK_3A00			(3 << 6)
#define ADP1650_STR_LV_EDGE				(0 << 5)
#define ADP1650_STR_LV_LEVEL			(1 << 5)
#define ADP1650_FREQ_FB_EN				(1 << 4)
#define ADP1650_OUTPUT_EN				(1 << 3)
#define ADP1650_STR_MODE_SW				(0 << 2)
#define ADP1650_STR_MODE_HW				(1 << 2)
#define ADP1650_LED_MODE_STBY			(0 << 0)
#define ADP1650_LED_MODE_VOUT			(1 << 0)
#define ADP1650_LED_MODE_ASSIST_LIGHT	(2 << 0)
#define ADP1650_LED_MODE_FLASH			(3 << 0)

/* ADP1650_REG_FAULT Bits and Masks */
#define ADP1650_FL_OVP			(1 << 7)
#define ADP1650_FL_SC			(1 << 6)
#define ADP1650_FL_OT			(1 << 5)
#define ADP1650_FL_TO			(1 << 4)
#define ADP1650_FL_TX1			(1 << 3)
#define ADP1650_FL_IO2			(1 << 2)
#define ADP1650_FL_IL			(1 << 1)
#define ADP1650_FL_IDC			(1 << 0)

/* ADP1650_REG_CONTROL Bits and Masks */
#define ADP1650_I_TX2_mA(x)		((((x) - 100) / 50) << 4)
#define ADP1650_I_TX1_mA(x)		((((x) - 100) / 50) & 0xF)

/* ADP1650_REG_AD_MODE Bits and Masks */
#define ADP1650_DYN_OVP_EN				(1 << 7)
#define ADP1650_SW_LO_1MHz5				(1 << 6)
#define ADP1650_STR_POL_ACTIVE_HIGH		(1 << 5)
#define ADP1650_I_ILED_2mA75			(0 << 4)
#define ADP1650_I_ILED_5mA50			(1 << 4)
#define ADP1650_I_ILED_8mA25			(2 << 4)
#define ADP1650_I_ILED_11mA00			(3 << 4)
#define ADP1650_IL_DC_1A50				(0 << 1)
#define ADP1650_IL_DC_1A75				(1 << 1)
#define ADP1650_IL_DC_2A00				(2 << 1)
#define ADP1650_IL_DC_2A25				(3 << 1)
#define ADP1650_IL_DC_EN				(1 << 0)

/* ADP1650_REG_ADC Bits and Masks */
#define ADP1650_FL_VB_LO			(1 << 6)
#define ADP1650_ADC_VAL(x)			(((x) & 0x3C) >> 2)
#define ADP1650_ADC_DIS				(0 << 0)
#define ADP1650_ADC_LED_VF			(1 << 0)
#define ADP1650_ADC_DIE_TEMP		(2 << 0)
#define ADP1650_ADC_EXT_VOLT		(3 << 0)

/* ADP1650_REG_BATT_LOW Bits and Masks */
#define ADP1650_CL_SOFT_EN			(1 << 7)
#define ADP1650_I_VB_LO_mA(x)		((((x) - 300) / 50) << 3)
#define ADP1650_V_VB_LO_DIS			(0 << 0)
#define ADP1650_V_VB_LO_3V30		(1 << 0)
#define ADP1650_V_VB_LO_3V35		(2 << 0)
#define ADP1650_V_VB_LO_3V40		(3 << 0)
#define ADP1650_V_VB_LO_3V45		(4 << 0)
#define ADP1650_V_VB_LO_3V50		(5 << 0)
#define ADP1650_V_VB_LO_3V55		(6 << 0)
#define ADP1650_V_VB_LO_3V60		(7 << 0)

/* /sys/class/leds/adp1650/brightness values / mode steering */
#define FL_MODE_OFF						0 /* OFF */
#define FL_MODE_TORCH_25mA				1 /* SW trigged TORCH to FLASH */
#define FL_MODE_TORCH_50mA				2 /* TORCH Intensity XmA */
#define FL_MODE_TORCH_75mA				3
#define FL_MODE_TORCH_100mA				4
#define FL_MODE_TORCH_125mA				5
#define FL_MODE_TORCH_150mA				6
#define FL_MODE_TORCH_175mA				7
#define FL_MODE_TORCH_200mA				8
#define FL_MODE_TORCH_TRIG_EXT_25mA		9 /* HW/IO trigged TORCH to FLASH */
#define FL_MODE_TORCH_TRIG_EXT_50mA		10/* TORCH Intensity XmA */
#define FL_MODE_TORCH_TRIG_EXT_75mA		11
#define FL_MODE_TORCH_TRIG_EXT_100mA	12
#define FL_MODE_TORCH_TRIG_EXT_125mA	13
#define FL_MODE_TORCH_TRIG_EXT_150mA	14
#define FL_MODE_TORCH_TRIG_EXT_175mA	15
#define FL_MODE_TORCH_TRIG_EXT_200mA	16
#define FL_MODE_FLASH					254 /* SW triggered FLASH */
#define FL_MODE_FLASH_TRIG_EXT			255 /* HW/Strobe trigged FLASH */

struct i2c_client; /* forward declaration */

#define TIMER_IOCFG_DEFAULT (ADP1650_IOCFG_IO2_HIGH_IMP |\
			ADP1650_IOCFG_IO1_TORCH |\
			ADP1650_FL_TIMER_ms(500))

#define CURRENT_SET_DEFAULT (ADP1650_I_FL_mA(900) |\
			ADP1650_I_TOR_mA(100))

#define OUTPUT_MODE_DEFAULT (ADP1650_IL_PEAK_2A25 |\
			ADP1650_STR_LV_EDGE |\
			ADP1650_FREQ_FB_EN |\
			ADP1650_OUTPUT_EN |\
			ADP1650_STR_MODE_HW |\
			ADP1650_LED_MODE_STBY)

#define CONTROL_DEFAULT (ADP1650_I_TX2_mA(400) |\
			ADP1650_I_TX1_mA(400))


#define AD_MODE_DEFAULT (ADP1650_DYN_OVP_EN |\
			ADP1650_STR_POL_ACTIVE_HIGH |\
			ADP1650_I_ILED_2mA75 |\
			ADP1650_IL_DC_1A50 |\
			ADP1650_IL_DC_EN)

#define BATT_LOW_DEFAULT (ADP1650_CL_SOFT_EN |\
			ADP1650_I_VB_LO_mA(400) |\
			ADP1650_V_VB_LO_3V50)


#define FLASH_NAME "qcom,led-flash"

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver adp1650_i2c_driver;

static struct msm_camera_i2c_reg_array adp1650_init_array[] = {
	{ADP1650_REG_TIMER_IOCFG, TIMER_IOCFG_DEFAULT},
	{ADP1650_REG_CURRENT_SET, CURRENT_SET_DEFAULT},
	{ADP1650_REG_OUTPUT_MODE, OUTPUT_MODE_DEFAULT},
	{ADP1650_REG_CONTROL, CONTROL_DEFAULT},
	{ADP1650_REG_AD_MODE, AD_MODE_DEFAULT},
	{ADP1650_REG_BATT_LOW, BATT_LOW_DEFAULT},
	
};

static struct msm_camera_i2c_reg_array adp1650_off_array[] = {
	{ADP1650_REG_OUTPUT_MODE, 
		OUTPUT_MODE_DEFAULT & ~(ADP1650_OUTPUT_EN | ADP1650_LED_MODE_FLASH)},
};

static struct msm_camera_i2c_reg_array adp1650_release_array[] = {
	{ADP1650_REG_OUTPUT_MODE, 
		OUTPUT_MODE_DEFAULT & ~(ADP1650_OUTPUT_EN | ADP1650_LED_MODE_FLASH)},
};

static struct msm_camera_i2c_reg_array adp1650_low_array[] = {
	{ADP1650_REG_CURRENT_SET, ADP1650_I_FL_mA(375) |
			ADP1650_I_TOR_mA(100),},
	{ADP1650_REG_TIMER_IOCFG, TIMER_IOCFG_DEFAULT},
	{ADP1650_REG_OUTPUT_MODE, OUTPUT_MODE_DEFAULT | ADP1650_LED_MODE_STBY | ADP1650_STR_MODE_HW},
};

static struct msm_camera_i2c_reg_array adp1650_high_array[] = {
	{ADP1650_REG_CURRENT_SET, ADP1650_I_FL_mA(850) |
			ADP1650_I_TOR_mA(100),},
	{ADP1650_REG_TIMER_IOCFG, TIMER_IOCFG_DEFAULT},
	{ADP1650_REG_OUTPUT_MODE, OUTPUT_MODE_DEFAULT | ADP1650_LED_MODE_FLASH | ADP1650_STR_MODE_HW},
};

static void __exit msm_flash_adp1650_i2c_remove(void)
{
	i2c_del_driver(&adp1650_i2c_driver);
	return;
}

static const struct of_device_id adp1650_i2c_trigger_dt_match[] = {
	{.compatible = "qcom,led-flash"},
	{}
};

MODULE_DEVICE_TABLE(of, adp1650_i2c_trigger_dt_match);

static const struct i2c_device_id flash_i2c_id[] = {
	{"qcom,led-flash", (kernel_ulong_t)&fctrl},
	{ }
};

static const struct i2c_device_id adp1650_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

static int msm_flash_adp1650_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
		pr_err("msm_flash_adp1650_i2c_probe:!!");
	if (!id) {
		pr_err("msm_flash_adp1650_i2c_probe: id is NULL");
		id = adp1650_i2c_id;
	}

	return msm_flash_i2c_probe(client, id);
}

static struct i2c_driver adp1650_i2c_driver = {
	.id_table = adp1650_i2c_id,
	.probe  = msm_flash_adp1650_i2c_probe,
	.remove = __exit_p(msm_flash_adp1650_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = adp1650_i2c_trigger_dt_match,
	},
};

static int __init msm_flash_adp1650_i2c_add_driver(void)
{
	CDBG("%s called\n", __func__);
	return i2c_add_driver(&adp1650_i2c_driver);
}

static struct msm_camera_i2c_client adp1650_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting adp1650_init_setting = {
	.reg_setting = adp1650_init_array,
	.size = ARRAY_SIZE(adp1650_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting adp1650_off_setting = {
	.reg_setting = adp1650_off_array,
	.size = ARRAY_SIZE(adp1650_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting adp1650_release_setting = {
	.reg_setting = adp1650_release_array,
	.size = ARRAY_SIZE(adp1650_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting adp1650_low_setting = {
	.reg_setting = adp1650_low_array,
	.size = ARRAY_SIZE(adp1650_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting adp1650_high_setting = {
	.reg_setting = adp1650_high_array,
	.size = ARRAY_SIZE(adp1650_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t adp1650_regs = {
	.init_setting = &adp1650_init_setting,
	.off_setting = &adp1650_off_setting,
	.low_setting = &adp1650_low_setting,
	.high_setting = &adp1650_high_setting,
	.release_setting = &adp1650_release_setting,
};

static struct msm_flash_fn_t adp1650_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &adp1650_i2c_client,
	.reg_setting = &adp1650_regs,
	.func_tbl = &adp1650_func_tbl,
};

/*subsys_initcall(msm_flash_i2c_add_driver);*/
module_init(msm_flash_adp1650_i2c_add_driver);
module_exit(msm_flash_adp1650_i2c_remove);
MODULE_DESCRIPTION("adp1650 FLASH");
MODULE_LICENSE("GPL v2");

