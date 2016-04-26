/*
 * Copyright 2015 Chen-Yu Tsai
 *
 * Chen-Yu Tsai	<wens@csie.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <driver.h>
#include <init.h>
#include <io.h>
#include <of.h>
#include <printk.h>
#include <stdio.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/reset.h>
#include <linux/reset-controller.h>

#define SUN9I_MMC_WIDTH		4

#define SUN9I_MMC_GATE_BIT	16
#define SUN9I_MMC_RESET_BIT	18

struct sun9i_mmc_clk_data {
	void __iomem			*membase;
	struct clk			*clk;
	struct reset_control		*reset;
	struct clk_onecell_data		clk_data;
	struct reset_controller_dev	rcdev;
};

static int sun9i_mmc_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct sun9i_mmc_clk_data *data = container_of(rcdev,
						       struct sun9i_mmc_clk_data,
						       rcdev);
	void __iomem *reg = data->membase + SUN9I_MMC_WIDTH * id;
	u32 val;

	clk_enable(data->clk);

	val = readl(reg);
	writel(val & ~BIT(SUN9I_MMC_RESET_BIT), reg);

	clk_disable(data->clk);

	return 0;
}

static int sun9i_mmc_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct sun9i_mmc_clk_data *data = container_of(rcdev,
						       struct sun9i_mmc_clk_data,
						       rcdev);
	void __iomem *reg = data->membase + SUN9I_MMC_WIDTH * id;
	u32 val;

	clk_enable(data->clk);

	val = readl(reg);
	writel(val | BIT(SUN9I_MMC_RESET_BIT), reg);

	clk_disable(data->clk);

	return 0;
}

static struct reset_control_ops sun9i_mmc_reset_ops = {
	.assert		= sun9i_mmc_reset_assert,
	.deassert	= sun9i_mmc_reset_deassert,
};

static int sun9i_a80_mmc_config_clk_probe(struct device_d *dev)
{
	struct device_node *np = dev->device_node;
	struct sun9i_mmc_clk_data *data;
	struct clk_onecell_data *clk_data;
	const char *clk_name = np->name;
	const char *clk_parent;
	struct resource *r;
	int count, i, ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	r = dev_get_resource(dev, IORESOURCE_MEM, 0);
	/* one clock/reset pair per word */
	count = DIV_ROUND_UP((r->end - r->start + 1), SUN9I_MMC_WIDTH);
	data->membase = dev_get_mem_region(dev, 0);
	if (IS_ERR(data->membase))
		return PTR_ERR(data->membase);

	clk_data = &data->clk_data;
	clk_data->clk_num = count;
	clk_data->clks = kzalloc(count * sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	data->clk = clk_get(dev, NULL);
	if (IS_ERR(data->clk)) {
		dev_err(dev, "Could not get clock\n");
		return PTR_ERR(data->clk);
	}

	data->reset = reset_control_get(dev, NULL);
	if (IS_ERR(data->reset)) {
		dev_err(dev, "Could not get reset control\n");
		return PTR_ERR(data->reset);
	}

	ret = reset_control_deassert(data->reset);
	if (ret) {
		dev_err(dev, "Reset deassert err %d\n", ret);
		return ret;
	}

	clk_parent = __clk_get_name(data->clk);
	for (i = 0; i < count; i++) {
		of_property_read_string_index(np, "clock-output-names",
					      i, &clk_name);

		clk_data->clks[i] = clk_gate(clk_name, clk_parent,
					data->membase + SUN9I_MMC_WIDTH * i,
					SUN9I_MMC_GATE_BIT, 0, 0);

		if (IS_ERR(clk_data->clks[i])) {
			ret = PTR_ERR(clk_data->clks[i]);
			goto err_clk_register;
		}
	}

	ret = of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	if (ret)
		goto err_clk_provider;

	data->rcdev.nr_resets = count;
	data->rcdev.ops = &sun9i_mmc_reset_ops;
	data->rcdev.of_node = np;

	ret = reset_controller_register(&data->rcdev);
	if (ret)
		goto err_rc_reg;

	dev->priv = data;

	return 0;

err_rc_reg:
	of_clk_del_provider(np);

err_clk_provider:
err_clk_register:
	reset_control_assert(data->reset);

	return ret;
}

static void sun9i_a80_mmc_config_clk_remove(struct device_d *dev)
{
	struct device_node *np = dev->device_node;
	struct sun9i_mmc_clk_data *data = dev->priv;

	reset_controller_unregister(&data->rcdev);
	of_clk_del_provider(np);
	reset_control_assert(data->reset);
}

static __maybe_unused const struct of_device_id sun9i_a80_mmc_config_clk_dt_ids[] = {
	{ .compatible = "allwinner,sun9i-a80-mmc-config-clk" },
	{ /* sentinel */ }
};

static struct driver_d sun9i_a80_mmc_config_clk_driver = {
	.name = "sun9i-a80-mmc-config-clk",
	.of_compatible = DRV_OF_COMPAT(sun9i_a80_mmc_config_clk_dt_ids),
	.probe = sun9i_a80_mmc_config_clk_probe,
	.remove = sun9i_a80_mmc_config_clk_remove,
};

static int sun9i_a80_mmc_config_clk_init(void)
{
	return platform_driver_register(&sun9i_a80_mmc_config_clk_driver);
}
postcore_initcall(sun9i_a80_mmc_config_clk_init);

MODULE_AUTHOR("Chen-Yu Tsai <wens@csie.org>");
MODULE_DESCRIPTION("Allwinner A80 MMC clock/reset Driver");
MODULE_LICENSE("GPL v2");
