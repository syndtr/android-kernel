/* modem_pdp.c
 * 
 * Copyright (C) 2012 Suryandaru Triandana.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/tty.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include "modem_ctl.h"
#include "modem_ctl_p.h"

int mpdp_open(struct tty_struct *tty, struct file * filp)
{
	struct m_pdp *pdp = tty->driver->driver_state;

	tty->driver_data = pdp;
	tty->low_latency = 1;
	pdp->tty = tty;
	atomic_inc(&pdp->open_count);

	return 0;
}

void mpdp_close(struct tty_struct *tty, struct file * filp)
{
	struct m_pdp *pdp = tty->driver_data;

	if (atomic_dec_and_test(&pdp->open_count))
		pdp->tty = NULL;
}

int mpdp_write(struct tty_struct * tty,
		const unsigned char *buf, int count)
{
	struct m_pdp *pdp = tty->driver_data;
	struct m_multipdp *mpdp = pdp->mpdp;
	struct modem_io io;
	int ret;

	if (count > mpdp->size - 7)
		return -EINVAL;

	io.raw = false;
	io.data = (void *)buf;
	io.size = count;
	io.id = pdp->channel;
	io.cmd = 0;

	if (mutex_lock_interruptible(&mpdp->pipe->tx_lock))
		return -EINTR;
	ret = modem_pipe_send(mpdp->pipe, &io);
	mutex_unlock(&mpdp->pipe->tx_lock);

	if (!ret)
		return count;

	return ret;
}

int mpdp_write_room(struct tty_struct *tty)
{
	struct m_pdp *pdp = tty->driver_data;

	return pdp->mpdp->size - 7;
}

static const struct tty_operations mpdp_tty_ops = {
	.open = mpdp_open,
	.close = mpdp_close,
	.write = mpdp_write,
	.write_room = mpdp_write_room,
};

int mpdp_recv_filter(struct modem_io *io)
{
	struct m_multipdp *mpdp = io->data;
	struct m_pdp *pdp = NULL;
	int i;

	for (i = 0; i < mpdp->pdp_nr; i++) {
		pdp = &mpdp->pdp[i];
		if (pdp->channel == io->id) {
			if (!atomic_read(&pdp->open_count))
				return 0;
			io->data = pdp->tty;
			return 1;
		}
	}

	return 0;
}

void mpdp_recv_work(struct work_struct *work)
{
	struct m_multipdp *mpdp = container_of(work, struct m_multipdp, work);
	struct modem_io io;

	io.raw = false;
	io.filter = mpdp_recv_filter;
	io.data = mpdp;

	mutex_lock(&mpdp->pipe->rx_lock);
	while(modem_pipe_recv_tty(mpdp->pipe, &io) == 0) {
		io.id = 0;
		io.data = mpdp;
	}
	mutex_unlock(&mpdp->pipe->rx_lock);
}

#define M_PDP_DEFINE(_chan, _name) \
	{ .channel = _chan, .name = _name }

struct m_pdp raw_pdp[] = {
	M_PDP_DEFINE(1, "ttyCSD"),
	M_PDP_DEFINE(7, "ttyCDMA"),
	M_PDP_DEFINE(9, "ttyTRFB"),
};

static int modem_pdp_init(struct m_multipdp *mpdp, struct m_pdp *pdp)
{
	struct tty_driver *tty;
	int r;

	pdp->mpdp = mpdp;

	tty = alloc_tty_driver(1);
	if (!tty)
		return -ENOMEM;

	tty->owner = THIS_MODULE;
	tty->magic = TTY_DRIVER_MAGIC;
	tty->driver_name = "modemctl";
	tty->name = pdp->name;
	tty->major = TTYAUX_MAJOR;
	tty->minor_start = 65 + pdp->channel;
	tty->num = DPRAM_TTY_NR;
	tty->type = TTY_DRIVER_TYPE_SERIAL;
	tty->subtype = SERIAL_TYPE_NORMAL;
	tty->flags = TTY_DRIVER_REAL_RAW;
	tty->init_termios = tty_std_termios;
	tty->init_termios.c_cflag = B115200 | CS8 | CREAD |
					CLOCAL | HUPCL;
	tty->driver_state = pdp;
	tty_set_operations(tty, &mpdp_tty_ops);

	r = tty_register_driver(tty);
	if (r)
		goto err_tty;
	
	pdp->tty_driver = tty;

	dev_info(mpdp->mc->dev, "initialized '%s' channel=%d\n",
		 pdp->name, pdp->channel);

	return 0;

err_tty:
	put_tty_driver(tty);
	return r;
}

int modem_mpdp_init(struct modemctl *mc)
{
	struct m_pdp *pdp;
	int i, ret;

	mc->raw_mpdp.mc = mc;
	mc->raw_mpdp.pipe = &mc->raw_pipe;
	mc->raw_mpdp.pdp = raw_pdp;
	mc->raw_mpdp.pdp_nr = ARRAY_SIZE(raw_pdp);
	mc->raw_mpdp.size = SIZ_RAW_DATA;
	INIT_WORK(&mc->raw_mpdp.work, mpdp_recv_work);

	for (i = 0; i < mc->raw_mpdp.pdp_nr; i++) {
		pdp = &mc->raw_mpdp.pdp[i];
		ret = modem_pdp_init(&mc->raw_mpdp, pdp);
		if (ret)
			dev_err(mc->dev, "unable to initialize pdp channel=%d if=%s ret=%d\n",
				pdp->channel, pdp->name, ret);
	}

	return 0;
}
