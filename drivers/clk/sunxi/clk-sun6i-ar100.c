/*
 * Copyright (C) 2014 Free Electrons
 *
 * License Terms: GNU General Public License v2
 * Author: Boris BREZILLON <boris.brezillon@free-electrons.com>
 *
 * Allwinner A31 AR100 clock driver
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

#define SUN6I_AR100_MAX_PARENTS		4
#define SUN6I_AR100_SHIFT_MASK		0x3
#define SUN6I_AR100_SHIFT_MAX		SUN6I_AR100_SHIFT_MASK
#define SUN6I_AR100_SHIFT_SHIFT		4
#define SUN6I_AR100_DIV_MASK		0x1f
#define SUN6I_AR100_DIV_MAX		(SUN6I_AR100_DIV_MASK + 1)
#define SUN6I_AR100_DIV_SHIFT		8
#define SUN6I_AR100_MUX_MASK		0x3
#define SUN6I_AR100_MUX_SHIFT		16

struct ar100_clk {
	struct clk hw;
	void __iomem *reg;
};

static inline struct ar100_clk *to_ar100_clk(struct clk *hw)
{
	return container_of(hw, struct ar100_clk, hw);
}

static unsigned long ar100_recalc_rate(struct clk *hw,
				       unsigned long parent_rate)
{
	struct ar100_clk *clk = to_ar100_clk(hw);
	u32 val = readl(clk->reg);
	int shift = (val >> SUN6I_AR100_SHIFT_SHIFT) & SUN6I_AR100_SHIFT_MASK;
	int div = (val >> SUN6I_AR100_DIV_SHIFT) & SUN6I_AR100_DIV_MASK;

	return (parent_rate >> shift) / (div + 1);
}

static long ar100_round_rate(struct clk *clk, unsigned long rate,
			     unsigned long *parent_rate)
{
	unsigned long div, shift;

	div = DIV_ROUND_UP(*parent_rate, rate);

	/*
	 * The AR100 clk contains 2 divisors:
	 * - one power of 2 divisor
	 * - one regular divisor
	 *
	 * First check if we can safely shift (or divide by a power
	 * of 2) without losing precision on the requested rate.
	 */
	shift = ffs(div) - 1;
	if (shift > SUN6I_AR100_SHIFT_MAX)
		shift = SUN6I_AR100_SHIFT_MAX;

	div >>= shift;

	/*
	 * Then if the divisor is still bigger than what the HW
	 * actually supports, use a bigger shift (or power of 2
	 * divider) value and accept to lose some precision.
	 */
	while (div > SUN6I_AR100_DIV_MAX && shift < SUN6I_AR100_SHIFT_MAX) {
		shift++;
		div >>= 1;
	}

	return (*parent_rate >> shift) / div;
}

static int ar100_set_parent(struct clk *hw, u8 index)
{
	struct ar100_clk *clk = to_ar100_clk(hw);
	u32 val = readl(clk->reg);

	if (index >= SUN6I_AR100_MAX_PARENTS)
		return -EINVAL;

	val &= ~(SUN6I_AR100_MUX_MASK << SUN6I_AR100_MUX_SHIFT);
	val |= (index << SUN6I_AR100_MUX_SHIFT);
	writel(val, clk->reg);

	return 0;
}

static int ar100_get_parent(struct clk *hw)
{
	struct ar100_clk *clk = to_ar100_clk(hw);
	return (readl(clk->reg) >> SUN6I_AR100_MUX_SHIFT) &
	       SUN6I_AR100_MUX_MASK;
}

static int ar100_set_rate(struct clk *hw, unsigned long rate,
			  unsigned long parent_rate)
{
	unsigned long div = parent_rate / rate;
	struct ar100_clk *clk = to_ar100_clk(hw);
	u32 val = readl(clk->reg);
	int shift;

	if (parent_rate % rate)
		return -EINVAL;

	shift = ffs(div) - 1;
	if (shift > SUN6I_AR100_SHIFT_MAX)
		shift = SUN6I_AR100_SHIFT_MAX;

	div >>= shift;

	if (div > SUN6I_AR100_DIV_MAX)
		return -EINVAL;

	val &= ~((SUN6I_AR100_SHIFT_MASK << SUN6I_AR100_SHIFT_SHIFT) |
		 (SUN6I_AR100_DIV_MASK << SUN6I_AR100_DIV_SHIFT));
	val |= (shift << SUN6I_AR100_SHIFT_SHIFT) |
	       (div << SUN6I_AR100_DIV_SHIFT);
	writel(val, clk->reg);

	return 0;
}

static struct clk_ops ar100_ops = {
	.recalc_rate = ar100_recalc_rate,
	.round_rate = ar100_round_rate,
	.set_parent = ar100_set_parent,
	.get_parent = ar100_get_parent,
	.set_rate = ar100_set_rate,
};

static int sun6i_a31_ar100_clk_probe(struct device_d *dev)
{
	const char *parents[SUN6I_AR100_MAX_PARENTS];
	struct device_node *np = dev->device_node;
	const char *clk_name = np->name;
	struct ar100_clk *ar100;
	int nparents;
	int i, ret;

	ar100 = kzalloc(sizeof(*ar100), GFP_KERNEL);
	if (!ar100)
		return -ENOMEM;

	ar100->reg = dev_get_mem_region(dev, 0);
	if (IS_ERR(ar100->reg)) {
		ret = PTR_ERR(ar100->reg);
		goto error_free_ar100;
	}

	nparents = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (nparents > SUN6I_AR100_MAX_PARENTS)
		nparents = SUN6I_AR100_MAX_PARENTS;

	for (i = 0; i < nparents; i++)
		parents[i] = of_clk_get_parent_name(np, i);

	of_property_read_string(np, "clock-output-names", &clk_name);

	ar100->hw.name = clk_name;
	ar100->hw.ops = &ar100_ops;
	ar100->hw.parent_names = parents;
	ar100->hw.num_parents = nparents;

	ret = clk_register(&ar100->hw);
	if (ret)
		goto error_free_ar100;

	return of_clk_add_provider(np, of_clk_src_simple_get, &ar100->hw);

error_free_ar100:
	kfree(ar100);
	return ret;
}

static const struct of_device_id sun6i_a31_ar100_clk_dt_ids[] = {
	{ .compatible = "allwinner,sun6i-a31-ar100-clk" },
	{ /* sentinel */ }
};

static struct driver_d sun6i_a31_ar100_clk_driver = {
	.name = "sun6i-a31-ar100-clk",
	.of_compatible = DRV_OF_COMPAT(sun6i_a31_ar100_clk_dt_ids),
	.probe = sun6i_a31_ar100_clk_probe,
};

static int sun6i_a31_ar100_clk_init(void)
{
	return platform_driver_register(&sun6i_a31_ar100_clk_driver);
}
postcore_initcall(sun6i_a31_ar100_clk_init);


MODULE_AUTHOR("Boris BREZILLON <boris.brezillon@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A31 AR100 clock Driver");
MODULE_LICENSE("GPL v2");
