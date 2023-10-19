/* THIS VERSION SPECIAL FOR MINH, DO NOT USE */
/*	scope.c
 *
 *		Copyright 1994 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 *
 *	This module provides "scope loop" functions to aid in testing new
 *	hardware. Parameter entry is done via the functions in entry.c.
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: scope.c,v 1.9 1997/07/01 22:48:44 albaugh Exp $";
#endif
#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>

#define	STATIC	static

#define OP_2WRT	(4)
#define OP_WRT	(2)
#define OP_RD	(1)

#define SZ_BYT	(0)
#define SZ_HWD	(1)
#define SZ_WD	(2)

#ifdef MINH_WATCH
#define SZ_DWD	(3)
#define OP_NOINT (8)
#endif

struct sl_desc {
    unsigned long addr;
    unsigned long data1;
    unsigned long data2;
    unsigned long loops;
    int		size;
    int		op;
};

struct slt {
    struct menu_d	menu_desc;	/* _NOT_ a pointer */
    int			option;		/* one of OP_* */
};
#ifdef DEBUG
/* Scope-loop code, should only be visible in development.
 * Else delete all but the name to save space without change
 * to makefile
 */
STATIC int scope_loop
PARMS((
const struct menu_d *parm_mp
));

STATIC const struct slt SCOPE_LIST[] =
{
    { {"SCOPE LOOPS", 0}, 0},
    { {"READ", scope_loop}, OP_RD},
    { {"WRITE", scope_loop}, OP_WRT},
    { {"WRITE/READ", scope_loop}, OP_WRT|OP_RD},
    { {"WRITE/READ/WRITE/READ", scope_loop}, OP_RD|OP_WRT|OP_2WRT},
#ifdef OP_NOINT
    { {"WRITE/READ/WRITE/READ(NO IRQ)", scope_loop}, OP_RD|OP_WRT|OP_2WRT|OP_NOINT},
#endif
    { { 0, 0 }, 0 }
};

STATIC void
scope_host(stp)
struct sl_desc *stp;
{
    int op = stp->op;
    int	size = stp->size;
    int	count = stp->loops;
    int old_fields,old_ipl;

    old_ipl = INTS_ON;
    old_fields = 42;

    if ( (op & OP_2WRT) && !(op & OP_WRT) ) {
	/* Double-data scope loop without write? */
	return;
    }
#ifdef OP_NOINT
    if ( op & OP_NOINT ) {
	old_fields = vid_fields(-1);
	if ( old_fields >= 0 ) prc_delay(old_fields);
	old_ipl = prc_set_ipl(INTS_OFF);	
	op &= ~OP_NOINT;
    }
#endif
    if ( op & OP_2WRT ) {
	/* write alternate patterns, and possibly read back */
	op &= OP_RD;	/* leave only the "read" indication */
	switch (size) {
	    case SZ_BYT : {
		char ds1,ds2,dl;
		volatile char *ptr = (volatile char *)(stp->addr);
		ds1 = stp->data1;
		ds2 = stp->data2;
		if ( op ) while ( count-- ) {
		    *ptr = ds1 ; dl = *ptr; *ptr = ds2 ; dl = *ptr;
		}
		else while ( count-- ) { *ptr = ds1 ; *ptr = ds2; }
		break;
	    }
	    case SZ_HWD : {
		short ds1,ds2,dl;
		volatile short *ptr = (volatile short *)(stp->addr);
		ds1 = stp->data1;
		ds2 = stp->data2;
		if ( op ) while ( count-- ) {
		    *ptr = ds1 ; dl = *ptr; *ptr = ds2 ; dl = *ptr;
		}
		else while ( count-- ) { *ptr = ds1 ; *ptr = ds2; }
		break;
	    }
	    case SZ_WD : {
		long ds1,ds2,dl;
		volatile long *ptr = (volatile long *)(stp->addr);
		ds1 = stp->data1;
		ds2 = stp->data2;
		if ( op ) while ( count-- ) {
		    *ptr = ds1 ; dl = *ptr; *ptr = ds2 ; dl = *ptr;
		}
		else while ( count-- ) { *ptr = ds1 ; *ptr = ds2; }
		break;
	    }
#ifdef SZ_DWD
	    case SZ_DWD : {
		long ds1,ds2,dl;
		volatile long *ptr = (volatile long *)(stp->addr);
		volatile long *ptr1 = ptr+1;
		ds1 = stp->data1;
		ds2 = stp->data2;
		if ( op ) while ( count-- ) {
		    *ptr = ds1 ; *ptr1 = ds2 ; dl = *ptr; dl = *ptr1 ;
		    *ptr = ds2 ; *ptr1 = ds1 ; dl = *ptr; dl = *ptr1 ;
		}
		else while ( count-- ) {
		    *ptr = ds1 ; *ptr1 = ds2; *ptr = ds2; *ptr1 = ds1;
		    }
		break;
	    }
#endif
	}
    } else if ( op & OP_WRT ) {
	/* write, and possibly read back */
	op &= OP_RD;	/* leave only the "read" indication */
	switch (size) {
	    case SZ_BYT : {
		char ds,dl;
		volatile char *ptr = (volatile char *)(stp->addr);
		ds = stp->data1;
		if ( op ) while ( count-- ) { *ptr = ds ; dl = *ptr; }
		else while ( count-- ) *ptr = ds;
		break;
	    }
	    case SZ_HWD : {
		short ds,dl;
		volatile short *ptr = (volatile short *)(stp->addr);
		ds = stp->data1;
		if ( op ) while ( count-- ) { *ptr = ds ; dl = *ptr; }
		else while ( count-- ) *ptr = ds;
		break;
	    }
	    case SZ_WD : {
		long ds,dl;
		volatile long *ptr = (volatile long *)(stp->addr);
		ds = stp->data1;
		if ( op ) while ( count-- ) { *ptr = ds ; dl = *ptr; }
		else while ( count-- ) *ptr = ds;
		break;
	    }
#ifdef SZ_DWD
	    case SZ_DWD : {
		long ds1,ds2,dl;
		volatile long *ptr = (volatile long *)(stp->addr);
		volatile long *ptr1 = ptr+1;
		ds1 = stp->data1;
		ds2 = stp->data2;
		if ( op ) while ( count-- ) {
		    *ptr = ds1 ; *ptr1 = ds2 ; dl = *ptr; dl = *ptr1 ;
		}
		else while ( count-- ) {
		    *ptr = ds1 ; *ptr1 = ds2;
		    }
		break;
	    }
#endif
	}
    } else {
	/* read-only loop */
	switch (size) {
	    case SZ_BYT : {
		char dl;
		volatile char *ptr = (volatile char *)(stp->addr);
		while ( count-- ) dl = *ptr;
		break;
	    }
	    case SZ_HWD : {
		short dl;
		volatile short *ptr = (volatile short *)(stp->addr);
		while ( count-- ) dl = *ptr;
		break;
	    }
	    case SZ_WD : {
		long dl;
		volatile long *ptr = (volatile long *)(stp->addr);
		while ( count-- ) dl = *ptr;
		break;
	    }
#ifdef SZ_DWD
	    case SZ_DWD : {
		long dl;
		volatile long *ptr = (volatile long *)(stp->addr);
		volatile long *ptr1 = ptr+1;
		while ( count-- ) { dl = *ptr; dl = *ptr1; }
		break;
	    }
#endif
	} /* end switch */
    }
    if ( old_fields != 42 ) {
	/* This was a "NO_INT" case */
	prc_set_ipl(old_ipl);
	vid_fields(old_fields);
    }
}

/*		Use generic parameter entry to build descriptor,
 *	then run test.
 */

/*	Provide a "safe" location to loop on, so just hitting
 *	ACTION will not do something deadly.
 */
STATIC U32 test_cell;
#if (HOST_BOARD == HCR4K)
/* For reasons known only to Dave and Forrest, the whole world was
 * presumed to use the MIPS method of specifying an address to be
 * "un-cached". Perhaps they will re-consider... (16APR96 MEA)
 */
#define TEST_CELL ( (U32) ( &test_cell + ( 0x20000000 / sizeof(U32) ) ) )
#else
#define TEST_CELL ( (U32) &test_cell )
#endif

/*		plist[] (below) describes the parameters we wish to
 *	enter.
 */
#if GUN_CNT
#define TOP (5)
#else
#define TOP (3)
#endif

static const struct parm plist [] = {
    /* Label		value		col/row		sigdig		curdig	*/
    { "Base Address: ",	TEST_CELL,	4,TOP,		(HEX_FIELD|8),	0 	},
#define PIDX_ADDR (0)
    { "Size:         ",	2,		4,TOP+2,	(HEX_FIELD|1),	0 	},
#define PIDX_SIZE  (1)
    { "Loop Counter: ",	1000,		4,TOP+4,	5,		0 	},
#define PIDX_LOOPS (2)
    { "Data 1:       ",	0,		4,TOP+6,	(HEX_FIELD|8),	0 	},
#define PIDX_DATA1 (3)
    { "Data 2:       ",	0xffffffff,	4,TOP+8,	(HEX_FIELD|8),	0 	},
#define PIDX_DATA2 (4)
    { 0,		0,		0,0,	0,		0 	}};

#define NPARMS (sizeof (plist)/sizeof(plist[0]))

/* Provide a place to "remember" last-used scope parameters for any
 * given diagnostic session.
 */
static struct parm work[NPARMS];

STATIC const char * const size_names[] = { "BYTE", "SHORT", "LONG"
#ifdef SZ_DWD
    , "64-BIT"
#endif
};
#define N_SIZES (sizeof(size_names)/sizeof(size_names[0]))

/*		sl_cb() is the "callback". That is, it is called by the
 *	parameter entry code to validate changes to the parameters and
 *	possibly copy them to the descriptor.
 */
STATIC int
sl_cb(struct parm *pp, int idx,struct opaque *op)
{
    struct sl_desc *sdp = (struct sl_desc *)op;
    unsigned long val;
    long	 delta;
    unsigned long mask;

    val = pp->val;
    switch ( idx ) {
	case PIDX_ADDR :
	    /* Validate address for loop. Force increment or
	     * decrement to occur only on boundaries appropriate to size.
	     */
	    mask = (1<<sdp->size)-1;
	    delta = val - sdp->addr;
	    if ( delta >= 0 && delta < mask ) val += mask;
	    val &= ~mask;
	    pp->val = val;
	    sdp->addr = val;
	    break;

	case PIDX_SIZE : {
	    int row,col;

	    delta = val;	/* get it into a signed var */
	    if ( delta < 0 ) val = N_SIZES-1;
	    else if ( val >= N_SIZES ) val = 0;
	    mask = (1<<val)-1;
	    row = plist[PIDX_SIZE].row;
	    col = plist[PIDX_SIZE].col + strlen(plist[PIDX_SIZE].label)+2;
	    txt_clr_str(col,row,size_names[sdp->size],MNORMAL_PAL);
	    txt_str(col,row,size_names[sdp->size = val],MNORMAL_PAL);
	    pp->val = val;
	    sdp->addr &= ~mask;
	    break;
	}

	case PIDX_LOOPS : {
	    int row,col;

	    row = plist[PIDX_LOOPS].row;
	    col = plist[PIDX_LOOPS].col + strlen(plist[PIDX_LOOPS].label)+5;
	    txt_str(col,row,"000",MNORMAL_PAL);
	    sdp->loops = val * 1000;
	    break;
	}

	case PIDX_DATA1 :
	    sdp->data1 = val;
	    break;

	case PIDX_DATA2 :
	    sdp->data2 = val;
	    break;

    } /* end switch */
    return 1;
}

STATIC
int scope_loop( parm_mp )
const struct menu_d *parm_mp;
{
    struct parm *wp;
    const struct parm *cwp;
    struct sl_desc descrip;
    int idx,status,which;

    wp = work;
    cwp = plist;

    if ( wp->label == 0 ) {
	/* first time we have been called, copy
	 * initial values to work.
	 */
	do {
	    *wp++ = *cwp++;
	} while (wp[-1].label != 0);
    }
    else {
	/* Just re-generate any labels which may
	 * have been clobbered last time.
	 */
	do {
	    (wp++)->label = (cwp++)->label;
	} while (wp[-1].label != 0);
    }

    which = ((struct slt *)parm_mp)->option;

    if ( !(which & OP_2WRT) ) {
	/* no need for second data */
	work[PIDX_DATA2].label = 0;
    }
    if ( !(which & OP_WRT) ) {
	/* no need for first data */
	work[PIDX_DATA1].label = 0;
    }

    memset(&descrip,0,sizeof(descrip));
    descrip.op = which;

    for ( idx = 0 ; idx < NPARMS ; ++idx ) {
	if ( work[idx].label == 0 ) break;
	sl_cb(work+idx,idx,(struct opaque *)&descrip);
    }
    while (1) {
	status = utl_enter(work,sl_cb,(struct opaque *)&descrip);
	if ( status & SW_NEXT ) return 0;
	if ( status & SW_ACTION ) {
	    do {
		scope_host(&descrip);
		/* Some implementations of GUTS, especially
		 * those running "over" an OS, need to call
		 * prc_delay0() just to update the status
		 * of the controls.
		 */
		prc_delay0();
	    } while ( ! (ctl_read_sw(SW_NEXT) & SW_NEXT) );
	}
    }
    return 0;
}

int
scope_loops( smp )
const struct menu_d *smp;	/* Selected Menu Pointer */
{
    return st_menu((struct menu_d *)&SCOPE_LIST[0],sizeof(SCOPE_LIST[0]),RED_PALB,0);
}
#else
int
scope_loops( smp )
const struct menu_d *smp;	/* Selected Menu Pointer */
{
    txt_str(-1,AN_VIS_ROW/2,"REMOVED_TO_SAVE_SPACE",RED_PALB|AN_BIG_SET);
    while ( (ctl_read_sw(SW_NEXT) & SW_NEXT) == 0 ) prc_delay0();
    return 0;
}
#endif
