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
#include <nsprintf.h>
#include <string.h>
#include <optmenu.h>
#include <options.h>
#include <wnapi.h>


# define MSG_NEXT "REVERSE"
# define MSG_ACTION "ABORT"
# define MSG_EXTRA "TEST"
# define MSG_CONTROL "MUSIC and VIEW BUTTONS"

	/*	hold ACTION and press NEXT button			*/
	const char t_msg_clear[] =	"Hold " MSG_ACTION " and press " MSG_NEXT;

	/*	press NEXT button					*/
	const char t_msg_next[] =	"Press " MSG_NEXT;

	/*	press and hold NEXT button				*/
	const char t_msg_nexth[] =	"Press and hold " MSG_NEXT;

	/*	press ACTION button					*/
	const char t_msg_action[] =	"Press " MSG_ACTION;

	/*	press and hold ACTION button				*/
	const char t_msg_actionh[] =	"Press and hold " MSG_ACTION;

	/*	press EXTRA button					*/
	const char t_msg_extra[] =	"Press " MSG_EXTRA;

	/*	press and hold EXTRA button					*/
	const char t_msg_extrah[] =	"Press and hold " MSG_EXTRA;

	/*	Use the control						*/

	const char t_msg_control[] =	"Use " MSG_CONTROL;
	/* Choose then press ACTION button				*/
	const char t_msg_ret_menu[] =	"To return to menu,";
	const char t_msg_save_ret[] =	"To SAVE setting and exit,";
	const char t_msg_restore[] =	"To RESTORE old Setting,";


/*	>>->	Declare any functions that will be called
		from the SELFTEST main menu.  Make sure you add any
		files that contain custom selftest routines to the list
		in the makefile!
 */
extern	int	adj_vol( const struct menu_d *);
extern	int	cn_config(const struct menu_d *);
extern	int	SwitchTest(const struct menu_d *);
extern	int	RAM_group(const struct menu_d *);
extern	int	quad_tst( const struct menu_d *);
extern	int	snd_test( const struct menu_d *);
extern	int	scope_loops(const struct menu_d *);
extern	int	time_test(const struct menu_d *);
extern	int	ide_test(const struct menu_d *);
extern	int	net_test(const struct menu_d *);
extern	int	rtc_set_clock( const struct menu_d *);
extern	int	st_mon_group(const struct menu_d *);

static int
Statistics(smp)
const struct menu_d *smp;
{
    eer_stats(1);	/* erase ok */
    return 0;
}


static int bitcount( unsigned int x )
{
 int b;

 for ( b = 0; x != 0; x >>= 1 )
  if ( x & 1 ) b++;

 return b;
}

static const unsigned char *game_ptrs[2];
#define MAX_LINKS 8

const unsigned char **GUTS_finds_car_color( void )
{
 int node = ((eer_gets( EER_GMOPT ) & GO_COLOR) >> GOS_COLOR) % MAX_LINKS;
 unsigned char token = bitcount(GO_COLOR) | (GOS_COLOR << 3);
 const unsigned char *str = gamemenu;
 while ( *str != token ) str += 1 + strlen( (char *)str );
 game_ptrs[0] = str + 1;
 do str += 1 + strlen( (char *)str ); while ( node-- );
 if ( *str == '*' ) ++str;
 game_ptrs[1] = str;
 return game_ptrs;
}

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

#define FACTORY_SETTING	(1<<31)

static void st_options(const unsigned char *menu, int eer_opt) {
    U32 old_options,new_options;

    if (!menu) menu = fake_menu;
    old_options = eer_gets(eer_opt);
    new_options = DoOptions(menu, old_options, 0);
    if ( new_options == old_options ) return;
    if ( (new_options&FACTORY_SETTING) ) {
	new_options &= ~FACTORY_SETTING;
	eer_puts(eer_opt, factory_setting(menu));
    } else {
	eer_puts(eer_opt, new_options);
    }
    return;
}

static int GameOptions(const struct menu_d *smp) {
    st_options(pbase->p_optmenu, EER_GMOPT);
    return 0;
}

static int TrackOptions(const struct menu_d *smp) {
    st_options(pbase->p_trkmenu, EER_TRKOPT);
    return 0;
}

static int LapOptions(const struct menu_d *smp) {
    st_options(pbase->p_lapmenu, EER_LAPOPT);
    return 0;
}

static int TournOptions(const struct menu_d *smp) {
    st_options(pbase->p_trnmenu, EER_TRNOPT);
    return 0;
}

#ifdef EER_GUTS_OPT
static const unsigned char gut_menu[] =
#if (AUD_AV_MSK==3) && (AUD_AV_SHF==0)
/* Normal hiding place for attract volume ratio is bottom two bits */
    "\002Attract Audio\000Mute\0001/3\000*2/3\000Full\000"
#endif
    "\021Debug Switches\000*Disabled\000Enabled\000"
    "\041Development\000*No\000Yes\000"
#ifdef GUTS_OPT_AUD_PANIC
    "\051Audio Panic\000*No\000Yes\000"
#endif
#ifdef GUTS_OPT_PM_WDOG
    "\071PM dump WDOG resets\000Enabled\000*Disabled\000"
#endif
;

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
 extern void rtc_display(int col, int row, int pal);
 static int msg_up;
 if ( smp == 0 )
 {
  int col = AN_VIS_COL / 2;
  col -= 2;
  col -= strlen( (char *)GUTS_finds_car_color()[0]);
  col -= strlen( (char *)GUTS_finds_car_color()[1]);
  txt_str((AN_VIS_COL>>1)-17,2,"Entered:  ",WHT_PAL);
  rtc_display((AN_VIS_COL>>1)-7, 2, WHT_PAL);
  txt_str((AN_VIS_COL>>1)-10,AN_VIS_ROW-1,"SERIAL#:  ",VERSION_PAL|BGBIT);
  txt_cstr(GetMFG()[MFG_TYPE], VERSION_PAL|BGBIT);
  txt_cstr(GetMFG()[MFG_SERIAL], VERSION_PAL|BGBIT);
  txt_str( col, AN_VIS_ROW-12, (char *)GUTS_finds_car_color()[0], VERSION_PAL|AN_BIG_SET );
  txt_cstr( "  ", VERSION_PAL|AN_BIG_SET );
  txt_cstr( (char *)GUTS_finds_car_color()[1], VERSION_PAL|AN_BIG_SET );
 }
 if ((eer_rtc&15) == 0)
 {
  if ((*(VU32*)GP_STS&(1<<B_GPSTS_BLOW)))
  {
   msg_up = batt_msg((eer_rtc&16));
  }
  else if (msg_up)
  {
   msg_up = batt_msg(0);
  }
 }
 return 0;
}

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
    {	"SELFTEST",		menu_clock	},
    {	"ADJUST VOLUME",	adj_vol		},
    {	"?\nDEBUG OPTIONS",	guts_options	},
    {	"\nSTATISTICS",		Statistics	},
    {	"GAME OPTIONS",		GameOptions	},
    {	"GAME DIFFICULTY",	TrackOptions	},
    {	"NUMBER OF LAPS",	LapOptions	},
    {	"TOURNAMENT OPTIONS",	TournOptions	},
    {	"COIN OPTIONS",		cn_config	},
    {	"\nCONTROLS TEST",	SwitchTest	},
    {	"\nMEMORY TESTS",	RAM_group	},
    {	"\nSOUND TEST",		snd_test	},
    {	"?\nSCOPE LOOPS",	scope_loops	},
    {	"\nDISK TEST",		ide_test	},
    {	"\nNETWORK TESTS",	net_test	},
    {	"\nSET CLOCK",		rtc_set_clock	},
    {	"\nMONITOR TESTS",	st_mon_group	},
    {	"\nEXIT TO GAME",	exit_to_game	},
    {0,0}
};

#if 0
/* >>->	You may want to get more clever with what your OS shows when no
 * game is loaded and you flip out of self test.
 */
extern void erc_vbex();
static VU32 frame;
static int prev_scan;

static void dummy_gamevb()
{
    TimerPll *pll;
    pll = get_timer_pll();
    prev_scan = pll->prev_scan;
#ifdef WDOG
    WDOG = 0;
#endif
    erc_vbex();
    ++frame;
#if defined(TEST) && B_TEST >= 0
    if ((TEST&(1<<B_TEST)) == 0) prc_reboot();
#endif
}
#endif


#define MAC_OFFS 0
#define IP_OFFS  6
#define STR_OFFS 10

static void dummy_main(void)
{
 extern wnError wnInitLink ( void );
 wnError init,que,isr,link;
    U8  oper;
    U8  data[MAX_HW_DATA_SIZE];
    U32 size;

    prc_delay_options(PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR);

    vid_clear();

    txt_str(3,3,"Wavenet Init - ",WHT_PAL);

    init = que = isr = link = 0;
    if ( ( init = wnInitNSS() ) != WN_OK )
    {
     while(1) prc_delay0();
    }
    if ( ( init = wnInitNSS() ) != WN_OK )
    {
     txt_cstr("init",RED_PAL);
     while(1) prc_delay0();
    }
    if ( ( que = wnInitServiceQueues() ) != WN_OK )
    {
     while(1) prc_delay0();
    }
    if ( ( isr = wnInstallISR() ) != WN_OK )
    {
     while(1) prc_delay0();
    }
    if ( ( link =  wnInitLink() ) != WN_OK )
    {
     while(1) prc_delay0();
    }

    if ( ( que = wnInitServiceQueues() ) != WN_OK )
    {
     txt_cstr("que",RED_PAL);
     while(1) prc_delay0();
    }
    if ( ( isr = wnInstallISR() ) != WN_OK )
    {
     txt_cstr("isr",RED_PAL);
     while(1) prc_delay0();
    }
    if ( ( link =  wnInitLink() ) != WN_OK )
    {
     txt_cstr("link",RED_PAL);
     while(1) prc_delay0();
    }

#if 0
    if ( WN_InitWaveNetService() == WNTrue )
#endif
    {
     txt_cstr("Passed",GRN_PAL);
     WN_SendDiagMessage(N_DIAG_NSS_REQ);
     while ( WN_RecvDiagMessage( &oper, data, &size ) ==  WNFalse ) prc_delay0();

/* PGG START	Undefined for now... */
#if 0

     txt_printf( 3, 5, WHT_PAL, " Address:  %02x:%02x:%02x:%02x:%02x:%02x", \
	data[ MAC_OFFS + 0 ], data[ MAC_OFFS + 1 ], data[ MAC_OFFS + 2 ],  \
	data[ MAC_OFFS + 3 ], data[ MAC_OFFS + 4 ], data[ MAC_OFFS + 5 ] );

     txt_printf( 3, 6, WHT_PAL, "      IP:  %d.%d.%d.%d", data[ IP_OFFS + 3 ], \
	data[ IP_OFFS + 2 ], data[ IP_OFFS + 1 ], data[ IP_OFFS + 0 ] );

     txt_printf( 3, 7, WHT_PAL, "Location:  %s", &data[ STR_OFFS ] );

#endif
/* PGG END */

    }
#if 0
    else
    {
     txt_cstr("Failed",RED_PAL);
     while(1) prc_delay0();
    }
#endif

    while(1) prc_delay0();
}

/* >>->	If your self test needs some sort of data (e.g. anim tables), here
 * is a reasonable place to put it. For Beathead, we needed a set of pictures
 * in RLE compressed form, and a set of palettes to use for the pictures.
 */

/* >>->	Now specify the DEFAULT configuration block you want to use	*/
/*	for this game							*/

const struct pconfigb guts_pbase =  {
    dummy_main,		/* RESET vector	 for non-game	*/
    gamemenu,		/* no game option menu (stats.c will use fakemenu) */
    coinmenu,		/* no coin option menu (stats.c will use coinmenu) */
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
    const unsigned char *menu;

    txt_str(-1,(AN_VIS_ROW/2)-1,"SETTING BRAM DEFAULT VALUES",ERROR_PAL);
    if ( (menu = pbase->p_optmenu) != 0 ) {
	/* if game spec's a menu, use the *'d defaults */
	eer_puts(EER_GMOPT,factory_setting(menu));
    }
#if EER_TRKOPT
    if ( (menu = pbase->p_trkmenu) != 0 ) {
	/* if game spec's a menu, use the *'d defaults */
	eer_puts(EER_TRKOPT,factory_setting(menu));
    }
#endif
#if EER_LAPOPT
    if ( (menu = pbase->p_lapmenu) != 0 ) {
	/* if game spec's a menu, use the *'d defaults */
	eer_puts(EER_LAPOPT,factory_setting(menu));
    }
#endif
#if EER_TRNOPT
    if ( (menu = pbase->p_trnmenu) != 0 ) {
	/* if game spec's a menu, use the *'d defaults */
	eer_puts(EER_TRNOPT,factory_setting(menu));
    }
#endif
    /* Also Restore coin defaults.
     */
    if ( (menu = pbase->p_coinmenu) != 0 ) {
	eer_puts(EER_CNOPT,factory_setting(menu));
    }
#ifdef EER_GUTS_OPT
    eer_puts(EER_GUTS_OPT,factory_setting(gut_menu));
#endif
#ifdef EER_AUD_VOL
#ifdef DEF_VOLUME
    eer_puts(EER_AUD_VOL,DEF_VOLUME);
#endif
#endif
    prc_delay(180);
    txt_clr_str(-1,(AN_VIS_ROW/2)-1,"SETTING EEPROM DEFAULT VALUES",ERROR_PAL);
}
