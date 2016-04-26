/*
 * Allwinner SoCs SRAM Controller Driver
 *
 * Copyright (C) 2015 Maxime Ripard
 *
 * Author: Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <driver.h>
#include <io.h>
#include <init.h>
#include <of.h>
#include <of_address.h>

#include <linux/spinlock.h>
#include <linux/soc/sunxi/sunxi_sram.h>

struct sunxi_sram_func {
	char	*func;
	u8	val;
};

struct sunxi_sram_data {
	char			*name;
	u8			reg;
	u8			offset;
	u8			width;
	struct sunxi_sram_func	*func;
	struct list_head	list;
};

struct sunxi_sram_desc {
	struct sunxi_sram_data	data;
	bool			claimed;
};

#define SUNXI_SRAM_MAP(_val, _func)				\
	{							\
		.func = _func,					\
		.val = _val,					\
	}

#define SUNXI_SRAM_DATA(_name, _reg, _off, _width, ...)		\
	{							\
		.name = _name,					\
		.reg = _reg,					\
		.offset = _off,					\
		.width = _width,				\
		.func = (struct sunxi_sram_func[]){		\
			__VA_ARGS__, { } },			\
	}

static struct sunxi_sram_desc sun4i_a10_sram_a3_a4 = {
	.data	= SUNXI_SRAM_DATA("A3-A4", 0x4, 0x4, 2,
				  SUNXI_SRAM_MAP(0, "cpu"),
				  SUNXI_SRAM_MAP(1, "emac")),
};

static struct sunxi_sram_desc sun4i_a10_sram_d = {
	.data	= SUNXI_SRAM_DATA("D", 0x4, 0x0, 1,
				  SUNXI_SRAM_MAP(0, "cpu"),
				  SUNXI_SRAM_MAP(1, "usb-otg")),
};

static const struct of_device_id sunxi_sram_dt_ids[] = {
	{
		.compatible	= "allwinner,sun4i-a10-sram-a3-a4",
		.data		= &sun4i_a10_sram_a3_a4.data,
	},
	{
		.compatible	= "allwinner,sun4i-a10-sram-d",
		.data		= &sun4i_a10_sram_d.data,
	},
	{}
};

static struct device_d *sram_dev;
static LIST_HEAD(claimed_sram);
static void __iomem *base;

static inline struct sunxi_sram_desc *to_sram_desc(const struct sunxi_sram_data *data)
{
	return container_of(data, struct sunxi_sram_desc, data);
}

static const struct sunxi_sram_data *sunxi_sram_of_parse(struct device_node *node,
							 unsigned int *value)
{
	const struct of_device_id *match;
	struct of_phandle_args args;
	int ret;

	ret = of_parse_phandle_with_fixed_args(node, "allwinner,sram", 1, 0,
					       &args);
	if (ret)
		return ERR_PTR(ret);

	if (!of_device_is_available(args.np)) {
		ret = -EBUSY;
		goto err;
	}

	if (value)
		*value = args.args[0];

	match = of_match_node(sunxi_sram_dt_ids, args.np);
	if (!match) {
		ret = -EINVAL;
		goto err;
	}

	return match->data;

err:
	return ERR_PTR(ret);
}

int sunxi_sram_claim(struct device_d *dev)
{
	const struct sunxi_sram_data *sram_data;
	struct sunxi_sram_desc *sram_desc;
	unsigned int device;
	u32 val, mask;

	if (IS_ERR(base))
		return -EPROBE_DEFER;

	if (!dev || !dev->device_node)
		return -EINVAL;

	sram_data = sunxi_sram_of_parse(dev->device_node, &device);
	if (IS_ERR(sram_data))
		return PTR_ERR(sram_data);

	sram_desc = to_sram_desc(sram_data);

	spin_lock(&sram_lock);

	if (sram_desc->claimed) {
		spin_unlock(&sram_lock);
		return -EBUSY;
	}

	mask = ((1 << sram_data->width) - 1) << sram_data->offset;
	val = readl(base + sram_data->reg);
	val &= ~mask;
	writel(val | ((device << sram_data->offset) & mask),
	       base + sram_data->reg);

	spin_unlock(&sram_lock);

	return 0;
}
EXPORT_SYMBOL(sunxi_sram_claim);

int sunxi_sram_release(struct device_d *dev)
{
	const struct sunxi_sram_data *sram_data;
	struct sunxi_sram_desc *sram_desc;

	if (!dev || !dev->device_node)
		return -EINVAL;

	sram_data = sunxi_sram_of_parse(dev->device_node, NULL);
	if (IS_ERR(sram_data))
		return -EINVAL;

	sram_desc = to_sram_desc(sram_data);

	sram_desc->claimed = false;

	return 0;
}
EXPORT_SYMBOL(sunxi_sram_release);

static int sunxi_sram_probe(struct device_d *dev)
{
	sram_dev = dev;

	base = dev_get_mem_region(dev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	of_platform_populate(dev->device_node, NULL, dev);

	return 0;
}

static const struct of_device_id sunxi_sram_dt_match[] = {
	{ .compatible = "allwinner,sun4i-a10-sram-controller" },
	{ },
};

static struct driver_d sunxi_sram_driver = {
	.name		= "sunxi-sram",
	.of_compatible	= DRV_OF_COMPAT(sunxi_sram_dt_match),
	.probe		= sunxi_sram_probe,
};

static int sunxi_sram_init(void)
{
	return platform_driver_register(&sunxi_sram_driver);
}
postcore_initcall(sunxi_sram_init);

MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner sunXi SRAM Controller Driver");
MODULE_LICENSE("GPL");
