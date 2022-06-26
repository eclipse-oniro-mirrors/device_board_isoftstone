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

#include <linux/crc32.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/system_info.h>
#include <linux/rockchip/cpu.h>

unsigned long rockchip_soc_id;
EXPORT_SYMBOL(rockchip_soc_id);

static int rockchip_cpuinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_cell *cell;
	unsigned char *efuse_buf, buf[16];
	size_t len = 0;
	int i;

	cell = nvmem_cell_get(dev, "cpu-code");
	if (!IS_ERR(cell)) {
		efuse_buf = nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR(efuse_buf))
			return PTR_ERR(efuse_buf);

		if (len == 2)
			rockchip_set_cpu((efuse_buf[0] << 8 | efuse_buf[1]));
		kfree(efuse_buf);
	}

	cell = nvmem_cell_get(dev, "cpu-version");
	if (!IS_ERR(cell)) {
		efuse_buf = nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (IS_ERR(efuse_buf))
			return PTR_ERR(efuse_buf);

		if ((len == 1) && (efuse_buf[0] > rockchip_get_cpu_version()))
			rockchip_set_cpu_version(efuse_buf[0]);
		kfree(efuse_buf);
	}

	cell = nvmem_cell_get(dev, "id");
	if (IS_ERR(cell)) {
		dev_err(dev, "failed to get id cell: %ld\n", PTR_ERR(cell));
		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return PTR_ERR(cell);
		return PTR_ERR(cell);
	}
	efuse_buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(efuse_buf))
		return PTR_ERR(efuse_buf);

	if (len != 16) {
		kfree(efuse_buf);
		dev_err(dev, "invalid id len: %zu\n", len);
		return -EINVAL;
	}

	for (i = 0; i < 8; i++) {
		buf[i] = efuse_buf[1 + (i << 1)];
		buf[i + 8] = efuse_buf[i << 1];
	}

	kfree(efuse_buf);

	dev_info(dev, "SoC\t\t: %lx\n", rockchip_soc_id);

#ifdef CONFIG_NO_GKI
	system_serial_low = crc32(0, buf, 8);
	system_serial_high = crc32(system_serial_low, buf + 8, 8);

	dev_info(dev, "Serial\t\t: %08x%08x\n",
		 system_serial_high, system_serial_low);
#endif

	return 0;
}

static const struct of_device_id rockchip_cpuinfo_of_match[] = {
	{ .compatible = "rockchip,cpuinfo", },
	{ },
};
MODULE_DEVICE_TABLE(of, rockchip_cpuinfo_of_match);

static struct platform_driver rockchip_cpuinfo_driver = {
	.probe = rockchip_cpuinfo_probe,
	.driver = {
		.name = "rockchip-cpuinfo",
		.of_match_table = rockchip_cpuinfo_of_match,
	},
};

static int __init rockchip_cpuinfo_init(void)
{
	return platform_driver_register(&rockchip_cpuinfo_driver);
}
subsys_initcall_sync(rockchip_cpuinfo_init);

static void __exit rockchip_cpuinfo_exit(void)
{
	platform_driver_unregister(&rockchip_cpuinfo_driver);
}
module_exit(rockchip_cpuinfo_exit);

MODULE_LICENSE("GPL");
