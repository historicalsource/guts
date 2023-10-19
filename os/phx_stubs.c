/* $Id: phx_stubs.c,v 1.52 1997/10/02 22:03:19 shepperd Exp $
 *
 *	This contains some interfaces to allow one to use an XBUSMON board
 *	on the ROMBUS connector (or XBUS) of a Phoenix host so that the selftest
 *	menu will appear on a (dumb) serial terminal instead of or in addition
 *	to the normal video menu. It also optionally swaps the buttons to those
 *	on the XBUSMON board so one can it to run the selftest diags on a system
 *	with a broken UIO (or other misconfigured or broken I/O board).
 *
 *	I make no pretense that this code will be useful for anything other
 *	than a Phoenix board at this time. Good luck making it work on a 68k or
 *	R3K host for example (not impossible).
 *
 *		Copyright 1996 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#include <os_proto.h>
#include <phx_proto.h>
#include <stdarg.h>
#include <math.h>
#include <wms_proto.h>

#if INCLUDE_QIO
#include <stdio.h>
#include <errno.h>
#include <qio.h>
#include <fsys.h>
#endif

extern int twi_vfprintf(int (*ortn)(void *, const char *, int), void *iop, int size, const char *fmt, va_list ap);

#define BREAK() __asm__("BREAK")

void breakpoint(void) {
    __asm__("BREAK");
}

#if !NO_EXIT_FUNC
void promexit(int arg) { while (1) BREAK(); }
void exit(int arg) { while (1) BREAK(); }
#endif

#if !NO_MATHERR
int matherr(struct exception *h) {
    return 0;
}
#endif

#if ANNOUNCE_BOOT_ACTIONS
extern void shims_putc(char c);
extern int shims_puts(const char *);
extern int shims_putc_disable;

static int wchrs(void *filler, const char *ptr, int len) {
    static char last_c;
    int olen = len;
    char c;
    if (shims_putc_disable) return 0;
    while (len--) {
	c = *ptr++;
	if (c == '\n' && last_c != '\r') shims_putc('\r');
	shims_putc(c);
	last_c = c;
    }
    return olen;
}
#endif

#if INCLUDE_QIO
time_t time(time_t *ptr) {
    if (ptr) *ptr = 0;
    return 0;
}
#endif			/* INCLUDE_QIO */

#if !INCLUDE_QIO

int puts(const char *str) {
# if ANNOUNCE_BOOT_ACTIONS
    return shims_puts(str);
# else
    return 0;
# endif
}

int printf(const char *fmt, ...)
{
# if ANNOUNCE_BOOT_ACTIONS
    int ret;
    va_list ap;

    va_start(ap, fmt);

    ret = twi_vfprintf(wchrs, 0, 0, fmt, ap);
    va_end(ap);

    return ret;
# else
    return 0;
# endif
}

int fprintf(void *filler, const char *fmt, ...) {
# if ANNOUNCE_BOOT_ACTIONS
    int ret;
    va_list ap;

    va_start(ap, fmt);

    ret = twi_vfprintf(wchrs, 0, 0, fmt, ap);
    va_end(ap);

    return ret;
# else
    return 0;
# endif
}
#endif		/* !INCLUDE_QIO */

int shims_printf(const char *fmt, ...)
{
#if ANNOUNCE_BOOT_ACTIONS
    int ret;
    va_list ap;

    va_start(ap, fmt);

    ret = twi_vfprintf(wchrs, 0, 0, fmt, ap);
    va_end(ap);

    return ret;
#else
    return 0;
#endif
}

int shims_info(int indent, const char *fmt, ...)
{
#if ANNOUNCE_BOOT_ACTIONS
# if 0			/* Glide is WAY too noisy with info enabled */
    int ret;
    va_list ap;

    for (; indent > 0; --indent) {
	shims_putc(' ');
	shimc_putc(' ');
    }

    va_start(ap, fmt);

    ret = twi_vfprintf(wchrs, 0, 0, fmt, ap);
    va_end(ap);

    return ret;
# else
    return 0;
# endif
#else
    return 0;
#endif
}

#if USE_XBUSMON_CTLMODLATCH
static U32 cntr_bitshd;

U32 ctl_mod_latch(int x) {
    U32 old;
    int oldipl = prc_set_ipl(INTS_OFF);
    old = cntr_bitshd;
    if (x < 0) {
	cntr_bitshd &= x;
    } else {
	cntr_bitshd |= x;
    }
    prc_set_led(cntr_bitshd);
    prc_set_ipl(oldipl);
    return old;
}
#endif

#define n_elts(x) (sizeof(x)/sizeof(x[0]))

#ifndef BLAB
# define BLAB(x) do { ; } while (0)
#endif
#ifndef BLABF
# define BLABF(x) do { ; } while (0)
# define NO_PCI_DUMP	1
#endif

#if GALILEO_BASE
int GT64010rev;
#endif

#if !NO_PCI_DUMP
static void dump_pci_devices(void) {
    int ii;
    int id;

# if GALILEO_BASE
    for (ii = 0; ii < 11; ii++) {
	BLABF(("\nStubs: PCI device %2d has ID of ... ", ii));
	id = get_pci_config_reg(ii, 0);
	BLABF(("%04lX", id&0xFFFF));
	if (ii==0) ii = 6-1;
    }
# else
    for (ii = 0; ii < 6; ii++) {
	BLABF(("\nStubs: PCI device %2d has ID of ... ", ii));
	id = get_pci_config_reg(ii, 0);
	BLABF(("%04lX", id&0xFFFF));
    }
# endif
    return;
}
#endif

#ifndef VIRT_TO_PHYS
# define VIRT_TO_PHYS K1_TO_PHYS
#endif

void init_3dfx_pci(void) {
    int id;

/* Set PCI (sst1) memory response area to K0_TO_PHYS(SST_BASE) (physical) */
/* First Turn on memory */

    id = get_sst_device_number();

    if (id > 0) {

        put_pci_config_reg(id, 4, -1);
        get_pci_config_reg(id, 4);
        put_pci_config_reg(id, 4, VIRT_TO_PHYS(SST_BASE) );
        get_pci_config_reg(id, 1);
        put_pci_config_reg(id, 1, 2);
        put_pci_config_reg(id, 4, VIRT_TO_PHYS(SST_BASE) );

        prc_wait_n_usecs(1000);

        put_pci_config_reg(id, 1, 2);

        prc_wait_n_usecs(1000);

        put_pci_config_reg(id, 4, VIRT_TO_PHYS(SST_BASE) );
    } else {
	__asm__("BREAK");
    }
}

#if defined(SSEG_T)
void walker_led(void) {
    int ii = 0x1;
    while (1) {
	SSEG_T = ~ii;
	prc_wait_n_usecs(100000);
	ii = ii + ii;
	if ((ii&0x40)) {
	    ii = 0x01;
	}
    }
}
#endif

/*	For historical reasons, prc_init_vecs() was called
 *	individualy. In an attempt to move some host-board-
 *	generic stuff out of config.mac and into host-board-
 *	generic source, MEA calls it from here. We should
 *	#include intvecs.h for the extern, but that would
 *	break a bunch of pre-existing Makefiles.
 */
extern void prc_init_vecs(void);
extern void malloc_init(void);

void reent_init(void) {
}

void hdw_init(int coldflag) {
    int oldtxt;

    malloc_init();

#if !BOOT_FROM_DISK && defined(WDOG) && !EPROM_ST && !NO_WDOG
    WDOG = 0;
#endif

    prc_wait_n_usecs(1000);

#if GALILEO_BASE
    BLAB("Stubs: Set GALILEO's PCI registers...");
    gt64010_init();                     /* init the PCI part of the Galileo */
    
    BLABF(("\nStubs: GALIILEO version %s", GT64010rev > 1 ? "2 (aka A)" : "1"));

    *(VU32*)GALILEO_PCI_RETRY = 0xFF070F;	/* see what this does */

/* Clipped from WMS's stdio.c */
/* Make sure the PCI Endianess is is LITTLE */

    {
	int ii = 10000;
	BLAB("\nStubs: Set GALILEO to little endian...");
	do {
	    *(VU32*)GALILEO_PCI_CMD = 1;
	} while ( !( (*(VU32 *)GALILEO_PCI_CMD) & 1) && --ii);

	if (!ii) {
	    BLAB("Failed\n");
	    __asm__("BREAK");
	}
    }
    
    BLAB("\nStubs: Turn on SERR bits...");
    
    *(VU32 *)(GALILEO_BASE+0xC28) = 0x3f;		/* enable SERR bits */
    put_pci_config_reg(0, 1, 0x1FF);			/* enable all PCI operations */
#endif

#if !NO_PCI_DUMP
    dump_pci_devices();
#endif

#if GALILEO_BASE
    *(VU32*)GALILEO_INT_CAUSE = 0;	/* clear any stuck interrupt causes */
#endif

#if INCLUDE_QIO
    qio_init();
# if INCLUDE_FSYS
    fsys_init();
# endif
#endif

    oldtxt = txt_select(TXT_NONE);	/* no vid_init() yet, turn off text */

#if HOST_BOARD == CHAMELEON
    prc_set_count(0);
    prc_set_compare(prc_get_count() + CPU_SPEED/2000);
    prc_set_cause(0);
#endif

    prc_init_vecs();
    BLAB("\nStubs: Finished setting interrupt vectors...");
#if ANNOUNCE_BOOT_ACTIONS && !NO_TDFX_TESTS
    if (!(*(VU32*)IO_DIPSW&(IO_DIPSW1|IO_DIPSW3))) {
	extern void tdfx_cmd_loop(void);
	eer_init();			/* need to init this for the ide/fsys code */
	BLAB("\nStubs: Enabling interrupts...");
	prc_set_ipl(INTS_ON);
	BLAB("\r\n");
	shims_putc_disable = 1;		/* turn off blab code */
	tdfx_cmd_loop();
	txt_select(oldtxt);		/* put it back */
	shims_putc_disable = 0;		/* continue with startup blab */
    }
#endif
}
