/*	menus.c
 *	Copyright 1987-1995 ATARI Games Corporation.  Unauthorized reproduction,
 *	adaptation, distribution, performance or display of this computer
 *	program or the associated audiovisual work is strictly prohibited. 
 */
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: menus.c,v 1.13 1997/07/01 22:40:48 albaugh Exp $";
#endif
/*
 *		Menu-driven option setting
 *	This stuff is used by the coin-option and game-option screens
 *	of self-test. The origianl menus.c was created by splitting
 *	out routines from stats.c (11-AUG-1992 MEA) This latest major
 *	re-write was done in SEPT/OCT 1995, also by MEA. It is a bit larger
 *	but includes "hooks" to allow odd-ball controls (such as the
 *	Area51 gun) to be used more effectively.
 */


#include <config.h>
#include <eer_defs.h>
#include <string.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#define	STATIC	static

/*	Some definitions for default placement on screen
 */
#define TOP_MARGIN (3)
#ifndef MENU_HDR_COL
#define MENU_HDR_COL (4)
#endif

#define MENU_OPTION_OFFSET (2)

/* Forward declarations */
STATIC const U8 * findopt
PARMS((const U8 *menu, int field));
STATIC int scan_menu( const U8 *menu, struct opt_menu_state *mnp );
STATIC int show_menu(
struct opt_menu_state *mnp,
int action );

/*		st_dispmenu()
 * A simplified, publish-able entry to this rats-nest, to allow
 * other bits of selftest (notably the Zoid test routines) to
 * display "packed" registers a bit more legibly.
 */
void st_dispmenu(const U8 *menu, U32 opt_bits, int erase) {
    struct opt_menu_state mns;

    if ( erase ) erase = M_ACT_ERASE_ALL;
    else erase = M_ACT_REDRAW;
    if ( scan_menu( menu, &mns ) > 0 ) {
	mns.bits = opt_bits;
	show_menu( &mns, erase );
    }
}

/*		st_optmenu()
 *	New, improved, (I would _hope_ MEA learned _something_ in the
 *	last 13 years) menu-driven option-setting code. Rather than
 *	a lot of special hacks to implements, e.g. "reset to original",
 *	this version has a "callback hook".
 */
#ifndef BAIL_TIME
#define BAIL_TIME (VCR_HOLD_TIME*10)
#endif
STATIC const char * const whats_wrong[] = {
    "Unknown problem with menu",
    "Zero-bit option",
    "Option over-runs U32",
    "Two options modify same bits",
    "Menu not terminated",
    "No bits modified by menu"
};

STATIC void complain(int row, int problem)
{
    int bail_time;
    if (  (problem < 0)
	|| (problem >= n_elts(whats_wrong)) ) problem = 0;
	
    ctl_read_sw(SW_NEXT);	/* kill latent edge */
    txt_str(-1,row,whats_wrong[problem],ERROR_PAL);
    st_insn(row+2, t_msg_ret_menu, t_msg_next, ERROR_PAL);
    for ( bail_time = BAIL_TIME ; bail_time > 0 ; --bail_time ) {
	if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) break;
	prc_delay(1);
    }
}

/*		scan_menu()
 *	Reads a menu of the "Traditional" System I form, and splits
 *	it into items. Returns the number of items, or < zero if
 *	any of the the various "sanity checks" fail.
 */
STATIC int scan_menu( const U8 *menu, struct opt_menu_state *mnp ) {
    int len,lsb;
    U32 sofar,mask;
    const U8 *current;
    int nitems;

    sofar = 0;
    current = menu;

    mnp->select = mnp->top = 0;
    for ( nitems = 0 ; nitems < MAX_OPT_ITEMS ; ++nitems ) {
	/* Check which bits are used by this menu item.
	 * complain and bail if we find overlap.
	 */
	current = findopt(menu,nitems);
	if ( !current ) break;
	lsb = *current >> 3;
	len = *current & 7;
	if ( (lsb | len) == 0 ) {
	    /* normal end of menu is an item starting with '\000' */
	    mnp->items[nitems] = 0;
	    break;
	}
	if ( len == 0 ) {
	    /* Length of zero but non-zero lsb is error.
	     */
	    return -1;
	}
	if ( (len-1 + lsb) >= MAX_OPT_ITEMS ) {
	    /* We are asking for an option to go beyond limits
	     * of a U32.
	     */
	    return -2;
	}
	mask = (1<<len)-1;
	mask <<= lsb;
	if ( (sofar & mask) != 0 ) {
	    /* This field overlaps one previously encountered.
	     * Likewise complain and return un-changed option bits.
	     */
	    return -3;
	}
	sofar |= mask;
	mnp->items[nitems] = current;
	if ( current == 0 ) break;
    } /* end for ( each menu item ) */
    if ( current != 0 ) {
	/* Menu runs on after all 32 bits have been accounted for.
	 */
	return -4;
    } /* endif ( menu ill-terminated ) */
    if ( sofar == 0 ) {
	/* No bits actually specified...
	 */
	return -5;
    } /* endif ( menu ill-terminated ) */
    /* Fill rest of "items" array with null pointers
     * to simplify other users.
     */
    for ( len = nitems ; len < MAX_OPT_ITEMS ; ++len ) {
	mnp->items[len] = 0;
    }
    mnp->n_items = nitems;
    return nitems;
} 

/*		show_menu()
 *	Display a series of menu items, given a opt_menu_state struct
 *	which contains screen limits.
 *	returns updated row number.
 */
STATIC int show_menu(
struct opt_menu_state *mnp,
int action )
{
    int item,nitems;
    int lsb,len;
    U32 mask;
    int width;
    U8 tbuf[AN_VIS_COL_MAX];
    const U8 *txt;

    int row, bottom, opt_col;

    nitems = mnp->n_items;
    row = mnp->margin_t;
    bottom = mnp->margin_b;
    opt_col = mnp->margin_l+MENU_OPTION_OFFSET;

    if ( mnp->top ) {
	/* We have extra items above those shown on screen.
	 * Say so.
	 */
	if ( (action & ~M_ACT_ERASE) != M_ACT_SELECTED ) {
	    if ( action & M_ACT_ERASE ) {
		txt_clr_str(mnp->margin_l+3,row,
		  "MORE ABOVE",MNORMAL_PAL);
	    } else {
		txt_str(mnp->margin_l+3,row,
		  "MORE ABOVE",MNORMAL_PAL);
	    }
	}
	row += 2;
    }

    for ( item = mnp->top ; item < nitems ; ++item ) {
	int fld;
	int palette = MNORMAL_PAL;

	if ( (row+3) > bottom ) {
	    /* adding this item would over-run the
	     * bottom of the screen. We make a special
	     * exception for the very last item, but
	     * otherwise bail now.
	     */
	    if ( item > (nitems-1) || (row+2) > bottom ) break;
	}
	if ( ((action & ~M_ACT_ERASE) == M_ACT_SELECTED)
	   && (item != mnp->select) ) {
	    row += 3;
	    continue;
	}
	/* get field description from first byte of item,
	 * split into lsb and length, and skip possible '?'
	 */
	txt = mnp->items[item];
	lsb = *txt++;
	len = lsb&7;
	lsb >>= 3;
	if ( *txt == '?' ) ++txt;

	/* Limit string length so some doofus doesn't crash
	 * some over-delicate hardware.
	 */
	for ( width = 0 ; width < (mnp->margin_r-mnp->margin_l) ; ++width ) {
	    if ( (tbuf[width] = txt[width]) == '\0' ) break;
	}
	tbuf[width] = '\0';

	/* Erase or write menu item header.
	 */
	if ( item == mnp->select ) palette = MHILITE_PAL;
	if ( action & M_ACT_ERASE ) {
	    txt_clr_str(mnp->margin_l,row++,(char *)tbuf,palette);
	} else {
	    txt_str(mnp->margin_l,row++,(char *)tbuf,palette);
	}

	/* Get the current value for this item
	 */
	mask = (1L<<len)-1;
	mask &= (mnp->bits >> lsb);
	txt += width;		/* point to end of displayable header */
	while ( *txt++ ) {;}	/* Skip over rest of header, if any */
	for ( fld = 0 ; fld < mask ; ++fld ) {
	    /* count zero-terminated strings after header to get to the
	     * correct label for this field's value.
	     */
	    while ( *txt++ ) {;}
	}
	if ( *txt == '*' ) {
	    /* This is factory default for this item.
	     */
	    ++txt;
	    palette = GRN_PAL;
	    if ( mnp->select == item ) palette = GRN_PALB;
	}
	/* More-or-less same length protection and output for item
	 * value.
	 */
	for ( width = 0 ; width < (mnp->margin_r-opt_col) ; ++width ) {
	    if ( (tbuf[width] = txt[width]) == '\0' ) break;
	}
	tbuf[width] = '\0';
	if ( action & M_ACT_ERASE ) {
	    txt_clr_str(opt_col,row++,(char *)tbuf,palette);
	} else {
	    txt_str(opt_col,row++,(char *)tbuf,palette);
	}
	++row;	/* one blank line between items */
    }
    if ( item < mnp->n_items && (action & ~M_ACT_ERASE) != M_ACT_SELECTED ) {
	/* Could not fit all, say "MORE BELOW"
	 */
	if ( action & M_ACT_ERASE ) {
	    txt_clr_str(mnp->margin_l+3,row,"MORE BELOW",MNORMAL_PAL);
	} else {
	    txt_str(mnp->margin_l+3,row,"MORE BELOW",MNORMAL_PAL);
	}
    }
    return row;
}

/*		fit_menu()
 *	Calculate how many menu items will fit into a given
 *	vertical space. The "top" field of the referenced menu
 *	may be modified by this routine. The basic layout
 *	is two lines of "MORE ABOVE" and a blank, if top_item
 *	is (or becomes) non-zero, followed by one or more
 *	instances of (header,value,leading), which take three
 *	lines, followed by one line of "MORE BELOW" if applicable.
 *	If the selected item will not fit, "top" is incremented
 *	and we re-try. Returns number of items we can display.
 */

STATIC int fit_menu(
struct opt_menu_state *mnp
) {
    int can_fit;
    int lines_used;
    int space;		/* Number of lines available for display */

    space = mnp->margin_b - mnp->margin_t;

    can_fit = mnp->n_items-mnp->top;	/* Assume the best */
    
    while (1) {
	if ( can_fit <= 0 ) return 0;
	if ( mnp->select >= (can_fit+mnp->top) ) {
	    /* our selected item fell off the
	     * bottom, move the top down
	     * and re-calc optimistic guess.
	     */
	    mnp->top += 1;
	    can_fit = mnp->n_items-mnp->top;
	}
	/* Basic menu requires 2 lines per item,
	 * plus one line of leading for all but last.
	 */
	lines_used = ((can_fit-1)*3)+2;
	/* If the top item is not the first
	 * item in the menu, add 2 lines for
	 * "MORE ABOVE" and its leading.
	 */
	if ( mnp->top ) lines_used += 2;
	/* If not all items (below the top)
	 * fit on screen, and two lines for
	 * "MORE BELOW" and its leading.
	 */
	if ( can_fit < (mnp->n_items - mnp->top) ) {
	    lines_used += 2;
	}
	/* We win if it all fits.
	 */
	if ( lines_used <= space ) return can_fit;
	/* Otherwise we need to trim an item
	 * and re-try.
	 */
	--can_fit;
    }
}

#if J_LEFT && J_UP && (GUN_CNT == 0)
STATIC int joy_mdriver( struct opt_menu_state *cur)
{
    U32 ctls;
    U32 opt_bits;
    int nitems = cur->n_items;
    opt_bits = cur->bits;

    ctls = ctl_read_sw(JOY_BITS);

    if ( (ctls & JOY_BITS) == 0 ) return M_ACT_NONE;
    if ( (ctls & J_UP) && (cur->select > 0) ) {
	/* move up in menu, possibly scrolling.
	 */
	if ( --cur->select < cur->top ) cur->top = cur->select;
	return M_ACT_REDRAW;
    }
    if ( (ctls & J_DOWN) && (cur->select < (nitems-1)) ) {
	/* move down in menu.
	 */
	++cur->select;
	return M_ACT_REDRAW;
    }
    if ( ctls & J_LEFT ) return M_ACT_DECVAL;
    if ( ctls & J_RIGHT ) return M_ACT_INCVAL;
    return M_ACT_REDRAW;
}
#endif /* J_LEFT && J_UP which is to say, do we have a joystick, and no GUN */

#if (!J_LEFT || !J_UP) && (GUN_CNT == 0)
/* Modified "two button" interface originally done for "Genocide".
 * SW_NEXT selects menu item, LEFT/RIGHT (or SW_ACTION) diddle values,
 * _holding_ SW_NEXT exits (done in common code, st_optmenu())
 */
STATIC int joy_mdriver( struct opt_menu_state *cur)
{
    U32 ctls;
    U32 opt_bits;
    int nitems = cur->n_items;
    int retval = M_ACT_NONE;

    opt_bits = cur->bits;

#if J_UP
    ctls = ctl_read_sw(JOY_VERT);

    if ( (ctls & J_UP) && (cur->select > 0) ) {
	/* move up in menu, possibly scrolling.
	 */
	if ( --cur->select < cur->top ) cur->top = cur->select;
	return M_ACT_REDRAW;
    }
    if ( (ctls & J_DOWN) && (cur->select < (nitems-1)) ) {
	/* move down in menu.
	 */
	++cur->select;
	return M_ACT_REDRAW;
    }
#else
    ctls = ctl_read_sw(SW_NEXT);
    if ( ctls & SW_NEXT ) {
	/* Move down in menu, possibly scrolling. At bottom,
	 * return to top.
	 */
	++cur->select;
	if ( cur->select >= nitems ) cur->select = cur->top = 0;
	return M_ACT_REDRAW;
    }
#endif
#if J_LEFT
    ctls = ctl_read_sw(J_LEFT|J_RIGHT);
    if ( ctls & J_LEFT ) return M_ACT_DECVAL;
    if ( ctls & J_RIGHT ) return M_ACT_INCVAL;
#else
    if ( ctl_read_sw(SW_ACTION) & SW_ACTION ) return M_ACT_INCVAL;
#endif
    return retval;
}
#endif /* J_LEFT && J_UP which is to say, do we have a joystick, and no GUN */

#if GUN_CNT
extern int gun_mdriver(struct opt_menu_state *cur);
#endif

int (*st_opt_callback())(struct opt_menu_state *mnp)
{
#if GUN_CNT
    return gun_mdriver;
#else
    return joy_mdriver;
#endif
}

#define EX_CNT (40)
#define RST_CNT (40)

U32 st_optmenu(
const U8 *menu,
U32 opt_bits,
int top,
int bottom,
int (*callback)( struct opt_menu_state *mnp),
void *cb_vars
){
    struct opt_menu_state prev,cur;
    int nitems = 0;			/* Number of menu items */
    int redraw;				/* non-zero to re-draw menu */
    int fld_val;			/* value of current selected option field */
    U32 ctls;
#if !J_UP
    int ex_cnt = EX_CNT;		/* Timer for "Press and hold SW_NEXT" */
#endif
#if !J_LEFT
    int rst_cnt = RST_CNT;		/* Timer for "Press and hold SW_ACTION" */
#endif
    /* we need at least six lines to do anything: 
     * 2 for "More Above" and a blank line
     * 3 for a single menu item and blank line
     * 1 for "More Below"
     *
     * A menu consisting of a single item could be done
     * in two lines, but I'm not going to lose a lot of
     * sleep over it. (MEA 2OCT95)
     */
    if ( (bottom - top) < 6 ) return opt_bits;
    prev.cb_vars = cb_vars;
    cur.cb_vars = cb_vars;
    prev.n_items = 0;
    prev.top = 0;
    cur.top = 0;
    /* Establish margins for menu display. The control callback
     * can modify these (E.g. Gun-control might need wider
     * margins) but should probably not make the drawing area
     * larger than our caller asked for.
     */
    prev.margin_t = cur.margin_t = top;
    prev.margin_b = cur.margin_b = bottom;
    prev.margin_l = cur.margin_l = MENU_HDR_COL;
    prev.margin_r = cur.margin_r = AN_VIS_COL-2;
    prev.select = 0;
    cur.select = 0;
    cur.bits = cur.orig_bits = opt_bits;
    cur.warn_bits = 0;
    redraw = M_ACT_NONE;

    if ( !callback ) callback = st_opt_callback();

    while ( 1 ) {
	if ( nitems == 0 ) {
	    /* Either this is the first time we have seen the menu,
	     * or the menu has possibly changed (future enhancement)
	     * Build a list of pointers to the individual menu items.
	     */
	    nitems = scan_menu(menu, &cur);
	    if ( nitems < 0 ) {
		/* Some problem with menu format. Best we
		 * can do is complain and return unchanged
		 * option bits.
		 */
		txt_decnum(-1,2,(U32)(-nitems),10,LJ_NF,ERROR_PAL);
		complain(top,-nitems);
		return cur.orig_bits;
	    }
	    redraw = M_ACT_REDRAW;
	    fld_val = -1;		/* force re-calc of selected field */
	} /* endif ( we need to re-parse menu ) */

	/* Read and process controls */
	if ( callback && (redraw == M_ACT_NONE) ) {
	    redraw = callback( &cur );
	    opt_bits = cur.bits;
	}
	/* below is written as a "while" so we "fast forward"
	 * over any illegal options.
	 */
	while ( redraw == M_ACT_INCVAL || redraw == M_ACT_DECVAL ) {
	    /* change value of current field */
	    int len,lsb;
	    U32 mask,new;
	    const U8 *mp;
	    mp = cur.items[cur.select];
	    lsb = *mp >> 3;
	    len = *mp & 7;
	    mask = (1L<<len)-1;
	    /* get current value of this field, and increment
	     * or decrement it.
	     */
	    new = (opt_bits >> lsb)&mask;
	    if ( redraw == M_ACT_DECVAL ) --new;
	    else ++new;
	    /* Splice new value into opt_bits, so it will be correct
	     * when we break;
	     */
	    opt_bits &= ~(mask<<lsb);
	    opt_bits |= ((new&=mask)<<lsb);
	    cur.bits = opt_bits;
	    /* Scan for the corresponding label, to validate choice.
	     */
	    while (*++mp) {;}	/* Skip to '\000' before 1st label */
	    while (new) {
		while (*++mp) {;}	/* skip to '\0' before nth label */
		--new;
	    }
	    if ( mp[1] != '\0' ) redraw = M_ACT_SELECTED;
	}
	ctls = ctl_read_sw(SW_NEXT|SW_ACTION);

#if J_UP
	/* If we have a Joystick, _any_ closure on SW_NEXT
	 * will cause us to exit. Otherwise, only
	 * holding SW_NEXT will.
	 */
	if ( ctls & SW_NEXT ) redraw = M_ACT_EXIT;
	else
#else
	if ( ctl_read_sw(0) & SW_NEXT ) {
	    if ( --ex_cnt <= 0 ) redraw = M_ACT_EXIT;
	} else ex_cnt = EX_CNT;
#endif
#if J_LEFT
	/* If we have even LEFT/RIGHT, _any_ closure on SW_ACTION
	 * will cause us to reset to entry values. Otherwise,
	 * only _holding_ SW_ACTION will.
	 */
	if ( ctls & SW_ACTION ) {
	    cur.bits = opt_bits = cur.orig_bits;
	    redraw = M_ACT_REDRAW;
	}
#else
	if ( ctl_read_sw(0) & SW_ACTION ) {
	    if ( --rst_cnt <= 0 ) {
		cur.bits = opt_bits = cur.orig_bits;
		redraw = M_ACT_REDRAW;
	    }
	} else rst_cnt = RST_CNT;
#endif
	if ( redraw & M_ACT_EXIT ) break;
	if ( redraw || (cur.top != prev.top) ) {
	    /* Need to redraw whole menu */
	    if ( prev.n_items > 0 ) {
		/* there already was a menu, we need to erase it.
		 */
		fit_menu(&prev);
		show_menu( &prev, M_ACT_ERASE_ALL );
	    } /* endif ( need to erase ) */

	    fit_menu(&cur);
	    show_menu( &cur, M_ACT_REDRAW );
	    /* indicate we have already redrawn menu */
	    prev = cur;
	    redraw = M_ACT_NONE;
	} /* endif ( need to redraw whole menu) */
	if ( cur.bits != prev.bits ) {
	    /* need to re-draw one item */
	    show_menu(&prev, (M_ACT_SELECTED|M_ACT_ERASE));
	    show_menu(&cur, M_ACT_SELECTED);
	    prev.bits = cur.bits;
	}
	prc_delay0();
    } /* end while (1) */
    return cur.bits;
}


U32 DoOptions(
const U8 *menu,
U32 old_opts,
U32 swReset
){
    int bottom = AN_VIS_ROW-3;
#if J_UP
    bottom = st_insn(bottom,t_msg_save_ret, t_msg_next, INSTR_PAL);
#else
    bottom = st_insn(bottom,t_msg_save_ret, t_msg_nexth, INSTR_PAL);
#endif
#if J_LEFT
    bottom = st_insn(bottom,t_msg_restore, t_msg_action, INSTR_PAL);
#else
    bottom = st_insn(bottom,t_msg_restore, t_msg_actionh, INSTR_PAL);
#endif
    return st_optmenu( menu, old_opts, TOP_MARGIN, bottom-1,0,0);
}

/* :::::: FINDOPT scans through the given MENU STRUCTURE and :::::::	*/
/*	skips the appropriate number of fields				*/
/*	RETURNS 0 if no entry for that field				*/

STATIC const U8 *
findopt(menu,field)
const U8 *menu;
int field;
{
    m_int count;
    U8 c;
    const U8 *trail = menu;

    if ( field < 0 ) return 0;

    for ( ; field >= 0 ; --field)
    {
	trail = menu;
	if (*menu == 0) break;	/* zero length in header, end of menu	*/
	c = *menu++;
	count = 1 << ( c & 7 ); /* number of choices */
	if ( *menu == '?' ) {
	    /* this field should only be available during development
	     * and/or when enabled via "secret handshake"
	     */
	    ++menu;
#ifdef DEBUG
	    if ( (debug_mode & GUTS_OPT_DEVEL) == 0 )
#endif
		++field;
	}
	for ( ; count >= 0 ; --count )	     /* Skip title and choices*/
	    while (*menu++);
    }

    if (*trail == '\0')		/* zero length in header, end of menu	*/
	return(0);
    return(trail);
}

/*		factory_setting(menu)
 *	Computes and returns the "factory setting" as indicated
 *	by the selection (in each menu field) starting with a '*'
 */
U32
factory_setting(menu)
const U8 *menu;
{
    m_int count,shift,i;
    U32 options;

    options = 0;
    while ( *menu ) {
	shift = *menu >> 3;
	count = 1 << (*menu++ & 7);
	while ( *menu++ ) {;}
	for ( i = 0 ; i < count ; ++i ) {
	    if ( *menu == '*' ) options |= (i << shift);
	    while ( *menu++ ) {;}
	}
    }
    return options;
}

#if (0)
/*		dispopt(menu,opt_bits,y,color,erase)
*	Display one option selection given a menu string, formatted per below,
* the option word (and defaults), the position, and the color. An "erase flag"
* indicates that the the selection is to be erased, rather than written. If
* the current value matches the default value for this field (Indicated by a
* '*' in the menu,) the value string is output in palette 1 (HILITE).
*    This routine returns 0 if the end of the menu has been reached. It
*    normally returns the updated menu pointer (pointing to the next group of
*    option bits).
*	The menu string is encoded as follows. Each group of bits in the option
*    word is represented by a header byte whose bottom 3 bits indicate the
*    length of the field, and whose top 5 bits represent the bit number of the
*    lsb.
*    This is followed by the string to title the field (zero terminated),  which
*    is in turn followed by the strings corresponding to each possible value for
*    the field. The value string starting with an asterisk (*) is the factory
*    default. Any invalid setting is a null string (i.e. just a zero byte).
*    There must be a "string" (if only a zero) for each value, e.g. a three bit
*    field must have 8 "strings", although some (those corresponding to invalid
*    values) will be single zero bytes. The example menu string below encodes
*    bit D0 of the switch word as a field with two values and D2-D1 a one with
*    three values, the '10' code being invalid.
*
*  char *samplemenu =
*      "\001Single bit option:\0*No\0Yes\0\012Two bit:\0one\0two\0\0three\0\0\0"
*
*	Two things to note in this sample are the "012" which is the OCTAL
*    representation of the header byte (don't blame me, blame K&R), and
*    the fact that only two of the needed three terminating bytes are shown,
*    since C appends one zero-byte to the end of a quoted string. One other
*    caveat is to always use the full three digit form of the \000 if your
*    string starts with a digit. A slightly more legible example appears at
*    coinmenu, in coin91.c. By using the \<newline> (NOT \n) escape, the menu
*    can be made significantly more legible. An even more readable form can
*    be built with ANSI/ISO string concatenation, although I'm not absolutely
*    sure I can safely depend on its existance at this time (June, 1994. MEA)
*/

#endif
