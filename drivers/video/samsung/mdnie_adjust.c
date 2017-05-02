/* linux/drivers/video/samsung/mdnie_rgb_adj.c
 *
 * RGB color tuning interface for Samsung mDNIe
 *
 * Copyright (c) 2016 The CyanogenMod Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/device.h>
#include <linux/mdnie.h>
#include <linux/platform_device.h>

#include "mdnie_adjust.h"
#include "s3cfb.h"
#include "s3cfb_mdnie.h"

#ifdef CONFIG_FB_MDNIE_RGB_ADJUST
static inline u8 mdnie_scale_value(u8 val, u8 adj) {
	int big = val * adj;
	return big / 255;
}

#if defined(CONFIG_CPU_EXYNOS4210)
static const unsigned short default_scr_values[] = {
	0x00c8, 0x0000,	/*kb R	SCR */
	0x00c9, 0x0000,	/*gc R */
	0x00ca, 0xafaf,	/*rm R */
	0x00cb, 0xafaf,	/*yw R */
	0x00cc, 0x0000,	/*kb G */
	0x00cd, 0xb7b7,	/*gc G */
	0x00ce, 0x0000,	/*rm G */
	0x00cf, 0xb7b7,	/*yw G */
	0x00d0, 0x00bc,	/*kb B */
	0x00d1, 0x00bc,	/*gc B */
	0x00d2, 0x00bc,	/*rm B */
	0x00d3, 0x00bc,	/*yw B */
	END_SEQ, 0x0000,
};
#else
static const unsigned short default_scr_values[] = {
	0x00e1, 0xff00, /*SCR RrCr*/
	0x00e2, 0x00ff, /*SCR RgCg*/
	0x00e3, 0x00ff, /*SCR RbCb*/
	0x00e4, 0x00ff, /*SCR GrMr*/
	0x00e5, 0xff00, /*SCR GgMg*/
	0x00e6, 0x00ff, /*SCR GbMb*/
	0x00e7, 0x00ff, /*SCR BrYr*/
	0x00e8, 0x00ff, /*SCR BgYg*/
	0x00e9, 0xff00, /*SCR BbYb*/
	0x00ea, 0x00ff, /*SCR KrWr*/
	0x00eb, 0x00ff, /*SCR KgWg*/
	0x00ec, 0x00ff, /*SCR KbWb*/
	END_SEQ, 0x0000,
};
#endif

u16 mdnie_rgb_hook(struct mdnie_info *mdnie, int reg, int val) {
	u8 adj = 255, c1, c2;
	u16 res;

	if (!mdnie->rgb_adj_enable) {
		return val;
	}

	if (reg >= MDNIE_SCR_START && reg <= MDNIE_SCR_END) {
		/* adjust */
		if (IS_SCR_RED(reg)) {
			// XrYr - adjust r
			adj = mdnie->r_adj;
		} else if (IS_SCR_GREEN(reg)) {
			// XgYg - adjust g
			adj = mdnie->g_adj;
		} else if (IS_SCR_BLUE(reg)) {
			// XbYb - adjust b
			adj = mdnie->b_adj;
		}
		c1 = mdnie_scale_value(val >> 8, adj); // adj for X
		c2 = mdnie_scale_value(val, adj); // adj for Y
		res = (c1 << 8) + c2;
		return res;
	}
	return val;
}

void mdnie_send_rgb(struct mdnie_info *mdnie) {
	int i = 0;

	if (!mdnie->rgb_adj_enable) {
		return;
	}

	while (default_scr_values[i] != END_SEQ) {
		mdnie_write(default_scr_values[i],
				mdnie_rgb_hook(mdnie, default_scr_values[i], default_scr_values[i+1]));
		i += 2;
	}
}
#endif // CONFIG_FB_MDNIE_RGB_ADJUST

#ifdef CONFIG_FB_MDNIE_OUTDOOR
void mdnie_send_outdoor(struct mdnie_info *mdnie) {
	int i = 0;
	while (mdnie_outdoor_default[i] != END_SEQ) {
		mdnie_write(mdnie_outdoor_default[i], mdnie_outdoor_default[i+1]);
		i += 2;
	}
}
#endif

unsigned short mdnie_effect_master_hook(struct mdnie_info *mdnie, unsigned short val) {
#ifdef CONFIG_FB_MDNIE_RGB_ADJUST
	if (mdnie->r_adj != 255 || mdnie->g_adj != 255 || mdnie->b_adj != 255) {
		val |= MDNIE_SCR_MASK;
	}
#endif
#ifdef CONFIG_FB_MDNIE_OUTDOOR
	if (mdnie->outdoor_enable) {
		val |= MDNIE_OUTDOOR_MASK;
	}
#endif

	return val;
}

void mdnie_setup_adjust(struct mdnie_adjust *adj) {
#ifdef CONFIG_FB_MDNIE_RGB_ADJUST
	adj->applied_rgb = false;
#endif
#ifdef CONFIG_FB_MDNIE_OUTDOOR
	adj->applied_outdoor = false
#endif
}

unsigned short mdnie_hook(struct mdnie_info *mdnie, struct mdnie_adjust *adj, unsigned short reg, unsigned short val) {
	if (reg == MDNIE_EFFECT_MASTER) {
		return mdnie_effect_master_hook(mdnie, val);
	}
#ifdef CONFIG_FB_MDNIE_RGB_ADJUST
	if (reg >= MDNIE_SCR_START && reg <= MDNIE_SCR_END) {
		adj->applied_rgb = true;
		return mdnie_rgb_hook(mdnie_rgb_hook(mdnie, reg, val));
	} else if (!adj->applied_rgb && reg > MDNIE_SCR_END) {
		mdnie_send_rgb(mdnie);
		adj->applied_rgb = true;
	}
#endif
#ifdef CONFIG_FB_MDNIE_OUTDOOR
	if (reg >= MDNIE_OUTDOOR_START && reg <= MDNIE_OUTDOOR_END) {
		adj->applied_outdoor = true;
		return val | GET_OUTDOOR_VALUE(reg); // TODO: is this what we want, or should we just override this?
	} else if (!adj->applied_outdoor && reg > MDNIE_OUTDOOR_END) {
		mdnie_send_outdoor(mdnie);
		adj->applied_outdoor = true;
	}
#endif
	return val;
}
