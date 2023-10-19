
/*	Rush Switch test		*/

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>
#include <eer_defs.h>

#if !defined(B_TEST)
# define B_TEST (-1)
#endif

#if (HOST_BOARD == LCR3K) && (B_TEST < 0)
       U32 fixup_ctl_sw();
extern U32 ctl_sw_asm();
#endif

#ifndef AN_BALL
#define AN_BALL (AN_A_STMP+'O'-'A')
#endif
#ifndef AN_DOT
#define AN_DOT (AN_A_STMP+'.'-'A')
#endif

#define	OFF	('X'-'A'+AN_A_STMP)			/* BUTTON GRAPHICS	*/
#define	ON	('O'-'A'+AN_A_STMP)

#if SST_GAME	/* for now */
# define J1_X	54
# define J1_Y	26
#else		/* for now */
#if J3_BITS == 0
# define J1_X	(AN_VIS_COL/5)
# define J1_Y 	(16)
# define J2_X	((3*AN_VIS_COL)/5)
# define J2_Y	(16)
#else
# define J1_X	(AN_VIS_COL/6)
# define J1_Y 	(16)
# define J2_X	(2*AN_VIS_COL/6)
# define J2_Y	(16)
# define J3_X	(3*AN_VIS_COL/6)
# define J3_Y 	(16)
# define J4_X	(4*AN_VIS_COL/6)
# define J4_Y	(16)
#endif
#endif		/* for now */

unsigned long fake_controls;


#if (0)
typedef struct
{
	U32	mask;
	U8	x,y;
	const char * const label;
} S_BTN;
#define BTN(mask, x, y, label, color) { mask, x, y, label }
#else
typedef struct
{
	const char * const label;
	U32	mask;
	U8	x,y;
	U8	color;
} S_BTN;
#define BTN(mask, x, y, label, color) { label, mask, x, y, color }
#endif
#ifdef CN_STATUS
#define CN_X (10)
#if SST_GAME	/* for now */
#define CN_Y (12)
#else						/* for now */
#define CN_Y (9)
#endif						/* for now */
static const S_BTN	cnsw[] =
{
    BTN( (1<<B_COINL),	CN_X,    CN_Y, " 1", 0),
    BTN( (1<<B_COINR),	CN_X+3,  CN_Y, " 2", 0),
    BTN( (1<<B_COIN3),	CN_X+6,  CN_Y, " 3", 0),
    BTN( (1<<B_COIN4),	CN_X+9,  CN_Y, " 4", 0),
    BTN( (1<<B_DOLLAR),	CN_X+12, CN_Y, "BILL", 0),
    BTN( 0, 0, 0, 0, 0)
};
#endif

static const S_BTN	btn[] =
{
#if (COJAG_GAME&COJAG_AREA51)
# define FIRST_NON_DEBUG (6)
#else
# define FIRST_NON_DEBUG (0)
#endif
#ifdef J1_FIRE
	BTN(J1_FIRE,J1_X+4,J1_Y+2,"FIRE", 0),
#endif
#ifdef J1_A
	BTN(J1_A,J1_X+4,J1_Y-1,"A", 0),
#endif
#ifdef J1_B
	BTN(J1_B,J1_X+7,J1_Y-1,"B", 0),
#endif
#ifdef J2_FIRE
	BTN(J2_FIRE,J2_X+4,J2_Y+2,"FIRE", 0),
#endif
#ifdef J2_A
	BTN(J2_A,J2_X+4,J2_Y-1,"A", 0),
#endif
#ifdef J2_B
	BTN(J2_B,J2_X+7,J2_Y-1,"B", 0),
#endif
#ifdef J1_TRIG
	BTN(J1_TRIG,J1_X+7,J1_Y+4,"TRIGGER", 0),
	BTN(J1_START,J1_X-1,J1_Y+4,"START", 0),
#else
# if J1_START
	BTN(J1_START,J1_X+9,J1_Y+2,"START", 0),
# endif
#endif
#ifdef J2_TRIG
	BTN(J2_TRIG,J2_X+7,J2_Y+4,"TRIGGER", 0),
	BTN(J2_START,J2_X-1,J2_Y+4,"START", 0),
#else
# ifdef J2_START
	BTN(J2_START,J2_X+9,J2_Y+2,"START", 0),
# endif
#endif
#if (SST_GAME & SST_RUSH)	/* for now */
	BTN(SW_ABORT,54,24,"ABORT", 0),
	BTN(SW_VIEW1,4,24,"VIEW1", 0),
	BTN(SW_VIEW2,10,24,"VIEW2", 0),
	BTN(SW_VIEW3,16,24,"VIEW3", 0),
	BTN(SW_MUSIC,10,21,"MUSIC", 0),
	BTN(SW_TRACK1,13,16,"TRACK1", 0),
	BTN(SW_TRACK2,20,16,"TRACK2", 0),
	BTN(SW_TRACK3,27,16,"TRACK3", 0),
	BTN(SW_TRACK4,34,16,"TRACK4", 0),
	BTN(SW_JOIN,41,16,"JOIN", 0),
	BTN(SW_REVERSE,54,39,"REVERSE", 0),
	BTN(SW_FIRST,53,33," 1", 0),
	BTN(SW_SECOND,53,36," 2", 0),
	BTN(SW_THIRD,55,33," 3", 0),
	BTN(SW_FOURTH,55,36," 4", 0),
	BTN(SW_TILT,4,29,"TILT", 0),
	BTN(SW_CREDIT,4,41,"SERVICE CREDIT", 0),
	BTN(SW_VOL_DN,4,38,"VOL DOWN", 0),
	BTN(SW_VOL_UP,4,35,"VOL UP", 0),
	BTN(SW_TEST,4,32,"TEST MODE", 0),
#endif				/* for now */
#if SST_GAME && ((SST_GAME & SST_RUSH) == 0)	/* for now */
	BTN(J_UP,54,23,"UP", 0),
	BTN(J_DOWN,54,31,"DOWN", 0),
	BTN(J_LEFT,49,27,"LEFT", 0),
	BTN(J_RIGHT,58,27,"RIGHT", 0),
	BTN(SW_WHITE,20,27,"WHITE", 0),
	BTN(SW_GREEN,10,27,"GREEN", 0),
	BTN(SW15,5,18,"TEST", 0),
	BTN(SW14,10,18,"SW14", 0),
	BTN(SW13,15,18,"SW13", 0),
	BTN(SW12,20,18,"SW12", 0),
	BTN(SW11,25,18,"SW11", 0),
	BTN(SW10,30,18,"SW10", 0),
	BTN(SW09,35,18,"SW09", 0),
	BTN(SW08,40,18,"SW08", 0),
	BTN(SW01,45,18,"SW01", 0),
#endif						/* for now */
	BTN(0,0,0,0, 0),
};

#define EX_FRAMES (60)
#define RES_FRAMES (90)

/*		st_joy_disp()
 *	Just to have a single place to change how we draw the little
 *	"tic-tac-toe" display of a joystick used in swt_test.c and
 *	possibly snd_test.c and pots.c.
 */

void st_joy_disp( cx, cy, sw )
int cx;			/* center X coordinate */
int cy;			/* center Y cordinate */
unsigned long sw;	/* Switch bits */
{
    int row,col;
    int bits;		/* LSB is top left. 1 == BALL */

    bits = 0x10;
    if ( sw & J_LEFT ) bits >>= 1;
    else if ( sw & J_RIGHT ) bits <<= 1;
    if ( sw & J_UP ) bits >>= 3;
    else if ( sw & J_DOWN ) bits <<= 3;
    /* paint the pattern from left-to-right, top-to-bottom */
    for ( row = 0; row < 3 ; ++row ) {
#if (0)
	txt_clr_wid(cx-1,cy-1+row,3);
#endif
	for ( col = 0 ; col < 3 ; ++col ) {
	    txt_stamp(col+cx-1,row+cy-1,(bits & 1) ? AN_BALL : AN_DOT,WHT_PAL);
	    bits >>= 1;
	}
    }
}

enum num_swts {
	SWT_INPUTS,
        SWT_LAST,
	SWT_SW,
	SWT_DEBUG,
	SWT_MAX
};

#if (HOST_BOARD == PHOENIX) && (SST_GAME & SST_RUSH)	/* for now */

#define FORCE_MSG " use COIN switches, START and VIEW1 buttons"
#define LAMPS (*(volatile char *)(XBUS_SLOT4+0x0000080))
#define SWFORCE (*(volatile char *)(XBUS_SLOT5+0x0000080))
#define LSCLK 0x1f
#define MSCLK 0xe0
#define SWMIN (S16)(-127)
#define SWMAX (S16)(126)
#define SWCEN (S16)(0)

unsigned long gopts;
int SWInit;
S16 sw_val;
S16 swpos;
S16 last_sw_val;
S16 last_swpos;

char xor(char *start_val)
{
    char save_val,end_val;
    int i;
    U8 mask_val;
    
    end_val = 0;
    save_val = *start_val;
    mask_val = 1;

    for (i = 1; i <= 8; i++) {
        if (!(mask_val & save_val)) {
            end_val |= mask_val;
        }
        mask_val = 1<<i;
    }
    return(end_val);
}

void SetSWForce(S16 force)
{
  SWFORCE = ~force;
  txt_hexnum(38,25,~force,2,2,RED_PAL);
}

void wheelforce(U32 sw)
{
 if (sw & J_UP) sw_val += 0x10;
 else if (sw & J_DOWN) sw_val -= 0x10;
 else if (sw & SW_COINL) sw_val++;
 else if (sw & SW_COINR) sw_val--;
 if ( sw & SW_ACTION )
 {
  SWInit = 1;
  SetSWForce(sw_val);
 }
 else if ( SWInit )
 {
  SWInit = 0;
  SetSWForce(SWCEN);	/* Init SW */
 }
}

# if USE_XBUSMON_CTLMODLATCH == 0
static U32 cntr_bitshd;

U32 ctl_mod_latch(int x)
{
 if ( x )
 {
    U32 old;
    int oldipl = prc_set_ipl(INTS_OFF);
    old = cntr_bitshd;
    if (x < 0) {
	cntr_bitshd &= x;
    } else {
	cntr_bitshd |= x;
    }
    LAMPS = cntr_bitshd;
    prc_set_ipl(oldipl);
    return old;
 }
 return cntr_bitshd;
}
# endif

#endif							/* for now */

#if HOST_BOARD == FLAGSTAFF	/* for now */

int last_sw;
int mod_sw;
int ForceInit;

S16 sw_val;
S16 swpos;
S16 last_sw_val;
S16 last_swpos;

#define LMA_DATA_MASK	0x00FF		/* 8 bits data */
#define LMA_ADDR_MASK	0x0700		/* 3 bits address */
#define LMA_ADDR_LAMPS	0x0400		/* lamps address */
#define LMA_ADDR_POWER	0x0500		/* high power lamp address */
#define LMA_DATA_STROBE	0x0800		/* 1 bit strobe */

static U32 cntr_bitshd;

void lma_init()
{
 int oldipl = prc_set_ipl(INTS_OFF);
 cntr_bitshd = 0;
 *(VU32 *)IO_GP_OUTPUTS = 0;
 *(VU32 *)IO_GP_OUTPUTS = 0 | LMA_DATA_STROBE;
 *(VU32 *)IO_GP_OUTPUTS = LMA_ADDR_LAMPS;
 *(VU32 *)IO_GP_OUTPUTS = LMA_ADDR_LAMPS | LMA_DATA_STROBE;
 *(VU32 *)IO_GP_OUTPUTS = LMA_ADDR_POWER;
 *(VU32 *)IO_GP_OUTPUTS = LMA_ADDR_POWER | LMA_DATA_STROBE;
 prc_set_ipl(oldipl);
}

# if USE_XBUSMON_CTLMODLATCH == 0

U32 ctl_mod_latch(int x)
{
 if ( x )
 {
  U32 old, lamps;
  int oldipl = prc_set_ipl(INTS_OFF);
  old = cntr_bitshd;
  if (x < 0)
  {
   cntr_bitshd &= x;
  }
  else
  {
   cntr_bitshd |= x;
  }
  lamps = cntr_bitshd ^ old;
  if ( lamps & LA_LAMP_MASK )
  {
   U32 output = LMA_ADDR_LAMPS | ( cntr_bitshd & LA_LAMP_MASK );
   *(VU32 *)IO_GP_OUTPUTS = output;
   *(VU32 *)IO_GP_OUTPUTS = output | LMA_DATA_STROBE;
  }
  if ( lamps & LA_POWER_MASK )
  {
   U32 output = LMA_ADDR_POWER | ( ( cntr_bitshd & LA_POWER_MASK ) >> 8 );
   *(VU32 *)IO_GP_OUTPUTS = output;
   *(VU32 *)IO_GP_OUTPUTS = output | LMA_DATA_STROBE;
  }
  prc_set_ipl(oldipl);
  return old;
 }
 return cntr_bitshd;
}
# endif

void SetForce(S16 force)
{
 int oldipl = prc_set_ipl(INTS_OFF);
 *(VU32 *)IO_GP_OUTPUTS = ( force & LMA_DATA_MASK );
 *(VU32 *)IO_GP_OUTPUTS = ( force & LMA_DATA_MASK ) | LMA_DATA_STROBE;
 prc_set_ipl(oldipl);
}

void force_test(U32 sw)
{
#if BOOT_FROM_DISK
 if (sw & J_UP) sw_val += 0x10;
 else if (sw & J_DOWN) sw_val -= 0x10;
 else if (sw & J_LEFT) sw_val++;
 else if (sw & J_RIGHT) sw_val--;
#else
 if ( sw_val == 0x80 ) ForceInit = 1;
 if ( sw_val == 0x7f ) ForceInit = -1;
 sw_val += ForceInit;
 sw_val &= 0xff;
#endif
 if ( sw & SW_ACTION )
 {
#if BOOT_FROM_DISK
  ForceInit = 1;
#else
  if ( ForceInit == 0 ) ForceInit = 1;
#endif
  SetForce(sw_val);
  txt_hexnum(39,25,sw_val,2,2,RED_PAL);
  if ( sw_val )
  {
   txt_clr_wid(28,21,13);
   if ( sw_val & 0x80 )
    txt_str(28,21,"CCW",RED_PAL);
   else
    txt_str(39,21,"CW",RED_PAL);
  }
 }
 else if ( ForceInit )
 {
#if !BOOT_FROM_DISK
  sw_val = 0;
#endif
  ForceInit = 0;
  SetForce( 0 );    /* Init SW */
  txt_hexnum(39,25,0,2,2,RED_PAL);
  txt_clr_wid(28,21,13);
 }
}

#endif				/* for now */

int
SwitchTest(smp)
const struct menu_d *smp;
{
	m_int	ex_cnt;
	int bottom, row;
	int debugging,frames;
	U32 prev_swt[SWT_MAX];

#if (POT_CNT)
	m_int	res_cnt;
	S16 prev[POT_CNT];
#endif

	frames = 0;
	debugging = debug_mode & GUTS_OPT_DEVEL;
	if ( debugging ) {
	    prev_swt[SWT_LAST] = 1;
	    row = 4;
	    txt_str(3,row++,"ADDRESS    NAME     RAW DATA",WHT_PAL);
	    txt_str(3,row++,"-------- ---------  --------",WHT_PAL);
#ifdef JAMMA
	    txt_hexnum(3,row,(unsigned long)&JAMMA,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "JAMMA", WHT_PAL);
#endif
#if (HOST_BOARD == PHOENIX) && (SST_GAME & SST_RUSH)	/* for now */
	    txt_hexnum(3,row,(unsigned long)&START,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "TEST", WHT_PAL);
	    txt_hexnum(3,row,(unsigned long)&INPUTS,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "INPUTS", WHT_PAL);
	    txt_hexnum(3,row,(unsigned long)&DIAG0,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "DIAG0", WHT_PAL);
	    txt_hexnum(3,row,(unsigned long)&DIAG4,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "DIAG4", WHT_PAL);
#else							/* for now */
# ifdef INPUTS
	    txt_hexnum(3,row,(unsigned long)&INPUTS,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "INPUTS", WHT_PAL);
# endif
# ifdef EXTEND
	    txt_hexnum(3,row,(unsigned long)&EXTEND,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "EXTEND", WHT_PAL);
# endif
# ifdef MISC
	    txt_hexnum(3,row,(unsigned long)&MISC,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "MISC", WHT_PAL);
# endif
# ifdef DIAG
	    txt_hexnum(3,row,(unsigned long)&DIAG,8,0,WHT_PAL);
	    txt_str(3+8+1, row++,     "DIAG", WHT_PAL);
# endif
#endif							/* for now */
	}
#ifdef CN_STATUS
	txt_str(CN_X-7,CN_Y-1,"COINS",WHT_PAL);
#endif

	bottom = st_insn(AN_VIS_ROW-2,t_msg_ret_menu,t_msg_nexth,INSTR_PAL);

#ifdef SW_TILT
# ifdef COUNTER_ON
	bottom = st_insn(bottom, "To activate coin counter,", "press TILT switch", INSTR_PAL);
# endif
#endif

#if POT_CNT
	prev[0] = -32767;

	bottom = st_insn(bottom,"To reset limits,",t_msg_extrah,INSTR_PAL);
	res_cnt = 0;
#endif

#if (HOST_BOARD == PHOENIX) && (SST_GAME & SST_RUSH)	/* for now */

	gopts = eer_gets(EER_GUTS_OPT);
	last_sw_val = last_swpos = -1;
	SetSWForce(SWCEN);	/* Init SW */
	SWInit = 0;
	sw_val = 0;		/* Set value for wheel force test */

	txt_str(27,19,"STEERING WHEEL", WHT_PAL);
	txt_str(24,21,"POSITION  FORCE TEST", WHT_PAL);
	txt_str(24,25,"SET FORCE", RED_PAL);

	bottom = st_insn(bottom,"To test wheel force,",t_msg_action,INSTR_PAL);
	bottom = st_insn(bottom,"To set wheel force,",FORCE_MSG,INSTR_PAL);

#endif							/* for now */

#if HOST_BOARD == FLAGSTAFF	/* for now */

	SetForce(0);		/* Init Force */
	ForceInit = 0;
	last_sw_val = last_sw = -1;
	sw_val = 0;		/* Set value for wheel force test */

	txt_str(3,15,"OPTIONAL",WHT_PAL);
	txt_str(28,23,"FORCE TEST", WHT_PAL);
	txt_str(28,25,"SET FORCE", RED_PAL);

	bottom = st_insn(bottom,"To test wheel force,",t_msg_action,INSTR_PAL);
# if BOOT_FROM_DISK
	bottom = st_insn(bottom,"To set wheel force,",t_msg_control,INSTR_PAL);
# endif

#endif	 			/* for now */

	prc_delay0();

	prev_swt[SWT_SW] = ~ctl_read_sw(0);

	for(ex_cnt = EX_FRAMES ; ex_cnt > 0 ;)
	{
		U32	sw;
		VU16	*swp;
		const S_BTN	*p;
		int	pal;

		pal = 0;
		swp = 0;		/* Quiet warnings in degenerate case */
		if ( debugging ) {
		    row = 6;
# ifdef JAMMA
		    swp = (VU16 *)&JAMMA;
		    sw = (*swp <<16) | swp[1];
		    txt_hexnum(3+8+1+10+1,row++,sw,8,0,RED_PAL);	/* display raw data */
# endif
# ifdef READ_RAW_SWITCHES
		    sw = READ_RAW_SWITCHES(0);
# else
#  ifdef INPUTS
		    swp = (VU16 *)&INPUTS;
		    sw = (*swp <<16) | swp[1];
#  endif
# endif
#if (HOST_BOARD == PHOENIX) && (SST_GAME & SST_RUSH)	/* for now */
{
		int dbg = READ_RAW_SWITCHES(1);
		    if (prev_swt[SWT_LAST] || prev_swt[SWT_INPUTS] != sw || prev_swt[SWT_DEBUG] != dbg) {
			txt_hexnum(3+8+1+10+1,row++,START&0xffff,8,0,RED_PAL);	/* display raw data */
			txt_hexnum(3+8+1+10+1,row++,INPUTS&0xffff,8,0,RED_PAL);	/* display raw data */
			txt_hexnum(3+8+1+10+1,row++,DIAG0&0xffff,8,0,RED_PAL);	/* display raw data */
			txt_hexnum(3+8+1+10+1,row++,DIAG4&0xffff,8,0,RED_PAL);	/* display raw data */
			prev_swt[SWT_INPUTS] = sw;
			prev_swt[SWT_DEBUG] = dbg;
		    }
}
#else							/* for now */
# if SST_GAME
#  ifdef INPUTS
	    txt_hexnum(3+8+1+10+1,row++,INPUTS,8,0,RED_PAL);	/* display raw data */
#  endif
#  ifdef EXTEND
	    txt_hexnum(3+8+1+10+1,row++,EXTEND,8,0,RED_PAL);	/* display raw data */
#  endif
#  ifdef MISC
	    txt_hexnum(3+8+1+10+1,row++,MISC,8,0,RED_PAL);	/* display raw data */
#  endif
#  ifdef DIAG
	    txt_hexnum(3+8+1+10+1,row++,DIAG,8,0,RED_PAL);	/* display raw data */
#  endif
# else
#  if defined(READ_RAW_SWITCHES) || defined(INPUTS)
		    if (prev_swt[SWT_LAST] || prev_swt[SWT_INPUTS] != sw) {
			txt_hexnum(3+8+1+10+1,row,sw,8,0,RED_PAL);	/* display raw data */
			prev_swt[SWT_INPUTS] = sw;
		    }
		    ++row;
#  endif
# endif
#endif				/* for now */
		    prev_swt[SWT_LAST] = 0;
		}
/* This would be a great place to add use of ST_E_CRPT, if we thought we
 * would ever again read coins via JSA III.
 */
#ifdef CN_STATUS
		sw = ~(CN_STATUS);

		/* coin switches...	*/
		for( p=&cnsw[0]; p->mask != 0; p++ )
		{
			txt_stamp( p->x,p->y,(sw & p->mask)? ON:OFF,RED_PAL );
			if( p->label )
				txt_str( p->x-1,p->y-1,p->label,WHT_PAL );
		}
#endif
		sw = ctl_read_sw(0);
		if (sw != prev_swt[SWT_SW]) {
#if (COJAG_GAME&COJAG_AREA51)
		    if ( debugging ) 
#endif
		    {
#if J1_BITS && J1_X
			if ((sw&J1_BITS) != (prev_swt[SWT_SW]&J1_BITS))
			    st_joy_disp(J1_X,J1_Y,(sw & J1_BITS));
#endif
#if J2_BITS && J2_X
			if ((sw&J2_BITS) != (prev_swt[SWT_SW]&J2_BITS))
			    st_joy_disp(J2_X,J2_Y,(sw & J2_BITS));
#endif
#if J3_BITS && J3_X
			if ((sw&J3_BITS) != (prev_swt[SWT_SW]&J3_BITS))
			    st_joy_disp(J3_X,J3_Y,(sw & J3_BITS));
#endif
#if J4_BITS && J4_X
			if ((sw&J4_BITS) != (prev_swt[SWT_SW]&J4_BITS))
			    st_joy_disp(J4_X,J4_Y,(sw & J4_BITS));
#endif

		    }
		    /* misc buttons...	*/
		    p = btn;
		    if ( !debugging ) p += FIRST_NON_DEBUG;
		    for( ; p->mask != 0; p++ )
		    {
			if ((sw&p->mask) != (prev_swt[SWT_SW]&p->mask)) {
			    cwrite( p->x,p->y,(sw & p->mask)? ON:OFF,RED_PAL );
			    if( p->label )
				    txt_str( p->x-1,p->y-1,p->label,WHT_PAL );
			}
		    }

#ifdef LB_CABLIGHT
		    /* if left trigger AND thumb depressed, turn on strobe... */
		    if( (sw & (J1_TRIG|J1_THUMB)) == (J1_TRIG|J1_THUMB) )
		    {
			    prc_mod_latch(1 << (LB_CABLIGHT+16));
			    pal = YEL_PAL;
		    }
		    else
		    {
			    prc_mod_latch(~(1 << (LB_CABLIGHT+16)));
			    pal = WHT_PAL;
		    }
		    if ( (sw & (J1_TRIG|J1_THUMB)) != (prev_swt[SWT_SW]&(J1_TRIG|J1_THUMB)) ) {
			txt_str(27,3," CAB LIGHT",pal);
			txt_str(27,4,"(lt trg-thm)",pal);
		    }
#endif
		    prev_swt[SWT_SW] = sw;
		}

#if (SST_GAME & SST_RUSH)	/* for now */
# if (HOST_BOARD == PHOENIX)	/* for now */

		if (sw & SW_VIEW1) ctl_mod_latch(LA_VIEW1); else ctl_mod_latch(~LA_VIEW1);
		if (sw & SW_VIEW2) ctl_mod_latch(LA_VIEW2); else ctl_mod_latch(~LA_VIEW2);
		if (sw & SW_VIEW3) ctl_mod_latch(LA_VIEW3); else ctl_mod_latch(~LA_VIEW3);
		if (sw & SW_MUSIC) ctl_mod_latch(LA_MUSIC); else ctl_mod_latch(~LA_MUSIC);
		if (sw & SW_ABORT) ctl_mod_latch(LA_ABORT); else ctl_mod_latch(~LA_ABORT);
		if (sw & SW_REVERSE) ctl_mod_latch(LA_REVERSE); else ctl_mod_latch(~LA_REVERSE);
		if (sw & SW_FIRST) ctl_mod_latch(LA_NOT_LDR); else ctl_mod_latch(~LA_NOT_LDR);

# else				/* for now */

#ifdef SW_TILT
# ifdef COUNTER_ON
		if (sw & SW_TILT) COUNTER_ON( B_EMC );
		else COUNTER_OFF( B_EMC );
# endif
#endif

		mod_sw = last_sw ^ sw;

		if (mod_sw & SW_VIEW1)
		if (sw & SW_VIEW1) ctl_mod_latch(LA_VIEW1); else ctl_mod_latch(~LA_VIEW1);
		if (mod_sw & SW_VIEW2)
		if (sw & SW_VIEW2) ctl_mod_latch(LA_VIEW2); else ctl_mod_latch(~LA_VIEW2);
		if (mod_sw & SW_VIEW3)
		if (sw & SW_VIEW3) ctl_mod_latch(LA_VIEW3); else ctl_mod_latch(~LA_VIEW3);
		if (mod_sw & SW_MUSIC)
		if (sw & SW_MUSIC) ctl_mod_latch(LA_MUSIC); else ctl_mod_latch(~LA_MUSIC);
		if (mod_sw & SW_ABORT)
		if (sw & SW_ABORT) ctl_mod_latch(LA_ABORT); else ctl_mod_latch(~LA_ABORT);
		if (mod_sw & SW_FIRST)
		if (sw & SW_FIRST) ctl_mod_latch(LA_LEADER); else ctl_mod_latch(~LA_LEADER);
		if (mod_sw & SW_THIRD)
		if (sw & SW_THIRD) ctl_mod_latch(LA_WINNER); else ctl_mod_latch(~LA_WINNER);

		last_sw = sw;

# endif				/* for now */

# if POT_CNT
		swpos = pot_raw(POT_WHEEL);
# endif

# if (HOST_BOARD == PHOENIX)	/* for now */

		if (last_swpos != swpos) {
		    txt_hexnum(27,23,swpos,2,0,WHT_PAL);
		    last_swpos = swpos;   }

		wheelforce( ctl_read_sw(J_UP|J_DOWN|SW_COINL|SW_COINR) );

		if (last_sw_val != sw_val) {
		    txt_hexnum(38,23,sw_val,2,0,WHT_PAL);
		    last_sw_val = sw_val;   }

# else				/* for now */

		if ( ++frames > 30)
		{
		 force_test( ctl_read_sw(J_UP|J_DOWN|J_LEFT|J_RIGHT) );
		 frames = 30;
		}

		if (last_sw_val != sw_val) {
		    txt_hexnum(39,23,sw_val,2,0,WHT_PAL);
		    last_sw_val = sw_val;   }

# endif				/* for now */

#endif							/* for now */

#if POT_CNT
		pot_display(prev);

		/* if trigger HELD, reset the pots... */

		if ( sw & SW_EXTRA ) {
			if ( ++res_cnt > RES_FRAMES ) {
			    pot_init(-1);
			    res_cnt = 0;
			}
		} else res_cnt = 0;
#endif

		prc_delay0();

		/* if SW_NEXT held for 2 seconds, exit switch test */
		if ( sw & SW_NEXT ) --ex_cnt;
		else ex_cnt = EX_FRAMES;
	}
#if POT_CNT
	pot_save();
#endif
#if (HOST_BOARD == PHOENIX) && (SST_GAME & SST_RUSH)	/* for now */
	ctl_mod_latch(0x80000000);	/* turn off lamps */
	ctl_mod_latch(LA_NOT_LDR);	/* turn off inverted lamp */
#endif							/* for now */
#if HOST_BOARD == FLAGSTAFF	/* for now */
	lma_init();
#endif				/* for now */
	ctl_read_sw(JOY_ALL);	/* kill edges */
	return 0;
}

unsigned long dbswitch;		/* user visible debounced switches */

struct switch_state {
    /* First the successively "cooked" form of the switches */
    unsigned long prev;		/* previous sample of switches */
    unsigned long db;		/* Debounced switches */
    /* following _two_ variables for edges allow "single writer"
     * operation, which should help FOM avoid mega-bounce without
     * calling prc_set_ipl() in ctl_read_sw.
     */
    unsigned long edge_i;	/* Interrupt messes with this */
    unsigned long edge_m;	/* Mainline messes with this */
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
    unsigned long dying,old,changes,stable,edge;
    old = sw->prev;
    sw->prev = new;

/*  The following code is a 'C' translation of the 68000 assembly
 *  in lipson/joy_sw.asm. It contains several oddities....
 */
    changes = old ^ new;
    stable = ~changes;
    dying = (changes & sw->db);		/* ones where changing to off? */
    sw->db = dying | (new & stable);	/* debounce = dying 1s or stable ones*/
    edge = sw->edge_i ^ sw->edge_m;	/* Different means edge seen */
    edge &= sw->db;			/* Kill "seen" if switch off */
    if ( (dying & sw->rpt) == 0 && (sw->db & sw->rpt) ) {
	/* at least one of our auto-repeat switches is on, and
	 * none are dying, run the timer.
	 */
	if ( --(sw->timer) == 0 ) {
	    sw->timer = sw->delay;	/* re-init to "later" delay */
	    edge &= ~sw->rpt;		/* re-trigger selected switches */
	}
    } else sw->timer = sw->init;	/* all off or dying?, init timer */
    /* now sync edge_i to edge_m where edges == 0, force difference
     * where edges == 1.
     */
    sw->edge_i = sw->edge_m ^ edge;
    return sw->db;
}

#if (POT_CNT)
extern void pot_vbread();
extern U32 PotsToSwitches();
#endif

#if (HOST_BOARD == LCR3K) && (B_TEST < 0)
U32 fixup_ctl_sw() {
    U32 raw;
    raw = ctl_sw_asm();			/* get switches from hardware */
    if ((raw&(J2_UP|J2_DOWN)) == (J2_UP|J2_DOWN)) {
       raw &= ~(J2_UP|J2_DOWN);
       raw |= SW_ACTION;
    }
    if ((raw&(J2_LEFT|J2_RIGHT)) == (J2_LEFT|J2_RIGHT)) {
       raw &= ~(J2_LEFT|J2_RIGHT);
       raw |= SW_NEXT;
    }
    return raw;
}
#endif

#if (COJAG_GAME&COJAG_AREA51)
extern void gun_joy();	/* Should be U32, return controls */
static int gun_kluge_timer;
#define GUN_LAG (5)
#endif

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

# ifdef DIAG
	swp = (VU16 *)&DIAG;
	raw = (swp[0] <<16) | swp[1];
	(void) debounce_swtch(&db_state,~raw);
# endif
# ifdef JAMMA
	swp = (VU16 *)&JAMMA;
# else
	swp = (VU16 *)&INPUTS;
# endif
	raw = (swp[0] <<16) | swp[1];
	raw = ~raw;			/* Switches are low_true, invert them */
    }
#endif

#if (POT_CNT)
# if 0
    raw &= ~JOY_BITS;
# endif
    raw |= fake_controls;		/* merge in the pots */
#endif
#if (COJAG_GAME&COJAG_AREA51)
    /* We are violating the "single writer" rule here, but the
     * other writer (ctl_read_sw()) never reads, so this should
     * be safe.
    */
    if ( gun_kluge_timer && --gun_kluge_timer ) gun_joy();
    raw |= fake_controls;		/* merge in the gun "switches" */
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
    unsigned long edges,edge_bits,val;

    struct switch_state *sw = &sw_state;

    edge_bits = sw->edge_i ^ sw->edge_m;	/* ones where different */
#ifdef EARLY_ON
    val = sw->db | sw->prev;
#else
    val = sw->db;
#endif
    if ( edge_mask != 0) {
	edges = ~edge_bits & val & edge_mask;
	edge_bits |= edges;
	/* Make edge_m different from edge_i where we want "edge bits"
	 */
	sw->edge_m = sw->edge_i ^ edge_bits;
	val = (val & ~edge_mask) | edges;
    }
#if GUN_UP
    if ( edge_mask & (GUN_UP|GUN_DOWN|GUN_LEFT|GUN_RIGHT) ) gun_kluge_timer = GUN_LAG;
#endif
    return val;
}

/*	More-or-less the same thing, but for the debug (Diagnostic)
 *	switches.
 */
unsigned long
ctl_read_debug(edge_mask)
unsigned long edge_mask;
{
    unsigned long edges,edge_bits,val;

    struct switch_state *sw = &db_state;

    val = debug_mode & GUTS_OPT_DBG_SW;
    if ( val == 0 ) return val;
 
    val = sw->db;
    if ( edge_mask != 0) {
	edge_bits = sw->edge_i ^ sw->edge_m;	/* ones where different */
	edges = (edge_bits ^ val) & edge_mask;
	edge_bits |= edges;
	/* Make edge_m different from edge_i where we want "edge bits"
	 */
	sw->edge_m = sw->edge_i ^ edge_bits;
	val = (val & ~edge_mask) | edges;
    }
    return val;
}

