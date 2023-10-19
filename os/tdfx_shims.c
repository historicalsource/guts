#include <config.h>
#include <os_proto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uart.h>
#include <stdarg.h>
#include <nsprintf.h>
#include <qio.h>
#include <uart.h>
#include <fsys.h>
#include <fcntl.h>
#include <errno.h>
#include <phx_proto.h>
#include <wms_proto.h>
#include <intvecs.h>

#if !defined(BR9600)
# define BR9600 IO_UART_CTL_9600_BAUD
#endif

extern int ide_init(void);

static struct ta {
    char buf0, buf1;
    char tabuf[256];
    volatile unsigned char in;
    unsigned char out;
} typeahead;

static void uart_input(QioIOQ *ioq) {
    typeahead.tabuf[typeahead.in] = *(char *)ioq->user;
    if (typeahead.in+1 != typeahead.out) ++typeahead.in;
    qio_read(ioq, ioq->user, 1);
}

int getch(void) {
    while (typeahead.in == typeahead.out) { ; }	/* wait for a character to show up */
    return typeahead.tabuf[typeahead.out++];	/* got one, give it to 'em */
}

int kbhit(void) {
    return typeahead.in == typeahead.out ? 0 : 1;
}

extern void qio_uart_init(void);

static void report_error(QioIOQ *ioq, const char *prefix, int sts, int stop) {
    char emsg[AN_VIS_COL_MAX];
    qio_errmsg(sts, emsg, sizeof(emsg));
    if (ioq) {
	sts = qio_write(ioq, prefix, strlen(prefix));
	while (!sts) sts = ioq->iostatus;
	sts = qio_write(ioq, ":\r\n", 3);
	while (!sts) sts = ioq->iostatus;
	sts = qio_write(ioq, emsg, strlen(emsg));
	while (!sts) sts = ioq->iostatus;
	sts = qio_write(ioq, "...", 3);
	while (!sts) sts = ioq->iostatus;
    } else {
	fprintf(stderr, "%s:\r\n%s...", prefix, emsg);
    }
    if (stop) while (1) { ; }
}

static void our_prc_delay(void) {
    prc_wait_n_usecs(16000);
}

static int timer_ticks;
static void timer_int(void) {
    ++timer_ticks;
    if (timer_ticks > 16) {
	++eer_rtc;
	timer_ticks = 0;
    }
}

void tdfx_init(void) {
    QioIOQ *i0, *i1;
    int sts;
    
    prc_set_vec(PRC_DELAY_INTVEC, our_prc_delay);
    prc_set_vec(TIMER_INTVEC, timer_int);

    qio_uart_init();
    i0 = qio_getioq();

    sst_text_init(0, 0);

    qio_open(i0, "/tty0", O_WRONLY);

    if (!(*(VU32*)IO_DIPSW&IO_DIPSW2)) {
	struct qio_uart termio;
	qio_ioctl(i0, QIO_UART_IOCGC, &termio);
	termio.cflag |= QIO_UART_CFLG_CTSIN;	/* signal to honor CTS in */
	qio_ioctl(i0, QIO_UART_IOCPC, &termio);
    }

    i1 = qio_getioq();
    i1->file = 0;
    sts = qio_close(i1);
    while (!sts) { sts = i1->iostatus; }
    if (QIO_ERR_CODE(sts)) report_error(i0, "Error closing fd 0", sts, 1);
    stdin = freopen("/tty0", "r", stdin);
    if (!stdin) report_error(i0, "Error reopening stdin", errno, 1);
    i1->file = 1;
    sts = qio_close(i1);
    while (!sts) { sts = i1->iostatus; }
    if (QIO_ERR_CODE(sts)) report_error(i0, "Error closing fd 1", sts, 1);
    stdout = freopen("/tty0", "w", stdout);
    if (!stdout) report_error(i0, "Error reopening stdout", errno, 1);
    i1->file = 2;
    sts = qio_close(i1);
    while (!sts) { sts = i1->iostatus; }
    if (QIO_ERR_CODE(sts)) report_error(i0, "Error closing fd 2", sts, 1);
    stderr = freopen("/tty0", "w", stderr);
    if (!stderr) report_error(i0, "Error reopening stderr", errno, 1);
    stderr->_flags |= __SNBF;		/* stderr is unbuffered */
    qio_close(i0);

    sts = qio_open(i0, "/tty0", O_RDONLY);
    while (!sts) { sts = i0->iostatus; }
    if (QIO_ERR_CODE(sts)) report_error(0, "Error opening /tty0 for input", sts, 1);
    i0->complete = uart_input;
    *i1 = *i0;
    i0->user = (void *)&typeahead.buf0;
    i1->user = (void *)&typeahead.buf1;
    sts = qio_read(i0, i0->user, 1);
    if (sts) report_error(0, "Error queing first read on tty0", sts, 1);
    sts = qio_read(i1, i1->user, 1);
    if (sts) report_error(0, "Error queing second read on tty0", sts, 1);
    
}

void sleep(int time) {
    while ( time > 0) { prc_delay(60); --time; }
    return;
}

#if 0
extern void tdfx_cmd_loop(void);
extern void prc_init_vecs(void);

int GT64010rev;

void hdw_init(int coldflag) {
    int oldtxt;
    int ii, id;

#if !BOOT_FROM_DISK && defined(WDOG) && !EPROM_ST && !NO_WDOG
    WDOG = 0;
#endif

    prc_wait_n_usecs(1000);

    gt64010_init();                     /* init the PCI part of the Galileo */
    
    *(VU32*)GALILEO_PCI_RETRY = 0xFF070F;	/* see what this does */

/* Clipped from WMS's stdio.c */
/* Make sure the PCI Endianess is is LITTLE */

    ii = 10000;
    do {
        *(VU32*)GALILEO_PCI_CMD = 1;
    } while ( !( (*(VU32 *)GALILEO_PCI_CMD) & 1) && --ii);

    if (!ii) {
	__asm__("BREAK");
    }
    
    *(VU32 *)(GALILEO_BASE+0xC28) = 0x3f;		/* enable SERR bits */
    put_pci_config_reg(0, 1, 0x1FF);			/* enable all PCI operations */

/* Set PCI (sst1) memory response area to 0x08000000 (physical) */
/* First Turn on memory */

    id = get_sst_device_number();

    if (id > 0) {
        put_pci_config_reg(id, 4, -1);
        get_pci_config_reg(id, 4);
        put_pci_config_reg(id, 4, 0x08000000);
        get_pci_config_reg(id, 1);
        put_pci_config_reg(id, 1, 2);
        put_pci_config_reg(id, 4, 0x08000000);

        prc_wait_n_usecs(1000);

        put_pci_config_reg(id, 1, 2);

        prc_wait_n_usecs(1000);

        put_pci_config_reg(id, 4, 0x8000000);
    } else {
	__asm__("BREAK");
    }

    *(VU32*)GALILEO_INT_CAUSE = 0;	/* clear any stuck interrupt causes */

    qio_init();
    fsys_init();
    oldtxt = txt_select(TXT_NONE);	/* no vid_init() yet, turn off text */
    prc_init_vecs();
    prc_set_ipl(INTS_ON);		/* enable interrupts */
    tdfx_cmd_loop();
}

time_t time(time_t *ptr) {
    if (ptr) *ptr = 0;
    return 0;
}

# ifndef JUNK_POOL_SIZE
#  define JUNK_POOL_SIZE	(64*1024)
# endif
# if JUNK_POOL_SIZE
#  if JUNK_POOL_SIZE > 0
static U8 junk_memory[JUNK_POOL_SIZE];
#  else
extern U8 bss_end[];
#  endif
U8 *junk_brk_value;
int junk_free_size;
static int junk_inited;
# endif

void *_sbrk_r(struct _reent *ptr, size_t amt) {
    extern struct _reent qio_reent;
    void *old;
    if (ptr == &qio_reent) return qio_sbrk(ptr, amt);
# if JUNK_POOL_SIZE
    if (!junk_inited) {
#  if JUNK_POOL_SIZE > 0
	junk_brk_value = junk_memory;
	junk_free_size = sizeof(junk_memory);
#  else
	junk_brk_value = bss_end;
	junk_free_size = ((U8 *)&old - junk_brk_value) - 128*1024;
#  endif
	junk_inited = 1;
    }
    if (amt > junk_free_size || junk_free_size+amt < junk_free_size) {
	ptr->_errno = ENOMEM;
	return (void *)-1;
    }
    old = (void *)junk_brk_value;
    junk_brk_value += amt;
    junk_free_size -= amt;
# else
    ptr->_errno = ENOMEM;
    old = (void *)-1;
# endif
    return old;
}
#endif
