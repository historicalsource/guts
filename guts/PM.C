/*
 *		pm.c
 *
 *		Copyright 1991 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
*/
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: pm.c,v 1.31 1997/07/11 00:30:50 shepperd Exp $";
#endif
/*
 *		Display "post mortem" after reset. Borrowing heavily from
 *	Pete Lipson's trapdump.asm, this file is written in C instead of
 *	assembly, and is run after the game hardware has been re-initialized.
 *	Any "crash" is intended to leave a "death note" consisting of a string
 *	describing the error, the PC where the error occured, and a pointer
 *	to the stack at the time of the error. All of these should, of course,
 *	be taken with a grain of salt, since they are by definition the product
 *	of hardware/software that was not functioning correctly at the time.
 *
 */

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

#ifndef VCR_HOLD_TIME
#define VCR_HOLD_TIME (120)	/* two minutes or three taps */
#endif

/*	The variables below have a high probability of being set by an error.
 *	If any are non-zero, a post-mortem dump is performed. The vcr is
 *	turned on for VCR_HOLD_TIME seconds to "preserve the evidence", and
 *	then the variables are cleared and the board "re-booted". This last
 *	step is needed because the power-on root may have taken some pains
 *	to avoid over-writing the stack in the case of a post-mortem dump.
 */
/*
 *	The R3K/R4K saves the pm variables in a "struct pm_rxk_str" called pm_data.
 *	The pm_data variable itself is declared in os_rootrxk.mac. Until this
 *	scheme can be back patched into the 68k flavor, the members of the
 *	struct are copied to local pm_ variables at entry to pm_dump.
 */
/* 		wlc(buff,str,max)
 *	Copy string <str> to buffer <buff>, limiting to maximum width <max>.
 *	We try to break at a "space". returns number of characters of <str>
 *	it successfully copied. This is only a forward reference because
 *	of a bug in MRI C that loses the const qualifier on old-style
 *	definitions.
 */
int wlc( char *buff, const char *str, int max);

int pm_validate(struct pm_general *pmptr) {
    if ( (pmptr->pm_cntr < 0 && pmptr->pm_cntr != -42) )
    {
        return 0;			/* not a valid pm */
    }
    if ( pmptr->pm_msg == 0 && pmptr->pm_pc == 0 && pmptr->pm_stack == 0 ) {
        return 0;			/* not a valid pm */
    }
    return 1;				/* probably a valid pm */
}

/* in the absence of any switch input,  hold screen for VCR_HOLD_TIME
 * seconds or (what amounts to) forever if GUTS_OPT_PM_HOLD is set.
 * If ACTION or NEXT is _TAPPED_ three times, exit.
 */

static void hold_pm_display(void) {
    int taps, timer;

    timer = VCR_HOLD_TIME*60;		/* assume normal timeout */
#if defined(EER_GUTS_OPT) && defined(GUTS_OPT_PM_HOLD)
    if ((eer_gets(EER_GUTS_OPT) & GUTS_OPT_PM_HOLD)) timer = 0x7FFFFFFF;
#endif

    taps = 0;
    for ( ; --timer > 0 ; ) {
	prc_delay0();
	if ( (ctl_read_sw(SW_NEXT|SW_ACTION) & (SW_NEXT|SW_ACTION))
	   && ++taps >= 3 ) break;
    }
}

void pm_dump_param(struct pm_general *pmptr, int cputype, int start_row)
{
#ifdef WRAM_ADDR
    char msgbuf[AN_VIS_COL_MAX+1];
    int idx,row,col,s_row;
    int numregs;
    U32 *regptr;

    if ( (pmptr->pm_cntr < 0 && pmptr->pm_cntr != -42) )
    {
        pmptr->pm_cntr = 0;		/* probably a power-on reset with garbage in pm_cntr */
        pmptr->pm_pc = 0;		/* so clear some stuff */
        pmptr->pm_stack = 0;
        pmptr->pm_stkrelative = 0;
        pmptr->pm_msg = 0;
        return;			/* and ignore it */
    }
#if (COJAG_GAME&COJAG_AREA51)
    if ( (pmptr->pm_cntr >= EER_RESET) && (pmptr->pm_cntr <= EER_LINKFAIL) ) {
	eer_p_incs(pmptr->pm_cntr,1);
	pmptr->pm_cntr = 0;
    }
#endif
    if ( pmptr->pm_msg == 0 && pmptr->pm_pc == 0 && pmptr->pm_stack == 0 ) {
        pmptr->pm_cntr = 0;
        return;
    }
    row = start_row;
    start_row += 3;
#ifdef TEXT_HOST
    txt_select(TXT_HOST);
#endif
    vid_clear();
    if ( pmptr->pm_msg ) {
	const char *msg = pmptr->pm_msg;
	while ( row < start_row ) {
	    idx = wlc(msgbuf,msg,AN_VIS_COL-1);
	    txt_str(-1,row,msgbuf,MNORMAL_PAL);
	    msg += idx;
	    ++row;
	}
    } else {
	txt_str(-1,row++,"UNKNOWN ERROR",MNORMAL_PAL);
    }

    txt_str((AN_VIS_COL-3-8-5-8)/2, row, "PC=", MNORMAL_PAL);
    txt_chexnum(pmptr->pm_pc, 8, RJ_ZF, MNORMAL_PAL);
    txt_cstr("  SP=",MNORMAL_PAL);
    txt_chexnum((U32)pmptr->pm_stack, 8, RJ_ZF, MNORMAL_PAL);
    txt_str((AN_VIS_COL-3-8-8-8)/2, ++row, "SR=", MNORMAL_PAL);
    txt_chexnum(pmptr->pm_sr, 8, RJ_ZF, MNORMAL_PAL);
    cputype &= -16;		/* we only care about the major cpu numbers */
    numregs = 0;
    if (cputype == MIPS3000 || cputype == MIPS4000) {
	txt_cstr("  CAUSE=", MNORMAL_PAL);
	txt_chexnum(pmptr->pm_cause, 8, RJ_ZF, MNORMAL_PAL);
	txt_str((AN_VIS_COL-9-8)/2, ++row, "BadVAddr=", MNORMAL_PAL);
	txt_chexnum(pmptr->pm_badvaddr, 8, RJ_ZF, MNORMAL_PAL);
	numregs = 32;
    }
    if (cputype == 0) {		/* 68k */
	numregs = 16;
    }
    if (cputype == ASAP) {
        numregs = 32;
    }
    col = 0;
    for ( regptr = pmptr->pm_regs, idx = 0 ; idx < numregs ; ++idx ) {
#if (PROCESSOR&PROCESSOR_CLASS) == MIPS4000
	static char * const regnames[] = {
	    "r0=", "at=", "v0=", "v1=", "a0=", "a1=", "a2=", "a3=",
	    "t0=", "t1=", "t2=", "t3=", "t4=", "t5=", "t6=", "t7=",
	    "t8=", "t9=", "s0=", "s1=", "s2=", "s3=", "s4=", "s5=",
	    "s6=", "s7=", "k0=", "k1=", "gp=", "sp=", "fp=", "ra="
	};
# define REG_COL_INC (9+3)
#else
# define REG_COL_INC (9)
#endif
	if ( (idx & 3) == 0 ) {
	    ++row;
	    col = 1;
	} else col += REG_COL_INC;
#if (PROCESSOR&PROCESSOR_CLASS) == MIPS4000
	txt_str(col, row, regnames[idx], MNORMAL_PAL);
	txt_chexnum(*regptr++, 8, RJ_ZF, MNORMAL_PAL);
#else
	txt_hexnum(col, row, *regptr++ ,8, RJ_ZF, MNORMAL_PAL);
#endif
    }
    s_row = ++row;
    col = 0;
    numregs = 1;
    if (cputype == ASAP || cputype == MIPS3000 || cputype == MIPS4000) {
	numregs = (int)pmptr->pm_stack&3;
    } else if (cputype == 0) {		/* 68k */
	numregs = (int)pmptr->pm_stack&1;
    }
    if (numregs == 0) {
	U32 *stkrelative, *stack;
	stkrelative = pmptr->pm_stkrelative;
	stack = pmptr->pm_stack;
	while ( stkrelative && stack > pmptr->pm_stklower && stack < pmptr->pm_stkupper ) {
	    /* stack pointer is within WRAM */
	    if ( ++row < AN_VIS_ROW-1 ) {
		txt_str(col,row," *",MNORMAL_PAL);
		txt_chexnum((unsigned long)stack,8,RJ_ZF,MNORMAL_PAL);
		txt_cstr(" =",MNORMAL_PAL);
		txt_chexnum(*stkrelative,8,RJ_ZF,MNORMAL_PAL);
	    } else {
		/* hit bottom of screen, go back for another column */
		row = s_row;
		if ( (col += 20) >= (AN_VIS_COL-20) ) break;
	    }
	    ++stack;
	    ++stkrelative;
	}
    }
    txt_str(-1,AN_VIS_ROW-1,"THREE TAPS AND YOU'RE OUT",INSTR_PAL);
#ifdef VCR_RECORD
    vcr(VCR_RECORD);
#endif
    hold_pm_display();
#ifdef VCR_RECORD
    vcr(VCR_STOP);
#endif /* def VCR_RECORD */
#endif
    vid_clear();
    pmptr->pm_msg = 0;
    pmptr->pm_pc = 0;
    pmptr->pm_stack = 0;
    pmptr->pm_stkrelative = 0;
#ifdef WRAM_ADDR
    prc_reboot();
#endif
}

void pm_dump(void) {
    extern U32 INIT_SP[], bss_end[];
    struct pm_general *lclpp;

/* To remain compatible with old 68k and ASAP code... */
#if ((PROCESSOR&PROCESSOR_CLASS) == 0) || ((PROCESSOR&PROCESSOR_CLASS) == ASAP)	/* 68k or ASAP */
    extern const char *pm_msg;
    extern U32 *pm_stack, pm_pc, pm_regs[], pm_sr;
    extern int pm_cntr;    
    struct pm_general lclpm;
    int ii;

    lclpp = &lclpm;
    lclpp->pm_msg = pm_msg;
    lclpp->pm_stack = pm_stack;
    lclpp->pm_stkupper = INIT_SP;
    lclpp->pm_stklower = bss_end;
    lclpp->pm_stkrelative = pm_stack;
    lclpp->pm_cntr = pm_cntr;
    lclpp->pm_pc = pm_pc;
    lclpp->pm_sr = pm_sr;
# if (PROCESSOR&PROCESSOR_CLASS) == 0		/* 68k */
#  define NREGS 16
# else
#  define NREGS 32
# endif
    for (ii=0; ii < NREGS; ++ii) lclpp->pm_regs[ii] = pm_regs[ii];
#else					/* 68k or ASAP */
    extern struct pm_general pm_data;
    lclpp = &pm_data;

    lclpp->pm_stkupper = INIT_SP;
    lclpp->pm_stklower = bss_end;
    lclpp->pm_stkrelative = lclpp->pm_stack;

#endif					/* 68k or ASAP */


    if ( lclpp->pm_cntr <= 0 && lclpp->pm_msg == 0 ) {
	/* special hack for watchdog reset, or maybe just a
	 * reset on the bench :-)
	 */
	if ( lclpp->pm_cntr == -42
#ifdef GUTS_OPT_PM_WDOG
		|| (debug_mode & GUTS_OPT_PM_WDOG)
#endif
        ) {
	    /* this was just prc_reboot having a little fun with us */
#if ((PROCESSOR&PROCESSOR_CLASS) == 0) || ((PROCESSOR&PROCESSOR_CLASS) == ASAP)	/* 68k or ASAP */
    	    pm_cntr = 0;
    	    pm_msg = 0;
    	    pm_pc = 0;
    	    pm_stack = 0;
#else					/* 68k */
	    lclpp->pm_cntr = 0;
            lclpp->pm_msg = 0;
	    lclpp->pm_pc = 0;		/* init the important variables */
	    lclpp->pm_stack = 0;
            lclpp->pm_stkrelative = 0;
#endif
	    return;
	}
	if ( 
#if (HOST_BOARD == PHOENIX_AD) || (HOST_BOARD == FLAGSTAFF) || (HOST_BOARD == SEATTLE) || (HOST_BOARD == VEGAS)
    	     0				/* debug mode is always on on AD and FLAGSTAFF systems */
#else
    	     debug_mode & GUTS_OPT_DBG_SW
#endif
           ) {
	    /* If debug switches are enabled, we are probably on the
	     * bench and this "watchdog reset" is just somebody
	     * smacking the button.
	     */
	    prc_reboot();
	} else {
#ifdef EER_RESET
	    lclpp->pm_cntr = EER_RESET;
#endif
	    lclpp->pm_msg = "WATCHDOG RESET"; 	    
	}
    }

    pm_dump_param(lclpp, PROCESSOR, 1);

#if ((PROCESSOR&PROCESSOR_CLASS) == 0) || ((PROCESSOR&PROCESSOR_CLASS) == ASAP)	/* 68k or ASAP */
    pm_msg = lclpp->pm_msg;
    pm_stack = lclpp->pm_stack;
    pm_cntr = lclpp->pm_cntr;
    pm_pc = lclpp->pm_pc;
#endif
}

/* 		wlc(buff,str,max)
 *	Copy string <str> to buffer <buff>, limiting to maximum width <max>.
 *	We try to break at a "space". returns number of characters of <str>
 *	it successfully copied.
 */

int wlc(buff,str,max)
char *buff;
const char *str;
int max;
{
    const char *lp,*last_sp;
    int idx;

    lp = last_sp = str;
    idx = 0;
    while (*lp) {
	if ( (*lp) == ' ' ) last_sp = lp;
	buff[idx] = *lp++;
	if ( ++idx == max ) {
	    /* running out of room. Trim back to prev space */
	    int back;
	    back = lp - last_sp;
	    if ( back >= idx ) {
		/* no space in the whole run, hyphenate */
		buff[idx-1] = '-';
		--lp;	/* back up to repeat */	    
	    } else {
		/* overlay space with E-O-L */
		idx -= back;
		lp = last_sp+1;
	    }
	    break;
	}
    }
    buff[idx] = '\0';
    return lp - str;
}
