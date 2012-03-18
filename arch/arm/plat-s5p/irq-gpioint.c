/* linux/arch/arm/plat-s5p/irq-gpioint.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * Author: Kyungmin Park <kyungmin.park@samsung.com>
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 * Author: Marek Szyprowski <m.szyprowski@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>

#include <mach/map.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/pm.h>

#include <asm/mach/irq.h>

#define GPIO_BASE(chip)		(((unsigned long)(chip)->base) & 0xFFFFF000u)

#define CON_OFFSET		0x700
#define MASK_OFFSET		0x900
#define PEND_OFFSET		0xA00
#define REG_OFFSET(x)		((x) << 2)

struct s5p_gpioint_pmsave {
	u32	con, msk;
};

struct s5p_gpioint_bank {
	struct list_head	list;
	int			start;
	int			nr_groups;
	int			irq;
	struct samsung_gpio_chip	**chips;
	void			(*handler)(unsigned int, struct irq_desc *);
#ifdef CONFIG_PM
	struct s5p_gpioint_pmsave	*pmsave;
#endif
};

static LIST_HEAD(banks);

static int s5p_gpioint_set_type(struct irq_data *d, unsigned int type)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = gc->chip_types;
	unsigned int shift = (d->irq - gc->irq_base) << 2;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		type = S5P_IRQ_TYPE_EDGE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		type = S5P_IRQ_TYPE_EDGE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		type = S5P_IRQ_TYPE_EDGE_BOTH;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		type = S5P_IRQ_TYPE_LEVEL_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		type = S5P_IRQ_TYPE_LEVEL_LOW;
		break;
	case IRQ_TYPE_NONE:
	default:
		printk(KERN_WARNING "No irq type\n");
		return -EINVAL;
	}

	gc->type_cache &= ~(0x7 << shift);
	gc->type_cache |= type << shift;
	writel(gc->type_cache, gc->reg_base + ct->regs.type);
	return 0;
}

static void s5p_gpioint_handler(unsigned int irq, struct irq_desc *desc)
{
	struct s5p_gpioint_bank *bank = irq_get_handler_data(irq);
	int group, pend_offset, mask_offset;
	unsigned int pend, mask;

	struct irq_chip *chip = irq_get_chip(irq);
	chained_irq_enter(chip, desc);

	for (group = 0; group < bank->nr_groups; group++) {
		struct samsung_gpio_chip *chip = bank->chips[group];
		if (!chip)
			continue;

		pend_offset = REG_OFFSET(group);
		pend = __raw_readl(GPIO_BASE(chip) + PEND_OFFSET + pend_offset);
		if (!pend)
			continue;

		mask_offset = REG_OFFSET(group);
		mask = __raw_readl(GPIO_BASE(chip) + MASK_OFFSET + mask_offset);
		pend &= ~mask;

		while (pend) {
			int offset = fls(pend) - 1;
			int real_irq = chip->irq_base + offset;
			generic_handle_irq(real_irq);
			pend &= ~BIT(offset);
		}
	}
	chained_irq_exit(chip, desc);
}

static __init int s5p_gpioint_add(struct samsung_gpio_chip *chip)
{
	static int used_gpioint_groups = 0;
	int group = chip->group;
	struct s5p_gpioint_bank *b, *bank = NULL;
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;

	if (used_gpioint_groups >= S5P_GPIOINT_GROUP_COUNT)
		return -ENOMEM;

	list_for_each_entry(b, &banks, list) {
		if (group >= b->start && group < b->start + b->nr_groups) {
			bank = b;
			break;
		}
	}
	if (!bank)
		return -EINVAL;

	if (!bank->handler) {
		bank->chips = kzalloc(sizeof(struct samsung_gpio_chip *) *
				      bank->nr_groups, GFP_KERNEL);
		if (!bank->chips)
			return -ENOMEM;

		irq_set_chained_handler(bank->irq, s5p_gpioint_handler);
		irq_set_handler_data(bank->irq, bank);
		bank->handler = s5p_gpioint_handler;
		printk(KERN_INFO "Registered chained gpio int handler for interrupt %d.\n",
		       bank->irq);
	}

	/*
	 * chained GPIO irq has been successfully registered, allocate new gpio
	 * int group and assign irq nubmers
	 */
	chip->irq_base = S5P_GPIOINT_BASE +
			 used_gpioint_groups * S5P_GPIOINT_GROUP_SIZE;
	used_gpioint_groups++;

	bank->chips[group - bank->start] = chip;

	gc = irq_alloc_generic_chip("s5p_gpioint", 1, chip->irq_base,
				    (void __iomem *)GPIO_BASE(chip),
				    handle_level_irq);
	if (!gc)
		return -ENOMEM;
	ct = gc->chip_types;
	ct->chip.irq_ack = irq_gc_ack_set_bit;
	ct->chip.irq_mask = irq_gc_mask_set_bit;
	ct->chip.irq_unmask = irq_gc_mask_clr_bit;
	ct->chip.irq_set_type = s5p_gpioint_set_type,
	ct->regs.ack = PEND_OFFSET + REG_OFFSET(group - bank->start);
	ct->regs.mask = MASK_OFFSET + REG_OFFSET(group - bank->start);
	ct->regs.type = CON_OFFSET + REG_OFFSET(group - bank->start);
	irq_setup_generic_chip(gc, IRQ_MSK(chip->chip.ngpio),
			       IRQ_GC_INIT_MASK_CACHE,
			       IRQ_NOREQUEST | IRQ_NOPROBE, 0);
	return 0;
}

int __init s5p_register_gpio_interrupt(int pin)
{
	struct samsung_gpio_chip *my_chip = samsung_gpiolib_getchip(pin);
	int offset, group;
	int ret;

	if (!my_chip)
		return -EINVAL;

	offset = pin - my_chip->chip.base;
	group = my_chip->group;

	/* check if the group has been already registered */
	if (my_chip->irq_base)
		return my_chip->irq_base + offset;

	/* register gpio group */
	ret = s5p_gpioint_add(my_chip);
	if (ret == 0) {
		my_chip->chip.to_irq = samsung_gpiolib_to_irq;
		printk(KERN_INFO "Registered interrupt support for gpio group %d.\n",
		       group);
		return my_chip->irq_base + offset;
	}
	return ret;
}

int __init s5p_register_gpioint_bank(int chain_irq, int start, int nr_groups)
{
	struct s5p_gpioint_bank *bank;
	struct s5p_gpioint_pmsave *pmsave = NULL;

	bank = kzalloc(sizeof(*bank), GFP_KERNEL);
	if (!bank)
		return -ENOMEM;

#ifdef CONFIG_PM
	pmsave = kzalloc(sizeof(*pmsave) * nr_groups, GFP_KERNEL);
	if (!pmsave) {
		kfree(bank);
		return -ENOMEM;
	}
#endif

	bank->start = start;
	bank->nr_groups = nr_groups;
	bank->irq = chain_irq;
	bank->pmsave = pmsave;

	list_add_tail(&bank->list, &banks);
	return 0;
}

#ifdef CONFIG_PM
static void s5p_gpioint_do_save(struct s5p_gpioint_bank *bank, bool restore)
{
	int group;
	void __iomem *base;
	struct s5p_gpioint_pmsave *pmsave;

	if (!bank->pmsave)
		return;

	for (group = 0; group < bank->nr_groups; group++) {
		struct samsung_gpio_chip *chip = bank->chips[group];
		if (!chip)
			continue;

		pmsave = &bank->pmsave[group];
		base = (void __iomem *)(GPIO_BASE(chip) + REG_OFFSET(group));

		if (restore) {
			u32 old_con = __raw_readl(base + CON_OFFSET);
			u32 old_msk = __raw_readl(base + MASK_OFFSET);
			
			__raw_writel(pmsave->con, base + CON_OFFSET);
			__raw_writel(pmsave->msk, base + MASK_OFFSET);
			
			S3C_PMDBG("[GPIOINT] %s: CON %08x => %08x, MASK %08x => %08x\n",
				  chip->chip.label,
				  old_con, __raw_readl(base + CON_OFFSET),
				  old_msk, __raw_readl(base + MASK_OFFSET));
		} else {
			pmsave->con = __raw_readl(base + CON_OFFSET);
			pmsave->msk = __raw_readl(base + MASK_OFFSET);
			
			S3C_PMDBG("[GPIOINT] %s: save CON %08x, MASK %08x\n",
				  chip->chip.label,
				  pmsave->con,
				  pmsave->msk);
		}
	}
}

static int s5p_gpioint_save(void)
{
	struct s5p_gpioint_bank *b;

	list_for_each_entry(b, &banks, list) {
		s5p_gpioint_do_save(b, false);
	}

	return 0;
}

static void s5p_gpioint_restore(void)
{
	struct s5p_gpioint_bank *b;

	list_for_each_entry(b, &banks, list) {
		s5p_gpioint_do_save(b, true);
	}
}

static struct syscore_ops gpioint_syscore_ops = {
	.suspend	= s5p_gpioint_save,
	.resume		= s5p_gpioint_restore,
};

static __init int gpioint_syscore_init(void)
{
	register_syscore_ops(&gpioint_syscore_ops);
	return 0;
}
arch_initcall(gpioint_syscore_init);
#endif
