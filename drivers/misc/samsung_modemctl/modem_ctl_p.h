/*
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

#ifndef __MODEM_CONTROL_P_H__
#define __MODEM_CONTROL_P_H__

#define DPRAM_MAJOR		255

#define DPRAM_TTY_NR		1
#define DPRAM_TTY_FMT		0

#define MODEM_OFF		0
#define MODEM_CRASHED		1
#define MODEM_RAMDUMP		2
#define MODEM_POWER_ON		3
#define MODEM_BOOTING_NORMAL	4
#define MODEM_BOOTING_RAMDUMP	5
#define MODEM_DUMPING		6
#define MODEM_RUNNING		7

#define modem_offline(mc) ((mc)->status < MODEM_POWER_ON)
#define modem_running(mc) ((mc)->status == MODEM_RUNNING)

#define M_PIPE_MAX_HDR 16


/* protocol definitions */
#define MB_VALID		0x0080
#define MB_COMMAND		0x0040

/* CMD_INIT_END extended bit */
#define CP_BOOT_ONLINE		0x0000
#define CP_BOOT_AIRPLANE	0x1000
#define AP_OS_ANDROID		0x0100
#define AP_OS_WINMOBILE		0x0200
#define AP_OS_LINUX		0x0300
#define AP_OS_SYMBIAN		0x0400

/* CMD_PHONE_START extended bit */
#define CP_QUALCOMM		0x0100
#define CP_INFINEON		0x0200
#define CP_BROADCOM		0x0300

#define MBC_NONE		0x0000
#define MBC_INIT_START		0x0001
#define MBC_INIT_END		0x0002
#define MBC_REQ_ACTIVE		0x0003
#define MBC_RES_ACTIVE		0x0004
#define MBC_TIME_SYNC		0x0005
#define MBC_POWER_OFF		0x0006
#define MBC_RESET		0x0007
#define MBC_PHONE_START		0x0008
#define MBC_ERR_DISPLAY		0x0009
#define MBC_SUSPEND		0x000A
#define MBC_RESUME		0x000B
#define MBC_EMER_DOWN		0x000C
#define MBC_REQ_SEM		0x000D
#define MBC_RES_SEM		0x000E
#define MBC_MAX			0x000F

/* data mailbox flags */
#define MBC_REQ_ACK_FMT		0x0020
#define MBC_REQ_ACK_RAW		0x0010
#define MBC_RES_ACK_FMT		0x0008
#define MBC_RES_ACK_RAW		0x0004
#define MBD_SEND_FMT		0x0002
#define MBD_SEND_RAW		0x0001

#define MODEM_MSG_SBL_DONE		0x12341234
#define MODEM_CMD_BINARY_LOAD		0x45674567
#define MODEM_MSG_BINARY_DONE		0xabcdabcd

#define MODEM_CMD_RAMDUMP_START		0xDEADDEAD
#define MODEM_MSG_RAMDUMP_LARGE		0x0ADD0ADD // 16MB - 2KB
#define MODEM_CMD_RAMDUMP_MORE		0xEDEDEDED
#define MODEM_MSG_RAMDUMP_SMALL		0xFADEFADE // 5MB + 4KB

#define RAMDUMP_LARGE_SIZE	(16*1024*1024 - 2*1024)
#define RAMDUMP_SMALL_SIZE	(5*1024*1024 + 4*1024)

#define SIZ_FMT_DATA		4092
#define SIZ_RAW_DATA		12272
#define SIZ_ERROR_MSG		65

/* onedram shared memory map */
#define OFF_MAGIC		0x00000000
#define OFF_ACCESS		0x00000002

#define OFF_FMT_TX_HEAD		(OFF_ACCESS + 2)
#define OFF_FMT_TX_TAIL		(OFF_FMT_TX_HEAD + 2)
#define OFF_FMT_TX_DATA		(OFF_FMT_TX_TAIL + 2)

#define OFF_RAW_TX_HEAD		(OFF_FMT_TX_DATA + SIZ_FMT_DATA)
#define OFF_RAW_TX_TAIL		(OFF_RAW_TX_HEAD + 2)
#define OFF_RAW_TX_DATA		(OFF_RAW_TX_TAIL + 2)

#define OFF_FMT_RX_HEAD		(OFF_RAW_TX_DATA + SIZ_RAW_DATA)
#define OFF_FMT_RX_TAIL		(OFF_FMT_RX_HEAD + 2)
#define OFF_FMT_RX_DATA		(OFF_FMT_RX_TAIL + 2)

#define OFF_RAW_RX_HEAD		(OFF_FMT_RX_DATA + SIZ_FMT_DATA)
#define OFF_RAW_RX_TAIL		(OFF_RAW_RX_HEAD + 2)
#define OFF_RAW_RX_DATA		(OFF_RAW_RX_TAIL + 2)

#define OFF_ERROR_MSG		OFF_FMT_RX_DATA

/* onedram registers */

/* Mailboxes are named based on who writes to them.
 * MBOX_BP is written to by the (B)aseband (P)rocessor
 * and only readable by the (A)pplication (P)rocessor.
 * MBOX_AP is the opposite.
 */
#define OFF_SEM		0xFFF800
#define OFF_MBOX_BP	0xFFF820
#define OFF_MBOX_AP	0xFFF840
#define OFF_CHECK_BP	0xFFF8A0
#define OFF_CHECK_AP	0xFFF8C0

struct net_device;

struct m_pipe {
	int (*push_header)(struct modem_io *io, void *header);
	int (*pull_header)(struct modem_io *io, void *header);

	unsigned header_size;

	struct m_fifo *tx;
	struct m_fifo *rx;

	struct modemctl *mc;
	unsigned ready;

	struct miscdevice dev;

	struct mutex tx_lock;
	struct mutex rx_lock;

	struct wake_lock wakelock;
};
#define to_m_pipe(misc) container_of(misc, struct m_pipe, dev)

struct m_fifo {
	u16 *head;
	u16 *tail;
	u16 size;
	void *data;

	unsigned avail;
	unsigned bits;
	unsigned req_ack_bits;
	unsigned res_ack_bits;
	unsigned ack_requested;
	unsigned unused1;
	unsigned unused2;
};

struct m_tty {
	struct modemctl *mc;
	struct tty_struct *tty;
	int id;
	int size;
	atomic_t open_count;
	char *buffer;
	struct m_pipe *pipe;
	struct work_struct work;
};

struct m_multipdp;

struct m_pdp {
	unsigned channel;
	char *name;
	struct tty_driver *tty_driver;
	struct tty_struct *tty;
	atomic_t open_count;
	struct m_multipdp *mpdp;
};

struct m_multipdp {
	struct modemctl *mc;
	struct m_pipe *pipe;
	struct m_pdp *pdp;
	int pdp_nr;
	struct work_struct work;
	int size;
};

struct modemstats {
	unsigned request_no_wait;
	unsigned request_wait;

	unsigned release_no_action;
	unsigned release_bp_waiting;
	unsigned release_bp_signaled;

	unsigned bp_req_instant;
	unsigned bp_req_delayed;
	unsigned bp_req_confused;

	unsigned rx_unknown;
	unsigned rx_dropped;
	unsigned rx_purged;
	unsigned rx_received;

	unsigned tx_no_delay;
	unsigned tx_queued;
	unsigned tx_bp_signaled;
	unsigned tx_fifo_full;

	unsigned pipe_tx;
	unsigned pipe_rx;
	unsigned pipe_tx_delayed;
	unsigned pipe_rx_purged;

	unsigned resets;
};

#define MODEM_COUNT(mc,s) (((mc)->stats.s)++)

struct modemctl {
	struct device *dev;
	void __iomem *mmio;
	struct modemstats stats;

	struct tty_driver *tty;
	struct m_tty ttys[DPRAM_TTY_NR];

	struct miscdevice errdev;
	char errbuf[SIZ_ERROR_MSG + 4];
	size_t errlen;
	bool errpolled;

	/* lock and waitqueue for shared memory state */
	spinlock_t lock;
	wait_queue_head_t wq;

	/* shared memory semaphore management */
	unsigned mmio_req_count;
	unsigned mmio_bp_request;
	unsigned mmio_owner;
	unsigned mmio_signal_bits;

	struct m_fifo fmt_tx;
	struct m_fifo fmt_rx;
	struct m_fifo raw_tx;
	struct m_fifo raw_rx;

	int status;
	bool initialized;

	unsigned mmbase;
	unsigned mmsize;

	int irq_bp;
	int irq_mbox;

	unsigned gpio_phone_active;
	unsigned gpio_pda_active;
	unsigned gpio_cp_reset;
	unsigned gpio_phone_on;
	bool is_cdma_modem;
	bool is_modem_delta_update;
	unsigned dpram_prev_phone_active;
	unsigned dpram_prev_status;

	struct m_pipe cmd_pipe;
	struct m_pipe raw_pipe;

	struct m_multipdp raw_mpdp;

	struct mutex ctl_lock;
	ktime_t mmio_t0;

	/* used for ramdump mode */
	unsigned ramdump_size;
	loff_t ramdump_pos;

	unsigned logdump;
	unsigned logdump_data;
};
#define to_modemctl(misc) container_of(misc, struct modemctl, dev)
#define errdev_to_modemctl(misc) container_of(misc, struct modemctl, errdev)


/* called when semaphore is held and there may be io to process */
//void modem_handle_io(struct modemctl *mc);
void modem_update_state(struct modemctl *mc);

/* called once at probe() */
int modem_io_init(struct modemctl *mc, void __iomem *mmio);
int modem_mpdp_init(struct modemctl *mc);

/* called when modem boots and goes offline */
void modem_io_enable(struct modemctl *mc);
void modem_io_disable(struct modemctl *mc);


/* Block until control of mmio area is obtained (0)
 * or interrupt (-ERESTARTSYS) or failure (-ENODEV)
 * occurs.
 */
int modem_acquire_mmio(struct modemctl *mc);

/* Request control of mmio area.  Returns 1 if
 * control obtained, 0 if not (request pending).
 * Either way, release_mmio() must be called to
 * balance this.
 */
int modem_request_mmio(struct modemctl *mc);

/* Return control of mmio area once requested
 * by modem_request_mmio() or acquired by a
 * successful modem_acquire_mmio().
 *
 * The onedram semaphore is only actually returned
 * to the BP if there is an outstanding request
 * for it from the BP, or if the bits argument
 * to one of the release_mmio() calls was nonzero.
 */
void modem_release_mmio(struct modemctl *mc, unsigned bits);

/* Send a request for the hw mmio sem to the modem.
 * Used ONLY by the internals of modem_request_mmio() and
 * some trickery in vnet_xmit().  Please do not use elsewhere.
 */
void modem_request_sem(struct modemctl *mc);

/* update pipe counter */
void modem_update_pipe(struct m_pipe *pipe);

/* send to pipe */
int modem_pipe_send(struct m_pipe *pipe, struct modem_io *io);

/* recv pipe to tty */
int modem_pipe_recv_tty(struct m_pipe *pipe, struct modem_io *io);

/* recv pipe to buffer */
int modem_pipe_recv(struct m_pipe *pipe, struct modem_io *io);

/* purge pipe */
int modem_pipe_purge(struct m_pipe *pipe);

/* internal glue */
void modem_debugfs_init(struct modemctl *mc);
void modem_force_crash(struct modemctl *mc);

#define INIT_M_FIFO(name, type, dir, base) \
	name.head = base + OFF_##type##_##dir##_HEAD; \
	name.tail = base + OFF_##type##_##dir##_TAIL; \
	name.data = base + OFF_##type##_##dir##_DATA; \
	name.size = SIZ_##type##_DATA;

/* Circular buffer - based on include/linux/circ_buf.h */

/* Return count in buffer.  */
#define CIRC_CNT(head,tail,size) \
	({int n = ((head) - (tail)); \
	  n >= 0 ? n : ((size) + n);})

/* Return space available, 0..size-1.  We always leave one free char
   as a completely full buffer has head == tail, which is the same as
   empty.  */
#define CIRC_SPACE(head,tail,size) CIRC_CNT((tail),((head)+1),(size))

/* Return count up to the end of the buffer.  Carefully avoid
   accessing head and tail more than once, so they can change
   underneath us without returning inconsistent results.  */
#define CIRC_CNT_TO_END(head,tail,size) \
	({int end = (size) - (tail); \
	  int n = ((head) + end) % (size); \
	  n < end ? n : end;})

/* Return space available up to the end of the buffer.  */
#define CIRC_SPACE_TO_END(head,tail,size) \
	({int end = (size) - 1 - (head); \
	  int n = (end + (tail)) % (size); \
	  n <= end ? n : end+1;})

/* Debug */

#ifdef MODEM_DEBUG_TRACE
#define DBG_TRACE(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DBG_TRACE(fmt, ...)
#endif

#endif
