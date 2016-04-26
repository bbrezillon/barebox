/*
 * Allwinner A1X SoCs pinctrl driver.
 *
 * Copyright (C) 2012 Maxime Ripard
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <driver.h>
#include <io.h>
#include <malloc.h>
#include <of.h>
#include <of_address.h>
#include <pinctrl.h>
#include <linux/clk.h>

#include "pinctrl-sunxi.h"

static const struct sunxi_desc_pin *
sunxi_pinctrl_desc_find_pin_by_name(struct sunxi_pinctrl *pctl,
				    const char *name)
{
	int i;

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct sunxi_desc_pin *pin = pctl->desc->pins + i;

		if (!strcmp(pin->pin.name, name))
			return pin;
	}

	return NULL;
}

static const struct sunxi_desc_function *
sunxi_pinctrl_desc_find_function_by_pin(struct sunxi_pinctrl *pctl,
					const u16 pin_num,
					const char *func_name)
{
	int i;

	for (i = 0; i < pctl->desc->npins; i++) {
		const struct sunxi_desc_pin *pin = pctl->desc->pins + i;

		if (pin->pin.number == pin_num) {
			const struct sunxi_desc_function *func =
							pin->functions;

			while (func->name) {
				if (!strcmp(func->name, func_name))
					return func;

				func++;
			}
		}
	}

	return NULL;
}

static void sunxi_pmx_set(struct sunxi_pinctrl *pctl, u16 pin, u8 config)
{
	u32 val, mask;

	pin -= pctl->desc->pin_base;
	val = readl(pctl->membase + sunxi_mux_reg(pin));
	mask = MUX_PINS_MASK << sunxi_mux_offset(pin);
	writel((val & ~mask) | config << sunxi_mux_offset(pin),
		pctl->membase + sunxi_mux_reg(pin));
}

static void sunxi_pconf_set_drive_strength(struct sunxi_pinctrl *pctl, u16 pin,
					   u16 strength)
{
	u32 val, mask;
	u8 dlevel;

	pin -= pctl->desc->pin_base;

	/*
	 * We convert from mA to what the register expects:
	 *   0: 10mA
	 *   1: 20mA
	 *   2: 30mA
	 *   3: 40mA
	 */
	dlevel = strength / 10 - 1;
	val = readl(pctl->membase + sunxi_dlevel_reg(pin));
	mask = DLEVEL_PINS_MASK << sunxi_dlevel_offset(pin);
	writel((val & ~mask) | dlevel << sunxi_dlevel_offset(pin),
	       pctl->membase + sunxi_dlevel_reg(pin));

}

static void sunxi_pconf_set_pullup(struct sunxi_pinctrl *pctl, u16 pin)
{
	u32 val, mask;

	pin -= pctl->desc->pin_base;

	val = readl(pctl->membase + sunxi_pull_reg(pin));
	mask = PULL_PINS_MASK << sunxi_pull_offset(pin);
	writel((val & ~mask) | 1 << sunxi_pull_offset(pin),
	       pctl->membase + sunxi_pull_reg(pin));
}

static void sunxi_pconf_set_pulldown(struct sunxi_pinctrl *pctl, u16 pin)
{
	u32 val, mask;

	pin -= pctl->desc->pin_base;

	val = readl(pctl->membase + sunxi_pull_reg(pin));
	mask = PULL_PINS_MASK << sunxi_pull_offset(pin);
	writel((val & ~mask) | 2 << sunxi_pull_offset(pin),
	       pctl->membase + sunxi_pull_reg(pin));
}

static int sunxi_pctrl_set_state(struct pinctrl_device *pctldev,
				 struct device_node *node)
{
	struct sunxi_pinctrl *pctl = to_sunxi_pinctrl(pctldev);
	struct property *prop;
	const char *function;
	const char *group;
	int ret;
	u32 val;

	ret = of_property_read_string(node, "allwinner,function", &function);
	if (ret) {
		dev_err(pctldev->dev,
			"missing allwinner,function property in node %s\n",
			node->name);
		return -EINVAL;
	}

	of_property_for_each_string(node, "allwinner,pins", prop, group) {
		const struct sunxi_desc_pin *pdesc =
			sunxi_pinctrl_desc_find_pin_by_name(pctl, group);
		const struct sunxi_desc_function *fdesc;

		if (!pdesc) {
			dev_err(pctldev->dev, "unknown pin %s", group);
			continue;
		}

		fdesc = sunxi_pinctrl_desc_find_function_by_pin(pctl,
							pdesc->pin.number,
							function);
		if (!fdesc) {
			dev_err(pctldev->dev, "unsupported function %s on pin %s",
				function, group);
			continue;
		}

		sunxi_pmx_set(pctl, pdesc->pin.number, fdesc->muxval);

		if (!of_property_read_u32(node, "allwinner,drive", &val)) {
			u16 strength = (val + 1) * 10;
			sunxi_pconf_set_drive_strength(pctl,
						       pdesc->pin.number,
						       strength);
		}

		if (!of_property_read_u32(node, "allwinner,pull", &val)) {
			if (val == 1)
				sunxi_pconf_set_pullup(pctl,
						       pdesc->pin.number);
			else if (val == 2)
				sunxi_pconf_set_pulldown(pctl,
							 pdesc->pin.number);
		}
	}

	return 0;
}

static struct pinctrl_ops sunxi_pctrl_ops = {
	.set_state = sunxi_pctrl_set_state,
};

int sunxi_pinctrl_init(struct device_d *dev,
		       const struct sunxi_pinctrl_desc *desc)
{
	struct sunxi_pinctrl *pctl;
	int ret;

	pctl = kzalloc(sizeof(*pctl), GFP_KERNEL);
	if (!pctl)
		return -ENOMEM;

	dev->priv = pctl;

	pctl->membase = dev_get_mem_region(dev, 0);
	if (IS_ERR(pctl->membase))
		return PTR_ERR(pctl->membase);

	pctl->desc = desc;
	pctl->pctl_dev.dev = dev;
	pctl->pctl_dev.ops = &sunxi_pctrl_ops;

	pctl->clk = clk_get(dev, NULL);
	if (IS_ERR(pctl->clk)) {
		ret = PTR_ERR(pctl->clk);
		goto free_pctl_error;
	}

	ret = clk_enable(pctl->clk);
	if (ret)
		goto free_pctl_error;

	ret = pinctrl_register(&pctl->pctl_dev);
	if (ret) {
		dev_err(dev, "couldn't register pinctrl driver\n");
		goto disable_clk_error;
	}

	dev_info(dev, "initialized sunXi PIO driver\n");

	return 0;

disable_clk_error:
	clk_disable(pctl->clk);
free_pctl_error:
	kfree(pctl);
	return ret;
}
