/*
 * Copyright (C) 2014 Antoine Ténart
 *
 * Antoine Ténart <antoine.tenart@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __MACH_DEBUG_LL_H__
#define __MACH_DEBUG_LL_H__

#include <io.h>

#define UART_BASE	0x01c28000
#define UARTn_BASE(n)	(UART_BASE + ((n) * 0x400))

#define UART_THR	0x00
#define UART_LSR	0x14
#define   LSR_THRE	BIT(5)

#ifdef CONFIG_DEBUG_SUNXI_UART0
#define EARLY_UART	UARTn_BASE(0)
#else
#define EARLY_UART	UARTn_BASE(1)
#endif

static inline void PUTC_LL(char c)
{
	/* Wait until there is space in the FIFO */
	while (!(readl(EARLY_UART + UART_LSR) & LSR_THRE))
		;

	/* Send the character */
	writel(c, EARLY_UART + UART_THR);

	/* Wait to make sure it hits the line */
	while (!(readl(EARLY_UART + UART_LSR) & LSR_THRE))
		;
}
#endif
