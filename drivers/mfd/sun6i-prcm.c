/*
 * Copyright (C) 2014 Free Electrons
 *
 * License Terms: GNU General Public License v2
 * Author: Boris BREZILLON <boris.brezillon@free-electrons.com>
 *
 * Allwinner PRCM (Power/Reset/Clock Management) driver
 *
 */

#include <driver.h>
#include <init.h>
#include <of.h>
#include <malloc.h>
#include <printk.h>
#include <stdio.h>
#include <linux/kernel.h>

static const struct resource sun6i_a31_ar100_clk_res[] = {
	{
		.start = 0x0,
		.end = 0x3,
		.flags = IORESOURCE_MEM,
	},
};

static const struct resource sun6i_a31_apb0_clk_res[] = {
	{
		.start = 0xc,
		.end = 0xf,
		.flags = IORESOURCE_MEM,
	},
};

static const struct resource sun6i_a31_apb0_gates_clk_res[] = {
	{
		.start = 0x28,
		.end = 0x2b,
		.flags = IORESOURCE_MEM,
	},
};

static const struct resource sun6i_a31_ir_clk_res[] = {
	{
		.start = 0x54,
		.end = 0x57,
		.flags = IORESOURCE_MEM,
	},
};

static const struct resource sun6i_a31_apb0_rstc_res[] = {
	{
		.start = 0xb0,
		.end = 0xb3,
		.flags = IORESOURCE_MEM,
	},
};

static const struct of_device_id prcm_dt_ids[] = {
	{
		.compatible = "allwinner,sun8i-a23-apb0-clk",
		.data = sun6i_a31_apb0_clk_res,
	},
	{
		.compatible = "allwinner,sun8i-a23-apb0-gates-clk",
		.data = sun6i_a31_apb0_gates_clk_res,
       	},
	{
		.compatible = "allwinner,sun6i-a31-clock-reset",
		.data = sun6i_a31_apb0_rstc_res
	},
	{
		.compatible = "allwinner,sun6i-a31-ar100-clk",
		.data = sun6i_a31_ar100_clk_res,
	},
	{
		.compatible = "allwinner,sun6i-a31-apb0-clk",
		.data = sun6i_a31_apb0_clk_res,
	},
	{
		.compatible = "allwinner,sun6i-a31-apb0-gates-clk",
		.data = sun6i_a31_apb0_gates_clk_res,
       	},
	{
		.compatible = "allwinner,sun4i-a10-mod0-clk",
		.data = sun6i_a31_ir_clk_res,
 	},
	{ /* sentinel */ },
};

static const struct of_device_id sun6i_prcm_dt_ids[] = {
	{ .compatible = "allwinner,sun6i-a31-prcm" },
	{ .compatible = "allwinner,sun8i-a23-prcm" },
	{ /* sentinel */ },
};

static int sun6i_prcm_probe(struct device_d *dev)
{
	struct device_node *child, *np = dev->device_node;
	const struct of_device_id *match;
	const struct prcm_data *data;
	struct resource *res;
	int ret;

	match = of_match_node(sun6i_prcm_dt_ids, np);
	if (!match)
		return -EINVAL;

	data = match->data;

	res = dev_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no prcm memory region provided\n");
		return -ENOENT;
	}

	for_each_child_of_node(np, child) {
		struct resource *subres;
		struct device_d *subdev;

		if (!of_device_is_available(np))
			continue;

		match = of_match_node(prcm_dt_ids, child);
		if (!match)
			continue;

		subres = xzalloc(sizeof(*subres));
		*subres = *((struct resource *)match->data);
		subres->start += res->start;
		subres->end += res->start;

		subdev = xzalloc(sizeof(*subdev));
		subdev->id = DEVICE_ID_SINGLE;
		subdev->device_node = child;
		subdev->parent = dev;
		subdev->resource = subres;
		subdev->num_resources = 1;
		snprintf(subdev->name, MAX_DRIVER_NAME, "%s", child->name);
		ret = platform_device_register(dev);
		if (ret) {
			dev_err(dev, "failed to add device %s\n", child->name);
			free(subdev);
			free(subres);
			continue;
		}
	}

	return 0;
}

static struct driver_d sun6i_prcm_driver = {
	.name = "sun6i-prcm",
	.of_compatible = DRV_OF_COMPAT(sun6i_prcm_dt_ids),
	.probe = sun6i_prcm_probe,
};

static int sun6i_prcm_init(void)
{
	return platform_driver_register(&sun6i_prcm_driver);
}
postcore_initcall(sun6i_prcm_init);

MODULE_AUTHOR("Boris BREZILLON <boris.brezillon@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner sun6i PRCM driver");
MODULE_LICENSE("GPL v2");
