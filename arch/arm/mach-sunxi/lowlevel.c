/*
 * Copyright (C) 2014 Antoine Ténart
 *
 * Antoine Ténart <antoine.tenart@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <common.h>

#define SUNXI_BOOTUP_MEMORY_BASE	0x40000000
#define SUNXI_BOOTUP_MEMORY_SIZE	SZ_64M

#if 0
#define CONSOLE_UART_BASE	UARTn_BASE(CONFIG_SUNXI_CONSOLE_UART)

static struct NS16550_plat uart_plat = {
	.shift	= 2,
};
#endif

void __naked __noreturn sunxi_barebox_entry(void *fdt)
{
	barebox_arm_entry(SUNXI_BOOTUP_MEMORY_BASE,
			  SUNXI_BOOTUP_MEMORY_SIZE, fdt);
}

void __naked barebox_arm_reset_vector(void)
{
	arm_cpu_lowlevel_init();
	sunxi_barebox_entry(NULL);
}
