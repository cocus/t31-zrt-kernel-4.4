/*
 * mt9v022 Camera Driver
 *
 * Copyright (C) 2010 Alberto Panizzo <maramaopercheseimorto@gmail.com>
 *
 * Based on ov772x, ov9640 drivers and previous non merged implementations.
 *
 * Copyright 2005-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2006, OmniVision
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-clk.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-image-sizes.h>

#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#define REG_CHIP_ID_HIGH	0x300a
#define REG_CHIP_ID_LOW		0x300b
#define CHIP_ID_HIGH		0x56
#define CHIP_ID_LOW		0x40

#define  MT9V022_DEFAULT_WIDTH    320
#define  MT9V022_DEFAULT_HEIGHT   240

/* Private v4l2 controls */
#define V4L2_CID_PRIVATE_BALANCE  (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PRIVATE_EFFECT  (V4L2_CID_PRIVATE_BASE + 1)

#define REG_TC_VFLIP			0x3820
#define REG_TC_MIRROR			0x3821
#define MT9V022_FLIP_VAL			((unsigned char)0x04)
#define MT9V022_FLIP_MASK		((unsigned char)0x04)

/* whether sensor support high resolution (> vga) preview or not */
#define SUPPORT_HIGH_RESOLUTION_PRE		1

#define MT9V022_SNAPSHOT
/*
 * Struct
 */
struct regval_list {
	u16 reg_num;
	u16 value;
};

struct mode_list {
	u16 index;
	const struct regval_list *mode_regs;
};

/* Supported resolutions */
enum mt9v022_width {
	W_720P	= MT9V022_DEFAULT_WIDTH,
};

enum mt9v022_height {
	H_720P	= MT9V022_DEFAULT_HEIGHT,
};

struct mt9v022_win_size {
	char *name;
	enum mt9v022_width width;
	enum mt9v022_height height;
	const struct regval_list *regs;
};

struct mt9v022_priv {
	struct v4l2_subdev		subdev;
	struct v4l2_ctrl_handler	hdl;
	u32	cfmt_code;
	struct v4l2_clk			*clk;
	const struct mt9v022_win_size	*win;

	int				model;
	u16				balance_value;
	u16				effect_value;
	u16				flag_vflip:1;
	u16				flag_hflip:1;

	struct soc_camera_subdev_desc	ssdd_dt;
	struct gpio_desc *resetb_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *vcc_en_gpio;

	struct regulator *reg;
	struct regulator *reg1;
};

static int mt9v022_s_power(struct v4l2_subdev *sd, int on);
static inline int sensor_i2c_master_send(struct i2c_client *client,
		const char *buf ,int count)
{
	return 0;
}

static inline int sensor_i2c_master_recv(struct i2c_client *client,
		char *buf ,int count)
{
	return 0;
}


s32 mt9v022_read_reg(struct i2c_client *client, u16 reg)
{
	return i2c_smbus_read_word_swapped(client, reg);
}

s32 mt9v022_write_reg(struct i2c_client *client, u8 reg, u16 val)
{
	return i2c_smbus_write_word_swapped(client, reg, val);
}

/*
 * Registers settings
 */

#define ENDMARKER { 0xff, 0xff }

static const struct regval_list mt9v022_init_regs[] = {
#ifndef MT9V022_SNAPSHOT
	{0x0031, 0x001F},	//V1_CONTROL
	{0x0032, 0x001A}, 	//V2_CONTROL
	{0x0033, 0x0012},	//V3_CONTROL
	{0x00AF, 0x0000},	//AUTO_BLOCK_CONTROL
	{0x002B, 0x0003},
	{0x0010, 0x0040},

	{0x000F, 0x0051}, 	//PIXEL_OPERATION_MODE
	{0x0010, 0x0040},
	{0x0015, 0x7F32},
	{0x001C, 0x0003},	    //DATA_COMPRESSION
	{0x0020, 0x01D5},
	{0x002B, 0x0000},
	{0x0048, 0x0000},     //TARGET_CALIB_VALUE
	{0x0070, 0x0004}, 	//ROW_NOISE_CONTROL
	{0x0073, 0x02F7}, 	//DARK_COL_START
	{0x00AB, 0x0000},     //GAIN_LPF_H
	{0x00BF, 0x0014}, 	//INTERLACE_VBLANK
	{0x00C2, 0x0940},

	{0x000F, 0x0011}, 	//PIXEL_OPERATION_MODE
	{0x0015, 0x7F32},
	{0x001C, 0x0002},	    //DATA_COMPRESSION
	{0x0020, 0x01D1},
	{0x00C2, 0x0840},

	{0x0001, 0x0001}, // [0:00:01.449]  REG=1,     1       	//COL_WINDOW_START_REG
	{0x0002, 0x0004}, // [0:00:01.452]  REG=2,     4       	//ROW_WINDOW_START_REG
	{0x0003, 0x01E0},	// [0:00:01.454]  REG=3,   480       	//ROW_WINDOW_SIZE_REG
//	{0x0004, 0x02F0},	// [0:00:01.456]  REG=4,   752       	//COL_WINDOW_SIZE_REG
	{0x0004, 0x0280},	// [0:00:01.456]  REG=4,   640       	//COL_WINDOW_SIZE_REG
	{0x0005, 0x005E}, // [0:00:01.457]  REG=5,    94       	//HORZ_BLANK_REG
	{0x0006, 0x002D}, // [0:00:01.458]  REG=6,    45       	//VERT_BLANK_REG
	{0x0007, 0x0388},	// [0:00:01.459]  REG=7,   904       	//CONTROL_MODE_REG
	{0x0008, 0x01BB},	// [0:00:01.460]  REG=8,   443       	//SHUTTER_WIDTH_REG_1
	{0x0009, 0x01D9},	// [0:00:01.461]  REG=9,   473       	//SHUTTER_WIDTH_REG_2
	{0x000A, 0x0164},	// [0:00:01.462]  REG=10,   356       	//SHUTTER_WIDTH_CONTROL
	{0x000B, 0x01E0},	// [0:00:01.463]  REG=11,   480       	//INTEG_TIME_REG
	{0x000C, 0x0000}, // [0:00:01.464]  REG=12,     0       	//RESET_REG
	{0x000D, 0x0304},	// [0:00:01.465]  REG=13,   768       	//READ_MODE_REG
	{0x000E, 0x0000}, // [0:00:01.466]  REG=14,     0       	//MONITOR_MODE_CONTROL
	{0x000F, 0x0011}, // [0:00:01.467]  REG=15,    17       	//PIXEL_OPERATION_MODE
	{0x0010, 0x0040}, // [0:00:01.468]  REG=16,  0x40
	{0x0011, 0x8042}, 	// [0:00:01.469]REG=17, 32834
	{0x0012, 0x0022}, // [0:00:01.470]  REG=18,    34
	{0x0013, 0x2D32}, 	// [0:00:01.471]REG=19, 0x2D32
	{0x0014, 0x0E02},	// [0:00:01.472]  REG=20,  3586
	{0x0015, 0x7F32}, 	// [0:00:01.473]REG=21, 0x7F32
	{0x0016, 0x2802}, 	// [0:00:01.474]REG=22, 10242
	{0x0017, 0x3E38}, 	// [0:00:01.475]REG=23, 15928
	{0x0018, 0x3E38}, 	// [0:00:01.476]REG=24, 15928
	{0x0019, 0x2802}, 	// [0:00:01.477]REG=25, 10242
	{0x001A, 0x0428},	// [0:00:01.478]  REG=26,  1064
	{0x001B, 0x0000}, // [0:00:01.479]  REG=27,     0       	//LED_OUT_CONTROL
	{0x001C, 0x0002}, // [0:00:01.482]  REG=28,     2       	//DATA_COMPRESSION
	{0x001D, 0x0000}, // [0:00:01.483]  REG=29,     0
	{0x001E, 0x0000}, // [0:00:01.484]  REG=30,     0
	{0x001F, 0x0000}, // [0:00:01.485]  REG=31,     0
	{0x0020, 0x01D1},	// [0:00:01.486]  REG=32,  0x1D1
	{0x0021, 0x0020}, // [0:00:01.487]  REG=33,    32
	{0x0022, 0x0020}, // [0:00:01.488]  REG=34,    32
	{0x0023, 0x0010}, // [0:00:01.489]  REG=35,    16
	{0x0024, 0x0010}, // [0:00:01.490]  REG=36,    16
	{0x0025, 0x0020}, // [0:00:01.491]  REG=37,    32
	{0x0026, 0x0010}, // [0:00:01.492]  REG=38,    16
	{0x0027, 0x0010}, // [0:00:01.493]  REG=39,    16
	{0x0028, 0x0010}, // [0:00:01.494]  REG=40,    16
	{0x0029, 0x0010}, // [0:00:01.495]  REG=41,    16
	{0x002A, 0x0020}, // [0:00:01.496]  REG=42,    32
	{0x002B, 0x0004}, // [0:00:01.497]  REG=43,     4
	{0x002C, 0x0004}, // [0:00:01.498]  REG=44,     4
	{0x002D, 0x0004}, // [0:00:01.499]  REG=45,     4
	{0x002E, 0x0007}, // [0:00:01.500]  REG=46,     7
	{0x002F, 0x0004}, // [0:00:01.501]  REG=47,     4
	{0x0030, 0x0003}, // [0:00:01.502]  REG=48,     3
	{0x0031, 0x001D}, // [0:00:01.503]  REG=49,    29 	      //V1_CONTROL
	{0x0032, 0x0018}, // [0:00:01.504]  REG=50,    24 	      //V2_CONTROL
	{0x0033, 0x0015}, // [0:00:01.505]  REG=51,    21 	      //V3_CONTROL
	{0x0034, 0x0004}, // [0:00:01.506]  REG=52,     4 	      //V4_CONTROL
	{0x0035, 0x0010}, // [0:00:01.507]  REG=53,    16 	      //GLOBAL_GAIN_REG
	{0x0036, 0x0040}, // [0:00:01.508]  REG=54,    64 	      //MAXIMUM_GAIN_REG
	{0x0037, 0x0000}, // [0:00:01.509]  REG=55,     0
	{0x0038, 0x0000}, // [0:00:01.510]  REG=56,     0
	{0x0046, 0x231D}, 	// [0:00:01.511]REG=70,  8989 	      //DARK_AVG_THRESHOLDS
	{0x0047, 0x8080}, 	// [0:00:01.512]REG=71, 32896 	      //CALIB_CONTROL_REG
	{0x004C, 0x0002}, // [0:00:01.513]  REG=76,     2 	      //STEP_SIZE_AVG_MODE
	{0x0060, 0x0000}, // [0:00:01.514]  REG=96,     0
	{0x0061, 0x0000}, // [0:00:01.515]  REG=97,     0
	{0x0062, 0x0000}, // [0:00:01.516]  REG=98,     0
	{0x0063, 0x0000}, // [0:00:01.517]  REG=99,     0
	{0x0064, 0x0000}, // [0:00:01.518]  REG=100,     0
	{0x0065, 0x0000}, // [0:00:01.519]  REG=101,     0
	{0x0066, 0x0000}, // [0:00:01.520]  REG=102,     0
	{0x0067, 0x0000}, // [0:00:01.521]  REG=103,     0
	{0x006C, 0x0000}, // [0:00:01.522]  REG=108,     0
	{0x0070, 0x0034}, // [0:00:01.523]  REG=112,    52          //ROW_NOISE_CONTROL
	{0x0071, 0x0000}, // [0:00:01.524]  REG=113,     0
	{0x0072, 0x002A}, // [0:00:01.525]  REG=114,    42          //NOISE_CONSTANT
	{0x0073, 0x02F7},	// [0:00:01.526]  REG=115,   759          //DARK_COL_START
//	{0x0074, 0x0000}, // [0:00:01.527]  REG=116,     0          //PIXCLK_CONTROL
	{0x0074, 0x0012}, // [0:00:01.527]  REG=116,     0          //PIXCLK_CONTROL
	{0x007F, 0x0000}, // [0:00:01.528]  REG=127,     0          //TEST_DATA
	{0x0080, 0x00F4}, // [0:00:01.529]  REG=128,   244          //TILE_X0_Y0
	{0x0081, 0x00F4}, // [0:00:01.530]  REG=129,   244          //TILE_X1_Y0
	{0x0082, 0x00F4}, // [0:00:01.531]  REG=130,   244          //TILE_X2_Y0
	{0x0083, 0x00F4}, // [0:00:01.532]  REG=131,   244          //TILE_X3_Y0
	{0x0084, 0x00F4}, // [0:00:01.533]  REG=132,   244          //TILE_X4_Y0
	{0x0085, 0x00F4}, // [0:00:01.534]  REG=133,   244          //TILE_X0_Y1
	{0x0086, 0x00F4}, // [0:00:01.535]  REG=134,   244          //TILE_X1_Y1
	{0x0087, 0x00F4}, // [0:00:01.536]  REG=135,   244          //TILE_X2_Y1
	{0x0088, 0x00F4}, // [0:00:01.537]  REG=136,   244          //TILE_X3_Y1
	{0x0089, 0x00F4}, // [0:00:01.538]  REG=137,   244          //TILE_X4_Y1
	{0x008A, 0x00F4}, // [0:00:01.539]  REG=138,   244          //TILE_X0_Y2
	{0x008B, 0x00F4}, // [0:00:01.540]  REG=139,   244          //TILE_X1_Y2
	{0x008C, 0x00F4}, // [0:00:01.541]  REG=140,   244          //TILE_X2_Y2
	{0x008D, 0x00F4}, // [0:00:01.542]  REG=141,   244          //TILE_X3_Y2
	{0x008E, 0x00F4}, // [0:00:01.543]  REG=142,   244          //TILE_X4_Y2
	{0x008F, 0x00F4}, // [0:00:01.544]  REG=143,   244          //TILE_X0_Y3
	{0x0090, 0x00F4}, // [0:00:01.545]  REG=144,   244          //TILE_X1_Y3
	{0x0091, 0x00F4}, // [0:00:01.546]  REG=145,   244          //TILE_X2_Y3
	{0x0092, 0x00F4}, // [0:00:01.547]  REG=146,   244          //TILE_X3_Y3
	{0x0093, 0x00F4}, // [0:00:01.548]  REG=147,   244          //TILE_X4_Y3
	{0x0094, 0x00F4}, // [0:00:01.549]  REG=148,   244          //TILE_X0_Y4
	{0x0095, 0x00F4}, // [0:00:01.550]  REG=149,   244          //TILE_X1_Y4
	{0x0096, 0x00F4}, // [0:00:01.551]  REG=150,   244          //TILE_X2_Y4
	{0x0097, 0x00F4}, // [0:00:01.552]  REG=151,   244          //TILE_X3_Y4
	{0x0098, 0x00F4}, // [0:00:01.553]  REG=152,   244          //TILE_X4_Y4
	{0x0099, 0x0000}, // [0:00:01.554]  REG=153,     0          //X0_SLASH5
	{0x009A, 0x0096}, // [0:00:01.555]  REG=154,   150          //X1_SLASH5
	{0x009B, 0x012C},	// [0:00:01.556]  REG=155,   300          //X2_SLASH5
	{0x009C, 0x01C2},	// [0:00:01.557]  REG=156,   450          //X3_SLASH5
	{0x009D, 0x0258},	// [0:00:01.558]  REG=157,   600          //X4_SLASH5
	{0x009E, 0x02F0},	// [0:00:01.559]  REG=158,   752          //X5_SLASH5
	{0x009F, 0x0000}, // [0:00:01.560]  REG=159,     0          //Y0_SLASH5
	{0x00A0, 0x0060}, // [0:00:01.561]  REG=160,    96          //Y1_SLASH5
	{0x00A1, 0x00C0}, // [0:00:01.563]  REG=161,   192          //Y2_SLASH5
	{0x00A2, 0x0120},	// [0:00:01.564]  REG=162,   288          //Y3_SLASH5
	{0x00A3, 0x0180},	// [0:00:01.565]  REG=163,   384          //Y4_SLASH5
	{0x00A4, 0x01E0},	// [0:00:01.566]  REG=164,   480          //Y5_SLASH5
	{0x00A5, 0x003A}, // [0:00:01.567]  REG=165,    58          //DESIRED_BIN
	{0x00A6, 0x0002}, // [0:00:01.568]  REG=166,     2          //EXP_SKIP_FRM
	{0x00A7, 0x0000}, // [0:00:01.570]  REG=167,     0
	{0x00A8, 0x0000}, // [0:00:01.572]  REG=168,     0          //EXP_LPF
	{0x00A9, 0x0002}, // [0:00:01.573]  REG=169,     2          //GAIN_SKIP_FRM_H
	{0x00AA, 0x0000}, // [0:00:01.574]  REG=170,     0
	{0x00AB, 0x0002}, // [0:00:01.575]  REG=171,     2          //GAIN_LPF_H
	{0x00AF, 0x0003}, // [0:00:01.576]  REG=175,     3          //AUTO_BLOCK_CONTROL
	{0x00B0, 0xABE0}, 	// [0:00:01.577]REG=176, 44000          //PIXEL_COUNT
	{0x00B1, 0x0002}, // [0:00:01.578]  REG=177,     2          //LVDS_MASTER_CONTROL
	{0x00B2, 0x0010}, // [0:00:01.579]  REG=178,    16          //SHFT_CLK_CONTROL
	{0x00B3, 0x0010}, // [0:00:01.580]  REG=179,    16          //LVDS_DATA_CONTROL
	{0x00B4, 0x0000}, // [0:00:01.581]  REG=180,     0          //STREAM_LATENCY_SELECT
	{0x00B5, 0x0000}, // [0:00:01.582]  REG=181,     0          //LVDS_INTERNAL_SYNC
	{0x00B6, 0x0000}, // [0:00:01.583]  REG=182,     0          //USE_10BIT_PIXELS
	{0x00B7, 0x0000}, // [0:00:01.584]  REG=183,     0          //STEREO_ERROR_CONTROL
	{0x00BD, 0x01E0},	// [0:00:01.585]  REG=189,   480          //MAX_EXPOSURE
	{0x00BE, 0x0014}, // [0:00:01.586]  REG=190,    20
	{0x00BF, 0x0016}, // [0:00:01.588]  REG=191,    22          //INTERLACE_VBLANK
	{0x00C0, 0x000A}, // [0:00:01.589]  REG=192,    10          //IMAGE_CAPTURE_NUM
	{0x00C2, 0x0840},	// [0:00:01.590]  REG=194,  0x840
	{0x00C3, 0x0000}, // [0:00:01.591]  REG=195,     0          //NTSC_FV_CONTROL
	{0x00C4, 0x4416}, 	// [0:00:01.592]REG=196, 17430          //NTSC_HBLANK
	{0x00C5, 0x4421}, 	// [0:00:01.593]REG=197, 17441          //NTSC_VBLANK
	{0x00F1, 0x0000}, // [0:00:01.594]  REG=241,     0          //BYTEWISE_ADDR_REG
	{0x00FE, 0xBEEF}, 	// [0:00:01.595]REG=254, 48879          //REGISTER_LOCK_REG
#else
#if 1
	{0x0031, 0x001F},	//V1_CONTROL
	{0x0032, 0x001A}, 	//V2_CONTROL
	{0x0033, 0x0012},	//V3_CONTROL
	{0x00AF, 0x0000},	//AUTO_BLOCK_CONTROL
	{0x002B, 0x0003},
	{0x0010, 0x0040},

	{0x000F, 0x0051}, 	//PIXEL_OPERATION_MODE
	{0x0010, 0x0040},
	{0x0015, 0x7F32},
	{0x001C, 0x0003},	    //DATA_COMPRESSION
//	{0x0020, 0x01D5},
	/*change*/
	{0x0020, 0x03D5},
	{0x002B, 0x0000},
	{0x0048, 0x0000},     //TARGET_CALIB_VALUE
	{0x0070, 0x0004}, 	//ROW_NOISE_CONTROL
	{0x0073, 0x02F7}, 	//DARK_COL_START
	{0x00AB, 0x0000},     //GAIN_LPF_H
	{0x00BF, 0x0014}, 	//INTERLACE_VBLANK
	{0x00C2, 0x0940},

	{0x000F, 0x0011}, 	//PIXEL_OPERATION_MODE
	{0x0015, 0x7F32},
	{0x001C, 0x0002},	    //DATA_COMPRESSION
//	{0x0020, 0x01D1},
	/*change*/
	{0x0020, 0x03D5},
	{0x00C2, 0x0840},

	{0x0001, 0x0001}, // [0:00:01.449]  REG=1,     1       	//COL_WINDOW_START_REG
	{0x0002, 0x0004}, // [0:00:01.452]  REG=2,     4       	//ROW_WINDOW_START_REG
	{0x0003, 0x01E0},	// [0:00:01.454]  REG=3,   480       	//ROW_WINDOW_SIZE_REG
//	{0x0004, 0x02F0},	// [0:00:01.456]  REG=4,   752       	//COL_WINDOW_SIZE_REG
	{0x0004, 0x0280},	// [0:00:01.456]  REG=4,   640       	//COL_WINDOW_SIZE_REG
	{0x0005, 0x005E}, // [0:00:01.457]  REG=5,    94       	//HORZ_BLANK_REG
	{0x0006, 0x002D}, // [0:00:01.458]  REG=6,    45       	//VERT_BLANK_REG
//	{0x0007, 0x0388},	// [0:00:01.459]  REG=7,   904       	//CONTROL_MODE_REG
	{0x0007, 0x0398},	// [0:00:01.459]  REG=7,   904       	//CONTROL_MODE_REG
	{0x0008, 0x01BB},	// [0:00:01.460]  REG=8,   443       	//SHUTTER_WIDTH_REG_1
	{0x0009, 0x01D9},	// [0:00:01.461]  REG=9,   473       	//SHUTTER_WIDTH_REG_2
	{0x000A, 0x0164},	// [0:00:01.462]  REG=10,   356       	//SHUTTER_WIDTH_CONTROL
	{0x000B, 0x01E0},	// [0:00:01.463]  REG=11,   480       	//INTEG_TIME_REG
	{0x000C, 0x0000}, // [0:00:01.464]  REG=12,     0       	//RESET_REG
	/*pclk reduce*/
	{0x000D, 0x0304},	// [0:00:01.465]  REG=13,   768       	//READ_MODE_REG
	{0x000E, 0x0000}, // [0:00:01.466]  REG=14,     0       	//MONITOR_MODE_CONTROL
	/*monochrome color choice*/
	{0x000F, 0x0015}, // [0:00:01.467]  REG=15,    17       	//PIXEL_OPERATION_MODE
	{0x0010, 0x0040}, // [0:00:01.468]  REG=16,  0x40
	{0x0011, 0x8042}, 	// [0:00:01.469]REG=17, 32834
	{0x0012, 0x0022}, // [0:00:01.470]  REG=18,    34
	{0x0013, 0x2D32}, 	// [0:00:01.471]REG=19, 0x2D32
	{0x0014, 0x0E02},	// [0:00:01.472]  REG=20,  3586
	{0x0015, 0x7F32}, 	// [0:00:01.473]REG=21, 0x7F32
	{0x0016, 0x2802}, 	// [0:00:01.474]REG=22, 10242
	{0x0017, 0x3E38}, 	// [0:00:01.475]REG=23, 15928
	{0x0018, 0x3E38}, 	// [0:00:01.476]REG=24, 15928
	{0x0019, 0x2802}, 	// [0:00:01.477]REG=25, 10242
	{0x001A, 0x0428},	// [0:00:01.478]  REG=26,  1064
	{0x001B, 0x0000}, // [0:00:01.479]  REG=27,     0       	//LED_OUT_CONTROL
	{0x001C, 0x0002}, // [0:00:01.482]  REG=28,     2       	//DATA_COMPRESSION
	{0x001D, 0x0000}, // [0:00:01.483]  REG=29,     0
	{0x001E, 0x0000}, // [0:00:01.484]  REG=30,     0
	{0x001F, 0x0000}, // [0:00:01.485]  REG=31,     0
//	{0x0020, 0x01D1},	// [0:00:01.486]  REG=32,  0x1D1
//	{0x0020, 0x0204},	// [0:00:01.486]  REG=32,  0x1D1
	/*change*/
	{0x0020, 0x03D5},
	{0x0021, 0x0020}, // [0:00:01.487]  REG=33,    32
	{0x0022, 0x0020}, // [0:00:01.488]  REG=34,    32
	{0x0023, 0x0010}, // [0:00:01.489]  REG=35,    16
	{0x0024, 0x0010}, // [0:00:01.490]  REG=36,    16
	{0x0025, 0x0020}, // [0:00:01.491]  REG=37,    32
	{0x0026, 0x0010}, // [0:00:01.492]  REG=38,    16
	{0x0027, 0x0010}, // [0:00:01.493]  REG=39,    16
	{0x0028, 0x0010}, // [0:00:01.494]  REG=40,    16
	{0x0029, 0x0010}, // [0:00:01.495]  REG=41,    16
	{0x002A, 0x0020}, // [0:00:01.496]  REG=42,    32
	{0x002B, 0x0004}, // [0:00:01.497]  REG=43,     4
	{0x002C, 0x0004}, // [0:00:01.498]  REG=44,     4
	{0x002D, 0x0004}, // [0:00:01.499]  REG=45,     4
	{0x002E, 0x0007}, // [0:00:01.500]  REG=46,     7
	{0x002F, 0x0004}, // [0:00:01.501]  REG=47,     4
	{0x0030, 0x0003}, // [0:00:01.502]  REG=48,     3
	{0x0031, 0x001D}, // [0:00:01.503]  REG=49,    29 	      //V1_CONTROL
	{0x0032, 0x0018}, // [0:00:01.504]  REG=50,    24 	      //V2_CONTROL
	{0x0033, 0x0015}, // [0:00:01.505]  REG=51,    21 	      //V3_CONTROL
	{0x0034, 0x0004}, // [0:00:01.506]  REG=52,     4 	      //V4_CONTROL
	{0x0035, 0x0010}, // [0:00:01.507]  REG=53,    16 	      //GLOBAL_GAIN_REG
	{0x0036, 0x0040}, // [0:00:01.508]  REG=54,    64 	      //MAXIMUM_GAIN_REG
	{0x0037, 0x0000}, // [0:00:01.509]  REG=55,     0
	{0x0038, 0x0000}, // [0:00:01.510]  REG=56,     0
	{0x0046, 0x231D}, 	// [0:00:01.511]REG=70,  8989 	      //DARK_AVG_THRESHOLDS
	{0x0047, 0x8080}, 	// [0:00:01.512]REG=71, 32896 	      //CALIB_CONTROL_REG
	{0x004C, 0x0002}, // [0:00:01.513]  REG=76,     2 	      //STEP_SIZE_AVG_MODE
	{0x0060, 0x0000}, // [0:00:01.514]  REG=96,     0
	{0x0061, 0x0000}, // [0:00:01.515]  REG=97,     0
	{0x0062, 0x0000}, // [0:00:01.516]  REG=98,     0
	{0x0063, 0x0000}, // [0:00:01.517]  REG=99,     0
	{0x0064, 0x0000}, // [0:00:01.518]  REG=100,     0
	{0x0065, 0x0000}, // [0:00:01.519]  REG=101,     0
	{0x0066, 0x0000}, // [0:00:01.520]  REG=102,     0
	{0x0067, 0x0000}, // [0:00:01.521]  REG=103,     0
	{0x006C, 0x0000}, // [0:00:01.522]  REG=108,     0
	{0x0070, 0x0034}, // [0:00:01.523]  REG=112,    52          //ROW_NOISE_CONTROL
	{0x0071, 0x0000}, // [0:00:01.524]  REG=113,     0
	{0x0072, 0x002A}, // [0:00:01.525]  REG=114,    42          //NOISE_CONSTANT
	{0x0073, 0x02F7},	// [0:00:01.526]  REG=115,   759          //DARK_COL_START
//	{0x0074, 0x0000}, // [0:00:01.527]  REG=116,     0          //PIXCLK_CONTROL
	{0x0074, 0x0012}, // [0:00:01.527]  REG=116,     0          //PIXCLK_CONTROL
	{0x007F, 0x0000}, // [0:00:01.528]  REG=127,     0          //TEST_DATA
	{0x0080, 0x00F4}, // [0:00:01.529]  REG=128,   244          //TILE_X0_Y0
	{0x0081, 0x00F4}, // [0:00:01.530]  REG=129,   244          //TILE_X1_Y0
	{0x0082, 0x00F4}, // [0:00:01.531]  REG=130,   244          //TILE_X2_Y0
	{0x0083, 0x00F4}, // [0:00:01.532]  REG=131,   244          //TILE_X3_Y0
	{0x0084, 0x00F4}, // [0:00:01.533]  REG=132,   244          //TILE_X4_Y0
	{0x0085, 0x00F4}, // [0:00:01.534]  REG=133,   244          //TILE_X0_Y1
	{0x0086, 0x00F4}, // [0:00:01.535]  REG=134,   244          //TILE_X1_Y1
	{0x0087, 0x00F4}, // [0:00:01.536]  REG=135,   244          //TILE_X2_Y1
	{0x0088, 0x00F4}, // [0:00:01.537]  REG=136,   244          //TILE_X3_Y1
	{0x0089, 0x00F4}, // [0:00:01.538]  REG=137,   244          //TILE_X4_Y1
	{0x008A, 0x00F4}, // [0:00:01.539]  REG=138,   244          //TILE_X0_Y2
	{0x008B, 0x00F4}, // [0:00:01.540]  REG=139,   244          //TILE_X1_Y2
	{0x008C, 0x00F4}, // [0:00:01.541]  REG=140,   244          //TILE_X2_Y2
	{0x008D, 0x00F4}, // [0:00:01.542]  REG=141,   244          //TILE_X3_Y2
	{0x008E, 0x00F4}, // [0:00:01.543]  REG=142,   244          //TILE_X4_Y2
	{0x008F, 0x00F4}, // [0:00:01.544]  REG=143,   244          //TILE_X0_Y3
	{0x0090, 0x00F4}, // [0:00:01.545]  REG=144,   244          //TILE_X1_Y3
	{0x0091, 0x00F4}, // [0:00:01.546]  REG=145,   244          //TILE_X2_Y3
	{0x0092, 0x00F4}, // [0:00:01.547]  REG=146,   244          //TILE_X3_Y3
	{0x0093, 0x00F4}, // [0:00:01.548]  REG=147,   244          //TILE_X4_Y3
	{0x0094, 0x00F4}, // [0:00:01.549]  REG=148,   244          //TILE_X0_Y4
	{0x0095, 0x00F4}, // [0:00:01.550]  REG=149,   244          //TILE_X1_Y4
	{0x0096, 0x00F4}, // [0:00:01.551]  REG=150,   244          //TILE_X2_Y4
	{0x0097, 0x00F4}, // [0:00:01.552]  REG=151,   244          //TILE_X3_Y4
	{0x0098, 0x00F4}, // [0:00:01.553]  REG=152,   244          //TILE_X4_Y4
	{0x0099, 0x0000}, // [0:00:01.554]  REG=153,     0          //X0_SLASH5
	{0x009A, 0x0096}, // [0:00:01.555]  REG=154,   150          //X1_SLASH5
	{0x009B, 0x012C},	// [0:00:01.556]  REG=155,   300          //X2_SLASH5
	{0x009C, 0x01C2},	// [0:00:01.557]  REG=156,   450          //X3_SLASH5
	{0x009D, 0x0258},	// [0:00:01.558]  REG=157,   600          //X4_SLASH5
	{0x009E, 0x02F0},	// [0:00:01.559]  REG=158,   752          //X5_SLASH5
	{0x009F, 0x0000}, // [0:00:01.560]  REG=159,     0          //Y0_SLASH5
	{0x00A0, 0x0060}, // [0:00:01.561]  REG=160,    96          //Y1_SLASH5
	{0x00A1, 0x00C0}, // [0:00:01.563]  REG=161,   192          //Y2_SLASH5
	{0x00A2, 0x0120},	// [0:00:01.564]  REG=162,   288          //Y3_SLASH5
	{0x00A3, 0x0180},	// [0:00:01.565]  REG=163,   384          //Y4_SLASH5
	{0x00A4, 0x01E0},	// [0:00:01.566]  REG=164,   480          //Y5_SLASH5
	{0x00A5, 0x003A}, // [0:00:01.567]  REG=165,    58          //DESIRED_BIN
	{0x00A6, 0x0002}, // [0:00:01.568]  REG=166,     2          //EXP_SKIP_FRM
	{0x00A7, 0x0000}, // [0:00:01.570]  REG=167,     0
	{0x00A8, 0x0000}, // [0:00:01.572]  REG=168,     0          //EXP_LPF
	{0x00A9, 0x0002}, // [0:00:01.573]  REG=169,     2          //GAIN_SKIP_FRM_H
	{0x00AA, 0x0000}, // [0:00:01.574]  REG=170,     0
	{0x00AB, 0x0002}, // [0:00:01.575]  REG=171,     2          //GAIN_LPF_H
//	{0x00AF, 0x0003}, // [0:00:01.576]  REG=175,     3          //AUTO_BLOCK_CONTROL
//	{0x00AF, 0x0002}, // [0:00:01.576]  REG=175,     3          //AUTO_BLOCK_CONTROL
	/* change */
	{0x00AF, 0x0000}, // [0:00:01.576]  REG=175,     3          //AUTO_BLOCK_CONTROL
	{0x00B0, 0xABE0}, 	// [0:00:01.577]REG=176, 44000          //PIXEL_COUNT
	{0x00B1, 0x0002}, // [0:00:01.578]  REG=177,     2          //LVDS_MASTER_CONTROL
	{0x00B2, 0x0010}, // [0:00:01.579]  REG=178,    16          //SHFT_CLK_CONTROL
	{0x00B3, 0x0010}, // [0:00:01.580]  REG=179,    16          //LVDS_DATA_CONTROL
	{0x00B4, 0x0000}, // [0:00:01.581]  REG=180,     0          //STREAM_LATENCY_SELECT
	{0x00B5, 0x0000}, // [0:00:01.582]  REG=181,     0          //LVDS_INTERNAL_SYNC
	{0x00B6, 0x0000}, // [0:00:01.583]  REG=182,     0          //USE_10BIT_PIXELS
	{0x00B7, 0x0000}, // [0:00:01.584]  REG=183,     0          //STEREO_ERROR_CONTROL
	{0x00BD, 0x01E0},	// [0:00:01.585]  REG=189,   480          //MAX_EXPOSURE
	{0x00BE, 0x0014}, // [0:00:01.586]  REG=190,    20
	{0x00BF, 0x0016}, // [0:00:01.588]  REG=191,    22          //INTERLACE_VBLANK
	{0x00C0, 0x000A}, // [0:00:01.589]  REG=192,    10          //IMAGE_CAPTURE_NUM
	{0x00C2, 0x0840},	// [0:00:01.590]  REG=194,  0x840
	{0x00C3, 0x0000}, // [0:00:01.591]  REG=195,     0          //NTSC_FV_CONTROL
	{0x00C4, 0x4416}, 	// [0:00:01.592]REG=196, 17430          //NTSC_HBLANK
	{0x00C5, 0x4421}, 	// [0:00:01.593]REG=197, 17441          //NTSC_VBLANK
	{0x00F1, 0x0000}, // [0:00:01.594]  REG=241,     0          //BYTEWISE_ADDR_REG
	{0x00FE, 0xBEEF}, 	// [0:00:01.595]REG=254, 48879          //REGISTER_LOCK_REG
#else
	{0x00, 0x1313}, //CHIP_VERSION_REG
	{0x01, 0x0001},	//COL_WINDOW_START_REG
	{0x02, 0x0004},	//ROW_WINDOW_START_REG

	{0x03, 0x00F0},	//ROW_WINDOW_SIZE_REG
	{0x04, 0x0140},	//COL_WINDOW_SIZE_REG

	{0x05, 0x005E},	//HORZ_BLANK_REG
	{0x06, 0x002D},	//VERT_BLANK_REG
	{0x07, 0x0398},	//CONTROL_MODE_REG
	{0x08, 0x01BB},	//SHUTTER_WIDTH_REG_1
	{0x09, 0x01D9},	//SHUTTER_WIDTH_REG_2
	{0x0A, 0x0164},	//SHUTTER_WIDTH_CONTROL
	{0x0B, 0x01E0},	//INTEG_TIME_REG
	{0x0C, 0x0000},	//RESET_REG
	{0x0D, 0x0300},	//READ_MODE_REG
	{0x0E, 0x0000},	//MONITOR_MODE_CONTROL
	{0x0F, 0x0011},	//PIXEL_OPERATION_MODE
	{0x10, 0x0040},	//RAMP_START_DELAY
	{0x11, 0x8042},	//OFFSET_CONTROL
	{0x12, 0x0022},	//AMP_RESET_BAR_CONTROL
	{0x13, 0x2D32},	//5T_PIXEL_RESET_CONTROL
	{0x14, 0x0E02},	//6T_PIXEL_RESET_CONTROL
	{0x15, 0x7F32},	//TX_CONTROL
	{0x16, 0x2802},	//5T_PIXEL_SHS_CONTROL
	{0x17, 0x3E38},	//6T_PIXEL_SHS_CONTROL
	{0x18, 0x3E38},	//5T_PIXEL_SHR_CONTROL
	{0x19, 0x2802},	//6T_PIXEL_SHR_CONTROL
	{0x1A, 0x0428},	//COMPARATOR_RESET_CONTROL
	{0x1B, 0x0000},	//LED_OUT_CONTROL
	{0x1C, 0x0003},	//DATA_COMPRESSION
	{0x1D, 0x0000},	//ANALOG_TEST_CONTROL
	{0x1E, 0x0000},	//SRAM_TEST_DATA_ODD
	{0x1F, 0x0000},	//SRAM_TEST_DATA_EVEN
	{0x20, 0x03D5},	//BOOST_ROW_EN
	{0x21, 0x0020},	//I_VLN_CONTROL
	{0x22, 0x0020},	//I_VLN_AMP_CONTROL
	{0x23, 0x0010},	//I_VLN_CMP_CONTROL
	{0x24, 0x0010},	//I_OFFSET_CONTROL
	{0x25, 0x0020},	//I_FUSE_CONTROL
	{0x26, 0x0010},	//I_VLN_VREF_ADC_CONTROL
	{0x27, 0x0010},	//I_VLN_STEP_CONTROL
	{0x28, 0x0010},	//I_VLN_BUF_CONTROL
	{0x29, 0x0010},	//I_MASTER_CONTROL
	{0x2A, 0x0020},	//I_VLN_AMP_60MHZ_CONTROL
	{0x2B, 0x0003},	//VREF_AMP_CONTROL
	{0x2C, 0x0004},	//VREF_ADC_CONTROL
	{0x2D, 0x0004},	//VBOOST_CONTROL
	{0x2E, 0x0007},	//V_HI_CONTROL
	{0x2F, 0x0004},	//V_LO_CONTROL
	{0x30, 0x0003},	//V_RST_LIM_CONTROL
	{0x31, 0x001F},	//V1_CONTROL
	{0x32, 0x001A},	//V2_CONTROL
	{0x33, 0x0012},	//V3_CONTROL
	{0x34, 0x0004},	//V4_CONTROL
	{0x35, 0x0010},	//GLOBAL_GAIN_REG
	{0x36, 0x0040},	//MAXIMUM_GAIN_REG
	{0x37, 0x0000},	//VOLTAGE_CONTROL
	{0x38, 0x0000},	//IDAC_VOLTAGE_MONITOR
	{0x42, 0x0022},	//TARGET_DARK_AVG
	{0x46, 0x231D},	//DARK_AVG_THRESHOLDS
	{0x47, 0x8080},	//CALIB_CONTROL_REG
	{0x48, 0x005C},	//TARGET_CALIB_VAL
	{0x4C, 0x0002},	//STEP_SIZE_AVG_MODE
	{0x60, 0x0000},	//READ_FUSE_ADDR
	{0x61, 0x0000},	//DEFECT_ADDRESS_1
	{0x62, 0x0000},	//DEFECT_ADDRESS_2
	{0x63, 0x0000},	//DEFECT_ADDRESS_3
	{0x64, 0x0000},	//DEFECT_ADDRESS_4
	{0x65, 0x0000},	//DEFECT_ADDRESS_5
	{0x66, 0x0000},	//DEFECT_ADDRESS_6
	{0x67, 0x0000},	//FUSE_CONTROL_REG
	{0x68, 0x51FF},	//FUSE_ID_1
	{0x69, 0xA5AE},	//FUSE_ID_2
	{0x6A, 0x1098},	//FUSE_ID_3
	{0x6B, 0x6028},	//FUSE_ID_4
	{0x6C, 0x0000},	//FUSE_RELOAD
	{0x70, 0x0034},	//ROW_NOISE_CONTROL
	{0x71, 0x0000},	//RESERVED_71
	{0x72, 0x002A},	//NOISE_CONSTANT
	{0x73, 0x02F7},	//DARK_COL_START
	{0x74, 0x0000},	//PIXCLK_CONTROL
	{0x7F, 0x0000},	//TEST_DATA
	{0x80, 0x00F4},	//TILE_X0_Y0
	{0x81, 0x00F4},	//TILE_X1_Y0
	{0x82, 0x00F4},	//TILE_X2_Y0
	{0x83, 0x00F4},	//TILE_X3_Y0
	{0x84, 0x00F4},	//TILE_X4_Y0
	{0x85, 0x00F4},	//TILE_X0_Y1
	{0x86, 0x00F4},	//TILE_X1_Y1
	{0x87, 0x00F4},	//TILE_X2_Y1
	{0x88, 0x00F4},	//TILE_X3_Y1
	{0x89, 0x00F4},	//TILE_X4_Y1
	{0x8A, 0x00F4},	//TILE_X0_Y2
	{0x8B, 0x00F4},	//TILE_X1_Y2
	{0x8C, 0x00F4},	//TILE_X2_Y2
	{0x8D, 0x00F4},	//TILE_X3_Y2
	{0x8E, 0x00F4},	//TILE_X4_Y2
	{0x8F, 0x00F4},	//TILE_X0_Y3
	{0x90, 0x00F4},	//TILE_X1_Y3
	{0x91, 0x00F4},	//TILE_X2_Y3
	{0x92, 0x00F4},	//TILE_X3_Y3
	{0x93, 0x00F4},	//TILE_X4_Y3
	{0x94, 0x00F4},	//TILE_X0_Y4
	{0x95, 0x00F4},	//TILE_X1_Y4
	{0x96, 0x00F4},	//TILE_X2_Y4
	{0x97, 0x00F4},	//TILE_X3_Y4
	{0x98, 0x00F4},	//TILE_X4_Y4
	{0x99, 0x0000},	//X0_SLASH5
	{0x9A, 0x0096},	//X1_SLASH5
	{0x9B, 0x012C},	//X2_SLASH5
	{0x9C, 0x01C2},	//X3_SLASH5
	{0x9D, 0x0258},	//X4_SLASH5
	{0x9E, 0x02F0},	//X5_SLASH5
	{0x9F, 0x0000},	//Y0_SLASH5
	{0xA0, 0x0060},	//Y1_SLASH5
	{0xA1, 0x00C0},	//Y2_SLASH5
	{0xA2, 0x0120},	//Y3_SLASH5
	{0xA3, 0x0180},	//Y4_SLASH5
	{0xA4, 0x01E0},	//Y5_SLASH5
	{0xA5, 0x003A},	//DESIRED_BIN
	{0xA6, 0x0002},	//EXP_SKIP_FRM
	{0xA7, 0x0000},	//RESERVED_A7
	{0xA8, 0x0000},	//EXP_LPF
	{0xA9, 0x0002},	//GAIN_SKIP_FRM_H
	{0xAA, 0x0000},	//RESERVED_AA
	{0xAB, 0x0002},	//GAIN_LPF_H
	{0xAF, 0x0000},	//AUTO_BLOCK_CONTROL
	{0xB0, 0xABE0},	//PIXEL_COUNT
	{0xB1, 0x0002},	//LVDS_MASTER_CONTROL
	{0xB2, 0x0010},	//SHFT_CLK_CONTROL
	{0xB3, 0x0010},	//LVDS_DATA_CONTROL
	{0xB4, 0x0000},	//STREAM_LATENCY_SELECT
	{0xB5, 0x0000},	//LVDS_INTERNAL_SYNC
	{0xB6, 0x0000},	//USE_10BIT_PIXELS
	{0xB7, 0x0000},	//STEREO_ERROR_CONTROL
	{0xB8, 0x0000},	//STEREO_ERROR_FLAG
	{0xB9, 0x0000},	//LVDS_DATA_OUTPUT
	{0xBA, 0x0010},	//AGC_GAIN
	{0xBB, 0x01E0},	//AEC_EXPOSURE
	{0xBC, 0x002A},	//CURRENT_BIN
	{0xBD, 0x01E0},	//MAX_EXPOSURE
	{0xBE, 0x0014},	//BIN_DIFF_THRESHOLD
	{0xBF, 0x0016},	//INTERLACE_VBLANK
	{0xC0, 0x000A},	//IMAGE_CAPTURE_NUM
	{0xC1, 0x024C},	//THERMAL_INFO
	{0xC2, 0x0840},	//ANALOG_CONTROLS
	{0xC3, 0x0000},	//NTSC_FV_CONTROL
	{0xC4, 0x4416},	//NTSC_HBLANK
	{0xC5, 0x4421},	//NTSC_VBLANK
	{0xF0, 0x2100},	//PAGE_REGISTER
	{0xF1, 0x0000},	//BYTEWISE_ADDR_REG
	{0xFE, 0x0000},	//REGISTER_LOCK_REG
	{0xFF, 0x0000},	//CHIP_VERSION_REG2
#endif

#endif
	ENDMARKER,
};

static const struct regval_list mt9v022_vga_itu656[] = {
	ENDMARKER,
};
static const struct regval_list mt9v022_qvga_regs[] = {

	ENDMARKER,
};

static const struct regval_list mt9v022_vga_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_720p_regs[] = {

	ENDMARKER,
};

static const struct regval_list mt9v022_1080p_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_wb_auto_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_wb_incandescence_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_wb_daylight_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_wb_fluorescent_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_wb_cloud_regs[] = {
	ENDMARKER,
};

static const struct mode_list mt9v022_balance[] = {
	{0, mt9v022_wb_auto_regs}, {1, mt9v022_wb_incandescence_regs},
	{2, mt9v022_wb_daylight_regs}, {3, mt9v022_wb_fluorescent_regs},
	{4, mt9v022_wb_cloud_regs},
};


static const struct regval_list mt9v022_effect_normal_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_effect_grayscale_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_effect_sepia_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_effect_colorinv_regs[] = {
	ENDMARKER,
};

static const struct regval_list mt9v022_effect_sepiabluel_regs[] = {
	ENDMARKER,
};

static const struct mode_list mt9v022_effect[] = {
	{0, mt9v022_effect_normal_regs}, {1, mt9v022_effect_grayscale_regs},
	{2, mt9v022_effect_sepia_regs}, {3, mt9v022_effect_colorinv_regs},
	{4, mt9v022_effect_sepiabluel_regs},
};

#define MT9V022_SIZE(n, w, h, r) \
	{.name = n, .width = w , .height = h, .regs = r }

static struct mt9v022_win_size mt9v022_supported_win_sizes[] = {
	MT9V022_SIZE("720P", W_720P, H_720P, mt9v022_720p_regs),
};

#define N_WIN_SIZES (ARRAY_SIZE(mt9v022_supported_win_sizes))


static u32 mt9v022_codes[] = {
	MEDIA_BUS_FMT_Y8_1X8,
//	V4L2_MBUS_FMT_Y8_1X8
};

/*
 * Supported balance menus
 */
static const struct v4l2_querymenu mt9v022_balance_menus[] = {
	{
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 0,
		.name		= "auto",
	}, {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 1,
		.name		= "incandescent",
	}, {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 2,
		.name		= "fluorescent",
	},  {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 3,
		.name		= "daylight",
	},  {
		.id		= V4L2_CID_PRIVATE_BALANCE,
		.index		= 4,
		.name		= "cloudy-daylight",
	},

};

/*
 * Supported effect menus
 */
static const struct v4l2_querymenu mt9v022_effect_menus[] = {
	{
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 0,
		.name		= "none",
	}, {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 1,
		.name		= "mono",
	}, {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 2,
		.name		= "sepia",
	},  {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 3,
		.name		= "negative",
	}, {
		.id		= V4L2_CID_PRIVATE_EFFECT,
		.index		= 4,
		.name		= "aqua",
	},
};


/*
 * General functions
 */
static struct mt9v022_priv *to_mt9v022(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct mt9v022_priv,
			    subdev);
}

static int mt9v022_write_array(struct i2c_client *client,
			      const struct regval_list *vals)
{
	int ret;

	while ((vals->reg_num != 0xff) || (vals->value != 0xff)) {

		ret = mt9v022_write_reg(client, vals->reg_num, vals->value);
		dev_vdbg(&client->dev, "array: 0x%02x, 0x%02x",
			 vals->reg_num, vals->value);

		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int mt9v022_mask_set(struct i2c_client *client,
			   u16  reg, u16  mask, u16  set)
{
	s32 val = mt9v022_read_reg(client, reg);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	dev_vdbg(&client->dev, "masks: 0x%02x, 0x%02x", reg, val);

	return mt9v022_write_reg(client, reg, val);
}

static int mt9v022_reset(struct i2c_client *client)
{
	return 0;
}

/*
 * soc_camera_ops functions
 */
static int mt9v022_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int mt9v022_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct mt9v022_priv, hdl)->subdev;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct mt9v022_priv *priv = to_mt9v022(client);

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		ctrl->val = priv->flag_vflip;
		break;
	case V4L2_CID_HFLIP:
		ctrl->val = priv->flag_hflip;
		break;
	case V4L2_CID_PRIVATE_BALANCE:
		ctrl->val = priv->balance_value;
		break;
	case V4L2_CID_PRIVATE_EFFECT:
		ctrl->val = priv->effect_value;
		break;
	default:
		break;
	}
	return 0;
}

static int mt9v022_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct mt9v022_priv, hdl)->subdev;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct mt9v022_priv *priv = to_mt9v022(client);
	int ret = 0;
	int i = 0;
	u16 value;

	int balance_count = ARRAY_SIZE(mt9v022_balance);
	int effect_count = ARRAY_SIZE(mt9v022_effect);

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_BALANCE:
		if(ctrl->val > balance_count)
			return -EINVAL;

		for(i = 0; i < balance_count; i++) {
			if(ctrl->val == mt9v022_balance[i].index) {
				ret = mt9v022_write_array(client,
						mt9v022_balance[ctrl->val].mode_regs);
				priv->balance_value = ctrl->val;
				break;
			}
		}
		break;

	case V4L2_CID_PRIVATE_EFFECT:
		if(ctrl->val > effect_count)
			return -EINVAL;

		for(i = 0; i < effect_count; i++) {
			if(ctrl->val == mt9v022_effect[i].index) {
				ret = mt9v022_write_array(client,
						mt9v022_effect[ctrl->val].mode_regs);
				priv->effect_value = ctrl->val;
				break;
			}
		}
		break;

	case V4L2_CID_VFLIP:
		value = ctrl->val ? MT9V022_FLIP_VAL : 0x00;
		priv->flag_vflip = ctrl->val ? 1 : 0;
		ret = mt9v022_mask_set(client, REG_TC_VFLIP, MT9V022_FLIP_MASK, value);
		break;

	case V4L2_CID_HFLIP:
		value = ctrl->val ? MT9V022_FLIP_VAL : 0x00;
		priv->flag_hflip = ctrl->val ? 1 : 0;
		ret = mt9v022_mask_set(client, REG_TC_MIRROR, MT9V022_FLIP_MASK, value);
		break;

	default:
		dev_err(&client->dev, "no V4L2 CID: 0x%x ", ctrl->id);
		return -EINVAL;
	}

	return ret;
}

static int mt9v022_querymenu(struct v4l2_subdev *sd,
					struct v4l2_querymenu *qm)
{
	switch (qm->id) {
	case V4L2_CID_PRIVATE_BALANCE:
		memcpy(qm->name, mt9v022_balance_menus[qm->index].name,
				sizeof(qm->name));
		break;

	case V4L2_CID_PRIVATE_EFFECT:
		memcpy(qm->name, mt9v022_effect_menus[qm->index].name,
				sizeof(qm->name));
		break;
	}

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9v022_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	reg->size = 1;
	if (reg->reg > 0xff)
		return -EINVAL;

	ret = mt9v022_read_reg(client, reg->reg);
	if (ret < 0)
		return ret;

	reg->val = ret;

	return 0;
}

static int mt9v022_s_register(struct v4l2_subdev *sd,
			     const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg > 0xff ||
	    reg->val > 0xff)
		return -EINVAL;

	return mt9v022_write_reg(client, reg->reg, reg->val);
}
#endif

/* Select the nearest higher resolution for capture */
static const struct mt9v022_win_size *mt9v022_select_win(u32 *width, u32 *height)
{
	int i, default_size = ARRAY_SIZE(mt9v022_supported_win_sizes) - 1;

	for (i = 0; i < ARRAY_SIZE(mt9v022_supported_win_sizes); i++) {
		if ((*width >= mt9v022_supported_win_sizes[i].width) &&
		    (*height >= mt9v022_supported_win_sizes[i].height)) {
			*width = mt9v022_supported_win_sizes[i].width;
			*height = mt9v022_supported_win_sizes[i].height;
			return &mt9v022_supported_win_sizes[i];
		}
	}

	*width = mt9v022_supported_win_sizes[default_size].width;
	*height = mt9v022_supported_win_sizes[default_size].height;
	return &mt9v022_supported_win_sizes[default_size];
}

static int mt9v022_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct mt9v022_priv *priv = to_mt9v022(client);

	if (format->pad)
		return -EINVAL;

	mf->width = MT9V022_DEFAULT_WIDTH;//priv->win->width;
	mf->height = MT9V022_DEFAULT_HEIGHT;//priv->win->height;
	mf->code = priv->cfmt_code;

	mf->colorspace = V4L2_COLORSPACE_SRGB;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int mt9v022_set_params(struct i2c_client *client, u32 *width, u32 *height, u32 code)
{
	struct mt9v022_priv       *priv = to_mt9v022(client);
	int ret;

	int bala_index = priv->balance_value;
	int effe_index = priv->effect_value;

	/* select win */
	priv->win = mt9v022_select_win(width, height);

	/* select format */
	priv->cfmt_code = 0;

	/* reset hardware */
	mt9v022_reset(client);

	/* initialize the sensor with default data */
	dev_dbg(&client->dev, "%s: Init default", __func__);
	ret = mt9v022_write_array(client, mt9v022_init_regs);
	if (ret < 0)
		goto err;

	/* set balance */
	ret = mt9v022_write_array(client, mt9v022_balance[bala_index].mode_regs);
	if (ret < 0)
		goto err;

	/* set effect */
	ret = mt9v022_write_array(client, mt9v022_effect[effe_index].mode_regs);
	if (ret < 0)
		goto err;

	/* set size win */
	ret = mt9v022_write_array(client, priv->win->regs);
	if (ret < 0)
		goto err;

	priv->cfmt_code = code;
	*width = priv->win->width;
	*height = priv->win->height;

	return 0;

err:
	dev_err(&client->dev, "%s: Error %d", __func__, ret);
	mt9v022_reset(client);
	priv->win = NULL;

	return ret;
}

static int mt9v022_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (format->pad)
		return -EINVAL;

	/*
	 * select suitable win, but don't store it
	 */
	mt9v022_select_win(&mf->width, &mf->height);

	mf->field	= V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		return mt9v022_set_params(client, &mf->width,
					 &mf->height, mf->code);
	cfg->try_fmt = *mf;
	return 0;
}

static int mt9v022_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(mt9v022_codes))
		return -EINVAL;

	code->code = mt9v022_codes[code->index];
	return 0;
}

static int mt9v022_enum_frame_size(struct v4l2_subdev *sd,
		       struct v4l2_subdev_pad_config *cfg,
		       struct v4l2_subdev_frame_size_enum *fse)
{
	int i, j;
	int num_valid = -1;
	__u32 index = fse->index;

	if(index >= N_WIN_SIZES)
		return -EINVAL;

	j = ARRAY_SIZE(mt9v022_codes);
	while(--j)
		if(fse->code == mt9v022_codes[j])
			break;

	for (i = 0; i < N_WIN_SIZES; i++) {
		if (index == ++num_valid) {
			fse->code = mt9v022_codes[j];
			fse->min_width = mt9v022_supported_win_sizes[index].width;
			fse->max_width = fse->min_width;
			fse->min_height = mt9v022_supported_win_sizes[index].height;
			fse->max_height = fse->min_height;
			return 0;
		}
	}

	return -EINVAL;
}

static int mt9v022_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MT9V022_DEFAULT_WIDTH;
	a->c.height	= MT9V022_DEFAULT_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int mt9v022_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= MT9V022_DEFAULT_WIDTH;
	a->bounds.height		= MT9V022_DEFAULT_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int mt9v022_video_probe(struct i2c_client *client)
{
	struct mt9v022_priv *priv = to_mt9v022(client);
	int ret;
	int data = 0;

	ret = mt9v022_s_power(&priv->subdev, 1);
	if (ret < 0)
		return ret;
	/* Read out the chip version register */
	data = mt9v022_read_reg(client, 0x0000);

	/* must be 0x1311, 0x1313 or 0x1324 */
	if (data != 0x1311 && data != 0x1313 && data != 0x1324) {
		ret = -ENODEV;
		dev_info(&client->dev, "No MT9V022 found, ID register 0x%x\n",
				data);
		return ret;  }
	dev_info(&client->dev, "Detected a MT9V022 chip ID %x\n", data);


	/* Soft reset */
	ret = mt9v022_write_reg(client, 0x000c, 1);

	/*******/
	ret = v4l2_ctrl_handler_setup(&priv->hdl);

	mt9v022_s_power(&priv->subdev, 0);
	return ret;
}

static int mt9v022_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct mt9v022_priv *priv = to_mt9v022(client);

	return soc_camera_set_power(&client->dev, ssdd, priv->clk, on);
}

static const struct v4l2_ctrl_ops mt9v022_ctrl_ops = {
	.s_ctrl = mt9v022_s_ctrl,
	.g_volatile_ctrl = mt9v022_g_ctrl,
};

static struct v4l2_subdev_core_ops mt9v022_subdev_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9v022_g_register,
	.s_register	= mt9v022_s_register,
#endif
	.s_power	= mt9v022_s_power,
	.querymenu	= mt9v022_querymenu,
};

static int mt9v022_g_mbus_config(struct v4l2_subdev *sd,
		struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);

	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_FALLING | V4L2_MBUS_MASTER |
		V4L2_MBUS_VSYNC_ACTIVE_LOW | V4L2_MBUS_HSYNC_ACTIVE_HIGH |
		V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;

	cfg->flags = soc_camera_apply_board_flags(ssdd, cfg);

	return 0;
}

static struct v4l2_subdev_video_ops mt9v022_subdev_video_ops = {
	.s_stream	= mt9v022_s_stream,
	.cropcap	= mt9v022_cropcap,
	.g_crop		= mt9v022_g_crop,
	.g_mbus_config	= mt9v022_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops mt9v022_subdev_pad_ops = {
	.enum_mbus_code = mt9v022_enum_mbus_code,
	.enum_frame_size = mt9v022_enum_frame_size,
	.get_fmt	= mt9v022_get_fmt,
	.set_fmt	= mt9v022_set_fmt,
};

static struct v4l2_subdev_ops mt9v022_subdev_ops = {
	.core	= &mt9v022_subdev_core_ops,
	.video	= &mt9v022_subdev_video_ops,
	.pad	= &mt9v022_subdev_pad_ops,
};

/* OF probe functions */
static int mt9v022_hw_power(struct device *dev, int on)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mt9v022_priv *priv = to_mt9v022(client);

	dev_dbg(&client->dev, "%s: %s the camera\n",
			__func__, on ? "ENABLE" : "DISABLE");

	/* thses gpio should be set according to the active level in dt defines */
	if(priv->vcc_en_gpio) {
		gpiod_direction_output(priv->vcc_en_gpio, on);
	}

	if (priv->pwdn_gpio) {
		gpiod_direction_output(priv->pwdn_gpio, !on);
	}

	msleep(10);
	return 0;
}

static int mt9v022_hw_reset(struct device *dev)
{
	return 0;
}


static int mt9v022_probe_dt(struct i2c_client *client,
		struct mt9v022_priv *priv)
{

	struct soc_camera_subdev_desc	*ssdd_dt = &priv->ssdd_dt;
	struct v4l2_subdev_platform_data *sd_pdata = &ssdd_dt->sd_pdata;
	struct device_node *np = client->dev.of_node;
	int supplies = 0, index = 0;

	supplies = of_property_count_strings(np, "supplies-name");
	if(supplies <= 0) {
		goto no_supply;
	}

	sd_pdata->num_regulators = supplies;
	sd_pdata->regulators = devm_kzalloc(&client->dev, supplies * sizeof(struct regulator_bulk_data), GFP_KERNEL);
	if(!sd_pdata->regulators) {
		dev_err(&client->dev, "Failed to allocate regulators.!\n");
		goto no_supply;
	}

	for(index = 0; index < sd_pdata->num_regulators; index ++) {
		of_property_read_string_index(np, "supplies-name", index,
				&(sd_pdata->regulators[index].supply));

		dev_dbg(&client->dev, "sd_pdata->regulators[%d].supply: %s\n",
				index, sd_pdata->regulators[index].supply);
	}

	soc_camera_power_init(&client->dev, ssdd_dt);

no_supply:

	/* Request the power down GPIO asserted */
	priv->pwdn_gpio = devm_gpiod_get_optional(&client->dev, "pwdn",
			GPIOD_OUT_HIGH);
	if (!priv->pwdn_gpio)
		dev_dbg(&client->dev, "pwdn gpio is not assigned!\n");
	else if (IS_ERR(priv->pwdn_gpio))
		return PTR_ERR(priv->pwdn_gpio);

	/* Request the power down GPIO asserted */
	priv->vcc_en_gpio = devm_gpiod_get_optional(&client->dev, "vcc-en",
			GPIOD_OUT_HIGH);
	if (!priv->vcc_en_gpio)
		dev_dbg(&client->dev, "vcc_en gpio is not assigned!\n");
	else if (IS_ERR(priv->vcc_en_gpio))
		return PTR_ERR(priv->vcc_en_gpio);

	/* Initialize the soc_camera_subdev_desc */
	priv->ssdd_dt.power = mt9v022_hw_power;
	priv->ssdd_dt.reset = mt9v022_hw_reset;
	client->dev.platform_data = &priv->ssdd_dt;
	return 0;
}

#include <linux/regulator/consumer.h>
/*
 * i2c_driver functions
 */
static int mt9v022_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct mt9v022_priv	*priv;
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);
	int	ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,
			"mt9v022: I2C-Adapter doesn't support SMBUS\n");
		return -EIO;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct mt9v022_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&adapter->dev,
			"Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	priv->clk = v4l2_clk_get(&client->dev, "cgu_cim");
	if (IS_ERR(priv->clk))
		return -EPROBE_DEFER;

	v4l2_clk_set_rate(priv->clk, 24000000);

	if (!ssdd && !client->dev.of_node) {
		dev_err(&client->dev, "Missing platform_data for driver\n");
		ret = -EINVAL;
		goto err_videoprobe;
	}

	if (!ssdd) {
		ret = mt9v022_probe_dt(client, priv);
		if (ret)
			goto err_clk;
	}


	v4l2_i2c_subdev_init(&priv->subdev, client, &mt9v022_subdev_ops);

	/* add handler */
	v4l2_ctrl_handler_init(&priv->hdl, 2);
	v4l2_ctrl_new_std(&priv->hdl, &mt9v022_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &mt9v022_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		ret = priv->hdl.error;
		goto err_clk;
	}

	ret = mt9v022_video_probe(client);
	if (ret < 0)
		goto err_videoprobe;

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0)
		goto err_videoprobe;

	dev_info(&adapter->dev, "mt9v022 Probed\n");

	return 0;

err_videoprobe:
	v4l2_ctrl_handler_free(&priv->hdl);
err_clk:
	v4l2_clk_put(priv->clk);
	return ret;
}

static int mt9v022_remove(struct i2c_client *client)
{
	struct mt9v022_priv       *priv = to_mt9v022(client);

	v4l2_async_unregister_subdev(&priv->subdev);
	v4l2_clk_put(priv->clk);
	v4l2_device_unregister_subdev(&priv->subdev);
	v4l2_ctrl_handler_free(&priv->hdl);
	return 0;
}

static const struct i2c_device_id mt9v022_id[] = {
	{ "mt9v022",  0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9v022_id);
static const struct of_device_id mt9v022_of_match[] = {
	{.compatible = "micron,mt9v022", },
	{},
};
MODULE_DEVICE_TABLE(of, mt9v022_of_match);
static struct i2c_driver mt9v022_i2c_driver = {
	.driver = {
		.name = "mt9v022",
		.of_match_table = of_match_ptr(mt9v022_of_match),
	},
	.probe    = mt9v022_probe,
	.remove   = mt9v022_remove,
	.id_table = mt9v022_id,
};
module_i2c_driver(mt9v022_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for micron mt9v022 sensor");
MODULE_AUTHOR("Alberto Panizzo");
MODULE_LICENSE("GPL v2");
