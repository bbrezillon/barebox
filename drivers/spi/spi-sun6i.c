/*
 * Copyright (C) 2012 - 2014 Allwinner Tech
 * Pan Nan <pannan@allwinnertech.com>
 *
 * Copyright (C) 2014 Maxime Ripard
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <clock.h>
#include <driver.h>
#include <init.h>
#include <io.h>
#include <of.h>
#include <malloc.h>
#include <printk.h>
#include <spi/spi.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define SUN6I_FIFO_DEPTH		128

#define SUN6I_GBL_CTL_REG		0x04
#define SUN6I_GBL_CTL_BUS_ENABLE		BIT(0)
#define SUN6I_GBL_CTL_MASTER			BIT(1)
#define SUN6I_GBL_CTL_TP			BIT(7)
#define SUN6I_GBL_CTL_RST			BIT(31)

#define SUN6I_TFR_CTL_REG		0x08
#define SUN6I_TFR_CTL_CPHA			BIT(0)
#define SUN6I_TFR_CTL_CPOL			BIT(1)
#define SUN6I_TFR_CTL_SPOL			BIT(2)
#define SUN6I_TFR_CTL_CS_MASK			0x30
#define SUN6I_TFR_CTL_CS(cs)			(((cs) << 4) & SUN6I_TFR_CTL_CS_MASK)
#define SUN6I_TFR_CTL_CS_MANUAL			BIT(6)
#define SUN6I_TFR_CTL_CS_LEVEL			BIT(7)
#define SUN6I_TFR_CTL_DHB			BIT(8)
#define SUN6I_TFR_CTL_FBS			BIT(12)
#define SUN6I_TFR_CTL_XCH			BIT(31)

#define SUN6I_INT_CTL_REG		0x10
#define SUN6I_INT_CTL_RF_OVF			BIT(8)
#define SUN6I_INT_CTL_TC			BIT(12)

#define SUN6I_INT_STA_REG		0x14

#define SUN6I_FIFO_CTL_REG		0x18
#define SUN6I_FIFO_CTL_RF_RST			BIT(15)
#define SUN6I_FIFO_CTL_TF_RST			BIT(31)

#define SUN6I_FIFO_STA_REG		0x1c
#define SUN6I_FIFO_STA_RF_CNT_MASK		0x7f
#define SUN6I_FIFO_STA_RF_CNT_BITS		0
#define SUN6I_FIFO_STA_TF_CNT_MASK		0x7f
#define SUN6I_FIFO_STA_TF_CNT_BITS		16

#define SUN6I_CLK_CTL_REG		0x24
#define SUN6I_CLK_CTL_CDR2_MASK			0xff
#define SUN6I_CLK_CTL_CDR2(div)			(((div) & SUN6I_CLK_CTL_CDR2_MASK) << 0)
#define SUN6I_CLK_CTL_CDR1_MASK			0xf
#define SUN6I_CLK_CTL_CDR1(div)			(((div) & SUN6I_CLK_CTL_CDR1_MASK) << 8)
#define SUN6I_CLK_CTL_DRS			BIT(12)

#define SUN6I_BURST_CNT_REG		0x30
#define SUN6I_BURST_CNT(cnt)			((cnt) & 0xffffff)

#define SUN6I_XMIT_CNT_REG		0x34
#define SUN6I_XMIT_CNT(cnt)			((cnt) & 0xffffff)

#define SUN6I_BURST_CTL_CNT_REG		0x38
#define SUN6I_BURST_CTL_CNT_STC(cnt)		((cnt) & 0xffffff)

#define SUN6I_TXDATA_REG		0x200
#define SUN6I_RXDATA_REG		0x300

struct sun6i_spi {
	struct spi_master	master;
	void __iomem		*base_addr;
	struct clk		*hclk;
	struct clk		*mclk;
	struct reset_control	*rstc;

	const u8		*tx_buf;
	u8			*rx_buf;
	int			len;
};

static inline struct sun6i_spi *to_sun6i_spi(struct spi_master *master)
{
	return container_of(master, struct sun6i_spi, master);
}

static inline u32 sun6i_spi_read(struct sun6i_spi *sspi, u32 reg)
{
	return readl(sspi->base_addr + reg);
}

static inline void sun6i_spi_write(struct sun6i_spi *sspi, u32 reg, u32 value)
{
	writel(value, sspi->base_addr + reg);
}

static inline void sun6i_spi_drain_fifo(struct sun6i_spi *sspi, int len)
{
	u32 reg, cnt;
	u8 byte;

	/* See how much data is available */
	reg = sun6i_spi_read(sspi, SUN6I_FIFO_STA_REG);
	reg &= SUN6I_FIFO_STA_RF_CNT_MASK;
	cnt = reg >> SUN6I_FIFO_STA_RF_CNT_BITS;

	if (len > cnt)
		len = cnt;

	while (len--) {
		byte = readb(sspi->base_addr + SUN6I_RXDATA_REG);
		if (sspi->rx_buf)
			*sspi->rx_buf++ = byte;
	}
}

static inline void sun6i_spi_fill_fifo(struct sun6i_spi *sspi, int len)
{
	u8 byte;

	if (len > sspi->len)
		len = sspi->len;

	while (len--) {
		byte = sspi->tx_buf ? *sspi->tx_buf++ : 0;
		writeb(byte, sspi->base_addr + SUN6I_TXDATA_REG);
		sspi->len--;
	}
}

static void sun6i_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct sun6i_spi *sspi = to_sun6i_spi(spi->master);
	u32 reg;

	reg = sun6i_spi_read(sspi, SUN6I_TFR_CTL_REG);
	reg &= ~SUN6I_TFR_CTL_CS_MASK;
	reg |= SUN6I_TFR_CTL_CS(spi->chip_select);

	if (enable)
		reg |= SUN6I_TFR_CTL_CS_LEVEL;
	else
		reg &= ~SUN6I_TFR_CTL_CS_LEVEL;

	sun6i_spi_write(sspi, SUN6I_TFR_CTL_REG, reg);
}


static int sun6i_spi_transfer_one(struct spi_master *master,
				  struct spi_device *spi,
				  struct spi_transfer *tfr)
{
	struct sun6i_spi *sspi = to_sun6i_spi(master);
	unsigned int mclk_rate, div;
	unsigned int tx_len = 0;
	int ret = 0;
	u32 reg;

	/* We don't support transfer larger than the FIFO */
	if (tfr->len > SUN6I_FIFO_DEPTH)
		return -EINVAL;

	sspi->tx_buf = tfr->tx_buf;
	sspi->rx_buf = tfr->rx_buf;
	sspi->len = tfr->len;

	/* Clear pending interrupts */
	sun6i_spi_write(sspi, SUN6I_INT_STA_REG, ~0);

	/* Reset FIFO */
	sun6i_spi_write(sspi, SUN6I_FIFO_CTL_REG,
			SUN6I_FIFO_CTL_RF_RST | SUN6I_FIFO_CTL_TF_RST);

	/*
	 * Setup the transfer control register: Chip Select,
	 * polarities, etc.
	 */
	reg = sun6i_spi_read(sspi, SUN6I_TFR_CTL_REG);

	if (spi->mode & SPI_CPOL)
		reg |= SUN6I_TFR_CTL_CPOL;
	else
		reg &= ~SUN6I_TFR_CTL_CPOL;

	if (spi->mode & SPI_CPHA)
		reg |= SUN6I_TFR_CTL_CPHA;
	else
		reg &= ~SUN6I_TFR_CTL_CPHA;

	if (spi->mode & SPI_LSB_FIRST)
		reg |= SUN6I_TFR_CTL_FBS;
	else
		reg &= ~SUN6I_TFR_CTL_FBS;

	/*
	 * If it's a TX only transfer, we don't want to fill the RX
	 * FIFO with bogus data
	 */
	if (sspi->rx_buf)
		reg &= ~SUN6I_TFR_CTL_DHB;
	else
		reg |= SUN6I_TFR_CTL_DHB;

	/* We want to control the chip select manually */
	reg |= SUN6I_TFR_CTL_CS_MANUAL;

	sun6i_spi_write(sspi, SUN6I_TFR_CTL_REG, reg);

	/* Ensure that we have a parent clock fast enough */
	mclk_rate = clk_get_rate(sspi->mclk);
	if (mclk_rate < (2 * spi->max_speed_hz)) {
		clk_set_rate(sspi->mclk, 2 * spi->max_speed_hz);
		mclk_rate = clk_get_rate(sspi->mclk);
	}

	/*
	 * Setup clock divider.
	 *
	 * We have two choices there. Either we can use the clock
	 * divide rate 1, which is calculated thanks to this formula:
	 * SPI_CLK = MOD_CLK / (2 ^ cdr)
	 * Or we can use CDR2, which is calculated with the formula:
	 * SPI_CLK = MOD_CLK / (2 * (cdr + 1))
	 * Wether we use the former or the latter is set through the
	 * DRS bit.
	 *
	 * First try CDR2, and if we can't reach the expected
	 * frequency, fall back to CDR1.
	 */
	div = mclk_rate / (2 * spi->max_speed_hz);
	if (div <= (SUN6I_CLK_CTL_CDR2_MASK + 1)) {
		if (div > 0)
			div--;

		reg = SUN6I_CLK_CTL_CDR2(div) | SUN6I_CLK_CTL_DRS;
	} else {
		div = ilog2(mclk_rate) - ilog2(spi->max_speed_hz);
		reg = SUN6I_CLK_CTL_CDR1(div);
	}

	sun6i_spi_write(sspi, SUN6I_CLK_CTL_REG, reg);

	/* Setup the transfer now... */
	if (sspi->tx_buf)
		tx_len = tfr->len;

	/* Setup the counters */
	sun6i_spi_write(sspi, SUN6I_BURST_CNT_REG, SUN6I_BURST_CNT(tfr->len));
	sun6i_spi_write(sspi, SUN6I_XMIT_CNT_REG, SUN6I_XMIT_CNT(tx_len));
	sun6i_spi_write(sspi, SUN6I_BURST_CTL_CNT_REG,
			SUN6I_BURST_CTL_CNT_STC(tx_len));

	/* Fill the TX FIFO */
	sun6i_spi_fill_fifo(sspi, SUN6I_FIFO_DEPTH);

	/* Enable the interrupts */
	sun6i_spi_write(sspi, SUN6I_INT_CTL_REG, SUN6I_INT_CTL_TC);

	/* Start the transfer */
	reg = sun6i_spi_read(sspi, SUN6I_TFR_CTL_REG);
	sun6i_spi_write(sspi, SUN6I_TFR_CTL_REG, reg | SUN6I_TFR_CTL_XCH);

	ret = wait_on_timeout(SECOND,
			      sun6i_spi_read(sspi, SUN6I_INT_STA_REG) &
			      SUN6I_INT_CTL_TC);
	if (ret)
		goto out;

	sun6i_spi_drain_fifo(sspi, SUN6I_FIFO_DEPTH);

out:
	sun6i_spi_write(sspi, SUN6I_INT_CTL_REG, 0);

	return ret;
}

static int sun6i_spi_transfer(struct spi_device *spi, struct spi_message *mesg)
{
	struct spi_transfer *t;
	int ret = 0;

	sun6i_spi_set_cs(spi, true);
	list_for_each_entry(t, &mesg->transfers, transfer_list) {
		ret = sun6i_spi_transfer_one(spi->master, spi, t);
		if (ret)
			break;
	}
	sun6i_spi_set_cs(spi, false);

	return ret;
}

#define SUN6I_SPI_SUPPORTED_MODES	\
	(SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST)

static int sun6i_spi_setup(struct spi_device *spi)
{
	if (spi->bits_per_word != 8) {
		dev_err(spi->master->dev, "master does not support %d bits per word\n",
			spi->bits_per_word);
		return -EINVAL;
	}

	if ((spi->mode & SUN6I_SPI_SUPPORTED_MODES) != spi->mode) {
		dev_err(spi->master->dev, "master does not support mode %08x\n",
			spi->mode);
	}

	return 0;
}

static int sun6i_spi_probe(struct device_d *dev)
{
	struct spi_master *master;
	struct sun6i_spi *sspi;
	int ret = 0;

	sspi = xzalloc(sizeof(*sspi));
	master = &sspi->master;

	dev->priv = sspi;

	sspi->base_addr = dev_get_mem_region(dev, 0);
	if (IS_ERR(sspi->base_addr)) {
		ret = PTR_ERR(sspi->base_addr);
		goto err_free_sspi;
	}

	master->dev = dev;
	master->bus_num = dev->id;
	master->setup = sun6i_spi_setup;
	master->transfer = sun6i_spi_transfer;
	master->num_chipselect = 4;

	sspi->hclk = clk_get(dev, "ahb");
	if (IS_ERR(sspi->hclk)) {
		dev_err(dev, "Unable to acquire AHB clock\n");
		ret = PTR_ERR(sspi->hclk);
		goto err_free_sspi;
	}

	sspi->mclk = clk_get(dev, "mod");
	if (IS_ERR(sspi->mclk)) {
		dev_err(dev, "Unable to acquire module clock\n");
		ret = PTR_ERR(sspi->mclk);
		goto err_free_sspi;
	}

	sspi->rstc = reset_control_get(dev, NULL);
	if (IS_ERR(sspi->rstc)) {
		dev_err(dev, "Couldn't get reset controller\n");
		ret = PTR_ERR(sspi->rstc);
		goto err_free_sspi;
	}

	ret = clk_enable(sspi->hclk);
	if (ret) {
		dev_err(dev, "Couldn't enable AHB clock\n");
		goto err_free_sspi;
	}

	ret = clk_enable(sspi->mclk);
	if (ret) {
		dev_err(dev, "Couldn't enable module clock\n");
		goto err_disable_hclk;
	}

	ret = reset_control_deassert(sspi->rstc);
	if (ret) {
		dev_err(dev, "Couldn't deassert the device from reset\n");
		goto err_disable_mclk;
	}

	sun6i_spi_write(sspi, SUN6I_GBL_CTL_REG,
			SUN6I_GBL_CTL_BUS_ENABLE | SUN6I_GBL_CTL_MASTER | SUN6I_GBL_CTL_TP);

	ret = spi_register_master(master);
	if (ret) {
		dev_err(dev, "cannot register SPI master\n");
		goto err_assert_rstc;
	}

	return 0;

err_assert_rstc:
	reset_control_assert(sspi->rstc);
err_disable_mclk:
	clk_disable(sspi->mclk);
err_disable_hclk:
	clk_disable(sspi->hclk);
err_free_sspi:
	free(sspi);
	return ret;
}

static const struct of_device_id sun6i_spi_match[] = {
	{ .compatible = "allwinner,sun6i-a31-spi", },
	{}
};

static struct driver_d sun6i_spi_driver = {
	.probe		= sun6i_spi_probe,
	.name		= "sun6i-spi",
	.of_compatible	= DRV_OF_COMPAT(sun6i_spi_match),
};
device_platform_driver(sun6i_spi_driver);

MODULE_AUTHOR("Pan Nan <pannan@allwinnertech.com>");
MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A31 SPI controller driver");
MODULE_LICENSE("GPL");
