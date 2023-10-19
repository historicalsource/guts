/*	gamedefs.c							*/

/*	>>->	define your switches to match the real controls to the
 *		internally used symbolic names of the switches. For
 *		example, if you use the RED BUTTON for SW_ACTION then
 *		you would define t_msg_action to be "Press RED BUTTON";
 */

#include <config.h>
#include <eer_defs.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>
#include <phx_proto.h>
#include <wms_proto.h>
#include <glide.h>
#include <nsprintf.h>
#include <nsc_idereg.h>
#include <string.h>
#include <pic_defs.h>

# define MSG_NEXT "P1-A or P2-A"
# define MSG_ACTION "P1-B or P2-B"
# define MSG_EXTRA "P1-C or P2-C"
# define MSG_CONTROL "Joystick"

	/* NEXT = RED, ACTION = BLUE, EXTRA = WHITE	*/
	/*	hold ACTION and press NEXT button			*/
	const char t_msg_clear[] =	"hold " MSG_ACTION " and press " MSG_NEXT;

	/*	press NEXT button					*/
	const char t_msg_next[] =	"press " MSG_NEXT;

	/*	press and hold NEXT button				*/
	const char t_msg_nexth[] =	"press and hold " MSG_NEXT;

	/*	press ACTION button					*/
	const char t_msg_action[] =	"press " MSG_ACTION;

	/*	press and hold ACTION button				*/
	const char t_msg_actionh[] =	"press and hold " MSG_ACTION;

	/*	press EXTRA button					*/
	const char t_msg_extra[] =	"press " MSG_EXTRA;

	/*	Use the control						*/

	const char t_msg_control[] =	"use " MSG_CONTROL;
	/* Choose then press ACTION button				*/
	const char t_msg_ret_menu[] =	"To return to menu,";
	const char t_msg_save_ret[] =	"To SAVE setting and exit,";
	const char t_msg_restore[] =	"To RESTORE old Setting,";


/*	>>->	Declare any functions that will be called
		from the SELFTEST main menu.  Make sure you add any
		files that contain custom selftest routines to the list
		in the makefile!
 */
extern	int	audio_test( const struct menu_d *);
extern	int	adj_vol( const struct menu_d *);
extern	int	snd_test( const struct menu_d *);
extern	int	CoinOptions(const struct menu_d *);
extern	int	SwitchTest(const struct menu_d *);
extern	int	st_mon_group(const struct menu_d *);
extern	int	st_text_group(const struct menu_d *);
extern	int	RAM_group(const struct menu_d *);
extern	int	ROMTest(const struct menu_d *);
extern 	int	ANTest(const struct menu_d *);
extern 	int	AN_stamp_test(const struct menu_d *);
extern 	int	vid_group(const struct menu_d *);
extern	int	scope_loops(const struct menu_d *);
extern	int	disp_toggle(const struct menu_d *);
extern	int	time_test(const struct menu_d *);
extern	int	cit_on_off(const struct menu_d *);
extern	int	ide_test(const struct menu_d *);
extern	int	wb_test(const struct menu_d *);
extern	int	rtc_set_clock(const struct menu_d *);
extern	int	rand(void);

#if TIME_EXCEPTIONS
extern U32 exception_time[2];
#endif

#if 0
static int
Statistics(smp)
const struct menu_d *smp;
{
    eer_stats(1);	/* erase ok */
    return 0;
}
#endif

static const unsigned char fake_menu[] =
/* The default hex switch menu	*/
"\344MSD X.......\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\304    .X......\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\244    ..X.....\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\204    ...X....\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\144    ....X...\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\104    .....X..\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\044    ......X.\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000\
\004LSD .......X\000\
0\0001\0002\0003\0004\0005\0006\0007\000\
8\0009\000A\000B\000C\000D\000E\000F\000";

#if 0
static int GameOptions(const struct menu_d *smp) {
    eer_opt(pbase->p_optmenu);
    return 0;
}
#endif

#ifdef EER_GUTS_OPT
static const unsigned char gut_menu[] =
    "\021Debug Switches\000Disabled\000*Enabled\000"
    "\041Development\000No\000*Yes\000"
#ifdef GUTS_OPT_AUD_PANIC
    "\051Audio Panic\000*No\000Yes\000"
#endif
#ifdef GUTS_OPT_PM_WDOG
    "\071PM dump WDOG resets\000Enabled\000*Disabled\000"
#endif
;

#if 0
static int
guts_options(smp)
const struct menu_d *smp;
{
    unsigned long gopts;
    gopts = eer_gets(EER_GUTS_OPT)|(debug_mode&GUTS_OPT_DEVEL);
    gopts = DoOptions(gut_menu,gopts,SW_EXTRA);
    debug_mode = gopts;
    eer_puts(EER_GUTS_OPT,gopts & ~GUTS_OPT_DEVEL);
    return 0;
}
#endif
#endif

#if 0
static int test_3dfx(const struct menu_d *smp) {
    extern void grBanner(void);
    ExitInst(INSTR_PAL);
    while (1) {
	grBanner();
	if (ctl_read_sw(SW_NEXT|SW_ACTION)&(SW_NEXT|SW_ACTION)) break;
	prc_delay(0);
    }
    ctl_read_sw(-1);
    return 0;
}
#endif

static int pic_getsn(const struct menu_d *smp) {
    char **buf;
    int l1, l2, pal;

    buf = GetMFG();
    l1 = strlen(buf[ MFG_SERIAL ]);
    l2 = strlen(buf[ MFG_TYPE ]);
    if (l2 > l1) l1 = l2;
    l2 = strlen(buf[ MFG_DATE ]);
    if (l2 > l1) l1 = l2;
    l2 = (AN_VIS_COL-l1-8)/2;
    l1 = AN_VIS_ROW/2;
    txt_str(l2, l1, "  Date: ", WHT_PAL);
    pal = GRN_PAL;
    if (strncmp(buf[ MFG_TYPE ], "Cann", 4) == 0) pal = RED_PAL;
    txt_cstr(buf[MFG_DATE], pal);
    l1 += 2;
    txt_str(l2, l1, "  Type: ", WHT_PAL);
    txt_cstr(buf[MFG_TYPE], pal);
    l1 += 2;
    txt_str(l2, l1, "Serial: ", WHT_PAL);
    txt_cstr(buf[MFG_SERIAL], pal);
    ExitInst(INSTR_PAL);
    while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) { prc_delay(0); }
    return 0;
}

#define SHOW_ADDR 1
#define SHOW_DATA 2

static int pic_nvram(const struct menu_d *smp) {
    int addr, data, what=0;
    U8 ans=0;
    char *msg=0;

    txt_str(-1, AN_VIS_ROW/2, "This will take awhile", WHT_PAL);
    prc_delay(0);
    do {
	for (addr=8, data=1; data < 256; data += data) {
	    if (WritePICNVRAM(addr, data) != SUCCESS) {
		msg = "Failed to write to PIC, NVRAM address: ";
		what = SHOW_ADDR;
		break;
	    }
	    if (ReadPICNVRAM(addr, &ans) != SUCCESS) {
		msg = "Failed to read from PIC, NVRAM address: ";
		what = SHOW_ADDR;
		break;
	    }
	    if (ans != data) {
		msg = "Walking 1 bit error, NVRAM address: ";
		what = SHOW_DATA|SHOW_ADDR;
		break;
	    }
	}
	if (what) break;
	for (addr=8; addr < 64; ++addr) {
	    if (WritePICNVRAM(addr, addr) != SUCCESS) {
		msg = "Failed to write to PIC, NVRAM address: ";
		what = SHOW_ADDR;
		break;
	    }
	}
	if (what) break;
	for (addr=8; addr < 64; ++addr) {
	    if (ReadPICNVRAM(addr, &ans) != SUCCESS) {
		msg = "Failed to read from PIC, NVRAM address: ";
		what = SHOW_ADDR;
		break;
	    }
	    if (ans != addr) {
		msg = "Addr into location error, NVRAM address: ";
		what = SHOW_DATA|SHOW_ADDR;
		data = addr;
		break;
	    }
	}
    } while (0);
    txt_clr_wid(2, AN_VIS_ROW/2, AN_VIS_COL-4);
    if (!what) {
	txt_str(-1, AN_VIS_ROW/2, "PASSED", GRN_PAL);
    }
    if ((what&SHOW_ADDR)) {
	txt_clr_wid(2, AN_VIS_ROW/2, AN_VIS_COL-4);
	txt_str(2, AN_VIS_ROW/2, msg, WHT_PAL);
	txt_chexnum(addr, 2, RJ_ZF, RED_PAL);
    }
    if ((what&SHOW_DATA)) {
	txt_str(2, AN_VIS_ROW/2+2, "Expected data: ", WHT_PAL);
	txt_chexnum(data, 2, RJ_ZF, GRN_PAL);
	txt_str(2, AN_VIS_ROW/2+3, "  Actual data: ", WHT_PAL);
	txt_chexnum(ans, 2, RJ_ZF, RED_PAL);
	txt_str(2, AN_VIS_ROW/2+4, "Bits in error: ", WHT_PAL);
	txt_chexnum(ans^data, 2, RJ_ZF, YEL_PAL);
    }
    ExitInst(INSTR_PAL);
    while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) { prc_delay(0); }
    return 0;
}

const struct menu_d picMenu[] =
{
    {	"PIC TESTS",		0		},
    {	"\nGET SERIAL NUMBER",	pic_getsn	},
    {	"\nPIC N/V RAM TEST",	pic_nvram	},
    {	"\nSET TIME and DATE",	rtc_set_clock	},
    { 0, 0 }
};

static int pic_tests(const struct menu_d *smp) {
    int sts;
    ctl_read_sw(-1);		/* zap edges */
    sts = st_menu(picMenu, sizeof(picMenu[0]), MNORMAL_PAL, 0);
    ctl_read_sw(-1);
    return sts;
}

static int check_wdog(const struct menu_d *smp) {
    U32 timer;
    int oldps;
    int row = AN_VIS_ROW/2;

    txt_str(-1, row++, "This test will wait for WDOG to timeout. If WDOG works", WHT_PAL);
    txt_str(-1, row++, "the game will reset after a couple of seconds.", WHT_PAL);
    prc_delay(120);
    row += 3;
    txt_str(-1, row, "Starting the test now...", WHT_PAL); 
    prc_delay(0);
    oldps = prc_set_ipl(INTS_OFF);
    timer = prc_get_count();
    while (1) {
        U32 t;
        t = prc_get_count() - timer;
        if (t > (CPU_SPEED/2)*5) break; /* WDOG didn't work */
    }
    prc_set_ipl(oldps);

    txt_clr_wid(1, row, AN_VIS_COL-2);
    row -= 3;
    txt_clr_wid(1, --row, AN_VIS_COL-2);
    txt_clr_wid(1, --row, AN_VIS_COL-2);
    txt_str(-1, AN_VIS_ROW/2, "FAILED", RED_PAL|AN_BIG_SET);

    ExitInst(INSTR_PAL);
    prc_delay(30);
    while (1) { if ((ctl_read_sw(0)&SW_NEXT)) break; prc_delay(0); }
    return 0;
}

static int batt_msg(int onoff) {
    char *msg = "Battery needs to be Replaced";
    if (onoff) {
	txt_str(-1, AN_VIS_ROW-7, msg, RED_PAL);
    } else {
	txt_clr_str(-1, AN_VIS_ROW-7, msg, RED_PAL);
    }
    return onoff;
}

static int menu_clock(const struct menu_d *smp)
{
    static unsigned long seconds;
    static int msg_up;
    unsigned long new_frame;

    if ( smp == 0 ) return 0;
    new_frame = eer_rtc;
    if ((new_frame&15) == 0) {
	if ((*(VU32*)GP_STS&(1<<B_GPSTS_BLOW))) {
	    msg_up = batt_msg((new_frame&16));
	} else if (msg_up) {
	    msg_up = batt_msg(0);
	}
    }
    new_frame /= 60;
    if ( new_frame == seconds ) return 0;
    txt_decnum(-1,2,seconds=new_frame,4,RJ_BF,MNORMAL_PAL);
    return 0;
}

int go_to_self_test;

/*	>>->	Build your main SELFTEST menu here.  Add or delete
 *		any tests you require for your game.
 *
 *	SPECIAL codes at the beginning of the label string:
 *	 "\n" indicates skip an extra line before this item
 *	and for linked games (such as SPACE)
 *	 "\r" indicate skip this item if (DOUBLE == 0 || RIGHT != 0)
 */
const struct menu_d	mainMenu[] =
{
    {	"SA DIAGS",		menu_clock	},
    {	"\nJAMMA SWITCH TEST",	SwitchTest	},
    {	"\nWIDGET + LMA TESTS", wb_test		},
    {	"\nSOUND TESTS",	snd_test	},
    {	"\nMEMORY TESTS",	RAM_group	},
    {	"\nPIC TESTS",		pic_tests	},
    {	"\nMONITOR TESTS",	st_mon_group	},
    {	"\nDISK TESTS",		ide_test	},
    {	"\nWDOG TEST",		check_wdog	},
    {	"\nSCOPE LOOPS",	scope_loops	},
    {0,0}
};

extern U32 bss_end[];
extern void boot_from_disk(U8*);

#if 0
#include <qio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

extern qio_uart_init(void);

static void report_error(const char *prefix, int sts) {
    char emsg[AN_VIS_COL];
    qio_errmsg(sts, emsg, sizeof(emsg));
    txt_str(-1, AN_VIS_ROW/2, prefix, WHT_PAL);
    txt_str(-1, AN_VIS_ROW/2+2, emsg, RED_PAL);
    while (1) prc_delay(0);
}

static void test_uart(void) {
    QioIOQ *rioq, *wioq, *tioq;
    int sts;
    FILE *in, *out, *err;

    ctl_read_sw(-1);
    txt_str(-1, AN_VIS_ROW-2, "Press GREEN or BLACK to continue", INSTR_PAL);
    while (!(ctl_read_sw(SW_NEXT|SW_ACTION)&(SW_NEXT|SW_ACTION))) prc_delay(0);
    txt_clr_wid(0, AN_VIS_ROW-2, AN_VIS_COL);
    prc_delay(0);

    qio_uart_init();
    rioq = qio_getioq();
    wioq = qio_getioq();
    tioq = qio_getioq();

    sts = qio_open(rioq, "/tty0", O_RDONLY);
    while (!sts) { sts = rioq->iostatus; }
    if (QIO_ERR_CODE(sts)) {
	report_error("Error opening /tty0 for read", sts);
    }	
    sts = qio_open(wioq, "/tty0", O_WRONLY);
    while (!sts) { sts = rioq->iostatus; }
    if (QIO_ERR_CODE(sts)) {
	report_error("Error opening /tty0 for write", sts);
    }	

    tioq->file = 0;
    sts = qio_close(tioq);
    while (!sts) { sts = tioq->iostatus; }
    if (QIO_ERR_CODE(sts)) {
	report_error("Error closing fd 0", sts);
    }	
    in = freopen("/tty0", "r", stdin);
    if (!in) {
	report_error("Error reopening stdin", errno);
    }    
    tioq->file = 1;
    sts = qio_close(tioq);
    while (!sts) { sts = tioq->iostatus; }
    if (QIO_ERR_CODE(sts)) {
	report_error("Error closing fd 1", sts);
    }	
    out = freopen("/tty0", "w", stdout);
    if (!out) {
	report_error("Error reopening stdout", errno);
    }    
    tioq->file = 2;
    sts = qio_close(tioq);
    while (!sts) { sts = tioq->iostatus; }
    if (QIO_ERR_CODE(sts)) {
	report_error("Error closing fd 2", sts);
    }	
    qio_freeioq(tioq);
    err = freopen("/tty0", "w", stderr);
    if (!err) {
	report_error("Error reopening stderr", errno);
    }    
    err->_flags |= __SNBF;		/* stderr is unbuffered */

#define MSG1 "This is a test of a write to stdout\r\n"
    sts = fwrite(MSG1, 1, sizeof(MSG1)-1, stdout);
    if (sts != sizeof(MSG1)-1) {
	report_error("Error fwriting", errno);
    }
#define MSG2 "This is a test of a write to stderr\r\n"
    fwrite(MSG2, 1, sizeof(MSG2)-1, stderr);
    if (sts != sizeof(MSG2)-1) {
	report_error("Error fwriting", errno);
    }
    printf("This is a test printf to stdout: %s\r\n", "def");
    fprintf(stderr,"This is a test fprintf to stderr: %s\r\n", "abc");
    printf("Stdin uses fd %d, stdout uses %d and stderr uses %d\r\n",
    	stdin->_file, stdout->_file, stderr->_file);

    while (1) {
	char inp[3];
	sts = qio_read(rioq, inp, 1);
	while (!sts) { sts = rioq->iostatus; }
	if (QIO_ERR_CODE(sts)) {
	    report_error("Error reading /tty0", sts);
	}	
	txt_clr_wid(0, AN_VIS_ROW/2+4, AN_VIS_COL);
	txt_clr_wid(0, AN_VIS_ROW/2+6, AN_VIS_COL);
	txt_str((AN_VIS_COL-15)/2, AN_VIS_ROW/2+4, "Read ", WHT_PAL);
	txt_cdecnum(rioq->iocount, 4, RJ_BF, GRN_PAL);
	txt_cstr(" bytes", WHT_PAL);
	inp[rioq->iocount] = 0;
	txt_str((AN_VIS_COL-rioq->iocount-2)/2, AN_VIS_ROW/2+6, "\"", WHT_PAL);
	txt_cstr(isprint(inp[0]) ? inp : ".", GRN_PAL);
	txt_cstr("\"", WHT_PAL);
	prc_delay(0);
	if (inp[0] == '\r') {
	    inp[1] = '\n';
	    inp[2] = 0;
	    rioq->iocount = 2;
	}
	sts = qio_write(wioq, inp, rioq->iocount);
	while (!sts) { sts = wioq->iostatus; }
	if (QIO_ERR_CODE(sts)) {
	    report_error("Error writing /tty0", sts);
	}	
    }
}	
#endif

static void dummy_main(void)
{

    prc_delay_options(PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR);

    vid_clear();

#if 0
    test_uart();
#endif

    txt_str(-1, AN_VIS_ROW/2, "No Game", RED_PAL|AN_BIG_SET);
    while (1) prc_delay(0);
}

/* >>->	If your self test needs some sort of data (e.g. anim tables), here
 * is a reasonable place to put it. For Beathead, we needed a set of pictures
 * in RLE compressed form, and a set of palettes to use for the pictures.
 */

/* >>->	Now specify the DEFAULT configuration block you want to use	*/
/*	for this game							*/

const struct pconfigb guts_pbase =  {
    dummy_main,		/* RESET vector	 for non-game	*/
    0,			/* no game option menu (stats.c will use fakemenu) */
    0,			/* no coin option menu (stats.c will use coinmenu) */
    "",			/* MAIN linktime	*/
    0,			/* trap type 0:STOP +:68K -:68010  */
    NO_ROM_TEST+NO_RAM_TEST+NO_LOG_RESET	/* debug options */
#ifdef GAME_SPECIFIC
	,(int(*)())no_op
#endif
};

void
vcr(vcr_on_flag)		
U16 vcr_on_flag;
{
#ifdef LM_VCR_REC
    prc_mod_latch(vcr_on_flag ? LM_VCR_REC : ~LM_VCR_REC);
#endif
#ifdef CTL_VCR
    MISC_LATCHES[CTL_VCR] = !!vcr_on_flag;
#endif
}


/*
 *	default_values()
 *
 * >>->	Now the routine that specifies any non-zero DEFAULT values for
 *	information stored in EEPROM (or NVRAM). 
 *
 *	Only non-zero defaults need be set as the routine that
 *	calls this totally clears all DEF_STAT(stat_map.mac)
 *	defined values in EEROM shadow ram.
 *
 */
void
default_values()
{
#if 0
    const unsigned char *menu;
    txt_str(-1,(AN_VIS_ROW/2)-1,"SETTING EEPROM DEFAULT VALUES",ERROR_PAL);
    if ( (menu = pbase->p_optmenu) != 0 ) {
	/* if game spec's a menu, use the *'d defaults */
	eer_puts(EER_GMOPT,factory_setting(menu));
    }
    /* Also Restore coin defaults.
     */
    if ( (menu = pbase->p_coinmenu) != 0 ) {
	eer_puts(EER_CNOPT,factory_setting(menu));
    }
#endif
#ifdef EER_GUTS_OPT
    eer_puts(EER_GUTS_OPT,factory_setting(gut_menu));
#endif
#ifdef EER_AUD_VOL
#ifndef DEF_VOLUME
# define DEF_VOLUME ((127<<8)|127)
#endif
    eer_puts(EER_AUD_VOL,DEF_VOLUME);
#endif
#if 0
    prc_delay(180);
    txt_clr_str(-1,(AN_VIS_ROW/2)-1,"SETTING EEPROM DEFAULT VALUES",ERROR_PAL);
#endif
}

int cn_init(void) {
    return 0;
}

void cn_irq(void) {
    return;
}

void pm_dump(void) {
    return;
}
