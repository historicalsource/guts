#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>

#define EX_FRAMES (60)
#define RES_FRAMES (60)

extern	int	net_test(const struct menu_d *);

static int wb_aux, sw_val, ForceInit;
static int last_sw_val, mod_sw;

#define FORCE_TST_MSG_X	(28)
#define FORCE_TST_MSG_Y (28)

static void SetForce(S16 f)
{
 int force = ( f & 0xff ) ^ 0xff;
 int oldipl = prc_set_ipl(INTS_OFF);
 if ( wb_aux != VMDVR_WHEEL )
 {
  wb_aux = VMDVR_WHEEL;
  *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | VMDVR_WHEEL;
  *(VU32 *)WB_AUX = M_VMDVR_ADDR_STROBE | VMDVR_WHEEL;
  *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | VMDVR_WHEEL;
 }
 *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | force;
 *(VU32 *)WB_AUX = M_VMDVR_DATA_STROBE | force;
 *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | force;
 prc_set_ipl(oldipl);
}

static U32 cntr_bitshd;

U32 ctl_mod_latch(int x)
{
 if ( x )
 {
  U32 old;
  int oldipl = prc_set_ipl(INTS_OFF);
  old = cntr_bitshd;
  if (x < 0)
  {
   cntr_bitshd &= ( x & M_VMDVR_DATA );
  }
  else
  {
   cntr_bitshd |= ( x & M_VMDVR_DATA );
  }
  if ( wb_aux != VMDVR_LAMPS )
  {
   wb_aux = VMDVR_LAMPS;
   *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | VMDVR_LAMPS;
   *(VU32 *)WB_AUX = M_VMDVR_ADDR_STROBE | VMDVR_LAMPS;
   *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | VMDVR_LAMPS;
  }
  *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | cntr_bitshd;
  *(VU32 *)WB_AUX = M_VMDVR_DATA_STROBE | cntr_bitshd;
  *(VU32 *)WB_AUX = M_VMDVR_SETUP_HOLD | cntr_bitshd;
  prc_set_ipl(oldipl);
  return old;
 }
 return cntr_bitshd;
}

static void lma_init()
{
 int oldipl = prc_set_ipl(INTS_OFF);
 cntr_bitshd = 0;
 SetForce(0);
 prc_set_ipl(oldipl);
}

void force_test(U32 sw)
{
 if (sw & J_UP) sw_val += 0x10;
 else if (sw & J_DOWN) sw_val -= 0x10;
 else if (sw & J_LEFT) sw_val++;
 else if (sw & J_RIGHT) sw_val--;
 txt_clr_wid(FORCE_TST_MSG_X, FORCE_TST_MSG_Y+2, 10);
 if ( sw & P1_C )
 {
  ForceInit = 1;
  SetForce(sw_val);
  txt_hexnum(FORCE_TST_MSG_X+12, FORCE_TST_MSG_Y+4, sw_val, 2, 2, RED_PAL);
  if ( sw_val )
  {
    txt_str(FORCE_TST_MSG_X, FORCE_TST_MSG_Y+2, ( sw_val & 0x80 ) ? "CCW" : "        CW", RED_PAL);
  }
 }
 else if ( ForceInit )
 {
  ForceInit = 0;
  SetForce( 0 );    /* Init SW */
  txt_hexnum(FORCE_TST_MSG_X+12, FORCE_TST_MSG_Y+4, sw_val, 2, 2, WHT_PAL);
 }
}

extern int pot_only_when_ok;

static int pot_test(const struct menu_d *smp) {
    int bottom;
    int frames;
    U32 sw;
    S16 prev[POT_CNT];
    U32 lights_been_on=0;

    pot_only_when_ok = 1;	/* enable pot servicing */
    pot_init(0);		/* reload all the pot tables */
    prc_delay(4);		/* give 'em time to settle */

    frames = 0;
    memset(prev, 0, sizeof(prev));
    prev[0] = -32767;		/* signal to draw the headers */

    SetForce(0);		/* Init Force */
    ForceInit = 0;
    last_sw_val = -1;
    sw_val = 0;		/* Set value for wheel force test */

    txt_str(FORCE_TST_MSG_X, FORCE_TST_MSG_Y, "FORCE TEST", WHT_PAL);
    txt_str(FORCE_TST_MSG_X-1, FORCE_TST_MSG_Y+4, "FORCE VALUE:", YEL_PAL);

    bottom = st_insn(AN_VIS_ROW-2, t_msg_ret_menu, "press P1-A", INSTR_PAL);
    bottom = st_insn(bottom,"To reset pot limits,","press P1-B",INSTR_PAL);
    bottom = st_insn(bottom,"To test wheel force,", "press P1-C", INSTR_PAL);
    bottom = st_insn(bottom,"To set wheel force,", "use P1 Joystick", INSTR_PAL);

    txt_str(-1, 5, "Output LAMP drivers", WHT_PAL);
    txt_str(-1, 7, " 0   1   2   3   4   5   6   7", WHT_PAL);
    st_insn(10, "To activate LAMPs,", "press P2 U/D/L/R/A/B/C/D", INSTR_PAL);

    while (1) {
	int swpos;
	const char *s;
	static const char c_on[] = "ON  ";
	static const char c_off[] = "OFF ";
	static const U32 light_bits[8] = {
	    J2_UP,J2_DOWN,J2_LEFT,J2_RIGHT,P2_A,P2_B,P2_C,P2_D
	};
	sw = ctl_read_sw(J2_UP|J2_DOWN|J2_LEFT|J2_RIGHT|P2_A|P2_B|P2_C|P2_D)&
    			(J2_UP|J2_DOWN|J2_LEFT|J2_RIGHT|P2_A|P2_B|P2_C|P2_D);
	mod_sw ^= sw;
	for (swpos=0; swpos < 8; ++swpos) {
	    int pal, onoff;
	    onoff = mod_sw & light_bits[swpos];
	    if (onoff) {
		ctl_mod_latch(1<<swpos);
		lights_been_on |= 1<<swpos;
		pal = RED_PAL;
		s = c_on;
	    } else {
		ctl_mod_latch(~(1<<swpos));
		pal = (lights_been_on & (1<<swpos)) ? GRN_PAL : YEL_PAL;
		s = c_off;
	    }
	    if (swpos == 0) {
		txt_str((AN_VIS_COL-34)/2+2, 8, s, pal);
	    } else {
		txt_cstr(s, pal);
	    }
	}

	swpos = pot_raw(POT_WHEEL);

	if ( ++frames > 30)
	{
	 force_test( ctl_read_sw(J1_UP|J1_DOWN|J1_LEFT|J1_RIGHT)&(J1_UP|J1_DOWN|J1_LEFT|J1_RIGHT|P1_C) );
	 frames = 30;
	}

	if (last_sw_val != sw_val) {
	    txt_hexnum(FORCE_TST_MSG_X+12, FORCE_TST_MSG_Y+4, sw_val, 2, 0, WHT_PAL);
	    last_sw_val = sw_val;   }

	pot_display(prev);

	if ( ctl_read_sw(P1_B)&P1_B ) {
	    pot_init(-1);
	}

	prc_delay0();

	if ( ctl_read_sw(P1_A)& P1_A ) break;
    }
    lma_init();
    ctl_read_sw(-1);		/* kill edges */
    pot_only_when_ok = 0;	/* disable pot servicing */
    return 0;
}

static const struct menu_d wb_menu[] = {
    {	"WIDGET + LMA TESTS",	0		},
    {	"\nCONTROLS AND LMA TESTS", pot_test	},
    {	"\nNETWORK TEST",	net_test	},
    { 0, 0 }
};

int wb_test(const struct menu_d *smp) {
#if 0
    int row = AN_VIS_ROW/2, time;
    txt_str(-1, row, "WARNING WARNING WARNING", RED_PAL|AN_BIG_SET);
    row += 3;
    txt_str(-1, row++, "Running any of the widget board tests", WHT_PAL);
    txt_str(-1, row++, "with a broken or missing Widget Board,", WHT_PAL);
    txt_str(-1, row++, "will cause the CPU to hang. If WDOG is", WHT_PAL);
    txt_str(-1, row++, "is enabled, it will timeout and cause a reset.", WHT_PAL);
    st_insn(AN_VIS_ROW-3, "To continue,", t_msg_action, INSTR_PAL);
    ExitInst(INSTR_PAL);
    time = 0;
    while (1) {
	int sw;
	prc_delay(0);
	sw = ctl_read_sw(SW_NEXT|SW_ACTION);
	if ((sw&SW_NEXT)) return 0;
	if ((sw&SW_ACTION)) break;
	if (++time > 5*60) break;
    }
    txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
    txt_clr_wid(2, AN_VIS_ROW-2, AN_VIS_COL-4);
    ctl_read_sw(-1);				/* eat all edges */
#endif
    *(VU32 *)WB_RST = 7;			/* unreset the board */
    *(VU32 *)WB_INT = 4;			/* enable SMC interrupts */
    return st_menu(wb_menu, sizeof(wb_menu[0]), MNORMAL_PAL, 0);
}


