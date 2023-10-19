/*
 * $Id: phx_ide.c,v 1.53 1997/10/30 21:38:16 shepperd Exp $
 *
 *		Copyright 1996,1997 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

/********************************************************
* PHX_IDE.C | Copyright 1996, Atari Games, Corp.	*
* ===================================================== *
* Author: David Shepperd -- Oct 1, 1996			*
* ===================================================== *
* This file contains the IDE device driver functions.   *
* for a Phoenix (class) hardware using the NSC IDE 	*
* controller chip (National's PC87415).			*
********************************************************/

/* set TEST_DISK_ERRORS non-zero to test error handling code */

#define IDE_USE_DMA	1	/* Enables DMA related items in the device structure */

#include <config.h>
#ifndef NUM_HDRIVES
#define NUM_HDRIVES		1
#endif
#define QIO_LOCAL_DEFINES	1
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <os_proto.h>
#include <st_proto.h>
#include <phx_proto.h>
#include <wms_proto.h>
#include <intvecs.h>
#include <qio.h>
#include <fsys.h>
#include <nsprintf.h>
#include <eer_defs.h>
#include <nsc_defines.h>		/* Common C Definitions */
#include <nsc_pcicfig.h>		/* PCI Configuration Routines */
#include <nsc_415.h>			/* NSC PC87415 IDE Definitions */
#include <nsc_drvdisp.h>		/* IDE Driver Display Definitions */
#include <nsc_gt64010.h>		/* there is only one thing wanted out of here */
#include <nsc_idereg.h>

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#if !defined(KICK_WDOG)
# if defined(WDOG) && !NO_EER_WRITE && !NO_WDOG
#  define KICK_WDOG() WDOG = 0
# else
#  define KICK_WDOG() do { ; } while (0)
# endif
#endif

#ifndef BLAB
# define BLAB(x)
#endif
#ifndef BLABF
# define BLABF(x)
#endif

/*
 * #define IDE_RW_VIA_DMA 1	if you want to enable r/w via DMA
 */

#if NO_BLINKING_LED
# ifndef LED_ON
#  define LED_ON(x)	do { *(VU32*)LED_OUT &= ~(1<<B_LED_##x); } while (0)
# endif
# ifndef LED_OFF
#  define LED_OFF(x)	do { *(VU32*)LED_OUT |= (1<<B_LED_##x); } while (0)
# endif
#else
# ifndef LED_ON
#  define LED_ON(x)	do { ; } while(0)
# endif
# ifndef LED_OFF
#  define LED_OFF(x)	do { ; } while(0)
# endif
#endif

#ifndef IDE_MAX_DMA_CNT
# define IDE_MAX_DMA_CNT 63
#endif

#ifndef IDE_MAX_PIO_CNT
# define IDE_MAX_PIO_CNT 255
#endif

#ifndef HDIO_KERNEL_BUFSIZE
# if !FSYS_TIGHT_MEM
#  define HDIO_KERNEL_BUFSIZE	(IDE_MAX_DMA_CNT*512)
# else
#  define HDIO_KERNEL_BUFSIZE	8192
# endif
#endif

#define  IDE_CMD_RECALIBRATE    (0x10)
#define  IDE_CMD_SREAD          (0x20)
#define  IDE_CMD_SREAD_NORTY    (0x21)
#define  IDE_CMD_SWRITE         (0x30)
#define  IDE_CMD_SWRITE_NORTY   (0x31)
#define  IDE_CMD_SVERIFY        (0x40)
#define  IDE_CMD_FORMAT         (0x50)
#define  IDE_CMD_SEEK           (0x70)
#define  IDE_CMD_DIAGNOSTICS    (0x90)
#define  IDE_CMD_INITPARMS      (0x91)
#define  IDE_CMD_MREAD          (0xC4)
#define  IDE_CMD_MWRITE         (0xC5)
#define  IDE_CMD_MULTIMODE      (0xC6)
#define  IDE_CMD_RDDMA		(0xC8)
#define  IDE_CMD_RDDMA_NORTY	(0xC9)
#define  IDE_CMD_WRTDMA		(0xCA)
#define  IDE_CMD_WRTDMA_NORTY	(0xCB)
#define  IDE_CMD_BREAD          (0xE4)
#define  IDE_CMD_BWRITE         (0xE8)
#define  IDE_CMD_IDENTIFY       (0xEC)
#define  IDE_CMD_BUFFERMODE     (0xEF)
/* Status bits  */
#define  IDE_STB_BUSY           (0x80)
#define  IDE_STB_READY          (0x40)
#define  IDE_STB_WRFAULT        (0x20)
#define  IDE_STB_SEEKDONE       (0x10)
#define  IDE_STB_DATAREQ        (0x08)
#define  IDE_STB_CORRDATA       (0x04)
#define  IDE_STB_INDEX          (0x02)
#define  IDE_STB_ERROR          (0x01)
/* Error bits */
#define  IDE_ERB_BADBLOCK       (0x80)
#define  IDE_ERB_UNCDATA        (0x40)
#define  IDE_ERB_IDNFOUND       (0x10)
#define  IDE_ERB_ABORTCMD       (0x04)
#define  IDE_ERB_TK0NFOUND      (0x02)
#define  IDE_ERB_AMNFOUND       (0x01)
#define  DRIVE_HEAD_INFO        (0x00)
#define  BYTES_PER_SECTOR       (512)
#define  WORDS_PER_SECTOR       (BYTES_PER_SECTOR/2)
#define  LONGS_PER_SECTOR       (BYTES_PER_SECTOR/4)

struct ide_ctl {
#if !SHORT0_OFFS		/* little endian */
    union {
	VU32 ldata;
	struct { VU16 data; VU16 pad; } wdata;
	struct { VU8 data; VU8 precomp_error; VU8 scnt; VU8 snum; } bdata;
    } overlaid;
    VU8 lcylinder;
    VU8 hcylinder;
    VU8 drive_head;
    VU8 csr;
#else				/* big endian */
    union {
	VU32 ldata;
	struct { VU16 pad; VU16 data; } wdata;
	struct { VU8 snum; VU8 scnt; VU8 precomp_error; VU8 data; } bdata;
    } overlaid;
    VU8 csr;
    VU8 drive_head;
    VU8 hcylinder;
    VU8 lcylinder;
#endif
}; 

extern struct cpu_params cpu_params;

#ifndef HDIO_BATCH
# define HDIO_BATCH	32
#endif

enum hd_read_state {
    HDRW_BEGIN,
#if IDE_RW_VIA_DMA
    HDREAD_DMA,
    HDREAD_CHECK,
    HDWRITE_DMA,
    HDWRITE_CHECK,
#endif
    HDREAD_PIO,
    HDREAD_PIOCHK,
    HDWRITE_PIO,
    HDWRITE_PIOCHK,
    HDRW_XFER,
    HDRW_DONE,
    HDRW_TIMEOUT
};

#define HDIO_USE_KERNEL		1	/* flags below is set to this indicating to use kernel buffer for I/O */

typedef struct hd_io {
    struct hd_io *next;		/* ptr to next hd_io struct */
    const QioDevice *dvc;	/* dvc stored here just to save time */
    QioFile *file;		/* stored here just to save time */
    char *buff;			/* ptr to user's buffer */
    int xfer;			/* number of bytes currently being xferred */
    int wrt;			/* number of bytes that were written */
    int remain;			/* number of bytes remaining to xfer (multiple of sector size) */    
    int u_len;			/* number of bytes user asked for (may not be multiple of sector size) */
    int flags;			/* .ne. if using kernel buffer to do xfer */
    int lba;			/* sector number */
    int direction;		/* .ne. for write, .eq. for read */
    enum hd_read_state state;	/* current state */
} HdIO;

static HdIO *hdio_pool_head;

/************************************************************
 * hd_gethdio - Get a HdIO from the system's pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
static HdIO *hd_gethdio(void) {
    int oldipl, ii;
    HdIO *q;

    oldipl = prc_set_ipl(INTS_OFF); /* this cannot be interrupted */
    q = hdio_pool_head;		/* get next item */
    if (!q) {			/* need to get more ioq's */
	HdIO *new;
	prc_set_ipl(oldipl);	/* interrupts are ok now */
	new = QIOcalloc(HDIO_BATCH, sizeof(QioIOQ));
	if (!new) return 0;	/* no more */
	for (q=new, ii=0; ii < HDIO_BATCH-1; ++ii, ++q) {
	    q->next = q+1;
	}
	prc_set_ipl(INTS_OFF);
	q->next = hdio_pool_head;
	q = new;
    }
    hdio_pool_head = q->next;
    prc_set_ipl(oldipl);
    q->state = 0;
    return q;
}
    
/************************************************************
 * hd_freehdio - Free a HdIO as obtained from a previous
 * call to hd_gethdio().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	returns 0 if success, 1 if error
 */
static int hd_freehdio(HdIO *que) {
    int oldipl;

    if (!que) return 1;
    oldipl = prc_set_ipl(INTS_OFF); /* this cannot be interrupted */
    que->next = hdio_pool_head;
    hdio_pool_head = que;
    prc_set_ipl(oldipl);	/* interrupts ok again */
    return 0;
}

/**************************************************************/

#ifndef n_elts
# define n_elts(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifndef DREQ_TIMEOUT
# define DREQ_TIMEOUT    (2*60)   /* timeout for wait for DREQ */
#endif
#ifndef BUSY_TIMEOUT
# define BUSY_TIMEOUT    (5*60)   /* timeout for ide_wait_not_busy() */
#endif
#ifndef RESET_TIMEOUT
# define RESET_TIMEOUT   (10*60)  /* timeout for ide_soft_reset() */
#endif
#ifndef POWERUP_TIMEOUT
# define POWERUP_TIMEOUT (30*60)  /* timeout for drive powerup */
#endif

#if 0
extern int PCI_ReadConfigByte(int dev, U8 addr);
extern void PCI_WriteConfigByte(int dev, U8 addr, U8 value);
extern void PCI_WriteConfigDword(int dev, U8 addr, U32 value);
extern int PCI_ReadConfigWord(int dev, U8 addr);
extern U32 PCI_ReadConfigDWord(int dev, U8 addr);
extern void PCI_WriteConfigWord(int dev, U8 addr, U16 value);
#endif

static DeviceDesc device_list[NUM_HDRIVES];

#if NSC415_MOVEABLE
int PC87415_DEVICE_NUMBER;
#endif

#if HOST_BOARD != CHAMELEON
extern int GT64010rev;

/**************************************************************/

/*
** Due to a bug in the early versions of the Galileo, every other read of
** a byte to the PCI address space returns garbage. This problem manifests itself
** only if the Galileo is "parked" on the PCI bus and is most evident during
** the read of the status or alternate status registers. The ARB_CTL register
** is present in some designs to work around the problem. For the designs that
** don't have the ARB bit, we disable interrupts and read the requested byte
** twice discarding the first reference. As long as we stay in sync, there
** shouldn't be a problem. If we ever get out of sync, we are screwed...
** One might suggest that we do 'word' reads to an appropriate address instead
** except the IDE controller only responds to 'byte' accesses to its
** registers except for the data register which changes personality depending
** on whether the read is of a 'word' or a 'byte'.
*/

# define READ_BYTE(x) read_byte(x)

# if !defined(ARB_CTL)

#  ifndef JUNK_ADDR
#   define JUNK_ADDR (*(VU32*)(PCI_IO_BASE+0x40C))
#  endif
#  define JUNK_IT() do { U32 junk; junk = JUNK_ADDR; } while (0)

static int read_byte( VU8 *ptr) {
    int old, junk;
    old = prc_set_ipl(INTS_OFF); /* keep interrupts from getting us out of sync */
    JUNK_IT();
    junk = *ptr;	/* toss the first reading */
    prc_set_ipl(old);
    return junk;
}

# else

#  define ARB_CTL_P *(VU32*)ARB_CTL

#  if NUM_HDRIVES > 2
#   error * You will have to rewrite this hack to account for DMA on the 'other'
#   error * channel. Setting the ARB bit while DMA is in progress will trash
#   error * a PCI transaction.
#  endif

static int read_byte( VU8 *ptr) {
    int sts;
    if (GT64010rev <= 1) {
	int old;
	old = prc_set_ipl(INTS_OFF);	/* call me paranoid */
	ARB_CTL_P = 1<<B_ARB_PARK;		/* unpark the Galileo */
	prc_wait_n_usecs(2);		/* let the control GAL settle */
	sts = *ptr;				/* read the data */
	ARB_CTL_P = 0;			/* park the Galileo again */
	prc_set_ipl(old);			/* interrupts ok once again */
    } else {
	sts = *ptr;
    }
    return sts;
}

# endif			/* ARB_CTL */

#else			/* HOST_BOARD != CHAMELEON */

# define READ_BYTE(x) *(x)

#endif			/* HOST_BOARD != CHAMELEON */

/*
** *************************************
** ide_wait_not_busy(dev):
** Wait for the drive to become not busy
** *************************************
*/

static int ide_wait_not_busy( DeviceDesc *dev)
{
    U32 end_timer;

    end_timer = eer_rtc;
    while( (eer_rtc - end_timer) <  BUSY_TIMEOUT) {
	if (!(READ_BYTE(dev->alt_sts) & IDE_STB_BUSY)) break;
	if ((eer_rtc&15) == 0) KICK_WDOG();
    }
    return 0;

}   /* End: ide_wait_not_busy() */

/*
** ******************************************************
** ide_send_byte():
** Send the specified byte to the IDE controller.
** ======================================================
** Usage:
**   ide_send_byte( ptr, offset, cmd );
**
**   offset:  address offset (0-7) to write
**   cmd:  data to write to IDE controller.
**
** Returns:
**   0 on success, 1 if timeout
** ******************************************************
*/

#if CPU_SPEED_VARIABLE
extern int cpu_ticks_usec;
#define SEND_BYTE_WAIT_TIME (cpu_ticks_usec * 128)
#else
#define SEND_BYTE_WAIT_TIME (CPU_SPEED/1000000 * 128)
#endif

#if IDE_SENDBYTE_HISTORY
U8 ide_sendbyte_history[IDE_SENDBYTE_HISTORY][sizeof(IdeCtl)];
#endif

static int ide_send_byte(DeviceDesc *dev, int offset, int val) {
    U32 t;

#if IDE_SENDBYTE_HISTORY
    ide_sendbyte_history[0][offset] = val;
    if (offset == offsetof(IdeCtl, csr)) {	/* if writing to command byte */
	int ii;
	for (ii=IDE_SENDBYTE_HISTORY-1; ii > 0; --ii ) {
	    memcpy(ide_sendbyte_history[ii], ide_sendbyte_history[ii-1], sizeof(IdeCtl));
	}
	memset(ide_sendbyte_history[0], 0, sizeof(IdeCtl));
    }
#endif

    t = prc_get_count();

    do {
	if (!(READ_BYTE(dev->alt_sts) & IDE_STB_BUSY)) {
	    *((VU8*)dev->ideptr + offset) = val;
	    return 0;
	}
    } while ( (prc_get_count() - t) < SEND_BYTE_WAIT_TIME);

/*
 * Check one more time in case the above loop timed out due to 
 * interrupt servicing.
 */

    if (!(READ_BYTE(dev->alt_sts) & IDE_STB_BUSY)) {
	*((VU8*)dev->ideptr + offset) = val;
	return 0;
    }

    return 1;
}
    

#define DEVICE_SELECT_BIT  (4)

/*
** ************************************************
** ide_set_device():
** Set the active device for subsequent IDE functions.
** ================================================
** Usage:
**   ide_set_device( DeviceDesc *dev );
**
** Returns:
**   nothing.
** ************************************************
*/

static void ide_set_device( DeviceDesc *dev ) {
    int tmp;

    tmp = dev->ideptr->drive_head;
    tmp &= ~(1<<DEVICE_SELECT_BIT);
    dev->ideptr->drive_head = tmp | dev->select;
    return;
}   /* End: ide_set_device() */

/*
** ************************************************
** nsc_irq():
** Interrupt service routine
** ================================================
** Usage:
**   called from interrupt dispatcher
**
** Returns:
**   nothing.
** ************************************************
*/

static void nsc_irq(void) {
    DeviceDesc *dev;
    NSC415Cmd *dma;
    int ii;
    U32 dma_sts;
    
    LED_ON(GRN);
    for (ii=0; ii < NUM_HDRIVES; ii += 2) {
	dev = device_list+ii;
	dma = dev->dma;			/* point to h/w register */
	dma_sts = 0;
	if (dma) dma_sts = dma->cmd;	/* save dma status, if any */
	if ((dma_sts&DMA_STS_int)) {	/* interrupt on this channel and we're expecting one */
	    int sts;
#if GALILEO_BASE
	    sts = READ_BYTE(dev->alt_sts); /* do a dummy read for the benefit of the Galileo */
#endif
	    sts = READ_BYTE(&dev->ideptr->csr); /* clear the interrupt at the drive */
#if 0	/* technically, I believe we should do this, but it doesn't work on a SA-1 */
	    dma->cmd |= DMA_STS_int;	/* clear the interrupt at the chip by writing a 1 to it */
#endif
/* The following works because we happen to know there can be only one
 * drive per channel waiting for an interrupt at any one time.
 */
	    if (!dev->busy && (ii+1) < NUM_HDRIVES) ++dev;
	    if ( dev->busy ) {
		dev->busy &= ~1;	/* not busy anymore */
		if (dev->complete_q.action) {	/* if he has a completion routine */
		    prc_q_ast(QIO_ASTLVL, &dev->complete_q); /* queue it up */
		}
	    }
	}
    }
    LED_OFF(GRN);
    return;
}

static void reset_chip(void);

#if 1
static void ide_hard_reset(int which) {
    int tmp, max;
    DeviceDesc *dev;

    which &= ~1;		/* has to be even */
    max = which + 1;
    if (max > NUM_HDRIVES) max = NUM_HDRIVES;
    for (tmp=which, dev=device_list+which; tmp < max; ++tmp, ++dev) {
	if (!(tmp&1) && dev->dma) dev->dma->cmd = 0;	/* stop all DMA activity */
	dev->did_multsect = 0;	/* need to re-issue the SET MULTIPLE command */
    }	

#if NUM_HDRIVES > 2
# error *** You need to correct/interlock the reset sequence for the two channels.
#endif

    tmp = PCI_ReadConfigDWord(PC87415_DEVICE_NUMBER, offsetof(N415_CfigRegs, n415_Control[0]));
    PCI_WriteConfigByte(PC87415_DEVICE_NUMBER, offsetof(N415_CfigRegs, n415_Control[0]),
    	(tmp&~15) | (1<<18) | (1<<3));
    prc_wait_n_usecs(10000);
    PCI_WriteConfigByte(PC87415_DEVICE_NUMBER, offsetof(N415_CfigRegs, n415_Control[0]), tmp );
    prc_wait_n_usecs(500);
}
#endif

/**************************************************************/

static const U8 timing_regs[] = {
    offsetof(N415_CfigRegs, n415_C1D1_Dread),
    offsetof(N415_CfigRegs, n415_C1D2_Dread),
    offsetof(N415_CfigRegs, n415_C2D1_Dread),
    offsetof(N415_CfigRegs, n415_C2D2_Dread)
};

static void hd_lseek_q( QioIOQ *ioq ) {
    HdIO *hdio;
    QioFile *file;

    hdio = (HdIO *)ioq->private2;
    file = hdio->file;
    switch (hdio->flags) {
	case SEEK_SET:
	    file->pos = hdio->remain;
	    break;

	case SEEK_END:
	    file->pos = file->size + hdio->remain;
	    break;

	case SEEK_CUR:
	    file->pos += hdio->remain;
	    break;
    }
    if (file->pos > file->size) file->pos = file->size;
    ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    ioq->iocount = file->pos;
    qio_freemutex(file->dvc->mutex, ioq); /* done with HD mutex */
    hd_freehdio(hdio);			/* done with HD I/O stuff */
    ioq->private2 = 0;
    qio_complete(ioq);
}

static int hd_lseek( QioIOQ *ioq, off_t pos, int whence) {
    QioFile *file;
    HdIO *hdio;
    int sts;

    if (whence < 0 || whence > SEEK_END) return (ioq->iostatus = HDIO_INVARG);
    file = qio_fd2file(ioq->file);
    hdio = hd_gethdio();
    if (!hdio) return (ioq->iostatus = HDIO_NOHDIO);
    hdio->file = file;
    hdio->remain = pos;
    hdio->flags = whence;
    ioq->private2 = hdio;
    sts = qio_getmutex(file->dvc->mutex, hd_lseek_q, ioq);
    if (sts) {
	hd_freehdio(hdio);		/* done with HD I/O stuff */
	return sts;
    }
    return 0;
}
    
static int ide_hread_data( DeviceDesc *dev, U32 *rdbuf, int nsectors );
static int ide_hwrite_data( DeviceDesc *dev, U32 *rdbuf, int nsectors );

#if IDE_RW_VIA_DMA
/*
** ****************************************************************
** rw_via_dma():
** Use the host processor to read (or write) LBA sectors from the specified
** location on the hard drive into the read buffer (or visa-versa).
** ================================================================
** Usage:
**   status = rw_via_dma( rdbuf, lba, count );
**
**   U32 *rdbuf:    pointer to buffer to hold sector information.
**   int lba:       Logical block
**   int count:     number of sectors to read into buffer.
**
** Returns:
**   0
** ****************************************************************
*/

static int rw_via_dma( DeviceDesc *dev, HdIO *hdio ) {
    U32 addr;
    NSC415Cmd *dma;
    NSC415Pdt *pdts;
    int ii, tcnt, sts;

    dma = dev->dma;		/* set pointer to base of IDE controller registers */
    if (hdio->flags&HDIO_USE_KERNEL) {
	addr = (U32)dev->lclbuff;
	if (hdio->direction) {
	    prc_flush_pdcache((void *)dev->lclbuff, hdio->xfer); /* flush the kernel buffer on writes */
	} else {
	    prc_inv_pdcache((void *)dev->lclbuff, hdio->xfer); /* invalidate the kernel buffer on reads */
	}
    } else {
	addr = (U32)hdio->buff; 
    }
    tcnt = hdio->xfer;
    pdts = dev->pdts;		/* pointer to PDT's in non-cached and aligned memory */

    for (ii=0; ii < IDE_PDTS_DD && tcnt > 0; ++ii, ++pdts) {
	int bc;
	bc = (tcnt > 63*512) ? 63*512 : tcnt;	/* 32256 bytes max per xfer (32768-512) */
	if ((addr&0xFFFF0000) != ((addr+bc)&0xFFFF0000)) {	/* crossed a 64k page */
	    bc = ((addr&0xFFFF0000) + 0x10000) - addr;	/* amount to next 64k page */
	}
	pdts->phys = QIO_PHYS(addr);	/* get physical address of buffer */
	pdts->byte_cnt = bc&0xFFFF;
	addr += bc;
	tcnt -= bc;
    }
    --pdts;				/* backup to the last one loaded */
    pdts->byte_cnt |= DMA_PDT_EOT;	/* set the EOT bit */

    if (hdio->direction) {
	dma->cmd = DMA_STS_int|DMA_STS_error|DMA_STS_dma2|DMA_STS_dma1; /* stop any currently running DMA, set direction */
    } else {
	dma->cmd = DMA_STS_int|DMA_STS_error|DMA_STS_dma2|DMA_STS_dma1|DMA_CMD_write; /* stop any currently running DMA, set direction */
    }
    dma->prd_addr = QIO_PHYS(dev->pdts); /* convert PDT address to physical */
    ii = dev->ideptr->csr;		/* ACK any pending drive interrupts */

    dev->busy = 1;			/* say we're busy */

/* setup IDE drive to read n sectors */
    sts = ide_send_byte(dev, offsetof(IdeCtl, drive_head), ((hdio->lba>>24)&0xF) | 0x40 | dev->select);
    sts |= ide_send_byte(dev, offsetof(IdeCtl, hcylinder), (hdio->lba>>16)&0xFF);
    sts |= ide_send_byte(dev, offsetof(IdeCtl, lcylinder), (hdio->lba>>8)&0xFF);
    sts |= ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.snum), hdio->lba&0xFF);
    sts |= ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.scnt), hdio->xfer/512);

    if (hdio->direction) {
	dma->cmd = DMA_STS_int|DMA_STS_error|DMA_STS_dma2|DMA_STS_dma1|DMA_CMD_start;	/* start the DMA */
    } else {
	dma->cmd = DMA_STS_int|DMA_STS_error|DMA_STS_dma2|DMA_STS_dma1|DMA_CMD_start|DMA_CMD_write;	/* start the DMA */
    }
    READ_BYTE(&dev->ideptr->csr);	/* make sure drive interrupt has been ack'ed */
    sts |= ide_send_byte(dev, offsetof(IdeCtl, csr),
    		hdio->direction ? IDE_CMD_WRTDMA : IDE_CMD_RDDMA);
        
    return sts;
}
#endif

#ifndef QIO_IOQ_BATCH
# define QIO_IOQ_BATCH	16
#endif

/**********************************************************************************
 * hd_timeout is called by the timer interrupt. It runs as an ACTION routine
 * (which may have interrupted an AST in progress). Do not change anything in
 * in any HdIO or QioIOQ structs while in this routine.
 */

static void hd_timeout(void *arg) {
    HdIO *hdio;
    QioIOQ *ioq;
    DeviceDesc *dev;

    ioq = (QioIOQ *)arg;
    hdio = (HdIO *)ioq->private2;
    dev = (DeviceDesc *)hdio->dvc->private;
    dev->timeout = 1;				/* signal there was a device timeout */
    prc_q_ast(QIO_ASTLVL, &dev->complete_q);	/* call I/O completion routine */
    return;
}

#if defined(EER_DSK_ERR) || defined(EER_DSK_AMNF) || defined(EER_DSK_TK0NF) || \
    defined(EER_DSK_ABORT) || defined(EER_DSK_IDNF) || defined(EER_DSK_UNCDTA) || \
    defined(EER_DSK_TIMOUT) || defined(EER_DSK_WERR) || defined(EER_DSK_CORR)
static void inc_bram(int arg) {
    int t;
    t = eer_gets(arg);
    if (t != -1 && t < 255) eer_puts(arg, t+1);
}
#endif

static void hd_rw_q(QioIOQ *ioq) {
    HdIO *hdio;
    DeviceDesc *dev;
    QioMutex *mutex;
    int err;

    LED_ON(YEL);
    hdio = (HdIO *)ioq->private2;
    if (!hdio) {
        LED_OFF(YEL);
	return;				/* ACKKK!!! Can't do anything */
    }
    dev = (DeviceDesc *)hdio->dvc->private;	/* need to get at variables that irq will use */
    dev->dma->cmd = 0;				/* stop any pending DMA */

    err = READ_BYTE(dev->alt_sts);

#if TEST_DISK_ERRORS
    if (!(err&(IDE_STB_ERROR|IDE_STB_WRFAULT))) {
	if ((ctl_read_sw(J1_UP)&(J1_UP))) {
	    err = IDE_STB_CORRDATA;
	}
	if ((ctl_read_sw(J1_DOWN)&(J1_DOWN))) {
	    err = IDE_STB_ERROR;
	    ioq->iostatus = HDIO_UNCORR;
	}
	if ((ctl_read_sw(J1_LEFT)&(J1_LEFT))) {
	    dev->timeout = 1;
	}
    }
#endif
	
#ifdef EER_DSK_CORR
    if (hdio->state != HDRW_BEGIN && (err&IDE_STB_CORRDATA)) {
	inc_bram(EER_DSK_CORR);	/* record it */
    }
#endif

    if (hdio->state != HDRW_BEGIN && (dev->timeout || (err&(IDE_STB_ERROR|IDE_STB_WRFAULT)))) {
	if ((err&(IDE_STB_ERROR|IDE_STB_WRFAULT))) {	/* if there were any I/O errors */
#if ARB_CTL || (HOST_BOARD == CHAMELEON)
	    int t;
	    t = READ_BYTE(&dev->ideptr->overlaid.bdata.scnt);
	    ioq->iocount += t * 512;	/* record how many bytes were xferred before the error */
	    t = READ_BYTE(&dev->ideptr->overlaid.bdata.precomp_error);
	    if ((t&IDE_ERB_AMNFOUND)) {
		ioq->iostatus = HDIO_AMNF;
#ifdef EER_DSK_AMNF
		inc_bram(EER_DSK_AMNF);
#endif
	    } else if ((t&IDE_ERB_TK0NFOUND)) {
		ioq->iostatus = HDIO_TK0NF;
#ifdef EER_DSK_TK0NF
		inc_bram(EER_DSK_TK0NF);
#endif
	    } else if ((t&IDE_ERB_ABORTCMD)) {
		ioq->iostatus = HDIO_ABRT;
#ifdef EER_DSK_ABORT
		inc_bram(EER_DSK_ABORT);
#endif
	    } else if ((t&IDE_ERB_IDNFOUND)) {
		ioq->iostatus = HDIO_IDNF;
#ifdef EER_DSK_IDNF
		inc_bram(EER_DSK_IDNF);
#endif
	    } else if ((t&IDE_ERB_UNCDATA)) {
		ioq->iostatus = HDIO_UNCORR;
#ifdef EER_DSK_UNCDTA
		inc_bram(EER_DSK_UNCDTA);
#endif
	    }
#endif
#ifdef EER_DSK_WERR
	    if ((err&IDE_STB_WRFAULT)) inc_bram(EER_DSK_WERR);
#endif
#ifdef EER_DSK_ERR
	    inc_bram(EER_DSK_ERR);	/* record the fact we got any error at all */
#endif
	}
	if (!ioq->iostatus) {		/* assume anything else is device timeout */
	    ioq->iostatus = HDIO_TIMEOUT; /* under DMA or with no ARB_CTL, all errors are reported as timeouts */
	    reset_chip();		/* kick the IDE chip in the ass */
#ifdef EER_DSK_ERR
	    inc_bram(EER_DSK_ERR);	/* record the fact */
#endif
#ifdef EER_DSK_TIMOUT
	    inc_bram(EER_DSK_TIMOUT);	/* record the device timeout */
#endif
	}
    } else while (1) {
	switch (hdio->state) {
	    default:
		ioq->iostatus = HDIO_FATAL;	/* disaster strikes */
		break;
	    case HDRW_BEGIN:
		dev->complete_q.action = hd_rw_q; /* interrupts send us back to ourself */
		dev->complete_q.param = (void *)ioq; /* and we always get the same arg */
		if (ioq->timeout) {		/* if he wants a gross rw timeout */
		    dev->timer_q.delta = ioq->timeout;
		    dev->timer_q.func = hd_timeout;
		    dev->timer_q.vars = (void *)ioq;
		    dev->timeout = 0;		/* assume not timed out */
		    tq_ins(&dev->timer_q);	/* put it in the timer queue */
		}
#if IDE_RW_VIA_DMA
		hdio->state = hdio->direction ? HDWRITE_DMA : HDREAD_DMA; /* next state */
#else
		hdio->state = hdio->direction ? HDWRITE_PIO : HDREAD_PIO; /* next state */
#endif
		continue;

	    case HDREAD_PIO:
		hdio->xfer = IDE_MAX_PIO_CNT*512; /* assume we're to xfer the max */
		if (hdio->xfer > hdio->u_len) hdio->xfer = (hdio->u_len+511)&-512;
		hdio->state = HDREAD_PIOCHK;
#if 0
		if (!dev->did_multsect && dev->max_multsect > 1) {
		    int max;
		    max = dev->max_multsect;
		    if (max > IDE_MAX_DMA_CNT) max = IDE_MAX_DMA_CNT;
		    if (max > HDIO_KERNEL_BUFSIZE/512) max = HDIO_KERNEL_BUFSIZE/512;
		    err = ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.scnt), max);
		    err |= ide_send_byte(dev, offsetof(IdeCtl, csr), IDE_CMD_MULTIMODE); /* set multiple mode */
		    err |= ide_wait_not_busy( dev );
		    if (err) goto device_timeout;
		    err = READ_BYTE(&dev->ideptr->csr); /* clear any interrupt at the drive */
		    dev->did_multsect = (err&IDE_STB_ERROR) ? -1 : max;
		}
#endif
		dev->busy = 1;			/* say we're busy */
		err = ide_send_byte(dev, offsetof(IdeCtl, drive_head), ((hdio->lba>>24)&0xF) | 0x40 | dev->select);
		err |= ide_send_byte(dev, offsetof(IdeCtl, hcylinder), (hdio->lba>>16)&0xFF);
		err |= ide_send_byte(dev, offsetof(IdeCtl, lcylinder), (hdio->lba>>8)&0xFF);
		err |= ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.snum), hdio->lba&0xFF);
		err |= ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.scnt), hdio->xfer/512);
		err |= ide_send_byte(dev, offsetof(IdeCtl, csr), 
		    		dev->did_multsect > 1 ? IDE_CMD_MREAD : IDE_CMD_SREAD); /* read via PIO */
		if (err) {
device_timeout:
		    ioq->iostatus = HDIO_TIMEOUT; 
		    reset_chip();		/* kick the IDE chip in the ass */
#ifdef EER_DSK_ERR
		    inc_bram(EER_DSK_ERR);	/* record the fact */
#endif
#ifdef EER_DSK_TIMOUT
		    inc_bram(EER_DSK_TIMOUT);	/* record the device timeout */
#endif
		    break;			/* finish up and exit */
		}
		return;				/* wait for interrupt */
		
	    case HDREAD_PIOCHK: {
		U32 *bp;
		int amt, sects;

		sects = dev->did_multsect > 1 ? dev->did_multsect : 1;
		if (sects*512 > hdio->xfer) sects = hdio->xfer/512;
		amt = sects*512;		/* assume max length */
		if (amt > hdio->u_len) amt = hdio->u_len; /* unless user wants less */
		bp = (U32*)hdio->buff;
		dev->busy = 1;			/* say we're still busy */
		if (((U32)bp&3) || (amt&511)) { /* buffer is not longword aligned or amt not sector aligned */
		    ide_hread_data(dev, dev->lclbuff, sects); /* copy sector to kernel buffer */
		    memcpy(bp, dev->lclbuff, amt); /* then copy from there to user buffer */
		    ++dev->doofus;		/* tell 'em he screwed up */
		} else {
		    ide_hread_data(dev, bp, sects); /* copy n sectors to user's buffer */
		}
		ioq->iocount += amt;		/* tell 'em how much was input so far */
		hdio->buff += amt;		/* advance user's buffer pointer */
		hdio->u_len -= amt;		/* take from amt he asked for */
		hdio->xfer -= amt;		/* take from total we asked the drive for */
		hdio->lba += sects;		/* advance sector number */
		if (hdio->u_len > 0) {		/* if there's more to read */
		    if (hdio->xfer <= 0) {	/* if we've done all we can in one operation */
			hdio->state = HDREAD_PIO; /* start another read */
			continue;
		    }
		    return;			/* else just wait for next interrupt */
		}
		ioq->iostatus = HDIO_SUCC | SEVERITY_INFO; /* normal success */
		hdio->state = HDRW_DONE;		/* finish up and exit */
		break;
	    }

	    case HDWRITE_PIO: {
		int old_rtc;
		hdio->xfer = IDE_MAX_PIO_CNT*512; /* assume we're to xfer the max */
		if (hdio->xfer > hdio->u_len) hdio->xfer = (hdio->u_len+511)&-512;
		hdio->state = HDWRITE_PIOCHK;
		err = ide_send_byte(dev, offsetof(IdeCtl, drive_head), ((hdio->lba>>24)&0xF) | 0x40 | dev->select);
		err |= ide_send_byte(dev, offsetof(IdeCtl, hcylinder), (hdio->lba>>16)&0xFF);
		err |= ide_send_byte(dev, offsetof(IdeCtl, lcylinder), (hdio->lba>>8)&0xFF);
		err |= ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.snum), hdio->lba&0xFF);
		err |= ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.scnt), hdio->xfer/512);
		err |= ide_send_byte(dev, offsetof(IdeCtl, csr), IDE_CMD_SWRITE); /* write via PIO */
		if (err) goto device_timeout;
		hdio->wrt = 0;			/* assume we didn't write anything at first */
		old_rtc = eer_rtc;
		while( ((READ_BYTE(dev->alt_sts) & IDE_STB_DATAREQ) != IDE_STB_DATAREQ) &&
		       (eer_rtc - old_rtc) < DREQ_TIMEOUT )  {
		    if ((eer_rtc&15) == 0) KICK_WDOG();
		}

		continue;			/* start the first transaction */		
	    }		
	    case HDWRITE_PIOCHK: {
		U32 *bp;
		int amt;

		ioq->iocount += hdio->wrt;	/* tell 'em how much was successfully written */
		hdio->buff += hdio->wrt;	/* advance user's buffer pointer */
		hdio->u_len -= hdio->wrt;	/* take from amt he said to write */
		hdio->xfer -= hdio->wrt;	/* take from total told the drive we're sending */
		hdio->lba += hdio->wrt/512;	/* advance sector number */
		if (hdio->u_len <= 0) {
		    ioq->iostatus = HDIO_SUCC | SEVERITY_INFO; /* normal success */
		    hdio->state = HDRW_DONE;	/* we're done. */
		    break;
		}
		if (hdio->xfer <= 0) {		/* if we've done all we can in one operation */
		    hdio->state = HDWRITE_PIO;	/* start another write */
		    continue;
		}
		bp = (U32*)hdio->buff;
		dev->busy = 1;			/* say we're busy */
		amt = 512;
		if (amt > hdio->u_len) amt = hdio->u_len; /* unless user wants less */
		hdio->wrt = amt;		/* this is how much we're attempting to write */
		if (((U32)bp&3) || amt < 512) { /* buffer is not longword aligned or short */
		    memcpy(dev->lclbuff, bp, amt);	/* copy 1 sector's worth of kernel buffer to drive */
		    ide_hwrite_data(dev, dev->lclbuff, 1);
		    ++dev->doofus;		/* tell 'em he screwed up */
		} else {
		    ide_hwrite_data(dev, bp, 1);/* copy 1 sector's worth of user's buffer to drive */
		}
		return;				/* wait for next interrupt */
	    }

#if IDE_RW_VIA_DMA
	    case HDREAD_DMA:
		if ((hdio->flags&HDIO_USE_KERNEL)) {	/* now need to copy kernel buffer to user's buffer */
		    hdio->xfer = HDIO_KERNEL_BUFSIZE; /* assume we're to xfer the max */
		} else {
		    hdio->xfer = IDE_MAX_DMA_CNT*512; /* assume we're to xfer the max */
		}
		if (hdio->xfer > hdio->remain) hdio->xfer = hdio->remain;
		hdio->state = HDREAD_CHECK;
		rw_via_dma( dev, hdio );
                LED_OFF(YEL);
		return;
		
	    case HDREAD_CHECK:
		hdio->lba += hdio->xfer/512;		/* advance sector pointer */
		if ((hdio->flags&HDIO_USE_KERNEL)) {	/* now need to copy kernel buffer to user's buffer */
		    if (hdio->xfer > hdio->u_len) hdio->xfer = hdio->u_len;
		    memcpy(hdio->buff, dev->lclbuff, hdio->xfer);
		}
		ioq->iocount += hdio->xfer;		/* tell 'em how much was input so far */
		hdio->buff += hdio->xfer;		/* advance user's buffer pointer */
		hdio->remain -= hdio->xfer;		/* reduce amount we're to copy */
		hdio->u_len -= hdio->xfer;
		if (hdio->u_len > 0) {
		    hdio->state = HDREAD_DMA;
		    continue;				/* start another read */
		}
		ioq->iostatus = HDIO_SUCC | SEVERITY_INFO; /* normal success */
		hdio->state = HDRW_DONE;		/* finish up and exit */
		break;

	    case HDWRITE_DMA:
		if ((hdio->flags&HDIO_USE_KERNEL)) {
		    hdio->xfer = HDIO_KERNEL_BUFSIZE;
		} else {
		    hdio->xfer = IDE_MAX_DMA_CNT*512; /* assume we're to xfer the max */
		}
		if (hdio->xfer > hdio->remain) hdio->xfer = hdio->remain;
		if ((hdio->flags&HDIO_USE_KERNEL)) {	/* now need to copy user's buffer to kernel buffer */
		    if (hdio->xfer > hdio->u_len) {
			memcpy(dev->lclbuff, hdio->buff, hdio->u_len);
			memset((char *)dev->lclbuff + hdio->u_len, 0, hdio->xfer - hdio->u_len);
		    } else {
			memcpy(dev->lclbuff, hdio->buff, hdio->xfer);
		    }
		}
		hdio->state = HDWRITE_CHECK;
		rw_via_dma( dev, hdio );
                LED_OFF(YEL);
		return;
		
	    case HDWRITE_CHECK:
		hdio->buff += hdio->xfer;		/* advance user's buffer pointer */
		hdio->remain -= hdio->xfer;		/* reduce amount we're to copy */
		ioq->iocount += hdio->xfer;		/* tell 'em how much was output so far */
		hdio->lba += hdio->xfer/512;		/* advance sector pointer */
		hdio->u_len -= hdio->xfer;
		if (hdio->u_len > 0) {
		    hdio->state = HDWRITE_DMA;
		    continue;				/* start another write */
		}
		ioq->iostatus = HDIO_SUCC | SEVERITY_INFO; /* normal success */
		hdio->state = HDRW_DONE;		/* finish up and exit */
		break;
#endif

	    case HDRW_DONE:
		break;
	}
	break;
    }
    dev->complete_q.action = 0;		/* cover our tracks */
    dev->busy &= ~1;			/* channel is not busy anymore */
    hdio->file->pos = hdio->lba;	/* move file's position */
    if (dev->timer_q.que) tq_del(&dev->timer_q);	/* delete the timer, if any */
    mutex = hdio->dvc->mutex;		/* save this */
    hd_freehdio(hdio);			/* done with HD I/O stuff */
    ioq->private2 = 0;			/* probably don't need to do this, but do it anyway */
    qio_freemutex(mutex, ioq);		/* done with HD mutex */
    qio_complete(ioq);			/* call his completion routine if any */
    LED_OFF(YEL);
    return;
}

static int hd_readwpos( QioIOQ *ioq, off_t where, void *buff, long count) {
    DeviceDesc *dev;
    const QioDevice *dvc;
    QioFile *file;
    HdIO *hdio;
    int sts;

    file = qio_fd2file(ioq->file);
    if (!(file->mode&FREAD)) return (ioq->iostatus = HDIO_WRONLY); /* file not opened for read, reject it */
    if (where >= file->size) {
	ioq->iostatus = QIO_EOF;
    } else if (count == 0) {
	ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    }
    if (ioq->iostatus) {		/* EOF and count == 0 call completion routine */
	ioq->iocount = 0;
	qio_complete(ioq);
	return 0;
    }
    dvc = file->dvc;
    dev = (DeviceDesc *)dvc->private;
    if (!dev) return (ioq->iostatus = HDIO_FATAL); /* ide_init probably not run */
    hdio = hd_gethdio();
    if (!hdio) return (ioq->iostatus = HDIO_NOHDIO);
    ioq->private2 = hdio;
    hdio->u_len = hdio->remain = count;
    hdio->dvc = dvc;
    hdio->file = file;
    hdio->buff = (char *)buff;
    hdio->lba = where;
    hdio->flags = 0;			/* assume we're to DMA directly into user's buffer */
    hdio->direction = 0;		/* read */
    if ( ((U32)buff&(3 | (cpu_params.cpu_dcache_ls-1))) || /* if user's buffer is not on a U32 or cache-line boundary */
         (count&511) ) {		/* or count is not a multiple of a sector length */
	hdio->flags |= HDIO_USE_KERNEL; /* signal to use kernel buffer as intermediate storage */
	hdio->remain = (count+511)&-512;	/* round up local count to next sector boundary */
	++dev->doofus;			/* note the fact that we used a kernel buffer */
    } else if (!((U32)buff&0x20000000)) { /* if buffer is in cached memory */
#if (PROCESSOR&~1) == MIPS5000
# define MAX_FLUSH_COUNT	(512*1024)
#else
# define MAX_FLUSH_COUNT	(256*1024)
#endif
	if (count >= MAX_FLUSH_COUNT) {	/* if buffer is really big ... */
	    flush_dcache();		/* it is probably faster to just flush the entire data cache */
	} else {
	    prc_inv_pdcache(buff, count); /* otherwise try invalidating only the user's buffer */
	}
    }
    sts = qio_getmutex(dvc->mutex, hd_rw_q, (void *)ioq);	/* claim mutex, switch to ASTLVL and goto hd_rw_q */
    if (sts) {
	ioq->private2 = 0;	/* didn't queue the I/O, cleanup after ourselves */
	ioq->iostatus = sts;	/* tell 'em we failed */
	hd_freehdio(hdio);	/* give back our temps */
    }
    return sts;			/* return to caller with error code (if any) */
}
    
static int hd_read( QioIOQ *ioq, void *buff, long count) {
    QioFile *file;

    file = qio_fd2file(ioq->file);
    return hd_readwpos(ioq, file->pos, buff, count);
}
    
static int hd_writewpos( QioIOQ *ioq, off_t where, const void *buff, long count) {
    DeviceDesc *dev;
    const QioDevice *dvc;
    QioFile *file;
    HdIO *hdio;
    int sts;

    file = qio_fd2file(ioq->file);
    if (!(file->mode&FWRITE)) return (ioq->iostatus = HDIO_RDONLY); /* file not opened for write, reject it */
    if (where >= file->size) {
	ioq->iostatus = QIO_EOF;	/* we cannot extend the disk */
    } else if (count == 0) {
	ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    }
    if (ioq->iostatus) {		/* EOF and count == 0 call completion routine */
	ioq->iocount = 0;
	qio_complete(ioq);
	return 0;
    }
    dvc = file->dvc;
    dev = (DeviceDesc *)dvc->private;
    if (!dev) return (ioq->iostatus = HDIO_FATAL); /* ide_init probably not run */
    hdio = hd_gethdio();
    if (!hdio) return (ioq->iostatus = HDIO_NOHDIO);
    ioq->private2 = hdio;
    hdio->u_len = hdio->remain = count;
    hdio->dvc = dvc;
    hdio->file = file;
    hdio->buff = (char *)buff;		/* remember user's buffer ptr */
    hdio->lba = where;
    hdio->flags = 0;			/* assume we're to DMA directly into user's buffer */
    hdio->direction = 1;		/* write direction */
    if ( ((U32)buff&(3 | (cpu_params.cpu_dcache_ls-1))) || /* if user's buffer is not on a U32 or cache-line boundary */
         (count&511) ) {		/* or count is not a multiple of a sector length */
	hdio->flags |= HDIO_USE_KERNEL; /* signal to use kernel buffer as intermediate storage */
	hdio->remain = (count+511)&-512;	/* round up local count to next sector boundary */
	++dev->doofus;			/* note the fact that we used a kernel buffer */
    } else if (!((U32)buff&0x20000000)) { /* if buffer is in cached memory */
	if (count > MAX_FLUSH_COUNT) {	/* if buffer is really big ... */
	    flush_dcache();		/* it is probably faster to just flush the entire data cache */
	} else {
	    prc_flush_pdcache(buff, count); /* otherwise try flushing only the user's buffer */
	}
    }
    sts = qio_getmutex(dvc->mutex, hd_rw_q, (void *)ioq);	/* claim mutex, switch to ASTLVL and goto hd_rw_q */
    if (sts) {
	ioq->private2 = 0;	/* didn't queue the I/O, cleanup after ourselves */
	ioq->iostatus = sts;	/* tell 'em we failed */
	hd_freehdio(hdio);	/* give back our temps */
    }
    return sts;			/* return to caller with error code (if any) */
}
    
static int hd_write( QioIOQ *ioq, const void *buff, long count) {
    QioFile *file;
    file = qio_fd2file(ioq->file);
    return hd_writewpos(ioq, file->pos, buff, count);
}
    
static int hd_ioctl( QioIOQ *ioq, unsigned int arg1, void *arg2) {
    ioq->iostatus = HDIO_NOTSUPP;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static int hd_cancel( QioIOQ *ioq ) {
    ioq->iostatus = HDIO_NOTSUPP;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static int hd_open( QioIOQ *ioq, const char *name) {
    QioFile *file;
    DeviceDesc *d;
    file = qio_fd2file(ioq->file);
    file->pos = 0;                      /* current position in file */
    d = (DeviceDesc *)file->dvc->private;
#if IDE_MAX_CAPACITY
    file->size = IDE_MAX_CAPACITY < d->lba_capacity ? IDE_MAX_CAPACITY : d->lba_capacity;
#else
    file->size = d->lba_capacity;	/* size of file is whole disk */
#endif
    file->flags = 0;                    /* start at 0 for now */
    ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    ioq->iocount = ioq->file;           /* this is redundant */
    qio_complete(ioq);
    return 0;
}

static int hd_close( QioIOQ *ioq) {
    QioFile *file;
    ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    file->dvc = 0;                      /* file is no longer associated with device */
    qio_freefile(file);                 /* put file back on freelist */
    ioq->file = -1;                     /* tell 'em his fd is no good anymore */
    ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);                  /* call his completion routine */
    return 0;
}

static int hd_fstat( QioIOQ *ioq, struct stat *stat ) {
    DeviceDesc *d;
    ioq->iostatus = HDIO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    d = (DeviceDesc *)(qio_fd2file(ioq->file))->dvc->private;
    stat->st_mode = S_IFBLK;
#if IDE_MAX_CAPACITY
    stat->st_size = IDE_MAX_CAPACITY < d->lba_capacity ? IDE_MAX_CAPACITY : d->lba_capacity;
#else
    stat->st_size = d->lba_capacity;
#endif
    stat->st_blksize = 512;
    stat->st_blocks = d->lba_capacity;
    qio_complete(ioq);
    return 0;
}

static QioMutex hd_mutex[(NUM_HDRIVES-1)/2+1];

static const QioFileOps hd_fops = {
    hd_lseek,	/* lseek allowed on disk */
    hd_read, 	/* read allowed */
    hd_write,	/* writes allowed */
    hd_ioctl, 	/* ioctl may do something */
    hd_open,	/* open does something */
    hd_close,	/* close does something */
    0,		/* delete not allowed */
    0,		/* fsync not allowed */
    0,		/* mkdir not allowed */
    0,		/* rmdir not allowed */
    0,		/* rename not allowed */
    0,		/* truncate not allowed */
    0,		/* statfs not allowed */
    hd_fstat,	/* fstat allowed */
    hd_cancel,	/* cancel I/O required */
    0,		/* hd is not a tty */
    hd_readwpos,/* read with position */
    hd_writewpos /* write with position */
};

static const QioDevice hd_dvcs[] = {
    {"rd0",				/* device name */
     3,					/* length of name */
     &hd_fops,				/* list of file ops */
     hd_mutex,				/* drives 0 & 1 share the same mutex */
     0,					/* unit */
     (void *)(device_list+0)},		/* device specific parameters */

#if NUM_HDRIVES > 1
    {"rd1",				/* device name */
     3,					/* length of name */
     &hd_fops,				/* list of file ops */
     hd_mutex,				/* drives 0 & 1 share the same mutex */
     1,					/* unit */
     (void *)(device_list+1)},		/* device specific parameters */
#endif

#if NUM_HDRIVES > 2
    {"rd2",				/* device name */
     3,					/* length of name */
     &hd_fops,				/* list of file ops */
     hd_mutex+1,			/* drives 2 & 3 share the same mutex */
     2,					/* unit */
     (void *)(device_list+2)},		/* device specific parameters */
#endif

#if NUM_HDRIVES > 3
    {"rd3",				/* device name */
     3,					/* length of name */
     &hd_fops,				/* list of file ops */
     hd_mutex+1,			/* drives 2 & 3 share the same mutex */
     3,					/* unit */
    (void *)(device_list+3)},		/* device specific parameters */
#endif
};

/*
** ************************************************************
** ide_check_devstat():
** Repeatedly check for a connected hard drive until a valid
** device is found or the defined timeout period has elapsed.
** Return the result of the test to the calling function.
** ============================================================
** Usage:
**   status = ide_check_devstat(dev);
**
** Returns:
**   0	Device recognized.
**   1  No device recognized.
** ************************************************************
*/

static int ide_check_devstat( DeviceDesc *dev) {
    int status;
    U32 end_timer;
    VU8 alt_status;

    /*
    ** Look for a hard drive until a valid device is found or
    ** until the timeout has expired.  The test looks at the
    ** IDE Alternate Status register, if it contains the value
    ** 0x00 or 0xff then it is assumed that no hard drive is
    ** attached.
    */

    end_timer = eer_rtc;

    status = 1;
    do {
        alt_status = READ_BYTE(dev->alt_sts);
 
        if ( (alt_status != 0xff) && (alt_status != 0x00) ) {
            status = 0;
	    break;
        }
	if ((eer_rtc&15)) KICK_WDOG();	/* keep WDOG happy on these long transfers */
    } while( (eer_rtc - end_timer) < BUSY_TIMEOUT );

    return status;

}   /* End: ide_check_devstat() */

/*
** ************************************************
** ide_hread_data():
** Use the host processor to read in one sector of
** data from the hard drive.
** ================================================
** Usage:
**   status = ide_hread_data( rdbuf, nsectors );
**
**   U32 *rdbuf:    pointer to buffer to hold data.
**   U16 nsectors:  number of sectors to read.
**
** Returns:
**   Nothing.
** ************************************************
*/

static int ide_hread_data( DeviceDesc *dev, U32 *rdbuf, int nsectors ) {
    int ii, nloops;
    VU32 *ide_data_reg;
    struct ide_ctl *ide_dev;

    /* set pointer to base of IDE controller registers */
    ide_dev = dev->ideptr;

    /* set pointer to IDE controller data register */
    ide_data_reg = &ide_dev->overlaid.ldata;

    /* Calculate how many semi-unrolled loops to perform */
    nloops = (LONGS_PER_SECTOR * nsectors) / 16;

    /* read in the specified number of sectors */
    for( ii = 0; ii < nloops; ii++ ) {
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
         *rdbuf++ = *ide_data_reg;
     }
     return 0;
}   /* End: ide_hread_data() */

/*
** ************************************************
** ide_hwrite_data():
** Use the host processor to write n sectors' worth
** of data to the hard drive.
** ================================================
** Usage:
**   status = ide_hwrite_data( dev, rdbuf, nsectors );
**
**   DeviceDesc *dev: pointer to device specific parameters
**   U32 *rdbuf:    pointer to buffer to hold data.
**   int nsectors:  number of sectors to write.
**
** Returns:
**   Nothing.
** ************************************************
*/

static int ide_hwrite_data( DeviceDesc *dev, U32 *rdbuf, int nsectors ) {
    int ii, nloops;
    VU32 *ide_data_reg;
    struct ide_ctl *ide_dev;

    /* set pointer to base of IDE controller registers */
    ide_dev = dev->ideptr;

    /* set pointer to IDE controller data register */
    ide_data_reg = &ide_dev->overlaid.ldata;

    /* Calculate how many semi-unrolled loops to perform */
    nloops = (LONGS_PER_SECTOR * nsectors) / 16;

    /* write the specified number of sectors */
    for( ii = 0; ii < nloops; ii++ ) {
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
         *ide_data_reg = *rdbuf++;
     }
     return 0;
}   /* End: ide_hread_data() */

/*
** ********************************************************
** ide_identify():
** Send an IDENTIFY command to the IDE controller.
** The data returned is read directly using the host
** processor, not through an interrupt service routine.
** ========================================================
** Usage:
**   status = ide_identify( rdbuf );
**
**   U32 *rdbuf:  pointer to buffer for holding drive info.
**
** Returns:
**   Zero if no error occurred, 1 if an error occured.
** ********************************************************
*/

static int ide_identify( DeviceDesc *dev, U32 *rdbuf ) {
    U8 ackint;
    VU32 old_rtc;
    VU32 timeout;
    struct ide_ctl *ide_dev;
    int sts;

    /* Don't do anything if there's no device! */
    if ( ide_check_devstat(dev) ) return 1;

    /* set pointer to base of IDE controller registers */
    ide_dev = dev->ideptr;

    ide_send_byte(dev, offsetof(IdeCtl, csr), IDE_CMD_IDENTIFY );

    /*
    ** Wait for the drive to indicate data is waiting to be read
    */

    timeout = 0;
    old_rtc = eer_rtc;
    sts = 0;

    while( ((READ_BYTE(dev->alt_sts) & IDE_STB_DATAREQ) != IDE_STB_DATAREQ) &&
           ((timeout = eer_rtc - old_rtc) < DREQ_TIMEOUT) ) {
	if ((eer_rtc&15) == 0) KICK_WDOG();
    }

    /* read ID information only if 'wait for DREQ' loop didn't timeout */
    if ( timeout < DREQ_TIMEOUT ) {
	int ii;
	DriveID *id;

        /* acknowledge the HD interrupt and read the data */
        ackint = READ_BYTE(&ide_dev->csr);
        ide_hread_data( dev, rdbuf, 1 );
	id = (DriveID *)rdbuf;
	
	for (ii=0; ii<sizeof(id->serial_no); ii += 2) {	/* swap the bytes in the ASCII fields */
	    int tmp;
	    tmp = id->serial_no[ii];
	    id->serial_no[ii] = id->serial_no[ii+1];
	    id->serial_no[ii+1] = tmp;
	}
	for (ii=0; ii<sizeof(id->model); ii += 2) {
	    int tmp;
	    tmp = id->model[ii];
	    id->model[ii] = id->model[ii+1];
	    id->model[ii+1] = tmp;
	}
    }

    /* If an error occurred -- return it, else return zero for no error */
    if( READ_BYTE(dev->alt_sts) & IDE_STB_ERROR ) {
#if ARB_CTL || (HOST_BOARD == CHAMELEON)
        sts = READ_BYTE(&ide_dev->overlaid.bdata.precomp_error);
#else
        sts = 1;
#endif
    } 

    return sts;
}   /* End: ide_identify() */

static void setup_harddrive(void);

static void configure_timing_regs(int ii, DeviceDesc *dev) {
    struct ide_ctl *ide;
    int high, low;

    if ( (dev->flags&IDE_DVC_FLAG_ST9420) ) {
	ide_send_byte(dev, offsetof(IdeCtl, overlaid.bdata.scnt), 0);	/* no IDLE timer */
	ide_send_byte(dev, offsetof(IdeCtl, csr), 0xFA);	/* Set IDLE timer to 0, disabling it */
	ide_wait_not_busy(dev);
	ide_send_byte(dev, offsetof(IdeCtl, csr), 0xF9);	/* Enter Active mode */
	ide_wait_not_busy(dev);
    }		    
    ide = dev->ideptr;
    low = PCI_ReadConfigByte(PC87415_DEVICE_NUMBER,
		    offsetof(N415_CfigRegs, n415_Control[0]));
    low = (low&0xBF) | ((ii&2) ? 0x20 : 0x10); /* unmask INTA and map CHx to INTA */
    PCI_WriteConfigByte(PC87415_DEVICE_NUMBER,
		    offsetof(N415_CfigRegs, n415_Control[0]), low);
#if !PCI_SPEED
# define PCI_CYCLE_TIME	(30)				/* assume a 33MHZ PCI bus */
#else
# define PCI_CYCLE_TIME (10*100000000L/(PCI_SPEED))	/* compute cycle time in nanoseconds */
#endif
    low = dev->dma_ns % PCI_CYCLE_TIME;	 /* round it up to multiple of clock */
    if (low) dev->dma_ns += PCI_CYCLE_TIME - low;
    low = dev->dma_ns / PCI_CYCLE_TIME;	/* compute cycle time in clocks */

/* NOTE: Until we can get resolved what caps should be used on the IDE bus, we will
 * have to minimize the IDE cycle times to 240 nanoseconds (8 clocks). We do this here just to
 * make it easy.
 */
#if 0
    if (low < 3) low = 3;			/* minimize the value to 3 clocks */
#else
# ifndef TMP_IDE_LOW
# define TMP_IDE_LOW 8
# endif
    if (low < TMP_IDE_LOW) low = TMP_IDE_LOW;
#endif
    if (low > 33) low = 33;			/* maximize the value to 33 clocks */
    high = low/2;
    low = high + (low&1) - 1;
    low = (-high<<4) | (-low&0xF);
    dev->dma_timing = low;			/* save this so we can display it later */
    PCI_WriteConfigByte(PC87415_DEVICE_NUMBER, timing_regs[ii], low); /* set read timing */
    PCI_WriteConfigByte(PC87415_DEVICE_NUMBER, timing_regs[ii]+1, low); /* set write timing */
#if 0	/* someday we want to fool with this value */
    low = dev->pio_ns % PCI_CYCLE_TIME;		 /* round it up to multiple of clock */
    if (low) dev->pio_ns += PCI_CYCLE_TIME - low;
    low = dev->pio_ns / PCI_CYCLE_TIME;		/* compute cycle time in clocks */
    if (low < 3) low = 3;			/* minimize the value to 3 clocks */
    if (low > 33) low = 33;			/* maximize the value to 33 clocks */
    high = low/2;
    low = high + (low&1) - 1;
    low = (-high<<4) | (-low&0xF);
    dev->pio_timing = low;			/* save this so we can look at it later */
#endif
}

#if !defined(NSC415_INIT)
# define NSC415_INIT ide_init
#endif

#ifndef IDE_WAIT_VEC
# define IDE_WAIT_VEC nsc_wait_vec
#endif
#if NO_NSC_CHOOSE_DRV
extern
#endif
void (*IDE_WAIT_VEC)(int, int, int);

/*
** ****************************************************
** ide_init():
** Initialize the IDE device driver and all ide drives.
** ====================================================
** Usage:
**   ide_init(); Typically called at boot time.
**
** Returns:
**   Status of first connected device.
** ****************************************************
*/

int NSC415_INIT( void ) {
    U32 end_timer;
    int ii, jj, oldopt, setvec=0;
    DeviceDesc *dev;

    if (qio_install_dvc(hd_dvcs) < 0) return 0;	/* already init'd */

    setup_harddrive();

#if NSC415_MOVEABLE
    if (PC87415_DEVICE_NUMBER < 0) {
	BLABF(("PC87415_DEVICE_NUMBER is set to %d\n", PC87415_DEVICE_NUMBER));
	return -1;		/* not available */
    }
#endif

    end_timer = eer_rtc;			/* prepare for global powerup timeouts */

    oldopt = prc_delay_options(PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR);

#if TEST_DISK_ERRORS
    txt_str(-1, AN_VIS_ROW/4, "FAKE DISK ERRORS ENABLED", RED_PAL|AN_BIG_SET);
    prc_delay(4*60);
#endif

/* Hit hardware reset in case we got here via prc_reboot() */

    ide_hard_reset(0);				/* reset drives 0 and 1 */
#if NUM_HDRIVES > 2
    ide_hard_reset(2);				/* reset drives 2 and 3 */
#endif

#if NUM_HDRIVES < 3
# define IDE_PCI_CFG_INIT (0x00011230&~NSC415_CTL_CH1_INT_OFF)
#else
# define IDE_PCI_CFG_INIT (0x00011230&~(NSC415_CTL_CH1_INT_OFF|NSC415_CTL_CH2_INT_OFF))
#endif

    PCI_WriteConfigDword(PC87415_DEVICE_NUMBER,	offsetof(N415_CfigRegs, n415_Control[0]), IDE_PCI_CFG_INIT);
    for (ii=0; ii < NUM_HDRIVES; ++ii) {
	int sts;
	if (ii) qio_install_dvc(hd_dvcs + ii);
	dev = device_list + ii;
	if ((ii&2) == 0) {
	    dev->ideptr = (struct ide_ctl *)(PCI_IO_BASE+0x1F0);
	    dev->alt_sts = (VU8*)(PCI_IO_BASE+0x3F6);
	    dev->dma = (NSC415Cmd *)(PCI_IO_BASE+NSC415_DEFVAL_BAR4+0x00);
	} else {
	    dev->ideptr = (struct ide_ctl *)(PCI_IO_BASE+0x170);
	    dev->alt_sts = (VU8*)(PCI_IO_BASE+0x376);
	    dev->dma = (NSC415Cmd *)(PCI_IO_BASE+NSC415_DEFVAL_BAR4+0x08);
	}
	if ((ii&1) == 0) {
    	    dev->pdts = (NSC415Pdt *)QIO_MK_NONCACHE(QIO_ALIGN(dev->pdt, cpu_params.cpu_dcache_ls));
	    dev->lclbuff = QIOmalloc(HDIO_KERNEL_BUFSIZE+QIO_CACHE_LINE_SIZE);	/* grab an 'n' sector buffer */
	    if (!dev->lclbuff) return HDIO_NOMEM;	/* ran out of memory */
	    dev->lclbuff = (U32*)( QIO_ALIGN(dev->lclbuff, cpu_params.cpu_dcache_ls) );
	} else {
	    dev->lclbuff = (dev-1)->lclbuff;	/* drive 1 shares kernel buffer with drive 0 (3 shares with 2) */
	    dev->pdts = (dev-1)->pdts;		/* drive 1 shares pdts with drive 0 (3 shares with 2) */
	}
	dev->busy = 0;			/* not busy */
	dev->select = (ii&1) ? (1<<DEVICE_SELECT_BIT) : 0;
	dev->dma_timing = 0;
	dev->pio_timing = 0;
	dev->cyls = 0;
	dev->heads = 0;
	dev->sectors = 0;
	dev->lba_capacity = 0;
	dev->spc = 0;		/* sectors per cylinder */
	dev->status = 1;	/* assume invalid */

	ide_set_device(dev);

        /*
        ** Wait for the device to come out of reset.
        */

        for (sts=jj=0; (eer_rtc-end_timer) <  (POWERUP_TIMEOUT); ) {
	    sts = READ_BYTE(dev->alt_sts);
	    if (READ_BYTE(dev->alt_sts) != sts) continue;
	    if (sts == 0xff ) break;		/* assume there's no drive */
	    if (IDE_WAIT_VEC) IDE_WAIT_VEC(ii, (POWERUP_TIMEOUT-(eer_rtc-end_timer))/60, sts);
	    if ((eer_rtc&15)) KICK_WDOG();	/* keep WDOG happy on these long delays */
	    prc_delay(0);
            if ((sts&(IDE_STB_BUSY|IDE_STB_READY|IDE_STB_SEEKDONE)) ==
                     (             IDE_STB_READY|IDE_STB_SEEKDONE)) {
                break;
	    }
	}
	if ((sts&(IDE_STB_BUSY|IDE_STB_READY|IDE_STB_SEEKDONE)) !=
		 (             IDE_STB_READY|IDE_STB_SEEKDONE)) {
	    continue;			/* probably not there, do next one */
	}

        if( !(READ_BYTE(dev->alt_sts) & IDE_STB_BUSY) ) {
	    if ( ide_wait_not_busy(dev) ) continue;	/* drive is dead */
	    if (!setvec) {
#if !NSC415_MOVEABLE
		prc_set_vec(IDE_INTVEC, nsc_irq);	/* register our interrupt routine */
#else
# if HOST_BOARD == CHAMELEON
		BLABF(("Setting nsc ide vector to %d\n",
		    PC0_INTVEC+(PC87415_DEVICE_NUMBER-CHAM_PCICFG_SLOT0_V)));
		prc_set_vec(PC0_INTVEC+(PC87415_DEVICE_NUMBER-CHAM_PCICFG_SLOT0_V), nsc_irq);
# else
#  if HOST_BOARD != PHOENIX
		prc_set_vec( PC87415_DEVICE_NUMBER == PC87415_DEVICE_NUMBER_HW ? IDE_INTVEC : PCI_INTVEC, nsc_irq);
#  else
#   error * Need to add IDE vector setup code for SA-1 boards
#  endif
# endif
#endif
		setvec = 1;
	    }
	    if (!ide_identify( dev, dev->lclbuff )) {
		DriveID *id;
		id = (DriveID *)dev->lclbuff;
		dev->cyls = id->cyls;
		dev->heads = id->heads;
		dev->sectors = id->sectors;
		dev->spc = id->sectors*id->heads;
		dev->dma_ns = id->eide_dma_min;
		dev->pio_ns = id->eide_pio;
		dev->max_multsect = id->max_multsect;
		dev->lba_capacity = id->lba_capacity;
		if (strncmp((char *)id->model, "ST9420", 6) == 0) {	/* special for seagate 2.5" drive */
		    dev->flags |= IDE_DVC_FLAG_ST9420;
		}
		configure_timing_regs(ii, dev);
		dev->status = 0;
	    }
        }
    }
#if !IDE_NOTES
    prc_set_ipl(INTS_ON);		/* re-enable IDE interrupts */
#endif
#if TEST_DISK_ERRORS
    txt_clr_str(-1, AN_VIS_ROW/4, "FAKE DISK ERRORS ENABLED", RED_PAL|AN_BIG_SET);
#endif
    prc_delay(0);
    prc_delay_options(oldopt);

    flush_dcache();			/* make sure no cache lines point to non-cached kernel buffer(s) */

    /* return the status of device 0 */
    return device_list[0].status;

}   /* End: ide_init() */

#if TEST_DISK_ERRORS && USE_IDE_RESET_CHIP
static struct reset_hist {
    int sr;
    int init;	
    int pciinit;
    int sts;
    int act;
    int ast;
} reset_history[16];
static int rh_indx;
#endif

/*
** ****************************************************
** reset_chip():
** Initialize the IDE chip and all ide drives.
** ====================================================
** Usage:
**   reset_chip(); called by I/O completion routine to
**   try to recover from errors.
**
** Returns:
**   nothing
** ****************************************************
*/

static void reset_chip( void ) {
    int ii;

    for (ii=0; ii < NUM_HDRIVES; ii += 2) /* ide_hard_reset( ii ) */;

#if USE_IDE_RESET_CHIP
    {
	U32 end_timer;
	int jj, sts, oldps, pciinit;
	DeviceDesc *dev;

	oldps = prc_set_ipl(INTS_OFF);
	*(VU32*)RESET_CTL &= ~(1<<B_RESET_IDE);	/* reset the IDE chip */
	prc_wait_n_usecs(500);			/* wait a little */
	*(VU32*)RESET_CTL |=  (1<<B_RESET_IDE);	/* unreset the IDE chip */

	setup_harddrive();				/* set some PCI regs */

	pciinit = IDE_PCI_CFG_INIT|NSC415_CTL_CH1_INT_OFF|NSC415_CTL_CH2_INT_OFF;
	PCI_WriteConfigDword(PC87415_DEVICE_NUMBER,	offsetof(N415_CfigRegs, n415_Control[0]),
		    pciinit);
	prc_set_ipl(oldps);

	prc_wait_n_usecs(1000000);			/* wait 1 second for things to settle down */

	end_timer = eer_rtc;			/* prepare for global powerup timeouts */

	for (ii=0; ii < NUM_HDRIVES; ++ii) {

	    dev = device_list + ii;
	    ide_set_device(dev);
	    dev->status = 1;			/* assume the drive died */

	    /*
	    ** Wait for the device to come out of reset.
	    */

	    for (sts=jj=0; (eer_rtc-end_timer) <  (POWERUP_TIMEOUT); ) {
		sts = READ_BYTE(dev->alt_sts);
		if (READ_BYTE(dev->alt_sts) != sts) continue;
		if (sts == 0xff ) break;		/* assume there's no drive */
		if ((eer_rtc&15)) KICK_WDOG();	/* keep WDOG happy on these long delays */
		prc_wait_n_usecs(5000);
		if (sts == 0) continue;		/* Segate drives return 0 during powerup */
		if (!(sts&IDE_STB_BUSY)) break;
	    }

	    if ( sts == 0x00 || sts == 0xff ) continue; /* probably not there, do next one */

	    if( !(READ_BYTE(dev->alt_sts) & IDE_STB_BUSY) ) {
		configure_timing_regs(ii, dev);
		dev->status = 0;			/* assume drive ok */
		if (!(ii&1)) {
		    pciinit &= ~NSC415_CTL_CH1_INT_OFF;
		    PCI_WriteConfigDword(PC87415_DEVICE_NUMBER, offsetof(N415_CfigRegs,
			    n415_Control[0]), pciinit);
		} else {
		    pciinit &= ~NSC415_CTL_CH2_INT_OFF;
		    PCI_WriteConfigDword(PC87415_DEVICE_NUMBER, offsetof(N415_CfigRegs,
			    n415_Control[0]), pciinit);
		}
	    }
	}

# if TEST_DISK_ERRORS
	{
	    struct reset_hist *h = reset_history + rh_indx;
	    extern int _guts_inest, _guts_astlvl;
	    rh_indx += 1;
	    if (rh_indx >= n_elts(reset_history)) rh_indx = 0;
	    h->sr = prc_get_ipl();
	    h->init = PCI_ReadConfigDWord(PC87415_DEVICE_NUMBER,
			    offsetof(N415_CfigRegs, n415_Control[0]));
	    h->pciinit = pciinit;
	    h->sts = sts;
	    h->act = _guts_inest;
	    h->ast = _guts_astlvl;
	}
# endif    
    }
#endif
    return;

}   /* End: reset_chip() */

#if VERY_NOISY
/*
** **************************************************************
** ide_get_rpm():
** Calculate the approximate RPM of the IDE hard drive and return
** the result to the caller.
** ==============================================================
** Usage:
**    rpm = ide_get_rpm();
**
** Returns:
**    16-bit value with the approximate RPM of the hard drive.
** **************************************************************
*/

static int ide_get_rpm( DeviceDesc *dev ) {
    U32 revolutions = 0;
    U32 old_vcnt;
    int oipl;

    /* Don't do anything if there's no device! */
    if( ide_check_devstat(dev) ) return 0;

    ide_wait_not_busy(dev);

    oipl = prc_set_ipl(INTS_OFF);	/* disable interrupts for this */
    
    old_vcnt = prc_get_count();

    while( (prc_get_count() - old_vcnt) < CPU_SPEED/2 ) {	/* wait 1 second */
	U32 time;

	time = prc_get_count();
	while (1) {
	    if (READ_BYTE(dev->alt_sts) & IDE_STB_INDEX) break;
	    if ((prc_get_count()-time) > CPU_SPEED/20) break;	/* time it for 0.1 seconds */
	}
	time = prc_get_count();
        while (1) {
	    if (!(READ_BYTE(dev->alt_sts) & IDE_STB_INDEX)) break;
	    if ((prc_get_count()-time) > CPU_SPEED/20) break;	/* time it for 0.1 seconds */
	}
        revolutions += 60;
    }
    prc_set_ipl(oipl);

/* fudge it up or down */
    if (revolutions >= 3600-120 && revolutions <= 3600+120) revolutions = 3600;
    else if (revolutions >= 4500-120 && revolutions <= 4500+120) revolutions = 4500;
    else if (revolutions >= 5400-120 && revolutions <= 5400+120) revolutions = 5400;
    else if (revolutions >= 7200-120 && revolutions <= 7200+120) revolutions = 7200;

    return( revolutions );

}   /* End: ide_get_rpm() */

static int compute_avg(int *times, int nelts, int *max, int *min) {
    int ii;
    int tot;
    int lmin, lmax;
    lmin = 0x7FFFFFFF;
    lmax = 0;
    tot = 0;
    for (ii=0; ii < nelts; ++ii) {
	int t;
	t = times[ii]/(CPU_SPEED/2000000);
	if (t > lmax) lmax = t;
	if (t < lmin) lmin = t;
	tot += t;
    }
    if (max) *max = lmax;
    if (min) *min = lmin;
    return tot/nelts;
}
#endif

#ifndef NSC_SQUAWK
# define NSC_SQUAWK ide_squawk
#endif
#ifndef NSC_UNSQUAWK
# define NSC_UNSQUAWK ide_unsquawk
#endif
#if !defined(NSC415_CHOOSE_DRV)
# define NSC415_CHOOSE_DRV ide_choose_drv
#endif

#if !NO_NSC_CHOOSE_DRV
int NSC415_CHOOSE_DRV(int choices) {
    int col, active, ii;
    int bott=AN_VIS_ROW-3, new=1;
    U32 sws;

    if (!choices) return 0;
    txt_str(-1, AN_VIS_ROW/2, "Select drive to test:", MNORMAL_PAL);
    bott = st_insn(bott, "To begin test,", t_msg_action, INSTR_PAL);
    bott = st_insn(bott, "To select drive,", t_msg_control, INSTR_PAL);
    active = -1;
    while (1) {
	++active;
	if (active >= NUM_HDRIVES) active = 0;
	if (((1<<active)&choices)) break;
    }
    while (1) {
	if ((sws=ctl_read_sw(SW_ACTION|J_LEFT|J_RIGHT)&(SW_ACTION|SW_NEXT|J_LEFT|J_RIGHT))) {
	    if ((sws&SW_NEXT)) {
		active = -1;
		break;
	    }
	    if ((sws&SW_ACTION)) break;
	    if ((sws&J_LEFT)) {
		while (1) {
		    --active;
		    if (active < 0) active = NUM_HDRIVES-1;
		    if (((1<<active)&choices)) break;
		}
	    }
	    if ((sws&J_RIGHT)) {
		while (1) {
		    ++active;
		    if (active >= NUM_HDRIVES) active = 0;
		    if (((1<<active)&choices)) break;
		}
	    }
	    new = 1;
	}
	if (new) {
	    col = (AN_VIS_COL-4*NUM_HDRIVES)/2-1;
	    txt_str(col, AN_VIS_ROW/2+2, " ", WHT_PAL);
	    for (ii=0; ii < NUM_HDRIVES; ++ii) {
		int color;
		if (!((1<<ii)&choices)) color = RED_PAL;
		else if (ii == active) color = YEL_PALB;
		else color = MNORMAL_PAL;
		txt_cdecnum(ii, 1, RJ_ZF, color);
		txt_cstr("   ", MNORMAL_PAL);
	    }
	    new = 0;
	}
	prc_delay(0);
    }
    txt_clr_wid(1, ++bott, AN_VIS_COL-2);
    txt_clr_wid(1, ++bott, AN_VIS_COL-2);
    txt_clr_wid(1, AN_VIS_ROW/2, AN_VIS_COL-2);
    txt_clr_wid(1, AN_VIS_ROW/2+2, AN_VIS_COL-2);
    return active;
}

static struct show_drv {
    int row;
    int col;
} show_drv_details;

static void show_drv_sts(int drv, int time, int sts) {
    txt_str(show_drv_details.col, show_drv_details.row, "Waiting for drive", WHT_PAL);
#if NUM_HDRIVES > 1
    txt_cstr(" ", WHT_PAL);
    txt_cdecnum(drv, 1, RJ_BF, GRN_PAL);
#endif
    txt_cstr(", sts=", WHT_PAL);
    txt_chexnum(sts, 2, RJ_ZF, (sts&0x50) == 0x50 ? GRN_PAL : RED_PAL);
    txt_cstr(", timeout=", WHT_PAL);
    txt_cdecnum(time, 3, RJ_BF, WHT_PAL);
}

void NSC_SQUAWK(int row, int col) {
    show_drv_details.row = row;
    show_drv_details.col = col;
    IDE_WAIT_VEC = show_drv_sts;			/* drop in a function to call while waiting */
}

void NSC_UNSQUAWK(void) {
    txt_clr_wid(show_drv_details.col, show_drv_details.row, AN_VIS_COL-show_drv_details.col-1);
    IDE_WAIT_VEC = 0;				/* done with this */
}
#else
extern void NSC_SQUAWK(int row, int col);
extern void NSC_UNSQUAWK(void);
extern int NSC415_CHOOSE_DRV(int);
#endif

/*
** ********************************************************
** ide_test():
** IDE hard drive test, called by self test.
** ========================================================
** Usage:
**   retcode = ide_test( smp );
**
**   struct menu_d *smp:  pointer to self test menu struct.
**
** Returns:
**   Zero?
** ********************************************************
*/

#define TRKBUFSIZ (63 * BYTES_PER_SECTOR)

static int disk_test( const struct menu_d *smp ) {
    int row = 3;

    U32 ctls;

    U32 total_kbytes;
    U32 kbinc;
    U32 field;
    U32 iterations = 0;

    QioIOQ *ioq=0, *null=0;

    char tmp[AN_VIS_COL_MAX];
    DriveID *id;

    DeviceDesc *dev;
    struct ide_ctl *ide_dev;

    int ii, col, any, drvs, errors=0;
#ifdef EER_DSK_CORR
    int correctable_errs=0;
#endif

    F32 rate_min=0.0, rate_max=0.0, rate_tot=0.0, half_cpu;
    int whats_up=0, nxt_row=0, nxt_col=0;

    U32 uatrkbuf[(TRKBUFSIZ+QIO_CACHE_LINE_SIZE)/sizeof(U32)];
    U32 *trkbuf;

    half_cpu = CPU_SPEED;
    half_cpu = 2000000.0/half_cpu;

    trkbuf = (U32*)QIO_ALIGN(uatrkbuf, QIO_CACHE_LINE_SIZE);
    ExitInst(INSTR_PAL);

#define SHOW_DRV_CONNECT "Hard Drive Connected: "
#define SHOW_DRV_STATUS  "Alternate status:"
#define DRV_COL (2+sizeof(SHOW_DRV_CONNECT)-4)

    /* Show if the hard drive is connected */
    txt_str( 2, row, SHOW_DRV_CONNECT, MNORMAL_PAL );
#if NUM_HDRIVES > 1
    txt_str( 2, row+1, SHOW_DRV_STATUS, MNORMAL_PAL );
#endif

    prc_delay(0);				/* make sure we see what we've drawn so far */

    NSC_SQUAWK(row+NUM_HDRIVES, 2);

    /* initialize device driver if necessary */
    NSC415_INIT();				/* init the drivesubsystem */

    NSC_UNSQUAWK();

    prc_delay(0);				/* make sure we see what we've drawn so far */

    for (drvs=any=ii=0; ii < NUM_HDRIVES; ++ii) {
	int t=0;

	dev = device_list + ii;
	ide_set_device(dev);
	col = DRV_COL+ii*4;
#if NUM_HDRIVES > 1
	txt_decnum(col+1, row-1, ii, 4, RJ_BF, MNORMAL_PAL);
#endif
	if ( dev->status ) {
	    txt_str(col+2, row, " No ", RED_PAL );
	} else {
	    txt_str(col+2, row, "Yes ", GRN_PAL );
	    t = 1;
	    any |= 1<<ii;
	    ++drvs;
	}
#if NUM_HDRIVES > 1
	txt_hexnum(col+3, row+1, READ_BYTE(dev->alt_sts), 2, RJ_ZF, t?GRN_PAL:RED_PAL);
#endif
    }

    row += (NUM_HDRIVES > 1) ? 2 : 1;

    if (!any) {
	/* Wait for SW_NEXT before returning to main menu */
	ctl_read_sw(SW_ACTION|SW_NEXT);

	while ((ctl_read_sw(SW_NEXT|SW_ACTION)&(SW_NEXT|SW_ACTION)) == 0) {prc_delay(0);}

	return(0);
    }

    if (drvs > 1) {
	ii = NSC415_CHOOSE_DRV(any);
	if (ii < 0) goto done;		/* he wanted to abort */
    } else {
	for (ii=0; (any&(1<<ii)) == 0; ++ii) {;}
    }
	
#if NUM_HDRIVES > 1
#define TESTING_MSG "Testing drive: "
    txt_str(AN_VIS_COL-1-(sizeof(TESTING_MSG)-1)-4, 3, TESTING_MSG, MNORMAL_PAL);
    txt_cdecnum(ii, 1, RJ_ZF, GRN_PAL);
#endif

    dev = device_list + ii;			/* test first drive in chain */

    ioq = qio_getioq();			/* get an I/O queue */
    nsprintf(tmp, sizeof(tmp), "/rd%d", ii);
    qio_open(ioq, tmp, O_RDWR);		/* open the device */
    while (!ioq->iostatus) { ; }	/* wait for complete */

    ide_set_device(dev);

    /* set pointer to base of IDE controller interface registers. */
    ide_dev = dev->ideptr;

    prc_delay(0);			/* show what we've drawn so far */

    /* display drive identification */
    row++;
    ide_identify( dev, trkbuf );

    id = (DriveID *)trkbuf;
    id->model[sizeof(id->model)-1] = 0;
    txt_str( 2, row++, (char *)id->model, GRN_PAL );

    /* display the number of heads, cylinders, and sectors */
    ++row;
    
#if VERY_NOISY
    if (debug_mode & GUTS_OPT_DEVEL) {
	txt_str(2, row, "Heads:", MNORMAL_PAL );
	txt_cdecnum( dev->heads, 4, RJ_BF, GRN_PAL );
	txt_cstr( "  Cylinders:", MNORMAL_PAL );
	txt_cdecnum( dev->cyls, 5, RJ_BF, GRN_PAL );
	txt_cstr( "  SPT:", MNORMAL_PAL );
	txt_cdecnum( dev->sectors, 3, RJ_BF, GRN_PAL );
	++row;
    }
#endif

    txt_str(2, row, "Logical sectors available:", MNORMAL_PAL);
    txt_cdecnum(dev->lba_capacity, 10, RJ_BF, GRN_PAL);
    ++row;

#if IDE_MAX_CAPACITY
    {
	QioFile *f;
	f = qio_fd2file(ioq->file);
	if (f) f->size = dev->lba_capacity;	/* cheat and force file size to drive size for this test */
    }
#endif

#if VERY_NOISY
    if (debug_mode & GUTS_OPT_DEVEL) {
	int rpm;
	txt_str(2, row, "ns/DMA cycle:", MNORMAL_PAL);
	txt_cdecnum( dev->dma_ns, 4, RJ_BF, GRN_PAL);
	txt_cstr("  ns/PIO cycle:", MNORMAL_PAL);
	txt_cdecnum(dev->pio_ns, 4, RJ_BF, GRN_PAL);
	++row;
	txt_str(2, row++, "DMA timing register set to: ", MNORMAL_PAL);
	txt_chexnum( dev->dma_timing, 2, RJ_ZF, GRN_PAL);

	txt_str(2, row++, "Approx RPM: ", MNORMAL_PAL);
	prc_delay(0);
	rpm = ide_get_rpm(dev);
	txt_cdecnum(rpm, 5, RJ_BF, GRN_PAL);
	prc_delay(0);

	txt_str(2, row++, "Avg rotational latency: ", MNORMAL_PAL);
	nsprintf(tmp, sizeof(tmp), "%6.2f", 30000.0/(float)rpm);
	txt_cstr(tmp, GRN_PAL);
	txt_cstr(" milliseconds", MNORMAL_PAL);
    }
#endif

    prc_delay(0);
    
#define AMT_TO_READ	(TRKBUFSIZ/512)	/* no bigger than our buffer */
#define SKIP_SECTORS	(AMT_TO_READ+1) /* in case drive has "look for benchmark" ucode */

    ioq->timeout = DREQ_TIMEOUT*16000;		/* timeout in microseconds */

#if VERY_NOISY
    if (debug_mode & GUTS_OPT_DEVEL) do {
	U32 full, half, one16;
	int full_min, full_max;
	int half_min, half_max;
	int one16_min, one16_max;
	int rate_tot, time, lba, ii, dir;
	int times[64];
	F32 fzclock;
	VU32 zclock;
	U32 *oldzclock;

	oldzclock = prc_timer_ptr((U32*)&zclock);

	zclock = 0;
	rate_tot = 0;
	qio_readwpos(ioq, SKIP_SECTORS, trkbuf, TRKBUF_SIZE);
	while (!ioq->iostatus) { ; }
	if (QIO_ERR_CODE(ioq->iostatus)) break;
	for (ii=0; ii < n_elts(times); ++ii) {
	    time = prc_get_count();
	    qio_readwpos(ioq, (ii&1) ? SKIP_SECTORS : dev->lba_capacity-1-SKIP_SECTORS, trkbuf, AMT_TO_READ*512);
	    while (!ioq->iostatus) { ; }
	    times[ii] = prc_get_count()-time;
	    if (QIO_ERR_CODE(ioq->iostatus)) break;
	}	    
	if (QIO_ERR_CODE(ioq->iostatus)) break;
	full = compute_avg(times, n_elts(times), &full_max, &full_min);
	for (ii=0; ii < n_elts(times); ++ii) {
	    time = prc_get_count();
	    switch (ii&3) {
		case 1:
		    qio_readwpos(ioq, SKIP_SECTORS, trkbuf, AMT_TO_READ*512);
		    while (!ioq->iostatus) { ; }
		    break;
		case 2:
		case 0:
		    qio_readwpos(ioq, (dev->lba_capacity-2*SKIP_SECTORS)/2, trkbuf, AMT_TO_READ*512);
		    while (!ioq->iostatus) { ; }
		    break;
		case 3:
		    qio_readwpos(ioq, dev->lba_capacity-1-SKIP_SECTORS, trkbuf, AMT_TO_READ*512);
		    while (!ioq->iostatus) { ; }
		    break;
	    }
	    times[ii] = prc_get_count()-time;
	    if (QIO_ERR_CODE(ioq->iostatus)) break;
	}	    
	if (QIO_ERR_CODE(ioq->iostatus)) break;
	half = compute_avg(times, n_elts(times), &half_max, &half_min);
	dir = -dev->lba_capacity/16;
	lba = dev->lba_capacity + dir;
	for (ii=0; ii < n_elts(times); ++ii) {
	    time = prc_get_count();
	    qio_readwpos(ioq, lba, trkbuf, AMT_TO_READ*512);
	    while (!ioq->iostatus) { ; }
	    times[ii] = prc_get_count()-time;
	    if (QIO_ERR_CODE(ioq->iostatus)) break;
	    if (lba+dir < SKIP_SECTORS || lba+dir >= dev->lba_capacity-SKIP_SECTORS) {
		dir = -dir;
	    }
	    lba += dir;		
	}	    
	if (QIO_ERR_CODE(ioq->iostatus)) break;
	++row;
	fzclock = zclock;		/* freeze time here */
	one16 = compute_avg(times, n_elts(times), &one16_max, &one16_min);
	txt_str(2, row++, "               Full     Half     1/16  strokes", MNORMAL_PAL);
	txt_str(2, row++, "Seeks: Avg:", MNORMAL_PAL);
	nsprintf(tmp, sizeof(tmp), "%9.2f%9.2f%9.2f",
		(float)full/1000.0, (float)half/1000.0, (float)one16/1000.0);
	txt_cstr(tmp, GRN_PAL);
	txt_cstr(" milliseconds", MNORMAL_PAL);
	txt_str(2, row++, "       Max:", MNORMAL_PAL);
	nsprintf(tmp, sizeof(tmp), "%9.2f%9.2f%9.2f",
		(float)full_max/1000.0, (float)half_max/1000.0, (float)one16_max/1000.0);
	txt_cstr(tmp, GRN_PAL);
	txt_str(2, row++, "       Min:", MNORMAL_PAL);
	nsprintf(tmp, sizeof(tmp), "%9.2f%9.2f%9.2f",
		(float)full_min/1000.0, (float)half_min/1000.0, (float)one16_min/1000.0);
	txt_cstr(tmp, GRN_PAL);
	txt_str(2, row++, "Total test time:", MNORMAL_PAL);
	nsprintf(tmp, sizeof(tmp), "%4.1f", fzclock/1000.0);
	txt_cstr(tmp, GRN_PAL);
	txt_cstr(" sec, overall avg: ", MNORMAL_PAL);
	nsprintf(tmp, sizeof(tmp), "%4.1f", fzclock/193.0);
	txt_cstr(tmp, GRN_PAL);
	txt_cstr("  Millisecs", MNORMAL_PAL);
	prc_timer_ptr(oldzclock);
    } while (0);
#endif

    row += (debug_mode & GUTS_OPT_DEVEL) ? 2 : 3;
    txt_str( 8, row,   "LBA  MB/sec", MNORMAL_PAL );
    txt_str( 12, row+1, "00.000 avg", MNORMAL_PAL );
    txt_str( 12, row+2, "00.000 min", MNORMAL_PAL );
    txt_str( 12, row+3, "00.000 max", MNORMAL_PAL );
    txt_str( 2, row+5, "Tests completed: ", MNORMAL_PAL );

#if 0
    /* print the date information from the last sector on the drive */
    if (debug_mode & GUTS_OPT_DEVEL) {
	qio_readwpos(ioq, dev->lba_capacity-1, trkbuf, 512);
	while (!ioq->iostatus) { ; }
	if (!QIO_ERR_CODE(ioq->iostatus)) {
	    char *bptr;
	    bptr = (char *)trkbuf;
	    tmp[0] = '.';
	    tmp[1] = 0;
	    tmp[3] = 0;
	    txt_str( 2, row+7, "Date code: ", MNORMAL_PAL);
	    for (ii=0; ii < 36; ++ii) {
		if (bptr[ii] < '!' || bptr[ii] > '~') {
		    txt_cstr(tmp, RED_PAL);
		} else {
		    tmp[2] = bptr[ii];
		    txt_cstr(tmp+2, GRN_PAL);
		}
	    }
	}
    }
#endif

    prc_delay(0);

    null = qio_getioq();
    qio_open(null, "/null", O_RDWR);
    ioq->iostatus = 0;

    while(1) {
	QioIOQ *whichio;
	int sts;

	int time, etime, lba, maxsec, inner;

	whichio = ioq /* null */;

        total_kbytes = 0;
        kbinc = (BYTES_PER_SECTOR * dev->sectors) / 1024;

        txt_decnum( 19, row+5, iterations, 4, RJ_BF, GRN_PAL );
        prc_delay(0);		/* show something */
	time = 0;
	maxsec = AMT_TO_READ;
        for( inner = 0, lba = 0; lba < dev->lba_capacity; ++inner) {
	    int seccnt;
#ifdef EER_DSK_CORR
	    int p_recover, recover;
#endif

	    seccnt = maxsec;
	    if (lba+seccnt > dev->lba_capacity) {
		seccnt = dev->lba_capacity - lba;
	    }
	    txt_decnum( 2, row+1, lba, 9, RJ_BF, GRN_PAL );

#ifdef EER_DSK_CORR
	    p_recover = eer_gets(EER_DSK_CORR);
#endif
	    field = prc_get_count();

	    sts = qio_readwpos(whichio, lba, trkbuf, seccnt*512);

	    while (!sts) sts = whichio->iostatus;

	    etime = prc_get_count();

	    if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) goto done;

#ifdef EER_DSK_CORR
	    recover = eer_gets(EER_DSK_CORR);
	    if (recover != p_recover) {
		correctable_errs += recover - p_recover;
		if (!(whats_up&2)) {
		    txt_str(5, row+7, "Correctables:", MNORMAL_PAL);
		    whats_up |= 2;
		}
		txt_decnum(19, row+7, correctable_errs, 4, RJ_BF, YEL_PAL);
	    }
#endif
#if TEST_DISK_ERRORS && USE_IDE_RESET_CHIP
	    {
		struct reset_hist *h = reset_history;
		int ii, lrow = row+9;
		for (ii=0; ii < n_elts(reset_history); ++ii, ++h, ++lrow) {
		    char emsg[AN_VIS_COL_MAX];
		    nsprintf(emsg, sizeof(emsg), "sr=%08lX, i=%lX, pci=%lX, sts=%02X, act=%d, ast=%d",
			h->sr, h->init, h->pciinit, h->sts, h->act, h->ast);
		    txt_str(1, lrow, emsg, (ii == rh_indx) ? YEL_PAL : GRN_PAL);
		}
	    }
#endif
	    if ( QIO_ERR_CODE(sts) ) {
		char emsg[AN_VIS_COL_MAX-13-2];
		lba += whichio->iocount/512;	/* skip to failling lba */
		if (!(whats_up&1)) {
		    txt_str(2, (debug_mode & GUTS_OPT_DEVEL) ? row-1 : row-2, "Last error: ", MNORMAL_PAL);
		    txt_str(3, row+6, "Uncorrectables:", MNORMAL_PAL);
		    txt_str(25, row, "List of bad sectors:", MNORMAL_PAL);
		    whats_up |= 1;
		}
		qio_errmsg(whichio->iostatus, emsg, sizeof(emsg));
		txt_clr_wid(14, (debug_mode & GUTS_OPT_DEVEL) ? row-1 : row-2, AN_VIS_COL-14-1);
		txt_str(14, (debug_mode & GUTS_OPT_DEVEL) ? row-1 : row-2, emsg, YEL_PAL);
		txt_decnum(19, row+6, ++errors, 4, RJ_BF, RED_PAL);
		prc_delay(0);
		if (!nxt_col || nxt_col > AN_VIS_COL-1-8) {
		    if (nxt_row >= AN_VIS_ROW-3) {
			prc_delay(0);
			continue;
		    }
		    if (!nxt_row) nxt_row = row;
		    ++nxt_row;
		    nxt_col = 24;
		}
		txt_decnum(nxt_col, nxt_row, lba, 8, RJ_BF, RED_PAL);
		++lba;			/* skip bad sector */
		nxt_col += 8;
	    } else {
		lba += seccnt;
		time += etime - field;
		total_kbytes += seccnt;
		if ( time > CPU_SPEED/2 /* || !(inner&1023) */ ) {
		    float ftime=0.0, frate;
		    ftime = time;
		    ftime *= half_cpu;
		    frate = total_kbytes*512 + 511;
		    frate /= ftime;
		    if (rate_min == 0.0 || rate_min > frate) rate_min = frate;
		    if (rate_max < frate) rate_max = frate;
		    rate_tot += frate;
		    nsprintf(tmp, sizeof(tmp), "%6.3f", frate);
		    txt_str(12, row+1, tmp, GRN_PAL);
		    nsprintf(tmp, sizeof(tmp), "%6.3f", rate_min);
		    txt_str(12, row+2, tmp, GRN_PAL);
		    nsprintf(tmp, sizeof(tmp), "%6.3f", rate_max);
		    txt_str(12, row+3, tmp, GRN_PAL);
		    total_kbytes = 0;
		    time = 0;
		    prc_delay(0);
		}
	    }
	}
	iterations++;
    }      /* End: while(1) */

    /* Wait for SW_NEXT before returning to main menu */
    while( (ctl_read_sw(0) & SW_ACTION) != 0 ) prc_delay(0);

    do { ctls = ctl_read_sw(SW_NEXT);
	prc_delay(0);
    } while( !(ctls & SW_NEXT) );

done:

    if (ioq) {
	qio_close(ioq);
	qio_freeioq(ioq);
    }
    if (null) {
	qio_close(null);
	qio_freeioq(null);
    }

    return(0);

}   /* End: disk_test() */

#if INCLUDE_FSYS
extern	int	fsys_test(const struct menu_d *);
# if HOST_BOARD == CHAMELEON
#  define DT_MSG "415 DISK TESTS"
# else
#  define DT_MSG "DISK TESTS"
# endif
static const struct menu_d dt_menu[] = {
    {	DT_MSG,			0		},
    {	"\nDRIVE TEST",		disk_test	},
    {	"\nFILESYSTEM CHECK",	fsys_test	},
    { 0, 0 }
};
#endif

#if !defined(NSC415_TEST)
# define NSC415_TEST ide_test
#endif

int NSC415_TEST(const struct menu_d *smp) {
#if !INCLUDE_FSYS
    return disk_test();
#else
    return st_menu(dt_menu, sizeof(dt_menu[0]), MNORMAL_PAL, 0);
#endif
}

/****************************************************************************
    Copyright (C) 1995 National Semiconductor Corp.  All Rights Reserved
*****************************************************************************
*
*   File:               NSC_415.C
*
*   Purpose:            Functions for handling the NSC PC87415 IDE
*                       controller chip.
*
*   Update History:      1/19/95 GDV Created get/set registers
*                        1/24/95 MF  Created calc registers
*                        4/23/96 DMS Change names of types
*
****************************************************************************/

#ifndef NUM_HDRIVES
# define NUM_HDRIVES	(1)        	/* default to number of IDE devices supported */
#endif

/* timing limits for the PC87415: */
#define DBA_CLKMIN              2       /* Data Byte Active */
#define DBA_CLKMAX              17
#define DBR_CLKMIN              1       /* Data Byte Recover */
#define DBR_CLKMAX              16

#define CBA_CLKMIN              3       /* Command Byte Active */
#define CBA_CLKMAX              17
#define CBR_CLKMIN              3       /* Command Byte Recover */
#define CBR_CLKMAX              18

PUBLIC void NSC415_GetCurrentRegs   /* Gets current '415 IDE register values */
(
    DEVHANDLE       devhand,            /* handle for device access */
    NSC415_Regs *   regptr,             /* ptr to returned PC87415 reg values */
    BOOL            getdma              /* TRUE to get bus master DMA registers */
)
{
    UINT            i;                  /* loop counter */
    BYTE *          dbptr;              /* pointer to bytes in structure */
    WORD            baseadr;            /* base address for bus master regs */
    DWORD	    tmp;

    dbptr = (BYTE *)&(regptr->nsc_ConfigRegs);

    for (i = 0; i < sizeof(N415_CfigRegs); ++i, ++dbptr)
        *dbptr = PCI_ReadConfigByte(devhand, (BYTE)i);

    if (getdma)
    {
      /* BAR4 always reads back with bit 0 set */
        baseadr = ((WORD)regptr->nsc_ConfigRegs.n415_BaseAddr4) & 0x0FFFE;

        dbptr = (BYTE *)&(regptr->nsc_BusMastRegs);

        for (i = 0; i < sizeof(N415_MastRegs); ++i, ++dbptr, ++baseadr) {
	  tmp =  *dbptr = (*(int *)((baseadr + PCI_IO_BASE) & 0xfffffffc));
	  *dbptr = (BYTE)(tmp >> (8 * ((baseadr + PCI_IO_BASE) & 0x03)));
	}
    }
}

PUBLIC void NSC415_SetRegValues     /* Sets actual '415 IDE controller regs */
(
    DEVHANDLE       devhand,            /* handle for device access */
    NSC415_Regs *   regptr              /* ptr to values to set PC87415 regs */
)
{
    N415_CfigRegs * figptr;             /* pointer to configuration registers */

    figptr  = &(regptr->nsc_ConfigRegs);

    PCI_WriteConfigWord(devhand, (BYTE)((DWORD)&(figptr->n415_Command  ) - (DWORD)figptr), figptr->n415_Command);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_ProgIface) - (DWORD)figptr), figptr->n415_ProgIface);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_Latency  ) - (DWORD)figptr), figptr->n415_Latency);

#if 1
    PCI_WriteConfigByte( devhand, (BYTE)((DWORD)&(figptr->n415_Control[0]) - (DWORD)figptr), figptr->n415_Control[0]);
    PCI_WriteConfigByte( devhand, (BYTE)((DWORD)&(figptr->n415_Control[1]) - (DWORD)figptr), figptr->n415_Control[1]);
    PCI_WriteConfigByte( devhand, (BYTE)((DWORD)&(figptr->n415_Control[2]) - (DWORD)figptr), figptr->n415_Control[2]);
#else
    PCI_WriteConfigDword( devhand, (BYTE)((DWORD)&(figptr->n415_Control[0]) - (DWORD)figptr), *(DWORD*)figptr->n415_Control);
#endif
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr0)  - (DWORD)figptr), figptr->n415_BaseAddr0);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr1)  - (DWORD)figptr), figptr->n415_BaseAddr1);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr2)  - (DWORD)figptr), figptr->n415_BaseAddr2);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr3)  - (DWORD)figptr), figptr->n415_BaseAddr3);
    PCI_WriteConfigDword(devhand, (BYTE)((DWORD)&(figptr->n415_BaseAddr4)  - (DWORD)figptr), figptr->n415_BaseAddr4);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D1_Dread ) - (DWORD)figptr), figptr->n415_C1D1_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D1_Dwrite) - (DWORD)figptr), figptr->n415_C1D1_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D2_Dread ) - (DWORD)figptr), figptr->n415_C1D2_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C1D2_Dwrite) - (DWORD)figptr), figptr->n415_C1D2_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D1_Dread ) - (DWORD)figptr), figptr->n415_C2D1_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D1_Dwrite) - (DWORD)figptr), figptr->n415_C2D1_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D2_Dread ) - (DWORD)figptr), figptr->n415_C2D2_Dread);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_C2D2_Dwrite) - (DWORD)figptr), figptr->n415_C2D2_Dwrite);

    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_CmdCtrl_RdWrt) - (DWORD)figptr), figptr->n415_CmdCtrl_RdWrt);
    PCI_WriteConfigByte(devhand, (BYTE)((DWORD)&(figptr->n415_SectorSize   ) - (DWORD)figptr), figptr->n415_SectorSize);
}

#if HOST_BOARD == CHAMELEON
# define TO_PHYS(x) ((x)&0xFFFE)
#else
# define TO_PHYS(x) (K1_TO_PHYS(x))
#endif

PUBLIC void NSC415_InitRegValues    /* Initializes values for '415 IDE regs */
(
    BOOL *          dodrive,            /* TRUE for each drive to initialize */
    NSC415_Regs *   regptr              /* ptr to returned PC87415 reg values */
)
{
    N415_CfigRegs * nptr;               /* pointer to configuration registers */
    N415_MastRegs * bptr;               /* pointer to bus master registers */

    nptr = &(regptr->nsc_ConfigRegs);

    nptr->n415_Command    = 0x0145;     /* enable bus master and errors */
    nptr->n415_SectorSize = 0xEE;       /* sector size = 512 bytes */

    nptr->n415_ProgIface   =  0x8A;     /* enable master IDE and legacy mode */
    nptr->n415_Control[0]  =  0x70;	/* No write to Vendor ID regs, mask INTA ... */
    					/* map both ints to INTA, turn on PWR, turn off IDE resets */
    nptr->n415_Control[1] &= ~0x0B;     /* disable data phase watchdog, unmask both interrupts */
    nptr->n415_Control[1] |=  0x03;     /* mask both interrupts */

#if 0
# define M0_TIMING	(0xEF)
# define CC_TIMING	(0xF0)
#else
# define M0_TIMING	(0x1B)
# define CC_TIMING	(0xB7)
#endif

    if (dodrive[0] || dodrive[1])       /* configure the first channel */
    {
        nptr->n415_ProgIface  &= ~0x01; /* use legacy mode, not BAR 0,1 */
        nptr->n415_Control[1] &= ~0x48; /* map IDE to BAR 0,1, disable watchdog */
        nptr->n415_Control[1] |=  0x11; /* mask int, buffer BAR 0,1 accesses */
        nptr->n415_Control[2] |=  0x01; /* enable buffers for first channel */
	nptr->n415_BaseAddr0 = TO_PHYS(PCI_IO_BASE) + 0x400;	/* set the base registers */
	nptr->n415_BaseAddr1 = TO_PHYS(PCI_IO_BASE) + 0x408;

        if (dodrive[0]) {
	    nptr->n415_Control[2] &= ~0x10; /* use IORDY for first drive */
	    nptr->n415_C1D1_Dread  = M0_TIMING;  /* use mode 0 timings */
	    nptr->n415_C1D1_Dwrite = M0_TIMING;
	}

        if (dodrive[1]) {
	    nptr->n415_Control[2] &= ~0x20; /* use IORDY for second drive */
	    nptr->n415_C1D2_Dread  = M0_TIMING;
	    nptr->n415_C1D2_Dwrite = M0_TIMING;
	}
    }

    if (dodrive[2] || dodrive[3])       /* configure the second channel */
    {
        nptr->n415_ProgIface  &= ~0x04;	/* use legacy mode, not BAR 2,3 */
        nptr->n415_Control[1] &= ~0x8C; /* map IDE to BAR 2,3, disable watchdog */
        nptr->n415_Control[1] |=  0x22; /* mask int, buffer BAR 2,3 accesses */
        nptr->n415_Control[2] |=  0x02; /* enable buffers for second channel */
	nptr->n415_BaseAddr2 = TO_PHYS(PCI_IO_BASE) + 0x410;	/* set the base registers */
	nptr->n415_BaseAddr3 = TO_PHYS(PCI_IO_BASE) + 0x418;
	nptr->n415_Control[1] &= ~0x0B;     /* disable data phase watchdog, unmask both interrupts */

        if (dodrive[2]) {
	    nptr->n415_Control[2] &= ~0x40; /* use IORDY for first drive */
	    nptr->n415_C2D1_Dread  = M0_TIMING;
	    nptr->n415_C2D1_Dwrite = M0_TIMING;
	}

        if (dodrive[3]) {
	    nptr->n415_Control[2] &= ~0x80; /* use IORDY for second drive */
	    nptr->n415_C2D2_Dread  = M0_TIMING;
	    nptr->n415_C2D2_Dwrite = M0_TIMING;
	}
    }

    nptr->n415_CmdCtrl_RdWrt = CC_TIMING;

    nptr->n415_BaseAddr4 = TO_PHYS(PCI_IO_BASE) + NSC415_DEFVAL_BAR4;

    bptr = &(regptr->nsc_BusMastRegs);

    bptr->n415_Mast1_Cmd  = 0x00;       /* stop any DMA transfers */
    bptr->n415_Mast1_Stat = 0x06;       /* reset error/interrupts */

    bptr->n415_Mast2_Cmd  = 0x00;       /* stop any DMA transfers */
    bptr->n415_Mast2_Stat = 0x06;       /* reset error/interrupts */
}

#define DUMP_REGS 0

#if DUMP_REGS
static int dump_regs(NSC415_Regs *dregs, int row) {
    N415_CfigRegs *regs;

    regs = &dregs->nsc_ConfigRegs;

    txt_str(1, row++, "Vendor ", WHT_PAL);
    txt_chexnum(regs->n415_VendorID, 4, RJ_ZF, WHT_PAL);
    txt_cstr("  Device ", WHT_PAL);
    txt_chexnum(regs->n415_DeviceID, 4, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "CMD ", WHT_PAL);
    txt_chexnum(regs->n415_Command, 4, RJ_ZF, WHT_PAL);
    txt_cstr("  Status ", WHT_PAL);
    txt_chexnum(regs->n415_Status, 4, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "Revison ", WHT_PAL);
    txt_chexnum(regs->n415_RevisionID, 2, RJ_ZF, WHT_PAL);
    txt_cstr("  Device ", WHT_PAL);
    txt_chexnum(regs->n415_ProgIface, 2, RJ_ZF, WHT_PAL);
    txt_cstr(" SubClass ", WHT_PAL);
    txt_chexnum(regs->n415_SubClass, 2, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "ClassCode ", WHT_PAL);
    txt_chexnum(regs->n415_ClassCode, 2, RJ_ZF, WHT_PAL);
    txt_cstr(" Latency ", WHT_PAL);
    txt_chexnum(regs->n415_Latency, 2, RJ_ZF, WHT_PAL);
    txt_cstr("  Device ", WHT_PAL);
    txt_chexnum(regs->n415_HeadType, 2, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "BAddr0 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr0, 8, RJ_ZF, WHT_PAL);
    txt_cstr("     BAddr1 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr1, 8, RJ_ZF, WHT_PAL);
    txt_str(1, row++, "BAddr2 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr2, 8, RJ_ZF, WHT_PAL);
    txt_cstr("     BAddr3 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr3, 8, RJ_ZF, WHT_PAL);
    txt_str(1, row++, "BAddr4 " , WHT_PAL);
    txt_chexnum(regs->n415_BaseAddr4, 8, RJ_ZF, WHT_PAL);

    txt_str(1, row++, "CmdCtl " , WHT_PAL);
    txt_chexnum(regs->n415_CmdCtrl_RdWrt, 2, RJ_ZF, WHT_PAL);
    txt_cstr("     SectorSize " , WHT_PAL);
    txt_chexnum(regs->n415_SectorSize, 2, RJ_ZF, WHT_PAL);

    return row;
}

typedef volatile struct ide_regs {
    unsigned char data;
    unsigned char err_precomp;
    unsigned char sect_cnt;
    unsigned char sect_numb;
    unsigned char low_cyl;
    unsigned char hi_cyl;
    unsigned char drv_hd;
    unsigned char csr;
    unsigned char filler[0x1FE];
    unsigned char di_fd;
    unsigned char alt_sts;
} IdeRegs;
#endif

static void setup_harddrive(void) {
    NSC415_Regs   disk_controller_regs;
    BOOL          drives[3];

#if NSC415_MOVEABLE
    if ((PC87415_DEVICE_NUMBER = get_device_number(NSC415_VENDOR_ID)) < 0) {
	BLAB("Didn't find NSC415 chip\n");
	return;
    }
    BLABF(("Found NSC415 chip at device %d\n", PC87415_DEVICE_NUMBER));
#endif

/* grab registers */
    NSC415_GetCurrentRegs(PC87415_DEVICE_NUMBER, \
                        &disk_controller_regs, (BOOL)FALSE);

/* initialize the NS87415 */
    drives[0] = (NUM_HDRIVES > 0);
    drives[1] = (NUM_HDRIVES > 1);
    drives[2] = (NUM_HDRIVES > 2);
    drives[3] = (NUM_HDRIVES > 3);

    NSC415_InitRegValues(&drives[0], &disk_controller_regs);

/* store everything back */
    NSC415_SetRegValues(PC87415_DEVICE_NUMBER, &disk_controller_regs);

#if DUMP_REGS
/* grab registers again */
    NSC415_GetCurrentRegs(PC87415_DEVICE_NUMBER, \
                        &disk_controller_regs, 0);

    dump_regs(&disk_controller_regs, 4);

    while (1) prc_delay(0);
#endif

    return;
}

#if 0
static void nsc_drvconfig(int reg, U32 value) {
    PCI_WriteConfigDword(PC87415_DEVICE_NUMBER, reg, value);
    return;
}
#endif
/***************************************************************************/
