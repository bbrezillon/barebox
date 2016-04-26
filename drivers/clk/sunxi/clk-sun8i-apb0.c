/*
 * Copyright (C) 2014 Chen-Yu Tsai
 * Author: Chen-Yu Tsai <wens@csie.org>
 *
 * Allwinner A23 APB0 clock driver
 *
 * License Terms: GNU General Public License v2
 *
 * Based on clk-sun6i-apb0.c
 * Allwinner A31 APB0 clock driver
 *
 * Copyright (C) 2014 Free Electrons
 * Author: Boris BREZILLON <boris.brezillon@free-electrons.com>
 *
 */

#include <driver.h>
#include <init.h>
#include <io.h>
#include <malloc.h>
#include <of.h>
#include <printk.h>
#include <stdio.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>

static int sun8i_a23_apb0_clk_probe(struct device_d *dev)
{
	struct device_node *np = dev->device_node;
	const char *clk_name = np->name;
	const char *clk_parent;
	void __iomem *reg;
	struct clk *clk;

	reg = dev_get_mem_region(dev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	clk_parent = of_clk_get_parent_name(np, 0);
	if (!clk_parent)
		return -EINVAL;

	of_property_read_string(np, "clock-output-names", &clk_name);

	/* The A23 APB0 clock is a standard 2 bit wide divider clock */
	clk = clk_divider_power_of_two(clk_name, clk_parent, reg, 0, 2, 0);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	return of_clk_add_provider(np, of_clk_src_simple_get, clk);
}

static const struct of_device_id sun8i_a23_apb0_clk_dt_ids[] = {
	{ .compatible = "allwinner,sun8i-a23-apb0-clk" },
	{ /* sentinel */ }
};

static struct driver_d sun8i_a23_apb0_clk_driver = {
	.name = "sun8i-a23-apb0-clk",
	.of_compatible = DRV_OF_COMPAT(sun8i_a23_apb0_clk_dt_ids),
	.probe = sun8i_a23_apb0_clk_probe,
};

static int sun8i_a23_apb0_clk_init(void)
{
	return platform_driver_register(&sun8i_a23_apb0_clk_driver);
}
postcore_initcall(sun8i_a23_apb0_clk_init);

MODULE_AUTHOR("Chen-Yu Tsai <wens@csie.org>");
MODULE_DESCRIPTION("Allwinner A23 APB0 clock Driver");
MODULE_LICENSE("GPL v2");
