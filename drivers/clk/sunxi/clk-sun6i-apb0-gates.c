/*
 * Copyright (C) 2014 Free Electrons
 *
 * License Terms: GNU General Public License v2
 * Author: Boris BREZILLON <boris.brezillon@free-electrons.com>
 *
 * Allwinner A31 APB0 clock gates driver
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

#define SUN6I_APB0_GATES_MAX_SIZE	32

struct gates_data {
	DECLARE_BITMAP(mask, SUN6I_APB0_GATES_MAX_SIZE);
};

static const struct gates_data sun6i_a31_apb0_gates __initconst = {
	.mask = {0x7F},
};

static const struct gates_data sun8i_a23_apb0_gates __initconst = {
	.mask = {0x5D},
};

static const struct of_device_id sun6i_a31_apb0_gates_clk_dt_ids[] = {
	{ .compatible = "allwinner,sun6i-a31-apb0-gates-clk", .data = &sun6i_a31_apb0_gates },
	{ .compatible = "allwinner,sun8i-a23-apb0-gates-clk", .data = &sun8i_a23_apb0_gates },
	{ /* sentinel */ }
};

static int sun6i_a31_apb0_gates_clk_probe(struct device_d *dev)
{
	struct device_node *np = dev->device_node;
	struct clk_onecell_data *clk_data;
	const struct gates_data *data;
	const char *clk_parent;
	const char *clk_name;
	void __iomem *reg;
	int ngates;
	int ret, i, j = 0;

	if (!np)
		return -ENODEV;

	ret = dev_get_drvdata(dev, (const void **)&data);
	if (ret)
		return ret;

	reg = dev_get_mem_region(dev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	clk_parent = of_clk_get_parent_name(np, 0);
	if (!clk_parent)
		return -EINVAL;

	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	/* Worst-case size approximation and memory allocation */
	ngates = find_last_bit(data->mask, SUN6I_APB0_GATES_MAX_SIZE);
	clk_data->clks = kzalloc((ngates + 1) * sizeof(struct clk *),
				 GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	for_each_set_bit(i, data->mask, SUN6I_APB0_GATES_MAX_SIZE) {
		of_property_read_string_index(np, "clock-output-names",
					      j, &clk_name);

		clk_data->clks[i] = clk_gate(clk_name, clk_parent,
					     reg, i, 0, 0);
		WARN_ON(IS_ERR(clk_data->clks[i]));
		clk_register_clkdev(clk_data->clks[i], clk_name, NULL);

		j++;
	}

	clk_data->clk_num = ngates + 1;

	return of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
}

static struct driver_d sun6i_a31_apb0_gates_clk_driver = {
	.name = "sun6i-a31-apb0-gates-clk",
	.of_compatible = DRV_OF_COMPAT(sun6i_a31_apb0_gates_clk_dt_ids),
	.probe = sun6i_a31_apb0_gates_clk_probe,
};

static int sun6i_a31_apb0_gates_clk_init(void)
{
	return platform_driver_register(&sun6i_a31_apb0_gates_clk_driver);
}
postcore_initcall(sun6i_a31_apb0_gates_clk_init);

MODULE_AUTHOR("Boris BREZILLON <boris.brezillon@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A31 APB0 gate clocks driver");
MODULE_LICENSE("GPL v2");
