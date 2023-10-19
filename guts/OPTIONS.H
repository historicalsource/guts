
/*
 * options.h, options.i and optmenu.c are created from
 * 	option_map.mac (see Makefile and option_pp.mac).
 * Option values can be obtained with eer_gets(EER_GMOPT).
 */

#define GO_FACTORY (0x80000000)	/* Reset Options to Factory Defaults */
#define GOS_FACTORY (0x1F)	/* Number of bits to shift */

#define GO_TOURNEY (0x3)	/* Tournament Setup */
#define GOS_TOURNEY (0x0)	/* Number of bits to shift */

#define GO_CLEARHS (0x40000000)	/* Clear High Scores Now */
#define GOS_CLEARHS (0x1E)	/* Number of bits to shift */

#define GO_AUTOCLEARHS (0x20000000)	/* Clear High Scores after 2000 games or 200 non H.S. games */
#define GOS_AUTOCLEARHS (0x1D)	/* Number of bits to shift */

#define GO_CENSOR (0x4)	/* Censor High Score Table */
#define GOS_CENSOR (0x2)	/* Number of bits to shift */

#define GO_ATTRACT_AUDIO (0x18)	/* Audio in Attract */
#define GOS_ATTRACT_AUDIO (0x3)	/* Number of bits to shift */

#define GO_SPARE1 (0x60)	/* ?Not Used */
#define GOS_SPARE1 (0x5)	/* Number of bits to shift */

#define GO_COLOR (0x380)	/* Car Color */
#define GOS_COLOR (0x7)	/* Number of bits to shift */

#define GO_DAMAGE (0x400)	/* Car Damage */
#define GOS_DAMAGE (0x0A)	/* Number of bits to shift */

#define GO_MARKER (0x800)	/* Player Car Markers */
#define GOS_MARKER (0x0B)	/* Number of bits to shift */

#define GO_FORCE (0x3000)	/* Steering Wheel Force */
#define GOS_FORCE (0x0C)	/* Number of bits to shift */

#define GO_LANG (0x4000)	/* Language */
#define GOS_LANG (0x0E)	/* Number of bits to shift */
#define	LANG_ENGLISH	(0)
#define	LANG_JAPANESE	(1)

#define GO_METRIC (0x8000)	/* Speed Units */
#define GOS_METRIC (0x0F)	/* Number of bits to shift */
#define	UNITS_ENGLISH	(0)
#define	UNITS_METRIC	(1)

#define GO_SHIFTER (0x10000)	/* Shifter */
#define GOS_SHIFTER (0x10)	/* Number of bits to shift */

#define GO_ADVERT (0x20000)	/* Tournament Info Screen */
#define GOS_ADVERT (0x11)	/* Number of bits to shift */

#define GO_CHEATS (0x40000)	/* ?Cheat Modes */
#define GOS_CHEATS (0x12)	/* Number of bits to shift */
/*
 *  Number of option bits used is 22 out of a total of 32 bits available.
 */

#define GO_TRFACTORY (0x80000000)	/* Reset Options to Non-Tournament Mode */
#define GOS_TRFACTORY (0x1F)	/* Number of bits to shift */

#define GO_FREE_GAME (0x1)	/* Free Game for Winner */
#define GOS_FREE_GAME (0x0)	/* Number of bits to shift */

#define GO_FREEDIFF (0x6)	/* Free Game Difficulty (Applies only if Free Game is On) */
#define GOS_FREEDIFF (0x1)	/* Number of bits to shift */

#define GO_SPARE2 (0x8)	/* :-- TRACK DIFFICULTY (A=Easiest, H=Hardest) ---------- */
#define GOS_SPARE2 (0x3)	/* Number of bits to shift */

#define GO_T1DIFF (0x70)	/* Track 1 (A-H) */
#define GOS_T1DIFF (0x4)	/* Number of bits to shift */

#define GO_T2DIFF (0x380)	/* Track 2 (A-H) */
#define GOS_T2DIFF (0x7)	/* Number of bits to shift */

#define GO_T3DIFF (0x1C00)	/* Track 3 (A-H) */
#define GOS_T3DIFF (0x0A)	/* Number of bits to shift */

#define GO_T4DIFF (0x0E000)	/* Track 4 (A-H) */
#define GOS_T4DIFF (0x0D)	/* Number of bits to shift */

#define GO_SPARE3 (0x10000)	/* :-- NUMBER OF LAPS (1-4) ----------------------------- */
#define GOS_SPARE3 (0x10)	/* Number of bits to shift */

#define GO_TRK1LAPS (0x60000)	/* Track 1 (1-4) */
#define GOS_TRK1LAPS (0x11)	/* Number of bits to shift */

#define GO_TRK2LAPS (0x180000)	/* Track 2 (1-4) */
#define GOS_TRK2LAPS (0x13)	/* Number of bits to shift */

#define GO_TRK3LAPS (0x600000)	/* Track 3 (1-4) */
#define GOS_TRK3LAPS (0x15)	/* Number of bits to shift */

#define GO_TRK4LAPS (0x1800000)	/* Track 4 (1-4) */
#define GOS_TRK4LAPS (0x17)	/* Number of bits to shift */
/*
 *  Number of option bits used is 26 out of a total of 32 bits available.
 */


#define GO_TOFACTORY (0x80000000)	/* Reset Options to Factory Defaults */
#define GOS_TOFACTORY (0x1F)	/* Number of bits to shift */


#define GO_SENDTOURN (0x1)	/* Send Tournament Setup Message to all Cabinets */
#define GOS_SENDTOURN (0x0)	/* Number of bits to shift */

#define GO_CABINET1 (0x10000)	/* Red Cabinet */
#define GOS_CABINET1 (0x10)	/* Number of bits to shift */

#define GO_CABINET2 (0x20000)	/* Green Cabinet */
#define GOS_CABINET2 (0x11)	/* Number of bits to shift */

#define GO_CABINET3 (0x40000)	/* Blue Cabinet */
#define GOS_CABINET3 (0x12)	/* Number of bits to shift */

#define GO_CABINET4 (0x80000)	/* Purple Cabinet */
#define GOS_CABINET4 (0x13)	/* Number of bits to shift */

#define GO_CABINET5 (0x100000)	/* Yellow Cabinet */
#define GOS_CABINET5 (0x14)	/* Number of bits to shift */

#define GO_CABINET6 (0x200000)	/* Teal Cabinet */
#define GOS_CABINET6 (0x15)	/* Number of bits to shift */

#define GO_CABINET7 (0x400000)	/* Black Cabinet */
#define GOS_CABINET7 (0x16)	/* Number of bits to shift */

#define GO_CABINET8 (0x800000)	/* Orange Cabinet */
#define GOS_CABINET8 (0x17)	/* Number of bits to shift */

#define GO_JOININ (0x6)	/* Join In */
#define GOS_JOININ (0x1)	/* Number of bits to shift */

#define GO_TRNTRACK (0x38)	/* Track */
#define GOS_TRNTRACK (0x3)	/* Number of bits to shift */

#define GO_TRNCAR (0x3C0)	/* Car */
#define GOS_TRNCAR (0x6)	/* Number of bits to shift */

#define GO_TRNSHIFT (0x0C00)	/* Shifter */
#define GOS_TRNSHIFT (0x0A)	/* Number of bits to shift */

#define GO_TRNLAPS (0x7F000000)	/* Laps */
#define GOS_TRNLAPS (0x18)	/* Number of bits to shift */

#define GO_TRNDRONE (0x3000)	/* Drones */
#define GOS_TRNDRONE (0x0C)	/* Number of bits to shift */

#define GO_TRNCATCH (0x0C000)	/* Catchup */
#define GOS_TRNCATCH (0x0E)	/* Number of bits to shift */
/*
 *  Number of option bits used is 32 out of a total of 32 bits available.
 */
