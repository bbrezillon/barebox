/*
 * Copyright 2013 Emilio López
 *
 * Emilio López <emilio@elopez.com.ar>
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

#include <of.h>
#include <of_address.h>
#include <malloc.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

#define SUNXI_OSC24M_GATE	0

static int __init sun4i_osc_clk_setup(struct device_node *node)
{
	struct clk *clk;
	struct clk *fixed;
	struct clk *gate;
	const char *clk_name = node->name;
	u32 rate;
	int ret;

	if (of_property_read_u32(node, "clock-frequency", &rate))
		return -EINVAL;

	of_property_read_string(node, "clock-output-names", &clk_name);

	/* allocate fixed-rate and gate clock structs */
	fixed = clk_fixed_alloc(clk_name, rate);
	if (!fixed)
		return -ENOMEM;


	gate = clk_gate_alloc(clk_name, NULL, of_iomap(node, 0),
			      SUNXI_OSC24M_GATE, 0, 0);
	if (!gate) {
		ret = -ENOMEM;
		goto err_free_fixed;
	}

	clk = clk_register_composite(clk_name, NULL, 0, NULL,
				    fixed, gate, 0);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto err_free_gate;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);
	clk_register_clkdev(clk, clk_name, NULL);

	return 0;

err_free_gate:
	kfree(gate);
err_free_fixed:
	kfree(fixed);
	return ret;
}
CLK_OF_DECLARE(sun4i_osc, "allwinner,sun4i-a10-osc-clk", sun4i_osc_clk_setup);
