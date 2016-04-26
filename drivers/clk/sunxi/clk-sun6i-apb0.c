/*
 * Copyright (C) 2014 Free Electrons
 *
 * License Terms: GNU General Public License v2
 * Author: Boris BREZILLON <boris.brezillon@free-electrons.com>
 *
 * Allwinner A31 APB0 clock driver
 *
 */

#include <driver.h>
#include <init.h>
#include <io.h>
#include <malloc.h>
#include <of.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/kernel.h>

/*
 * The APB0 clk has a configurable divisor.
 *
 * We must use a clk_div_table and not a regular power of 2
 * divisor here, because the first 2 values divide the clock
 * by 2.
 */
static const struct clk_div_table sun6i_a31_apb0_divs[] = {
	{ .val = 0, .div = 2, },
	{ .val = 1, .div = 2, },
	{ .val = 2, .div = 4, },
	{ .val = 3, .div = 8, },
	{ /* sentinel */ },
};

static int sun6i_a31_apb0_clk_probe(struct device_d *dev)
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

	clk = clk_divider_table(clk_name, clk_parent, reg, 0, 2,
				sun6i_a31_apb0_divs, 0);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	return of_clk_add_provider(np, of_clk_src_simple_get, clk);
}

static const struct of_device_id sun6i_a31_apb0_clk_dt_ids[] = {
	{ .compatible = "allwinner,sun6i-a31-apb0-clk" },
	{ /* sentinel */ }
};

static struct driver_d sun6i_a31_apb0_clk_driver = {
	.name = "sun6i-a31-apb0-clk",
	.of_compatible = DRV_OF_COMPAT(sun6i_a31_apb0_clk_dt_ids),
	.probe = sun6i_a31_apb0_clk_probe,
};

static int sun6i_a31_apb0_clk_init(void)
{
	return platform_driver_register(&sun6i_a31_apb0_clk_driver);
}
postcore_initcall(sun6i_a31_apb0_clk_init);

MODULE_AUTHOR("Boris BREZILLON <boris.brezillon@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A31 APB0 clock Driver");
MODULE_LICENSE("GPL v2");
