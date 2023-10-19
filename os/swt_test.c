/* $Id: swt_test.c,v 1.3 1997/10/16 04:16:41 shepperd Exp $	*/

/* Generic switch test for Seattle */

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>
#include <eer_defs.h>

unsigned long fake_controls;

#ifndef n_elts
# define n_elts(x) (sizeof(x)/sizeof(x[0]))
#endif

typedef struct
{
    U8	x,y;
    U32	mask;
    const char * label;
    const char * wire_color;
    U8	color;
} S_BTN;

static const char   upstr[] = "Up   ";
static const char   dnstr[] = "Down ";
static const char   ltstr[] = "Left ";
static const char   rtstr[] = "Right";
static const char   astr[]  = "  A  ";
static const char   bstr[]  = "  B  ";
static const char   cstr[]  = "  C  ";
static const char   dstr[]  = "  D  ";

static const S_BTN misc_disp[] = {
    { 2,  5, SW_COIN1,	"COIN1",    "BLK-BRN"},
    { 2,  6, SW_COIN2,	"COIN2",    "BLK-RED"},
    { 2,  7, SW_P1START,"P1 START", "WHT-WHT"},
    { 2,  8, SW_TILT,	"TILT",     "BLK-GRN"},
    { 2,  9, SW_TEST,	"TEST",     "BLK-BLU"},
    { 2, 10, SW_P2START,"P2 START", "VIO-WHT"},
    { 2, 11, SW_CREDIT,	"SERV",     "WHT-GRY"},
    { 2, 12, SW_COIN3,	"COIN3",    "BLK-ORG"},
    { 2, 13, SW_COIN4,	"COIN4",    "BLK-YEL"},
    { 2, 14, SW_P3START,"P3 START", "BLU-WHT"},
    { 2, 15, SW_P4START,"P4 START", "GRY-WHT"},
    { 2, 16, SW_VOLM,	"VOL-",     "ORG-RED"},
    { 2, 17, SW_VOLP,	"VOL+",     "ORG-GRN"},
    { 2, 18, SW_UNUSED0,"UNUSED",   "       "},
    { 2, 19, SW_INTER,	"DOOR",     "BLK-GRY"},
    { 2, 20, SW_BILL,	"BILL",     "BLK-WHT"},
    {18, 20, SW_OPT0,	"U9-1",     ""},
    {18, 19, SW_OPT1,	"U9-2",     ""},
    {18, 18, SW_OPT2,	"U9-3",     ""},
    {18, 17, SW_OPT3,	"U9-4",     ""},
    {18, 16, SW_OPT4,	"U9-5",     ""},
    {18, 15, SW_OPT5,	"U9-6",     ""},
    {18, 14, SW_OPT6,	"U9-7",     ""},
    {18, 13, SW_OPT7,	"U9-8",     ""},
    {18, 12, SW_OPT8,	"U8-1",     ""},
    {18, 11, SW_OPT9,	"U8-2",     ""},
    {18, 10, SW_OPT10,	"U8-3",     ""},
    {18,  9, SW_OPT11,	"U8-4",     ""},
    {18,  8, SW_OPT12,	"U8-5",     ""},
    {18,  7, SW_OPT13,	"U8-6",     ""},
    {18,  6, SW_OPT14,	"U8-7",     ""},
    {18,  5, SW_OPT15,	"U8-8",     ""}
};

static const S_BTN player_disp[] = {
    {34,  5, J1_UP,	upstr, "WHT-BLK"},
    {34,  6, J1_DOWN,	dnstr, "WHT-BRN"},
    {34,  7, J1_LEFT,	ltstr, "WHT-RED"},
    {34,  8, J1_RIGHT,	rtstr, "WHT-ORG"},
    {34,  9, P1_A,	astr,  "WHT-YEL"},
    {34, 10, P1_B,	bstr,  "WHT-GRN"},
    {34, 11, P1_C,	cstr,  "WHT-BLU"},
    {34, 12, P1_D,	dstr,  "WHT-VIO"},

    {34, 15, J2_UP,	upstr, "VIO-BLK"},
    {34, 16, J2_DOWN,	dnstr, "VIO-BRN"},
    {34, 17, J2_LEFT,	ltstr, "VIO-RED"},
    {34, 18, J2_RIGHT,	rtstr, "VIO-ORG"},
    {34, 19, P2_A,	astr,  "VIO-YEL"},
    {34, 20, P2_B,	bstr,  "VIO-GRN"},
    {34, 21, P2_C,	cstr,  "VIO-BLU"},
    {34, 22, P2_D,	dstr,  "VIO-VIO"},

    {50,  5, J3_UP,	upstr, "BLU-BLK"},
    {50,  6, J3_DOWN,	dnstr, "BLU-BRN"},
    {50,  7, J3_LEFT,	ltstr, "BLU-RED"},
    {50,  8, J3_RIGHT,	rtstr, "BLU-ORG"},
    {50,  9, P3_A,	astr,  "BLU-YEL"},
    {50, 10, P3_B,	bstr,  "BLU-GRN"},
    {50, 11, P3_C,	cstr,  "BLU-BLU"},
    {50, 12, P3_D,	dstr,  "BLU-VIO"},

    {50, 15, J4_UP,	upstr, "GRY-BLK"},
    {50, 16, J4_DOWN,	dnstr, "GRY-BRN"},
    {50, 17, J4_LEFT,	ltstr, "GRY-RED"},
    {50, 18, J4_RIGHT,	rtstr, "GRY-ORG"},
    {50, 19, P4_A,	astr,  "GRY-YEL"},
    {50, 20, P4_B,	bstr,  "GRY-GRN"},
    {50, 21, P4_C,	cstr,  "GRY-BLU"},
    {50, 22, P4_D,	dstr,  "GRY-VIO"}

};

#define Y_OFFSET (AN_VIS_ROW > 32 ? 0 : -1)

#define EX_FRAMES (60)
#define RES_FRAMES (90)

static void show_em(S_BTN *p, U32 sw, U32 mask) {
    for( ; p->mask != 0; p++ ) {
	int pal;
	U32 ms;

	if ((sw & p->mask)) {
	    pal = RED_PAL;
	    p->color = 1;
	} else {
	    pal = p->color ? GRN_PAL : YEL_PAL;
	}
	txt_str( p->x, p->y, p->label, pal );
	ms = sw&mask;
	if ((ms&p->mask) && (!(ms & ~p->mask))) {
	    txt_str( 14, 25, p->wire_color, CYN_PAL );
	}
    }
}

int SwitchTest(const struct menu_d *smp) {
    m_int	ex_cnt;
    int meters[4], mon=0;
    int bottom, ii;
    static const int meter_index[4] = {SW_COIN1, SW_COIN2, SW_COIN3, SW_COIN4};

    S_BTN rbtn_plyr[n_elts(player_disp)+1], rbtn_misc[n_elts(misc_disp)+1];

    memcpy(rbtn_plyr, player_disp, sizeof(player_disp));
    memset(rbtn_plyr+n_elts(player_disp), 0, sizeof(S_BTN));
    memcpy(rbtn_misc, misc_disp, sizeof(misc_disp));
    memset(rbtn_misc+n_elts(misc_disp), 0, sizeof(S_BTN));

    txt_str(2, 4, "Misc", WHT_PAL);
    txt_str(18-4, 4, "DIP switches", WHT_PAL);
    txt_str(34-1,  4, "Player 1", WHT_PAL);
    txt_str(34-1, 14, "Player 2", WHT_PAL);
    txt_str(50-1,  4, "Player 3", WHT_PAL);
    txt_str(50-1, 14, "Player 4", WHT_PAL);
    txt_str( 2, 25, "Wire Color:", WHT_PAL);

#define METER_COL  ((AN_VIS_COL-12)/2)
#define METER_ROW  (AN_VIS_ROW*2/3)

    txt_str(METER_COL, METER_ROW+0, "    Coin Meters", WHT_PAL);
    txt_str(METER_COL, METER_ROW+2, "1     2     3     4", WHT_PAL);

    for (ii=0; ii < n_elts(meters); ++ii) {
	meters[ii] = 0;
	txt_str(METER_COL+ii*6-1, METER_ROW+3, "OFF", YEL_PAL);
    }
    bottom = st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_nexth, INSTR_PAL);
    bottom = st_insn(bottom, "To pulse coin meters,", "press coin switches", INSTR_PAL);
    txt_str((AN_VIS_COL-52)/2, --bottom, "GRN", GRN_PAL);
    txt_cstr("=Switch ok. ", WHT_PAL);
    txt_cstr("RED", RED_PAL);
    txt_cstr("=Switch ON. ", WHT_PAL);
    txt_cstr("YEL", YEL_PAL);
    txt_cstr("=Switch not tested.", WHT_PAL);

    prc_delay0();

    for (ex_cnt = EX_FRAMES ; ex_cnt > 0 ;) {
	U32 sw;

	txt_str(14, 25, "       ", WHT_PAL);	/* preclear wire color */

	/* player inputs */
	sw = ctl_read_sw(0);
	show_em(rbtn_plyr, sw, 0xFFFFFFFF);
	
	/* misc and dipsw */
	sw = ctl_read_debug(0);
	show_em(rbtn_misc, sw, 0xFFFF<<SH_MISC); /* show misc (mask = isolate MISC switches from DIPSW) */

	sw = ctl_read_debug(SW_COIN1|SW_COIN2|SW_COIN3|SW_COIN4)&(SW_COIN1|SW_COIN2|SW_COIN3|SW_COIN4);
	for (ii=0; ii < n_elts(meter_index); ++ii) {
	    if ((sw&meter_index[ii])) {
		meters[ii] += 64;
	    }
	    if (meters[ii]) {
		if (!mon || (mon&(1<<ii))) {	/* if nobody is already on, or we're it */
		    --meters[ii];		/* time it */
		    if ((meters[ii]&32)) {
			mon |= 1<<ii;		/* record the fact that we're on */
			COUNTER_ON(ii);
			txt_str(METER_COL+ii*6-1, METER_ROW+3, " ON", RED_PAL);
		    } else {
			mon &= ~(1<<ii);	/* record the fact that we're off */
			COUNTER_OFF(ii);
			txt_str(METER_COL+ii*6-1, METER_ROW+3, "OFF", GRN_PAL);
		    }
		}
	    }
	}

	prc_delay(0);

	if (mon) continue;			/* let meters timeout before exiting */

	/* if SW_NEXT held for 2 seconds, exit switch test */
	if ( ctl_read_sw(0) & SW_NEXT ) --ex_cnt;
	else ex_cnt = EX_FRAMES;
    }
    ctl_read_sw(-1);	/* kill all edges */
    return 0;
}

unsigned long dbswitch;		/* user visible debounced switches */

struct switch_state {
    /* First the successively "cooked" form of the switches */
    unsigned long prev;		/* previous sample of switches */
    unsigned long db;		/* Debounced switches */
    unsigned long edge;		/* "New" switch closures (since off OR read) */
    /* now the timers for auto-repeat */
    unsigned long rpt;		/* mask for which switches auto-repeat */
    unsigned short timer;
    unsigned short init;	/* auto-repeat initial delay */
    unsigned short delay;	/* auto-repeat successive delay */
} sw_state,db_state;

/*	Gang debounce routine for up to 32 switches. Called by sf_swtch
 *	for both debug and player switches. Returns new debounced state.
 */
static unsigned long
debounce_swtch(sw,new)
struct switch_state *sw;
U32 new;			/* new raw switches, high-true */
{
    unsigned long dying,old,changes,stable;
    old = sw->prev;
    sw->prev = new;

/*  The following code is a 'C' translation of the 68000 assembly
 *  in lipson/joy_sw.asm. It contains several oddities....
 */
    changes = old ^ new;
    stable = ~changes;
    dying = (changes & sw->db);		/* ones where changing to off? */
    sw->db = dying | (new & stable);	/* debounce = dying 1s or stable ones*/
    sw->edge &= sw->db;
    if ( (dying & sw->rpt) == 0 && (sw->db & sw->rpt) ) {
	/* at least one of our auto-repeat switches is on, and
	 * none are dying, run the timer.
	 */
	if ( --(sw->timer) == 0 ) {
	    sw->timer = sw->delay;	/* re-init to "later" delay */
	    sw->edge &= ~sw->rpt;	/* re-trigger selected switches */
	}
    } else sw->timer = sw->init;	/* all off or dying?, init timer */
    return sw->db;
}

U32 (*read_raw_sw_vec)(int which);

/* switch reading routine called from VBINT during self-test
 */
unsigned long
ctl_upd_sw()
{
    U32 raw;
    if (read_raw_sw_vec) {
	debounce_swtch(&db_state, read_raw_sw_vec(1));
	return debounce_swtch(&sw_state, read_raw_sw_vec(0));
    }
#ifdef READ_RAW_SWITCHES
    raw = READ_RAW_SWITCHES(1);
    debounce_swtch(&db_state, raw);
    raw = READ_RAW_SWITCHES(0);
#else
    {
	VU16 *swp;

	swp = (VU16 *)&DIAG;
	raw = (swp[0] <<16) | swp[1];
	(void) debounce_swtch(&db_state,~raw);
	swp = (VU16 *)&INPUTS;
	raw = (swp[0] <<16) | swp[1];
	raw = ~raw;			/* Switches are low_true, invert them */
    }
#endif
    return dbswitch = debounce_swtch(&sw_state,raw);
}

void
ctl_autorepeat(switches,initial,later)
unsigned long switches;
int initial,later;
{
    sw_state.rpt = switches;
    sw_state.init = initial;
    sw_state.delay = later;
}

/*		ctl_read_sw(MASK)
*
*	for bits that are TRUE in MASK
*		code returns edge state, and clears the edge state
*
*	for bits that are FALSE in MASK
*		code returns level state
*/
#define EARLY_ON (1)

unsigned long
ctl_read_sw(edge_mask)
unsigned long edge_mask;
{
    unsigned long edges,val;

    struct switch_state *sw = &sw_state;
#ifdef EARLY_ON
    val = sw->db | sw->prev;
#else
    val = sw->db;
#endif
    if ( edge_mask != 0) {
	edges = (sw->edge ^ val) & edge_mask;
	sw->edge |= edges;
	val = (val & ~edge_mask) | edges;
    }
    return val;
}

/*	More-or-less the same thing, but for the debug (Diagnostic)
 *	switches.
 */
unsigned long
ctl_read_debug(edge_mask)
unsigned long edge_mask;
{
    unsigned long edges,val;

    struct switch_state *sw = &db_state;

#if 0
    val = debug_mode & GUTS_OPT_DBG_SW;
    if ( val == 0 ) return val;
#endif
 
    val = sw->db;
    if ( edge_mask != 0) {
	edges = (sw->edge ^ val) & edge_mask;
	sw->edge |= edges;
	val = (val & ~edge_mask) | edges;
    }
    return val;
}
