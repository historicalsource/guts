/* THESE DEFINITIONS are ATARI CONVENTIONS! don't re-define them unless	*/
/*   you have a REAL GOOD understanding of what you're doing.		*/

#define	NO_MECH	(-1)	/* No mech					*/
#define	LFT_M	(0)	/* Left mech					*/
#define	RGT_M	(1)	/* Right mech					*/
#define	AUX_M	(2)	/* AUX mech					*/
#define	AUX2_M	(3)	/* alternate AUX mech				*/
#define	RLFT_M	(2)	/* Right-Left mech				*/
#define	RRGT_M	(3)	/* Right-Right mech				*/

/*
*	The array defined using MECH_ORDER provides a mapping of switch inputs
*	to the desired logical locations. These LOGICAL locations are used
*	throughout the program, as indices into the EEROM counts and for
*	left/right multipliers for example, so you must be consistent.
*
*	The macro CN_STATUS must be defined to return the switches in
*	bits 7-0. Each position in MECH_ORDER cooresponds to a single bit
*	in those 8 bit positions with the first entry representing bit 0.
*	Don't make MECH_ORDER more than 8 entries large or things will either
*	quietly break or burn down the building.
*
*	for example, if MECH_ORDER is defined as { RRGT_M, RLFT_M, RGT_M, LFT_M, LLFT_M }
*	then the bits as returned from CN_STATUS:    0,      1,      2,     3,     4
*
* NOTE: I doubt the following table has any meaning anymore, but I am loathe
* to delete it. I suggest you don't believe a word of it.
*
*	The coins come into cn_cvt_crdt() as follows:
*		2x2 Mech Doors	4 Mech Door	2 Mech Door
*	D7/6 -	left 		left 		left switch
*	D5/4 -	right 		left-center	right switch
*	D3/2 -  right-left 	right center	aux switch
*	D1/0 -	right-right 	right		aux2 switch
*
* >> So to configure this for YOUR system:
* 1) Define the location index of coin switches and AUX in the switch byte
*   using MECH_ORDER. Use 'NO_MECH' to skip any unused 1-bit field.
* 2) Use MAX_NEEDED_CREDS to set the number of 'creds' returned when it's in
*   free-play.  It should be set to the number of credits needed to start the
*   most expensive type of game with the maximum number of players.
* 3) #DEFINE the INCL_MENU if you want to use the default coin menu.
* 4) If you don't like to see the fractions reduced (e.g. 1/4,2/4,3/4)
*   then define CREDS_REDUCE to be 0
*
*/

#ifndef MECH_ORDER
# if (0)
#  define	MECH_ORDER { NO_MECH, NO_MECH, NO_MECH, NO_MECH, \
	AUX_M, AUX_M, RGT_M, LFT_M }	/* NO semi:	*/
# else
#  define MECH_ORDER { RGT_M, LFT_M, AUX_M, AUX_M }
# endif
#endif

#ifndef INCL_MENU
# define INCL_MENU	1	/* if DEF'd use the DEFAULT menu	*/
#endif
#ifndef CREDS_REDUCE
# define CREDS_REDUCE	1	/* reduce partial credit fractions 	*/
#endif

/* 	The following is used to determine what number of "fabricated" credits
*	should be alledged if the game is set in free-play. In general, it
*	should match the amount needed to start all players.
*/
#ifndef MAX_NEEDED_CREDS
#define MAX_NEEDED_CREDS (2)	/* Free-play credits (SET AS NEEDED)	*/
#endif

/* 	The following limit was 25, but bumping to 30 used to leave room for
*	a $5 bill even with most generous bonus adder. Now if some bozo sets
*	bonus adder at "four gives 3" and dollar mech at 8, a five will be
* 	worth 70, which is close enough to infinite for most operators!
*/

#ifndef MAXCOINS
#define MAXCOINS 30
#endif

/*	cn_credit() returns whole credits and partial credits.
*	The following #define determines whether or not to reduce
*	the partial credit fraction. (e.g. 4/8->1/2)
*/
#ifndef CREDS_REDUCE
#define	CREDS_REDUCE	1	/* reduce partial credit fractions	*/
#endif

/* 	The various options were formerly done with bitfields, but generated
*	code was horrible _and_ compiler dependant, which clashed with the
*	idea of menu-driven option setting.
*/

#define cmode_free(x) (x & 0x8000)
#define cmode_discnt(x) (x & 0x4000)
#define cmode_cost(x) ((x >> 11)&7)
#define cmode_thresh(x) ((x >> 8)&7)
#define cmode_award(x) ((x >> 6)&3)
#define cmode_rmul(x) ((x >> 3)&7)
#define cmode_lmul(x) (x&7)


/* 		cn_click_counter()
*	This routine "queues" a pulse to drive an electro-mechanical coin
*	counter. It is the "official" interface which coin94.c uses to
*	acknowledge receipt of a coin from xxx_coin.c.
*/
extern void 
cn_click_counter
PARMS((
    unsigned int counter_num
));

/*		cn_add_coin()
 *	This routine notifies coin94.c of the arrival of a valid coin
 *	on "logical mech" <mech_no>. The constants for the names of the
 *	logical mechs are defined above (LFT_M, RGT_M...). Note that these
 *	may be wildly different from the bit order or even the names on the
 *	schematic, depending on how whimsical hardware design and PC layout
 *	are feeling. It is the responsibility of xxx_coin.c to sort it all
 *	out before calling this routine.
 */
extern void cn_add_coin(
#if __STDC__
int mech_no		/* actually, the "logical mech" number, see above */
#endif
);

