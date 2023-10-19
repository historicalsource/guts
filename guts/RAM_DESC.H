/*		ram_desc.h
 *	Descriptors for each area of memory to test, and
 *	how to test it. This file is separate for the purpose
 *	of making it easier to see the _real_ differences
 *	between versions of ram_test.c
 */

#ifdef SPECIAL_TESTS

#include <glide.h>
#include <phx_proto.h>
#if BOOT_FROM_DISK
#  define SECRET_HS
#else
#  define SECRET_HS "?"
#endif

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#undef BAD_PAUSE
#define BAD_PAUSE (0x7FFFFFF0)

extern int zag_test_bram( struct diag_params *dp );
extern char *mem_test_msgs[];

static int lbram_test( const struct menu_d *parm ) {
    const struct ramt *tst_mp = (struct ramt *)parm;
    struct rrb result;
    struct diag_params zag_diags;
    const struct rdb *rdp;
    int		errors= 0;
    int		passes = 0;
    int		old_ipl = INTS_OFF;
    int		status;
    int		pause;
    int		should_loop;

#if !NO_BRAM_AUS
    txt_str(-1, AN_VIS_ROW/2, "THIS TEST COMPLETELY ERASES THE BRAM", RED_PAL);
    txt_str(-1, AN_VIS_ROW/2+1, "ARE YOU SURE YOU WANT TO DO THIS?", RED_PAL);
#define MSG_IF_SO "If so, "
    pause = strlen(t_msg_actionh) + sizeof(MSG_IF_SO)-1;
    txt_str((AN_VIS_COL-pause)/2, AN_VIS_ROW/2+3, MSG_IF_SO, GRN_PALB);
    txt_cstr(t_msg_actionh, GRN_PALB);
#define MSG_CHANGE_MIND "Time to change your mind: "
    txt_str((AN_VIS_COL-sizeof(MSG_CHANGE_MIND)+4)/2, AN_VIS_ROW/2+5, "Time to change your mind: ", WHT_PAL);
    ctl_read_sw(-1);			/* flush all edges */
    for (pause = 5*60; pause; --pause) {
	txt_decnum((AN_VIS_COL-sizeof(MSG_CHANGE_MIND)+4)/2+sizeof(MSG_CHANGE_MIND),
		   AN_VIS_ROW/2+5, pause/60+1, 4, LJ_BF, GRN_PAL);
	prc_delay(0);
	if (ctl_read_sw(SW_ACTION)&SW_ACTION) break;
    }
    if (!pause) return 0;
    for (pause = 5*60; pause; --pause) {
	txt_decnum((AN_VIS_COL-sizeof(MSG_CHANGE_MIND)+4)/2+sizeof(MSG_CHANGE_MIND),
		   AN_VIS_ROW/2+5, pause/60+1, 4, LJ_BF, RED_PAL);
	prc_delay(0);
	if (!(ctl_read_sw(0)&SW_ACTION)) break;
    }
    if (pause) return 0;

    txt_clr_wid(1, AN_VIS_ROW/2, AN_VIS_COL-2);
    txt_clr_wid(1, AN_VIS_ROW/2+1, AN_VIS_COL-2);
    txt_clr_wid(1, AN_VIS_ROW/2+3, AN_VIS_COL-2);
    txt_clr_wid(1, AN_VIS_ROW/2+5, AN_VIS_COL-2);
#endif

    txt_str(-1, AN_VIS_ROW/2, t_msg_actionh, INSTR_PAL);
    txt_str(-1, AN_VIS_ROW/2+1, "to make test LOOP", INSTR_PAL);

    rdp = &tst_mp->rdesc;
    should_loop = tst_mp->notes & NOTE_LOOPING;
    show_results(rdp, (struct rrb *)0, tst_mp, passes, errors);
    for ( pause = LOOP_CK_PAUSE ; pause >= 0 ; --pause ) {
	/* if user _holds_ SW_ACTION, make this a looping test */
	if ( (ctl_read_sw(0) & SW_ACTION) == 0 ) break;
	prc_delay0();
    }
    if ( pause < 0 ) should_loop = 1;
    if ( should_loop ) txt_str(-1,2,"(LOOPING)",GRY_PAL);
    txt_clr_wid(1, AN_VIS_ROW/2, AN_VIS_COL-2);
    txt_clr_wid(1, AN_VIS_ROW/2+1, AN_VIS_COL-2);
    while ( passes == 0 || should_loop ) {
	txt_clr_wid(1, AN_VIS_ROW-3, AN_VIS_COL-2);	/* erase the exit instructions */
	txt_clr_wid(1, AN_VIS_ROW-2, AN_VIS_COL-2);	/* erase the exit instructions */
	prc_delay(0);
	if ( tst_mp->notes & NOTE_NOINT ) old_ipl = prc_set_ipl(INTS_OFF);
	status = zag_test_bram(&zag_diags);
	if ( tst_mp->notes & NOTE_NOINT ) prc_set_ipl(old_ipl);
	pause = GOOD_PAUSE;
	++passes;
	if ( status ) {
	    result.rr_addr = (unsigned long *)zag_diags.bad_address;
	    result.rr_expected = zag_diags.expected_lsb;
	    result.rr_got = zag_diags.actual_lsb;
	    result.rr_test_no = zag_diags.subtest;
	    pause = BAD_PAUSE;
	    show_results(rdp, &result, tst_mp, passes, ++errors);
	    txt_str(MSG_COL, MSG_ROW+5, "Failed during:", WHT_PAL);
	    txt_str(MSG_COL, MSG_ROW+6, mem_test_msgs[zag_diags.subtest], YEL_PAL);
	} else {
	    show_results(rdp, (struct rrb *)0, tst_mp, passes, errors);
	}
	st_insn(AN_VIS_ROW-3, "To hold display,", t_msg_actionh, INSTR_PAL);
	if ( should_loop ) {
	    txt_clr_wid(1, AN_VIS_ROW-2, AN_VIS_COL-2);	/* erase old exit instructions */
	    st_insn(AN_VIS_ROW-2, "To stop looping,", t_msg_next, INSTR_PAL);
	} else {
	    ExitInst(INSTR_PAL);
	}
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

static const struct parm bram_plist [] = {
    /* Label		value		col/row	sigdig		curdig	*/
    { "Base Address: ",	(U32)BRAM_BASE, 4,3,	(HEX_FIELD|8),	0 	},
#define BRAM_PIDX_ADDR (0)
    { "Length (Bytes)",	BRAM_SIZE,	4,5,	(HEX_FIELD|8),	0 	},
#define BRAM_PIDX_LEN (1)
    { 0,		0,		0,0,	0,		0 	}};

#define BRAM_NPARMS (sizeof (bram_plist)/sizeof(bram_plist[0]))

/*		bramt_cb() is the "callback". That is, it is called by the
 *	parameter entry code to validate changes to the parameters and
 *	possibly copy them to the RAM descriptor.
 */
static int bramt_cb(struct parm *pp, int idx, struct opaque *op) {
    struct ramt *rtp = (struct ramt *)op;
    unsigned long val;

    val = pp->val;
    switch ( idx ) {
	case BRAM_PIDX_ADDR :
	    /* Validate Base address of area to test. Since our
	     * test code can only test on longword boundaries, we
	     * detect an attempt to increment or decrement and make
	     * it by four...
	     */
	    if ( (val & 3) == 1 ) val += 3;
	    else if ( (val & 3) == 3) val -= 3;
	    if (val >= BRAM_BASE+BRAM_SIZE) val = BRAM_BASE+BRAM_SIZE-4;
	    if (val < BRAM_BASE) val = BRAM_BASE;
	    if (val + rtp->rdesc.rd_len >= BRAM_BASE+BRAM_SIZE) {
		rtp->rdesc.rd_len = BRAM_BASE+BRAM_SIZE - val;
	    }
	    pp->val = val;
	    rtp->rdesc.rd_base = (U32 *)val;
	    break;

	case BRAM_PIDX_LEN :
	    /* Validate length of area to test. Again, force inc/dec to
	     * be by four.
	     */
	    if ( (val & 3) == 1 ) val += 3;
	    else if ( (val & 3) == 3) val -= 3;
	    if (val > BRAM_SIZE) val = BRAM_SIZE;
	    if ((val + (U32)rtp->rdesc.rd_base) >= BRAM_BASE+BRAM_SIZE) {
		val = BRAM_BASE+BRAM_SIZE - (U32)rtp->rdesc.rd_base;
	    }
	    pp->val = val;
	    rtp->rdesc.rd_len = val;
	    break;
    } /* end switch */
    return 1;
}

static int custom_bram_test( parm_mp )
const struct menu_d *parm_mp;
{
    struct parm work[BRAM_NPARMS],*wp;
    const struct parm *cwp;
    struct ramt descrip;
    int idx,status;

    ExitInst(INSTR_PAL);
    wp = work;
    cwp = bram_plist;
    descrip = *(struct ramt *)parm_mp;

    do {
	*wp++ = *cwp++;
    } while (wp[-1].label != 0);
    for ( idx = 0 ; idx < BRAM_NPARMS ; ++idx ) {
	bramt_cb(work+idx,idx,(struct opaque *)&descrip);
    }
    while (1) {
	status = utl_enter(work, bramt_cb, (struct opaque *)&descrip);
	if ( status & SW_NEXT ) return 0;
	if ( status & SW_ACTION ) {
	    /* do this test */
	    status = lbram_test((struct menu_d *)&descrip);
	}
    }
    return 0;
}

#define DM_BASE_ROW 9			/* staring row */
static int DM_MEM_COLS, DM_MEM_ROWS, DM_MEM_SCREEN;
#define DM_SIZE_BITS	0		/* misc shifted this many gets size */
#define DM_SIZE_MASK	0xF		/* size mask bits */
#define DM_AUTOINC_BIT	4		/* misc shifted this many gets autoinc bit */
#define DM_AUTOINC_MASK 0x1		/* a single bit */
#define DM_STRIDE_BITS  5		/* misc shifted this many gets stride bits */
#define DM_STRIDE_MASK  ( (1<<(32-DM_STRIDE_BITS))-1 )

static const struct parm dm_plist [] = {
    /* Label		value		col/row	sigdig		curdig	*/
    { "Base Address: ",	(U32)DRAM_BASEnc,  4, 3, (HEX_FIELD|8),	0 	},
    { "Length (Bytes)     ",   0,   4, 4, (HEX_FIELD|3),	0 	},
    { "Item size (Bytes)    ", 4,	   4, 5, (HEX_FIELD|1),	0 	},
    { "Stride (Items)  ", 1,		   4, 6, (HEX_FIELD|6),	0 	},
    { "Autoincrement addr   ", 1,	   4, 7, (HEX_FIELD|1),	0 	},
    { 0,		0,		   0, 0, 0,		0 	}};

#define DM_NPARMS (sizeof (dm_plist)/sizeof(dm_plist[0]))

/*		dm_cb() is the "callback". That is, it is called by the
 *	parameter entry code to validate changes to the parameters and
 *	possibly copy them to the RAM descriptor.
 */
static int dm_cb(struct parm *pp, int idx, struct opaque *op) {
    struct ramt *rtp = (struct ramt *)op;
    unsigned long val;

    val = pp->val;
    switch ( idx ) {
	case 0 : {
	    int mask;
	    /* Validate Base address of area to test. 
	     */
	    mask = (rtp->rdesc.rd_misc>>DM_SIZE_BITS)&DM_SIZE_MASK;
	    if (mask) {
		--mask;
		if (val > (U32)rtp->rdesc.rd_base) {
		    val += mask;	/* round up to next multiple of 'size' */
		}
		val &= ~mask;	/* round up or down as appropriate */
	    }
	    pp->val = val;
	    rtp->rdesc.rd_base = (U32 *)val;
	    break;
	}

	case 1:
	    /* Validate length of area to test. Again, force inc/dec to
	     * be by four.
	     */
	    if (val > DM_MEM_SCREEN) val = DM_MEM_SCREEN;
	    if (val) {
		rtp->rdesc.rd_len = val;
	    }
	    pp->val = val;
	    break;

	case 2: {
	    int last;
	    /* Validate item size. Can only be 1, 2, 4 or 8
	     */
	    last = (rtp->rdesc.rd_misc>>DM_SIZE_BITS)&DM_SIZE_MASK;
	    if (!last) {
		if (val == 1 || val == 2 || val == 4 || val == 8) {
		    last = val;
		} else {
		    last = 4;
		}
	    }
	    if ( val > last ) {
		if ( last < 8 ) last += last;
	    } else if (val < last) {
		if ( last > 1 ) last /= 2;
	    }
	    pp->val = last;
	    rtp->rdesc.rd_misc &= ~(DM_SIZE_MASK<<DM_SIZE_BITS);
	    rtp->rdesc.rd_misc |= last << DM_SIZE_BITS;
	    break;
	}
	case 3:
	    /* Validate stride.
	     */
	    if ( val < 0x01000000 && val >= 1 ) {
		pp->val = val;
		rtp->rdesc.rd_misc &= ~(DM_STRIDE_MASK<<DM_STRIDE_BITS);
		rtp->rdesc.rd_misc |= val << DM_STRIDE_BITS;
	    } else {
		pp->val = (rtp->rdesc.rd_misc >> DM_STRIDE_BITS)&DM_STRIDE_MASK;
	    }
	    break;
	case 4:
	    /* Validate autoincrement
	     */
	    val &= 1;
	    pp->val = val;
	    rtp->rdesc.rd_misc &= ~(DM_AUTOINC_MASK<<DM_AUTOINC_BIT);
	    rtp->rdesc.rd_misc |= val<<DM_AUTOINC_BIT;
	    break;
    } /* end switch */
    return 1;
}

static int dump_mem( struct ramt *mem ) {
    int ii, jj, len, stride, size, autoinc, col=0;
    union {
	U32 ubase;
	U32 *wbase;
	U16 *sbase;
	U8  *cbase;
    } ptr;

    for (ii=7; ii < AN_VIS_ROW-2; ++ii) txt_clr_wid(1, ii, AN_VIS_COL-2);
    len = mem->rdesc.rd_len;
    stride = (mem->rdesc.rd_misc >> DM_STRIDE_BITS)&DM_STRIDE_MASK;
    autoinc = (mem->rdesc.rd_misc >> DM_AUTOINC_BIT) & 1;
    size = (mem->rdesc.rd_misc >> DM_SIZE_BITS) & DM_SIZE_MASK;
    ptr.wbase = mem->rdesc.rd_base;
    ptr.ubase += size-1;
    ptr.ubase &= ~(size-1);
    switch (size) {
	case 1:
	    col = (AN_VIS_COL-(3*DM_MEM_COLS*4)-9)/2;
	    break;
	case 2:
	    col = (AN_VIS_COL-(5*DM_MEM_COLS*4/2)-9)/2;
	    break;
	case 4:
	    col = (AN_VIS_COL-(9*DM_MEM_COLS*4/4)-9)/2;
	    break;
	case 8:
	    col = (AN_VIS_COL-(17*DM_MEM_COLS*4/8)-9)/2;
	    break;
    }
    txt_str(col+2, DM_BASE_ROW, "Addr", WHT_PAL);
    switch (size) {
	case 1:
	    if (stride == 1) {
		txt_cstr("    +0 +1 +2 +3 +4 +5 +6 +7", WHT_PAL);
    		if (DM_MEM_COLS == 4) {
		    txt_cstr(" +8 +9 +A +B +C +D +E +F", WHT_PAL);
		}
	    }
	    break;
	case 2:
	    if (stride*size <= 0x1000) {
		txt_cstr("   ", WHT_PAL);
		for (jj=0; jj < DM_MEM_COLS*2; ++jj) {
		    txt_cstr(" +", WHT_PAL);
		    txt_chexnum(jj*stride*size, 3, RJ_ZF, WHT_PAL);
		}
	    }
	    break;
	case 4:
	    if (stride*size <= 0x10000000) {
		txt_cstr("   ", WHT_PAL);
		for (jj=0; jj < DM_MEM_COLS; ++jj) {
		    txt_cstr(" +", WHT_PAL);
		    txt_chexnum(jj*stride*size, 7, RJ_ZF, WHT_PAL);
		}
	    }
	    break;
	case 8:
	    txt_cstr("   ", WHT_PAL);
	    for (jj=0; jj < DM_MEM_COLS/2; ++jj) {
		txt_cstr("    +", WHT_PAL);
		txt_chexnum(jj*stride*size, 8, RJ_ZF, WHT_PAL);
		txt_cstr("    ", WHT_PAL);
	    }
	    break;
    }
    for (ii=DM_BASE_ROW+1; len && ii < DM_MEM_ROWS+DM_BASE_ROW+1; ++ii) {
	txt_hexnum(col, ii, ptr.ubase, 8, RJ_ZF, WHT_PAL);
	txt_cstr(" ", WHT_PAL);
	switch (size) {
	    case 1:
		for (jj=0; len && jj < DM_MEM_COLS*4; ++jj, ptr.cbase += stride, --len) {
		    txt_cstr(" ", WHT_PAL);
		    txt_chexnum(*ptr.cbase, 2, RJ_ZF, GRY_PAL);
		}
		break;
	    case 2:
		for (jj=0; len && jj < DM_MEM_COLS*2; ++jj, ptr.sbase += stride, len -= 2) {
		    txt_cstr(" ", WHT_PAL);
		    txt_chexnum(*ptr.sbase, 4, RJ_ZF, GRY_PAL);
		}
		break;
	    case 4:
		for (jj=0; len && jj < DM_MEM_COLS; ++jj, ptr.wbase += stride, len -= 4) {
		    txt_cstr(" ", WHT_PAL);
		    txt_chexnum(*ptr.wbase, 8, RJ_ZF, GRY_PAL);
		}
		break;
	    case 8:
		for (jj=0; len && jj < DM_MEM_COLS/2; ++jj, ptr.wbase += 2*stride, len -= 8) {
		    txt_cstr(" ", WHT_PAL);
		    txt_chexnum(ptr.wbase[1], 8, RJ_ZF, GRY_PAL);
		    txt_chexnum(ptr.wbase[0], 8, RJ_ZF, GRY_PAL);
		}
		break;
	}
    }
    if (autoinc) {
	mem->rdesc.rd_base = ptr.wbase;
    }	
    return 0;
}

static int dump_memory( parm_mp )
const struct menu_d *parm_mp;
{
    struct parm work[DM_NPARMS],*wp;
    const struct parm *cwp;
    struct ramt descrip;
    int idx,status;

    ExitInst(INSTR_PAL);
    wp = work;
    cwp = dm_plist;
    descrip = *(struct ramt *)parm_mp;

    do {
	*wp++ = *cwp++;
    } while (wp[-1].label != 0);
    DM_MEM_COLS = (AN_VIS_COL >= 64) ? 4 : 2;
    DM_MEM_ROWS = (AN_VIS_ROW-DM_BASE_ROW-4);
    DM_MEM_SCREEN = (DM_MEM_ROWS*DM_MEM_COLS*4);
    work[1].val = (DM_MEM_SCREEN > 256) ? 256 : DM_MEM_SCREEN;
    for ( idx = 0 ; idx < DM_NPARMS ; ++idx ) {
	dm_cb(work+idx,idx,(struct opaque *)&descrip);
    }
    while (1) {
	status = utl_enter(work, dm_cb, (struct opaque *)&descrip);
	if ( status & SW_NEXT ) return 0;
	if ( status & SW_ACTION ) {
	    /* do this action */
	    status = dump_mem(&descrip);
	    work[0].val = (U32)descrip.rdesc.rd_base;
	}
    }
    return 0;
}

#if HOST_BOARD == PHOENIX
#define U_NUMBER "(U43) "
#endif

#if ( HOST_BOARD == FLAGSTAFF ) || ( HOST_BOARD == PHOENIX_AD ) || ( HOST_BOARD == SEATTLE )
#define U_NUMBER "(U32) "
#endif

#if HOST_BOARD == CHAMELEON
# define U_NUMBER "(U42) "
# if !defined BOOT_EPROM_SIZE
#  define BOOT_EPROM_SIZE	0x00040000
# endif
#endif

#ifndef U_NUMBER
# define U_NUMBER "(\?\?\?) "
#endif

#if !defined BOOT_EPROM_SIZE
#  define BOOT_EPROM_SIZE	0x00080000
# endif

struct rom_descrip {
    char *name;
    struct rdb	descript;
};

#if (HOST_BOARD == SEATTLE) && SA_DIAGS
#define EXPROM_BASE	0x1FD00000
#define EXPROM_SIZE	0x00100000
#define ADDR_TEST_OFF   0x00000000
#define DATA_TEST_OFF   0x00060000
#define EXPROM_SIG_OFF  0x00070000

STATIC const struct rom_descrip rom_list[] = {
/*
 *	The seed value is actually +1 because we want to add up to 0,
 *	while the cksum program sets to add to 0xFF
 */
 { "Boot EPROM (U32)", {(U32*)0xBFC00000, BOOT_EPROM_SIZE, 0x04030201} },
 { "Expansion EPROM (U33)", {(U32*)PHYS_TO_K1(EXPROM_BASE), EXPROM_SIZE, 0} },
 { 0, {0, 0, 0} }
};
#endif

#if !NO_ROM_TEST_MENU
STATIC const struct rom_descrip rom_list[] = {
/*
 *	The seed value is actually +1 because we want to add up to 0,
 *	while the cksum program sets to add to 0xFF
 */
 { "BOOT ROM ", {(U32*)0xBFC00000, BOOT_EPROM_SIZE, 0x04030201} },
 { 0, {0, 0, 0} }
};
#endif

#if (HOST_BOARD == SEATTLE) && SA_DIAGS
static const char passed[] = " PASSED", failed[] = " FAILED", has_failed[] = " (has FAILED at least once)";
	    
STATIC int memexp(VU8 *base, int size, int row, int loop, int hist) {
    U8   memrval, rdv;
    int  ix;
    U32  addr_sh;
    VU8  *memaddr;

    memaddr = base + EXPROM_SIG_OFF;
    if (*(memaddr+0) != 'P' &&  /* Pinball */
        *(memaddr+1) != 'R' &&  /* Rules */
        *(memaddr+2) != 'T' &&  /* The */
        *(memaddr+3) != 'U') {  /* Universe ! */
	txt_cstr("is ", WHT_PAL);
	txt_cstr("NOT", RED_PAL);
	txt_cstr(" installed.", WHT_PAL);
	return 0;		/* signal an error */
    }

    memaddr = base;
    memrval = 1;

    for (addr_sh = 0; (1<<addr_sh) < size; addr_sh++) {
        if ((rdv = *memaddr) != memrval) {
exp_rom_rderr:
	    txt_cstr(failed, RED_PAL);
	    ++row;	
	    txt_str(3, row++, "Read error at addr: ", WHT_PAL);
	    txt_chexnum((U32)memaddr, 8, RJ_ZF, RED_PAL);
	    txt_str(5, row++, "Expected to read: ", WHT_PAL);
	    txt_chexnum(memrval, 2, RJ_ZF, GRN_PAL);
	    txt_str(8, row++, "Actually read: ", WHT_PAL);
	    txt_chexnum(rdv, 2, RJ_ZF, RED_PAL);
	    txt_str(8, row++, "Bits in error: ", WHT_PAL);
	    txt_chexnum(memrval^rdv, 2, RJ_ZF, RED_PAL);
	    return row;
        }

        memaddr = (VU8*)((U32)base | (1 << addr_sh));
        memrval++;
    }

    memaddr = base + DATA_TEST_OFF;
    memrval = 0;

    for (ix = 0; ix < 9; ix++) {
        if ((rdv = *memaddr) != memrval) {
	    goto exp_rom_rderr;
        }

        memaddr++;
        memrval = 1 << ix;
    }

    memrval = 0xFF;

    for (ix = 0; ix < 9; ix++) {
        if ((rdv = *memaddr) != memrval) {
	    goto exp_rom_rderr;
        }

        memaddr++;
        memrval = (0xFF ^ (1 << ix));
    }

    txt_cstr(passed, GRN_PAL);
    if (loop && hist) txt_cstr(has_failed, YEL_PAL);
    return row;				/* signal success */
}
#endif

STATIC int ROMTest(const struct menu_d *smp) {
    const struct rom_descrip *rd;
    int row;

#if !SA_DIAGS
    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    row = 3;
    for ( rd = &rom_list[0]; rd->name; ++rd ) {
	txt_str(3, row++, rd->name, WHT_PAL);
	txt_cstr(U_NUMBER, WHT_PAL);
	if ( rom_cksum((const struct rdb *)&rd->descript) ) {
	    txt_cstr(" FAILED", RED_PAL);
	    prc_delay0();
	    while ( rom_cksum((const struct rdb *)&rd->descript) ) {
		if (ctl_read_sw(0) & SW_NEXT) return 0;
	    }
	} else {
	    txt_cstr(" PASSED", GRN_PAL);
	}
    }
    while ( (ctl_read_sw(SW_NEXT) & SW_NEXT) == 0 ) prc_delay0();
#else
    int errs, err_hist=0, idx, sts, succ=0, fail=0;
    struct diag_params zag_diags;

    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    do {
	errs = 0;
	prc_delay(0);
	row = 3;
	idx = 1;
	txt_clr_wid(3, row, AN_VIS_COL-4);
	txt_str(3, row, "Battery RAM (U16)", WHT_PAL);
	sts = zag_test_bram(&zag_diags);
	if (sts) {
	    txt_cstr(failed, RED_PAL);
	    errs |= idx;
	    err_hist |= idx;
	} else {
	    txt_cstr(passed, GRN_PAL);
	    if ((succ+fail) && (err_hist&idx)) {
		txt_cstr(has_failed, YEL_PAL);
	    }
	}
	for ( idx += idx, rd = &rom_list[0]; rd->name; ++rd, idx += idx ) {
	    row += 2;
	    txt_clr_wid(3, row, AN_VIS_COL-4);
	    txt_str(3, row, rd->name, WHT_PAL);
	    if (idx == 2) {
		if ( rom_cksum((const struct rdb *)&rd->descript) ) {
		    txt_cstr(failed, RED_PAL);
		    errs |= idx;
		    err_hist |= idx;
		} else {
		    txt_cstr(passed, GRN_PAL);
		    if ((succ+fail) && (err_hist&idx)) {
			txt_cstr(has_failed, YEL_PAL);
		    }
		}
	    } else if (idx == 4) {
		int new_row;
		new_row = memexp((VU8*)PHYS_TO_K1(EXPROM_BASE), EXPROM_SIZE, row, succ+fail, err_hist&idx);
		if (new_row != row) {
		    errs |= idx;
		    err_hist |= idx;
		}
		if (new_row) row = new_row;
	    }
	}
	if (errs) {
	    ++fail;
	} else {
	    ++succ;
	}
	row = AN_VIS_ROW/2;
	txt_str(3, row++, "Loop counts: ", WHT_PAL);
	txt_str(3, row++, "  Successes: ", WHT_PAL);
	txt_cdecnum(succ, 6, RJ_BF, GRN_PAL);
	txt_str(3, row++, "   Failures: ", WHT_PAL);
	txt_cdecnum(fail, 6, RJ_BF, fail ? RED_PAL : GRN_PAL);
	if (ctl_read_sw(SW_NEXT) & SW_NEXT) return 0;
    } while (1);
#endif
    return 0;
}

#else /* SPECIAL_TESTS */
#ifndef RAM_DESC
#define RAM_DESC(title, rtn, base, len, bits, notes) \
    {\
	{ title, rtn},\
	{ (unsigned long *)(base), (len), (bits) },\
	(notes)\
    }
#endif /* ndef RAM_DESC */

#ifndef RAM_TIME
#define RAM_TIME ""
#endif

#if !SA_DIAGS
RAM_DESC( "\nWORKING RAM (QUICK)", norm_test, \
    	0, 0, 0, \
    	 (NOTE_NOINT|NOTE_QUICK) \
),
RAM_DESC( "WORKING RAM" RAM_TIME, norm_test, \
    	0, 0, 0, \
    	 (NOTE_NOINT) \
),
#else
RAM_DESC( "\nEPROM+BRAM TEST", ROMTest, \
    	0, 0, 0, \
    	0
),
#endif
#if HOST_BOARD == CHAMELEON
RAM_DESC( "\nRAMROM BOARD (QUICK)", norm_test, \
    	RAMROM_BASE, 0x00200000, 0, \
    	 (NOTE_QUICK) \
),
RAM_DESC( "RAMROM BOARD", norm_test, \
    	RAMROM_BASE, 0x00200000, 0, \
    	 (0) \
),
#endif
#if 0 && BOOT_FROM_DISK
RAM_DESC( "BANK 1 RAM (QUICK)", norm_test, \
    	DRAM_BASEnc+0x00200000, 0x00200000, 0, \
    	 (NOTE_QUICK) \
),

RAM_DESC( "BANK 1 RAM", norm_test, \
    	DRAM_BASEnc+0x00200000, 0x00200000, 0, \
    	 (0) \
),
#endif

#if 0
RAM_DESC( "\n3DF/X FB RAM (QUICK)", frame_buf_test0, \
    	SST_BASE+0x00400000, 0x00200000, 0, \
    	 (NOTE_QUICK) \
),

RAM_DESC( "3DF/X FB RAM", frame_buf_test0, \
    	SST_BASE+0x00400000, 0x00200000, 0, \
    	 (NOTE_QUICK) \
),
#endif

#if !NO_AUTO_TEST_MENU
RAM_DESC( "\nALL RAM (QUICK)", auto_test, \
	 0, 0, 0, \
	NOTE_QUICK\
),

RAM_DESC( "ALL RAM (FULL)", auto_test, \
	 0, 0, 0, \
	0\
),
#endif

#if !SA_DIAGS
RAM_DESC( SECRET_HS "\nBattery Backed RAM", lbram_test, \
    	BRAM_BASE, BRAM_SIZE, 0xFFFFFF00, 0
),
#endif
#if !SA_DIAGS
# define CB_SPACER ""
#else
# define CB_SPACER "\n"
#endif

RAM_DESC( SECRET_HS CB_SPACER "CUSTOM Battery Backed RAM TEST (loop)", custom_bram_test, \
	 0, 0, 0, \
	(NOTE_NOVID|NOTE_LOOPING)\
),

RAM_DESC( SECRET_HS "\nCUSTOM TEST (loop)", custom_test, \
	 0, 0, 0, \
	(NOTE_NOVID|NOTE_LOOPING)\
),

RAM_DESC( SECRET_HS "\nDUMP MEMORY", dump_memory, \
	 0, 0, 0, \
	0\
),

#endif /* SPECIAL_TESTS */

