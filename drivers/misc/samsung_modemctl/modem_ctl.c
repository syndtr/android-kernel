/* modem_ctl.c
 * 
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
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
#include <linux/tty_flip.h>

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

static inline u32 read_semaphore(struct modemctl *mc)
{
	return ioread32(mc->mmio + OFF_SEM) & 1;
}

static void return_semaphore(struct modemctl *mc)
{
	iowrite32(0, mc->mmio + OFF_SEM);
}

void modem_request_sem(struct modemctl *mc)
{
	writel(MB_COMMAND | MB_VALID | MBC_REQ_SEM,
	       mc->mmio + OFF_MBOX_AP);
}

static inline int mmio_sem(struct modemctl *mc)
{
	return readl(mc->mmio + OFF_SEM) & 1;
}

int modem_request_mmio(struct modemctl *mc)
{
	unsigned long flags;
	int ret;
	spin_lock_irqsave(&mc->lock, flags);
	mc->mmio_req_count++;
	ret = mc->mmio_owner;
	if (!ret) {
		if (mmio_sem(mc) == 1) {
			/* surprise! we already have control */
			ret = mc->mmio_owner = 1;
			wake_up(&mc->wq);
			modem_update_state(mc);
			MODEM_COUNT(mc,request_no_wait);
		} else {
			/* ask the modem for mmio access */
			if (modem_running(mc))
				modem_request_sem(mc);
			MODEM_COUNT(mc,request_wait);
		}
	} else {
		MODEM_COUNT(mc,request_no_wait);
	}
	/* TODO: timer to retry? */
	spin_unlock_irqrestore(&mc->lock, flags);
	return ret;
}

void modem_release_mmio(struct modemctl *mc, unsigned bits)
{
	unsigned long flags;
	spin_lock_irqsave(&mc->lock, flags);
	mc->mmio_req_count--;
	mc->mmio_signal_bits |= bits;
	if ((mc->mmio_req_count == 0) && modem_running(mc)) {
		if (mc->mmio_bp_request) {
			mc->mmio_bp_request = 0;
			writel(0, mc->mmio + OFF_SEM);
			writel(MB_COMMAND | MB_VALID | MBC_RES_SEM,
			       mc->mmio + OFF_MBOX_AP);
			MODEM_COUNT(mc,release_bp_waiting);
		} else if (mc->mmio_signal_bits) {
			writel(0, mc->mmio + OFF_SEM);
			writel(MB_VALID | mc->mmio_signal_bits,
			       mc->mmio + OFF_MBOX_AP);
			MODEM_COUNT(mc,release_bp_signaled);
		} else {
			MODEM_COUNT(mc,release_no_action);
		}
		mc->mmio_owner = 0;
		mc->mmio_signal_bits = 0;
	}
	spin_unlock_irqrestore(&mc->lock, flags);
}

static int mmio_owner_p(struct modemctl *mc)
{
	unsigned long flags;
	int ret;
	spin_lock_irqsave(&mc->lock, flags);
	ret = mc->mmio_owner || modem_offline(mc);
	spin_unlock_irqrestore(&mc->lock, flags);
	return ret;
}

int modem_acquire_mmio(struct modemctl *mc)
{
	if (modem_request_mmio(mc) == 0) {
		int ret = wait_event_interruptible_timeout(
			mc->wq, mmio_owner_p(mc), 500 * HZ);
		if (ret <= 0) {
			modem_release_mmio(mc, 0);
			if (ret == 0) {
				pr_err("modem_acquire_mmio() TIMEOUT\n");
				return -ENODEV;
			} else {
				return -ERESTARTSYS;
			}
		}
	}
	if (!modem_running(mc)) {
		modem_release_mmio(mc, 0);
		return -ENODEV;
	}
	return 0;
}

void modem_fifo_req_ack(struct modemctl *mc, struct m_fifo *fifo) {
	writel(MB_VALID | fifo->req_ack_bits,
	       mc->mmio + OFF_MBOX_AP);
}

void modem_fifo_res_ack(struct modemctl *mc, struct m_fifo *fifo) {
	if (!fifo->ack_requested)
		return;
	writel(MB_VALID | fifo->res_ack_bits,
	       mc->mmio + OFF_MBOX_AP);
	fifo->ack_requested = 0;
}

void modem_fifo_get_ack(struct m_fifo *fifo, unsigned cmd) {
	fifo->ack_requested = !!(cmd & fifo->req_ack_bits);
}

static int modem_start(struct modemctl *mc, int ramdump)
{
	int ret;

	pr_info("[MODEM] modem_start() %s\n",
		ramdump ? "ramdump" : "normal");

	if (mc->status != MODEM_POWER_ON) {
		pr_err("[MODEM] modem not powered on\n");
		return -EINVAL;
	}

	writel(0, mc->mmio + OFF_SEM);
	if (ramdump) {
		mc->status = MODEM_BOOTING_RAMDUMP;
		mc->ramdump_size = 0;
		mc->ramdump_pos = 0;
		writel(MODEM_CMD_RAMDUMP_START, mc->mmio + OFF_MBOX_AP);

		ret = wait_event_timeout(mc->wq, mc->status == MODEM_DUMPING, 25 * HZ);
		if (ret == 0)
			return -ENODEV;
	} else {
		mc->status = MODEM_RUNNING;
	}

	pr_info("[MODEM] modem_start() DONE\n");
	return 0;
}

static int modem_reset(struct modemctl *mc)
{
	pr_info("[MODEM] modem_reset()\n");

	/* ensure pda active pin set to low */
	gpio_set_value(mc->gpio_pda_active, 0);

	/* read inbound mbox to clear pending IRQ */
	(void) readl(mc->mmio + OFF_MBOX_BP);

	/* write outbound mbox to assert outbound IRQ */
	writel(0, mc->mmio + OFF_MBOX_AP);

	gpio_set_value(mc->gpio_phone_on, 1);
	msleep(50);

	/* ensure cp_reset pin set to low */
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(100);

	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(500);

	gpio_set_value(mc->gpio_pda_active, 1);

	mc->status = MODEM_POWER_ON;

	return 0;
}

static int modem_off(struct modemctl *mc)
{
	pr_info("[MODEM] modem_off()\n");
	gpio_set_value(mc->gpio_cp_reset, 0);
	mc->status = MODEM_OFF;
	return 0;
}

static int modem_pwr_status(struct modemctl *mc)
{
	pr_debug("%s\n", __func__);
	return gpio_get_value(mc->gpio_phone_active);
}

static int dpram_modem_pwroff(struct modemctl *mc)
{
	pr_debug("%s\n", __func__);

	gpio_set_value(mc->gpio_phone_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
	mdelay(100);

	return 0;
}

static void dpram_req_reset(struct modemctl *mc)
{
	unsigned long flags;
	char *msg = "9 $PHONE-RESET";

	spin_lock_irqsave(&mc->lock, flags);
	memcpy(mc->errbuf, msg, sizeof(msg));
	mc->errlen = sizeof(msg) - 1;
	mc->errpolled = false;
	wake_up(&mc->wq);
	spin_unlock_irqrestore(&mc->lock, flags);
}

static int dpram_read_to_user(struct modemctl *mc, char __user *buf,
			       int pos, size_t size)
{
	int ret;

	if (!read_semaphore(mc)) {
		pr_err("Semaphore is held by modem!");
		return -EINVAL;
	}

	ret = copy_to_user(buf, mc->mmio + pos, size);
	if (ret < 0) {
		pr_err("[%s:%d] Copy to user failed\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int dpram_write_from_user(struct modemctl *mc, int addr,
				const char __user *data, size_t size)
{
	int ret;

	if (!read_semaphore(mc)) {
		pr_err("Semaphore is held by modem!");
		return -EINVAL;
	}

	ret = copy_from_user(mc->mmio + addr, data, size);
	if (ret < 0) {
		pr_err("[%s:%d] Copy from user failed\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int dpram_read_write(struct modemctl *mc, struct modem_rw *rw)
{
	if (rw->write)
		return dpram_write_from_user(mc, rw->addr, rw->data, rw->size);
	return dpram_read_to_user(mc, rw->data, rw->addr, rw->size);
}

int dpram_open(struct tty_struct *tty, struct file * filp)
{
	struct modemctl *mc = tty->driver->driver_state;
	struct m_tty *mtty = &mc->ttys[tty->index];

	tty->driver_data = mtty;
	tty->low_latency = 1;
	mtty->tty = tty;

	if (atomic_inc_return(&mtty->open_count) > 1)
		return -EBUSY;

	return 0;
}

void dpram_close(struct tty_struct *tty, struct file * filp)
{
	struct m_tty *mtty = tty->driver_data;

	if (atomic_dec_and_test(&mtty->open_count))
		mtty->tty = NULL;
}

int dpram_write(struct tty_struct * tty,
		const unsigned char *buf, int count)
{
	struct m_tty *mtty = tty->driver_data;
	struct modem_io io;
	int ret;

	if (count > mtty->size - 1)
		return -EINVAL;

	io.raw = true;
	io.data = (void *)buf;
	io.size = count;

	if (mutex_lock_interruptible(&mtty->pipe->tx_lock))
		return -EINTR;
	ret = modem_pipe_send(mtty->pipe, &io);
	mutex_unlock(&mtty->pipe->tx_lock);

	if (!ret)
		return count;

	return ret;
}

void dpram_recv_work(struct work_struct *work)
{
	struct m_tty *mtty = container_of(work, struct m_tty, work);
	struct modem_io io;

	io.raw = true;
	io.data = mtty->buffer;
	io.size = SIZ_FMT_DATA;
	io.filter = NULL;

	mutex_lock(&mtty->pipe->rx_lock);
	if (atomic_read(&mtty->open_count)) {
		while(modem_pipe_recv(mtty->pipe, &io) == 0) {
			if (tty_insert_flip_string(mtty->tty, io.data, io.size) != io.size)
				pr_err("modemctl: cannot insert tty flip string\n");
			tty_flip_buffer_push(mtty->tty);
			io.size = SIZ_FMT_DATA;
		}
	} else {
		modem_pipe_purge(mtty->pipe);
	}
	modem_fifo_res_ack(mtty->mc, mtty->pipe->rx);
	mutex_unlock(&mtty->pipe->rx_lock);
}

int dpram_write_room(struct tty_struct *tty)
{
	struct m_tty *mtty = tty->driver_data;

	return mtty->size - 1;
}

int dpram_ioctl(struct tty_struct *tty,
		unsigned int cmd, unsigned long arg)
{
	struct m_tty *mtty = tty->driver_data;
	struct modemctl *mc = mtty->mc;
	int ret = 0;

	DBG_TRACE("dpram_ioctl: CMD %x\n", cmd);

	mutex_lock(&mc->ctl_lock);
	switch (cmd) {
	case IOCTL_MODEM_ON:
		modem_reset(mc);
		modem_start(mc, 0);
		break;
	case IOCTL_MODEM_OFF:
		dpram_modem_pwroff(mc);
		break;
	case IOCTL_MODEM_GETSTATUS: {
		unsigned int status = modem_pwr_status(mc);
		ret = copy_to_user((unsigned int *)arg, &status,
				   sizeof(status));
		break;
	}
	case IOCTL_MODEM_RESET:
		modem_reset(mc);
		MODEM_COUNT(mc,resets);
		break;
	case IOCTL_MODEM_RAMDUMP_ON:
		break;
	case IOCTL_MODEM_RAMDUMP_OFF:
		break;
	case IOCTL_MODEM_MEM_RW: {
		struct modem_rw rw;
		ret = copy_from_user((void *)&rw, (void *)arg, sizeof(rw));
		ret = dpram_read_write(mc, &rw);
		break;
	}
	default:
		ret = -ENOIOCTLCMD;
	}
	mutex_unlock(&mc->ctl_lock);

	return ret;
}

static const struct tty_operations dpram_tty_ops = {
	.open = dpram_open,
	.close = dpram_close,
	.write = dpram_write,
	.write_room = dpram_write_room,
	.ioctl = dpram_ioctl,
};

static int dpram_err_open(struct inode *inode, struct file *filp)
{
	struct modemctl *mc = errdev_to_modemctl(filp->private_data);
	filp->private_data = mc;
	return 0;
}


static ssize_t dpram_err_read(struct file *filp, char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct modemctl *mc = filp->private_data;
	unsigned long flags;
	ssize_t ret;

	while (1) {
		spin_lock_irqsave(&mc->lock, flags);
		if (!mc->errpolled && mc->errlen) {
			count = min(count, mc->errlen + 1);
			ret = copy_to_user(buf, mc->errbuf, count);
			if (ret)
				ret = -EFAULT;
			else
				ret = count;
		}
		mc->errpolled = true;
		spin_unlock_irqrestore(&mc->lock, flags);

		if (ret != 0)
			goto done;

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto done;
		}

		ret = wait_event_interruptible(mc->wq, !mc->errpolled);
		if (ret < 0)
			goto done;
	}

done:
	return ret;
}

static unsigned int dpram_err_poll(struct file *filp, poll_table *wait)
{
	struct modemctl *mc = filp->private_data;
	unsigned long flags;
	int ret = 0;

	poll_wait(filp, &mc->wq, wait);

	spin_lock_irqsave(&mc->lock, flags);
	if (!mc->errpolled && mc->errlen)
		ret = POLLIN | POLLRDNORM;
	mc->errpolled = true;
	spin_unlock_irqrestore(&mc->lock, flags);

	return ret;
}

static const struct file_operations dpram_err_fops = {
	.owner = THIS_MODULE,
	.open = dpram_err_open,
	.read = dpram_err_read,
	.poll = dpram_err_poll,
	.llseek = no_llseek,
};

static void modem_handle_io(struct modemctl *mc, unsigned cmd)
{
	int i;
	struct m_tty *mtty;

	for (i = 0; i < DPRAM_TTY_NR; i++) {
		mtty = &(mc->ttys[i]);
		if (cmd & mtty->pipe->rx->bits) {
			modem_fifo_get_ack(mtty->pipe->rx, cmd);
			schedule_work(&mtty->work);
		}
	}

	if (cmd & mc->raw_mpdp.pipe->rx->bits) {
		modem_fifo_get_ack(mc->raw_mpdp.pipe->rx, cmd);
		schedule_work(&mc->raw_mpdp.work);
	}
}

static irqreturn_t modemctl_bp_irq_handler(int irq, void *_mc)
{
	struct modemctl *mc = _mc;
	int value = 0;

	value = gpio_get_value(((struct modemctl *)_mc)->gpio_phone_active);
	pr_info("[MODEM] bp_irq() PHONE_ACTIVE_PIN=%d\n", value);

	if (mc->initialized && value == 0)
		dpram_req_reset(mc);

	return IRQ_HANDLED;

}

static irqreturn_t modemctl_mbox_irq_handler(int irq, void *_mc)
{
	struct modemctl *mc = _mc;
	unsigned cmd;
	unsigned long flags;

	cmd = readl(mc->mmio + OFF_MBOX_BP);

	DBG_TRACE("modemctl_mbox_irq_handler: CMD %x\n", cmd);

	if (unlikely(mc->status != MODEM_RUNNING)) {
		pr_info("modem: is offline\n");
		return IRQ_HANDLED;
	}

	if (!(cmd & MB_VALID)) {
		pr_info("modem: what is %08x\n",cmd);
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&mc->lock, flags);

	if (cmd & MB_COMMAND) {
		switch (cmd & 15) {
		case MBC_REQ_SEM:
			if (mmio_sem(mc) == 0) {
				/* Sometimes the modem may ask for the
				 * sem when it already owns it.  Humor
				 * it and ack that request.
				 */
				writel(MB_COMMAND | MB_VALID | MBC_RES_SEM,
				       mc->mmio + OFF_MBOX_AP);
				MODEM_COUNT(mc,bp_req_confused);
			} else if (mc->mmio_req_count == 0) {
				/* No references? Give it to the modem. */
				modem_update_state(mc);
				mc->mmio_owner = 0;
				writel(0, mc->mmio + OFF_SEM);
				writel(MB_COMMAND | MB_VALID | MBC_RES_SEM,
				       mc->mmio + OFF_MBOX_AP);
				MODEM_COUNT(mc,bp_req_instant);
				goto done;
			} else {
				/* Busy now, remember the modem needs it. */
				mc->mmio_bp_request = 1;
				MODEM_COUNT(mc,bp_req_delayed);
				break;
			}
		case MBC_RES_SEM:
			break;
		case MBC_PHONE_START:
			mc->initialized = true;
			/* TODO: should we avoid sending any other messages
			 * to the modem until this message is received and
			 * acknowledged?
			 */
			writel(MB_COMMAND | MB_VALID |
			       MBC_INIT_END | CP_BOOT_AIRPLANE | AP_OS_ANDROID,
			       mc->mmio + OFF_MBOX_AP);

			/* TODO: probably unsafe to send this back-to-back
			 * with the INIT_END message.
			 */
			/* if somebody is waiting for mmio access... */
			if (mc->mmio_req_count)
				modem_request_sem(mc);
			break;
		case MBC_RESET:
			pr_err("$$$ MODEM RESET $$$\n");
			mc->status = MODEM_CRASHED;
			wake_up(&mc->wq);
			break;
		case MBC_ERR_DISPLAY: {
			char *buf = mc->errbuf;
			int i;
			pr_err("$$$ MODEM ERROR $$$\n");
			mc->status = MODEM_CRASHED;
			if (!modem_pwr_status(mc)) {
				char *msg = "8 $PHONE-OFF";
				memcpy(buf, msg, sizeof(msg));
				mc->errlen = sizeof(msg) - 1;
				buf += 2;
				goto modem_off;
			}
			buf[0] = '1';
			buf[1] = ' ';
			buf += 2;
			memcpy(buf, mc->mmio + OFF_ERROR_MSG, SIZ_ERROR_MSG);
			for (i = 0; i < SIZ_ERROR_MSG; i++)
				if ((buf[i] < 0x20) || (buf[1] > 0x7e))
					buf[i] = 0x20;
			buf[i] = 0;
			i--;
			while ((i > 0) && (buf[i] == 0x20))
				buf[i--] = 0;
			mc->errlen = i + 2;
modem_off:
			mc->errpolled = false;
			wake_up(&mc->wq);
			pr_err("$$$ %s $$$\n", buf);
			break;
		}
		case MBC_SUSPEND:
			break;
		case MBC_RESUME:
			break;
		}
	}

	/* On *any* interrupt from the modem it may have given
	 * us ownership of the mmio hw semaphore.  If that
	 * happens, we should claim the semaphore if we have
	 * threads waiting for it and we should process any
	 * messages that the modem has enqueued in its fifos
	 * by calling modem_handle_io().
	 */
	if (mmio_sem(mc) == 1) {
		if (!mc->mmio_owner) {
			modem_update_state(mc);
			if (mc->mmio_req_count) {
				mc->mmio_owner = 1;
				wake_up(&mc->wq);
			}
		}

		if (!(cmd & MB_COMMAND))
			modem_handle_io(mc, cmd);

		/* If we have a signal to send and we're not
		 * hanging on to the mmio hw semaphore, give
		 * it back to the modem and send the signal.
		 * Otherwise this will happen when we give up
		 * the mmio hw sem in modem_release_mmio().
		 */
		if (mc->mmio_signal_bits && !mc->mmio_owner) {
			writel(0, mc->mmio + OFF_SEM);
			writel(MB_VALID | mc->mmio_signal_bits,
			       mc->mmio + OFF_MBOX_AP);
			mc->mmio_signal_bits = 0;
		}
	}
done:
	spin_unlock_irqrestore(&mc->lock, flags);
	return IRQ_HANDLED;
}

void modem_force_crash(struct modemctl *mc)
{
	unsigned long int flags;
	pr_info("modem_force_crash() BOOM!\n");
	spin_lock_irqsave(&mc->lock, flags);
	mc->status = MODEM_CRASHED;
	wake_up(&mc->wq);
	spin_unlock_irqrestore(&mc->lock, flags);
}

static int modemctl_dpram_init(struct modemctl *mc)
{
	struct m_tty *mtty;
	int r;
	
	mc->tty = alloc_tty_driver(DPRAM_TTY_NR);
	if (!mc->tty)
		return -ENOMEM;

	mc->tty->owner = THIS_MODULE;
	mc->tty->magic = TTY_DRIVER_MAGIC;
	mc->tty->driver_name = "modemctl";
	mc->tty->name = "dpram";
	mc->tty->major = TTYAUX_MAJOR;
	mc->tty->minor_start = 64;
	mc->tty->num = DPRAM_TTY_NR;
	mc->tty->type = TTY_DRIVER_TYPE_SERIAL;
	mc->tty->subtype = SERIAL_TYPE_NORMAL;
	mc->tty->flags = TTY_DRIVER_REAL_RAW;
	mc->tty->init_termios = tty_std_termios;
	mc->tty->init_termios.c_cflag = B115200 | CS8 | CREAD |
					CLOCAL | HUPCL;
	mc->tty->driver_state = mc;
	tty_set_operations(mc->tty, &dpram_tty_ops);

	r = tty_register_driver(mc->tty);
	if (r)
		goto err_tty;

	mtty = &mc->ttys[DPRAM_TTY_FMT];
	mtty->mc = mc;
	mtty->id = DPRAM_TTY_FMT;
	mtty->size = SIZ_FMT_DATA;
	INIT_WORK(&mtty->work, dpram_recv_work);
	mtty->buffer = kmalloc(SIZ_FMT_DATA, GFP_KERNEL);
	if (!mtty->buffer) {
		dev_err(mc->dev, "Unable to alloc buffer for tty_fmt\n");
		r = -ENOMEM;
		goto err_tty;
	}

	return 0;

err_tty:
	put_tty_driver(mc->tty);
	return r;
}

static void modemctl_dpram_free(struct modemctl *mc)
{
	if (!mc->tty)
		return;

	tty_unregister_driver(mc->tty);
	put_tty_driver(mc->tty);
	mc->tty = NULL;
}

static int modemctl_dpramerr_init(struct modemctl *mc)
{
	mc->errdev.minor = MISC_DYNAMIC_MINOR;
	mc->errdev.name = "dpramerr";
	mc->errdev.fops = &dpram_err_fops;

	return misc_register(&mc->errdev);
}

static void modemctl_dpramerr_free(struct modemctl *mc)
{
	misc_deregister(&mc->errdev);
}

static int __devinit modemctl_probe(struct platform_device *pdev)
{
	struct modemctl *mc;
	struct modemctl_data *pdata;
	struct resource *res;
	int r = 0;

	pdata = pdev->dev.platform_data;

	mc = kzalloc(sizeof(*mc), GFP_KERNEL);
	if (!mc)
		return -ENOMEM;

	mc->dev = &pdev->dev;

	init_waitqueue_head(&mc->wq);
	spin_lock_init(&mc->lock);
	mutex_init(&mc->ctl_lock);

	mc->irq_bp = platform_get_irq_byname(pdev, "active");
	mc->irq_mbox = platform_get_irq_byname(pdev, "onedram");

	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_phone_on = pdata->gpio_phone_on;
	mc->is_cdma_modem = pdata->is_cdma_modem;

	if (gpio_request(mc->gpio_phone_on, "PHONE_ON")) {
		dev_err(&pdev->dev, "Unable to request phone_on gpio\n");
		r = -ENODEV;
		goto err_free;
	}
	gpio_direction_output(mc->gpio_phone_on, 0);

	if (gpio_request(mc->gpio_cp_reset, "CP_RESET")) {
		dev_err(&pdev->dev, "Unable to request cp_reset gpio\n");
		r = -ENODEV;
		goto err_free;
	}
	gpio_direction_output(mc->gpio_cp_reset, 1);

	if (gpio_request(mc->gpio_pda_active, "PDA_ACTIVE")) {
		dev_err(&pdev->dev, "Unable to request pda_active gpio\n");
		r = -ENODEV;
		goto err_free;
	}
	gpio_direction_output(mc->gpio_pda_active, 1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		r = -ENOMEM;
		goto err_free;
	}
	mc->mmbase = res->start;
	mc->mmsize = resource_size(res);

	mc->mmio = ioremap_nocache(mc->mmbase, mc->mmsize);
	if (!mc->mmio) {
		r = -ENOMEM;
		goto err_free;
	}

	platform_set_drvdata(pdev, mc);

	r = modemctl_dpram_init(mc);
	if (r)
		goto err_free;

	r = modemctl_dpramerr_init(mc);
	if (r)
		goto err_dpram;

	mc->status = MODEM_OFF;

	modem_io_init(mc, mc->mmio);
	modem_mpdp_init(mc);

	r = request_irq(mc->irq_bp, modemctl_bp_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"modemctl_bp", mc);
	if (r)
		goto err_dpramerr;

	r = request_irq(mc->irq_mbox, modemctl_mbox_irq_handler,
			IRQF_TRIGGER_FALLING, "modemctl_mbox", mc);
	if (r)
		goto err_irq_bp;

	enable_irq_wake(mc->irq_bp);
	enable_irq_wake(mc->irq_mbox);

	modem_debugfs_init(mc);

	return 0;

err_irq_bp:
	free_irq(mc->irq_bp, mc);
err_dpramerr:
	modemctl_dpramerr_free(mc);
err_dpram:
	modemctl_dpram_free(mc);
	iounmap(mc->mmio);
err_free:
	kfree(mc);
	return r;
}

static int modemctl_suspend(struct device *pdev)
{
	struct modemctl *mc = dev_get_drvdata(pdev);
	gpio_set_value(mc->gpio_pda_active, 0);
	return 0;
}

static int modemctl_resume(struct device *pdev)
{
	struct modemctl *mc = dev_get_drvdata(pdev);
	unsigned long flags;

	gpio_set_value(mc->gpio_pda_active, 1);

	local_irq_save(flags);
	modemctl_mbox_irq_handler(mc->irq_mbox, mc);
	local_irq_restore(flags);

	return 0;
}

static const struct dev_pm_ops modemctl_pm_ops = {
	.suspend    = modemctl_suspend,
	.resume     = modemctl_resume,
};

static struct platform_driver modemctl_driver = {
	.probe = modemctl_probe,
	.driver = {
		.name = "modemctl",
		.pm   = &modemctl_pm_ops,
	},
};

static int __init modemctl_init(void)
{
	return platform_driver_register(&modemctl_driver);
}

module_init(modemctl_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Control Driver");
