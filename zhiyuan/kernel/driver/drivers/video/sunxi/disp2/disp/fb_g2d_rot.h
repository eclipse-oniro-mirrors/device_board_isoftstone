/*
 * drivers/video/fbdev/sunxi/disp2/disp/fb_g2d_rot/fb_g2d_rot.h
 *
 * Copyright (c) 2007-2019 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
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
#ifndef _FB_G2D_ROT_H
#define _FB_G2D_ROT_H

#include "disp_sys_intf.h"
#include <sunxi-g2d.h>
#include "de/disp_features.h"
#include <linux/fb.h>
#include <video/sunxi_display2.h>
#include "dev_disp.h"


enum {
	FB_ROTATION_HW_0 = 0,
	FB_ROTATION_HW_90 = 1,
	FB_ROTATION_HW_180 = 2,
	FB_ROTATION_HW_270 = 3,
};

struct fb_g2d_rot_t {
	g2d_blt_h info;
	void *dst_vir_addr;
	dma_addr_t dst_phy_addr;
	unsigned int dst_mem_len;
	struct fb_info *fb;
	int switch_buffer_flag;
	int (*apply)(struct fb_g2d_rot_t *inst,
		     struct disp_layer_config *config);
	int (*free)(struct fb_g2d_rot_t *inst);
	int (*set_degree)(struct fb_g2d_rot_t *inst,
		      struct disp_layer_config *config, unsigned int degree);
};

struct fb_g2d_rot_t *fb_g2d_rot_create(struct fb_info *p_info,
				       unsigned int fb_id,
				       struct disp_layer_config *config);

#endif /*End of file*/
