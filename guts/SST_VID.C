/* $Id: sst_vid.c,v 1.78 1997/10/15 06:09:52 shepperd Exp $
 *
 *	Video handling subroutines for 3DFX video board.
 *
 *		Copyright 1996 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#ifndef B_TEST
#define B_TEST (-1)
#else
# ifndef TEST_DEBOUNCE_FRAMES
# define TEST_DEBOUNCE_FRAMES 30
# endif
#endif
#include <string.h>
#include <limits.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>
#include <intvecs.h>
#include <eer_defs.h>
#include <phx_proto.h>
#include <wms_proto.h>
#include <glide.h>
#include <math.h>
#include <sst_tdfx.h>
#include <nsprintf.h>
#if GUN_CNT
#include <cham_gun.h>
#endif

#define TEST_SST_RESET 0	/* set non-zero to enable sst_reset test */

#if HOST_BOARD == CHAMELEON
# ifndef BLAB
#  define BLAB(x)  do { ; } while (0)
# endif
# ifndef BLABF
#  define BLABF(x) do { ; } while (0)
# endif
#endif

#define STATIC static

/* You might want to define PHX_PUTS(x) to phx_puts(x) */

#if !defined(PHX_PUTS)
# define PHX_PUTS(x) 
#endif

#undef SST_BUSY
#define SST_BUSY() (*(VU32*)SST_BASE&(1<<7)) 

#define n_elts(array) (sizeof(array)/sizeof(array[0]))

void (*gameint)();
void (*volatile tpllvec)(void);

void (*vid_set_vb ( void (*new_rtn)() ))()
{
    return prc_set_vec(GAMEVB_INTVEC,new_rtn);
}

static TimerPll pll_data;

TimerPll *get_timer_pll(void) {
    return &pll_data;
}

/*		vid_fields( frame_rate )
 *	Establish the frame rate, for such hardware as needs special
 *	treatment to switch buffers, show the current screen, etc.
 *	if <frame_rate> < 0, that part of the video (e.g. GXn video RAM)
 *	will be turned off. Otherwise, <frame_rate> specifies the number
 *	of fields (vertical traces of the CRT screen) to display for each
 *	frame (unique visual image) of video. <frame_rate>s from 0..4
 *	should be supported on any hardware. Video hardwares that do not
 *	have such a concept can simply return 0.
 *
 *	(fields < 0) now means "video generation off", while (fields == 0)
 *	means "do not wait at all to swap buffers".
 */

int vid_fields( frame_rate )
int frame_rate;
{
    int retval;
    retval = pll_data.fields_frame;
    pll_data.fields_frame = frame_rate;
    return retval; 
}

void sst_bufswap(void) {
    if (pll_data.fields_frame >= 0) {
	++pll_data.swap_in;		/* let interrupt routine know we asked to swap buffers */
#if USE_AGC_BUFSWAP
	agcBufferSwap( (pll_data.fields_frame << 1) | 1 );
#else
	grBufferSwap( pll_data.fields_frame );
#endif
    }
    return;
}

#if defined(B_PIC_PCI) && USE_PCI_VSYNC
# define VSYNC_IE_BIT B_PIC_PCI
# define VSYNC_CAUSE_BIT B_PIC_PCI
# define VSYNC_IVEC PCI_INTVEC
# undef SST_VSYNC_ACK			/* force it to use the timer for vsync enable */
#else
# if defined B_PIC_VSY
#  define VSYNC_IE_BIT B_PIC_VSY
#  define VSYNC_CAUSE_BIT B_PIC_VSY
#  define VSYNC_IVEC VSY_INTVEC
# endif
#endif

#if (HOST_BOARD != CHAMELEON) && (!defined(SST_VSYNC_ACK) && defined(VSYNC_IE_BIT))
static struct tq vb_tq;

static void test_vb_reen(void *arg) {
    *(VU32*)INTCTL_IE |= 1<<VSYNC_IE_BIT;	/* re-enable vsync interrupts */
    return;
}
#endif
    
static void frame_handler(void) {
    int do_frame=0;
    TimerPll *pll;

    pll = get_timer_pll();		/* point to our timer stuff */
    if (pll->fields_frame > 0) {
	int swaps;
	swaps = (pll->swap_in-pll->swap_out); /* get number of swaps requested */
	if (swaps) {			/* he asked for a swap */
	    if ( swaps > grBufferNumPending()) { /* he asked for at least one swap and at least one happened */
		++pll->swap_out;	/* record that it happened */
		do_frame = 1;		/* signal to do something about it */
	    }
	} else {
	    int over;
	    swaps = (pll->field - pll->field_last);
	    over = pll->fields_frame/2+1;
	    if (swaps >= pll->fields_frame + over) {
		++pll->frame_overrun;	/* signal there was a frame overrun */
		do_frame = 1;
	    }
	}
    } else {
	do_frame = 1;			/* fake a frame */
    }
    if (do_frame) {
	++pll->frame;			/* signal the frame */
	if (pll->fields_frame > 0 && framevec) framevec();	/* call user's frame function if there is one */
	pll->field_last = pll->field; /* record the field counter at this frame */
    }
}

extern U32 sst_memfifo_min;
#if B_TEST >= 0
static int ts_been_on;
static int ts_been_off;
#endif

static void sst_vb_tasks(int how) {
    TimerPll *pll;

    pll = get_timer_pll();
    pll->ticks = 0;			/* ticks are always reset at vb */

    if (how) {				/* if we got here due to a VB IRQ */
	prc_timer_rate(prc_timer_rate(0));	/* resync the timer */
	prc_timer_jobs(0);		/* fake a timer interrupt */
	pll->nxt_upd = pll->ints_upd;
    }

    eer_hwt();				/* once per field, update eeprom */
    ctl_upd_sw();			/* once per field, update switches */

#if QUAD_CNT
    ctl_upd_quad();			/* once per field, update quadrature switches */
#endif
#if GUN_CNT
    gun_vb();
#endif

#if SHOW_FIFO_COUNT_COL || SHOW_FIFO_COUNT_ROW
    {
	U32 cook;
	cook = txt_setpos(0);
# if !SHOW_FIFO_COUNT_COL
#  undef SHOW_FIFO_COUNT_COL
#  define SHOW_FIFO_COUNT_COL (-1)
# endif
# if !SHOW_FIFO_COUNT_ROW
#  undef SHOW_FIFO_COUNT_ROW
#  define SHOW_FIFO_COUNT_ROW (AN_VIS_ROW-1)
# endif
	txt_hexnum(SHOW_FIFO_COUNT_COL, SHOW_FIFO_COUNT_ROW, sst_memfifo_min, 4, RJ_ZF, WHT_PALB);
	txt_setpos(cook);
    }
#endif

#if B_TEST >= 0
# ifdef ST_SWITCH
  if ( go_to_self_test == 0 ) {
# endif
    if ( (TEST & (1<<B_TEST)) ) {
	if (ts_been_off < 0x1000) ++ts_been_off;
	ts_been_on = 0;
    } else {
	if ( ts_been_off > TEST_DEBOUNCE_FRAMES ) {
	    if ( ++ts_been_on > TEST_DEBOUNCE_FRAMES ) {
# ifdef ST_SWITCH
		go_to_self_test = 1;
# endif
		prc_reboot();
	    }
	} else {
	    ts_been_off = 0;
	}
    }
# ifdef ST_SWITCH
  }
# endif
#endif

#if POT_CNT
    { /* Until I find out where to prototype these ... */
	extern void VBIReadPots();
	extern void PotsToSwitches();
	VBIReadPots();
	PotsToSwitches();
    } /* ... fom 3/28/96 */
#endif

    if (gameint) gameint();		/* call user's vb routine if there is one */
    ++pll->field;			/* advance field */
#if !NO_BLINKING_LED
# if defined(LED_OUT)
    if ((pll->field&0xF) == 0) {
	if (pll->field&0x10) {
	    *(VU32*)LED_OUT |= (1<<B_LED_GRN);
	} else {
	    *(VU32*)LED_OUT &= ~(1<<B_LED_GRN);
	}
    }
# else
#  if defined(IO_MAIN_CTL_T) && IO_MAIN_LED_ON
    if ((pll->field&0xF) == 0) {
	int oldps;
	oldps = prc_set_ipl(INTS_OFF);
	if (pll->field&0x10) {
	    IO_MAIN_CTL_T |= IO_MAIN_LED_ON;
	} else {
	    IO_MAIN_CTL_T &= ~IO_MAIN_LED_ON;
	}
	prc_set_ipl(oldps);
    }
#  endif
# endif
#endif
    if (pll->fields_frame <= 0 || (pll->field-pll->field_last) >= pll->fields_frame) {
	frame_handler();
    }
    return;
}

#if defined(VSYNC_IE_BIT)
static void sst_vb(void) {
    U32 ie;
# if defined(INTCTL_CAUSE_T)
    ie = INTCTL_CAUSE_T;
# else
    ie = *(VU32*)INTCTL_CAUSE;
# endif
    if ( !(ie&(1<<VSYNC_CAUSE_BIT)) ) return;	/* not us */
# if defined(SST_VSYNC_ACK)
    SST_VSYNC_ACK();		/* ack the VSYNC interrupt. */
# else
#  if defined(INTCTL_IE_T)
    INTCTL_IE_T &= ~(1<<VSYNC_IE_BIT);	/* disable VSYNC interrupts */
#  else
    *(VU32*)INTCTL_IE &= ~(1<<VSYNC_IE_BIT);	/* disable VSYNC interrupts */
#  endif
    vb_tq.func = test_vb_reen;
    vb_tq.delta = 4000;			/* wait 4 milliseconds */
    tq_ins(&vb_tq);			/* enable a restart later */
# endif
    if (pll_data.no_vsync < 1) sst_vb_tasks(1);
}
#endif

void timer_pll() {
    TimerPll *pll;
    FxU32 sc1, scanline;
    VU32 *sst = (VU32*)(SST_BASE+0x204);

    pll =  &pll_data;
    ++pll->ticks;			/* count this tick */
    if (!pll->no_vsync && pll->ticks > 16) return; /* don't do anything if too many ints occured */
    if (--pll->nxt_upd <= 0) {
	cn_irq();			/* handle coins */
	pll->nxt_upd = pll->ints_upd;	/* reset the timer */
	if (ms4vec) ms4vec();
    }
    if (pll->no_vsync > 0) {		/* if there's no h/w VB interrupts */
	do {
	    sc1 = *sst&0x7FF;		/* get number of line currently being displayed */
	    scanline = *sst&0x7FF;
	} while (sc1 != scanline);    	/* until two reads in a row match values */
#if HOST_BOARD == CHAMELEON
	if (pll->ticks > 4 && (scanline > pll->vlines || /* if we've marched off the end of visible screen */
    			       scanline < pll->prev_scan)) { /* or started a new display */
	    sst_vb_tasks(0);		/* fake a VB interrupt */
	}
#else
	if (pll->ticks > 4 && scanline > pll->vlines) {	/* if we've marched off the end of visible screen */
	    sst_vb_tasks(0);		/* fake a VB interrupt */
	}
#endif
	pll->prev_scan = scanline;
    }
    return;
}

void (*prc_delay_vec)(void);

static int prc_delay_flags;

U32 prc_delay_options(U32 new) {
    U32 old;
    old = prc_delay_flags;
    prc_delay_flags = new;
    return old;
}

static int no_ints_cntr;
static int bg_color;

int vid_set_bg(int new) {
    int old;
    old = bg_color;
    bg_color = new;
    return old;
}

void sst_prc_delay(void) {
    int ortc = eer_rtc;
    int oframe = pll_data.frame;	/* record the current frame number */

    while ( SST_BUSY() ) { ; }		/* wait for all rendering to stop (may also wait for VB) */
    while (oframe == pll_data.frame) {	/* wait until a frame happens */
	if (ortc == eer_rtc) {		/* if no VB since waiting for busy */
	    if (vid_waitvb(1) == 0) {	/* returns 0 if interrupts are off */
		eer_hwt();		/* keep this going if interrupts are off */
		ctl_upd_sw();		/* check switches once in awhile too */
		++no_ints_cntr;		/* record this fact */
		break;			/* assume 1 field/frame */
	    }
	    if (pll_data.fields_frame <= 0) break; /* if no video, assume 1 field/frame */
	}
    }
    if (pll_data.fields_frame >= 0) {
	if ((prc_delay_flags&PRC_DELAY_OPT_TEXT2FB)) {
	    sst_text2fb(0);
#if GUN_CNT
	    gun_draw_crosshairs();
#endif
	}
	if (prc_delay_flags&PRC_DELAY_OPT_SWAP) sst_bufswap();
	if (prc_delay_flags&PRC_DELAY_OPT_CLEAR) grBufferClear( bg_color, 0, GR_WDEPTHVALUE_FARTHEST );
    }
}

void prc_delay(int cnt) {
    do {
#if defined(WDOG) && !NO_WDOG
	WDOG = 0;
#endif
	if (prc_delay_vec) {
	    (*prc_delay_vec)();
	} else {
	    sst_prc_delay();
	}
    } while (cnt-- > 0);
}

void prc_delay0() {
   prc_delay(0);
}

/*	In self-test, we call eer_hwt to update EEPROM (and bump eer_rtc)
 *	and call ctl_upd_swtch() to debounce switches. Separately "debounce"
 *	self-test switch (if any :-) to transit from selftest to game.
 */

#ifndef WDOG
int WDOG;
#endif

void
st_vblank()
{
    WDOG = 0;
}

volatile unsigned long vb_count;
void vid_clr_alphas(void);

extern void st_dispmenu(const U8 *menu, U32 opt_bits, int erase);

static const unsigned char sst_menu[] =
    "\042Resolution\000*Medium\000\000\000\000"
    "\063Frame rate\000No Delay\000*60 Hz\00030 Hz\00020 Hz\000"
    "15 Hz\00012 Hz\00010 Hz\000No ZRE\000"
;

static U32 sst_opt_memo;

#define NCACHE_READ (300)	/* nanoseconds to read from SST chip */
#define TOO_LONG (20000000L)	/* A field should not take > 20 million nanoseconds */

#if 0
#define WREFWIDTH 640.
#define WREFHEIGHT 480.0

#define WINSCALEX(x) ((x/WREFWIDTH) * wWidth)
#define WINSCALEY(y) ((y/WREFHEIGHT) * wHeight)  
#endif

extern void breakpoint(void);
void grErrorCallback(const char *msg, int val) { PHX_PUTS(msg); while(1) __asm__("BREAK"); }

GrHwConfiguration hwconfig;

static int vid_warm;

#define NVBLANK (*sst&(1<<6))		/* negative true VB signal */

#if CPU_SPEED_VARIABLE
# if HOST_BOARD == CHAMELEON
#  error * You cannot use variable CPU_SPEED on this host
# else
int CPU_SPEED;
int cpu_ticks_usec;
# endif
#endif

static void time_vb(void) {
    int ii,oldps;
    U32 field_time, line_time, lc1, lc2, start;
    VU32 *sst, *lctr;
#if GALILEO_TIMER2
    U32 oldctl;
    VU32 *timr, *tctl;
#endif
    TimerPll *pll;

    sst = (VU32*)SST_BASE;
    lctr = (VU32*)(SST_BASE+0x204);
    pll = get_timer_pll();
#if GALILEO_TIMER2
    timr = (VU32*)GALILEO_TIMER2; 
    tctl = (VU32*)GALILEO_TIMER_CTL;

# define GET_TIME() *timr
    oldctl = *tctl;
    *tctl &= ~(3<<4);			/* disable timer 2 */
    *timr = 0x00FFFFFC;			/* set timer to max */
    *tctl |= (3<<4);			/* turn on timer 2 */
#else
# define GET_TIME() prc_get_count()
#endif
    oldps = prc_set_ipl(INTS_OFF);	/* no interrupts to get the most accurate times */
    while (  NVBLANK ) ;		/* if not in VB, wait until VB*/
    while ( !NVBLANK ) ;		/* if in VB, wait until not in VB */
    start = GET_TIME();			/* record start time */
#if CPU_SPEED_VARIABLE
    CPU_SPEED = prc_get_count();
#endif
    while (  NVBLANK ) ;		/* wait for a VB to show up */
    while ( !NVBLANK ) ;		/* wait for VB to deassert */
#if HOST_BOARD == CHAMELEON
    field_time = GET_TIME() - start;
#else
    field_time = start - GET_TIME();
#endif
    lc1 = *lctr & 0x7FF;
    while ((lc2=(*lctr&0x7FF)) == lc1);	/* look for a scanline edge */
    start = GET_TIME();			/* record start time */
    for (ii=0; ii<16; ++ii) {		/* count 16 lines */
	while ((lc1 = (*lctr&0x7FF)) == lc2) ;
	lc2 = lc1;
    }
    line_time = GET_TIME();		/* record end time */
    prc_set_ipl(oldps);			/* interrupts are ok from here on */
#if HOST_BOARD == CHAMELEON
    line_time = ((line_time - start)+8)/16;
#else
    line_time = ((start - line_time)+8)/16;
#endif
    pll->ticks_field = field_time;
    pll->ticks_line = line_time;
    pll->lines = field_time/line_time;
    pll->vlines = *(VU32*)(SST_BASE+0x20C)>>16;
    if (VIS_V_PIX == 256) {
	pll->fields_sec = 57;
    }
    if (VIS_V_PIX == 384) {
	pll->fields_sec = 60;
    }
#if GALILEO_TIMER2
    *tctl = oldctl;
#endif
    while (  NVBLANK ) ;		/* wait for a VB to show up */
    while ( !NVBLANK ) ;		/* wait for VB to deassert */
#if CPU_SPEED_VARIABLE
    if (VIS_V_PIX == 256) {
	CPU_SPEED = (((prc_get_count()-CPU_SPEED)/1750+5)/10)*1000000;
    }
    if (VIS_V_PIX == 384) {
	CPU_SPEED = (((prc_get_count()-CPU_SPEED)/1667+5)/10)*1000000;
    }
    cpu_ticks_usec = CPU_SPEED/2000000;
#endif
    ii = pll->vlines+4;		/* assume our fake VB is about 4 lines after visible */
    if (ii > pll->lines) ii = pll->lines-1; /* but no further than max lines */
    while (1) {
	do {
	    lc1 = (*lctr&0x7FF);	/* get current scanline */
	    lc2 = (*lctr&0x7FF);
	} while (lc1 == lc2) ;
	if (lc1 > ii) break;
    }
    prc_timer_rate(field_time/16);	/* divide field into 16 equal parts */
    pll->field = 0;			/* start this timer at 0 */
    pll->ticks = 0;
    pll->nxt_upd = 0;
    pll->ints_upd = 4;
    pll->no_vsync = -1;			/* signal timer not to do vsyncs */
    pll->fields_frame = 0;		/* 1 field per frame, allow prc_delay to drop out */
    prc_set_vec(TIMERPLL_INTVEC, timer_pll);

#if defined(SST_VSYNC_ACK)
    SST_VSYNC_ACK();			/* ack any pending VB interrupts */
#endif
#if defined(VSYNC_IE_BIT)
    prc_set_vec(VSYNC_IVEC, sst_vb);
# if VSYNC_IE_BIT != VSYNC_CAUSE_BIT
    INTCTL_IE_T = (INTCTL_IE_T & ~(1<<VSYNC_CAUSE_BIT)) | (1<<VSYNC_IE_BIT);
# endif
    prc_delay(10);			/* wait a little */
    if (!pll->field) {			/* no vsync interrupts */
	U32 oldopt;
#if defined(INTCTL_IE_T)
	char emsg[AN_VIS_COL_MAX];
	INTCTL_IE_T &= ~(1<<VSYNC_IE_BIT); /* disable this in case it starts all by itself */
#else
	*(VU32*)INTCTL_IE &= ~(1<<VSYNC_IE_BIT); /* disable this in case it starts all by itself */
#endif
	prc_set_vec(VSYNC_IVEC, 0);	/* might as well zap this to save time */
	pll->no_vsync = 1;		/* tell timer to take over */
	pll->fields_frame = 1;		/* enable video */
	oldopt = prc_delay_options(PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR|PRC_DELAY_OPT_TEXT2FB);
	txt_str(-1, AN_VIS_ROW/2, "No VSYNC interrupts detected", RED_PAL);
#if defined(INTCTL_IE_T)
	nsprintf(emsg, sizeof(emsg), "INTMAP=0x%04X, INTCTL=0x%04X",
	    INTCTL_MAP_T, INTCTL_IE_T);
	txt_str(-1, AN_VIS_ROW/2+2, emsg, WHT_PAL);
#endif
	prc_delay(240);			/* show message for a little */
	vid_clear();			/* done with message */
	prc_delay_options(oldopt);
    } else {
	pll->no_vsync = 0;		/* let vsync do its thing */
    }
#else
    pll->no_vsync = 1;
#endif
#if 0 && (HOST_BOARD == CHAMELEON)
    BLABF(("\nsst_vid: Fields/sec: %d, ticks/field: %d, ticks/line: %d",
	pll->fields_sec, pll->ticks_field, pll->ticks_line));
    BLABF(("\nsst_vid: timer rate: %d, lines: %d, visible lines: %d",
	prc_timer_rate(0), pll->lines, pll->vlines));
#endif
    return;
}

static void sst_reset_hw(void) {
    static int been_here;
    if (been_here) {
	grGlideShutdown();
    }
#if HOST_BOARD == FLAGSTAFF || HOST_BOARD == SEATTLE 
    {
	extern void init_3dfx_pci(void);
	int oldps = prc_set_ipl(INTS_OFF);
	*(VU32*)RESET_CTL &= ~(1<<B_RESET_3DFX);	/* reset the 3dfx chip */
	prc_wait_n_usecs(500);			/* wait a little while */
	*(VU32*)RESET_CTL |= (1<<B_RESET_3DFX);	/* unreset 3dfx */
	init_3dfx_pci();
	*(VU32*)INTCTL_VSY_ACK = 0;			/* purge any pending VSYNC interrupts */
	prc_set_ipl(oldps);
    }
#endif
#if HOST_BOARD == CHAMELEON
    {
	extern void init_3dfx_pci(void);
	int id, oldps = prc_set_ipl(INTS_OFF);
	id = get_sst_device_number();
	if (id >= 3 && id <= 5) {			/* Can only be slot 0-2 */
	    BLABF(("\nsst_vid: reseting 3dfx board in slot %d...", id-3));
	    RESET_CTL_T &= ~(RESET_PCI_SLOT_0<<(id-3));	/* reset the 3dfx chip */
	    prc_wait_n_usecs(500);			/* wait a little while */
	    RESET_CTL_T |= (RESET_PCI_SLOT_0<<(id-3));	/* unreset 3dfx */
	    init_3dfx_pci();	
	    SST_VSYNC_ACK();			/* purge any pending VSYNC interrupts */
	} else {
	    BLABF(("\nsst_vid: Found 3dfx chip in invalid slot: %d...", id));
	}
	prc_set_ipl(oldps);
    }
#endif
    been_here = 1;
}

#ifndef AGC_GAMMA_BASE
# define AGC_GAMMA_BASE (28.0)
#endif
#ifndef AGC_GAMMA_RATIO
# define AGC_GAMMA_RATIO (0.67)
#endif

#if DYNAMIC_GAMMA_CORRECTION
extern float agcGammaBase, agcGammaRatio;
#endif

void sst_reset(void) {
    sst_reset_hw();
#if DYNAMIC_GAMMA_CORRECTION
    if (agcGammaBase == 0.0) agcGammaBase = AGC_GAMMA_BASE;
    if (agcGammaRatio == 0.0) {
	if ((IO_DIPSW_T^IO_DIPSW_INVERT)&IO_DIPSW10) {
	    agcGammaRatio = (256.0-AGC_GAMMA_BASE)/256.0;
	} else {
	    agcGammaRatio = AGC_GAMMA_RATIO;
	}
    }
#endif
    DEFAULT_RES_METHOD();
#if GLIDE_VERSION > 203
    {
	extern void _GlideInitEnvironment(void);
	PHX_PUTS("Doing _GlideInitEnvironment() ...\r\n");
	_GlideInitEnvironment();
	PHX_PUTS("Doing grResetTriStats() ...\r\n");
	grResetTriStats();
    }
#else
    PHX_PUTS("Doing grSstQueryHardware() ...\r\n");
    if ( !grSstQueryHardware(&hwconfig) )
    {
            grErrorCallback("main: grSstQueryHardware failed!", FXTRUE);
    }
#endif
    PHX_PUTS("Doing grSstSelect()\r\n");
    grSstSelect(0);

    PHX_PUTS("Doing grSstOpen()\r\n");
    if ( !grSstOpen(SST_RESOLUTION, SST_REFRESH_RATE, SST_COLOR_FORMAT,
                  SST_ORIGIN, SST_SMOOTHING, 2) )
    {
            grErrorCallback("main: grSstOpen failed!", FXTRUE);
    }

    /*
    ** G-shading, no texture mapping.
    */
    guColorCombineFunction( GR_COLORCOMBINE_ITRGB );
    grTexCombineFunction( GR_TMU0, GR_TEXTURECOMBINE_ZERO);

    PHX_PUTS("Finished the 3dfx init\r\n");

}

#if TEST_SST_RESET
#include <nsprintf.h>

static int test_sst_reset( const struct menu_d *smp ) {
    U32 stop_time, start_time;
    float fstart, fstop;
    char buf[AN_VIS_COL];

    stop_time = prc_get_count();
    grGlideShutdown();
    stop_time = prc_get_count() - stop_time;
    start_time = prc_get_count();
    sst_reset();
    start_time = prc_get_count() - start_time;
    fstop = (float)stop_time*2000.0/(float)CPU_SPEED;
    nsprintf(buf, sizeof(buf), "Stop time = %.1fms", fstop);
    txt_str(-1, AN_VIS_ROW/2, buf, WHT_PAL);
    fstart = (float)start_time*2000.0/(float)CPU_SPEED;
    nsprintf(buf, sizeof(buf), "Start time = %.1fms", fstart);
    txt_str(-1, AN_VIS_ROW/2+2, buf, WHT_PAL);
    ctl_read_sw(-1);
    while (!(ctl_read_sw(SW_NEXT|SW_ACTION)&(SW_NEXT|SW_ACTION))) prc_delay(0);
    return 0;
}
#endif

#if GLIDE_VERSION > 203
# if !NO_VIDEO_TIMING_STR && (DYNAMIC_VIS_PIX || VIS_V_PIX_MAX == 256)
const sst1VideoTimingStruct sst_t512x256 = {
    39,        /* hSyncOn */
    605,       /* hSyncOff */
    4,         /* vSyncOn */
    276,       /* vSyncOff */
    74,        /* hBackPorch */
    13,        /* vBackPorch */
    512,       /* xDimension */
    256,       /* yDimension */
    64,        /* memOffset */
    0x410,     /* memFifoEntries_1MB  ... 32256 entries in memory fifo (no Z) */
    0x100,     /* memFifoEntries_2MB  ... 57344 entries in memory fifo */
    0x100,     /* memFifoEntries_4MB  ... 57344 entries in memory fifo */
    8,         /* tilesInX_Over2 */
    23,        /* vFifoThreshold */
    FXFALSE,   /* video16BPPIsOK */
    FXTRUE,    /* video24BPPIsOK */
    20.66F,    /* clkFreq16bpp */
    20.66F     /* clkFreq24bpp */
};
# endif

# if !NO_VIDEO_TIMING_STR && (DYNAMIC_VIS_PIX || VIS_V_PIX_MAX == 384)
const sst1VideoTimingStruct sst_t512x384 = {
#  if DMS_TIMING
    63,        /* hSyncOn */
    575,       /* hSyncOff */
    5,         /* vSyncOn */
    411,       /* vSyncOff */
    32,        /* hBackPorch (s/b 49) */
    25,        /* vBackPorch */
    512,       /* xDimension */
    384,       /* yDimension */
    96,        /* memOffset */
    0x0,       /* memFifoEntries_1MB  ... 32256 entries in memory fifo (no Z) */
    0x100,     /* memFifoEntries_2MB  ... 57344 entries in memory fifo */
    0x100,     /* memFifoEntries_4MB  ... 57344 entries in memory fifo */
    8,         /* tilesInX_Over2 */
    23,        /* vFifoThreshold */
    FXTRUE,    /* video16BPPIsOK */
    FXTRUE,    /* video24BPPIsOK */
    16.0F,     /* clkFreq16bpp */
    32.0F      /* clkFreq24bpp */
#  else
    23,        /* hSyncOn */
    640,       /* hSyncOff */
    3,         /* vSyncOn */
    411,       /* vSyncOff */
#   if !SDMS_TIMING
    90,        /* hBackPorch */
#   else
    73,        /* hBackPorch */
#   endif
    24,        /* vBackPorch */
    512,       /* xDimension */
    384,       /* yDimension */
    96,        /* memOffset */
    0x410,     /* memFifoEntries_1MB  ... 32256 entries in memory fifo (no Z) */
    0x100,     /* memFifoEntries_2MB  ... 57344 entries in memory fifo */
    0x100,     /* memFifoEntries_4MB  ... 57344 entries in memory fifo */
    8,         /* tilesInX_Over2 */
    23,        /* vFifoThreshold */
    FXFALSE,   /* video16BPPIsOK */
    FXTRUE,    /* video24BPPIsOK */
    33.0F,     /* clkFreq16bpp */
    33.0F      /* clkFreq24bpp */
#  endif
};
# endif
#endif

void vid_init() {
    long options;

#if VID_WAIT_FOR_BUTTON
    while ( !(READ_RAW_SWITCHES(0)&(SW_NEXT|SW_ACTION)) ) { ; }
#endif

    sst_reset();

    sst_memfifo_min = 0xFFFF;

#if defined(EER_SST_OPT)
    options = eer_gets(EER_SST_OPT);
    if ( !options ) {
	options = sst_opt_memo;
	eer_puts(EER_SST_OPT,options);
    } else {
	sst_opt_memo = options;
    }
#else
    options = sst_opt_memo;
#endif

    sst_text_init(0, 0);		/* establish a fake screen */

    txt_select(TXT_VSCREEN);		/* default text drawing into fake screen */

    vid_clear();			/* clear the screen(s) */
    setancolors();

    PHX_PUTS("Timing V-blank signals\r\n");
    time_vb();

    vid_fields(1);			/* assume normal video mode */

    vid_warm = 1;
    PHX_PUTS("Exiting vid_init\r\n");
    prc_delay(10);			/* wait awhile to let switches get debounced */
}

#define MAX_WAIT (320000L)

#define U32S_PER_LINE (VIS_H_PIX)

static int bypass_enabled;

/*		bm_rect()
 *	Draw a rectangle in the currently _active_ (not visible, but
 *	being drawn into) frame buffer. 
 *	It is not "static"
 *	because some of the tests in other modules use it at the moment.
 */
int bm_rect( int x1, int y1, int x2, int y2, int outside, int inside) {
    int ul_x, ul_y, lr_x, lr_y;
    m_int y,x;
    int v_retrace;
    int next_line = TOT_H_PIX;
    FxU16 *ptr, *lp;

    /* Guard against inversion of upper-left and lower-right
     */ 
    ul_x = x1 >= 0 ? x1 : 0;
    lr_x = x2 >= 0 ? x2 : 0;
    if ( lr_x < ul_x ) {
	ul_x = x2;
	lr_x = x1;
    }
    ul_y = y1 >= 0 ? y1 : 0;
    lr_y = y2 >= 0 ? y2 : 0;
    if ( lr_y > ul_y ) {
	ul_y = y2;
	lr_y = y1;
    }
    /* Clip to within visible screen, being careful to remember that
     * y = 0 is _bottom_ of the screen.
     */
    if ( lr_x >= VIS_H_PIX ) lr_x = VIS_H_PIX-1;
    if ( ul_x >= VIS_H_PIX ) ul_x = VIS_H_PIX-1;

    if ( ul_y >= TOT_V_PIX ) ul_y = TOT_V_PIX-1;
    if ( lr_y >= TOT_V_PIX ) lr_y = TOT_V_PIX-1;

    v_retrace = (ul_y-lr_y)-1;

    /*	Duplicate colors so we don't care which 16-bit word
     *	we are trying to store.
     */
    outside &= 0x7FFF;
    outside |= (outside << 16);

    inside &= 0x7FFF;
    inside |= (inside << 16);

    if (!bypass_enabled) {
	grLfbBegin();
	grLfbBypassMode(GR_LFBBYPASS_ENABLE);
	grLfbWriteMode(GR_LFBWRITEMODE_555);
    }

    /*
    ** G-shading, no texture mapping.
    */
    guColorCombineFunction( GR_COLORCOMBINE_ITRGB );
    grTexCombineFunction( GR_TMU0, GR_TEXTURECOMBINE_ZERO);

/* OK, we have control and agree which buf we are talking to.
 * First draw inside
 */
    ptr = (FxU16 *)grLfbGetWritePtr( GR_BUFFER_BACKBUFFER );

    lp = ptr;
    lp += next_line*(lr_y+1);
    for ( y = ul_y-1 ; y > lr_y ; --y ) {
	for ( x = ul_x+1 ; x < lr_x ; ++x ) lp[x] = inside;
	lp += next_line;
    }

/* draw bottom */

    lp = ptr;
    lp += next_line*lr_y;
    for ( x = ul_x ; x <= lr_x ; ++x ) {
	lp[x] = outside;
    }

/* draw top */

    lp = ptr;
    lp += next_line*ul_y;
    for ( x = ul_x ; x <= lr_x ; ++x ) {
	lp[x] = outside;
    }

/* now the sides */

    lp = ptr;
    lp += next_line*(lr_y+1);
    for ( y = ul_y-1 ; y > lr_y ; --y ) {
	lp[ul_x] = outside;		/* write whichever half... */
	lp[lr_x] = outside;		/* write whichever half... */
	lp += next_line;
    }
    if (!bypass_enabled) {
	grLfbBypassMode(GR_LFBBYPASS_DISABLE);
	grLfbEnd();
    }
    return 0;
}

extern int (*dbg_str)(int col, int row, const char *string, int font);

void vid_clr_alphas(void) {
    m_int y;
    struct txt_alpha_ptr *alp;
    alp = txt_vsptr(0);
    if (alp) {
	if (dbg_str) {
	    for ( y = AN_VIS_ROW-1 ; y >= 0 ; --y ) {
/* egregious hack for remote-alphas selftest with ztv board */
		txt_clr_wid(0,y,AN_VIS_COL);
	    }
	} else {
	    memset((char *)alp->screen, 0, AN_VIS_ROW*AN_VIS_COL*2);
	}
    }
}

#if GUN_CNT
extern void gun_hide();
#endif

void vid_clear(void) {
    int which;

    which = txt_select(TXT_NONE);
    txt_select(which);
    vid_clr_alphas();
    grBufferClear( 0, 0, GR_WDEPTHVALUE_NEAREST );
    sst_bufswap( );
    grBufferClear( 0, 0, GR_WDEPTHVALUE_NEAREST );
#if GUN_CNT
    gun_hide();
#endif
    while ( SST_BUSY() ) { ; }
}

void stamp_atxy(int num, int x, int y, int color) {
    return;
}

#if (0)
/* MEA deleted lmul_div(), which was apparently a fossil, left over from
 * the _last_ time we needed to set a PLL-clock (ASAP Bitmap Board).
 */
/*	lmul_div(start,mul,div,mod_p) computes the function (start*mul)/div
 *	with enough precision in the intermediate result for exact results.
 *	If mod_p is non-null, the remainder is also returned.
 */
#ifndef ULONG_MAX
#define ULONG_MAX ((unsigned long)-1L)
#endif

unsigned long lmul_div(unsigned long start, unsigned long mul, unsigned long div, unsigned long *mod_p) {
    unsigned long acc_hi, acc_mid, acc_lo;
    unsigned int s_hi, s_lo, m_hi, m_lo;
    int bit;

    s_hi = (start >> 16) & 0xFFFF;
    s_lo = (start & 0xFFFF);
    m_hi = (mul >> 16) & 0xFFFF;
    m_lo = (mul & 0xFFFF);

    /*	First the easy partial products.
     */
    acc_lo = s_lo * m_lo;
    acc_hi = s_hi * m_hi;

    /*	Now the "middle" partials, accumulating as we go into lo and hi
     */
    acc_mid = s_hi * m_lo;

    acc_hi += (acc_mid >> 16);
    acc_mid &= 0xFFFF;

    acc_mid += (acc_lo >> 16);
    acc_lo &= 0xFFFF;
    acc_hi += (acc_mid >> 16);
    acc_lo |= (acc_mid << 16);

    acc_mid = s_lo * m_hi;

    acc_hi += (acc_mid >> 16);
    acc_mid &= 0xFFFF;

    acc_mid += (acc_lo >> 16);
    acc_lo &= 0xFFFF;
    acc_hi += (acc_mid >> 16);
    acc_lo |= (acc_mid << 16);

    if ( div <= acc_hi ) {
	return ULONG_MAX;
    }
    for ( bit = 32 ; bit ; --bit ) {
	int qbit = 0;
	acc_hi += acc_hi;
	if ( acc_lo & 0x80000000L ) ++acc_hi;
	if ( div <= acc_hi ) {
	    qbit = 1;
	    acc_hi -= div;
	}
	acc_lo = ((acc_lo << 1) + qbit) & 0xFFFFFFFFL;
    }
    if ( mod_p ) *mod_p = acc_hi;
    return acc_lo;
}
#endif

/*		SetANPal(f_and_b)
 *	Sets one AlphaNumeric palette to have the colors specified
 *	in f_and_b. The lower 16 bits of f_and_b specify the foreground
 *	color (in game's coding) while the upper 16 bits specify the
 *	background color. An alias color is synthesized "halfway between"
 *	the two.
 */
extern void SetANPal(int, U32);

void
setancolors()
{
    SetANPal(GRY_PAL,GRY_SLT);
    SetANPal(BLU_PAL,GRY_BLU);
    SetANPal(GRN_PAL,GRY_GRN);
    SetANPal(CYN_PAL,GRY_CYN);
    SetANPal(RED_PAL,GRY_RED);
    SetANPal(VIO_PAL,GRY_VIO);
    SetANPal(YEL_PAL,GRY_YEL);
    SetANPal(WHT_PAL,GRY_WHT);
#if GUN_CNT
    {
	extern void gun_check_pal_swap();
	gun_check_pal_swap();
    }
#endif
}

static const int col_bar_desc[] = {
    RED_MSK, GRN_MSK, BLU_MSK, ALL_MSK, 0
};

struct col_desc {
    const char * const text;
    unsigned long color;
};

/*	On ZTV2, we use the bitmap for purity. Later, we may merge this
 *	with ZTV1, which will make us less dependant on the ALPHA ROM
 */

static const struct col_desc purity_colors[] = {
    {  "Red"   , RED_MSK},
    { "Green"  , GRN_MSK},
    {  "Blue"  , BLU_MSK},
    { "BRIGHT" , (RED_MSK|GRN_MSK|BLU_MSK)},
    { "White"  ,  (((RED_MSK*13)>>4)&RED_MSK)
		| (((GRN_MSK*13)>>4)&GRN_MSK)
		| (((BLU_MSK*13)>>4)&BLU_MSK)},
    {"50% Grey",  ((RED_MSK+RED_LSB)>>1)
		| ((GRN_MSK+GRN_LSB)>>1)
		| ((BLU_MSK+BLU_LSB)>>1) },
    {  "Grey"  ,  (((RED_MSK*9)>>5)&RED_MSK)
		| (((GRN_MSK*9)>>5)&GRN_MSK)
		| (((BLU_MSK*9)>>5)&BLU_MSK)},
    {  "Ones"  , ALL_LSB},
    { "Black"  , 0}
};

#if GLIDE_VERSION >= 203
extern int AGCGamma(U32 *ptr, float r, float g, float b, float pcnt, float base);
# define GAMMA_R		1.4
# define GAMMA_G		1.4
# define GAMMA_B 		1.4
# define GAMMA_GRN_CORR		0.85
# if DYNAMIC_GAMMA_CORRECTION
#  define GAMMA_BASE		agcGammaBase
#  define GAMMA_RATIO		agcGammaRatio
# else
#  define GAMMA_BASE		AGC_GAMMA_BASE
#  define GAMMA_RATIO		AGC_GAMMA_RATIO
# endif
#endif

int purity( smp )
const struct menu_d *smp;
{
    U32 cntrls;
    m_int i,old_i, oldpd;
    U32 color;
    i = 0;
    old_i = -1;
    oldpd = prc_delay_options(0);
#if GLIDE_VERSION >= 203
    AGCGamma((U32*)SST_BASE, GAMMA_R, GAMMA_G, GAMMA_B, GAMMA_RATIO, GAMMA_BASE); /* change gamma for white tracking */
#endif
    while (1) {
	prc_delay(0);
	while ( SST_BUSY() ) { ; }
	cntrls = ctl_read_sw(SW_ACTION|SW_NEXT);
	if ( cntrls & SW_NEXT ) break;
	if ( cntrls & SW_ACTION ) {
	    if ( ++i >= n_elts(purity_colors)  ) {
		i = 0;
	    }
	}
	color = purity_colors[i].color;
	if ( old_i != i ) {
	    /*
	    ** G-shading, no texture mapping.
	    */
	    grBufferClear( 0, 0, GR_WDEPTHVALUE_NEAREST );	    
	    bm_rect(0, 0, VIS_H_PIX-1, VIS_V_PIX-1, color, color);
	    if ( old_i >= 0 ) {
		txt_clr_str(-1,2,purity_colors[old_i].text,MNORMAL_PAL);
	    }
	    txt_str(-1,2,purity_colors[i].text,WHT_PALB); 
	    st_insn(AN_VIS_ROW-4,"To change color,",t_msg_action,INSTR_PAL);
	    while ( SST_BUSY() ) { ; }
	    sst_text2fb(0);
	    sst_bufswap();
	    while ( SST_BUSY() ) { ; }
	    old_i = i;
	}
    }
#if GLIDE_VERSION >= 203
    AGCGamma((U32*)SST_BASE, GAMMA_R, GAMMA_G*GAMMA_GRN_CORR, GAMMA_B, GAMMA_RATIO, GAMMA_BASE); /* put it back */
#endif
    prc_delay_options(oldpd);
    return 0;
}

static	const struct col_desc converge_colors[] = {
    {" White  ", ALL_MSK },
    {" Violet ", VIO_MSK },
    {" Green  ", GRN_MSK }
};

int converge( smp )
const struct menu_d *smp;
{
    U32 cntrls;
    m_int i, old_i;
    U32 color;
    int oldpd;

    i = 0;
    old_i = -1;
    color = 0xFF;
    oldpd = prc_delay_options(0);	/* we'll do all the buffer swapping, etc */
    vid_clear();			/* clear the whole screen */

    while (1) {
	prc_delay(0);
	cntrls = ctl_read_sw(SW_ACTION|SW_NEXT);
	if ( cntrls & SW_NEXT ) break;
	if ( cntrls & SW_ACTION ) {
	    if ( ++i >= n_elts(converge_colors)  ) {
		i = 0;
	    }
	}
	if ( old_i != i ) {
	    int x, y;
	    VU16 *ptr;
	    VU16 *pixel;
	    grBufferClear( 0, 0, GR_WDEPTHVALUE_NEAREST );
	    if (!bypass_enabled) {
		grLfbBegin();
		grLfbBypassMode(GR_LFBBYPASS_ENABLE);
		grLfbWriteMode(GR_LFBWRITEMODE_555);
	    }

	    /*
	    ** G-shading, no texture mapping.
	    */
	    guColorCombineFunction( GR_COLORCOMBINE_ITRGB );
	    grTexCombineFunction( GR_TMU0, GR_TEXTURECOMBINE_ZERO);

	    ptr = (VU16*)grLfbGetWritePtr( GR_BUFFER_BACKBUFFER );
	    color = converge_colors[i].color;
	    color |= (color << 16);
	    for ( y = 0 ; y < VIS_V_PIX ; ++y ) {
		pixel = ptr + y*1024;
		for ( x = 0; x < VIS_H_PIX ; ++x ) {
		    if ( (x & 0x0F) == 0 || (y & 0x0F) == 0 || 
			( (x & 0x0F) == 8 && (y & 0x0F) == 8 ) )
			*pixel++ = color;
		    else *pixel++ = 0;
		}
	    }
	    if (!bypass_enabled) {
		grLfbBypassMode(GR_LFBBYPASS_DISABLE);
		grLfbEnd();
	    }
	    if ( old_i >= 0 ) {
		txt_clr_str(-1,2,converge_colors[old_i].text, WHT_PALB);
		txt_clr_wid(-1, 3, 8);
	    }
	    txt_str(-1,2,converge_colors[i].text,WHT_PALB);
	    txt_hexnum(-1, 3, color, 8, RJ_ZF, WHT_PALB); 
	    st_insn(AN_VIS_ROW-4,"To change color,",t_msg_action,INSTR_PAL);
	    while ( SST_BUSY() ) { ; }
	    sst_text2fb(0);
	    sst_bufswap( );
	    while ( SST_BUSY() ) { ; }
	    old_i = i;
	}
    }
    prc_delay_options(oldpd);
    return 0;
}

/*		bm_color_bars(mp, display)
 *	paints color bars into 3DFX bitmap via direct access.
 *	mp points to a table of masks for the bands of color, top to bottom.
 *	If display != 0, paints color bars on second display.
 *	returns x offset to left edge of color bars. This is a hack
 *	to speed drawing of color bars.
 */
STATIC const int block_y = 20;
#define BAND_HT (VIS_V_PIX-(block_y<<1))
STATIC
int bm_color_bars( const int *mp ) {
    m_int y,swath,band_y,band_x,max_col,n_bands,band_ht;
    int block_x,pix_per_band;
    unsigned long color,incr;
    int log_max,bits;
    int red_msk,grn_msk,blu_msk,red_shf,grn_shf,blu_shf;
    int next_line = TOT_H_PIX;
    
    const int *wmp;

    unsigned long *lp,*top;
    FxU32 *ptr;

    /* count how many bands we need, then figure the band height needed */
    wmp = mp;
    n_bands = 0;
    while ( *wmp ) { ++wmp, ++n_bands; }
    band_ht = BAND_HT/n_bands;

    /* first figure out max value of any gun. Some assumptions:
     *	The bits of a gun are contiguous and increase in significance
     * right-to-left
     *	The maximum value for a gun is all-ones, i.e. (xxx_MSK >> xxx_SHF)
     */
    max_col = (RED_MSK >> RED_SHF);
    if ( max_col < (GRN_MSK >> GRN_SHF) ) max_col = (GRN_MSK >> GRN_SHF);  
    if ( max_col < (BLU_MSK >> BLU_SHF) ) max_col = (BLU_MSK >> BLU_SHF);  

    for ( log_max = 0 ; max_col > (1<<log_max) ; ++log_max ) {;}

    /* Build a word with ones at the bottom of three equal-sized fields.
     * This will be used to increment all guns simultaneously.
     */
    incr = 1 | (1<<log_max) | (1<<(log_max<<1));

    /* Now figure out how much we need to shift each equal-sized field to
     * get it into the hardware location, and how to mask for only the
     * bits for each gun.
     */
    /* find how many bits of RED */
    red_msk = RED_MSK>>RED_SHF;
    for ( bits = 0 ; red_msk > (1<<bits) ; ++bits ) {;}
    red_shf = (log_max<<1)+(log_max-bits);
    red_msk <<= red_shf;

    grn_msk = GRN_MSK>>GRN_SHF;
    for ( bits = 0 ; grn_msk > (1<<bits) ; ++bits ) {;}
    grn_shf = (log_max)+(log_max-bits);
    grn_msk <<= grn_shf;

    blu_msk = BLU_MSK>>BLU_SHF;
    for ( bits = 0 ; blu_msk > (1<<bits) ; ++bits ) {;}
    blu_shf = (log_max-bits);
    blu_msk <<= blu_shf;

    if (!bypass_enabled) {
	grLfbBegin();
	grLfbBypassMode(GR_LFBBYPASS_ENABLE);
	grLfbWriteMode(GR_LFBWRITEMODE_555);
    }

    /*
    ** G-shading, no texture mapping.
    */
    guColorCombineFunction( GR_COLORCOMBINE_ITRGB );
    grTexCombineFunction( GR_TMU0, GR_TEXTURECOMBINE_ZERO);
    ptr = grLfbGetWritePtr( GR_BUFFER_BACKBUFFER );
    top = ptr;

    /* screen coordinates have (0,0) at bottom left */
    top += (VIS_V_PIX-block_y)*next_line/2;

    /* Figure out how many pixels we need to paint of each color to get
     * a "reasonable" block. This depends on the maximum available
     * resolution (max_col) and the space we have to paint it in.
     * to allow for some margin, we add 4 "colors" worth.
     */
    pix_per_band = VIS_H_PIX/(max_col+4);
    block_x = (VIS_H_PIX-(pix_per_band*(max_col+1)))/2;

    /* Now paint bands of pixels vertically down the screen, changing
     * the mask for each major horizontal section.
     */
    color = 0;
    band_x = block_x;
    for ( swath = 0 ; swath <= max_col ; swath += 1 ) {
	/* top of swath, reset mask pointer and bitmap pointer */
	wmp = mp;
	lp = top + band_x/2;
	band_y = band_ht;
	while (*wmp) {
	    /* one band of swath */
	    unsigned long hdw_col;
	    hdw_col  = ((color & red_msk) >> red_shf) << RED_SHF;
	    hdw_col |= ((color & grn_msk) >> grn_shf) << GRN_SHF;
	    hdw_col |= ((color & blu_msk) >> blu_shf) << BLU_SHF;
	    hdw_col &= *wmp;
	    hdw_col |= (hdw_col << 16);
	    for ( y = 0 ; y < band_y ; ++y ) {
		U32 *tp;
		int pix_idx;
		tp = lp;
		/* draw a rectangle <pix_per_band> wide by <band_ht> high */
		for ( pix_idx = 0 ; pix_idx < pix_per_band/2 ; ++pix_idx ) {    
		    *tp++ = hdw_col;
		}
		lp -= next_line/2;	/* remember, 0,0 is bottom left */
	    } /* end of drawing one vertical band of pixels */
	    ++wmp;	/* next band gets different mask */
	} /* end of swath */
	band_x += pix_per_band;
	color += incr;
    } /* end of swath */
    if (!bypass_enabled) {
	grLfbBypassMode(GR_LFBBYPASS_DISABLE);
	grLfbEnd();
    }
    return block_x;
}

STATIC int color_bars( const struct menu_d *smp ) {
    int xpos;
    int do_it, color, format, oldpd;
    U32 ctls;

    format = -1;
    do_it = 1;
    oldpd = prc_delay_options(0);
    while ( ((ctls = ctl_read_sw(SW_NEXT|SW_ACTION)) & SW_NEXT) == 0 ) {
	if ( ctls & SW_ACTION ) do_it = 1;
	if ( do_it ) {
	    while ( SST_BUSY() ) { ; }
	    grBufferClear( 0, 0, GR_WDEPTHVALUE_FARTHEST );
	    do_it = 0;
	    ++format;
	    if ( format & 2 ) color = 0;
	    else color = SLT_FUL;
	    xpos = bm_color_bars(&col_bar_desc[0]);
	    if ( xpos < 0 ) break;
	    bm_rect(0,0,VIS_H_PIX-1,block_y,color,color);
	    bm_rect(0,VIS_V_PIX-block_y,VIS_H_PIX-1,VIS_V_PIX-1,color,color);
	    bm_rect(0,block_y,xpos,VIS_V_PIX-block_y,color,color);
	    bm_rect(VIS_H_PIX-xpos,block_y,VIS_H_PIX-1,VIS_V_PIX-block_y,color,color);
	    while ( SST_BUSY() ) { ; }
	    sst_text2fb(0);
	    sst_bufswap( );
	    while ( SST_BUSY() ) { ; }
	}
	prc_delay(0);
    }
    prc_delay_options(oldpd);
    return 0;
}

STATIC int rectangles( smp )
const struct menu_d *smp;
{
    int which, redraw, oldpd;

    while ( SST_BUSY() ) { ; }
    grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );
    bm_rect(0,0,VIS_H_PIX-1,VIS_V_PIX-1,0x7FFF,0);
    sst_bufswap( );
    while ( SST_BUSY() ) { ; }
    grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );
    bm_rect(0,0,VIS_H_PIX-1,VIS_V_PIX-1,0x7FFF,0);
    redraw = 1;
    which = *(VU32*)SST_BASE&(1<<10);
    oldpd = prc_delay_options(0);
    while ( (ctl_read_sw(SW_NEXT) & SW_NEXT) == 0 ) {
	if ( ctl_read_sw(SW_ACTION) & SW_ACTION ) redraw = 1;
	if ( redraw ) {
	    /* Draw rectangle in "active" buffer then toggle
	     * buffers to display it.
	     */
	    int in,out;
	    redraw = 0;
	    out = RED_MSK;
	    if ( which ) {
		in = GRN_MSK;
		which = 0;
	    } else {
		in = BLU_MSK;
		which = 1;
	    }
	    bm_rect(8,8,VIS_H_PIX-9,VIS_V_PIX-9,out,in);
	    while ( SST_BUSY() ) { ; }
	    sst_text2fb(0);
	    sst_bufswap( );
	    while ( SST_BUSY() ) { ; }
	}
	prc_delay(0);
    }
    prc_delay_options(oldpd);
    return 0;
}

STATIC const struct menu_d mon_list[] = {
    { "MONITOR TESTS", 	0	},
    { "COLOR BARS", 	color_bars },
    { "CONVERGENCE",	converge},
    { "PURITY",	    	purity	},
    { "RECTANGLES",	rectangles },
#if TEST_SST_RESET
    { "RESET 3DFX SYSTEM", test_sst_reset },
#endif
    { 0, 0 }
};

int st_mon_group( smp )
const struct menu_d *smp;
{
    int sts;
    sts = st_menu(&mon_list[0],sizeof(mon_list[0]),RED_PALB,0);
    return sts;
}

int MS4Field;

extern void tq_maint(unsigned long);
 
int sst_options(const struct menu_d *smp) {
    unsigned long gopts;
#ifdef EER_SST_OPT
    gopts = eer_gets(EER_SST_OPT);
#else
    gopts = sst_opt_memo;
#endif
    gopts = DoOptions(sst_menu, gopts, SW_EXTRA);
#ifdef EER_SST_OPT
    eer_puts(EER_SST_OPT, gopts);
#endif
    sst_opt_memo = gopts;
    vid_init();
    return 0;
}

U32
sst_restore_defaults()
{
    unsigned long options;
    options = factory_setting(sst_menu);
    sst_opt_memo = options;
#ifdef EER_SST_OPT
    eer_puts(EER_SST_OPT,options);
#endif
    return options;
}

#define VB_MASK		(1<<6)
#define VB_DATA		(*(VU32*)(SST_BASE)&VB_MASK)
#define VB_ACT		0
#define VB_INACT	VB_MASK

int vid_waitvb(int edge) {
    extern volatile unsigned long eer_rtc;	/* timer calls eer_hwt()... */
    int old_rtc = eer_rtc;			/*  ... which changes eer_rtc */
    int count = TOO_LONG;

/*
 * If edge == 0, skip initial active state;
 * otherwise, skip initial inactive state.
 */

    int skip = (edge) ? VB_INACT : VB_ACT;

    while (old_rtc == eer_rtc && VB_DATA == skip) {
	count -= NCACHE_READ;
	if (count <= 0) return 0;
    }

    while (old_rtc == eer_rtc && VB_DATA != skip) {
	count -= NCACHE_READ;
	if (count <= 0) return 0;
    }

/* return 0 if timer did not execute */

    return ( (old_rtc == eer_rtc) ? 0 : 1 );
}
