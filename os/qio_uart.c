/*
 *	qio_uart.c -- Dave Shepperd -- Nov 1996
 *	Stolen mostly from ioa_uart.c.
 *
 *	QIO Driver for the I/O ASIC uart on Phoenix/Flagstaff.
 *
 *		Copyright 1996 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

/*
 *	Functions:
 *		static void uart_irq()
 *		static void uart_ast(void *status)
 *		void qio_uart_init()
 */

#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>
#include <phx_proto.h>
#include <intvecs.h>
#include <ioa_uart.h>
#include <uart.h>
#include <qio.h>
#include <fcntl.h>
#include <string.h>

#ifndef n_elems
# define n_elems(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifndef NUM_UARTS
# define NUM_UARTS 1
#endif

#ifndef NCCS
# define NCCS	23
#endif

#ifndef UART_RB_LEN
#define UART_RB_LEN  64
#endif

#define UF_RBF 0x01		/* Receive buffer full */
#define UF_TXE 0x02		/* Transmitter interrupt enabled */

#define	STATIC	static

typedef struct uart_device {
    VU16 *ctlp;		/* pointer to control register */
    VU16 *xmitp;	/* pointer to xmitter register */
    VU16 *recvp;	/* pointer to receiver register */
    QioMutex readm;	/* read mutex */
    QioMutex writem;	/* write mutex */
    QioIOQ *xmit;	/* current xmit info */
    QioIOQ *recv;	/* current receive info */
    int sent_cr;	/* signal we've inserted a cr */
    U16 tr_buf[UART_RB_LEN]; /* typeahead buffer */
    VU8 tr_in;		/* where irq puts char */
    VU8 tr_out;		/* where AST routine removes char */
    int uf;		/* flags used by interrupt service routine */
    int dropped;	/* number of dropped chars due to errors or TA buffer full */
    int intr_bits;
    int c_iflag;	/* termios interfaces:	input modes */
    int c_oflag;	/* 			output modes */
    int	c_cflag;	/* 			control modes */
    int c_lflag;	/* 			line discipline modes */
    char c_cc[NCCS];	/* 			control chars */
} UartDevice;

STATIC UartDevice uart_devices[NUM_UARTS];

struct c_flags {
    int qio_flag;
    int uart_flag;
    int polarity;
};

/* This table is set so a value of 0 in the appropriate bit in the c_cflag
 * field above is the default. One needs to 'set' the bit to select something
 * other than the default.
 */

STATIC const struct c_flags xlate_cflags[] = {
    { QIO_UART_CFLG_DISAB, IO_UART_CTL_INTERNAL_ENA, QIO_UART_CFLG_DISAB },
    { QIO_UART_CFLG_CTSIN, IO_UART_CTL_IGNORE_CTS_IN, QIO_UART_CFLG_CTSIN },
    { QIO_UART_CFLG_CTSOUT, IO_UART_CTL_FORCE_CTS_OUT, 0 },
    { QIO_UART_CFLG_BREAK, IO_UART_CTL_FORCE_BREAK_OUT, 0 },
    { QIO_UART_CFLG_XDISAB, IO_UART_CTL_DISABLE_XMITTER, 0 },
    { QIO_UART_CFLG_LOOPB, IO_UART_CTL_ENABLE_LOOP_BACK, 0 },
    { QIO_UART_CFLG_NOERRS, IO_UART_CTL_ENABLE_ERRORS, QIO_UART_CFLG_NOERRS },
    { QIO_UART_CFLG_GP, IO_UART_CTL_GP_OUT, 0 }
};

/*
 * uart_irq - Interrupt service routine saves the interrupt status,
 *	     services the hardware, and queues the AST routine.
 */
STATIC void uart_irq(void) {
    UartDevice *ud;
    QioIOQ *ioq;
    U32 intr;	/* Bits which actually caused this interrupt */

    intr = IO_STATUS & IO_CONTROL & UART_INT;

    if ( intr ) {
#if NUM_UARTS > 1
# error ** You need to fixup the interrupt service routine for multiple UARTs
#endif
	ud = uart_devices;    
	if ( intr & UART_RCV ) {
	    int ii, in;

	    ioq = ud->recv;			/* point to current receiver */
	    for ( ii = 0; ii < FIFO_SIZE; ++ii ) {
		U32 rc = *ud->recvp;

		if (!(rc&CHAR_RCV)) break;	/* no more chars to receive */
		if ( ud->tr_in == ud->tr_out && ioq ) {	/* if TA buf empty and user is waiting */
		    char *s;
		    s = ioq->pparam0;
		    *s++ = rc;
		    ioq->pparam0 = (void *)s;
		    ++ioq->iocount;		/* tell 'em he has it */
		    if (--ioq->iparam0 <= 0) {	/* count down how much he asked for */
			ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
			ud->recv = 0;		/* have given him all he wants, burn our bridge */
			qio_freemutex(&ud->readm, ioq); /* let someone else use the device */
			qio_complete(ioq);	/* tell user we're done */
			ioq = 0;		/* fill typeahead buffer from here on */
		    }
		    continue;
		}
#if (UART_RB_LEN & -UART_RB_LEN) == UART_RB_LEN
		in = (ud->tr_in + 1) & (UART_RB_LEN-1);
#else
		in = ud->tr_in + 1;
		if (in >= UART_RB_LEN ) in = 0;
#endif
		if ( in == ud->tr_out ) {	/* if no room at the inn */
		    ud->uf |= UF_RBF;		/* signal overflow */
		    intr |= ( UF_RBF << 16 );
		    ++ud->dropped;		/* discarded a character */
		} else {
		    ud->tr_buf[ in ] = (U16)rc;
		    ud->tr_in = in;
		}
	    }
	}

	if ( intr & UART_XMT ) {
	    ioq = ud->xmit;
	    if ( ioq ) {
		if ( ioq->iparam0 > 0 ) {
		    char c, *s;
		    s = (char *)ioq->pparam0;
		    c = *s++;
		    if (!ud->sent_cr && c == '\n') {	/* for now, always send a cr before a lf */
			c = '\r';
			ud->sent_cr = 1;
		    } else {
			ud->sent_cr = 0;
			ioq->pparam0 = (void *)s;
			--ioq->iparam0;
			++ioq->iocount;
		    }
		    *ud->xmitp = c;
		} else {
		    IO_CONTROL &= ~UART_XMT;
		    ud->uf &= ~UF_TXE;
		    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
		    ud->xmit = 0;
		    qio_freemutex(&ud->writem, ioq);
		    qio_complete(ioq);
		}
	    } else {
		IO_CONTROL &= ~UART_XMT;
		ud->uf &= ~UF_TXE;
	    }
	}
    }
}

static void cflags_to_uart( UartDevice *ud ) {
    const struct c_flags *cf = xlate_cflags;
    U32 result;
    int ii;

    result = ((ud->c_cflag >> QIO_UART_CFLG_BAUD_V) & QIO_UART_CFLG_BAUD_M) & 0x7F;
    for (ii=0; ii < n_elems(xlate_cflags); ++ii, ++cf) {
	if (((ud->c_cflag&cf->qio_flag) ^ cf->polarity) != 0) {
	    result |= cf->uart_flag;
	}
    }
    *ud->ctlp = result;		/* stuff result into UART control register */
}
	    
/*
 * u_init - Resets the UART, loads the interrupt vector, loads the
 *	   action routine, initializes the UART and enables the
 *	   receive interrupt.
 */
STATIC void u_init( const QioDevice *qp ) {
    UartDevice *ud;

#if NUM_UARTS > 1
# error ** This init code is only gonna work for 1 uart.
#endif

    qio_install_dvc( qp );
    ud = uart_devices + qp->unit;
    ud->ctlp = (VU16 *)&UART_CONTROL;
    ud->xmitp = (VU16 *)&UART_XMITTER;
    ud->recvp = (VU16 *)&UART_RECEIVER;
    *ud->ctlp = 0;				/* Reset the UART */
    prc_set_vec(UART_INTVEC, uart_irq);		/* Load the I/O ASIC Vector */
#if defined(BR9600)
    ud->c_cflag = BR9600 << QIO_UART_CFLG_BAUD_V; /* default the baud rate to 9600 */
#else
    ud->c_cflag = IO_UART_CTL_9600_BAUD << QIO_UART_CFLG_BAUD_V;
#endif
    cflags_to_uart( ud );			/* convert default cflags to uart bits */

    IO_CONTROL |= UART_RCV;			/* Start the receiver */

    return;
}

static void read_q(QioIOQ *ioq) {
    UartDevice *ud;

    ud = (UartDevice *)ioq->pparam1;
    ud->recv = ioq;			/* tell interrupt routine we're running */
    while ( ud->tr_out != ud->tr_in && ioq->iparam0 > 0) {
	U16 c;
	char *s;
	int out;
#if (UART_RB_LEN & -UART_RB_LEN) == UART_RB_LEN
	out = (ud->tr_out + 1) & (UART_RB_LEN-1);
#else
	out = ud->tr_out + 1;
	if (out >= UART_RB_LEN ) out = 0;
#endif
	c = ud->tr_buf[ out ];
	ud->tr_out = out;		/* let interrupt know what we're up to */
	s = (char *)ioq->pparam0;
	*s++ = c;
	ioq->pparam0 = (void *)s;
	--ioq->iparam0;
	++ioq->iocount;
    }
    if (!ioq->iparam0) {
	ud->recv = 0;			/* off the list */
	ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
	qio_freemutex(&ud->readm, ioq);
	qio_complete(ioq);
    }
    return;
}
    
static int uart_readwpos(QioIOQ *ioq, size_t where, void *buf, int len) {
    UartDevice *uart;
    const QioDevice *dvc;
    QioFile *file;
    int sts;
    
    file = qio_fd2file(ioq->file);
    if (!(file->mode&_FREAD)) return (ioq->iostatus = QIO_NOT_ORD); /* file not opened for read, reject it */
    if (len == 0) {
	ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    }
    if (ioq->iostatus) {		/* count == 0 call completion routine */
	ioq->iocount = 0;
	qio_complete(ioq);
	return 0;
    }
    dvc = file->dvc;
    uart = (UartDevice *)dvc->private;
    if (!uart) return (ioq->iostatus = QIO_FATAL); /* uart_init probably not run */
    ioq->pparam0 = buf;
    ioq->iparam0 = len;
    ioq->pparam1 = (void *)uart;
    sts = qio_getmutex(&uart->readm, read_q, ioq);
    return sts;
}

static int uart_read(QioIOQ *ioq, void *buf, int len) {
    return uart_readwpos(ioq, 0, buf, len);
}

static void write_q(QioIOQ *ioq) {
    UartDevice *ud;
    int oldipl;

    ud = (UartDevice *)ioq->pparam1;
    ud->xmit = ioq;			/* tell interrupt routine we're running */
    oldipl = prc_set_ipl(INTS_OFF);
    if (!(ud->uf&UF_TXE)) {
	char *s;
	s = ioq->pparam0;
	*ud->xmitp = *s++;
	ioq->pparam0 = (void *)s;
	IO_CONTROL |= UART_XMT;
	++ioq->iocount;
	--ioq->iparam0;
	ud->uf |= UF_TXE;
    }
    prc_set_ipl(oldipl);
    return;	
}

static int uart_writewpos(QioIOQ *ioq, size_t where, const void *buf, int len) {
    UartDevice *uart;
    const QioDevice *dvc;
    QioFile *file;
    int sts;
    
    file = qio_fd2file(ioq->file);
    if (!(file->mode&_FWRITE)) return (ioq->iostatus = QIO_NOT_OWR); /* file not opened for write, reject it */
    if (len == 0) {
	ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    }
    if (ioq->iostatus) {		/* count == 0 call completion routine */
	ioq->iocount = 0;
	qio_complete(ioq);
	return 0;
    }
    dvc = file->dvc;
    uart = (UartDevice *)dvc->private;
    if (!uart) return (ioq->iostatus = QIO_FATAL); /* uart_init probably not run */
    ioq->pparam0 = (void *)buf;
    ioq->iparam0 = len;
    ioq->pparam1 = (void *)uart;
    sts = qio_getmutex(&uart->writem, write_q, ioq);
    return sts;
}

static int uart_wrt(QioIOQ *ioq, const void *buf, int len) {
    return uart_writewpos(ioq, 0, buf, len);
}

static int uart_open(QioIOQ *ioq, const char *path) {
    QioFile *file;
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    file = qio_fd2file(ioq->file);
    file->private = 0;
    file->pos = 0;
    file->size = 0;
    file->flags = 0;
    file->next = 0;
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int uart_fstat( QioIOQ *ioq, struct stat *stat ) {
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    stat->st_mode = S_IFCHR;
    stat->st_size = 0;
    stat->st_blksize = 0;
    stat->st_blocks = 0;
    qio_complete(ioq);
    return 0;
}

static void ioctl_q( QioIOQ *ioq ) {
    UartDevice *uart;
    const QioDevice *dvc;
    QioFile *file;
    struct qio_uart *u;
    int sts;
    
    file = qio_fd2file(ioq->file);
    dvc = file->dvc;
    uart = (UartDevice *)dvc->private;
    u = (struct qio_uart *)ioq->pparam0;
    sts = 0;
    if (u->which&1) {
	uart->c_iflag = u->iflag;
	sts += 4;
    }
    if (u->which&2) {
	uart->c_oflag = u->oflag;
	sts += 4;
    }
    if (u->which&4) {
	uart->c_cflag = u->cflag;
	sts += 4;
    }
    if (u->which&8) {
	uart->c_lflag = u->lflag;
	sts += 4;
    }
    if (u->which&0x10) {
	memcpy(uart->c_cc, u->cc, NCCS);
	sts += NCCS;
    }
    ioq->iocount = sts;
    cflags_to_uart( uart );
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    qio_freemutex(&uart->writem, ioq);
    qio_complete(ioq);
    return;
}
    
static int uart_ioctl(QioIOQ *ioq, unsigned int cmd, void *arg) {
    QioFile *file;
    const QioDevice *dvc;
    UartDevice *uart;
    struct qio_uart *u;
    int sts;

    if (!arg) return ioq->iostatus = QIO_INVARG;
    if (cmd != QIO_UART_IOCPC && cmd != QIO_UART_IOCGC) return ioq->iostatus = QIO_INVARG;
    file = qio_fd2file(ioq->file);
    dvc = file->dvc;
    if (!dvc) return (ioq->iostatus = QIO_FATAL); /* uart_init probably not run */
    uart = (UartDevice *)dvc->private;
    if (!uart) return (ioq->iostatus = QIO_FATAL); 
    if (cmd == QIO_UART_IOCGC) {
	u = (struct qio_uart *)arg;
	u->which = 0x1F;	/* all fields are returned */
	u->iflag = uart->c_iflag;
	u->oflag = uart->c_oflag;
	u->cflag = uart->c_cflag;
	u->lflag = uart->c_lflag;
	memcpy(u->cc, uart->c_cc, NCCS);
	ioq->iocount = sizeof(struct qio_uart);
	ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
	qio_complete(ioq);
	return 0;
    }
    ioq->pparam0 = arg;			/* save user's parameters */
    ioq->iparam0 = cmd;
    sts = qio_getmutex(&uart->writem, ioctl_q, ioq); /* allow only 1 ioctl at a time */
    if (sts) ioq->iostatus = sts;
    return sts;
}

static int uart_succ(QioIOQ *ioq) {
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static const QioFileOps uart_fops = {
    (int (*)(QioIOQ *, size_t, int))uart_succ,	/* seeks always succeed but don't do anything */
    uart_read, 	/* read */
    uart_wrt,	/* write */
    uart_ioctl, /* ioctl not allowed on uart device at the moment */
    uart_open,	/* use open on uart device */
    0,		/* use default close on uart device */
    0,		/* delete not allowed */
    0,		/* fsync not allowed on uart device */
    0,		/* mkdir not allowed */
    0,		/* rmdir not allowed */
    0,		/* rename not allowed */
    0,		/* truncate not allowed */
    0,		/* statfs not allowed */
    uart_fstat,	/* fstat not allowed */
    0,		/* nothing to cancel on this device */
    uart_succ,	/* is a tty */
    uart_readwpos, /* lseeks are nops, this is just a read */
    uart_writewpos, /* lseeks are nops, this is just a write */
};

static const QioDevice uart_dvcs[] = {
   {"tty0",				/* device name (/tty0) */
    4,					/* length of name */
    &uart_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for device */
    0,					/* unit 0 */
    (void *)(uart_devices+0)}
#if NUM_UARTS > 1
   ,{"tty1",				/* device name (/tty1) */
    4,					/* length of name */
    &uart_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for device */
    1,					/* unit 0 */
    (void *)(uart_devices+1)}
#endif
#if NUM_UARTS > 2
   ,{"tty2",				/* device name (/tty2) */
    4,					/* length of name */
    &uart_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for device */
    2,					/* unit 0 */
    (void *)(uart_devices+2)}
#endif
#if NUM_UARTS > 3
   ,{"tty3",				/* device name (/tty3) */
    4,					/* length of name */
    &uart_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for device */
    3,					/* unit 0 */
    (void *)(uart_devices+3)}
#endif
};

/*
 * qio_uart_init -  Resides here (temporarily) until it becomes necessary to
 *		create a uart.c
 */
void qio_uart_init( void ) {
    
    u_init( uart_dvcs + 0 );
#if NUM_UARTS > 1
# error ** You need to put in the init code for the additional UARTs
#endif
    return;
}
