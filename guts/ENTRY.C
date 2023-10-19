/*		entry.c
 *	fairly generic parameter entry for development tests
 *
 *		Copyright 1993 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: entry.c,v 1.8 1997/07/01 22:23:15 albaugh Exp $";
#endif

#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>

#define	PRIVATE	static

/*	Each parameter is described by a struct that indicates its name,
 *	position on screen, and some working state. See st_proto.h for
 *	the definition of "struct parm" and the definitions HEX_FIELD
 *	and SIGNED_FIELD
 */
/* Following is transitional, to allow compilation with old st_proto.h
 */
#ifndef BCD_FIELD
#define BCD_FIELD (HEX_FIELD|0x20)
#endif
#ifndef BCD_TAG
/* Tag to "correct" hex numbers which are BCD */
#define BCD_TAG	  (BCD_FIELD&~HEX_FIELD)
#endif
#ifndef BCD_CORR
#define BCD_CORR (0x66666666)
#endif

#define MAX_WID (20)		/* suffices for 64-bit long in decimal */
PRIVATE void
disp_parm(pp)
struct parm *pp;
{
    int lab_wid,num_wid,posn,flags;
    long val;

    char valstr[MAX_WID],*vs = valstr;

    num_wid = pp->sig_dig;
    flags = num_wid & (HEX_FIELD|SIGNED_FIELD|BCD_TAG);
    num_wid -= flags;
    val = pp->val;
    if ( flags & SIGNED_FIELD ) {
	if ( val < 0 ) {
	    val = -val;
	    *vs++ = '-';
	} else *vs++ = ' ';
    }
    if ( flags & (HEX_FIELD|BCD_TAG) ) utl_chex(val,vs,num_wid,RJ_ZF);
    else utl_cdec(val,vs,num_wid,RJ_ZF);
    lab_wid = txt_width(pp->label,MNORMAL_PAL);
    txt_clr_wid(pp->col,pp->row,lab_wid+num_wid+(vs-valstr));
    txt_str(pp->col,pp->row,pp->label,MNORMAL_PAL);
    if ( (posn = pp->cur_dig) > 0 ) {
	txt_cstr(valstr,TITLE_PAL);
	/* If there is a current digit, highlight it. A "cur_dig" of
	 * 0 means there is no "current" digit in this parm. A cur_dig
	 * of one is the LSD, cur_dig == num_wid means MSD.
	 *
	 * Bend posn to be the index in valstr of the current digit.
	 */
	posn = (num_wid)-posn;
	vs[posn+1] = '\0';
	txt_str(pp->col+lab_wid+posn+(vs-valstr),pp->row,vs+posn,MHILITE_PAL);
    }
    else txt_cstr(valstr,MNORMAL_PAL);
}

#if KBD_UP
extern int kbd_getch();
extern int kbd_read( unsigned char *, int);
extern U32 strtoul( const char *, char **, int);
#endif

struct opaque;

int
utl_enter(pp,cb,cbp)
struct parm *pp;
int (*cb)(struct parm *,int,struct opaque*);
struct opaque *cbp;
{
    struct parm *wpp = pp;
    unsigned long edges,val,old_val;
    long inc;
    int radix,width,flags;
#if KBD_UP
#define BUF_WID (40)
    char numbuf[BUF_WID+1];
    int keyval;

    kbd_read(0,0);	/* Flush old input */
#endif
    while ( wpp && wpp->label ) {
	wpp->cur_dig = 0;
	disp_parm(wpp++);
    }
    wpp = pp;
    inc = 1;
    radix = 10;
    old_val = val = wpp->val;

    flags = wpp->sig_dig & (HEX_FIELD|SIGNED_FIELD|BCD_TAG);
    width = wpp->sig_dig - flags;
    if ( flags & HEX_FIELD ) radix = 0x10;
    /* This will be annoying to keyboard users, but until
     * we come up with a better way, we need to start at the
     * LSDigit, so that inc is correct.
     */
    wpp->cur_dig = 1;
#if KBD_UP
    if ( flags & HEX_FIELD ) {
	radix = 0x10;
	if ( width > 8 ) width = 8;
	inc = 1 << ((width-1)<<2);
	wpp->cur_dig = width;
    } else {
	int i;
	radix = 10;
	if ( width > 10 ) width = 10;
	wpp->cur_dig = width;
	inc = 1;
	i = width;
	while ( --i ) inc *= 10;
    }
#endif
    disp_parm(wpp);
    while ( 1 ) {
	/* Act on Joystick switches, calling cb when an entry changes
	 */
	prc_delay0();
	edges = ctl_read_sw(JOY_ALL);
	if ( edges & SW_NEXT ) break;
#if KBD_UP
	keyval = kbd_getch();
	if ( keyval > 0 ) {
	    /* Got a key that was not one of our special cases.
	     * dump the current value to a string buffer, "edit"
	     * it by inserting the character, and re-convert to
	     * an unsigned integer.
	     */
	    U32 tval;
	    char *rp;
	    if ( keyval == '\t' ) {
		/* TAB sets cursor to LSDigit, and fakes
		 * a move right, to go to next field.
		 */
		wpp->cur_dig = 1;
		edges &= SW_ACTION;
		edges |= J_RIGHT;
	    }
	    else if ( flags & HEX_FIELD ) {
		utl_chex(val,numbuf,width,RJ_ZF);
	    }
	    else utl_cdec(val,numbuf,width,RJ_ZF);
	    if ( flags & BCD_TAG && (keyval < '0' || keyval > '9') ) {
		/* BCD is treated like HEX, except that only
		 * 0-9 are valid. squash any other to force
		 * failure below.
		 */
		keyval = '?';
	    }
	    numbuf[width-wpp->cur_dig] = keyval;
	    tval = strtoul(numbuf,&rp, radix);
	    if ( rp == (numbuf+width) ) {
		/* the conversion succeded completely, accept
		 * the digit and bump to the right.
		 */
		val = tval;
		edges &= SW_ACTION;
		edges |= J_RIGHT;
	    }
	}
#endif /* KBD_UP */
	if ( flags & BCD_TAG ) {
	    /* BCD numbers need special care */
	    U32 acc,bcd_inc;

	    bcd_inc = 0;
#if J_UP
	    if ( edges & J_UP ) bcd_inc = inc;	/* use positive "inc" */
	    if ( edges & J_DOWN ) bcd_inc = (~BCD_CORR-inc)+1;	/* or 10's compl. */
#endif
	    if ( bcd_inc ) {
		/* "adding" either the excess-6 form of "inc" or "-inc"
		 * to the current value.
		 */
		U32 nyb_carries;

		/* Add val and inc, in binary */
		acc = val + bcd_inc + BCD_CORR;

		/* Carry into a nybble is indicated by a _difference_
		 * (in the lsb of the nybble) between the result (acc)
		 * and the exclusive-or of the two inputs.
		 */
		nyb_carries = (acc ^ val ^ bcd_inc) & 0x11111111;

		/* Shifted down, these "carry in" bits are "carry out"
		 * bits.
		 */
		nyb_carries >>= 4;

		/* The carry out of the most-significant nybble is
		 * detected when the result (acc) is greater than
		 * an input.
		 */
		if ( acc < (val|bcd_inc|BCD_CORR) ) nyb_carries |= (1<<28);

		/* Now that we have each "carry out" lined up with the
		 * nybble it came from, subtract the correction factor
		 * from every nybble that _didn't_ carry. Nybbles that
		 * carried in binary are already correct in BCD, but
		 * nybbles which did not are "too high" by the '6' we
		 * added above.
		 */
		val = acc - (BCD_CORR-(6*nyb_carries));
	    }
	} else {
	    /* Normal HEX or DECIMAL number */
#if J_UP
	    if ( edges & J_UP ) val += inc;
	    if ( edges & J_DOWN ) val -= inc;
#endif
	}
	if ( edges & J_LEFT ) {
	    /* moving to more significant digit, or previous parm
	     */
	    if ( wpp->cur_dig < width ) {
		inc *= radix;
		wpp->cur_dig += 1;
		disp_parm(wpp);		/* just move cursor */
	    } else {
		/* new field. we need to back-up unless we are at
		 * beginning.
		 */
		wpp->cur_dig = 0;	/* not longer "hot" */
		wpp->val = val;
		if ( val != old_val ) cb(wpp,wpp-pp,cbp); /* validate changes */
		disp_parm(wpp);				  /* and re-paint */
		if ( wpp == pp ) {
		    /* already at beginning, skip to end
		     */
		    while ( wpp[1].label ) ++wpp;
		} else --wpp;
		flags = wpp->sig_dig & (HEX_FIELD|SIGNED_FIELD|BCD_TAG);
		width = wpp->sig_dig - flags;
		if ( flags & HEX_FIELD ) radix = 0x10;
		else radix = 10;
		inc = 1;
		wpp->cur_dig = 1;
		val = wpp->val;
		disp_parm(wpp);
		old_val = val;
	    } /* end if ( digit vs field ) */
	} /* end if ( moving left ) */
	if ( edges & J_RIGHT ) {
	    /* moving to less significant digit, or next parm
	     */
	    if ( wpp->cur_dig > 1 ) {
		inc /= radix;
		wpp->cur_dig -= 1;
		disp_parm(wpp);		/* just move cursor */
	    } else {
		m_int i;
		/* new field. We need to wrap to beginning if at end
		 */
		wpp->cur_dig = 0;	/* not longer "hot" */
		wpp->val = val;
		if ( val != old_val ) cb(wpp,wpp-pp,cbp); /* validate changes */
		disp_parm(wpp);				  /* and re-paint */
		if ( wpp[1].label == 0) {
		    /* already at end, wrap to beginning
		     */
		    wpp = pp;
		} else ++wpp;
		flags = wpp->sig_dig & (HEX_FIELD|SIGNED_FIELD|BCD_TAG);
		width = wpp->sig_dig - flags;
		if ( flags & HEX_FIELD ) {
		    radix = 0x10;
		    if ( width > 8 ) width = 8;
		    inc = 1 << ((width-1)<<2);
		    wpp->cur_dig = width;
		} else {
		    radix = 10;
		    if ( width > 10 ) width = 10;
		    wpp->cur_dig = width;
		    inc = 1;
		    i = width;
		    while ( --i ) inc *= 10;
		}
		val = wpp->val;
		disp_parm(wpp);
		old_val = val;
	    } /* end if ( digit vs field ) */
	} /* end if ( moving right ) */
	if ( val != old_val || edges & SW_ACTION ) {
	    /* validate and re-paint */
	    wpp->val = val;
	    cb(wpp,wpp-pp,cbp);
	    old_val = val = wpp->val;
	    disp_parm(wpp);
	    if ( edges & SW_ACTION ) return SW_ACTION;
	}
    } /* end while (1) */
    return SW_NEXT;
}

#if (0)
static const struct parm plist [] = {
    /* Label		value		col	row	sigdig	curdig	*/
    { "Decparm 1 ",	0,		3,	5,	5,	0 	},
    { "Decparm 2 ", 	42,		20,	5,	10,	0 	},
    { "BCDparm ", 	0x49,		3,	6,	0xA8,	0 	},
    { "Hexparm 0x", 	0x55AA,		3,	7,	0x87,	0 	},
    { 0,		0,		0,	0,	0,	0 	}};

#define NPARMS (sizeof (plist)/sizeof(plist[0]))

int parm_cb(struct parm *pp,int idx,struct opaque *cbp) {return idx;}

int
parm_test(smp)
const struct menu_d *smp;
{
    struct parm work[NPARMS],*wp;
    const struct parm *cwp;

    ExitInst(INSTR_PAL);
    wp = work;
    cwp = plist;
    do {
	*wp++ = *cwp++;
    } while (wp[-1].label != 0);
    while ( utl_enter(work,parm_cb,0) != SW_NEXT ) {;}
    return 0;
}
#endif
