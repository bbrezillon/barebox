/*
 * Copyright (C) 2013 Emilio López <emilio@elopez.com.ar>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable factor-based clock implementation
 */

#include <common.h>
#include <io.h>
#include <malloc.h>
#include <of_address.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/string.h>

#include "clk-factors.h"

/*
 * DOC: basic adjustable factor-based clock
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is adjustable.
 *        clk->rate = (parent->rate * N * (K + 1) >> P) / (M + 1)
 * parent - fixed parent.  No clk_set_parent support
 */

#define to_clk_factors(_hw) container_of(_hw, struct clk_factors, hw)

#define FACTORS_MAX_PARENTS		5

#define SETMASK(len, pos)		(((1U << (len)) - 1) << (pos))
#define CLRMASK(len, pos)		(~(SETMASK(len, pos)))
#define FACTOR_GET(bit, len, reg)	(((reg) & SETMASK(len, bit)) >> (bit))

#define FACTOR_SET(bit, len, reg, val) \
	(((reg) & CLRMASK(len, bit)) | (val << (bit)))

static unsigned long clk_factors_recalc_rate(struct clk *hw,
					     unsigned long parent_rate)
{
	u8 n = 1, k = 0, p = 0, m = 0;
	u32 reg;
	unsigned long rate;
	struct clk_factors *factors = to_clk_factors(hw);
	struct clk_factors_config *config = factors->config;

	/* Fetch the register value */
	reg = readl(factors->reg);

	/* Get each individual factor if applicable */
	if (config->nwidth != SUNXI_FACTORS_NOT_APPLICABLE)
		n = FACTOR_GET(config->nshift, config->nwidth, reg);
	if (config->kwidth != SUNXI_FACTORS_NOT_APPLICABLE)
		k = FACTOR_GET(config->kshift, config->kwidth, reg);
	if (config->mwidth != SUNXI_FACTORS_NOT_APPLICABLE)
		m = FACTOR_GET(config->mshift, config->mwidth, reg);
	if (config->pwidth != SUNXI_FACTORS_NOT_APPLICABLE)
		p = FACTOR_GET(config->pshift, config->pwidth, reg);

	/* Calculate the rate */
	rate = (parent_rate * (n + config->n_start) * (k + 1) >> p) / (m + 1);

	return rate;
}

static long clk_factors_round_rate(struct clk *hw, unsigned long rate,
				   unsigned long *parent_rate)
{
	struct clk_factors *factors = to_clk_factors(hw);
	factors->get_factors((u32 *)&rate, (u32)*parent_rate,
			     NULL, NULL, NULL, NULL);

	return rate;
}

static int clk_factors_set_rate(struct clk *hw, unsigned long rate,
				unsigned long parent_rate)
{
	u8 n = 0, k = 0, m = 0, p = 0;
	u32 reg;
	struct clk_factors *factors = to_clk_factors(hw);
	struct clk_factors_config *config = factors->config;

	factors->get_factors((u32 *)&rate, (u32)parent_rate, &n, &k, &m, &p);

	/* Fetch the register value */
	reg = readl(factors->reg);

	/* Set up the new factors - macros do not do anything if width is 0 */
	reg = FACTOR_SET(config->nshift, config->nwidth, reg, n);
	reg = FACTOR_SET(config->kshift, config->kwidth, reg, k);
	reg = FACTOR_SET(config->mshift, config->mwidth, reg, m);
	reg = FACTOR_SET(config->pshift, config->pwidth, reg, p);

	/* Apply them now */
	writel(reg, factors->reg);

	/* delay 500us so pll stabilizes */
	udelay(500);

	return 0;
}

static const struct clk_ops clk_factors_ops = {
	.recalc_rate = clk_factors_recalc_rate,
	.round_rate = clk_factors_round_rate,
	.set_rate = clk_factors_set_rate,
};

struct clk *sunxi_factors_register(struct device_node *node,
				   const struct factors_data *data,
				   void __iomem *reg)
{
	struct clk *clk;
	struct clk_factors *factors;
	struct clk *gate = NULL;
	struct clk *mux = NULL;
	const char *clk_name = node->name;
	const char *parents[FACTORS_MAX_PARENTS];
	int i = 0;

	/* if we have a mux, we will have >1 parents */
	while (i < FACTORS_MAX_PARENTS &&
	       (parents[i] = of_clk_get_parent_name(node, i)) != NULL)
		i++;

	/*
	 * some factor clocks, such as pll5 and pll6, may have multiple
	 * outputs, and have their name designated in factors_data
	 */
	if (data->name)
		clk_name = data->name;
	else
		of_property_read_string(node, "clock-output-names", &clk_name);

	factors = kzalloc(sizeof(struct clk_factors), GFP_KERNEL);
	if (!factors)
		return NULL;

	/* set up factors properties */
	factors->reg = reg;
	factors->config = data->table;
	factors->get_factors = data->getter;

	/* Add a gate if this factor clock can be gated */
	if (data->enable) {
		gate = clk_gate_alloc(clk_name, NULL, reg,
				      data->enable, 0, 0);
		if (!gate) {
			kfree(factors);
			return NULL;
		}
	}

	/* Add a mux if this factor clock can be muxed */
	if (data->mux) {
		mux = clk_mux_alloc(clk_name, reg, data->mux,
				    data->muxmask + 1, parents, i, 0);
		if (!mux) {
			kfree(factors);
			kfree(gate);
			return NULL;
		}
	}

	clk = clk_register_composite(clk_name, parents, i, mux,
				     &factors->hw, gate, 0);

	if (!clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, NULL);
	}

	return clk;
}
