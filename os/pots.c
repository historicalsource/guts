/*	pots.c
 *	Copyright 1985-1997 ATARI Games Corporation.  Unauthorized reproduction,
 *	adaptation, distribution, performance or display of this computer
 *	program or the associated audiovisual work is strictly prohibited. 
*/
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: pots.c,v 1.23 1997/10/10 04:30:40 shepperd Exp $";
#endif
/*==============================================================*
 *                 How to use these routines			*
 *								*
 *  Initialization:						*
 *	pot_init(); (was InitPots()) once at power up		*
 *	ResetPots();		only to reset limits		*
 *  Read values:						*
 *	VBIReadPots();		each frame(VBINT)		*
 *	pot_linear();		called from a main()		*
 *	pot_parabolic();	called from a main()		*
 *	JoyPot();		whenever			*
 *	pot_raw();		whenever			*
 *  Save values:						*
 *	pot_save() (was SavePots())	periodically		*
 *	pot_age() (was AgePots())	occasionally in play	*
 ****************************************************************/
#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <eer_defs.h>

#if POT_CNT
#include <pot_desc.h>

# if POT_WHEN_OK
int pot_only_when_ok;
# endif

#define	PRIVATE	static

#define	LINEAR		1
#define	PARABOLIC	2
#define	POT_ANAL	(LINEAR+PARABOLIC)	/* method for analyzing pot data	*/

#define HISTART 0x0000		/* start hi here			*/
#define LOSTART 0xFFFF		/* start lo here 			*/
#define LIMCNT	2		/* count before adjusting lo or hi count */
#define NEWNOISE 1		/* Ignore quantization noise of A-D converter */

/*==============================================================*
 *                 ram/rom/defines/inits			*
 ****************************************************************/

typedef	struct
{
    U16		lim;	/* limiting value, clip at this value		*/
    U16		nxt;	/* next best try at a lim value when cnt gets hi enuf */
    U8		cnt;	/* # of times we tried to exceed lim value	*/
    U8		age;	/* count before limit reduced			*/
} LIM_TYP;

typedef	struct
{
    U16		new;	/* latest hardware value */
    U16 	good;	/* good value to use elsewhere, filtered and limited */
    LIM_TYP	hi;
    LIM_TYP	lo;
} POT_TYP;


PRIVATE	U16	RAW_AD[POT_CNT];	/* RAW AtoD inputs from hardware */
PRIVATE	POT_TYP	potBank[POT_CNT];	/* filtered and limited		*/
PRIVATE int	pots_being_read;

PRIVATE	void	ResetPots();
PRIVATE	void	Filter();
PRIVATE	void	Limit();
PRIVATE	void	LoadPots();
	S16	JoyPot();

#define POT_CODE
#include <pot_desc.h>
#undef POT_CODE

/*==============================================================*
 *	pot_init()	init pot vars on power_up		*
 ****************************************************************/
void
pot_init(level)
int level;
{
    U32	oldIPL;
    
#if POT_WHEN_OK
    if (!pot_only_when_ok) return;
#endif

    oldIPL = prc_set_ipl(INTS_OFF);

    ResetPots();
    if ( !level ) LoadPots();

    prc_set_ipl(oldIPL);

    pot_POST( level );
}

PRIVATE void
ResetPots()
{
    m_int	i;
    POT_TYP	*pots = potBank;

#if POT_WHEN_OK
    if (!pot_only_when_ok) return;
#endif

    tq_del( &tqtmr );			/* remove from queue if necessary */
    tqtmr.delta = POT_TIME;		/* delay until conversion valid */
    tqtmr.func = pot_timer;
    tqtmr.vars = (void *)0;		/* beginning of pot array */
    ReadPot( pot_desc[ 0 ].pot );	/* prime the A/D pump */
    tq_ins( &tqtmr );			/* start next series of reads */

    for (i=0; i<POT_CNT; ++i) {
	pots->good =
	pots->new = 0x0;		/* start here until limits are valid */
	pots->hi.nxt = 0xFFFF;
	pots->hi.lim = HISTART;		/* start hi limit very low	*/
	pots->lo.nxt = 0;
	pots->lo.lim = LOSTART;		/* start lo limit very hi	*/
	pots->hi.cnt =
	pots->lo.cnt = 0;		/* count starts at zero		*/
	pots->hi.age =
	pots->lo.age = 0;		/* count starts at zero		*/
	++pots;
    }
}

/*==============================================================*
 *	FilterPot()	filters pot values once when read	*
 ****************************************************************/
PRIVATE void
FilterPot(potnum)
U16	potnum;
{
    Filter(&potBank[potnum],RAW_AD[potnum]);
    Limit(&potBank[potnum]);		/* handle hi/lo limits */
}


PRIVATE void
Filter(pots,newval)
POT_TYP	*pots;
U16	newval;
{
    m_uint good;
    
    good = pots->good;
    
    if (newval <= (U16)(good + NEWNOISE)	/* ignore new values...	*/
    && newval >= (U16)(good - NEWNOISE))	/* ... too close	*/
	newval = good;				/* use previous value	*/

    if (pots->hi.lim < pots->lo.lim) {
	/* have no limits yet, so call current value "good" to average 
	 * with ourself below, avoids artificially low initial reading
	 */
	good = newval;
    }

    if (newval >= (good + (4 * NEWNOISE))	/* use new values...	*/
      || newval <= (good - (4 * NEWNOISE)))	/* ... if value distant	*/
	good = newval;			/* use current value	*/
    else
	good = (newval + good) >> 1;	/* save filtered value	*/
    pots->good = good;
/*
 *	 beware of NEWNOISE = 2 and newval = 13,14,15,16 and good = 16
 * because it will occilate between 13 and 16 instead of stopping at 14 or 15
 */
}


PRIVATE void
Limit(pots)
POT_TYP	*pots;
{
    if (pots->good > pots->hi.lim)	/* if above hi limit		*/
    {
	pots->hi.cnt += 1;		/* then count transgressions	*/
	pots->hi.age = 0;
    
	if (pots->hi.nxt > pots->good)	/* if ever going to change hilim */
	    pots->hi.nxt = pots->good;	/* then capture safest ( closest ) */
	    				/* change			*/
    
	if (pots->hi.cnt >= LIMCNT)
	{				/* Then move part way to new limit */
	    pots->hi.lim += ( pots->hi.nxt - pots->hi.lim ) >> 1;
	    pots->hi.nxt = 0xFFFF;	/* prepare for handling next...	*/
	    pots->hi.cnt = 0;		/* ... new limit		*/
	}
	
	pots->good = pots->hi.lim;	/* restrict good value to within limits */
    }
    else	/* within hi range, so start over looking for new hi limit */
    {
	pots->hi.nxt = 0xFFFF;	/* next trial limit must be less than this */
	pots->hi.cnt = 0;	/* start countup from scratch		*/
    }
    
    if (pots->good < pots->lo.lim) /* If good value is under current low limit */
    {
	pots->lo.cnt += 1;	/*   then keep track of this 'error'	*/
	pots->lo.age = 0;
    
	if (pots->lo.nxt < pots->good)	/* if ( nxt_potential_lolim < good_current_value */
	    pots->lo.nxt = pots->good;	/* move nxt up to good -- keep it as close to curr */
    
	if (pots->lo.cnt >= LIMCNT) 
	{				/* half speed to new lolim	*/
	    pots->lo.lim -= ( pots->lo.lim - pots->lo.nxt ) >> 1;
	    pots->lo.nxt = 0;		/* Prepare for next capture...	*/
	    pots->lo.cnt = 0;		/* of best ( highest ) lolim	*/
	}
	pots->good = pots->lo.lim;	/* since good was past limit, correct its value */
    }
    else			/* else good_currect_value is fine as is */
    {
	pots->lo.nxt = 0;	/* next trial limit must be more than this */
	pots->lo.cnt = 0;	/* start delay counter again */
    }
}


/*==============================================================*
 *	pot_age()	age pots to reduce limits		*
 ****************************************************************/
void
pot_age()
{
    POT_TYP	*pots;
    const struct pot_config *pcp;
    
    for (pots=&potBank[0], pcp=&pot_desc[0]; 
    pots<&potBank[POT_CNT];
    ++pots, ++pcp)
    {
	++pots->hi.age;
	if (pots->hi.age >= (pcp->flags&PF_AMSK))
	{
	    --pots->hi.lim;
	    pots->hi.age = 0;
	}
	++pots->lo.age;
	if (pots->lo.age >= (pcp->flags&PF_AMSK))
	{
	    ++pots->lo.lim;
	    pots->lo.age = 0;
	}
    }	
}


/*==============================================================*
 *	VBIReadPots()	handle pot values once per frame	*
 ****************************************************************/
void
VBIReadPots()
{
    register m_int	i;

    if ( tqtmr.que ) return;	/* wait until all pots are read */
    if ( tqtmr.delta > 0 )
	tq_ins( &tqtmr );	/* start next series of reads */

    if ( pots_being_read == 0 ) return;
    --pots_being_read;

#if POT_WHEN_OK
    if (!pot_only_when_ok) return;
#endif

    for ( i = 0; i < POT_CNT; ++i )
    {
	FilterPot(i);			/* filter,limit this Pot	*/
    }
}

/*==============================================================*
 *	PotsToSwitches()	pots->switches once per frame	*
 ****************************************************************/
void
PotsToSwitches()
{
#if USE_POT_SWITCHES || (FAKE_UP|FAKE_DOWN|FAKE_LEFT|FAKE_RIGHT)
    m_int	idx;
    int		delta,center;
    POT_TYP	*pot;
    const struct pot_config *pcp;

    unsigned long newstick;

#if POT_WHEN_OK
    if (!pot_only_when_ok) return;
#endif

    newstick = fake_controls;
    pot = &potBank[POT_CNT-1];
    pcp = &pot_desc[POT_CNT-1];
    for ( idx = POT_CNT - 1 ; idx >= 0 ; --idx ) {
	newstick &= ~(pcp->low_swt|pcp->high_swt);  
	delta = pot->hi.lim - pot->lo.lim;
	/* only use pots with valid ranges */
	if ( delta >= pcp->range ) {
	    delta >>= 2;
	    center = (pot->hi.lim + pot->lo.lim + 1)>>1;
	    if ( pot->good < (center - delta) ) {
		newstick |= pcp->low_swt;
	    }
	    else if ( pot->good > (center + delta) ) {
		newstick |= pcp->high_swt;
	    }
	}
	--pcp;
	--pot;
    }
    fake_controls = newstick;
#endif
}

#if (POT_ANAL & LINEAR)
/*==============================================================*
 *	pot_linear() (was LinearPot())	linear pot values	*
 *								*
 *	A value is returned from -width to +width.		*
 *	The play is the number of values around the middle to	*
 *	ignore (ie. treat as if the middle).			*
 *	The fuzz is the number of values at the max/min to	*
 *	ignore (ie. treat as the max/min).			*
 *								*
 *	Modified 2APR96 to accept a _negative_ width, and to	*
 *	interpret that as a request to return an unsigned value *
 *	from 0 to |width|, with <play> indicating the number	*
 *	of values to ignore at the low end, and <fuzz> 		*
 *	indicating the number of values to ignore at the high	*
 *	end. Note that this interpretation will return a total  *
 *	of (|width|+1) values, while the original routine	*
 *	returned (2*width) values. I didn't write it :-) MEA	*
 *								*
 *	Although the use of a _signed_ width to indicate an	*
 *	_unsigned_ return value is wierd, it was done in the	*
 *	name of backward compatibility. All previous games	*
 *	which used this code (or its ancestors) had centered	*
 *	controls. Most previous driving games came out of	*
 *	Applied Research, so they did all pot-massaging in the	*
 *	game code.						*
 *								*
 ****************************************************************/

int
pot_linear(potNum,play,width,fuzz)
S16	potNum,play,width,fuzz;
{
    POT_TYP	*pot;
    U32 lo,hi;	/* copy them here in case hi and lo change	*/
		/* instead of copying whole structure and stopping interrupts */
    U32 range;	/* lowest to highest pot value			*/
    U16 middle;	/* middle pot value				*/
    S16 x;	/* value in the range (-range/2,range/2)	*/

#if POT_WHEN_OK
    if (!pot_only_when_ok) return 0;
#endif

    pots_being_read = 2;
    pot = &potBank[potNum];

    hi = pot->hi.lim;
    lo = pot->lo.lim;
    if ( width > 0 ) {
	range = hi - lo - (2*play) - (2*fuzz);
	if (range <= 1)			/* pots are not good */
	    return(0);		/* avoid range=1 (divide by zero) */
	middle = (hi + lo + 1) / 2;
	x = pot->good - middle; 	/* change from the middle */
	if (x < 0			/* there should be two middle values in this case */
	&& (range&1) != 0)
	    ++x;
	range >>= 1;			/* half range */
	if (x < 0)
	{
	    x += play;		/* add in play near the middle */
	    if (x > 0)
		x = 0;		/* this is the same as the middle */
	    if (x < (short)-range)	/* fuzz factor here */
		x = -range;
	}
	else 
	{
	    x -= play;		/* add in play near the middle */
	    if (x < 0)
		x = 0;		/* this is the same as the middle */
	    if (x > (short)range)		/* fuzz factor here */
		x = range;
	}
    } else {
	/* Added hack for unsigned return values */
	range = hi - lo - play - fuzz;
	if (range <= 0)			/* pots are not good */
	    return(0);		/* avoid range=1 (divide by zero) */
	middle = (hi + lo + 1) / 2;
	x = pot->good - lo - play; 	/* change from bottom */
	if (x < 0 ) x = 0;		/* count ( x < play) as zero. */
	if ( x > (range-fuzz) ) x = range; /* count as max value */
	width = -width;			/* Make it positive */
    }
    return( (short)( (x * width) / (short)range) );
}
#endif


#if (POT_ANAL & PARABOLIC)
/*==============================================================*
 *	pot_parabolic() - parabolic pot values 			*
 *								*
 *	A value is returned from -width to +width.		*
 *	The play is the number of values around the middle to	*
 *	ignore (ie. treat as if the middle).			*
 ****************************************************************/
int
pot_parabolic(potnum,play,width)
S16	potnum,play,width;
{
    U16 lo,hi;			/* copy them here in case hi and lo change */
	    /* instead of copying whole structure and stopping interrupts */
    U16 range;			/* lowest to highest pot value		*/
    U16 middle;			/* middle pot value			*/
    S16 x;			/* value in the range (-range/2,range/2) */
    S32 result;
    
#if POT_WHEN_OK
    if (!pot_only_when_ok) return 0;
#endif

    pots_being_read = 2;
    hi = potBank[potnum].hi.lim;
    lo = potBank[potnum].lo.lim;
    if ((long)(hi-lo-2*play-1) <= 0) /* pots are not good */
	return(0);		/* avoid range=1 (divide by zero) */
    range = hi - lo - 2*play;
    middle = (hi + lo + 1) / 2;
    x = potBank[potnum].good - middle; /* change from the middle */
    if (x < 0			/* there should be two middle values in this case */
    && (range&1) != 0)
	++x;
    range >>= 1;			/* half range */
    if (x < 0)
    {
	x += play;		/* add in play near the middle */
	if (x > 0)
	    x = 0;		/* this is the same as the middle */
    }
    else 
    {
	x -= play;		/* add in play near the middle */
	if (x < 0)
	    x = 0;		/* this is the same as the middle */
    }
    result = (S16)((x << 7) / (S16)range);	/* 0 <= result <= 0x80 */
    result = (S16)result * (S16)result;	/* 0 <= result <= 0x4000 */
    result = (S16)result * (S16)width;	/* 0 <= result < 0x20000000 */
    result += 1<<13;			/* round up */
    result >>= 14;				/* 0 <= result < 0x8000 */
    if (x < 0)
	result = -result;
    return(result);
}
#endif
    
/*==============================================================*
 *	JoyPot()	signed pot values(center == 0)		*
 *								*
 *	A value is for the joystick simulation(-127,128).	*
 *								*
 ****************************************************************/
S16
JoyPot(potNum)
S16	potNum;
{
    POT_TYP	*pot;

#if POT_WHEN_OK
    if (!pot_only_when_ok) return 0;
#endif

    pots_being_read = 2;
    pot = &potBank[potNum];
    return(pot->good - ((pot->hi.lim + pot->lo.lim + 1) / 2));
}


/*==============================================================*
 *	RawPot()	raw pot values 				*
 *								*
 *	A value is for the raw pot value read.			*
 *								*
 ****************************************************************/
U16
pot_raw(potNum)
S16	potNum;
{
    pots_being_read = 2;

#ifndef GAME_VALUE
#define GAME_VALUE (potBank[potNum].good)
#endif

    return( GAME_VALUE );
}
    
    
/*==============================================================*
 *	LoadPots()	load pot var limits from EEROM		*
 ****************************************************************/
PRIVATE void
LoadPots()
{
    m_int	i;

#if POT_WHEN_OK
    if (!pot_only_when_ok) return;
#endif

    for (i=0; i<POT_CNT; ++i)
    {
#ifdef EER_POT0L
	int high = ( pot_desc[i].flags & PF_DBUG ) ? 0 : eer_gets(i + EER_POT0H);
	if ( high > 0 ) {
	    potBank[i].lo.lim = eer_gets(i + EER_POT0L);
	    potBank[i].hi.lim = high;
	} else
#endif
	{
	    potBank[i].lo.lim = 0x7fff;
	    potBank[i].hi.lim = 0;
	}
    }
}


/*==============================================================*
 *	pot_save()	save pot var limits into EEROM		*
 ****************************************************************/
void
pot_save()
{
#ifdef EER_POT0L
    m_int i;

    for (i=0; i<POT_CNT; ++i)
    {
	if ( pot_desc[i].flags & PF_DBUG ) break;
	eer_puts(i + EER_POT0L,potBank[i].lo.lim);
	eer_puts(i + EER_POT0H,potBank[i].hi.lim);
    }
#endif
}



/*
 *
 *		SELFTEST
 *
 */

PRIVATE void
SetupDisplayPots(unsigned long gopts)
{
    int idx;
    const struct pot_config *pcp;

    for ( idx = 0, pcp = pot_desc ; idx < POT_CNT ; ++idx, ++pcp ) {
	if ( ( ( gopts & GUTS_OPT_DEVEL ) == 0 ) && ( pcp->flags & PF_DBUG ) )
	    continue;
	if ( pcp->labels ) {
	    txt_str(pcp->x+5-(txt_width(pcp->labels, WHT_PALB)/2),
		pcp->y+2, pcp->labels,WHT_PALB);
	}
    }
}

/*	Display a pot's parameters, either vertically (axis != 0 )
 *	or horizontally (axis == 0).
 */
PRIVATE void
DisplayPot(potPtr,pcp)
POT_TYP	*potPtr;
const struct pot_config *pcp;
{
    S16	x, y, delta;

    if ( pcp->flags & PF_VERT ) {
	x = pcp->x;
	y = pcp->y - 1;
    }
    else {
	x = pcp->x + 2;
	y = pcp->y;
    }

    txt_hexnum(x,y,potPtr->lo.lim,2,RJ_ZF,WHT_PALB);

    delta = potPtr->hi.lim - potPtr->lo.lim;
    if (delta <= 0)
	delta = 1;
    if ( pcp->flags & PF_VERT ) y -= 4;
    else x += 3;
    txt_hexnum(x,y,potPtr->good,2,RJ_ZF,(delta >= pcp->range ? GRN_PALB : RED_PALB));

    /* temp display of normalized, but not scaled, value.
     */
    if ( ( pcp->flags & PF_DBUG ) == 0 ) {
	int value;
	if ( pcp->flags & PF_PARA ) {
	    value = pot_parabolic(potPtr-potBank,3,pcp->scale);
	} else {
	    value = pot_linear(potPtr-potBank,3,pcp->scale,0);
	}
	if ( value < 0 ) {
	    txt_str(x-3,y+1,"-",WHT_PALB);
	    txt_cdecnum(-value,4,RJ_ZF,WHT_PALB);
	} else {
	    txt_clr_str(x-3,y+1,"-",WHT_PALB);
	    txt_decnum(x-2,y+1,value,4,RJ_ZF,WHT_PALB);
	}
    }

    if ( pcp->flags & PF_VERT ) y -= 3;
    else x += 3;
    txt_hexnum(x,y,potPtr->hi.lim,2,RJ_ZF,WHT_PALB);
}

#ifndef plot_pot
/*	If the hardware can support pixel-plotting, show a dot and leave
 *	a trail tracing the physical limits of the pot (scaled down)
 */
static void
plot_pot(vals,pcp,color)
S16 *vals;
const struct pot_config *pcp;
int color;
{
    int x,y,pot_range;
    x = *vals++;
    y = *vals;
    pot_range = ( pcp->range < 0 ) ? -pcp->range : pcp->range;
    if ( x < -pot_range ) x = -pot_range;
    if ( x > pot_range ) x = pot_range;
    if ( y < -pot_range ) y = -pot_range;
    if ( y > pot_range ) y = pot_range;
    x += ((pcp->x+6) << 3);
    y += ((pcp->y-4)<<3);
#if VIDEO_BOARD == ZOID20_V
/* This works, but the dimensions are wrong for this video system.
 * SF Rush does not have joystick pots, so no effort will be spent
 * on this now.  - fom 4/5/96
 */
    {
	extern void bm_rect( int x1, int y1, int x2, int y2, int outside, int inside );
	extern int ztv_bufswap();
	bm_rect( x, y, x+1, y+1, color, color );
	ztv_bufswap();
	prc_delay0();
	bm_rect( x, y, x+2, y+2, color, color );
    }
#else
    draw_pt[y][x] = color;
#endif
}
#endif

void
pot_display(prev)
S16 *prev;
{
    int idx,plot_able;
    const struct pot_config *pcp;
    unsigned long gopts = eer_gets(EER_GUTS_OPT) | debug_mode ;

    pots_being_read = 2;
    plot_able = 1;
    if ( *prev < -32000 ) {
	SetupDisplayPots( gopts );
	plot_able = 0;
    }
    for ( idx = 0, pcp = pot_desc ; idx < POT_CNT ; ++idx, ++pcp ) {
	if ( ( ( gopts & GUTS_OPT_DEVEL ) == 0 ) && ( pcp->flags & PF_DBUG ) )
	    continue;
	if ( (idx & 1) == 0 && plot_able ) plot_pot(prev,pcp,GRY_PAL);
	if ( *prev != (S16)potBank[idx].good ) {
	    *prev = (S16)potBank[idx].good;
	    DisplayPot(potBank+idx, pcp);
	}
	if ( (idx & 1) && plot_able ) plot_pot(prev-1,pcp,YEL_PAL+1);
	++prev;
    }
}
#else
void pot_init PARMS((int level)) { return; }
int pot_linear( potnum, play, width, fuzz ) int potnum, play, width, fuzz;
{ return 0; }
int pot_parabolic(potnum,play,width)
S16	potnum,play,width;
{ return 0; }
#endif /* POT_CNT */
