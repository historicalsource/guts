/*	stats.c
 *		Contains Statistic Routines relating to Game Times & Histograms
 *	Latest Edit:			18-Jan-1989	Rich Moore
 *	Later Edit:			01-NOV-1989	John Salwitz
 *	Later Edit:			18-Jun-1990	Peter Lipson
 *	Incorporated SIMUL code from BLOOD Aug-1990	Peter Lipson
 */

/*
 *	Copyright 1987 ATARI.  Unauthorized reproduction, adaptation,
 *	distribution, performance or display of this computer
 *	program or the associated audiovisual work is strictly prohibited.
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: stats.c,v 1.47 1997/11/07 18:23:12 forrest Exp $";
#endif
#include <config.h>
#include <st_proto.h>
#include <os_proto.h>
#include <string.h>
#include <eer_defs.h>
/* MEA: We need to find out _why_ this is included, and who needs
 * for it _not_ to be...
 *
 * Part one is answered "AREA 51 needs it to track GMOPT_...", which
 * is labeled a kluge right there in their stat_map.mac file where
 * the dirty deed is done. The SST_* games included it because they
 * wanted access to USER_RECS for the purpose of dumping USER messages.
 * I (MEA) fixed ShowUserMsgs() to not need this, so I'm trimming
 * the kluge-list down to AREA_51.
 */
#if (COJAG_GAME&COJAG_AREA51)
#include <stat_defs.h>
#endif

/* Allow re-definable screen dimensions, by providing a separate
 * number to dimension string buffers, as distinct from the (possibly
 * variable) number of visible columns at any given time.
 */
#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

/* Below is far easier than rooting out all the TRUE and FALSE below. Stats.c
 * is a playpen anyway. MEA 27JUL94
 */
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

#define	PRIVATE	static

#define	INCL_MENU	1	/* if DEF'd provide 'fakemenu' menu	*/

static	const char	t_more_stats[] =	"For more stats,";

#ifdef	EER_LINKFAIL
extern	U32	rxCkErr;
#endif

/* ::::::::::::::	These change on a per_game basis	::::	*/

#ifndef N_PLAYERS
#ifdef EER_2PTIME	/* Guard against future RUSH-like games */
# define N_PLAYERS 	(2)
#else
# define N_PLAYERS 	(1)
#endif
#endif

/*		SIMUL
*	Set this TRUE if you have the standard 2 player game where you can
*	have players enter and exit during the game play.
*	There is no particular reason to ever set this to zero. Support
*	for non-SIMUL will disappear in the near future.
*
* NOTE	eer_play() and eer_end() will be called with a mask rather than a
*	player number.
*/
#define	SIMUL		TRUE		/* Simultaneous play for all players */


/* 		SESSIONS
*	Set this true if you want to keep "active minutes" and "session count"
*	in the table of stats.  This also means eer_end() and eer_play() must
*	be called with "N_PLAYERS" to increment the appropriate counters.
*
* NOTE	Only used if SIMUL is set TRUE.
*/
#define SESSIONS	1


/* The following are "frames per second" and "frames per minute". They are
*  #defined mainly to assure inclusion of the cast, which avoids a Greenhills
*  C (1.8.0) bug
*/
#define F_PER_S ((U16) 60)
#define F_PER_M ((U16) 3600)

/* Number of frames in 4 minutes (time to update) */
#define UPDTIM 	(4*60*60)

/* External Definitions */

extern	const unsigned char	coinmenu[];
extern	U32	ctl_read_sw();

/* following #defines are an attempt to clarify "structure" of timers[],
*  which is an array of longwords manipulated by eer_play and eer_end.
*/

#define	N_TIMERS (N_PLAYERS+1)	    /* one each player plus IDLE time	*/

    U32	time_plgame[N_TIMERS];		/* 0,1..N player(s) game time */
    U32	time_eachpl[N_PLAYERS+SESSIONS];	/* player 0,1..N game time */
    U32	time_residue[N_TIMERS+N_PLAYERS+SESSIONS];	/* left over times */
    U8	oldplaymask;

    U32	updtimer;
    U32	last_eertc;

/*		output utilities
*/

PRIVATE void
timenum(value,width)
U32 value;
m_uint width;
{
    U16 mins,secs;

    mins = (U16)(value / 60);
    txt_cdecnum(mins,width-3,LJ_NF,YEL_PAL);
    txt_cstr(":",YEL_PAL);
    secs = value - (60 * mins);
    txt_cdecnum(secs,2,RJ_ZF,YEL_PAL);		/* Write secs		*/
}

#if (0)
PRIVATE void
sayopt()
{
    txt_str(-1,AN_VIS_ROW-3,t_msg_next,INSTR_PAL);
    txt_str(-1,AN_VIS_ROW-2,t_msg_ret_menu,INSTR_PAL);
}
#endif

/*		Statistic Gathering
*	These routines were originally part of xxxEER.C, along with some
*	much more hardware-dependant routines. Now those routines are in
*	xxxEER.ASM and these have been moved to this file, to more effectively
*	modularize things. Anyway, eer_play(), eer_start(), eer_stop, and
*	eer_end() maintain a set of timers and periodically update the EEPROM
*	with said timer information.  The other routines in this file are used
*	to display the info during self-test.  Both sets call on the hardware
*	dependant routines in xxxEER.ASM.
*/


/*		eer_play(players)
*	Starts and stops game time accumulation by copying its parameter to the
*   internal player flag.  This flag contains number of active players.
*	This routine actually returns a pointer to an array of unsigned longs
*   named timers[]. For an explanation of this array, see the #defines above.
*   Just to get you started, the first (T_0PTIME) is time for 0 players,
*   These timers are used (and sometimes modified) by other routines in this
*   package, but should not be lightly messed with.
*
*  The parameter PLAYMASK is a MASK with bits for each active player -
*   it could also be a numeric value (0-players, 1-player, 2-players)
*   in a game where you don't have an arbitrary station running...
*  NOTE that this is a FREE interpretation, with one of three timers
*   running depending on whether it's IDLE/1-PLAYER/2-PLAYER; however,
*   this breaks down for 3-player or multi-player games; you must then
*   explicitly use, for example, 4 as a parameter when a 3-player game
*   is running.
* >>>>>>>>> NOTE:
* >>>>>>	add a CONFIG option to select which mode??
*/

U32 *
eer_play(playmask)		/* (SEE NOTE ABOVE)		*/
unsigned int playmask;
{
    m_uint minutes;
    unsigned int bump;		/* amount of time (frames) since last call */
    unsigned int n;		/* # of players (including player "SESSION")*/
    unsigned int m;		/* copy of old playmask */
    U32 time;			/* sub-minute time for each player */
    U32 *tptr;

    bump = last_eertc;				/* oldrtc */
    bump = (last_eertc = eer_rtc) - bump;	/* Get time since last call */

    /* If a player was enabled, add time since last call */

/* ::::::::::: COUNT EACH ACTIVE PLAYER ::::::::::::::::	*/
    n = 0;
    tptr = time_eachpl;
    for( m = oldplaymask; m != 0; m >>= 1)
    {
	if (m & 1)			/* This player active??	*/
	{
	    *tptr += bump;
	    ++n;			/* .. count active players	*/
	}
	++tptr;
    }
#if SESSIONS
    if (oldplaymask & (1 << N_PLAYERS))	/* is SESSION counter running?	*/
	--n;				/* .. counted one too many then	*/
#endif

    time_plgame[n] += bump;		/* Count time for game class	*/

    if (oldplaymask && playmask == 0)	/* Done with game??		*/
    {
#if (0)
/* This made the assumption that when all players on this machine
 * finished, the game was over. Unless and until we have distributed
 * statistics over a net, this is not true of networked games.
 */
#ifdef EER_GMS
					/* If sessions, games means ?	*/
	eer_incs(EER_GMS,1);		/* .. count it here??		*/
#endif
#endif
	updtimer = UPDTIM;		/* Force an update...		*/
    }

    /* Also bump time since last update, and do the update if greater than UPDTIM*/
    if ( (updtimer += bump) >= UPDTIM)
    {
	updtimer = 0;			/* reset the update timer */

	/* :: This loop updates N_TIMERS cells starting at EER_0PTIME	*/

	tptr = &time_plgame[0];		/* << FIRST CLASS-OF-GAME TIMER!! */
	for( n = 0;			/* Scan the timers	*/
	n < (sizeof(time_plgame)/sizeof(time_plgame[0]));		/* .. all of 'em!!!	*/
	++n, ++tptr)
	{
	    if ( (time = *tptr) > F_PER_M)
	    {
		minutes = ul_over_us(time,F_PER_M);
		/* EEPROM stat gets whole minutes.
		 * timer in RAM gets left-over fractional minutes.
		 */
		*tptr -= minutes * F_PER_M;
		eer_incs(n + EER_0PTIME,minutes);
		/* Assumes 0PTIME/1PTIME/2PTIME are contiguous */
	    }
	}
    }

    oldplaymask = playmask;
    return(&time_plgame[0]);
}


U32 *
eer_start(player)
unsigned int player;
{
#if SIMUL
    return(eer_play(oldplaymask | (1 << player)));	/* Turn ON plyr	*/
#else
    return eer_play(player);
#endif
}


U32 *
eer_stop(player)
unsigned int player;
{
#if SIMUL
    return(eer_play(oldplaymask & ~(1 << player)));	/* Turn OFF plyr */
#else
    return eer_play(0);
#endif
}



/* :::::: End the game either an original or a continuation game...	*/
/*  NOTE: the CONTIN flag is used as an INDEX to the 'bin' of the time.	*/
/*	In SIMUL games, it typically is 0 or 1 (NEW or CONTINUE).	*/
/*									*/
/*  The TANK convention for continue is:				*/
/*	0 = new game, (confusion with 2, below)				*/
/*	1 = paid-for continue						*/
/*	2 = continued for free (because he won the last one?)		*/

void
eer_end(player,contflag)
unsigned int player,contflag;
{
    m_uint mins;
    U32 ttime;

    eer_stop(player);			/* stop AND UPDATE the clocks	*/

    /* ::::: Count the current game as a NEW or CONTINUE or FREE  game	*/
    /* Here we need to protect against game programmers who refuse to read
     * os_proto.h.
     */
#ifndef MAX_CONTFLAG
#ifdef EER_FREEGMS
#define MAX_CONTFLAG (EER_FREEGMS-EER_NEWCOINS)
#endif
#endif
#ifndef MAX_CONTFLAG
#ifdef EER_CONTCOINS
#define MAX_CONTFLAG (EER_CONTCOINS-EER_NEWCOINS)
#endif
#endif
#ifndef MAX_CONTFLAG
#define MAX_CONTFLAG (0)
#endif

    if ( contflag > MAX_CONTFLAG ) contflag = MAX_CONTFLAG;

#if SST_GAME == SST_RUSH
# ifdef EER_GMS
					/* If sessions, games means ?	*/
	eer_incs(EER_GMS,1);		/* .. count it here??		*/
# endif
#endif

#ifdef EER_NEWCOINS
    if(player != N_PLAYERS)		/* Not a session end...		*/
    {
	eer_incs( (contflag + EER_NEWCOINS),1);	/* count a game...	*/
	/* Assumes NEWCOINS/CONTCOINS/FREEGMS are contiguous			*/
    }
#endif
#if EER_SESSIONS
    else	/* Ending a SESSION so count it and write all timers	*/
    {
	eer_incs(EER_SESSIONS,1);	/* count the end of a session	*/
    }
#endif
    updtimer = UPDTIM;		/* .. force update too..	*/

    /* Now find out how long this player's been playing...		*/
    /* (NOTE: the PLAYER could be a SESSION)				*/
    ttime = (time_residue[player] += time_eachpl[player]);
    if (ttime > F_PER_M)			/* Extract minutes...	*/
    {
	mins = ul_over_us(ttime,F_PER_M);
	time_residue[player] = ttime - (mins * F_PER_M);

#ifdef EER_SESTIM
    	if ( player == N_PLAYERS )	/* ending the session	*/
	{
	    eer_incs(EER_SESTIM,mins);
	}
	else	/* Add the player's time to NEW or CONTINUE time...		*/
#endif
#ifdef EER_NEWTIM
	{
	    eer_incs(contflag + EER_NEWTIM,mins);
	    /* Assumes NEWTIM/CONTTIM are contiguous */
	}
#endif
    }

    ttime = time_eachpl[player];	/* Get last game's time	*/
    time_eachpl[player] = 0;		/* Clean his clock	*/

    /* Record the time in the appropriate HISTOGRAM			*/

#if SESSIONS
    if( player == N_PLAYERS )
    {
	ttime = ul_over_us(ttime,F_PER_M);	/* Convert FRAMES to MINS */
#ifndef TANK
	/* TANK is counting sessions by games not time (in st_quit_session)	*/
#ifdef HIST_M_SESTIM
	eer_tally_hist(HIST_M_SESTIM,ttime);	/* .. and count		*/
#endif /* HIST_M_SESTIM */
/* Below is for backward compatibility, slated for demolition Jan '97 MEA */
#ifdef HIST_SESSION
	eer_tally_hist(HIST_SESSION,ttime);	/* .. and count		*/
#endif /* HIST_SESSION */

#endif /* TANK */
    }
#endif /* SESSIONS */
#ifdef HIST_NEWGAME
#if SESSIONS
    else
#endif /* SESSIONS */
    {
	ttime = ul_over_us(ttime,F_PER_S);	/* Convert FRAMES to SECS */
	eer_tally_hist( contflag + HIST_NEWGAME,ttime);
	/* assumes HIST_NEWGAME/HIST_CONTGAME/HIST_FREEGMS are contiguous */
    }
#endif /* HIST_NEWGAME */
}

#ifdef SPACE

StartWaveTimer()
{
    waveTimer = eer_rtc;
}


StopWaveTimer(waveType)
U16	waveType;
{
    U32	ttime;
    U16	mins;

    ttime = eer_rtc - waveTimer;		/* get frame delta	*/

    ttime = (waveResidue[waveType] += ttime);

    if (ttime > F_PER_M)
    {
	mins = ul_over_us(ttime,F_PER_M);
	waveResidue[waveType] = ttime - (mins * F_PER_M);

	eer_incs(waveType + EER_SOLOTIME,mins);	/* Bump wave time	*/
	/* Assumes PTIME/GTIME/STIME are contiguous */
    }

    ttime = ul_over_us(ttime,F_PER_S);		/* Convert FRAMES to SECS */
    eer_tally_hist(waveType + HIST_SOLOGAME,ttime);
}

#endif

static	const char barstr[] =
	     {
	     AN_SQUARE,					/* full first	*/
#ifdef AN_BARG
	     AN_BARG + 1,AN_BARG + 2,AN_BARG + 3, /* least partial...	*/
	     AN_BARG + 4,AN_BARG + 5,AN_BARG + 6,
	     AN_BARG + 7,			/* to most partial	*/
#else
#ifdef AN_HALFSQ
	     AN_HALFSQ,
#endif
#endif
	     0					/* terminate string	*/
	     };

struct hist_menu {
    char *label;
    int	(*call)(const struct menu_d *);
    int tabnum;
};

#if HIST_TABLES
/*		show_hist(smp)
 *	display one histogram, as specified by smp (selected
 *	menu pointer). This is the primitive to build a
 *	menu of histograms from.
 */

PRIVATE int
show_hist(smp)
const struct menu_d *smp;
{
    char buf[AN_VIS_COL_MAX],*s; 	/* Screen-line buffer		*/
    int	median,max,total,titlewide,bin,val;
    int	tblndx;
    const struct hist_menu *hmp = (const struct hist_menu *)smp;

    tblndx = hmp->tabnum;

    ExitInst(INSTR_PAL);
    titlewide = eer_hist_title(tblndx,buf,sizeof(buf),&max,&median,barstr);
    /* Write the title */

    for(bin = 0,total = 0; /* break when done */ ; ++bin)
    {
/* "MAGIC" -3 added to kluge for wrap-around bars. I do not know
 * when the bug was introduced, but this area is slated for major
 * re-modeling soon, so I'll live with the kluge. 17JUN95 MEA.
 */
	val = eer_hist_line(bin,buf,sizeof(buf)-3);
	if (val < 0)				/* DONE??	*/
	    break;

	txt_str(2,2 + bin,buf,(bin == median ? RED_PAL : YEL_PAL));

	total += val;		/* Accumulate the totals	*/
    }

    bin += 3;	/* become a line number				*/

    if ( median >= 0 ) {
	(void)eer_hist_line(median,buf,sizeof(buf));	/* Get median	*/

	s = strchr(buf,':');		/* See if it's a median type..	*/
	if (s)				/* YEP! it's meaningful		*/
	{
	    *s = '\0';
	    txt_str(15,bin,"MEDIAN:",RED_PAL);
	    txt_str(22,bin,buf,GRN_PAL);
	}
    }

    txt_str(3,bin,"TOTAL:",GRN_PAL);		/* ALWAYS total		*/
    txt_decnum(9,bin,total,5,LJ_NF,GRN_PAL);

    while ( (ctl_read_sw(SW_NEXT) & SW_NEXT) == 0 ) prc_delay0();
    return 0;
}

#define REGRET_TIME (30)	/* Tenths of a second */

PRIVATE int
clear_hist(smp)
struct menu_d *smp;
{
    int table,timer;
    unsigned long ctls;

    timer = REGRET_TIME;

    ExitInst(INSTR_PAL);
    while ( timer >= 0) {
	ctls = ctl_read_sw(SW_NEXT);
	if ( ctls & SW_NEXT ) return 0;
	if ( ctls & SW_ACTION ) {
	    /* action held, run timer */
	    txt_clr_str(-1,5,t_msg_actionh,INSTR_PAL);
	    txt_clr_str(-1,7,"SECONDS TO CLEAR HISTOGRAMS",INSTR_PAL);

	    txt_str(-1,5,"YOU HAVE",INSTR_PAL);
	    txt_decnum((AN_VIS_COL-3)/2,6,timer/10,1,RJ_BF,INSTR_PAL);
	    txt_cstr(".",INSTR_PAL);
	    txt_cdecnum((timer%10),1,RJ_BF,INSTR_PAL);
	    txt_str(-1,7,"SECONDS TO CHANGE YOUR MIND",INSTR_PAL);
	    --timer;
	} else {
	    /* action released, say what to do */
	    txt_clr_str(-1,5,"YOU HAVE",INSTR_PAL);
	    txt_clr_str(-1,7,"SECONDS TO CHANGE YOUR MIND",INSTR_PAL);
	    timer = REGRET_TIME;
	    txt_decnum((AN_VIS_COL-3)/2,6,timer/10,1,RJ_BF,INSTR_PAL);
	    txt_cstr(".",INSTR_PAL);
	    txt_cdecnum((timer%10),1,RJ_BF,INSTR_PAL);
	    txt_str(-1,5,t_msg_actionh,INSTR_PAL);
	    txt_str(-1,7,"SECONDS TO CLEAR HISTOGRAMS",INSTR_PAL);
	}
	prc_delay(6);
    }

    txt_str(-1,9,"CLEARING ALL",RED_PAL|AN_BIG_SET);
    txt_str(-1,11,"HISTOGRAMS",RED_PAL|AN_BIG_SET);

#ifdef HIST_TABLES
    for (table=0; table<HIST_TABLES; ++table)
	(void) eer_clrhist(table);
#endif

    prc_delay0();
    timer = 90;
    while ( (--timer > 0 || eer_busy() > 1)	/* time left	*/
    && (ctl_read_sw(SW_NEXT) & SW_NEXT) == 0)	/* no press yet	*/
	prc_delay0();

    return 0;
}
#endif

#if HIST_TABLES
/*		show_hist_group( start, end )
 *	Creates and "runs" a menu to display a group of
 *	histograms. A normal game migh call this with
 *	start == 0 and end == HIST_TABLES-1, but Hero
 *	needs more than one screen worth. Returns number
 *	of last histogram displayed (in case it is
 *	less than <end>)
 */
#ifndef HIST_ROWS_MAX
#define HIST_ROWS_MAX (AN_VIS_ROW_MAX-8)
#endif
#ifndef HIST_ROWS
#define HIST_ROWS (AN_VIS_ROW-8)
#endif

/*	Following kluge included solely to get proper instructions
 *	on multi-page histogram screen. When st_menu() is modified
 *	to be less aggressive about drawing histograms, and passes
 *	menu and "reason" to coroutines (future enhancement),
 *	this will change radically or go away altogether. The whole
 *	file stats.c is overdue for a re-write, so... MEA 22NOV95
 */
static int last_screen;

static int show_hist_coroutine( smp )
const struct menu_d *smp;
{
    int bottom;
    struct menu_d dummy;

    if ( smp != 0 ) return 0;

    dummy.mn_label = "HISTOGRAMS";
    dummy.mn_call = 0;

    if ( last_screen ) {
	bottom = st_frame( &dummy, TITLE_PAL, INSTR_PAL, 0);
    } else {
	bottom = st_frame( &dummy, TITLE_PAL, INSTR_PAL, STF_NOEXIT);
	bottom = st_insn(bottom,"For more histograms",t_msg_next,INSTR_PAL);
    }
    bottom = st_insn(bottom, "To show Histogram,", t_msg_action, INSTR_PAL);
    bottom = st_insn(bottom, "To select Histogram,", t_msg_control, INSTR_PAL);
    return 0;
}

static int show_hist_group(start, end, clear)
int start, end, clear;
{
    m_int tblndx,midx;
    char buf[AN_VIS_COL_MAX*HIST_ROWS_MAX],*s;		/* Screen-line buffer		*/
    int	titlewide;

    struct hist_menu hmenu[HIST_ROWS_MAX+3];

    s = buf;

    hmenu[0].label = "HISTOGRAMS";
    hmenu[0].call = show_hist_coroutine;

    tblndx = start;
    for (midx=1; midx <= HIST_ROWS; )
    {

	titlewide = eer_hist_title(tblndx,s,AN_VIS_COL-2,0,0,barstr);
	hmenu[midx].label = s;
	if ( titlewide <= 0 ) {
	    strcpy(s,"BOTCHED");
	}
	while (*s) ++s;		/* skip to end */
	++s;			/* and then some */
	hmenu[midx].call = show_hist;
	hmenu[midx].tabnum = tblndx;
	++midx;
	if ( ++tblndx > end ) break;
    }
    if ( clear && ((midx <= HIST_ROWS) || (tblndx > end)) ) {
	/* Add a menu line to clear all histograms
	 * This may appear in the normally blank line just
	 * above the instructions, if the last (or only)
	 * page of histograms exactly fit.
	*/
	hmenu[midx].label = "\nCLEAR ALL HISTOGRAMS";
	/* If we are wedging this line in to the normally
	 * blank line _after_ the list of histograms, leave
	 * out the blank line before it by skipping over
	 * the '\n'.
	 */
	if ( midx > HIST_ROWS ) hmenu[midx].label += 1;
	hmenu[midx].call = clear_hist;
	hmenu[midx].tabnum = -1;
	++midx;
    }
    hmenu[midx].label = 0;
    hmenu[midx].call = 0;

    last_screen =  ( tblndx > end );
    st_menu((struct menu_d*)hmenu,sizeof(hmenu[0]),MNORMAL_PAL,0);
    return tblndx;
}
#endif

PRIVATE void
ShowHist(clearFlag)
int	clearFlag;
{
#if HIST_TABLES
#if (1)
    int start;

    start = 0;
    do {
	start = show_hist_group(start, HIST_TABLES-1, clearFlag);
    } while ( start < HIST_TABLES-1 );
#else
    m_int tblndx,midx;
    char buf[AN_VIS_COL*HIST_TABLES],*s;		/* Screen-line buffer		*/
    int	titlewide;

    struct hist_menu hmenu[HIST_TABLES+3];

#ifdef TANK
    tblndx=HIST_SESSION;
#else
    tblndx = 0;
#endif
    s = buf;

    hmenu[0].label = "HISTOGRAMS";
    hmenu[0].call = 0;

    for (midx=1; midx <= HIST_TABLES; ++midx)
    {

	titlewide = eer_hist_title(tblndx,s,AN_VIS_COL-2,0,0,barstr);
	hmenu[midx].label = s;
	while (*s) ++s;		/* skip to end */
	++s;			/* and then some */
	hmenu[midx].call = show_hist;
	hmenu[midx].tabnum = tblndx;
	if ( ++tblndx >= HIST_TABLES ) break;
    }
    if ( clearFlag ) {
	/* add a menu line to clear all histograms */
	hmenu[++midx].label = "\nCLEAR ALL HISTOGRAMS";
	hmenu[midx].call = clear_hist;
	hmenu[midx].tabnum = -1;
    }
    hmenu[++midx].label = 0;
    hmenu[midx].call = 0;

    st_menu((struct menu_d*)hmenu,sizeof(hmenu[0]),MNORMAL_PAL,0);
#endif /* (1), old way saved for reference */
#endif		/* don't bother with SHOW_HIST if none defined		*/
}

#if (N_PLAYERS > 2)
U32
active_mins()
{
    unsigned long time;
    short i,j;
    time = 0;

    for ( i = (N_PLAYERS - 1) ; i >= 0 ; --i)
    {
	/* Kluge a long mul */
	for ( j = i ; j >= 0 ; --j ) time += eer_gets(EER_0PTIME + 1 + i);
    }
    return time;
}
#endif


/*		General Statistic Display
*/

/*		avgtim()
*	Returns the average time per coin.
*/
U32
avgtim()
{
    long time,coins,avg;
    int i;

    coins = cn_total_coins();
    if ( coins == 0 )
	return 0;

    time = eer_gets(EER_1PTIME);
    for ( i = 1 ; i < N_PLAYERS ; ++i ) {
	time +=  (i+1) * eer_gets(EER_1PTIME+i);
    }

    avg = time * 60;	/* time in seconds */
    return(ul_over_us(avg,coins));
}

#ifdef EER_CUM_COINS
#  define SUPPORTS_CUMULATIVE (1)
#else
#  ifdef EER_CUM_SERVICE
#    error /* CUM_COINS without CUM_SERVICE is not supported */
#  else
#    define SUPPORTS_CUMULATIVE (0)
#  endif
#endif

/* ::::: Shows the TOTAL COINS, AVG TIME, and PCT PLAY :::::::::	*/
/*		.. requires 3 display lines				*/
PRIVATE void
sho_avgtim(hpos,vpos)
short hpos,vpos;
		/* prints total coins and average time per coin in seconds */
{
    long	coins,usetim,tottim;
    short	useage;
    int		i;
#if (COJAG_GAME == COJAG_FISH)
#ifdef EER_GMS
    long games = eer_gets(EER_GMS);
#else
    long games = 0;
#endif
#endif
#ifdef SPACE
    usetim = eer_gets(EER_PTIME);			/* total use time */
#endif
    for(usetim = 0, i = 0; i < N_PLAYERS; ++i)	/* Get player times	*/
	usetim += eer_gets(EER_0PTIME + 1 + i);

    tottim = usetim + eer_gets(EER_0PTIME);		/* overall time */

    usetim = (U16)usetim * (U16)100;				/* 100 % */

    if (tottim != 0)						/* carefull */
	useage = usetim / tottim;
    else
	useage = 0;

    txt_str(hpos,vpos+2,"Percentage Play: ",YEL_PAL);
    txt_cdecnum(useage,11,LJ_NF,YEL_PAL);

    txt_str(hpos+3,vpos,"Total Coins : ",YEL_PAL);
    txt_cdecnum(coins = cn_total_coins(),11,LJ_NF,YEL_PAL);
#if SUPPORTS_CUMULATIVE
    {
	long cum_coins = cn_cum_coins(0, 0, 0);
	if ( (coins != cum_coins) && (debug_mode & GUTS_OPT_DEVEL) ) {
	    U32 old_svc, new_coin, new_svc, old_coin, tot;
	    int pal = MNORMAL_PAL;
	    old_coin = cn_cum_coins( &old_svc, &new_coin, &new_svc);
	    tot = old_coin+new_coin+old_svc+new_svc;
	    if ( tot != cum_coins ) pal = ERROR_PAL;
	    txt_cstr(" (",pal);
	    txt_cdecnum(cum_coins,11,LJ_NF,pal);
	    if ( tot != cum_coins ) {
		txt_cstr(" != ",pal);
		txt_cdecnum(old_coin,11,LJ_NF,pal);
		txt_cstr("+",pal);
		txt_cdecnum(old_svc,11,LJ_NF,pal);
		txt_cstr("+",pal);
		txt_cdecnum(new_coin,11,LJ_NF,pal);
		txt_cstr("+",pal);
		txt_cdecnum(new_svc,11,LJ_NF,pal);
	    }
	    txt_cstr(")",pal);
	}
    }
#endif
    if ( coins != 0 ) {
	txt_str(hpos+1,vpos+1,"AVG TIME/COIN : ",YEL_PAL);	/* Advances COL	*/
	timenum(avgtim(),6);
    }
#if (COJAG_GAME == COJAG_FISH)
    if ( games ) {
	long time;
	if ( coins == 0 ) txt_str(hpos+1,vpos+1,"AVG TIME/GAME : ",YEL_PAL);
	else txt_cstr(" /GAME : ",YEL_PAL);
	time = eer_gets(EER_1PTIME) + 2*eer_gets(EER_2PTIME);
	timenum((time*60)/games,6);
    }
#endif
}


/* ::::: Display the counters, average times, etc...	::::::::;	*/

typedef struct stat_label { char *stat_nm;  U8 stat_ndx; } STAT_STRUCT;

void
ShowStats(clearFlag)
int	clearFlag;
{
    int	update,i,bottom;
    static const STAT_STRUCT statNames[] =
    {
#ifdef B_BILL
	/* Guess that this means The WMS harness, so use
	 * their names.
	 */
	{ "Coin1 Coins",EER_CC0 },
	{ "Coin2 Coins",EER_CC1 },
#ifdef EER_CC2
	{ "Coin3 Coins",EER_CC2 },
#endif
#ifdef EER_CC3
	{ "Coin4 Coins",EER_CC3 },
#endif
#ifdef EER_CC4
	{ "Bill value",EER_CC4 },
#endif
#else	/* Atari names, two coin inputs */
	{ "Left Coins",EER_CC0 },
	{ "Right Coins",EER_CC1 },
#endif
	{ "Aux Coins",EER_CCX },
	{ "Idle Mins",EER_0PTIME },	/* maintained by EER_PLAY	*/
#if (SST_GAME & SST_HOCKEY)
	{ "1 Player Mins",EER_1PTIME },
	{ "2 Player Mins",EER_2PTIME },
	{ "3 Player Mins",EER_3PTIME },
	{ "4 Player Mins",EER_4PTIME },
	{ "New Games",EER_NEWCOINS },
	{ " Kid Records",EER_KIDRECORDS },
	{ " Team Records",EER_TEAMRECORDS },
	{ "Continues",EER_CONTCOINS },
	{ " Discounted Conts",EER_DISGMS },
	{ " Free Conts",EER_FREEGMS },
	{ "1-Player Periods",EER_1PPERIODS },
	{ "2-Player Periods",EER_2PPERIODS },
	{ "3-Player Periods",EER_3PPERIODS },
	{ "4-Player Periods",EER_4PPERIODS },
	{ "Total Periods",EER_PERIODS },
	{ "1-Player Games",EER_1PGAMS },
	{ "2-Player Games",EER_2PGAMS },
	{ "3-Player Games",EER_3PGAMS },
	{ "4-Player Games",EER_4PGAMS },
	{ "Total Games",EER_GMS },
	{ "Overtime Periods",EER_OVERTIME },
	{ "Team Play",EER_TEAMPLAY },
	{ "Head to Head",EER_HEADTOHEAD },
	{ "Fights",EER_FIGHTS },
#else
#ifdef EER_2PTIME
	{ "1 Player Mins",EER_1PTIME },
	{ "2 Player Mins",EER_2PTIME },
#else
	{ "Play Mins",EER_1PTIME },
#endif
#ifdef EER_SESTIM
	{ "Session Mins",EER_SESTIM },
#endif
#ifdef EER_NEWCOINS
	{ "New Games",EER_NEWCOINS },
#endif
#ifdef EER_CONTCOINS
	{ "Cont Games",EER_CONTCOINS },
#endif
#ifdef EER_DRAWS
#if (COJAG_GAME == COJAG_FISH)
	{ "Draw Waves",EER_DRAWS },
#else
	{ "Draw Rounds",EER_DRAWS },
#endif
#endif
#ifdef EER_FREEGMS
#if 1	/* Rush & Speed */
#define WANTS_EARNED_PCT (1)
	{ "Earned Games",EER_FREEGMS },
#else
	{ "Games Won",EER_FREEGMS },
#endif
#endif
#ifdef EER_GMS
	{ "Total Games",EER_GMS },
#endif
#ifdef EER_MIRROR
	{ "Mirror Games",EER_MIRROR },
#endif
#ifdef EER_LINKED
	{ "Linked Games",EER_LINKED },
#endif

#ifdef EER_LEFT_STARTS
	{ "Left Starts",EER_LEFT_STARTS },
#endif
#ifdef EER_LEFT_CONTS
	{ "Left Continues",EER_LEFT_CONTS },
#endif
#ifdef EER_RIGHT_STARTS
	{ "Right Starts",EER_RIGHT_STARTS },
#endif
#ifdef EER_RIGHT_CONTS
	{ "Right Continues",EER_RIGHT_CONTS },
#endif

#ifdef EER_SESSIONS
	{ "Mach. Sessions",EER_SESSIONS },
#endif
#ifdef EER_1PGAMS
	{ "1-Player Games",EER_1PGAMS },
#endif
#ifdef EER_2PGAMS
	{ "2-Player Games",EER_2PGAMS },
#endif
#ifdef EER_CHGAMS
	{ "Challenge Games",EER_CHGAMS },
#endif
#ifdef EER_QZGAMS
	{ "Quiz Games",EER_QZGAMS },
#endif
#ifdef EER_JNGAMS
	{ "Join-ins",EER_JNGAMS },
#endif

#if (0)
	{ "3-Player Games",EER_3PGAMS },
	{ "4-Player Games",EER_4PGAMS },
	{ "5-Player Games",EER_5PGAMS },
	{ "6-Player Games",EER_6PGAMS },
#endif
#ifdef EER_SECRET0
	{ "Chow Palace",EER_SECRET0 },
#endif
#ifdef EER_SECRET1
	{ "Head Quarters",EER_SECRET1 },
#endif
#ifdef EER_SECRET2
	{ "Shake Your Booda",EER_SECRET2 },
#endif
#ifdef EER_SECRET3
	{ "Egg-cellent",EER_SECRET3 },
#endif
#ifdef EER_SECRET4
	{ "CRY Mode",EER_SECRET4 },
#endif

#endif
#ifdef EER_ERRORS
# if SST_GAME
	{ "BRAM read errs",	EER_ERRORS },
# else
	{ "Error Count",	EER_ERRORS },
# endif
#endif
	{ 0,	-1 }
    };
#define	NUM_STAT_NAMES ((sizeof(statNames)/sizeof(statNames[0]))-1)
    static const struct menu_d myname = { "STATISTICS",0};

    update = TRUE;
    bottom = AN_VIS_ROW-2;
    while (TRUE)
    {
	prc_delay0();

	if (clearFlag			/* ::::: CLEAR THE STATS! :::::	*/
	&& ( (ctl_read_sw(0)&SW_NEXT) && (ctl_read_sw(0)&SW_ACTION) ) )
	{
	    while (ctl_read_sw(0) & SW_NEXT)	/* wait till release	*/
		prc_delay0();

	    /* We used to clear everything from EER_CCO to EER_CNOPT,
	     * but it makes _much_ more sense to clear all, and only
	     * the stats that appear on this page.
	     */
#if SUPPORTS_CUMULATIVE
	    /* If this page includes the coin stats, call special
	     * routine in coin stuff, to add current totals to
	     * cumulative totals.
	     */
	    for ( i = 0 ; statNames[i].stat_nm ; ++i ) {
		int sidx;
		sidx = statNames[i].stat_ndx;
		if ( sidx >= EER_CC0 && sidx <= EER_CCX ) {
		    cn_clear_coins();	/* Save coins before clearing */
		    break;
		}
	    }
#endif
	    for ( i = 0 ; statNames[i].stat_nm ; ++i ) {
		int sidx;
		sidx = statNames[i].stat_ndx;
		eer_puts(sidx,0);
	    }

#ifdef	EER_RAND
	    eer_puts(EER_RAND,eer_rtc);	/* seed power-on random seed	*/
#endif
	    clearFlag = FALSE;		/* can't clear more than once	*/
	    vid_clear();
	    bottom = st_frame(&myname,TITLE_PAL,INSTR_PAL,STF_NOEXIT);
	    update = TRUE;		/* need to update screen	*/
	}

	if ((ctl_read_sw(SW_NEXT) & SW_NEXT)
	&& (ctl_read_sw(0) & SW_ACTION) == 0)
	    break;

	if (update)			/* Need to draw the stats?	*/
	{
	    int	row,width;
	    U16 stat;

	    row = 3;
	    width = 0;
	    for (i=0; i<NUM_STAT_NAMES; ++i)
	    {
		int w;
		w = strlen(statNames[i].stat_nm);
		if ( width < w ) width = w;
	    }
	    for (i=0; i<NUM_STAT_NAMES; ++row,++i)
	    {
		int num = statNames[i].stat_ndx;

		txt_str(3,row,statNames[i].stat_nm,GRY_PAL);
		txt_str(width+3,row,":",GRY_PAL);
		txt_cdecnum(stat = eer_gets(num),11,LJ_NF,GRY_PAL);
#ifdef EER_ERRORS
		if ( num == EER_ERRORS && stat > 75 ) {
		    /* Pay special attention to EEROM ERROR	*/
		    txt_str(27,12,"EEPROM ERROR",RED_PAL);
		}
#endif
	    }

	    sho_avgtim(3,++row);	/* Requires 3 lines of display	*/
#if WANTS_EARNED_PCT
#ifdef EER_FREEGMS
#ifdef EER_GMS
	    {
		long free,tot;
		tot = eer_gets(EER_GMS);
		free = eer_gets(EER_FREEGMS);
		if ( (tot > 0) && (free >= 0) ) {
		    /* Both successfully reead, and tot != 0 */
		    row+=3;
		    txt_str(3,row,"Earned Ratio   : ",YEL_PAL);
		    free = (free*100)/tot;
		    txt_cdecnum(free,3,LJ_NF,YEL_PAL);
		    txt_cstr("%",YEL_PAL);
		}
	    }
#endif /* EER_GMS */
#endif /* EER_FREEGMS */
#endif /* WANTS_EARNED_PCT */
#if (0)
	    txt_str(-1,AN_VIS_ROW-3,t_msg_next,INSTR_PAL);
	    txt_clr_str(-1,AN_VIS_ROW-2,t_msg_ret_menu,INSTR_PAL);
	    txt_str(-1,AN_VIS_ROW-2,t_more_stats,INSTR_PAL);

	    if (clearFlag)
	    {
		txt_str(-1,AN_VIS_ROW-5,t_msg_clear,INSTR_PAL);
		txt_str(-1,AN_VIS_ROW-4,"to clear ALL statistics",INSTR_PAL);
	    }
#else
	    bottom = st_insn(bottom,t_more_stats,t_msg_next,INSTR_PAL);
	    if ( clearFlag ) {
		bottom = st_insn(bottom,"To clear these counters,",t_msg_clear,INSTR_PAL);
	    }
#endif
	    update = FALSE;
	}
    }
    txt_clr_str(27,12,"EEPROM ERROR",RED_PAL);
}

#if (COJAG_GAME&COJAG_AREA51)
#ifdef EER_SROOM7
void
ShowMaxForceStats(clearFlag)
int	clearFlag;
{
    int	update,i,bottom;
    static const STAT_STRUCT MFstatNames[] =
    {
#ifdef EER_MISSION1
	{ "Mission 1",EER_MISSION1 },
#endif
#ifdef EER_MISSION2
	{ "Mission 2",EER_MISSION2 },
#endif
#ifdef EER_MISSION3
	{ "Mission 3",EER_MISSION3 },
#endif

#ifdef EER_SROOM1
	{ "Secret Room 1",EER_SROOM1 },
#endif
#ifdef EER_SROOM2
	{ "Secret Room 2",EER_SROOM2 },
#endif
#ifdef EER_SROOM3
	{ "Secret Room 3",EER_SROOM3 },
#endif
#ifdef EER_SROOM4
	{ "Secret Room 4",EER_SROOM4 },
#endif
#ifdef EER_SROOM5
	{ "Secret Room 5",EER_SROOM5 },
#endif
#ifdef EER_SROOM6
	{ "Secret Room 6",EER_SROOM6 },
#endif
#ifdef EER_SROOM7
	{ "Secret Room 7",EER_SROOM7 },
#endif
#ifdef EER_SROOM8
	{ "Secret Room 8",EER_SROOM8 },
#endif
#ifdef EER_SROOM9
	{ "Secret Room 9",EER_SROOM9 },
#endif
#ifdef EER_SROOM10
	{ "Secret Room 10",EER_SROOM10 },
#endif
#ifdef EER_SROOM11
	{ "Secret Room 11",EER_SROOM11 },
#endif
#ifdef EER_SROOM12
	{ "Secret Room 12",EER_SROOM12 },
#endif
#ifdef EER_SROOM13
	{ "Secret Room 13",EER_SROOM13 },
#endif
#ifdef EER_SROOM14
	{ "Secret Room 14",EER_SROOM14 },
#endif
#ifdef EER_STRMERR
        { "Streaming Count",EER_STRMERR },
#endif
#ifdef EER_RELOADERR
        { "Reload Count",EER_RELOADERR },
#endif
	{ 0,	-1 }
    };
#define	NUM_MFSTAT_NAMES ((sizeof(MFstatNames)/sizeof(MFstatNames[0]))-1)
    static const struct menu_d myname = { "STATISTICS",0};

    update = TRUE;
    bottom = AN_VIS_ROW-2;
    while (TRUE)
    {
	prc_delay0();

	if (clearFlag			/* ::::: CLEAR THE STATS! :::::	*/
	&& ( (ctl_read_sw(0)&SW_NEXT) && (ctl_read_sw(0)&SW_ACTION) ))
	{
	    while (ctl_read_sw(0) & SW_NEXT)	/* wait till release	*/
		prc_delay0();

	    /* It makes _much_ more sense to clear all, and only
	     * the stats that appear on this page.
	     */
	    for ( i = 0 ; MFstatNames[i].stat_nm ; ++i ) {
		int sidx;
		sidx = MFstatNames[i].stat_ndx;
		eer_puts(sidx,0);
	    }

	    clearFlag = FALSE;		/* can't clear more than once	*/
	    vid_clear();
	    bottom = st_frame(&myname,TITLE_PAL,INSTR_PAL,STF_NOEXIT);
	    update = TRUE;		/* need to update screen	*/
	}

	if ((ctl_read_sw(SW_NEXT) & SW_NEXT)
	&& (ctl_read_sw(0) & SW_ACTION) == 0)
	    break;

	if (update)			/* Need to draw the stats?	*/
	{
	    int	row,width;
	    U16 stat;

	    vid_clear();
	    bottom = st_frame(&myname,TITLE_PAL,INSTR_PAL,0);

	    row = 3;
	    width = 0;
	    for (i=0; i<NUM_MFSTAT_NAMES; ++i)
	    {
		int w;
		w = strlen(MFstatNames[i].stat_nm);
		if ( width < w ) width = w;
	    }
	    for (i=0; i<NUM_MFSTAT_NAMES; ++row,++i)
	    {
		int num = MFstatNames[i].stat_ndx;

		txt_str(3,row,MFstatNames[i].stat_nm,GRY_PAL);
		txt_str(width+3,row,":",GRY_PAL);
		txt_cdecnum(stat = eer_gets(num),11,LJ_NF,GRY_PAL);
	    }

	    bottom = st_insn(bottom,t_more_stats,t_msg_next,INSTR_PAL);
	    if ( clearFlag ) {
		bottom = st_insn(bottom,"To clear these counters,",t_msg_clear,INSTR_PAL);
	    }
	    update = FALSE;
	}
    }
}
#endif /* defined EER_SROOM7, distinguish AREA51 from Max Force */
#endif /* AREA51, really Max Force */

PRIVATE void
ShowSysErrs(clearFlag)
int	clearFlag;
{
    int	update,i;
    static const STAT_STRUCT sysErrNames[] =
    {
	{ "WATCH DOG RESETS",		EER_RESET },
#ifdef EER_WR_RO
	{ "WRITE TO RO MEM",		EER_WR_RO },
#endif
#ifdef EER_RD_NEM
	{ "READ FROM N.E. MEM",		EER_RD_NEM },
#endif
#ifdef EER_WR_NEM
	{ "WRITE TO N.E. MEM",		EER_WR_NEM },
#endif
#ifdef EER_ADRERR_R
	{ "ADDR ALIGN ON RD", 		EER_ADRERR_R },
#endif
#ifdef EER_ADRERR_W
	{ "ADDR ALIGN ON WR", 		EER_ADRERR_W },
#endif
#ifdef EER_BUSERR_I
	{ "BUS ERR ON I FETCH", 	EER_BUSERR_I },
#endif
#ifdef EER_BUSERR_D
	{ "BUS ERR ON D FETCH", 	EER_BUSERR_D },
#endif
#ifdef EER_SYSCALL
	{ "SYSCALLS",			EER_SYSCALL },
#endif
#ifdef EER_BREAKP
	{ "BREAKPOINTS",		EER_BREAKP },
#endif
#ifdef EER_COPROC
	{ "COPROC UNUSABLE",		EER_COPROC },
#endif
#ifdef EER_ARITH
	{ "ARITHMETIC OVERFLOW",	EER_ARITH },
#endif
#ifdef EER_TRAP
	{ "TRAP EXCEPTIONS",		EER_TRAP },
#endif
#ifdef EER_RESERV
	{ "RESERVED INSTRUCTIONS",	EER_RESERV },
#endif
#ifdef EER_FLOAT
	{ "F. POINT EXCEPTIONS", 	EER_FLOAT },
#endif
#ifdef EER_UNDEF
	{ "UNDEFINED EXECPTIONS",	EER_UNDEF },
#endif
#ifdef EER_OVERFL
	{ "OVERFLOW CHECKS",		EER_OVERFL },
#endif
#ifdef EER_RANGE
	{ "RANGE CHECKS",		EER_RANGE },
#endif
#ifdef EER_UHINT
	{ "UNHANDLED INTERRUPTS",	EER_UHINT },
#endif
#ifdef EER_MOVERFL
	{ "MULTIPLY OVERFLOWS",		EER_MOVERFL },
#endif
#ifdef EER_BUSERR
	{ "BUS ERRORS",			EER_BUSERR },
#endif
#ifdef EER_ADRERR
	{ "ADDRESS ERRORS",		EER_ADRERR },
#endif
#ifdef EER_ILGINS
	{ "ILLEGAL INSTRUCTION",	EER_ILGINS },
#endif
#ifdef EER_DVDBY0
	{ "DIVIDE BY ZERO",		EER_DVDBY0 },
#endif
#ifdef EER_CHKINS
	{ "CHK INSTRUCTION",		EER_CHKINS },
#endif
#ifdef EER_TRPVINS
	{ "TRAPV INSTRUCTION",		EER_TRPVINS },
#endif
#ifdef EER_PRVVIOL
	{ "PRIVELEGE VIOLATION",	EER_PRVVIOL },
#endif
#ifdef EER_SNDSLST
	{ "SOUNDS LOST",		EER_SNDSLST },
#endif
#ifdef EER_DATALST
	{ "DATA FROM CH31 LOST",	EER_DATALST },
#endif
#ifdef EER_AUDRESET
	{ "CAGE RESETS",		EER_AUDRESET },
#endif
#ifdef EER_LINKFAIL
	{ "LINK CHECKSUM ERRORS",	EER_LINKFAIL },
#endif
#ifdef EER_DSK_ERR
	{ "ANY DISK I/O ERROR",		EER_DSK_ERR },
#endif
#ifdef EER_DSK_AMNF
	{ "DISK: ADDR MARK NF",		EER_DSK_AMNF },
#endif
#ifdef EER_DSK_TK0NF
	{ "DISK: TRACK 0 NF",		EER_DSK_TK0NF },
#endif
#ifdef EER_DSK_ABORT
	{ "DISK: COMMAND ABORT",	EER_DSK_ABORT },
#endif
#ifdef EER_DSK_IDNF
	{ "DISK: SECTOR ID NF",		EER_DSK_IDNF },
#endif
#ifdef EER_DSK_UNCDTA
	{ "DISK: UNCORR DATA",		EER_DSK_UNCDTA },
#endif
#ifdef EER_DSK_TIMOUT
	{ "DISK: DVC TIMEOUT",		EER_DSK_TIMOUT },
#endif
#ifdef EER_DSK_WERR
	{ "DISK: WRITE FAULTS",		EER_DSK_WERR },
#endif
#ifdef EER_DSK_CORR
	{ "DISK: CORRECTABLE",		EER_DSK_CORR },
#endif
#ifdef EER_FSYS_USEALT
	{ "FSYS: USED ALT FILE",	EER_FSYS_USEALT },
#endif
    };
#define	NUM_SYSERR_NAMES (sizeof(sysErrNames)/sizeof(sysErrNames[0]))
    static const struct menu_d myname = { "SYSTEM ERRORS",0};

    int	values[NUM_SYSERR_NAMES];
    int longest = 0;

    update = 0;
    for (i=0; i < NUM_SYSERR_NAMES; ++i) {
	int jj;
	jj = strlen(sysErrNames[i].stat_nm);
	if (jj > longest) longest = jj;
	if ((values[i] = eer_gets(sysErrNames[i].stat_ndx))) ++update;
    }

    if (update == 0)
	return;

    while (TRUE)
    {
	prc_delay0();

	if ((ctl_read_sw(SW_NEXT) & SW_NEXT)
	&& (ctl_read_sw(0) & SW_ACTION) == 0)
	    break;

	if(   (ctl_read_sw(0) & SW_NEXT)		/* left button down?	*/
	   && (ctl_read_sw(0) & SW_ACTION)		/* right button down?	*/
	   && clearFlag			/* ::::: CLEAR THE STATS! :::::	*/
	  )
	{
	    while (ctl_read_sw(0) & SW_NEXT)	/* wait till release	*/
		prc_delay0();

	    for (i = 0; i < NUM_SYSERR_NAMES; ++i) /* clear 'em ALL!	*/
		eer_puts(i + EER_RESET,values[i] = 0);

	    clearFlag = 0;		/* can't clear more than once	*/
	    update = 1;			/* need to update screen	*/
	}

	if (update)		/* display screen text			*/
	{
	    int	row, col, bottom;

	    vid_clear();
	    bottom = st_frame(&myname,TITLE_PAL,INSTR_PAL,0);
	    row = 3;
	    col = 4;
	    for (i=0; i < NUM_SYSERR_NAMES; ++i)
	    {
		if ( values[i] ) {	/* reduce the screen clutter */
		    txt_str(col, row, sysErrNames[i].stat_nm, GRY_PAL);
		    txt_decnum(col+longest+2, row, values[i], 3, RJ_BF, YEL_PAL);
		    ++row;
		    if (row >= bottom - 2) {
			if ((update <= 2*(bottom-2-3)) && (longest+2+3+3 < AN_VIS_COL/2)) {
			    col = AN_VIS_COL/2;
			} else {
			    col += longest+2+3+3;
			}
			row = 3;
		    }
		}
	    }

#if (0)
	    if (HIST_TABLES)
	    {
		txt_str(-1,AN_VIS_ROW-3,t_msg_next,INSTR_PAL);
		txt_str(-1,AN_VIS_ROW-2,"for histograms",INSTR_PAL);
	    }
	    else
		sayopt();
#else
# ifdef HIST_TABLES
	    bottom = st_insn(bottom,
	     (HIST_TABLES ? "For histograms," : t_msg_ret_menu),
	     t_msg_next, INSTR_PAL);
# endif
#endif

	    if (clearFlag)
	    {
#if (0)
		txt_str(-1,AN_VIS_ROW-5,t_msg_clear,INSTR_PAL);
	    	txt_str(-1,AN_VIS_ROW-4,"to clear ALL statistics",INSTR_PAL);
#else
		bottom = st_insn(bottom,"To clear these counters,",t_msg_clear,INSTR_PAL);
#endif
	    }


	    update = 0;
	}
    }
}


/* ::::::::: EER_STATS is the main entry point to display histograms	*/
void
GameStats(erase)
int erase;
{
    ClearScreen();
    eer_stats(erase);
}

#ifdef EER_UREC_IDX
PRIVATE void ShowUserMsgs(int erase);
#endif

void
eer_stats(erase)
int erase;
{
    ShowStats(erase);
#if (COJAG_GAME&COJAG_AREA51)
#ifdef EER_SROOM7
    ShowMaxForceStats(erase);
#endif /* defined EER_SROOM7, distinguish AREA51 from Max Force */
#endif /* AREA51, really Max Force */
#ifdef RELIEF
    show_time_stats();
    show_plyr_stats(erase);
#endif
    ShowSysErrs(erase);
#ifdef EER_UREC_IDX
    ShowUserMsgs(erase);
#endif
    ShowHist(erase);
}

/* ::::: EER_OPT reports and allows setting of the GAME OPTIONS :::::	*/

#ifdef INCL_MENU
static const unsigned char fakemenu[] =
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
#endif

#if (COJAG_GAME == COJAG_AREA51)
#ifndef GMOPT_LIVES
#define GMOPT_LIVES (7<<7)
#endif
#ifndef GMOPT_DIFFICULTY
#define GMOPT_DIFFICULTY ((3<<1)|GMOPT_LIVES)
#endif
#ifndef GMOPT_RST_HST
#define GMOPT_RST_HST	 (1<<5)
#endif
#ifndef GMOPT_FACT_DEFAULTS
#define GMOPT_FACT_DEFAULTS (1<<6)
#endif
#endif /* (COJAG_GAME == COJAG_AREA51) */

#ifdef __HST_ENT_TYPE_DEFINED
#if GMOPT_RST_HST | GMOPT_DIFFICULTY
static void clear_hsts()
{
    int rank,table;

    struct hst_ent *zot;
    vid_clear();
    txt_str(-1,AN_VIS_ROW/2,"CLEARING HIGH SCORES",RED_PALB|AN_BIG_SET);
    for ( table = 0 ; ; ++table ) {
#ifdef HST_SERIAL_NUM
	if ( table == HST_SERIAL_NUM ) continue;
#endif
	if ( eer_hstr(0,table) == 0 ) break;
	for ( rank = 0 ; ; ++rank ) {
	    zot = eer_hstr(rank, table);
	    if ( zot == 0 ) break;
	    memset(zot,0,sizeof(*zot));
	    eer_hstw(rank, zot, table);
	}
    }
    prc_delay(30);	/* Make sure they see it */
    while ( eer_busy() > 1 ) prc_delay(1);
}
#endif /* (GMOPT_RST_HST | GMOPT_DIFFICULTY) */
#endif /* __HST_ENT_TYPE_DEFINED */

void
eer_opt(menu)		/* The MENU is a pointer to the MENU table	*/
const unsigned char *menu;
{
    U32 old_options,new_options;
#if (GMOPT_DIFFICULTY|GMOPT_RST_HST)
    int clr_flg = 0;
#endif
    if (menu == 0 && (menu = fakemenu) == 0) return;
    old_options = eer_gets(EER_GMOPT);
    new_options = DoOptions(menu,old_options,0);

#if GMOPT_RST_HST
    /* If they have an option bit to clear High scores, and
     * they have set it...
     */
    if ( new_options & GMOPT_RST_HST ) {
	clr_flg = 1;
	new_options &= ~GMOPT_RST_HST;
    }
#endif /* defined ( GMOPT_RST_HST ) */

#ifdef GMOPT_FACT_DEFAULTS
    if ( new_options & GMOPT_FACT_DEFAULTS ) {
	/* wants to restore factory defaults.
	 * We check this before the "new == old"
	 * check so we can catch the case where
	 * the old options were non-default
	 * difficulty, hence setting to factory
	 * defaults changes difficulty.
	 */
	new_options = factory_setting(menu);
    }
#endif /* defined ( GMOPT_FACT_DEFAULTS ) */

#ifdef GMOPT_DIFFICULTY
    /* Or if they have manually changed the difficulty...
     */
    if ( (new_options ^ old_options) & GMOPT_DIFFICULTY ) {
	clr_flg = 1;
    }
#endif /* defined ( GMOPT_DIFFICULTY ) */

#ifdef __HST_ENT_TYPE_DEFINED
#if (GMOPT_DIFFICULTY|GMOPT_RST_HST)
    /* If for any reason they want to clear high scores, do so now.
     */
    if ( clr_flg ) clear_hsts();
#endif /* (GMOPT_DIFFICULTY|GMOPT_RST_HST) */
#endif /* Has any H.S.Ts to begine with... */

    if ( new_options == old_options ) return;
    eer_puts(EER_GMOPT,new_options);
}


int
CoinOptions(smp)
const struct menu_d *smp;
{
    const unsigned char *menu;
    if ( (menu = pbase->p_coinmenu) == 0 ) menu = coinmenu;
    eer_puts(EER_CNOPT,DoOptions(menu,eer_gets(EER_CNOPT),SW_EXTRA));
    return 0;
}


U32
GetCoinOptions()
{
#if (HAS_TWO)
extern	U16	coin_modes;
    if ((HDWRCONFIG & (SW_DOUBLE | SW_RIGHT)) == SW_RIGHT) /* LEFT of DOUBLE */
	coin_modes = Get1WMoveP(COMRAM_cnOpt);

    return (coin_modes);
#else
    return (eer_gets(EER_CNOPT));
#endif
}


int
PutGameOptions(options)
U32 	options;
{
    int	retVal;

    retVal = eer_puts(EER_GMOPT,options);

#if (HAS_TWO)
    if ((HDWRCONFIG & (SW_DOUBLE | SW_RIGHT)) == 0) /* RIGHT of DOUBLE	*/
	Put1WMoveP(options,COMRAM_gmOpt);	/* put opts in COMRAM	*/
#endif
    return (retVal);
}


U32
GetGameOptions()
{
#if (HAS_TWO)
extern	U16	gmopt;
    if ((HDWRCONFIG & (SW_DOUBLE | SW_RIGHT)) == SW_RIGHT) /* LEFT of DOUBLE */
	gmopt = Get1WMoveP(COMRAM_gmOpt);

    return (gmopt);
#else
    return (eer_gets(EER_GMOPT));
#endif
}

#if RELIEF

extern	void	rpm_init(),rpm_off();

void
card_options()
{
    U32 copt;

    txt_str(-1,0,"CARD_DISPENSER",TITLE_PAL + AN_BIG_SET);
    txt_str(-1,AN_VIS_ROW-6,"Press START to vend ONE CARD",RED_PALB);

    rpm_init();

    card_cntr = 0;
    card_timr = 0;

    copt = DoOptions(cardmenu,eer_gets(EER_CARDOPT),DEF_CDOPT,SW_EXTRA);
    eer_puts(EER_CARDOPT,copt);

    rpm_off();
}

PRIVATE void
show_time_stats()
{
    static STAT_STRUCT timenames[] =
    {
	{ "1-Player NEW :", EER_NEWCOINS },
	{ "1-Player CONT:", EER_CONTCOINS },
	{ "2-Player NEW :", EER_NEW2COINS },
	{ "2-Player CONT:", EER_CONT2COINS }
    };
#define	NUM_TIMES (sizeof(timenames) / sizeof(timenames[0]))

    int i;

    int row,j;

    txt_str( -1,1,"Statistics for each player position",0);
/*	   0123456789.123456789.123456789.123456789.12	*/
/*	      Center player:1234     12345   1234		*/
    txt_str( 12,3,        "COINS USED - TOT MIN - AVG",0);

    for(row = 5,i = 0; i < NUM_TIMES; ++i,++row)
    {
	U16 avgtim,tottim,totcns,mins,secs;

	if(i == 2)		/* Skip a row between TIMES and NEW */
	    ++row;
	totcns = eer_gets(timenames[i].stat_ndx);
#ifdef EER_NEWTIM
	tottim = eer_gets(timenames[i].stat_ndx + (EER_NEWTIM-EER_NEWCOINS));
#else
	tottim = eer_gets(timenames[i].stat_ndx);
#endif
	if(totcns)
	{
	    avgtim = (U16)((tottim * 60) / totcns);
	    mins = (U16)(avgtim / 60);
	    secs = avgtim - (60 * mins);
	}
	else
	{
	    mins = secs = avgtim = 0;
	}
	txt_str(3,row,timenames[i].stat_nm,0);
	txt_decnum(17,row,totcns,4,RJ_BF,GRN_PAL);
	txt_decnum(26,row,tottim,5,RJ_BF,GRN_PAL);
	txt_decnum(34,row,mins,3,RJ_BF,GRN_PAL);
	txt_str(37,row,":",3,GRN_PAL);
	txt_decnum(38,row,secs,2,RJ_ZF,GRN_PAL);
    }

    txt_str(-1,AN_VIS_ROW-3,t_msg_next,INSTR_PAL);
    txt_str(-1,AN_VIS_ROW-2,t_more_stats,INSTR_PAL);

    while(TRUE)
    {
	prc_delay0();
	if( (ctl_read_sw(SW_NEXT) & SW_NEXT) && (ctl_read_sw(0) & SW_ACTION) == 0)
	    break;
    }
}

void
show_plyr_stats(clrflag)
int clrflag;
{
    int i;
    S16 _era,whole;
    S16 div;
    S32 sum;

    static STAT_STRUCT statnames[] =
    {
	{ "       AT BATS ", EER_AT_BATS },
	{ "          HITS ", EER_HITS },
	{ "   RUNS SCORED ", EER_RUNS },
	{ "        HOMERS ", EER_HOMERS },
	{ "       TRIPLES ", EER_TRIPLES },
	{ "       DOUBLES ", EER_DOUBLES },
	{ "         WALKS ", EER_WALKS },
	{ "    SACRIFICES ", EER_SACS },
	{ "PITCHES THROWN ", EER_PITCH },
	{ "    STRIKEOUTS ", EER_KOS },
	{ "  RUNS ALLOWED ", EER_ALLOWED },
	{ "  ALLOWED HITS ", EER_AHITS },
	{ "WALKS GIVEN UP ", EER_AWALKS },
	{ "   BATTERS HIT ", EER_HBP },
	{ " HITTERS FACED ", EER_FACED },
	{ "     OUTS MADE ", EER_OUTS },
	{ "  DOUBLE PLAYS ", EER_DPS }
    };

#define	STATNAME_CNT (sizeof(statnames)/sizeof(statnames[0]))

    i = 1;

    while(TRUE)
    {
	if(   (ctl_read_sw(0) & SW_NEXT)		/* left button down?	*/
	   && (ctl_read_sw(0) & SW_ACTION)		/* right button down?	*/
	   && clrflag			/* ::::: CLEAR THE STATS! :::::	*/
	  )
	{
	    while(ctl_read_sw(0) & SW_NEXT)		/* wait till release	*/
		prc_delay0();
	    for ( i = 0;i < EER_CNOPT; ++i)	/* clear 'em ALL!	*/
		eer_puts(i,0);
	    clrflag = 0;
	    i = 1;
	}

	if(i)			/* first time through or after clearing */
	{

	/* ::: Display the stats from 0 to EEROM_ERRORS :::::		*/

	    txt_str(-1,1,"PERFORMANCE STATISTICS",0);
/*		   0123456789.123456789.123456789.123456789.12	*/

	    for (i = 0; i < STATNAME_CNT; ++i)
	    {
		U16 statval;

		statval = eer_gets(statnames[i].stat_ndx);
		txt_str(1,5 + i,statnames[i].stat_nm,GRN_PAL);
		txt_decnum(15,5 + i,statval,5,RJ_BF,GRN_PAL);
	    }

	    /*		256789.123456789.1			*/
	    txt_str(25,5, " AVG:     .     ",GRN_PAL);
	    txt_str(25,6, "SLUGGING% .    ",GRN_PAL);
	    txt_str(25,7, "TOT BASES:      ",GRN_PAL);
	    txt_str(25,14," ERA:      .    ",GRN_PAL);
	    txt_str(25,15,"PITCH/AB:  .    ",GRN_PAL);

	    /* Batting average	*/

	    div = eer_gets(EER_AT_BATS);
	    if(div == 0)
		div = 1;
	    whole = (S16)(((S16)eer_gets(EER_HITS) * 1000)/div);
	    txt_decnum(36,5,whole,3,RJ_ZF,GRN_PAL);

	    /* Slugging percentage & total bases		*/

	    div = eer_gets(EER_AT_BATS);
	    if(div == 0)
		div = 1;

	    sum = eer_gets(EER_HITS) + eer_gets(EER_DOUBLES);
	    sum += 2*eer_gets(EER_TRIPLES) + 3*eer_gets(EER_HOMERS);

	    whole = (S16)((sum * 1000)/div);
	    txt_decnum(36,6,whole,3,RJ_ZF,GRN_PAL);
	    txt_decnum(35,7,sum,4,RJ_BF,GRN_PAL);

	    /* ERA	*/

	    div = eer_gets(EER_OUTS);	/* How many outs made?		*/
	    if(div == 0)
		div = 1;
	    _era = (S16)((eer_gets(EER_ALLOWED) * 2700)/div);
	    whole = (S16)(_era / 100);		/* Runs per 27 outs	*/

	    txt_decnum(34,14,whole,2,RJ_BF,GRN_PAL);
	    _era -= whole * 100;
	    txt_decnum(37,14,_era,2,RJ_ZF,GRN_PAL);

	    /* Average number of pitches	*/

	    div = eer_gets(EER_FACED);	/* How many batters faced?	*/
	    if(div == 0)
		div = 1;
	    _era = (S16)((eer_gets(EER_PITCH) * 1000)/div);
	    whole = (S16)(_era / 1000);		/* pitches per batter	*/

	    txt_decnum(34,15,whole,2,RJ_BF,GRN_PAL);
	    _era -= whole * 1000;
	    txt_decnum(37,15,_era,3,RJ_ZF,GRN_PAL);

#if (0)
	    if (HIST_TABLES)
	    {
		txt_str(-1,AN_VIS_ROW-3,t_msg_next,INSTR_PAL);
		txt_str(-1,AN_VIS_ROW-2,"for histograms",INSTR_PAL);
	    }
	    else
		sayopt();
#else
	    bottom = st_insn(bottom,
	     (HIST_TABLES ? "For histograms," : t_msg_ret_menu),
	     t_msg_next, INSTR_PAL);
#endif

	    if (clrflag)
	    {
#if (0)
		txt_str(-1,AN_VIS_ROW-5,t_msg_clear,WHT_PALB);
	    	txt_str(-1,AN_VIS_ROW-4,"to clear ALL statistics",WHT_PALB);
#else
		bottom = st_insn(bottom,"To clear ALL statistics,",t_msg_clear,INSTR_PAL);
#endif

	    i = 0;
	}
	prc_delay0();
	if( (ctl_read_sw(SW_NEXT) & SW_NEXT) && (ctl_read_sw(0) & SW_ACTION) == 0)
	    break;
    }
}

#endif

#ifdef PEDES

/* PEEDES stat routines:	GAMEID is 0=centipede  1=missile

   st_sessn_start(plyr_code,gameid)	Start specified player(s)
   st_join_game(wave#)			add a player to a game
   st_quit_game(plyr_code,wave#)	one of the players died

 */

/* PLYR_CODE 0 = left;  1 = right;  2 = both				*/

st_sessn_start(playnum,gameid)		/* Start specified player(s)	*/
int playnum,gameid;
{
    curgame = gameid;

    eer_incs((gameid ? EER_MISSLSCNT : EER_CENTISCNT),1);

    eer_start(2);			/* Fire up the session		*/
    if(playnum == 2)
    {
	eer_start(0);
	eer_start(1);
	eer_incs(EER_CENTIGAME + gameid,2);
    }
    else
    {
	eer_start(playnum);
	eer_incs(EER_CENTIGAME + gameid,1);
    }
}

/* ST_JOIN_GAME returns FALSE if it's not a valid call			*/

st_join_game(wave)			/* We ASSUME that a game's on	*/
{
    if(oldplaymask <= 4 || oldplaymask == 7)	/* BUG??		*/
	return(FALSE);
    eer_tally_hist((curgame ? HIST_MISSLJOIN : HIST_CENTIJOIN),wave);
    eer_play(7);
    eer_incs(EER_CENTIGAME + curgame,1);
    return(TRUE);				/* it's alright...	*/
}

st_quit_game(playndx,wave)		/* one of the players died	*/
int playndx,wave;
{
    if(playndx == 2 || oldplaymask < 4)			/* Both out??	*/
    {
	eer_end(0);
	eer_end(1);
    }
    else
	eer_end(playndx);

    if(oldplaymask <= 4)		/* We're DONE!			*/
	eer_end(2);			/* .. kill the session		*/

    eer_tally_hist((curgame ? HIST_MISSLWAVE : HIST_CENTIWAVE),wave);
}

#endif

#ifdef TANK

/* TANK stat routines
 *
 *   st_start_session()			Start a session
 *   st_end_game()			End a game
 *   st_restart_session()		Restart/continue a session
 *   st_quit_session()			player walked away
 *
 */

void
st_start_session()	/* Start a session	*/
{
	eer_start(N_PLAYERS);		/* Fire up the session	*/
	eer_start(0);			/* and start the player	*/

	oldCkErrs = rxCkErr;		/* get checksum errors at start of game	*/
}

int		/* End a game	*/
st_end_game(contin,players,solo,wins,deaths,rank,AI1,AI2,AI3,AI4,
		reloads,tank_type,fired,hit,aborted)
int	contin,players,solo,wins,deaths,rank,AI1,AI2,AI3,AI4;
int	reloads,tank_type,fired,hit,aborted;
{
	eer_incs(EER_GMS,1);		/* .. count Total Games here	*/
	if ( players  &&  players < 7 )	/* .. count type of session here */
		eer_incs( EER_1PGAMS + players - 1, 1 );

	if ( players == 1 )	/* SOLO Game	*/
	{
		if ( solo	/* only count winning streaks for Solo SESSIONS		*/
		  && !contin )	/* AND only the first winning streak, i.e. non-continued games	*/
		{
			eer_tally_hist( HIST_WINNING, wins );
		}
		eer_tally_hist(HIST_HUMAN_RANK,rank);	/* report Human ranking for SOLO games */
	}
	else			/* LINKED Game	*/
	{
		if ( AI1 )
			eer_tally_hist(HIST_AI_RANK,0);	/* report AI ranking for LINKED games */
		if ( AI2 )
			eer_tally_hist(HIST_AI_RANK,1);	/* report AI ranking for LINKED games */
		if ( AI3 )
			eer_tally_hist(HIST_AI_RANK,2);	/* report AI ranking for LINKED games */
		if ( AI4 )
			eer_tally_hist(HIST_AI_RANK,3);	/* report AI ranking for LINKED games */
	}

	eer_tally_hist( HIST_RELOADS, reloads );

	switch( tank_type )
	{
	default:
	case 0:
		break;
	case 1:	/* SpeedMEK	*/
		eer_tally_hist( HIST_SP_RANK, rank );
#if 0
		eer_tally_hist( HIST_WEAPONS6, fired );
		eer_tally_hist( HIST_WEAPONS7, hit );
#endif
		break;
	case 2:	/* StealthMEK	*/
		eer_tally_hist( HIST_ST_RANK, rank );
#if 0
		eer_tally_hist( HIST_WEAPONS4, fired );
		eer_tally_hist( HIST_WEAPONS5, hit );
#endif
		break;
	case 3:	/* AssaultMEK	*/
		eer_tally_hist( HIST_AS_RANK, rank );
#if 0
		eer_tally_hist( HIST_WEAPONS1, fired );
		eer_tally_hist( HIST_WEAPONS2, hit );
		eer_tally_hist( HIST_WEAPONS3, aborted );
#endif
		break;
	case 4:	/* HyperMEK	*/
		eer_tally_hist( HIST_HP_RANK, rank );
#if 0
		eer_tally_hist( HIST_WEAPONS6, fired );
		eer_tally_hist( HIST_WEAPONS7, hit );
#endif
		break;
	case 5:	/* LurkerMEK	*/
		eer_tally_hist( HIST_LK_RANK, rank );
#if 0
		eer_tally_hist( HIST_WEAPONS4, fired );
		eer_tally_hist( HIST_WEAPONS5, hit );
#endif
		break;
	case 6:	/* SuicideMEK	*/
		eer_tally_hist( HIST_SU_RANK, rank );
#if 0
		eer_tally_hist( HIST_WEAPONS1, fired );
		eer_tally_hist( HIST_WEAPONS2, hit );
		eer_tally_hist( HIST_WEAPONS3, aborted );
#endif
		break;
	}

	eer_tally_hist(HIST_DEATHS,deaths);	/* count deaths per game */

	if(oldplaymask != 3)		/* should have player and session active (i.e. 3)	*/
		return(FALSE);			/* BUG??		*/

	eer_end(0,contin);		/* end the player	*/
	eer_play(3);			/* restart timing for player and session	*/
	return(TRUE);			/* return OK status	*/
}


/*
 *	By restarting the timing in st_end_game above
 *	 there is really no need for st_restart_session() below;
 *	  but I'll leave it here for now.
 */

int
st_restart_session()	/* Restart/continue a session	*/
{
#if 0
	if( oldplaymask != (1 << N_PLAYERS) )	/* at this point we should ONLY have session active */
		return(FALSE);			/* BUG??		*/
#endif

	eer_play(3);			/* restart timing for player and session	*/
	return(TRUE);			/* return OK status	*/
}

int
st_quit_session(players,games,tourney)		/* the player quit and walked away	*/
int	players,games,tourney;
{
    U16	temp;
    U32	new_errors;

	temp = TRUE;			/* assume status is OK	*/
#if 0	/* this condition NO longer occurs, st_end_game() restarts the player timer	*/
	if(oldplaymask != (1 << N_PLAYERS))		/* Only session should be active	*/
		temp = FALSE;				/* indicate that there was a problem	*/
#else
	eer_stop(0);					/* .. stop the player		*/
#endif
	eer_end(N_PLAYERS,0);				/* .. kill the session		*/
	eer_tally_hist(HIST_SESSION,games);		/* count games per session	*/
	if ( players == 1 )				/* SOLO Game	*/
		eer_tally_hist(HIST_TOURNAMENT,tourney-1);	/* count tournament stats	*/

	new_errors = oldCkErrs;
	new_errors = (oldCkErrs = rxCkErr) - new_errors;
	eer_incs( EER_LINKFAIL, new_errors );		/* count Link Checksum Errors	*/

	return( temp );
}
#endif

#ifdef EER_UREC_IDX
PRIVATE void ShowUserMsgs(int clearFlag) {

    int	update, ii, bottom, n_recs;
    static const struct menu_d myname = { "USER MSGS",0};
    U8 *rcd, tmp[AN_VIS_COL_MAX-3-3+1];

    bottom = st_bottom();
    update = 1;

    /* Pre-scan to see how many USER_RECS the user asked for,
     * so this can vary dynamically. That way, we don't have
     * to include stat_defs.h and/or worry how we spell it
     * when we develop our _next_ game on DOS...
     */
    for ( ii = 0 ; ; ++ii ) {
	rcd = eer_user_rd(ii, (int *)0);
	if ( rcd == 0 ) break;
	eer_user_free(ii);
    }
    n_recs = ii;

    while (TRUE)
    {
	prc_delay0();

	if ((ctl_read_sw(SW_NEXT) & SW_NEXT)
	&& (ctl_read_sw(0) & SW_ACTION) == 0)
	    break;

	if (update)		/* display screen text			*/
	{
	    int	row,idx;

	    vid_clear();
	    bottom = st_frame(&myname,TITLE_PAL,INSTR_PAL,0);
	    row = 3;
	    idx = eer_gets(EER_UREC_IDX);
	    for (ii=0; ii < n_recs; ++row,++ii)
	    {
		int jj, ilen;
		rcd = eer_user_rd(ii, &ilen);
		if (!rcd) continue;
		if (ilen > sizeof(tmp)-1) ilen = sizeof(tmp)-1;
		txt_decnum(2, row, ii, 2, RJ_BF, (idx == ii) ? WHT_PAL:GRY_PAL);
		txt_cstr(":", GRY_PAL);
		for (jj=0; jj < ilen; ++jj) {
		    U8 t;
		    t = rcd[jj];
		    if (!t) break;
		    tmp[jj] = (t >= ' ' && t <= '~') ? t : '.';
		}
		tmp[jj] = 0;
		eer_user_free(ii);		/* this record is not busy anymore */
		txt_cstr((char *)tmp, GRY_PAL);
	    }

	    if (clearFlag)
	    {
		bottom = st_insn(bottom,"To clear ALL messages,",t_msg_clear,INSTR_PAL);
	    }

	    update = 0;
	}

	if(   (ctl_read_sw(0) & SW_NEXT)		/* left button down?	*/
	   && (ctl_read_sw(0) & SW_ACTION)		/* right button down?	*/
	   && clearFlag			/* ::::: CLEAR THE STATS! :::::	*/
	  )
	{
	    int howmany = 0;
	    while (ctl_read_sw(0) & SW_NEXT)	/* wait till release	*/
		prc_delay0();

	    for (ii = 0; ii < n_recs ; ++ii) {		/* clear 'em ALL!	*/
		rcd = eer_user_rd(ii, 0);
		if (rcd && *rcd) {
		    *rcd = 0;
		    eer_user_wrt(ii);
		    ++howmany;
		} else {
		    eer_user_free(ii);	/* it's already clear */
		}
	    }

	    eer_puts(EER_UREC_IDX, 0);

	    clearFlag = 0;		/* can't clear more than once	*/
	    update = 1;			/* need to update screen	*/

	    if (howmany) {
#define USER_MSG "Waiting for writes to complete..."
		txt_str((AN_VIS_COL-sizeof(USER_MSG)-1-4)/2, bottom-2, USER_MSG, WHT_PAL);
		for (ii=bottom-1; ii < AN_VIS_ROW-1; ++ii) {
		    txt_clr_wid(2, ii, AN_VIS_COL-2);
		}
		for (ii=90*howmany; ;ii=ii>0?ii-1:0) {
		    if ( eer_busy() < 2 ) break;
		    prc_delay(0);
		    txt_decnum((AN_VIS_COL-sizeof(USER_MSG)-1-4)/2+sizeof(USER_MSG)-1, bottom-2,
			    ii/60, 4, RJ_BF, WHT_PAL);
		}
	    }
	}
    }
}
#endif
