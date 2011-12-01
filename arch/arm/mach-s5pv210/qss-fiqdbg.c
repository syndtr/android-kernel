/* linux/arch/arm/mach-s5pv210/qss-fiqdbg.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/sysdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/syscore_ops.h>

#include <asm/mach/irq.h>
#include <asm/fiq_debugger.h>
#include <asm/hardware/vic.h>

#include <mach/irqs.h>
#include <mach/map.h>

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>

#include <mach/qss.h>

#define QSS_FIQDBG_UART 2

#define QSS_FIQDBG_BASE S5P_VA_UART(QSS_FIQDBG_UART)

#define _JOIN(x,y) x##y

#define FIQDGB_IRQ_UART(x) _JOIN(IRQ_UART,x)

static DEFINE_SPINLOCK(vic_intselect_lock);
int vic_set_fiq(unsigned int irq, bool enable)
{
	u32 int_select;
	u32 mask;
	unsigned long irq_flags;
	void __iomem *base = irq_get_chip_data(irq);
	irq &= 31;
	mask = 1 << irq;

	spin_lock_irqsave(&vic_intselect_lock, irq_flags);
	int_select = readl(base + VIC_INT_SELECT);
	if (enable)
		int_select |= mask;
	else
		int_select &= ~mask;
	writel(int_select, base + VIC_INT_SELECT);
	spin_unlock_irqrestore(&vic_intselect_lock, irq_flags);

	return 0;
}

void _fiqdbg_uart_init(void)
{
	writel(S3C2410_UCON_TXILEVEL |
		S3C2410_UCON_RXILEVEL |
		S3C2410_UCON_TXIRQMODE |
		S3C2410_UCON_RXIRQMODE |
		S3C2410_UCON_RXFIFO_TOI |
		S3C2443_UCON_RXERR_IRQEN, QSS_FIQDBG_BASE + S3C2410_UCON);
	writel(S3C2410_LCON_CS8, QSS_FIQDBG_BASE + S3C2410_ULCON);
	writel(S3C2410_UFCON_FIFOMODE |
		S5PV210_UFCON_TXTRIG4 |
		S5PV210_UFCON_RXTRIG4 |
		S3C2410_UFCON_RESETRX, QSS_FIQDBG_BASE + S3C2410_UFCON);
	writel(0, QSS_FIQDBG_BASE + S3C2410_UMCON);
	/* 115200 */
	writel(35, QSS_FIQDBG_BASE + S3C2410_UBRDIV);
	writel(0x808, QSS_FIQDBG_BASE + S3C2443_DIVSLOT);
	writel(0xc, QSS_FIQDBG_BASE + S3C64XX_UINTM);
	writel(0xf, QSS_FIQDBG_BASE + S3C64XX_UINTP);
}

static int fiqdbg_uart_init(struct platform_device *pdev)
{
	_fiqdbg_uart_init();

	return 0;
}

static int fiqdbg_uart_getc(struct platform_device *pdev)
{
	unsigned int ufstat;

	if (readl(QSS_FIQDBG_BASE + S3C2410_UERSTAT) & S3C2410_UERSTAT_BREAK)
		return FIQ_DEBUGGER_BREAK;

	ufstat = readl(QSS_FIQDBG_BASE + S3C2410_UFSTAT);
	if (!(ufstat & (S5PV210_UFSTAT_RXMASK | S5PV210_UFSTAT_RXFULL)))
		return FIQ_DEBUGGER_NO_CHAR;
	return readb(QSS_FIQDBG_BASE + S3C2410_URXH);
}

static void fiqdbg_wait_for_tx_empty(void)
{
	unsigned int tmout = 10000;
	unsigned long ufstat;

	while (true) {
		ufstat = readl(QSS_FIQDBG_BASE + S3C2410_UFSTAT);
		if (!(ufstat & (S5PV210_UFSTAT_TXMASK | S5PV210_UFSTAT_TXFULL)))
			return;
		if (--tmout == 0)
			break;
		udelay(1);
	}
}

static void fiqdbg_uart_putc(struct platform_device *pdev,
					unsigned int c)
{
	fiqdbg_wait_for_tx_empty();
	writeb(c, QSS_FIQDBG_BASE + S3C2410_UTXH);
	if (c == 10) {
		fiqdbg_wait_for_tx_empty();
		writeb(13, QSS_FIQDBG_BASE + S3C2410_UTXH);
	}
	fiqdbg_wait_for_tx_empty();
}

static void fiq_enable(struct platform_device *pdev,
			unsigned int fiq, bool enabled)
{
	struct irq_data *d = irq_get_irq_data(fiq);
	struct irq_chip *chip = irq_data_get_irq_chip(d);

	vic_set_fiq(fiq, enabled);
	if (enabled)
		chip->irq_unmask(d);
	else
		chip->irq_mask(d);
}


static void fiq_ack(struct platform_device *pdev, unsigned int fiq)
{
	writel(0x3, QSS_FIQDBG_BASE + S3C64XX_UINTP);
}

static struct fiq_debugger_pdata qss_fiqdbg_pdata = {
	.clkname	= "pclk",
	.uart_init	= fiqdbg_uart_init,
	.uart_getc	= fiqdbg_uart_getc,
	.uart_putc	= fiqdbg_uart_putc,
	.fiq_enable	= fiq_enable,
	.fiq_ack	= fiq_ack,
};

static struct resource qss_fiqdbg_resources[] = {
	{
		.name 	= "uart_irq",
		.start	= FIQDGB_IRQ_UART(QSS_FIQDBG_UART),
		.end	= FIQDGB_IRQ_UART(QSS_FIQDBG_UART),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device qss_fiqdbg_device = {
	.name		= "fiq_debugger",
	.id		= QSS_FIQDBG_UART,
	.resource	= qss_fiqdbg_resources,
	.num_resources	= ARRAY_SIZE(qss_fiqdbg_resources),
	.dev = {
		.platform_data = &qss_fiqdbg_pdata,
	}
};

static struct syscore_ops fiqdbg_pm_syscore_ops = {
	.resume		= _fiqdbg_uart_init,
};

void __init qss_fiqdbg_init(void)
{
	platform_device_register(&qss_fiqdbg_device);
	register_syscore_ops(&fiqdbg_pm_syscore_ops);
}
