/*
 * Copyright 2013 Emilio López
 * Emilio López <emilio@elopez.com.ar>
 *
 * Copyright 2013 Chen-Yu Tsai
 * Chen-Yu Tsai <wens@csie.org>
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

#include <malloc.h>
#include <of.h>
#include <of_address.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

/**
 * sun7i_a20_gmac_clk_setup - Setup function for A20/A31 GMAC clock module
 *
 * This clock looks something like this
 *                               ________________________
 *  MII TX clock from PHY >-----|___________    _________|----> to GMAC core
 *  GMAC Int. RGMII TX clk >----|___________\__/__gate---|----> to PHY
 *  Ext. 125MHz RGMII TX clk >--|__divider__/            |
 *                              |________________________|
 *
 * The external 125 MHz reference is optional, i.e. GMAC can use its
 * internal TX clock just fine. The A31 GMAC clock module does not have
 * the divider controls for the external reference.
 *
 * To keep it simple, let the GMAC use either the MII TX clock for MII mode,
 * and its internal TX clock for GMII and RGMII modes. The GMAC driver should
 * select the appropriate source and gate/ungate the output to the PHY.
 *
 * Only the GMAC should use this clock. Altering the clock so that it doesn't
 * match the GMAC's operation parameters will result in the GMAC not being
 * able to send traffic out. The GMAC driver should set the clock rate and
 * enable/disable this clock to configure the required state. The clock
 * driver then responds by auto-reparenting the clock.
 */

#define SUN7I_A20_GMAC_GPIT	2
#define SUN7I_A20_GMAC_MASK	0x3
#define SUN7I_A20_GMAC_PARENTS	2

static u32 sun7i_a20_gmac_mux_table[SUN7I_A20_GMAC_PARENTS] = {
	0x00, /* Select mii_phy_tx_clk */
	0x02, /* Select gmac_int_tx_clk */
};

static int __init sun7i_a20_gmac_clk_setup(struct device_node *node)
{
	struct clk *clk;
	struct clk *mux;
	struct clk *gate;
	const char *clk_name = node->name;
	const char *parents[SUN7I_A20_GMAC_PARENTS];
	void __iomem *reg;
	int ret;

	if (of_property_read_string(node, "clock-output-names", &clk_name))
		return -EINVAL;

	reg = of_iomap(node, 0);
	if (!reg)
		return -ENOMEM;

	/* gmac clock requires exactly 2 parents */
	parents[0] = of_clk_get_parent_name(node, 0);
	parents[1] = of_clk_get_parent_name(node, 1);
	if (!parents[0] || !parents[1])
		return -EINVAL;

	/* allocate mux and gate clock structs */
	mux = clk_mux_table_alloc(clk_name, reg, 0, SUN7I_A20_GMAC_MASK + 1,
			    parents, SUN7I_A20_GMAC_PARENTS,
			    sun7i_a20_gmac_mux_table, 0);
	if (!mux)
		return -ENOMEM;

	gate = clk_gate_alloc(clk_name, NULL, reg, SUN7I_A20_GMAC_GPIT,
			      0, 0);
	if (!gate) {
		ret = -ENOMEM;
		goto free_mux;
	}

	clk = clk_register_composite(clk_name, parents, SUN7I_A20_GMAC_PARENTS,
			mux, NULL, gate, 0);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto free_gate;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);
	clk_register_clkdev(clk, clk_name, NULL);

	return 0;

free_gate:
	kfree(gate);
free_mux:
	kfree(mux);
	return ret;
}
CLK_OF_DECLARE(sun7i_a20_gmac, "allwinner,sun7i-a20-gmac-clk",
		sun7i_a20_gmac_clk_setup);
