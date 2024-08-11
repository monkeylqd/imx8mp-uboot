/*
 * Copyright (C) 2013 Guangzhou Tronlong Electronic Technology Co., Ltd. - www.tronlong.com
 *
 * @file lt8912b.c
 *
 * @brief lt8912b configure driver
 * This driver will reset ,setup and enable lt8912b
 *
 * @author Tronlong <support@tronlong.com>
 *
 * @version V1.0
 *
 * @date 2023-05-18
 *
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <asm/gpio.h>
#include <asm/arch-imx8m/imx8mp_pins.h>
#include <i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/kernel.h>

/* output format */
#define OUTPUT_1080P_60Hz
/* #define OUTPUT_720P_60Hz */

/* lvds bypass */
#define LVDS_BYPASS

/* mipi input 3210 swap */
/* #define LANE_SWAP */

/* mipi input lane pn swap */
/* #define PN_SWAP */

/*
 * lt8912b have three i2c module inside.
 * lt8912b i2c address: [0x48, 0x49, 0x4a]
 */
#define I2C_BASS_ADDRESS1	0x48
#define I2C_BASS_ADDRESS2	0x49
#define I2C_BASS_ADDRESS3	0x4a


struct lt8912b_priv {
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	struct udevice *dev_addr1;
	struct udevice *dev_addr2;
	struct udevice *dev_addr3;
	struct gpio_desc reset_gpio;
};


struct video_timing {
	unsigned short hfp;
	unsigned short hs;
	unsigned short hbp;
	unsigned short hact;
	unsigned short htotal;
	unsigned short vfp;
	unsigned short vs;
	unsigned short vbp;
	unsigned short vact;
	unsigned short vtotal;
	unsigned int pclk_khz;
};


struct panel_parameter {
	unsigned short hfp;
	unsigned short hs;
	unsigned short hbp;
	unsigned short hact;
	unsigned short htotal;
	unsigned short vfp;
	unsigned short vs;
	unsigned short vbp;
	unsigned short vact;
	unsigned short vtotal;
	unsigned int pclk_khz;
};

struct lt8912b_reg {
	unsigned char reg;
	unsigned char data;
};


static const struct display_timing default_timing = {
#ifdef OUTPUT_1080P_60Hz
	.pixelclock.typ		= 148500000,
	.hactive.typ		= 1920,
	.hfront_porch.typ	= 88,
	.hback_porch.typ	= 148,
	.hsync_len.typ		= 44,
	.vactive.typ		= 1080,
	.vfront_porch.typ	= 4,
	.vback_porch.typ	= 36,
	.vsync_len.typ		= 5,
#endif

#ifdef OUTPUT_720P_60Hz
	.pixelclock.typ		= 74250000,
	.hactive.typ		= 1280,
	.hfront_porch.typ	= 112,
	.hback_porch.typ	= 220,
	.hsync_len.typ		= 40,
	.vactive.typ		= 720,
	.vfront_porch.typ	= 5,
	.vback_porch.typ	= 20,
	.vsync_len.typ		= 5,
#endif
};



static struct lt8912b_priv lt8912b_priv;
static unsigned char g_i2c_addr = I2C_BASS_ADDRESS1;


/*
 * this timing is mipi timing, please set these timing paremeter same with
 * actual mipi timing(processor's timing)
 * hfp, hs, hbp,hact,htotal,vfp, vs, vbp,vact,vtotal
 */
/* static struct video_timing video_640x480_60Hz = { 8, 96, 40, 640, 784, 33, 2, 10, 480, 525}; */
/* static struct video_timing video_720x480_60Hz = {16, 62, 60, 720, 858, 9, 6, 30, 480, 525}; */
static struct video_timing video_1280x720_60Hz = {110,40, 220, 1280, 1650, 5, 5, 20, 720, 750};
/* static struct video_timing video_1366x768_60Hz = {14, 56, 64, 1366, 1500, 1, 3, 28, 768, 800}; */
static struct video_timing video_1920x1080_60Hz = {88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125};
/* static struct video_timing video_3840x1080_60Hz = {176,88, 296, 3840, 4400, 4, 5, 36, 1080, 1125}; */
/* static struct video_timing video_3840x2160_30Hz = {176,88, 296,3840, 4400, 8, 10, 72, 2160, 2250}; */
/* static struct video_timing video_1024x768_60Hz = {24, 136, 160, 1024, 1344, 3, 6, 29, 768, 806, 65000}; */
static struct video_timing video_1280x800_60Hz = {64, 136, 200, 1280, 1680, 1, 3, 24, 800, 828, 74250};
static struct video_timing video_800x1280_60Hz = {80, 20, 20, 800, 920, 15, 6, 8, 1280, 1309, 67200};


/*
 * panel timing
 * this timing is used for scaler output for LVDS,
 * HDMI output and lvds bypass mode will not use this timing.
 * hfp, hs, hbp,hact,htotal,vfp, vs, vbp,vact,vtotal,
 */
static struct video_timing video_1024x600_60Hz = {50, 20, 50, 1024, 1144, 9, 3, 13, 600, 625, 42500};
static struct video_timing video_lvds_60Hz = {24, 136, 160, 1024, 1344, 3, 6, 29, 768, 806, 65000};


static struct lt8912b_reg digital_clock_enable[] = {
	{0x02, 0xf7}, {0x08, 0xff}, {0x09, 0xff},
	{0x0a, 0xff}, {0x0b, 0x7c}, {0x0c, 0xff},
};

static struct lt8912b_reg tx_analog[] = {
	{0x31, 0xE1}, {0x32, 0xE1}, {0x33, 0x0c},
	{0x37, 0x00}, {0x38, 0x22}, {0x60, 0x82},
};

static struct lt8912b_reg cbus_analog[] = {
	{0x39, 0x45}, {0x3a, 0x00}, {0x3b, 0x00},
};

static struct lt8912b_reg hmdi_pll_analog[] = {
	{0x44, 0x31}, {0x55, 0x44}, {0x57, 0x01},
	{0x5a, 0x02},
};

static struct lt8912b_reg dds_config[] = {
	{0x4e, 0xaa}, {0x4f, 0xaa}, {0x50, 0x6a},
	{0x51, 0x80}, {0x1e, 0x4f}, {0x1f, 0x5e},
	{0x20, 0x01}, {0x21, 0x2c}, {0x22, 0x01},
	{0x23, 0xfa}, {0x24, 0x00}, {0x25, 0xc8},
	{0x26, 0x00}, {0x27, 0x5e}, {0x28, 0x01},
	{0x29, 0x2c}, {0x2a, 0x01}, {0x2b, 0xfa},
	{0x2c, 0x00}, {0x2d, 0xc8}, {0x2e, 0x00},
	{0x42, 0x64}, {0x43, 0x00}, {0x44, 0x04},
	{0x45, 0x00}, {0x46, 0x59}, {0x47, 0x00},
	{0x48, 0xf2}, {0x49, 0x06}, {0x4a, 0x00},
	{0x4b, 0x72}, {0x4c, 0x45}, {0x4d, 0x00},
	{0x52, 0x08}, {0x53, 0x00}, {0x54, 0xb2},
	{0x55, 0x00}, {0x56, 0xe4}, {0x57, 0x0d},
	{0x58, 0x00}, {0x59, 0xe4}, {0x5a, 0x8a},
	{0x5b, 0x00}, {0x5c, 0x34}, {0x51, 0x00},
};

static struct lt8912b_reg lvds_bypass[] = {
	{0x50, 0x24}, {0x51, 0x2d}, {0x52, 0x04},
	{0x69, 0x0e}, {0x69, 0x8e}, {0x6a, 0x00},
	{0x6c, 0xb8}, {0x6b, 0x51}, {0x04, 0xfb},
	{0x04, 0xff}, {0x7f, 0x00}, {0xa8, 0x13},
};

static struct lt8912b_reg lvds_output_on[] = {
	{0x02, 0xf7}, {0x02, 0xff}, {0x03, 0xcb},
	{0x03, 0xfb}, {0x03, 0xff}, {0x44, 0x30},
};

static int i2c_dev_write(unsigned char addr, unsigned char data)
{
	struct lt8912b_priv *priv = &lt8912b_priv;
	int err;

	switch(g_i2c_addr) {
	case I2C_BASS_ADDRESS1:
		err = dm_i2c_write(priv->dev_addr1, addr, &data, 1);
		break;
	case I2C_BASS_ADDRESS2:
		err = dm_i2c_write(priv->dev_addr2, addr, &data, 1);
		break;
	case I2C_BASS_ADDRESS3:
		err = dm_i2c_write(priv->dev_addr3, addr, &data, 1);
		break;
	default:
		return -1;
	}

	return err;
}

static int i2c_dev_read(unsigned char addr, unsigned char *data)
{
	struct lt8912b_priv *priv = &lt8912b_priv;
	unsigned char val;
	int err;

	switch(g_i2c_addr) {
	case I2C_BASS_ADDRESS1:
		err = dm_i2c_read(priv->dev_addr1, addr, &val, 1);
		break;
	case I2C_BASS_ADDRESS2:
		err = dm_i2c_read(priv->dev_addr2, addr, &val, 1);
		break;
	case I2C_BASS_ADDRESS3:
		err = dm_i2c_read(priv->dev_addr3, addr, &val, 1);
		break;
	default:
		return -1;
	}
	*data = val;

	return err;
}

static void lt8912b_delay(unsigned long ms)
{
	mdelay(ms);
}

static int lt8912b_write_byte(unsigned char addr, unsigned char data)
{
	int flag;

	flag = i2c_dev_write(addr, data);
	lt8912b_delay(1);
	return flag;
}

static unsigned char lt8912b_read_byte(unsigned char addr)
{
	unsigned char p_data = 0;

	if(i2c_dev_read(addr, &p_data) == 0)
		return p_data;

	return 0;
}

static void lt8912b_write_regs(struct lt8912b_reg *regs, unsigned int regs_num)
{
	int i;

	debug("regs num %d\n", regs_num);

	for(i = 0; i < regs_num; i++) {
		debug("write %x %x\n", regs[i].reg, regs[i].data);
		lt8912b_write_byte(regs[i].reg, regs[i].data);
	}
}

static void lt8912b_digital_clock_enable(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_regs(digital_clock_enable, ARRAY_SIZE(digital_clock_enable));
}

static void lt8912b_tx_analog(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_regs(tx_analog, ARRAY_SIZE(tx_analog));
}

static void lt8912b_cbus_analog(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_regs(cbus_analog, ARRAY_SIZE(cbus_analog));
}

static void lt8912b_hmdi_pll_analog(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_regs(hmdi_pll_analog, ARRAY_SIZE(hmdi_pll_analog));
}

static void lt8912b_avi_info_frame(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS3;
	/* enable null package */
	lt8912b_write_byte(0x3c, 0x41);

	/* defualt AVI */
	g_i2c_addr = I2C_BASS_ADDRESS1;
	/* sync polarity + */
	lt8912b_write_byte(0xab, 0x03);

	g_i2c_addr = I2C_BASS_ADDRESS3;
	/* PB0:check sum */
	lt8912b_write_byte(0x43, 0x27);
	/* PB1 */
	lt8912b_write_byte(0x44, 0x10);
	/* PB2 */
	lt8912b_write_byte(0x45, 0x28);
	/* PB3 */
	lt8912b_write_byte(0x46, 0x00);
	/* PB4:vic */
	lt8912b_write_byte(0x47, 0x10);

#ifdef OUTPUT_1080P_60Hz
	/* 1080P60Hz 16:9 */
	g_i2c_addr = I2C_BASS_ADDRESS1;
	/* sync polarity + */
	lt8912b_write_byte(0xab, 0x03);

	g_i2c_addr = I2C_BASS_ADDRESS3;
	/* PB0:check sum */
	lt8912b_write_byte(0x43, 0x27);
	/* PB1 */
	lt8912b_write_byte(0x44, 0x10);
	/* PB2 */
	lt8912b_write_byte(0x45, 0x28);
	/* PB3 */
	lt8912b_write_byte(0x46, 0x00);
	/* PB4:vic */
	lt8912b_write_byte(0x47, 0x10);
#endif


#ifdef OUTPUT_720P_60Hz
	/* 720P60Hz 16:9 */
	g_i2c_addr = I2C_BASS_ADDRESS1;
	/* sync polarity + */
	lt8912b_write_byte(0xab, 0x03);

	g_i2c_addr = I2C_BASS_ADDRESS3;
	/* PB0:check sum */
	lt8912b_write_byte(0x43, 0x33);
	/* PB1 */
	lt8912b_write_byte(0x44, 0x10);
	/* PB2 */
	lt8912b_write_byte(0x45, 0x28);
	/* PB3 */
	lt8912b_write_byte(0x46, 0x00);
	/* PB4:vic */
	lt8912b_write_byte(0x47, 0x04);
#endif


#ifdef OUTPUT_480P_60Hz
	/* 720x480 60Hz 4:3 */
	g_i2c_addr = I2C_BASS_ADDRESS1;
	/* sync polarity + */
	lt8912b_write_byte(0xab, 0x0c);

	g_i2c_addr = I2C_BASS_ADDRESS3;
	/* PB0:check sum */
	lt8912b_write_byte(0x43, 0x45);
	/* PB1 */
	lt8912b_write_byte(0x44, 0x10);
	/* PB2 */
	lt8912b_write_byte(0x45, 0x18);
	/* PB3 */
	lt8912b_write_byte(0x46, 0x00);
	/* PB4:vic */
	lt8912b_write_byte(0x47, 0x02);
#endif
}

static void lt8912b_mipi_analog(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
#ifdef PN_SWAP
	/* P/N swap */
	lt8912b_write_byte(0x3e, 0xf6);
#else
	/* if mipi pin map follow reference design, no need swap P/N. */
	lt8912b_write_byte(0x3e, 0xd6);
#endif

	lt8912b_write_byte(0x3f, 0xd4);
	lt8912b_write_byte(0x41, 0x3c);
}

static void lt8912b_mipi_basic_set(void)
{
	/* 0: 4lane; 1: 1lane; 2: 2lane; 3: 3lane */
	unsigned char lane = 0;
	g_i2c_addr = I2C_BASS_ADDRESS2;

	/* term en */
	lt8912b_write_byte(0x10, 0x01);
	/* settle */
	lt8912b_write_byte(0x11, 0x5);
	/* trail */
	/* lt8912b_write_byte(0x12,0x08); */
	lt8912b_write_byte(0x13, lane);
	/* debug mux */
	lt8912b_write_byte(0x14, 0x00);

/* for EVB only, if mipi pin map follow reference design, no need swap lane */
#ifdef LANE_SWAP
	/* lane swap:3210 */
	lt8912b_write_byte(0x15,0xa8);
	debug("mipi basic set: lane swap 3210, %d\n", lane);
#else
	/* lane swap:0123 */
	lt8912b_write_byte(0x15,0x00);
#endif
	/* hshift 3 */
	lt8912b_write_byte(0x1a,0x03);
	/* vshift 3 */
	lt8912b_write_byte(0x1b,0x03);
}

static void lt8912b_mipi_video_setup(struct video_timing *video_format)
{
	g_i2c_addr = I2C_BASS_ADDRESS2;
	/* hwidth */
	lt8912b_write_byte(0x18, (unsigned char)(video_format->hs % 256));
	/* vwidth 6 */
	lt8912b_write_byte(0x19, (unsigned char)(video_format->vs % 256));
	/* H_active[7:0] */
	lt8912b_write_byte(0x1c, (unsigned char)(video_format->hact % 256));
	/* H_active[15:8] */
	lt8912b_write_byte(0x1d, (unsigned char)(video_format->hact / 256));
	/* fifo_buff_length 12 */
	lt8912b_write_byte(0x2f, 0x0c);
	/* H_total[7:0] */
	lt8912b_write_byte(0x34, (unsigned char)(video_format->htotal % 256));
	/* H_total[15:8] */
	lt8912b_write_byte(0x35, (unsigned char)(video_format->htotal / 256));
	/* V_total[7:0] */
	lt8912b_write_byte(0x36, (unsigned char)(video_format->vtotal % 256));
	/* V_total[15:8] */
	lt8912b_write_byte(0x37, (unsigned char)(video_format->vtotal / 256));
	/* VBP[7:0] */
	lt8912b_write_byte(0x38, (unsigned char)(video_format->vbp % 256));
	/* VBP[15:8] */
	lt8912b_write_byte(0x39, (unsigned char)(video_format->vbp / 256));
	/* VFP[7:0] */
	lt8912b_write_byte(0x3a, (unsigned char)(video_format->vfp % 256));
	/* VFP[15:8] */
	lt8912b_write_byte(0x3b, (unsigned char)(video_format->vfp / 256));
	/* HBP[7:0] */
	lt8912b_write_byte(0x3c, (unsigned char)(video_format->hbp % 256));
	/* HBP[15:8] */
	lt8912b_write_byte(0x3d, (unsigned char)(video_format->hbp / 256));
	/* HFP[7:0] */
	lt8912b_write_byte(0x3e, (unsigned char)(video_format->hfp % 256));
	/* HFP[15:8] */
	lt8912b_write_byte(0x3f, (unsigned char)(video_format->hfp / 256));
}

static void lt8912b_mipi_rx_logic_resgister(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	/*mipi rx reset */
	lt8912b_write_byte(0x03, 0x7f);
	lt8912b_delay(10);
	lt8912b_write_byte(0x03, 0xff);

	/* dds reset */
	lt8912b_write_byte(0x05, 0xfb);
	lt8912b_delay(10);
	lt8912b_write_byte(0x05, 0xff);
}


static void lt8912b_dds_config(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS2;
	lt8912b_write_regs(dds_config, ARRAY_SIZE(dds_config));
}

static void lt8912b_audio_iis_enable(void)
{
	/* sampling 48K, sclk = 64*fs */
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_byte(0xB2, 0x01);
	g_i2c_addr = I2C_BASS_ADDRESS3;
	lt8912b_write_byte(0x06, 0x08);
	lt8912b_write_byte(0x07, 0xF0);
	/* 0xE2:32FS; 0xD2:64FS */
	lt8912b_write_byte(0x34, 0xD2);
}

static void lt8912b_lvds_power_up(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_byte(0x44, 0x30);
	lt8912b_write_byte(0x51, 0x05);
}

static void lt8912b_lvds_bypass(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_regs(lvds_bypass, ARRAY_SIZE(lvds_bypass));
}

static void lt8912b_lvds_output(int on)
{
	if(on) {
		g_i2c_addr = I2C_BASS_ADDRESS1;
		lt8912b_write_regs(lvds_output_on, ARRAY_SIZE(lvds_output_on));
		debug("lt8912b lvds output enable\n");
	} else {
		g_i2c_addr = I2C_BASS_ADDRESS1;
		lt8912b_write_byte(0x44, 0x31);
	}
}

static void lt8912b_hdmi_output(int on)
{
	if(on) {
		g_i2c_addr = I2C_BASS_ADDRESS1;
		/* enable hdmi output */
		lt8912b_write_byte(0x33, 0x0e);
	} else {
		g_i2c_addr = I2C_BASS_ADDRESS1;
		/* disable hdmi output */
		lt8912b_write_byte(0x33, 0x0c);
	}
}

#ifndef LVDS_BYPASS
static void lt8912b_core_pll_setup(struct panel_parameter *panel)
{
	unsigned char cpll_m, cpll_k1,cpll_k2;
	unsigned int temp;

	cpll_m = (panel->pclk_khz * 7) / 25 / 1000;

	temp = ((panel->pclk_khz * 7) / 25) % 1000;
	temp = temp * 16384 / 1000;
	cpll_k1 = temp % 256;
	cpll_k2 = temp / 256;

	g_i2c_addr = I2C_BASS_ADDRESS1;
	/* cp=50uA */
	lt8912b_write_byte(0x50, 0x24);
	/* xtal_clk as reference,second order passive LPF PLL */
	lt8912b_write_byte(0x51, 0x05);
	/* use second-order PLL */
	lt8912b_write_byte(0x52, 0x14);

	/* CP_PRESET_DIV_RATIO */
	lt8912b_write_byte(0x69, cpll_m);
	lt8912b_write_byte(0x69, (cpll_m | 0x80));

	/* RGD_CP_SOFT_K_EN,RGD_CP_SOFT_K[13:8] */
	lt8912b_write_byte(0x6c, (cpll_k2 | 0x80));
	lt8912b_write_byte(0x6b, cpll_k1);

	/* core pll reset */
	lt8912b_write_byte(0x04,0xfb);
	lt8912b_write_byte(0x04,0xff);
}

static void lt8912b_scaler_setup(struct video_timing *input_video,
					struct panel_parameter *panel)
{
	/*
	 * for example: 720P to 1280x800
	 * These register base on MIPI resolution and LVDS panel resolution.
	 */
	unsigned int h_ratio,v_ratio;
	unsigned char i;
	unsigned int htotal;
	h_ratio = input_video->hact * 4096 / panel->hact;
	v_ratio = input_video->vact * 4096 / panel->vact;

	g_i2c_addr = I2C_BASS_ADDRESS1;
	lt8912b_write_byte(0x80, 0x00);
	lt8912b_write_byte(0x81, 0xff);
	lt8912b_write_byte(0x82, 0x03);
	lt8912b_write_byte(0x83, (unsigned char)(input_video->hact % 256));
	lt8912b_write_byte(0x84, (unsigned char)(input_video->hact / 256));
	lt8912b_write_byte(0x85, 0x80);
	lt8912b_write_byte(0x86, 0x10);
	lt8912b_write_byte(0x87, (unsigned char)(panel->htotal % 256));
	lt8912b_write_byte(0x88, (unsigned char)(panel->htotal / 256));
	lt8912b_write_byte(0x89, (unsigned char)(panel->hs % 256));
	lt8912b_write_byte(0x8a, (unsigned char)(panel->hbp % 256));
	lt8912b_write_byte(0x8b, (unsigned char)(panel->vs % 256));
	lt8912b_write_byte(0x8c, (unsigned char)(panel->hact % 256));
	lt8912b_write_byte(0x8d, (unsigned char)(panel->vact % 256));
	lt8912b_write_byte(0x8e, (unsigned char)(panel->vact / 256) * 16 + (panel->hact / 256));
	lt8912b_write_byte(0x8f, (unsigned char)(h_ratio % 256));
	lt8912b_write_byte(0x90, (unsigned char)(h_ratio / 256));
	lt8912b_write_byte(0x91, (unsigned char)(v_ratio % 256));
	lt8912b_write_byte(0x92, (unsigned char)(v_ratio / 256));
	lt8912b_write_byte(0x7f, 0x96);
	/* lt8912b_write_byte(0x7f, 0xb0); */
	/* lt8912b_write_byte(0xa8, 0x3b); */
	lt8912b_write_byte(0xa8, 0x13);

	/* lvds pll reset */
	lt8912b_write_byte(0x02, 0xf7);
	lt8912b_write_byte(0x02, 0xff);

	/* scaler reset */
	lt8912b_write_byte(0x03, 0xcf);
	lt8912b_write_byte(0x03, 0xff);

	lt8912b_write_byte(0x7f, 0xb0);

	for(i = 0; i < 5; i++) {
		if(lt8912b_read_byte(0xa7) & 0x20) {
			htotal = (lt8912b_read_byte(0xa7) & 0x0f) * 0x100 + lt8912b_read_byte(0xa6);
			debug("scaler setup htotal = %d\n", htotal);
			break;
		}
		debug("scaler loop %d\n", i);
		lt8912b_delay(100);
	}
}
#endif

static void lt8912b_lvds_output_cfg(void)
{
	lt8912b_lvds_power_up();
#ifdef LVDS_BYPASS
	lt8912b_lvds_bypass();
#else
	lt8912b_core_pll_setup(&video_lvds_60Hz);
	lt8912b_scaler_setup(&video_lvds_60Hz, &video_lvds_60Hz);
#endif
}

static void lt8912b_mipi_input_det(void)
{
	static unsigned char Hsync_H_last = 0, Hsync_L_last = 0;
	static unsigned char Vsync_H_last = 0, Vsync_L_last = 0;
	static unsigned char Hsync_L, Hsync_H, Vsync_L, Vsync_H;

	g_i2c_addr = I2C_BASS_ADDRESS1;
	Hsync_L = lt8912b_read_byte(0x9c);
	Hsync_H = lt8912b_read_byte(0x9d);
	Vsync_L = lt8912b_read_byte(0x9e);
	Vsync_H = lt8912b_read_byte(0x9f);

	/* high byte changed */
	if((Hsync_H != Hsync_H_last) || (Vsync_H != Vsync_H_last)) {
		debug("0x9c~9f = %x, %x, %x, %x\n", Hsync_H, Hsync_L,Vsync_H, Vsync_L);

		if(Vsync_H == 0x02 && Vsync_L == 0x71) {
			/* 625 */
			lt8912b_mipi_video_setup(&video_1024x600_60Hz);
			debug("videoformat = VESA_1024x600_60\n");
		} else if(Vsync_H==0x02 && Vsync_L <= 0xef&&Vsync_L >= 0xec) {
			/* 0x2EE */
			lt8912b_mipi_video_setup(&video_1280x720_60Hz);
			debug("videoformat = VESA_1280x720_60\n");
		} else if(Vsync_H == 0x03 && Vsync_L <= 0x3a &&Vsync_L >= 0x34) {
			/* 0x337 */
			lt8912b_mipi_video_setup(&video_1280x800_60Hz);
			debug("videoformat = VESA_1280x800_60\n");
		} else if(Vsync_H == 0x04 && Vsync_L <= 0x67&&Vsync_L >= 0x63) {
			/* 0x465 */
			lt8912b_mipi_video_setup(&video_1920x1080_60Hz);
			debug("videoformat = VESA_1920x1080_60");
		} else if(Vsync_H == 0x03 && Vsync_L <= 0x23&&Vsync_L >= 0x1d) {
			/* 0x320 */
			lt8912b_mipi_video_setup(&video_lvds_60Hz);
			debug("videoformat = VESA_1366x768_60");
		} else if(Vsync_H == 0x1d && Vsync_L <= 0x05 &&Vsync_L >= 0x1d) {
			/* 0x320 */
			lt8912b_mipi_video_setup(&video_800x1280_60Hz);
			debug("videoformat = VESA_800x1280_60");
		} else {
#ifdef OUTPUT_1080P_60Hz
			lt8912b_mipi_video_setup(&video_1920x1080_60Hz);
			debug("videoformat = VESA_1920x1080_60\n");
#endif
#ifdef OUTPUT_720P_60Hz
			lt8912b_mipi_video_setup(&video_1280x720_60Hz);
			debug("videoformat = VESA_1280x720_60\n");
#endif
		}

		Hsync_L_last = Hsync_L;
		Hsync_H_last = Hsync_H;
		Vsync_L_last = Vsync_L;
		Vsync_H_last = Vsync_H;

		lt8912b_mipi_rx_logic_resgister();
	}
}

static unsigned char lt8912b_get_hpd(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	if((lt8912b_read_byte(0xc1) & 0x80) == 0x80)
		return 1;
	return 0;
}

static void lt8912b_suspend(int on)
{
	static int suspend_on = 0;

	/* 9mA,HPD detect is normal */
	if(on) {
		if(!suspend_on) {
			/* enter suspend mode */
			g_i2c_addr = I2C_BASS_ADDRESS1;
			lt8912b_write_byte(0x54, 0x1d);
			lt8912b_write_byte(0x51, 0x15);
			lt8912b_write_byte(0x44, 0x31);
			lt8912b_write_byte(0x41, 0xbd);
			lt8912b_write_byte(0x5c, 0x11);
			suspend_on = 1;
			debug("suspend on\n");
		  }
	} else {
		if(suspend_on) {
			/* exist suspend mode */
			g_i2c_addr = I2C_BASS_ADDRESS1;
			lt8912b_write_byte(0x5c, 0x10);
			lt8912b_write_byte(0x54, 0x1c);
			lt8912b_write_byte(0x51, 0x2d);
			lt8912b_write_byte(0x44, 0x30);
			lt8912b_write_byte(0x41, 0xbc);

			lt8912b_delay(10);
			lt8912b_write_byte(0x03, 0x7f);
			lt8912b_delay(10);
			lt8912b_write_byte(0x03, 0xff);

			lt8912b_write_byte(0x05, 0xfb);
			lt8912b_delay(10);
			lt8912b_write_byte(0x05, 0xff);
			suspend_on = 0;
			debug("suspend off\n");
		}
	}
}

static int lt8912b_setup(void)
{
	g_i2c_addr = I2C_BASS_ADDRESS1;
	printf("\nlt8912b id:0x%x 0x%x\n", lt8912b_read_byte(0x00),
				lt8912b_read_byte(0x01));

	lt8912b_digital_clock_enable();
	lt8912b_tx_analog();
	lt8912b_cbus_analog();
	lt8912b_hmdi_pll_analog();
	lt8912b_mipi_analog();
	lt8912b_mipi_basic_set();
	lt8912b_dds_config();
	lt8912b_mipi_input_det();
	lt8912b_audio_iis_enable();
	lt8912b_avi_info_frame();
	lt8912b_mipi_rx_logic_resgister();
	lt8912b_lvds_output_cfg();

	/* lvds output only */
	lt8912b_lvds_output(1);

	if(lt8912b_get_hpd()) {
		lt8912b_hdmi_output(1);
		debug("lt8912b_get_hpd: high\n");
	}

	lt8912b_mipi_input_det();
	lt8912b_delay(1000);

	lt8912b_suspend(0);
	lt8912b_hdmi_output(1);

	return 0;
}

static int lt8912b_enable_backlight(struct udevice *dev)
{
	return 0;
}

static int lt8912b_get_display_timing(struct udevice *dev,
					    struct display_timing *timings)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct lt8912b_priv *priv = &lt8912b_priv;

	memcpy(timings, &default_timing, sizeof(*timings));

	/* fill characteristics of DSI data link */
	if (device) {
		device->lanes = priv->lanes;
		device->format = priv->format;
		device->mode_flags = priv->mode_flags;
	}

	return 0;
}

static int lt8912b_probe(struct udevice *dev)
{
	struct lt8912b_priv *priv = &lt8912b_priv;
	int ret;
	unsigned char addr;

	debug("\n%s\n", __func__);

	priv->format = MIPI_DSI_FMT_RGB888;
	priv->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_VIDEO_HSE;

	/* reset lt8912b */
	ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset_gpio,
						GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "No reset-gpios property %d\n", ret);
		return ret;
	}
	ret = dm_gpio_set_value(&priv->reset_gpio, true);
	if (ret) {
		dev_err(dev, "reset gpio fails to set true\n");
		return ret;
	}
	lt8912b_delay(200);
	ret = dm_gpio_set_value(&priv->reset_gpio, false);
	if (ret) {
		dev_err(dev, "reset gpio fails to set false\n");
		return ret;
	}

	/* lt8912b i2c probe */
	ret = dm_i2c_probe(dev_get_parent(dev),
				I2C_BASS_ADDRESS1, 0, &priv->dev_addr1);
	if (ret) {
		addr = I2C_BASS_ADDRESS1;
		dev_err(dev, "Can't find device id=0x%x\n", addr);
		return -ENODEV;
	}
	ret = dm_i2c_probe(dev_get_parent(dev),
				I2C_BASS_ADDRESS2, 0, &priv->dev_addr2);
	if (ret) {
		addr = I2C_BASS_ADDRESS2;
		dev_err(dev, "Can't find device id=0x%x\n", addr);
		return -ENODEV;
	}
	ret = dm_i2c_probe(dev_get_parent(dev),
				I2C_BASS_ADDRESS3, 0, &priv->dev_addr3);
	if (ret) {
		addr = I2C_BASS_ADDRESS3;
		dev_err(dev, "Can't find device id=0x%x\n", addr);
		return -ENODEV;
	}

	lt8912b_setup();

	return 0;
}

static const struct panel_ops lt8912b_ops = {
	.enable_backlight = lt8912b_enable_backlight,
	.get_display_timing = lt8912b_get_display_timing,
};

static const struct udevice_id lt8912b_ids[] = {
	{ .compatible = "lontium,lt8912b" },
	{ }
};

U_BOOT_DRIVER(lt8912b_mipi2hdmi) = {
	.name		= "lt8912b_mipi2hdmi",
	.id		= UCLASS_PANEL,
	.of_match	= lt8912b_ids,
	.ops		= &lt8912b_ops,
	.probe		= lt8912b_probe,
	.plat_auto 	= sizeof(struct mipi_dsi_panel_plat),
};
