/*
 * 	ram_test.c
 *
 *		Copyright 1991 through 1997 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: ram_test.c,v 1.22 1997/10/09 23:26:20 shepperd Exp $";
#endif
#include <config.h>
#include <string.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>

#if HOST_BOARD_CLASS && ((HOST_BOARD_CLASS&HOST_BOARD) == PHOENIX)
# define PHX 1
#else
# define PHX 0
#endif

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#define	STATIC	static

#define CUSHION (0x200)
#define GOOD_PAUSE (120)	/* frames, two seconds */
#define BAD_PAUSE (360)		/* frames, six seconds */
#define LOOP_CK_PAUSE (60)	/* pause for one-shot/loop check */

#define	MSG_COL	13
#define	MSG_ROW	12

struct ramt {
    struct menu_d	menu_desc;	/* _NOT_ a pointer */
    struct rdb		rdesc;		/* also _NOT_ a pointer */
    int			notes;		/* for slight customization */
};

STATIC void show_results
PARMS((
const struct rdb *rdp,
const struct rrb *result,
const struct ramt *smp,
int passes,
int errors
));

extern	char	end_static;

#define NOTE_NOINT (1)
#define NOTE_VIDINIT (2)
#define NOTE_VIDCLR (4)
#define NOTE_LOOPING (8)
#define NOTE_NOVID (0x10)
#define NOTE_QUICK (0x20)

STATIC int norm_test
PARMS((
const struct menu_d *tst_mp
));

STATIC int custom_test
PARMS((
const struct menu_d *tst_mp
));

#if !NO_AUTO_TEST_MENU
STATIC int auto_test
PARMS((
const struct menu_d *tst_mp
));
#endif

/* The descriptors for RAM tests consist of generic menu stuff (label
 * and routine) plus info for the particular test. These are now gotten
 * from ram_desc.h to make it easier to "dif" ram_test.c from different
 * games and see only relevant differences. Actually, ram_desc.h is
 * included twice. The first time is to allow a particularly perverse
 * hardware to "inject" new ram-test routines for use in its menu.
 */
#define SPECIAL_TESTS
#include <ram_desc.h>
#undef SPECIAL_TESTS

extern int ROMTest();

STATIC const struct ramt RAM_LIST[] =
{
    {
	{"MEMORY TESTS", 0},
	{ 0, 0, 0 },
	0
    },
#include <ram_desc.h>
#if !NO_ROM_TEST_MENU
    {
	{"\nROM TEST", ROMTest},
	{ 0, 0, 0 },
	0
    },
#endif
    {
	{ 0, 0 },
	{ 0, 0, 0 },
	0
    }
};

STATIC int
norm_test( parm_mp )
const struct menu_d *parm_mp;
{
    const struct ramt *tst_mp = (struct ramt *)parm_mp;
    struct rrb result;
    const struct rdb *rdp;
    struct rdb t_rdb;
    char	*kp;
    int		old_ipl = INTS_OFF;
    int		errors= 0;
    int		passes = 0;
    int		status;
    int		pause;
    int		should_loop;
    int		old_fields = 0;

    rdp = &tst_mp->rdesc;

    if ( rdp->rd_len == 0 ) {
	/* special kluge for working RAM */
	t_rdb = *rdp;
	rdp = &t_rdb;
	kp = (char *)rdp - CUSHION;
#if !PHX
	t_rdb.rd_len = kp - (char *)(rdp->rd_base);
#else
	t_rdb.rd_len = kp - (char *)prc_extend_bss(0);
#endif
#if PHX
	t_rdb.rd_base = (U32 *)K0_TO_K1(prc_extend_bss(0));	/* test mem uncached */
#elif ((PROCESSOR&PROCESSOR_CLASS) == MIPS3000)
	t_rdb.rd_base = (U32 *)K0_TO_K1(t_rdb.rd_base);		/* test mem uncached */
#elif HOST_BOARD == HCR4K || HOST_BOARD == MB4600
	t_rdb.rd_base = (U32 *)((U32)t_rdb.rd_base-DRAM_BASE+DRAM_BASEnc); /* make addresses use uncached mem */
#endif
    }

    should_loop = tst_mp->notes & NOTE_LOOPING;
    show_results(rdp, (struct rrb *)0, tst_mp, passes,errors);
    for ( pause = LOOP_CK_PAUSE ; pause >= 0 ; --pause ) {
	/* if user _holds_ SW_ACTION, make this a looping test */
	if ( (ctl_read_sw(0) & SW_ACTION) == 0 ) break;
	prc_delay0();
    }
    if ( pause < 0 ) should_loop = 1;
    if ( should_loop ) {
	txt_str(-1,2,"(LOOPING)",GRY_PAL);
	prc_delay(0);			/* make sure message shows up on Phoenix class boards */
    }
    while ( passes == 0 || should_loop ) {
	txt_clr_wid(1, AN_VIS_ROW-3, AN_VIS_COL-2);	/* erase the exit instructions */
	txt_clr_wid(1, AN_VIS_ROW-2, AN_VIS_COL-2);	/* erase the exit instructions */
	prc_delay(0);			/* make sure it shows up on Phoenix class systems */
	if ( tst_mp->notes & NOTE_NOVID ) {
	    /* shut off semi-autonomous video and wait to take effect */
	    old_fields = vid_fields(-1);
	    if ( old_fields >= 0 ) prc_delay(old_fields);
	}
	if ( tst_mp->notes & NOTE_NOINT ) old_ipl = prc_set_ipl(INTS_OFF);
	if ( tst_mp->notes & NOTE_QUICK ) {
	    status = q_ram_test(rdp,&result);
	} else {
	    /* New C ram test combines bram_test with f_ram_test */
	    status = f_ram_test(rdp,&result);
	}
	if ( tst_mp->notes & NOTE_NOINT ) prc_set_ipl(old_ipl);
	if ( tst_mp->notes & NOTE_VIDINIT ) vid_init();
	if ( tst_mp->notes & NOTE_NOVID ) vid_fields(old_fields);
	if ( tst_mp->notes & (NOTE_VIDINIT|NOTE_VIDCLR) ) {
	    vid_clear();
	    setancolors();
	}
	pause = GOOD_PAUSE;
	++passes;
	if ( status ) {
	    pause = BAD_PAUSE;
	    show_results(rdp, &result, tst_mp, passes, ++errors);
	} else show_results(rdp, (struct rrb *)0, tst_mp, passes,errors);
	st_insn(AN_VIS_ROW-3, "To hold display,", t_msg_actionh, INSTR_PAL);
	if ( should_loop ) {
	    txt_str(-1,2,"(LOOPING)",GRY_PAL);
	    txt_clr_wid(1, AN_VIS_ROW-2, AN_VIS_COL-2);	/* erase the exit instructions */
	    st_insn(AN_VIS_ROW-2, "To stop looping,", t_msg_next, INSTR_PAL);
	} else {
	    ExitInst(INSTR_PAL);
	}
	/* intent of the following is to hold screen indefinitely if SW_ACTION
	 * is held, then pause for GOOD_PAUSE frames and exit, or exit
	 * immediately if NEXT is pressed.
	 */
	while ( pause ) {
	    if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) {
		pause = 1;
		should_loop = 0;
	    }
	    if ( ctl_read_sw(0) & SW_ACTION ) ++pause;
	    if ( ctl_read_sw(SW_ACTION) & SW_ACTION ) pause = GOOD_PAUSE;
	    prc_delay0();
	    --pause;
	}
    }
    txt_clr_wid(1, AN_VIS_ROW-3, AN_VIS_COL-2);	/* erase the exit instructions */
    txt_clr_wid(1, AN_VIS_ROW-2, AN_VIS_COL-2);	/* erase the exit instructions */
    return !!errors;
}

/*		Custom RAM test uses generic parameter entry to build
 *	RAM descriptor, then runs test.
 */

/*		plist[] (below) describes the parameters we wish to
 *	enter.
 */
#if defined(COLRAM_ADDR)
# define DEFAULT_ADDR (COLRAM_ADDR)
#else
# define DEFAULT_ADDR 0
#endif
#if MINH_WATCH
U32 *kluge_watch;
#endif

static const struct parm plist [] = {
    /* Label		value		col/row	sigdig		curdig	*/
    { "Base Address: ",	(U32)DEFAULT_ADDR, 4,3,	(HEX_FIELD|8),	0 	},
#define PIDX_ADDR (0)
    { "Length (Bytes)",	0x100,		4,5,	(HEX_FIELD|8),	0 	},
#define PIDX_LEN (1)
    { "IGNORE (MASK) ",	0,		4,7,	(HEX_FIELD|8),	0 	},
#define PIDX_IGNORE (2)
#if MINH_WATCH
    { "WATCH (MINH) ",	0,		4,9,	(HEX_FIELD|8),	0 	},
#define PIDX_WATCH (3)
#endif
    { 0,		0,		0,0,	0,		0 	}};

#define NPARMS (sizeof (plist)/sizeof(plist[0]))

/*		cramt_cb() is the "callback". That is, it is called by the
 *	parameter entry code to validate changes to the parameters and
 *	possibly copy them to the RAM descriptor.
 */
STATIC int
cramt_cb(struct parm *pp, int idx,struct opaque *op)
{
    struct ramt *rtp = (struct ramt *)op;
    unsigned long val;

    val = pp->val;
    switch ( idx ) {
	case PIDX_ADDR :
	    /* Validate Base address of area to test. Since our
	     * test code can only test on longword boundaries, we
	     * detect an attempt to increment or decrement and make
	     * it by four...
	     */
	    if ( (val & 3) == 1 ) val += 3;
	    else if ( (val & 3) == 3) val -= 3;
	    pp->val = val;
	    rtp->rdesc.rd_base = (U32 *)val;
	    break;

	case PIDX_LEN :
	    /* Validate length of area to test. Again, force inc/dec to
	     * be by four.
	     */
	    if ( (val & 3) == 1 ) val += 3;
	    else if ( (val & 3) == 3) val -= 3;
	    pp->val = val;
	    rtp->rdesc.rd_len = val;
	    break;

	case PIDX_IGNORE :
	    /* No validation needed for "bits to ignore"
	     */
	    rtp->rdesc.rd_misc = val ? ~val : val;
	    break;

#if MINH_WATCH
	case PIDX_WATCH :
	    /* Validate address to watch during test. Again, force inc/dec to
	     * be by four.
	     */
	    if ( (val & 3) == 1 ) val += 3;
	    else if ( (val & 3) == 3) val -= 3;
	    pp->val = val;
	    kluge_watch = (U32 *)val;
	    break;
#endif /* MINH_WATCH */
    } /* end switch */
    return 1;
}

STATIC
int custom_test( parm_mp )
const struct menu_d *parm_mp;
{
    struct parm work[NPARMS],*wp;
    const struct parm *cwp;
    struct ramt descrip;
    int idx,status;

    wp = work;
    cwp = plist;
    descrip = *(struct ramt *)parm_mp;

    do {
	*wp++ = *cwp++;
    } while (wp[-1].label != 0);
#if PHX
    work[0].val = (U32)K0_TO_K1(prc_extend_bss(0));	/* init the custom address to non-destructive */
    work[1].val = ((U8 *)work - prc_extend_bss(0) - 16384)&-256;
#endif
    for ( idx = 0 ; idx < NPARMS ; ++idx ) {
	cramt_cb(work+idx,idx,(struct opaque *)&descrip);
    }
    while (1) {
	txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
	txt_clr_wid(2, AN_VIS_ROW-2, AN_VIS_COL-4);
	st_insn(AN_VIS_ROW-3, "To start test,", t_msg_action, INSTR_PAL);
	ExitInst(INSTR_PAL);
	status = utl_enter(work,cramt_cb,(struct opaque *)&descrip);
	if ( status & SW_NEXT ) break;
	if ( status & SW_ACTION ) {
	    /* do this test */
	    txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
	    txt_clr_wid(2, AN_VIS_ROW-2, AN_VIS_COL-4);
	    prc_delay(0);
	    status = norm_test((struct menu_d *)&descrip);
	}
    }
#if MINH_WATCH
    kluge_watch = 0;
#endif
    return 0;
}

STATIC void
show_results(rdp, result, smp, passes, errors)
const struct rdb *rdp;
const struct rrb *result;
const struct ramt *smp;
int passes;
int errors;
{
    m_int	tmp;
    static const char	* const directionC = "FFBBFBFBF";
    char dirbuf[3];
    int		palette;

    /* First section is used whether or not we had an error on this pass
     */
    if (errors) palette = RED_PALB;
    else palette = GRN_PALB;

    st_frame((const struct menu_d *)smp, palette, INSTR_PAL, 0);
    ExitInst(INSTR_PAL);
#ifdef DEBUG
    txt_hexnum((AN_VIS_COL-27)/2,MSG_ROW-6,(U32)(rdp->rd_base),
      8, RJ_ZF, MNORMAL_PAL);
    txt_cstr(" ", MNORMAL_PAL);
    txt_chexnum((U32)(rdp->rd_base)+rdp->rd_len-4,
      8, RJ_ZF, MNORMAL_PAL);
    txt_cstr(" ",MNORMAL_PAL);
    txt_chexnum(smp->rdesc.rd_misc,8,RJ_ZF,MNORMAL_PAL);
#endif
    if ( errors ) txt_str(MSG_COL,MSG_ROW-4," ERRORS",palette);

    txt_str(10,MSG_ROW-3," TEST COUNT:",WHT_PALB);
    txt_cdecnum(passes,4,RJ_BF,WHT_PALB);
    if ( passes ) {
	txt_str(10,MSG_ROW-2,"ERROR COUNT:",palette);
	txt_cdecnum(errors,4,RJ_BF,palette);
    }
    /* Second section only used when we have an error to report.
     */
    if ( result == 0 ) return;
    
    tmp = result->rr_test_no;
    strcpy(dirbuf," X");
    txt_str(MSG_COL,MSG_ROW,"TEST #  : ",palette);
    txt_cdecnum(tmp,2,RJ_BF,WHT_PALB);
    if ( tmp > 0 && tmp <= strlen(directionC) ) {
	dirbuf[1] = directionC[tmp-1];
	txt_cstr(dirbuf,WHT_PALB);
    }
    txt_str(MSG_COL,MSG_ROW+1,"ADDRESS : ",palette);
    txt_chexnum((U32)result->rr_addr,8,RJ_ZF,WHT_PALB);
    
    txt_str(MSG_COL,MSG_ROW+2,"EXPECTED: ",palette);
    txt_chexnum(result->rr_expected,8,RJ_ZF,WHT_PALB);
    
    txt_str(MSG_COL,MSG_ROW+3,"GOT     : ",palette);
    txt_chexnum(result->rr_got,8,RJ_ZF,WHT_PALB);
}

/* The odd ordering below is to accomodate a bug in MRI C */

int RAM_group PARMS((
const struct ramt *smp		/* Selected Menu Pointer */
));

int
RAM_group( smp )
const struct ramt *smp;		/* Selected Menu Pointer */
{
    return st_menu((struct menu_d *)&RAM_LIST[0],sizeof(RAM_LIST[0]),MNORMAL_PAL,0);
}

#if !NO_AUTO_TEST_MENU
/*		auto_test()
 *	Runs through whole menu, testing each specified piece of RAM.
 *	Any RAM which fails will be re-tested.
 */

STATIC int auto_test ( smp )
const struct menu_d *smp;
{
    int status, errors;
    const struct ramt *rtp;
    const struct ramt *my_p = (const struct ramt *)smp;

    rtp = RAM_LIST+1;
    status = 1;
    errors = 0;
    while ( rtp->menu_desc.mn_label ) {
	/* Do not run our own line, or any below it,
	 * typically rom checksums or custom test.
	 */
	if ( rtp->menu_desc.mn_call == my_p->menu_desc.mn_call ) break;
	if ( rtp->menu_desc.mn_label[0] == '?'
	  || rtp->menu_desc.mn_call == custom_test ) {
	    /* we want to skip the custom line,
	     * and any "hidden" lines
	     */
	    ++rtp;
	} else {
	/* due to vestiges of 68K-based thinking, some hardwares _still_
	 * map more than one physical chunk of RAM onto the same address.
	 * because of this, we need to compare length and "bits" in addition
	 * to base address.
	 */
	    if ( (rtp->rdesc.rd_base == rtp[1].rdesc.rd_base)
	      && (rtp->rdesc.rd_len == rtp[1].rdesc.rd_len)
	      && (rtp->rdesc.rd_misc == rtp[1].rdesc.rd_misc)) {
		/* Same area mentioned twice. One is presumably
		 * the "Quick" version. Whether we use it or
		 * the other is controlled by the presence
		 * or absence of "NOTE_QUICK" on our _own_
		 * menu line.
		 */
		if ( (rtp->notes & NOTE_QUICK)
		  != (my_p->notes & NOTE_QUICK) ) {
		    ++rtp;
		}
	    }
	    vid_clear();
	    status = rtp->menu_desc.mn_call((const struct menu_d *)rtp);
	    if ( !status ) {
		/* success, next test */
		if ( (rtp->rdesc.rd_base == rtp[1].rdesc.rd_base)
		  && (rtp->rdesc.rd_len == rtp[1].rdesc.rd_len)
		  && (rtp->rdesc.rd_misc == rtp[1].rdesc.rd_misc)) {
		    /* we want to skip both of the pair */
		    ++rtp;
		}
		++rtp;
	    } else ++errors;
	}
	if ( rtp->menu_desc.mn_label == 0 ) {
	    /* we have stepped to the last line. Let the user
	     * choose to repeat or leave. But time out after 10 seconds
	     */
	    unsigned long ctls = 0;
	    char err_buf[AN_VIS_COL_MAX+2];
	    vid_clear();
	    setancolors();
	    st_frame(smp,TITLE_PAL,INSTR_PAL,0);
	    if ( errors ) {
		utl_cdec(errors,err_buf,6,LJ_NF);
		strcat(err_buf," ERROR");
		if ( errors > 1 ) strcat(err_buf,"S");
		txt_str(-1,AN_VIS_ROW/2,err_buf, RED_PAL+AN_BIG_SET);
	    } else {
		txt_str(-1,AN_VIS_ROW/2,"NO ERRORS", GRN_PAL+AN_BIG_SET);
	    }
	    txt_str(-1,AN_VIS_ROW-5,t_msg_action,INSTR_PAL);
	    txt_str(-1,AN_VIS_ROW-4,"TO REPEAT TEST",INSTR_PAL);
	    while ( 1 ) {
		ctls = ctl_read_sw(SW_NEXT|SW_ACTION);
		if ( ctls & SW_NEXT ) return errors;
		if ( ctls & SW_ACTION ) {
		    rtp = RAM_LIST+1;
		    break;
		}
		prc_delay0();
	    }
	    if ( rtp->menu_desc.mn_label ) continue;
	}
	if ( ctl_read_sw(0) & SW_NEXT ) break;
    }
    return errors;
}
#endif
