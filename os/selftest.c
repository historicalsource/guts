/*
 *	selftest.c
 *
 *		Copyright 1991 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: selftest.c,v 1.53 1997/10/02 02:39:51 shepperd Exp $";
#endif
#define BRAVE_NEW_WORLD (1)
#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <eer_defs.h>
#include <string.h>

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#ifndef ST_AUTO_INC
#define ST_AUTO_INC (0)
#endif

#ifndef ST_SWITCH
# if defined(TEST) && B_TEST >= 0
#  define ST_SWITCH (!(TEST & (1<<B_TEST)))
# else
#  define ST_SWITCH (1)
# endif
#endif

#if HOST_BOARD_CLASS && ((HOST_BOARD&HOST_BOARD_CLASS) == PHOENIX) 
# define LINK_TIME GUTS_LINK_TIME
#endif

#ifndef BLAB
# define BLAB(x)
#endif

#define	STATIC	static

/* Unique selftest main menu	*/
extern	struct menu_d	mainMenu[];

/* The time the GUTS was linked. The definition used to depend on whether
 * an ANSI compiler was used. Of course, both date.c and this file had to
 * be compiled with the same compiler. In one small step for correctness
 * versus backward compatibility, MEA deletes pre-ANSI behavior, 14JUN95
 */

extern const char LINK_TIME[];

/* We will be moving all the PBASE stuff to this file. For now, we at
 * least need to declare it (MEA 14JUN95).
 */
extern const struct pconfigp PBASE;
#if HOST_BOARD_CLASS && ((HOST_BOARD&HOST_BOARD_CLASS) == PHOENIX) 
/* Dave changed def_pbase (meaning Default Pbase) to guts_pbase to
 * avoid namespace collisions in games where a) GUTS is a library
 * and b) The game programmer did a careless cut-and-paste of
 * gamedefs.c to make the game pbase. Of course, changing the name
 * will not help, in that careless cut-and-pasters will now have two
 * definitions of guts_pbase, but at least I (MEA) don't have to go
 * edit all of Dave's games.
 */
extern const struct pconfigb guts_pbase;
#define DEF_PBASE guts_pbase
#else
extern const struct pconfigb def_pbase;
#define DEF_PBASE def_pbase
#endif

int debug_mode;				/* from EER_GUTS_OPT, enable debug sw */
#ifndef LINKTIME_X
# define LINKTIME_X	4
#endif

STATIC int
PrintDate(row)
int	row;
{
    /********************************************/
    /* writes the creation dates at the row	*/
    /* specified.			jfs	*/
    /********************************************/
	
#if !NO_GAME_LINKTIME
    if ( pbase->p_linktime && pbase->p_linktime[0] ) {
	/* Game program's link time */
	txt_str(LINKTIME_X,row, "MAIN: ",VERSION_PAL);
	txt_cstr(pbase->p_linktime,VERSION_PAL);
    } else {
	txt_str(LINKTIME_X,row, "No game at 0x",ERROR_PAL);
	txt_chexnum((U32)&PBASE,8,RJ_ZF,ERROR_PAL);
    }
    --row;
#endif
    txt_str(LINKTIME_X,row,	"GUTS: ",VERSION_PAL);
    txt_cstr(LINK_TIME,VERSION_PAL);		/* os link date	*/
    return --row;
}

/*		st_bottom()
 *	Returns bottom-most line available for instructions, etc.
 *	This value is what st_frame() or st_insn() last returned,
 *	at this "nesting level". st_menu() will save/restore the
 *	variable this uses, to facilitate nested menus.
 */

static int last_bottom;

int st_bottom()
{
    if ( last_bottom == 0 ) {
#if (GUN_CNT == 0) 
	last_bottom = AN_VIS_ROW-2;	/* Just above border */
#else
	last_bottom = AN_VIS_ROW-3;	/* Leave room for gun arrow */
#endif
    }
    return last_bottom;
}

/*		st_insn( row, action, switch, palette )
 *	A nauseatingly simple little routine that I (MEA) got
 *	tired of re-writing. Takes a row number, two text
 *	strings, and a palette, and tries to combine the
 *	two text strings on a single line, which it writes
 *	centered on the specified row. If it cannot, it
 *	writes the messages separately on two rows, the
 *	specified row and the one _ABOVE_ it. This is
 *	because instructions are typically on the bottom
 *	of the screen, and I want them to "grow up" (good
 *	advice for us all :-). Returns the number of the
 *	row above the highest one used, to allow a series
 *	of instructions to be assembled.
 *
 *	KLUGE WARNING: If row is negative, erases rather
 *	than writing, using |row|. In this case, also
 *	returns -|row| at end, after suitably modifying it.
 *	This is to allow those very few screens which need
 *	to modify instructions "on the fly" to do so.
 */
int st_insn( row, action, swt, palette)
int row, palette;
const char *action;
const char *swt;
{
    char buf[AN_VIS_COL_MAX+1];
    int len;

    if ( row == 0 ) row = st_bottom();
    strncpy(buf,action,AN_VIS_COL);
    len = AN_VIS_COL-strlen(buf);
    if ( (len-3) < strlen(swt) ) {
	if ( row >= 0 ) {
	    txt_str(-1,row--,swt,palette);
	    txt_str(-1,row--,action,palette);
	} else {
	    /* erasing */
	    row = -row;
	    txt_clr_str(-1,row--,swt,palette);
	    txt_clr_str(-1,row--,action,palette);
	    row = -row;
	}
    } else {
	strcat(buf," ");
	strncat(buf,swt,len-1);
	if ( row >= 0 ) {
	    txt_str(-1,row--,buf,palette);
	} else {
	    /* erasing */
	    row = -row;
	    txt_clr_str(-1,row--,buf,palette);
	    row = -row;
	}
    }
    last_bottom = row;
    return row;
}

/*		ExitInst(palette)
 *	Writes instructions for exiting in specified palette.
 *	Palette is changeable due to possible limitations in
 *	color-bar test.
 */
void
ExitInst(int palette)
{
#if J_UP
    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, palette);
#else
    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_nexth, palette);
#endif
}

#ifndef AN_UL_STMP
/* If no corner stamps, use squares */
#define AN_UL_STMP (AN_SQUARE)
#define AN_LL_STMP (AN_SQUARE)
#define AN_UR_STMP (AN_SQUARE)
#define AN_LR_STMP (AN_SQUARE)
#endif

#ifndef AN_TOP_STMP
/* If no horizontal "line" stamps, use squares */
#define AN_BOT_STMP (AN_SQUARE)
#define AN_TOP_STMP (AN_SQUARE)
#endif
#ifndef AN_LFT_STMP
/* If no horizontal "line" stamps, use squares */
#define AN_LFT_STMP (AN_SQUARE)
#define AN_RGT_STMP (AN_SQUARE)
#endif
/*		BorderEdge()
 *	Draws frame around screen.
 */

static void
BorderEdge()
{

    m_int x,y;

    for ( x = 0 ; x < AN_VIS_COL ; ++x ) {
	/* draw top and bottom edges */
	txt_stamp(x,0,AN_TOP_STMP,GRY_PAL);
	txt_stamp(x,AN_VIS_ROW-1,AN_BOT_STMP,GRY_PAL);
    }

    for ( y = 0 ; y < AN_VIS_ROW ; ++y ) {
	/* draw left and right edges */
	txt_stamp(0,y,AN_LFT_STMP,GRY_PAL);
	txt_stamp(AN_VIS_COL-1,y,AN_RGT_STMP,GRY_PAL);
    }

    txt_stamp(	0		,0		,AN_UL_STMP,GRY_PALB);
    txt_stamp(AN_VIS_COL-1	,0		,AN_UR_STMP,GRY_PALB);
    txt_stamp(	0		,AN_VIS_ROW-1	,AN_LL_STMP,GRY_PALB);
    txt_stamp(AN_VIS_COL-1	,AN_VIS_ROW-1	,AN_LR_STMP,GRY_PALB);
}

/* If you need to inject some sort of special hoo-haw (e.g. the CoJag
 * Revision-detect), do it in a macro you define as HDW_INIT(cold).
 * If not, the following do-nothing is used instead. If you have much to
 * do, just place it in another file and call it. For backward compatibility,
 * games that define LM_INIT_BITS and do not define HDW_INIT will get the
 * former default action.
 *
 * As of 1AUG1997, MEA changes default HDW_INIT(x) to hdw_init(x) because
 * just about _all_ our boards have some sort of special hoo-haw to do.
 * If you really need to do literally nothing, you could add the old
 * definition to your config.mac:
 *    #define HDW_INIT(cold) do {;} while (0)
 * or you could just put a dummy hdw_init() in your xxx_stubs.c
 * or xxx_shims.c file.
 */
#ifndef HDW_INIT
#ifdef LM_INIT_BITS
#define HDW_INIT(cold) do { prc_mod_latch(LM_INIT_BITS); } while (0)
#else
extern void hdw_init(int);
#define HDW_INIT(cold) hdw_init(cold)
#endif /* Def'd LM_INIT_BITS */
#endif /* ndef'd HDW_INIT */

#if (GUN_CNT > 0)
extern int gun2idx PARMS((
const struct menu_d * menu,
int menu_size,
int vpos
));
#define DOUBLE_CLICK (42)
extern int gun_init();
#endif
#if REMOTE_STATS
# if !defined(BR9600)
#  define BR9600 IO_UART_CTL_9600_BAUD
# endif
extern int rmt_init(U16);
#endif
#if NET_LVL
extern int net_init(void);
#endif
#ifndef MENU_FLAGS
#define MENU_FLAGS (STF_DATE|STF_NOEXIT)
#endif

void
SelfTest(cold_flag)
unsigned long cold_flag;	/* non-zero if this is "cold boot" */
{
    struct menu_d *menu;
    const struct st_envar *vec;

    HDW_INIT(cold_flag);

    if( PBASE.p_validate == PB_VALID ){
        pbase = PBASE.p_configb;
        }
    else pbase = &DEF_PBASE;

#if defined(PRC_DELAY_OPT_TEXT2FB)
/*
 * select a "default" prc_delay() action to copy text into the frame
 * buffer, swap buffers, clear non-displayed buffer and wait.
 */
    prc_delay_options(PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR);
#endif

#if HOST_BOARD != ASCLEAP
    BLAB("\nSelftest: Enabling interrupts...");
    prc_set_ipl(INTS_ON);
#endif
    BLAB("\nSelftest: Initializing video subsystem...");
    vid_init();
#ifdef PSX_GAME
    /* MEA has no idea why these can't be in hdw_init() or vid_init().
    */
    {
	extern void guts_init();
	extern void txt_set_mode( int );
	guts_init();
	txt_set_mode(TEXT_DRAW_REPLACE);
    }
#endif
#if ST_AN_SET
    /* If the default game alphanumerics are illegible,
     * we use ST_AN_SET for selftest.
     */
    st_font(ST_AN_SET);
#endif
    BLAB("\nSelftest: Setting alphanumeric colors...");
    setancolors();
    BLAB("\nSelftest: Reading EEPROM or BRAM...");
    while ( eer_init() < 0 ) {
	txt_str(-1,AN_VIS_ROW/2,"DATABASE ERRORS",ERROR_PAL);
	prc_delay(180);
	vid_clear();
	prc_delay(10);
    }
#ifdef EER_GUTS_OPT
    debug_mode = eer_gets(EER_GUTS_OPT) & ~AUD_AV_MSK;
#ifdef GAME_DEFAULT
    debug_mode |= GAME_DEFAULT;
#endif
#endif
    BLAB("\nSelftest: Checking for PostMortem dump...");
    pm_dump();
    BLAB("\nSelftest: Initializing audio...");
    aud_init(1);
#ifdef VCR_STOP
    BLAB("\nSelftest: Turning off VCR...");
    vcr(VCR_STOP);					/* turn off VCR	*/
#endif
#if GUN_CNT
    BLAB("\nSelftest: Initializing guns...");
    gun_init();
#endif
#if POT_CNT
    BLAB("\nSelftest: Initializing POTS...");
    pot_init(0);
    prc_delay(2);			/* get first pot->switch conversion */
#endif
#if REMOTE_STATS
    BLAB("\nSelftest: Initializing remote stats...");
    rmt_init(BR9600);
#endif
#if NET_LVL
    BLAB("\nSelftest: Initializing network...");
    net_init();
#endif
    vec = st_getenv("PRE ST and GAME", 0);
    if (vec && vec->value) {
	BLAB("\nSelftest: Calling PRE ST and GAME ...");
	((int (*)(void))(vec->value))();
    }
    if ( !ST_SWITCH && pbase->p_reset ) {
	/* Test switch off on entry, go to game */
#if ST_AN_SET
	/* Let the game use an illegible font for default.
	 */
	st_font(0);
#endif
	BLAB("\nSelftest: Set default volumes...");
	aud_setvol(-1);			/* Set game volume from EEPROM */
	aud_setvol(-2);			/* But keep quiet for now */
	BLAB("\nSelftest: Initialize coins...");
	cn_init();

	vec = st_getenv("PRE GAME", 0);
	if (vec && vec->value) {
	    BLAB("\nSelftest: Calling PRE GAME ...");
	    ((int (*)(void))(vec->value))();
	}
#if defined(PRC_DELAY_OPT_TEXT2FB)
	prc_delay_options(0);		/* no special options for game */
#endif
	BLAB("\nSelftest: Goto game...\n");
	pbase->p_reset();
	prc_reboot();		/* reset if game returns */
    }

/* Getting here means Test switch is on or no game, so run self-test */

    BLAB("\nSelftest: Set selftest vblank interrupt...");
    vid_set_vb(st_vblank);
#if (0)
    menu = (struct menu_d *)mainMenu;
#else
    {
	const struct st_envar *mp;
	mp = st_getenv("SELFTEST MENU",0);
	if ( mp ) menu = (struct menu_d *)mp->value;
	else {
	    txt_str(-1, st_bottom(),"st_getenv() failed",ERROR_PAL);
	    prc_delay(240);
	    menu = (struct menu_d *)mainMenu; 
	}
    }
#endif
#ifdef DEBUG
#ifdef HANDSHAKE
    if( (ctl_read_sw(0) & HANDSHAKE) == HANDSHAKE ) {
	/* "Secret Handshake" */
	int bozo_time;
	BLAB("\nSelftest: Do secret handshake...");
	txt_str(-1,(AN_VIS_ROW/2)+1,"OK, I HEARD YOU",MNORMAL_PAL);
	for ( bozo_time = 180 ; bozo_time >= 0 ; --bozo_time ) {
	    if ( (ctl_read_sw(0) & HANDSHAKE) == 0 ) {
		debug_mode |= GUTS_OPT_DEVEL;
		break;
	    }
	    prc_delay0();
	}
	txt_clr_str(-1,(AN_VIS_ROW/2)+1,"OK, I HEARD YOU",MNORMAL_PAL);
    }
#endif /* HANDSHAKE */
#endif /* DEBUG */

    ctl_read_sw(-1);	/* kill all edges */

    vec = st_getenv("PRE ST", 0);
    if (vec && vec->value) {
	BLAB("\nSelftest: Calling PRE ST ...");
	((int (*)(void))(vec->value))();
    }
    BLAB("\nSelftest: Done booting\n");
    while (1)		/* ::::::::::	MAIN LOOP :::::::::	*/
    {
	ctl_autorepeat(JOY_VERT,30,15); /* Autorepeat after 1/2secs @ 1/4 */

	st_menu(menu,sizeof(mainMenu[0]),MNORMAL_PAL, MENU_FLAGS);
#if !(MENU_FLAGS&STF_NOEXIT)
	/* Currently only one game (CalSpeed) requires that we
	 * pop back directly from anywhere in the main menu
	 * to the game, but this will probably change. We
	 * wrap this call so as not to disturb existing games
	 * which don't have/want this function, or supply it
	 * themselves in gamedefs.c (MEA 1AUG97)
	 */
	exit_to_game(0);
#endif
    }
}

/*	All of this should be in ram_test.c, rather than selftest.c, since
 *	there is a single MEMORY TEST item in the menu.
 */
struct rom_descrip {
    char *name;
    struct rdb	descript;
};
STATIC const struct rom_descrip rom_list[] = {
/*
 *	The seed value is actually +1 because we want to add up to 0,
 *	while the cksum program sets to add to 0xFF
 */
/*             31-24 23-16 15-8 7-0 */
/*	         3H   3P   3M   3K */
#if (HOST_BOARD == LCR3K)
#if (COJAG_GAME & COJAG_AREA51)
    /* Horrible kluge to get checksums nominally working for Area 51, R3K.
     * This whole section needs to be:
     * 1) re-written.
     * 2) Moved to a more logical file, maybe ram_test.c, despite the name...
     * 3) Integrated with power-on diagnostics as in "The old days..."
     */
#if (COJAG_REV >= 3) /* Rev 3/4 Single-board CoJag with integral LCR3K */
    { "HOST      22R  19R  22U  19U",	{(U32*)0x9FC00000, 0x200000, 0x01020304}},
#else /* Rev2 Cojag with seperate LCR3K */
    { "HOST      3H   3P   3M   3K",	{(U32*)0x9FC00000, 0x200000, 0x01020304}},
#endif /* (COJAG_REV >= 3) */
#else
    { "HOST      3H   3P   3M   3K",	{(U32*)0x9FC00000, 0x32000, 0x01020304}},
#endif /* Area 51 */
#endif /* LCR3K */
#if (HOST_BOARD == EC020cojag)
    { "HOST      3H   3P   3M   3K",	{(U32*)0x800000, 0x200000, 0x01020304}},
#endif
#if (0)
    { "TMEK0     24L  26L  28L  29L",	{0x00000, 0x80000, 0x01020304}}, */
#endif
    { 0, {0, 0, 0} }
};

STATIC int
rom_test()
{
    const struct rom_descrip *rd;
    m_int row,col;		/* For error message */
    U32	cksums,check;		/* Returned from rom_cksum */
    int	anyBadRoms,anyBad;
    unsigned int palette;
    m_int	x;

    rd = &rom_list[0];
    row = 3;
    anyBadRoms = 0;

#if (PROCESSOR == M68000 || PROCESSOR == M68010)
    txt_str(AN_VIS_COL/2,row,"   H     L",WHT_PALB);
#else
    txt_str(AN_VIS_COL/3,row++,"  0    1    2    3 ",WHT_PALB);
    txt_str(AN_VIS_COL/3,row++," HH   HL   LH   LL ",WHT_PALB);
#endif

    while (rd->name)
    {
#if 0		/* name does not include PCB locations	*/
	x = (AN_VIS_COL/3) - 4 - strlen(rd->name);
	if (x < 1)
	    x = 1;
	txt_str(x,row,rd->name,WHT_PALB);
#else		/* name has PCB locations */
	++row;
	x = (AN_VIS_COL/3) - 9;
	if (x < 1)
	    x = 1;
	txt_str(x,row++,rd->name,WHT_PALB);
#endif

	anyBad = -1;
	while (anyBad)
	{
	    anyBad = 0;
	    cksums = rom_cksum((const struct rdb *)&rd->descript);

#if (PROCESSOR == M68000 || PROCESSOR == M68010)
	    col = (AN_VIS_COL/2) + 6;
	    for (x=0; x<2; col-=6,cksums>>=16,++x)
	    {
		if ((check = (cksums & 0xFFFF)) & 0xFF)
#else
	    col = (AN_VIS_COL/3) + 16;
	    for (x=0; x<4; col-=5,cksums>>=8,++x)
	    {
		if ( (check = (cksums & 0xFF)) != 0 )
#endif
		{
		    anyBadRoms = anyBad = -1;
		    palette = RED_PALB;
		}
		else
		    palette = GRY_PAL;
#if (PROCESSOR == M68000 || PROCESSOR == M68010)
		txt_hexnum(col,row,check,4,RJ_ZF,palette);
#else
		txt_hexnum(col,row,check,2,RJ_ZF,palette);
#endif
	    }

	    /* Change our opinion of "badness" if user hits SW_ACTION, so
	     * we can get on to further instructions
	     */
	    if ( ctl_read_sw(SW_ACTION) & SW_ACTION ) anyBad = 0; /* lie */
	}

	++rd;			/* next specifier	*/
	++row;			/* next text row	*/
    }

    return (anyBadRoms);
}

int
ROMTest(smp)
const struct menu_d *smp;
{
    int	i;
    int bottom;

    bottom = PrintDate(AN_VIS_ROW-6);
#if (0)
    /* following not needed, as rom_test() now looks for
     * _edge_ to stop loop.
     */
    while (ctl_read_sw(0) & SW_ACTION)
	prc_delay0();
#endif

    bottom = st_insn( bottom, "To STOP test LOOP,", t_msg_action, INSTR_PAL);

    i = rom_test();
    if (i == 0)
	txt_str(-1,AN_VIS_ROW/2,"ALL ROMS are OK",GRN_PALB);

    ctl_read_sw(SW_NEXT);			/* lose this edge	*/

    ExitInst(INSTR_PAL);

    while (1)
    {
	prc_delay0();
	if (ctl_read_sw(SW_NEXT) & SW_NEXT)
	    break;
    }
    return i;
}

#ifndef MENU_Y_DEFAULT
#define MENU_Y_DEFAULT (3)
#endif
#ifndef MENU_X_DEFAULT
#define MENU_X_DEFAULT (4)
#endif

/*		st_menu()
 *	Menu-display harness for tests. Uses controls to select a
 *	menu-line, then runs selected test passing pointer to line.
 *	the size of a "menu line" struct must be passed to allow
 *	different callers to have different additional information.
 *
 *	The general function is:
 *	Present a menu of tests/function
 *	User selects a line with Up/Down controls, selected line is highlited
 *	SW_ACTION causes the corresponding function to be invoked
 *		A zero return from the function will cause the menu line
 *		to be incremented. A non-zero return will cause the line to
 *		"stick" on the selected test.
 *	SW_NEXT cause return of to the caller. The possible return values
 *	are: -1 if SW_NEXT was pressed on some line before the last, else
 *	0.
 *	
 *	If there is no UP/Down control (J_UP == 0), SW_NEXT serves a dual
 *	purpose. Each press advances to the next test, while _holding_
 *	the button returns to the previous caller (if any).
 */
static const struct menu_d * idx2menu PARMS((
const struct menu_d * menu,
int menu_size,
int idx
));

#if !J_UP
#ifndef EX_FRAMES
#define EX_FRAMES 40
#endif
#endif

int
st_menu( menu, menu_size, txt_pal, options )
const struct menu_d *menu;	/* caller's menu */
int	menu_size;		/* size of an individual item */
int	txt_pal;		/* palette to use for text */
int 	options;		/* to pass to st_frame() for date, exit */
{
    const struct menu_d *wmp;	/* working menu pointer */
    const struct menu_d *smp;	/* selected menu pointer */
    const struct menu_d *title = menu; /* Title entry of this menu */
    int		idx,oidx,widx;	/* line indices for display */
    int		status;		/* return value from sub-test*/
    int		vpos,top;	/* vertical position (working and top) */
    int		redraw;
    int		prev_bottom;
    const char *label;
#ifdef EX_FRAMES
    int ex_cnt = EX_FRAMES;
#endif
    /* possible co-routine to call during poll */
    int		(*coroutine) PARMS((const struct menu_d *));

    idx = 0,oidx = -2;
    redraw = 1;

    /*	Save last_bottom, in case we are nesting.
     */
    prev_bottom = last_bottom;
    last_bottom = 0;

    /* extract menu title (if any), vertical position, and possible
     * co-routine from first entry, then increment past it for the
     * duration.
     */
    top = MENU_Y_DEFAULT;
    if ( (label = menu->mn_label) != 0 ){
	if ( *label == '?' ) ++label;
	if ( *label == '\v' ) {
	    top += label[1] - '0';
	    label += 2;
	}
	if ( *label == 0 ) title = 0;
    }
    coroutine = menu->mn_call;
    menu = (const struct menu_d *)((char *)menu + menu_size);
    while ( 1 ) {
	/* (re)paint entire menu */
	wmp = menu;
	smp = 0;
	widx = 0;
	vpos = top;
	if ( redraw ) {
	    int bottom;
	    vid_clear();
	    setancolors();
	    bottom = st_frame( title, TITLE_PAL, INSTR_PAL, options );
	    bottom = st_insn(bottom, "To run test,", t_msg_action, INSTR_PAL);
	    bottom = st_insn(bottom, "To select test,",t_msg_control, INSTR_PAL);
	    if ( coroutine ) coroutine(0);
	    redraw = 0;
	}
	while ( oidx != idx ) {
	    if ( (label = wmp->mn_label) == 0 ) {
		oidx = idx;	/* done */
		break;
	    }
	    if ( *label == '?' ) {
		/* an item that should only be available
		 * during development or after a
		 * "secret handshake"
		 */
		++label;
#ifdef DEBUG
		if ( (debug_mode & GUTS_OPT_DEVEL) == 0 )
#endif
		{
		    wmp = (const struct menu_d *)((char *)wmp + menu_size);
		    ++widx;
		    continue;
		}
	    }
	    if ( *label == '\n' ) {
		/* caller wants leading */
		++vpos;
		++label;
	    }
	    if ( widx == oidx && widx != idx ) {
		/* de-highlight old line, if not same as current */
		txt_clr_str(MENU_X_DEFAULT,vpos,label,MHILITE_PAL);
		txt_str(MENU_X_DEFAULT,vpos,label,MNORMAL_PAL);
	    }
	    else if ( widx == idx ) {
		/* this is the selected line, save struct pointer and
		 * hilite line.
		 */
		smp = wmp;
		txt_clr_str(MENU_X_DEFAULT,vpos,label,MNORMAL_PAL);
		txt_str(MENU_X_DEFAULT,vpos,label,MHILITE_PAL);
	    }
	    else txt_str(MENU_X_DEFAULT,vpos,label,MNORMAL_PAL);
	    ++vpos;
	    wmp = (const struct menu_d *)((char *)wmp + menu_size);
	    ++widx;
	} /* end while (oidx != idx) for (re)paint of menu */

	/* now check switches for selection */
	while (oidx == idx) {
	    unsigned long ctls;
	    ctls = ctl_read_sw(SW_ACTION|SW_NEXT|JOY_VERT);
	    if ( coroutine && (status = coroutine(smp)) != 0) {
		last_bottom = prev_bottom;
		return status;
	    }
#ifndef EX_FRAMES
	    /* Traditional interpretation, any press on SW_NEXT
	     * returns to caller, -1 if not at the bottom, 0 if
	     * at the bottom. If in the "top level" menu (as
	     * indicated by STF_NOEXIT), just advance to next item.
	     */
	    if ( ctls & SW_NEXT ) {
		/* NEXT just advances menu pointer if at top level.
		*/
		if ( options & STF_NOEXIT ) ctls |= J_DOWN;
		else {
		    last_bottom = prev_bottom;
		    return (idx == widx) ? 0 : -1;
		}
		ctls &= ~SW_ACTION;
	    }
#else
	    /* Modified interpretation, _holding_ SW_NEXT causes
	     * exit as above, while simply _pressing_ it causes
	     * advance to next item.
	     */
	    if ( ctls & SW_NEXT ) {
#if J_DOWN
		ctls |= J_DOWN;
#else
		int wrap = 1;
		while ( wmp == 0 ) {
		    /* keep skipping forward until
		     * we land on a usable item.
		     */
		    if ( ++idx == widx ) {
			/* wrap to first, but not more than once */
			idx = 0;
			if ( --wrap < 0 ) prc_panic("Null Menu"); 
		    }
		    wmp = idx2menu(menu,menu_size,idx);
		}
#endif
		ctls &= ~SW_ACTION;
	    }
	    else if ( ctl_read_sw(0) & SW_NEXT ) {
		if ( --ex_cnt <= 0 ) {
		    last_bottom = prev_bottom;
		    return (idx == widx) ? 0 : -1;
		}
		ctls &= ~SW_ACTION;
	    } else ex_cnt = EX_FRAMES;
#endif
	    if ( ctls & SW_ACTION ) {
		/* run selected test */
		vid_clear();
		setancolors();
		/* Call below changed (MEA, 22SEP95) to include STF_NOEXIT.
		 * This avoids need for re-draw just to change wording
		 * for exit. It requires every sub-test to call
		 * ExitInst() (or equivalent st_insn()), but means fewer
		 * extra st_frame() calls.
		 */
		st_frame( smp, TITLE_PAL, INSTR_PAL, STF_NOEXIT);
		status = smp->mn_call(smp);
		oidx = -2;
		redraw = 1;
		ctls = 0;
#if ST_AUTO_INC
		if ( !status ) ctls = J_DOWN;
#endif
	    }
	    status = 1;
	    wmp = 0;
#if (GUN_CNT > 0)
	    {
		/* Temp gun control */
		int gidx;
#if DOUBLE_CLICK
		static int old_gidx;
		static unsigned long old_rtc;
#endif
		gidx = gun2idx(menu,menu_size,top);
		if ( gidx >= 0 ) {
		    wmp = idx2menu(menu,menu_size,idx=gidx);
#if DOUBLE_CLICK
		    if ( (gidx == old_gidx)
			&& ((eer_rtc - old_rtc) < DOUBLE_CLICK)
			&& (eer_rtc > (DOUBLE_CLICK<<1)) ) {
			/* double-click/shot, run selected test */
			old_gidx = -1;
			vid_clear();
			setancolors();
			st_frame( smp, TITLE_PAL, INSTR_PAL, STF_NOEXIT);
			status = smp->mn_call(smp);
			oidx = -2;
			redraw = 1;
			ctls = 0;
		    } else {
			/* Single click/shot, just position */
			old_gidx = gidx;
			old_rtc = eer_rtc;
		    }
#endif
		}
	    }
#endif
#if J_UP
	    if ( ctls & J_UP ) {
		while ( wmp == 0 ) {
		    /* keep skipping backward until
		     * we land on a usable item.
		     */
		    if ( --idx < 0 ) {
			/* wrap to last, but not more than once */
			idx = widx-1;
			if ( --status < 0 ) prc_panic("Null Menu"); 
		    }
		    wmp = idx2menu(menu,menu_size,idx);
		}
	    } else
#endif
#if J_DOWN
	    if ( ctls & J_DOWN ) {
		while ( wmp == 0 ) {
		    /* keep skipping forward until
		     * we land on a usable item.
		     */
		    if ( ++idx == widx ) {
			/* wrap to first, but not more than once */
			idx = 0;
			if ( --status < 0 ) prc_panic("Null Menu"); 
		    }
		    wmp = idx2menu(menu,menu_size,idx);
		}
	    }
#endif
	    prc_delay0();
	} /* end while waiting for line to change */
    }
}

static const struct menu_d * idx2menu(menu, menu_size, idx)
const struct menu_d * menu;
int menu_size;
int idx;
{
    int widx;
    if ( idx < 0 ) return 0;
    for ( widx = 0 ; widx < idx ; ++widx ) {
	if ( menu->mn_label == 0 ) {
	    /* we have run off the end */
	    return 0;
	}
	menu = (const struct menu_d *)((char *)menu + menu_size);
    }
    if ( menu->mn_label == 0 ) return 0;
    if ( menu->mn_label[0] == '?' ) {
#ifdef DEBUG
	if ( (debug_mode & GUTS_OPT_DEVEL) == 0 )
#endif
	{
	    return 0;
	} 
   }
   return menu;
}

/*		st_frame()
 *	re-paints the border and title, as well as optional date, exit
 *	instructions, etc. This is intended for use mainly by st_menu(),
 *	but will also be needed by any test that messes up the screen and
 *	thus needs to re-paint the boilerplate. Returns bottom-most row
 *	available for further instructions.
 */

int st_frame( smp, tpal, ipal, options )
const struct menu_d *smp;
int	tpal;
int	ipal;
int	options;
{
    const char *label;
    char title[AN_VIS_COL_MAX+2];
    int	bottom;
    int i;

    BorderEdge();
    last_bottom = 0;		/* Re-init */
    bottom = st_bottom();

    if ( options & STF_DATE ) {
	/* show os/main dates */
	bottom = PrintDate(bottom);
    }
    label = smp->mn_label;
    if ( *label == '?' ) ++label;
    if ( *label == '\v' ) label += 2;
    if ( *label == '\n' ) ++label;
    for ( i = 0 ; i < AN_VIS_COL ; ++i ) {
	if ( label[i] == ' ' ) title[i] = '_';
	else if ( (title[i] = label[i]) == '\0' ) break;
    }
    title[i] = '\0'; /* insurance */
    if ( txt_width(title,tpal|AN_BIG_SET) > (AN_VIS_COL-2) ) {
	/* oops, won't fit in BIG chars, take back those '_'s and
	 * make it little.
	 */
	for ( i = 0 ; title[i] ; ++i ) if ( title[i] == '_' ) title[i] = ' ';
    } else tpal |= AN_BIG_SET;
    txt_str(-1,0,title,tpal);
    if ( (options & STF_NOEXIT) == 0 ) {
#if J_UP
	bottom = st_insn(bottom, t_msg_ret_menu, t_msg_next, ipal);
#else
	bottom = st_insn(bottom, t_msg_ret_menu, t_msg_nexth, ipal);
#endif
    }
    last_bottom = bottom;
    return bottom;
}

/*		st_getenv(name,hook)
 *	Finds the "environment variable" indicated by <name>
 *	and returns a pointer to the (struct st_envar) that
 *	specifes it. These structs are _not_ simply text
 *	strings as in POSIX, for the convenience of routines
 *	that may need to pass arbitrarily complex data. The
 *	search starts at the (struct st_envar) specified by
 *	<hook>, to allow multiple values for the same name
 *	and allow the user to reject an entry and keep looking.
 *	The "rev" field of the structure is intended to
 *	facilitate such decisions. If <hook> is 0, the search
 *	starts at the "top" of the chain specified by the game,
 *	if any. After the game's chain is searched, any
 *	defaults supplied by GUTS are searched.
 */
static const struct st_envar guts_env = {
    "SELFTEST MENU",		/* Name */
    (void *)&mainMenu,		/* Value pointer */
    0,				/* Next */
    0				/* Version */
};

const struct st_envar *
st_getenv ( name, hook )
const char *name;
const struct st_envar *hook;
{
    const struct st_envar *curr = hook;
    if ( curr == 0 ) {
	/* Starting from scratch, look for game-supplied
	 * list.
	 */
	const struct pconfigp *game_base = &PBASE + 1;
	if ( game_base->p_validate == 0xFEEBF00D ) {
	    curr = (const struct st_envar *)game_base->p_configb;
	}
	else curr = hook = &guts_env;
    } else curr = curr->next;
    /* At this point, we are pointing at the first st_envar
     * to examine, if it exists. We _could_ have just walked
     * off the end of the first (game) chain, and need to pick
     * up with the GUTS chain, or we could have just walked off
     * the end of the GUTS chain. The only real way to know this
     * is to trace the GUTS chain and see if our entry hook
     * is on it. If so, we are totally done. If not, start at
     * &guts_env. This can happen at any time in the search,
     * so we put the check first in the loop.
     */
    while (1) {
	if ( !curr ) {
	    /* end of the line, maybe. try to find hook
	     * on guts list.
	     */
	    for ( curr = &guts_env ; curr ; curr = curr->next ) {
		if ( curr == hook ) return 0;	/* been there */
	    }
	    curr = &guts_env;		/* restart search with GUTS defaults */
	    hook = &guts_env;		/* Remember, for quick bail next time*/
	}
	if ( curr->name[0] == name[0] && !strcmp(curr->name,name) ) {
	    break;
	}
	curr = curr->next;
    }
    return curr;
}
