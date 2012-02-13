/* modem_io.c
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
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/if.h>
#include <linux/if_arp.h>

#include <linux/wakelock.h>

#include "modem_ctl.h"
#include "modem_ctl_p.h"

/* general purpose fifo access routines */

typedef void * (*copyfunc)(void *, const void *, __kernel_size_t);

static void *copy_to_tty(void *dst, const void *src, __kernel_size_t sz)
{
	int ret;

	while (sz > 0) {
		ret = tty_insert_flip_string((struct tty_struct *) dst, src, sz);
		if (ret < 0) {
			pr_err("modemctl: cannot copy ttydata\n");
			goto end;
		}
		sz -= ret;
	}
end:
	return dst;
}


static unsigned _fifo_read(struct m_fifo *q, void *dst,
			   u16 count, copyfunc copy, bool insert)
{
	unsigned n;
	u16 head = *q->head;
	u16 tail = *q->tail;
	u16 size = q->size;

	DBG_TRACE("_fifo_read: h: %d, t: %d, z: %d, count: %d, space: %d\n",
		  head, tail, size, count, CIRC_CNT(head, tail, size));

	if (CIRC_CNT(head, tail, size) < count)
		return 0;

	n = CIRC_CNT_TO_END(head, tail, size);

	if (likely(n >= count)) {
		copy(dst, q->data + tail, count);
	} else {
		
		copy(dst, q->data + tail, n);
		copy(dst + (insert ? 0 : n), q->data, count - n);
	}
	*q->tail = (tail + count) % size;

	DBG_TRACE("_fifo_read: [O] h: %d, t: %d count: %d\n",
		  *q->head, *q->tail, count);

	return count;
}

static unsigned _fifo_write(struct m_fifo *q, void *src,
			    u16 count, copyfunc copy)
{
	unsigned n;
	u16 head = *q->head;
	u16 tail = *q->tail;
	u16 size = q->size;

	DBG_TRACE("_fifo_write: h: %d, t: %d, z: %d, count: %d, space: %d\n",
	       head, tail, size, count, CIRC_SPACE(head, tail, size));

	if (CIRC_SPACE(head, tail, size) < count)
		return 0;

	n = CIRC_SPACE_TO_END(head, tail, size);

	DBG_TRACE("_fifo_write: space_to_end: %d\n", n);

	if (likely(n >= count)) {
		copy(q->data + head, src, count);
	} else {
		copy(q->data + head, src, n);
		copy(q->data, src + n, count - n);
	}
	*q->head = (head + count) % size;

	DBG_TRACE("_fifo_write: [O] h: %d, t: %d count: %d\n",
		  *q->head, *q->tail, count);

	return count;
}

static void fifo_purge(struct m_fifo *q)
{
	*q->tail = *q->head;
}

static unsigned fifo_skip(struct m_fifo *q, unsigned count)
{
	if (CIRC_CNT(*q->head, *q->tail, q->size) < count)
		return 0;
	*q->tail = (*q->tail + count) & q->size;
	return count;
}

#define fifo_read(q, dst, count) \
	_fifo_read(q, dst, count, memcpy, true)
#define fifo_read_tty(q, dst, count) \
	_fifo_read(q, dst, count, copy_to_tty, true)

#define fifo_write(q, src, count) \
	_fifo_write(q, src, count, memcpy)

#define fifo_count(mf) CIRC_CNT(*(mf)->head, *(mf)->tail, (mf)->size)
#define fifo_space(mf) CIRC_SPACE(*(mf)->head, *(mf)->tail, (mf)->size)

static void fifo_dump(const char *tag, struct m_fifo *q,
		      unsigned start, unsigned count)
{
	if (count > 64)
		count = 64;

	if ((start + count) <= q->size) {
		print_hex_dump_bytes(tag, DUMP_PREFIX_ADDRESS,
				     q->data + start, count);
	} else {
		print_hex_dump_bytes(tag, DUMP_PREFIX_ADDRESS,
				     q->data + start, q->size - start);
		print_hex_dump_bytes(tag, DUMP_PREFIX_ADDRESS,
				     q->data, count - (q->size - start));
	}
}

/* Called with mc->lock held whenever we gain access
 * to the mmio region.
 */
void modem_update_state(struct modemctl *mc)
{
	/* update our idea of space available in fifos */
	mc->fmt_tx.avail = fifo_space(&mc->fmt_tx);
	mc->fmt_rx.avail = fifo_count(&mc->fmt_rx);
	if (mc->fmt_rx.avail)
		wake_lock(&mc->cmd_pipe.wakelock);
	else
		wake_unlock(&mc->cmd_pipe.wakelock);

	mc->raw_tx.avail = fifo_space(&mc->raw_tx);
	mc->raw_rx.avail = fifo_count(&mc->raw_rx);
	if (mc->raw_rx.avail)
		wake_lock(&mc->raw_pipe.wakelock);
	else
		wake_unlock(&mc->raw_pipe.wakelock);

	DBG_TRACE("modem_update_state: "
		  "[fmt_tx] h: %d, t: %d, sz: %d "
		  "[fmt_rx] h: %d, t: %d, sz: %d "
		  "[raw_tx] h: %d, t: %d, sz: %d "
		  "[raw_rx] h: %d, t: %d, sz: %d ",
		  *mc->fmt_tx.head, *mc->fmt_tx.tail, mc->fmt_tx.avail,
		  *mc->fmt_rx.head, *mc->fmt_rx.tail, mc->fmt_rx.avail,
		  *mc->raw_tx.head, *mc->raw_tx.tail, mc->raw_tx.avail,
		  *mc->raw_rx.head, *mc->raw_rx.tail, mc->raw_rx.avail);

	/* wake up blocked or polling read/write operations */
	wake_up(&mc->wq);
}

void modem_update_pipe(struct m_pipe *pipe)
{
	unsigned long flags;
	spin_lock_irqsave(&pipe->mc->lock, flags);
	pipe->tx->avail = fifo_space(pipe->tx);
	pipe->rx->avail = fifo_count(pipe->rx);
	if (pipe->rx->avail)
		wake_lock(&pipe->wakelock);
	else
		wake_unlock(&pipe->wakelock);

	DBG_TRACE("modem_update_state: "
		  "[tx] h: %d, t: %d, sz: %d "
		  "[rx] h: %d, t: %d, sz: %d "
		  *pipe->tx->head, *pipe->tx->tail, pipe->tx->avail,
		  *pipe->rx->head, *pipe->rx->tail, pipe->rx->avail);

	spin_unlock_irqrestore(&pipe->mc->lock, flags);
}


/* must be called with pipe->tx_lock held */
int modem_pipe_send(struct m_pipe *pipe, struct modem_io *io)
{
	char hdr[M_PIPE_MAX_HDR];
	static char ftr = 0x7e;
	unsigned size = io->size;
	int ret;

	if (!io->raw) {
		ret = pipe->push_header(io, hdr);
		if (ret)
			return ret;
		size += pipe->header_size + 1;
	}
	
	if (size > 0x10000000)
		return -EINVAL;
	if (size >= (pipe->tx->size - 1))
		return -EINVAL;

	for (;;) {
		ret = modem_acquire_mmio(pipe->mc);
		if (ret)
			return ret;

		modem_update_pipe(pipe);

		if (pipe->tx->avail >= size) {
			if (!io->raw)
				fifo_write(pipe->tx, hdr, pipe->header_size);
			fifo_write(pipe->tx, io->data, io->size);
			if (!io->raw)
				fifo_write(pipe->tx, &ftr, 1);
			modem_update_pipe(pipe);
			modem_release_mmio(pipe->mc, pipe->tx->bits);
			MODEM_COUNT(pipe->mc, pipe_tx);
			return 0;
		}

		pr_info("modem_pipe_send: wait for space\n");
		MODEM_COUNT(pipe->mc, pipe_tx_delayed);
		modem_release_mmio(pipe->mc, 0);

		ret = wait_event_interruptible_timeout(
			pipe->mc->wq,
			(pipe->tx->avail >= size) || modem_offline(pipe->mc),
			5 * HZ);
		if (ret == 0)
			return -ENODEV;
		if (ret < 0)
			return ret;
	}

	return 0;
}

int modem_pipe_read_tty(struct m_pipe *pipe, struct modem_io *io)
{
	char hdr[M_PIPE_MAX_HDR];
	int ret, size;

	if (fifo_read(pipe->rx, hdr, pipe->header_size) == 0)
		return -EAGAIN;

	ret = pipe->pull_header(io, hdr);
	if (ret)
		return ret;

	if (io->filter && !io->filter(io)) {
		if (fifo_skip(pipe->rx, io->size + 1) != (io->size + 1))
			return -EIO;
	} else {
		size = io->size;
		if (io->raw) {
			copy_to_tty(io->data, hdr, pipe->header_size);
			size++;
		}

		if (fifo_read_tty(pipe->rx, io->data, size) != size)
			return -EIO;
		if (!io->raw && fifo_skip(pipe->rx, 1) != 1)
			return -EIO;
	}

	return 0;
}

/* must be called with pipe->rx_lock held */
int modem_pipe_recv_tty(struct m_pipe *pipe, struct modem_io *io)
{
	int ret;

	ret = modem_acquire_mmio(pipe->mc);
	if (ret)
		return ret;

	ret = modem_pipe_read_tty(pipe, io);

	modem_update_pipe(pipe);

	if ((ret != 0) && (ret != -EAGAIN)) {
		pr_err("[MODEM] purging %s fifo\n", pipe->dev.name);
		fifo_purge(pipe->rx);
		MODEM_COUNT(pipe->mc, pipe_rx_purged);
	} else if (ret == 0) {
		MODEM_COUNT(pipe->mc, pipe_rx);
		tty_flip_buffer_push(io->data);
	}

	modem_release_mmio(pipe->mc, 0);

	return ret;
}


int modem_pipe_read(struct m_pipe *pipe, struct modem_io *io)
{
	unsigned data_size = io->size;
	char hdr[M_PIPE_MAX_HDR];
	char *buf = io->data;
	int ret, size;

	if (fifo_read(pipe->rx, hdr, pipe->header_size) == 0)
		return -EAGAIN;

	ret = pipe->pull_header(io, hdr);
	if (ret) {
#ifdef MODEM_DEBUG_PACKET
		memcpy(buf, hdr, pipe->header_size);
		modem_update_pipe(pipe);
		fifo_read(pipe->rx, buf + pipe->header_size, pipe->rx->avail);
		print_hex_dump_bytes(tag, DUMP_PREFIX_ADDRESS,
				     buf, pipe->header_size + pipe->rx->avail);
#endif
		return ret;
	}

	if (io->filter && !io->filter(io)) {
		if (fifo_skip(pipe->rx, io->size + 1) != (io->size + 1))
			return -EIO;
	} else {
		size = (io->raw ? (io->size + pipe->header_size + 1) : (io->size));
		if (data_size < size) {
			pr_info("modem_pipe_read: discarding packet (%d)\n", io->size);
			if (fifo_skip(pipe->rx, io->size + 1) != (io->size + 1))
				return -EIO;
			return -EAGAIN;
		}

		io->size = size;

		if (io->raw) {
			memcpy(buf, hdr, pipe->header_size);
			buf += pipe->header_size;
			size -= pipe->header_size;
		}

		if (fifo_read(pipe->rx, buf, size) != size)
			return -EIO;
		if (!io->raw && fifo_skip(pipe->rx, 1) != 1)
			return -EIO;
	}

	return 0;
}

/* must be called with pipe->rx_lock held */
int modem_pipe_recv(struct m_pipe *pipe, struct modem_io *io)
{
	int ret;

	ret = modem_acquire_mmio(pipe->mc);
	if (ret)
		return ret;

	ret = modem_pipe_read(pipe, io);

	modem_update_pipe(pipe);

	if ((ret != 0) && (ret != -EAGAIN)) {
		pr_err("[MODEM] purging %s fifo\n", pipe->dev.name);
		fifo_purge(pipe->rx);
		MODEM_COUNT(pipe->mc, pipe_rx_purged);
	} else if (ret == 0) {
		MODEM_COUNT(pipe->mc, pipe_rx);
	}

	modem_release_mmio(pipe->mc, 0);

	return ret;
}

int modem_pipe_purge(struct m_pipe *pipe)
{
	int ret;

	ret = modem_acquire_mmio(pipe->mc);
	if (ret)
		return ret;

	pr_err("[MODEM] purging %s fifo\n", pipe->dev.name);
	fifo_purge(pipe->rx);
	MODEM_COUNT(pipe->mc, pipe_rx_purged);

	modem_release_mmio(pipe->mc, 0);

	return ret;
}

struct fmt_hdr {
	u8 start;
	u16 len;
	u8 control;
} __attribute__ ((packed));

static int push_fmt_header(struct modem_io *io, void *header)
{
	struct fmt_hdr *fh = header;

	if (io->id)
		return -EINVAL;
	if (io->cmd)
		return -EINVAL;
	fh->start = 0x7f;
	fh->len = io->size + 3;
	fh->control = 0;
	return 0;
}

static int pull_fmt_header(struct modem_io *io, void *header)
{
	struct fmt_hdr *fh = header;

	DBG_TRACE("pull_fmt_header: start: %x, control: %x, len: %d\n",
		  fh->start, fh->control, fh->len);

	if (fh->start != 0x7f)
		return -EINVAL;
	if (fh->control != 0x00)
		return -EINVAL;
	if (fh->len < 3)
		return -EINVAL;
	io->size = fh->len - 3;
	io->id = 0;
	io->cmd = 0;
	return 0;
}

struct raw_hdr {
	u8 start;
	u32 len;
	u8 channel;
	u8 control;
} __attribute__ ((packed));

static int push_raw_header(struct modem_io *io, void *header)
{
	struct raw_hdr *rh = header;

	if (io->id > 0xFF)
		return -EINVAL;
	if (io->cmd > 0xFF)
		return -EINVAL;
	rh->start = 0x7f;
	rh->len = io->size + 6;
	rh->channel = io->id;
	rh->control = io->cmd;
	return 0;
}

static int pull_raw_header(struct modem_io *io, void *header)
{
	struct raw_hdr *rh = header;

	DBG_TRACE("pull_raw_header: start: %x, channel: %x, len: %d\n",
		  rh->start, rh->channel, rh->len);

	if (rh->start != 0x7f)
		return -EINVAL;
	if (rh->len < 6)
		return -EINVAL;
	io->size = rh->len - 6;
	io->id = rh->channel;
	io->cmd = rh->control;
	return 0;
}

static int modem_pipe_register(struct modemctl *mc, struct m_pipe *pipe, const char *devname, int tty)
{
	if (tty >= DPRAM_TTY_NR)
		return -EINVAL;

	wake_lock_init(&pipe->wakelock, WAKE_LOCK_SUSPEND, devname);

	mutex_init(&pipe->tx_lock);
	mutex_init(&pipe->rx_lock);

	if (tty >= 0)
		mc->ttys[tty].pipe = pipe;

	return 0;
}

int modem_io_init(struct modemctl *mc, void __iomem *mmio)
{
	INIT_M_FIFO(mc->fmt_tx, FMT, TX, mmio);
	INIT_M_FIFO(mc->fmt_rx, FMT, RX, mmio);
	INIT_M_FIFO(mc->raw_tx, RAW, TX, mmio);
	INIT_M_FIFO(mc->raw_rx, RAW, RX, mmio);

	mc->cmd_pipe.tx = &mc->fmt_tx;
	mc->cmd_pipe.rx = &mc->fmt_rx;
	mc->cmd_pipe.tx->bits = MBD_SEND_FMT;
	mc->cmd_pipe.rx->bits = MBD_SEND_FMT;
	mc->cmd_pipe.rx->req_ack_bits = MBC_REQ_ACK_FMT;
	mc->cmd_pipe.rx->res_ack_bits = MBC_RES_ACK_FMT;
	mc->cmd_pipe.rx->ack_requested = 0;
	mc->cmd_pipe.push_header = push_fmt_header;
	mc->cmd_pipe.pull_header = pull_fmt_header;
	mc->cmd_pipe.header_size = sizeof(struct fmt_hdr);
	mc->cmd_pipe.mc = mc;
	if (modem_pipe_register(mc, &mc->cmd_pipe, "modem_fmt", DPRAM_TTY_FMT))
		pr_err("failed to register modem_fmt pipe\n");

	mc->raw_pipe.tx = &mc->raw_tx;
	mc->raw_pipe.rx = &mc->raw_rx;
	mc->raw_pipe.tx->bits = MBD_SEND_RAW;
	mc->raw_pipe.rx->bits = MBD_SEND_RAW;
	mc->raw_pipe.rx->req_ack_bits = MBC_REQ_ACK_RAW;
	mc->raw_pipe.rx->res_ack_bits = MBC_RES_ACK_RAW;
	mc->raw_pipe.rx->ack_requested = 0;
	mc->raw_pipe.push_header = push_raw_header;
	mc->raw_pipe.pull_header = pull_raw_header;
	mc->raw_pipe.header_size = sizeof(struct raw_hdr);
	mc->raw_pipe.mc = mc;
	if (modem_pipe_register(mc, &mc->raw_pipe, "modem_raw", -1))
		pr_err("failed to register modem_raw pipe\n");

	return 0;
}