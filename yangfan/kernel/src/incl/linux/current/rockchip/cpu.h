/*
 * Copyright (C) 2017 Rockchip Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#ifndef __LINUX_ROCKCHIP_CPU_H
#define __LINUX_ROCKCHIP_CPU_H

#include <linux/of.h>
#define ROCKCHIP_CPU_MASK		0xffff0000
#define ROCKCHIP_CPU_SHIFT		16

#if IS_ENABLED(CONFIG_ROCKCHIP_CPUINFO)

extern unsigned long rockchip_soc_id;

#define ROCKCHIP_CPU_VERION_MASK	0x0000f000
#define ROCKCHIP_CPU_VERION_SHIFT	12

static inline unsigned long rockchip_get_cpu_version(void)
{
	return (rockchip_soc_id & ROCKCHIP_CPU_VERION_MASK)
		>> ROCKCHIP_CPU_VERION_SHIFT;
}

static inline void rockchip_set_cpu_version(unsigned long ver)
{
	rockchip_soc_id &= ~ROCKCHIP_CPU_VERION_MASK;
	rockchip_soc_id |=
		(ver << ROCKCHIP_CPU_VERION_SHIFT) & ROCKCHIP_CPU_VERION_MASK;
}

static inline void rockchip_set_cpu(unsigned long code)
{
	if (!code)
		return;

	rockchip_soc_id &= ~ROCKCHIP_CPU_MASK;
	rockchip_soc_id |= (code << ROCKCHIP_CPU_SHIFT) & ROCKCHIP_CPU_MASK;
}
#else

#define rockchip_soc_id 0

static inline unsigned long rockchip_get_cpu_version(void)
{
	return 0;
}

static inline void rockchip_set_cpu_version(unsigned long ver)
{
}

static inline void rockchip_set_cpu(unsigned long code)
{
}
#endif

#define ROCKCHIP_SOC(id, ID) \
static inline bool soc_is_##id(void) \
{ \
	if (rockchip_soc_id) \
		return ((rockchip_soc_id & ROCKCHIP_SOC_MASK) == ROCKCHIP_SOC_ ##ID); \
	return of_machine_is_compatible("rockchip,"#id); \
}

#endif
