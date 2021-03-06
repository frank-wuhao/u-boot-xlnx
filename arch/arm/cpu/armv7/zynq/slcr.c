/*
 * Copyright (c) 2013 Xilinx Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <malloc.h>
#include <asm/arch/hardware.h>

#define SLCR_LOCK_MAGIC		0x767B
#define SLCR_UNLOCK_MAGIC	0xDF0D

#define SLCR_QSPI_ENABLE		0x02
#define SLCR_QSPI_ENABLE_MASK		0x03
#define SLCR_NAND_L2_SEL		0x10
#define SLCR_NAND_L2_SEL_MASK		0x1F

#define SLCR_IDCODE_MASK	0x1F000
#define SLCR_IDCODE_SHIFT	12

/*
 * zynq_slcr_mio_get_status - Get the status of MIO peripheral.
 *
 * @peri_name: Name of the peripheral for checking MIO status
 * @get_pins: Pointer to array of get pin for this peripheral
 * @num_pins: Number of pins for this peripheral
 * @mask: Mask value
 * @check_val: Required check value to get the status of  periph
 */
struct zynq_slcr_mio_get_status {
	const char *peri_name;
	const int *get_pins;
	int num_pins;
	u32 mask;
	u32 check_val;
};

static const int qspi0_pins[] = {
	1, 2, 3, 4, 5, 6
};

static const int qspi1_cs_pin[] = {
	0
};

static const int qspi1_pins[] = {
	9, 10, 11, 12, 13
};

static const int nand8_pins[] = {
	0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
};

static const int nand16_pins[] = {
	16, 17, 18, 19, 20, 21, 22, 23
};

static const struct zynq_slcr_mio_get_status mio_periphs[] = {
	{
		"qspi0",
		qspi0_pins,
		ARRAY_SIZE(qspi0_pins),
		SLCR_QSPI_ENABLE_MASK,
		SLCR_QSPI_ENABLE,
	},
	{
		"qspi1_cs",
		qspi1_cs_pin,
		ARRAY_SIZE(qspi1_cs_pin),
		SLCR_QSPI_ENABLE_MASK,
		SLCR_QSPI_ENABLE,
	},
	{
		"qspi1",
		qspi1_pins,
		ARRAY_SIZE(qspi1_pins),
		SLCR_QSPI_ENABLE_MASK,
		SLCR_QSPI_ENABLE,
	},
	{
		"nand8",
		nand8_pins,
		ARRAY_SIZE(nand8_pins),
		SLCR_NAND_L2_SEL_MASK,
		SLCR_NAND_L2_SEL,
	},
	{
		"nand16",
		nand16_pins,
		ARRAY_SIZE(nand16_pins),
		SLCR_NAND_L2_SEL_MASK,
		SLCR_NAND_L2_SEL,
	},
};

static int slcr_lock = 1; /* 1 means locked, 0 means unlocked */

void zynq_slcr_lock(void)
{
	if (!slcr_lock)
		writel(SLCR_LOCK_MAGIC, &slcr_base->slcr_lock);
}

void zynq_slcr_unlock(void)
{
	if (slcr_lock)
		writel(SLCR_UNLOCK_MAGIC, &slcr_base->slcr_unlock);
}

/* Reset the entire system */
void zynq_slcr_cpu_reset(void)
{
	/*
	 * Unlock the SLCR then reset the system.
	 * Note that this seems to require raw i/o
	 * functions or there's a lockup?
	 */
	zynq_slcr_unlock();

	/*
	 * Clear 0x0F000000 bits of reboot status register to workaround
	 * the FSBL not loading the bitstream after soft-reboot
	 * This is a temporary solution until we know more.
	 */
	clrbits_le32(&slcr_base->reboot_status, 0xF000000);

	writel(1, &slcr_base->pss_rst_ctrl);
}

/* Setup clk for network */
void zynq_slcr_gem_clk_setup(u32 gem_id, u32 rclk, u32 clk)
{
	zynq_slcr_unlock();

	if (gem_id > 1) {
		printf("Non existing GEM id %d\n", gem_id);
		goto out;
	}

	if (gem_id) {
		/* Set divisors for appropriate frequency in GEM_CLK_CTRL */
		writel(clk, &slcr_base->gem1_clk_ctrl);
		/* Configure GEM_RCLK_CTRL */
		writel(rclk, &slcr_base->gem1_rclk_ctrl);
	} else {
		/* Set divisors for appropriate frequency in GEM_CLK_CTRL */
		writel(clk, &slcr_base->gem0_clk_ctrl);
		/* Configure GEM_RCLK_CTRL */
		writel(rclk, &slcr_base->gem0_rclk_ctrl);
	}

out:
	zynq_slcr_lock();
}

void zynq_slcr_devcfg_disable(void)
{
	zynq_slcr_unlock();

	/* Disable AXI interface */
	writel(0xFFFFFFFF, &slcr_base->fpga_rst_ctrl);

	/* Set Level Shifters DT618760 */
	writel(0xA, &slcr_base->lvl_shftr_en);

	zynq_slcr_lock();
}

void zynq_slcr_devcfg_enable(void)
{
	zynq_slcr_unlock();

	/* Set Level Shifters DT618760 */
	writel(0xF, &slcr_base->lvl_shftr_en);

	/* Disable AXI interface */
	writel(0x0, &slcr_base->fpga_rst_ctrl);

	zynq_slcr_lock();
}

u32 zynq_slcr_get_boot_mode(void)
{
	/* Get the bootmode register value */
	return readl(&slcr_base->boot_mode);
}

u32 zynq_slcr_get_idcode(void)
{
	return (readl(&slcr_base->pss_idcode) & SLCR_IDCODE_MASK) >>
							SLCR_IDCODE_SHIFT;
}

/*
 * zynq_slcr_get_mio_pin_status - Get the MIO pin status of peripheral.
 *
 * @periph: Name of the peripheral
 *
 * Returns count to indicate the number of pins configured for the
 * given @periph.
 */
int zynq_slcr_get_mio_pin_status(const char *periph)
{
	const struct zynq_slcr_mio_get_status *mio_ptr;
	int val, i, j;
	int mio = 0;

	for (i = 0; i < ARRAY_SIZE(mio_periphs); i++) {
		if (strcmp(periph, mio_periphs[i].peri_name) == 0) {
			mio_ptr = &mio_periphs[i];
			for (j = 0; j < mio_ptr->num_pins; j++) {
				val = readl(&slcr_base->mio_pin
						[mio_ptr->get_pins[j]]);
				if ((val & mio_ptr->mask) == mio_ptr->check_val)
					mio++;
			}
			break;
		}
	}

	return mio;
}
