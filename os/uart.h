#ifndef _UART_H_
#define _UART_H_

/*
 *	uart.h -- Forrest Miller -- July 1996
 *
 *	Definitions for generic uart driver.
 *
 *
 *		Copyright 1996 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */


#ifndef UART_RB_LEN
#define UART_RB_LEN  16		/* 8 words, 32 bytes */
#endif

#ifndef NCCS
# define NCCS	23
#endif

struct ucb
{
 void (*rcv_cb)( U16 );		/* Receive char callback */
 void (*xmt_cb)( char * );	/* Transmit done callback */
 char  *xmt_buf;		/* Transmit buffer pointer */
 VU16   xmt_len;		/* Transmit length */
 VU16   xmt_cnt;		/* Transmit count */
 void (*xmt_busy)();		/* Transmit busy */
 void (*old_irq)();		/* Previous uart irq */
 U16    rcv_buf[UART_RB_LEN];	/* Temporary storage */
 VU8    rcv_in;			/* Where IRQ puts char */
 VU8    rcv_out;		/* Where action routine gets char */
 U8     dropped;		/* No room in rcv_buf */
 VU8    flags;			/* Driver internal status */
 char   baud;			/* Baud rate code */
 char   device;			/* future */
 char   flow_on;		/* future */
 char   flow_off;		/* future */
};

#define UF_RBF 0x01		/* Receive buffer full */
#define UF_TXD 0x02		/* Transmit done */


extern struct ucb *uart_init( struct ucb *ucbp );
extern int uart_write( int fd, char *buf, int n );

#define QIO_UART_IOCGC		0x01	/* ioctl command for get flags */
#define QIO_UART_IOCPC		0x02	/* ioctl command for put flags */
#define QIO_UART_FLG_INP	0x001	/* input flags */
#define QIO_UART_FLG_OUT	0x002	/* output flags */
#define QIO_UART_FLG_CTL	0x004	/* control flags */
#define QIO_UART_FLG_LINE	0x008	/* line discipline flags */
#define QIO_UART_FLG_CC		0x010	/* control characters (all at once) */
#define QIO_UART_CFLG_BAUD_M	0x000000FF	/* one of 256 baud rates */
#define QIO_UART_CFLG_BAUD_V	0		/* starting bit for baud rate */
#define QIO_UART_CFLG_DISAB	0x00000100	/* disable the uart */
#define QIO_UART_CFLG_CTSIN	0x00000200	/* honor CTS input */
#define QIO_UART_CFLG_CTSOUT	0x00000400	/* Force CTS output true */
#define QIO_UART_CFLG_BREAK	0x00000800	/* force break out */
#define QIO_UART_CFLG_XDISAB	0x00001000	/* disable transmit data */
#define QIO_UART_CFLG_LOOPB	0x00002000	/* enable loopback */
#define QIO_UART_CFLG_NOERRS	0x00004000	/* disable error flags */
#define QIO_UART_CFLG_GP	0x00008000	/* general purpose output bit */
#define QIO_UART_WHICH_IFLAG	0x01
#define QIO_UART_WHICH_OFLAG	0x02
#define QIO_UART_WHICH_CFLAG	0x04
#define QIO_UART_WHICH_LFLAG	0x08
#define QIO_UART_WHICH_CC	0x10

/* intput, line discipline and output flags and cc chars are not defined yet */

struct qio_uart {
    int which;			/* bit mask identifying which of the following to set */
    int iflag;			/* input modes */
    int oflag;			/* output modes */
    int cflag;			/* control modes */
    int lflag;			/* line discipline mode */
    char cc[NCCS];		/* control characters */
};
#endif

