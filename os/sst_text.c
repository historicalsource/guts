/* $Id: sst_text.c,v 1.38 1997/07/17 03:27:19 shepperd Exp $
 *
 *	Switchable method text-output routines for 3DFX
 *
 *	These routines were derived, in part, from the Jaguar text
 *	routines written by Robert Birmingham.
 *
 *		Copyright 1994,1995,1996 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

/* Stuff that needs fixing:
 *
 * Don't know yet.
 */

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>
#include <phx_proto.h>
#include <glide.h>
#include <wms_proto.h>

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#define BPPIXEL (2)	/* 2 bytes/pixel, but it is faster to write as single U32 */

#define FIFO_LIMIT (0xA004)

#ifndef AN_TOT_COL
# define AN_TOT_COL AN_VIS_COL
#endif
#ifndef CHAR_BITS
# define CHAR_BITS	8
#endif

#define SCREENW (VIS_H_PIX)
#define STATIC static
#define U32_BITS (sizeof(U32)*(CHAR_BITS))
#define DIRTY_U32_PER_ROW	((AN_VIS_COL_MAX+U32_BITS-1)/U32_BITS)

STATIC U16 fake_screen[AN_VIS_ROW_MAX][AN_VIS_COL_MAX];
STATIC U32 dirty_bits[AN_VIS_ROW_MAX*DIRTY_U32_PER_ROW];
static struct txt_alpha_ptr default_screen, *where_is_vs;

extern U32 sst_memfifo_min;

int (*vid_str)(int col, int row, const char *string, int font);
void (*vid_stamp)(int col, int row, int stampno, int pal_font);
void (*vid_tclr)(int col, int row, int width);
void (*vid_refresh)(int);

int (*dbg_str)(int col, int row, const char *string, int font);
void (*dbg_stamp)(int col, int row, int stampno, int pal_font);
void (*dbg_tclr)(int col, int row, int width);

/* txt_vsptr - set the pointer to the virtual screen
 * At entry:
 *	new - pointer to new TxtAlphaPtr struct to use or 0
 *		(if new->screen == 0, clears the current pointer)
 * At exit:
 *	returns the old contents of where_is_vs
 */
struct txt_alpha_ptr *txt_vsptr(struct txt_alpha_ptr *new) {
    struct txt_alpha_ptr *old;
    old = where_is_vs;			/* save previous value */
    if (new) {				/* if user supplied a value ...*/
	if (new->screen) {		/* and the screen member is not 0...*/
	    where_is_vs = new;		/* his value becomes the new one */
	} else {
	    where_is_vs = 0;		/* else there is no virtual screen anymore */
	}
    }					/* if new == 0, just return previous value */
    return old;				/* always return previous value */
}

/*	If the "hardware" does not define the bit-depth for alphanumerics,
 *	assume the fairly-standard two bits of System I.
 */
#ifndef AN_BIT_DEPTH
#define AN_BIT_DEPTH (2)
#endif
/*	If the "hardware" does not define the width for alphanumerics,
 *	assume the fairly-standard eight pixels of System I.
 */
#ifndef AN_STMP_WID
#define AN_STMP_WID (8)
#endif
/*	If the "hardware" does not define the height for alphanumerics,
 *	assume the fairly-standard eight lines of System I.
 */
#ifndef AN_STMP_HT
#define AN_STMP_HT (8)
#endif
static U16 an_pal[(AN_PAL_MSK>>AN_PAL_SHF)+1][1<<AN_BIT_DEPTH];

#define STAMPOUT_TYPE	0

#if STAMPOUT_TYPE == 0
#define stampout( scrptr, colors, scan) \
   do {					\
      U32 t; 				\
      t = colors[scan&3];	\
      scan >>= 2;			\
      *scrptr++ = t | (colors[scan&3]<<16);	\
      scan >>= 2;			\
      t = colors[scan&3];	\
      scan >>= 2;			\
      *scrptr++ = t | (colors[scan&3]<<16);	\
      scan >>= 2;			\
      t = colors[scan&3];	\
      scan >>= 2;			\
      *scrptr++ = t | (colors[scan&3]<<16);	\
      scan >>= 2;			\
      t = colors[scan&3];	\
      scan >>= 2;			\
      *scrptr++ = t | (colors[scan&3]<<16);	\
      scan >>= 2;			\
   } while (0)
#define stampout_nbg( scrptr, colors, scan)	\
   do {						\
      U16 *scp16 = (U16*)scrptr;		\
      int ii, jj;				\
      for (ii=0; ii < 8; ++ii, ++scp16, scan >>= 2) { \
	 jj = scan&3;				\
	 if (jj) *scp16 = colors[jj];		\
      }						\
      scrptr = (U32*)scp16;			\
   } while (0)
#endif

#if STAMPOUT_TYPE == 1
static int stmp_y;
static void stampout( U32 *scrptr, U16 *colors, int scan) {
    U32 t; 
    int jj;
    if (stmp_y == 0) {
	txt_hexnum(8+10+00, 7, colors[0], 4, RJ_ZF, WHT_PAL);
	txt_hexnum(8+10+10, 7, colors[1], 4, RJ_ZF, WHT_PAL);
	txt_hexnum(8+10+20, 7, colors[2], 4, RJ_ZF, WHT_PAL);
	txt_hexnum(8+10+30, 7, colors[3], 4, RJ_ZF, WHT_PAL);
    }
    txt_hexnum(8, 8+stmp_y, (U32)scrptr, 8, RJ_ZF, WHT_PAL);
    for (jj=0; jj < 4; ++jj) {
	t = colors[scan&3]<<16;
	scan >>= 2;
	t |= colors[scan&3];
	scan >>= 2;
	txt_hexnum(8+10+jj*10, 8+stmp_y, t, 8, RJ_ZF, WHT_PAL);
    }
    ++stmp_y;
    return;
} 
#endif

static struct posn {
    short row;
    short col;
} p_posn;

U32 txt_setpos(U32 cookie) {
    int oldcol, oldrow;
    oldcol = p_posn.col;
    oldrow = p_posn.row;
    if (cookie >= AN_VIS_ROW*AN_VIS_COL) {
	cookie -= AN_VIS_ROW*AN_VIS_COL;
	p_posn.row = cookie/AN_VIS_COL;
	p_posn.col = cookie%AN_VIS_COL;
    }
    return AN_VIS_ROW*AN_VIS_COL + oldrow*AN_VIS_COL + oldcol;
}

extern unsigned short an_stamps,an_norm_a,an_end;
#define AN_STAMPS &an_stamps
#define AN_A_OFFSET ((&an_norm_a-&an_stamps)>>3)
struct bigchar {
    unsigned char tl,bl,tr,br;
};

#include <txt_tab.h>

const int BIG_OFFSET = 0xA6;

void
txt_clr_wid(col, row, width)
int col, row, width;
{
    struct txt_alpha_ptr *which;
    VU32 *dirty;
    VU16 *screen;
    which = txt_vsptr(0);
    if ( which == 0 || which->screen == 0 ) return;
    if ( row < 0 ) {
	col = p_posn.col;	
	row = p_posn.row;
    }
    if ( row >= AN_VIS_ROW ) row = AN_VIS_ROW-1;
    if (col < 0 ) {
	/* wants to center */
	col = (AN_VIS_COL+1) >> 1;
	if ( col < 0 ) col = 0;
    }
    if ( col > (AN_VIS_COL-1) ) col = AN_VIS_COL-1;
    if ( (col+width) >= AN_VIS_COL ) {
	width = AN_VIS_COL-col;
    }
    dirty = which->dirty;
    if ( dirty ) {
	U32 dbits;
	int start,twid,skip;

	start = col;
	twid = width;
	dbits = 0;
	dirty += row*((AN_VIS_COL+U32_BITS-1)/U32_BITS);
	skip = (start/U32_BITS);
	dirty += skip;
	skip -= (skip*32);
	while ( twid > 0 ) {
	    dbits = (twid >= U32_BITS) ? -1U : ((1<<twid)-1);
	    *dirty &= ~(dbits<<(start&0x1F));
	    twid -= (32-(start&0x1F));
	    start = 0;
	    ++dirty;
	}
    } else {
	screen = (VU16 *) which->screen;
	screen += ((row*(AN_VIS_COL))+col);
	memset((void *)screen,0,width*sizeof(VU16));
    }
    if ( dbg_tclr ) dbg_tclr(col, row, width);
    if ( vid_tclr ) vid_tclr(col, row, width);
}

int
txt_cstr(string,psmask)
const char	*string;
int	psmask;
{
    return txt_str(0,-1,string,psmask);
}

void
txt_clr_str(x,y,string,pal)
int x,y,pal;
const char *string;
{
    int endy;
    int height = txt_height(string,pal);
    int width = txt_width(string,pal);
    if ( x < 0 ) {
	/* wants to center */
	x = (AN_VIS_COL+1-width) >> 1;
	if ( x < 0 ) {
	    x = 0;
	    width = AN_VIS_COL;
	}
    }
    /* We changed AN_BIG_SET to be bottom-justified,
     * but missed it here. Clearing should be on row
     * 'y' and 'height-1' lines above it. For compatibility
     * with older code, we make sure this does not "push"
     * AN_BIG_SET text above the top of the screen.
     */
    endy = y - (height - 1);
    if ( endy < 0 ) {
	y = height-1;
	endy = 0;
    }
    for (  ; y >= endy ; --y ) {
	txt_clr_wid(x,y,width);
    }
}

int
txt_width(text,set)
const char *text;
int set;
{
    int width = 0;
    int letter;
    const struct bigchar *bt = BTABLE;

    if ( (set & SETMSK) != AN_BIG_SET ) width = strlen(text);
    else while ( (letter = *text++) != '\0') {
	if ( ( letter = trantab[letter] ) < 0 ) ++width;
	else width += 1 + (bt[letter].tr != 0);
    }
    return width;
}

int txt_height(txt,set)
const char *txt;
int set;
{
    if ( (set & SETMSK) == AN_BIG_SET ) return 2;
    return 1;
}

int
txt_decnum(col,row,val,wid,form,palset)
int		col;	/* coord. of UL corner of first character of txt */
int 		row;	/* (0,0) is upper left corner of screen		*/
unsigned long	val;	/* value to output				*/
int		wid;	/* Size of output field				*/
int		form;	/* one of:
    			 * RJ_ZF (0) Right Justify with ZERO Fill
    			 * RJ_BF (1) Right Justify with BLANK Fill
    			 * LJ_BF (2) Left Justify with BLANK Fill
    			 * LJ_NF (3) Left Justify with NO Fill
    			 *						*/
int		palset;	/* Palette and Character Set selection		*/
{
    char num[20];
    utl_cdec(val,num,wid,form);
    return txt_str(col,row,num,palset);    
}

void
txt_cdecnum(val,wid,form,palset)
unsigned long	val;	/* value to output				*/
int		wid;	/* Size of output field				*/
int		form;	/* one of:
    			 * RJ_ZF (0) Right Justify with ZERO Fill
    			 * RJ_BF (1) Right Justify with BLANK Fill
    			 * LJ_BF (2) Left Justify with BLANK Fill
    			 * LJ_NF (3) Left Justify with NO Fill
    			 *						*/
int		palset;	/* Palette and Character Set selection		*/
{
    txt_decnum(0,-1,val,wid,form,palset);
}

int
txt_hexnum(col,row,val,wid,form,palset)
int		col;	/* coord. of UL corner of first character of txt */
int 		row;	/* (0,0) is upper left corner of screen		*/
unsigned long	val;	/* value to output				*/
int		wid;	/* Size of output field				*/
int		form;	/* one of:
    			 * RJ_ZF (0) Right Justify with ZERO Fill
    			 * RJ_BF (1) Right Justify with BLANK Fill
    			 * LJ_BF (2) Left Justify with BLANK Fill
    			 * LJ_NF (3) Left Justify with NO Fill
    			 *						*/
int		palset;	/* Palette and Character Set selection		*/
{
    char num[20];
    utl_chex(val,num,wid,form);
    return txt_str(col,row,num,palset);
}

void
txt_chexnum(val,wid,form,palset)
unsigned long	val;	/* value to output				*/
int		wid;	/* Size of output field				*/
int		form;	/* one of:
    			 * RJ_ZF (0) Right Justify with ZERO Fill
    			 * RJ_BF (1) Right Justify with BLANK Fill
    			 * LJ_BF (2) Left Justify with BLANK Fill
    			 * LJ_NF (3) Left Justify with NO Fill
    			 *						*/
int		palset;	/* Palette and Character Set selection		*/
{
    txt_hexnum(0,-1,val,wid,form,palset);
}

/*		SetANPal(f_and_b)
 *	Sets one AlphaNumeric palette to have the colors specified
 *	in f_and_b. The lower 16 bits of f_and_b specify the foreground
 *	color (in game's coding) while the upper 16 bits specify the
 *	background color. If we ever get more than 1 bit deep text on
 *	Jaguar, an alias color will be synthesized "halfway between"
 *	the two. We are not actually using the CLUT capability of the
 *	Jaguar for text, as we have a CRY bitmap to draw into and do
 *	not want to waste CLUT space on text.
 */

void
SetANPal( palette, colors)
int palette;
U32 colors;
{
    U32 fg, bg, anti_alias;

    bg = colors >> 16;
    fg = (colors & 0xFFFF);

    palette &= AN_PAL_MSK;
    palette >>= AN_PAL_SHF;
    anti_alias = (fg & ~(RED_LSB|GRN_LSB|BLU_LSB))>>1;
    anti_alias += (GRY_FUL & ~(RED_LSB|GRN_LSB|BLU_LSB))>>1;
    an_pal[palette][0] = bg;
    an_pal[palette][(1<<AN_BIT_DEPTH)-1] = fg;
#if AN_BIT_DEPTH > 1
#if (SST_GAME & SST_JUKO)
    an_pal[palette][1] = 0x421;		/* near-black Border on BIG alphas */;
#else
    an_pal[palette][1] = GRY_FUL;		/* Border on BIG alphas */;
#endif
    an_pal[palette][2] = anti_alias;
#endif
}

/*		txt_mod_pal( palette, mask, xfer )
 *	Hack added for Juko Thread. The idea is to allow the
 *	game to modify the text palettes, but without total anarchy.
 *	Copies those palette entries from xfer to the internal
 *	palette where there is a 'one' bit in mask. For example:
 *
 *	if my_pal[] is an array of U16's,
 *
 *	txt_mod_pal( RED_PAL, 0x2, my_pal);
 *
 *	will copy color 1 (0x2 == (1<<1)) to the palette selected
 *	by RED_PAL. Returns the number of colors in the palette.
 *	If mask is 0, copies _from_ the palette to *xfer, which
 *	_must_ be big enough to hold them. Since mask is a U32,
 *	a 32-entry array should suffice. For most platforms, using
 *	the System-I derived 2-bit deep character set:
 *	0 is the background
 *	1 is the border (for big chars) or Drop-shadow (for 8x8s)
 *	2 is the anti-alias color
 *	3 is the foreground
 */
int
txt_mod_pal( int palette, U32 mask, U16 *xfer)
{
    U16 *mine;
    int idx,limit;

    if ( palette < 0 || (palette >> AN_PAL_SHF) > (AN_PAL_MSK>>AN_PAL_SHF) ) {
	/* Palette out of range. */
	return -1;
    }
    palette &= AN_PAL_MSK;
    palette >>= AN_PAL_SHF;
    mine = an_pal[palette];
    limit = sizeof(an_pal[0])/sizeof(an_pal[0][0]);
    if ( mask ) {
	for ( idx = 0 ; idx < limit ; ++idx ) {
	    if ( mask & 1 ) mine[idx] = xfer[idx];
	    if ( (mask >>= 1) == 0 ) break;
	}
    } else {
	for ( idx = 0 ; idx < limit ; ++idx ) {
	    xfer[idx] = mine[idx];
	}
    }
    return limit;
}

/* 	While bringing up new hardware, we may want to output
 *	text to both the remote terminal and the screen. The
 *	set of pointers below is used for this purpose.
 */

static int def_font;

/*		txt_str( col, row, txtptr, color )
 *	Writes text at the specifed row and column, in
 *	the specified "color", which may also include
 *	font selection.
 */
int
txt_str(col,row,string,palette)
int	col,row;
const char	*string;
int	palette;
{
    struct posn *p = &p_posn;
    int width;
    int old_col = p->col;
    char tbuf[AN_VIS_COL_MAX+1];
    struct txt_alpha_ptr *which;
    VU16 *screen;
    VU32 *dirty;
    void (*do_stamp)(int col, int row, int stampno, int pal_font);

    width = txt_width(string,palette);
    if ( row < 0 ) {
	col = p->col;
	row = p->row;
    } else if ( col < 0 ) {
	col = (AN_VIS_COL+1 - width) >> 1;
	if ( col < 0 ) col = 0; 
    }
    old_col = p->col = col;
    p->row = row;
    /* Set up local pointer for "draw one stamp in video",
     * which we should always have. Then over-ride it if
     * we also have a (possibly more efficient) "output
     * whole string" routine.
     */
    do_stamp = vid_stamp;
    if ( vid_str ) do_stamp = 0;
    if ( (col + width) >= AN_VIS_COL ) {
	/* Nip over-long strings in the bud here, so
	 * we don't have to check in every vid_str() 
	 * routine.
	 * KLUGE: This will not work well for BIG character
	 * set, or for "width" expressed in anything but
	 * monospaced character cells. For now, we will
	 * ignore the problem, as this is a "proof of concept."
	 */
	int allow;
	if (col > AN_VIS_COL) col = AN_VIS_COL;
	allow = AN_VIS_COL-col;
	if (allow) memcpy(tbuf,string,allow);
	tbuf[allow] = '\0';
	string = tbuf;
	width = allow;
    }
    p->col = old_col + width;
    if ( (row < 0) || (row >= AN_VIS_ROW) ) return 0;
    if ( dbg_str ) dbg_str(col,row,string,palette);
    which = txt_vsptr(0);
    if ( !which ) return 0;
    if ( !vid_str ) txt_select(TXT_HOST);	/* default to host mode */
    screen = which->screen;
    if ( screen ) {
	/* Output stamps to fake screen _regardless_ of
	 * ultimate video output method. We do this here
	 * to avoid duplication of code, and consequent
	 * version drift, and because BIG characters take
	 * a bit of processing. Not to mention that we
	 * seem to have gone completely over to frame-buffered
	 * video wherein we _need_ a record of what text to
	 * repeatedly output.
	 */
	int stamp,idx;
	int color;
	color = palette & COLMSK;
	if ( (palette & SETMSK) == AN_BIG_SET ) {
	    /* Big characters are built from 2 or 4 stamps
	     * each.
	     */
	    int tc;		/* translated character */
	    VU16 *lp = (VU16*)screen;
	    const char *txtptr = string;
	    width = 0;
	    if ( row < 1 ) row = 1;	/* Strike a blow for bottom-justified */
	    lp += (row*AN_TOT_COL);
	    while ( (stamp = *txtptr) != '\0' ) {
		tc = trantab[stamp];
		idx = col+width;
		if ( idx >= AN_VIS_COL ) {
		    /* Trying to write past right edge of screen,
		     * because each ascii character may be more
		     * than one stamp wide.
		     */
		    break;
		}
		if ( tc < 0 ) {
		    /* NEW: Treat characters for which no
		     * BIG graphics exist as regular ascii
		     * in default font.
		     */
		    width += 1;
		    lp[idx] = (stamp+=(AN_A_OFFSET-'A'))|color;
		    lp[idx-AN_TOT_COL] = color;
		    if ( do_stamp ) do_stamp(idx,row,stamp,color);
		    ++txtptr;
		    continue;
		}
		stamp = BTABLE[tc].tl + BIG_OFFSET;
		lp[idx-AN_TOT_COL] = stamp | color;
		if ( do_stamp ) do_stamp(idx,row-1,stamp,color);
		stamp = BTABLE[tc].bl + BIG_OFFSET;
		lp[idx] = stamp | color;
		if ( do_stamp ) do_stamp(idx,row,stamp,color);
		++width;
		++txtptr;
		if ( BTABLE[tc].tr == 0 ) continue;
		++idx;
		stamp = BTABLE[tc].tr + BIG_OFFSET;
		lp[idx-AN_TOT_COL] = stamp | color;
		if ( do_stamp ) do_stamp(idx,row-1,stamp,color);
		stamp = BTABLE[tc].br + BIG_OFFSET;
		lp[idx] = stamp | color;
		if ( do_stamp ) do_stamp(idx,row,stamp,color);
		++width;
	    }
	} else {
	    /* Non-Big characters... */
	    int offset = AN_A_OFFSET-'A';
	    const char *txtptr = string;
	    VU16 *lp = (VU16 *)which->screen;
	    lp += (row*AN_TOT_COL);
	    idx = col;
	    while ( (stamp = *txtptr++) != '\0' ) {
		if ( stamp == ' ') stamp = 0;
		else stamp += offset;
		if ( do_stamp ) do_stamp(idx,row,stamp,color);
		lp[idx++] = (stamp|color);
	    }
	}
	dirty = which->dirty;
	if ( dirty ) {
	    U32 dbits;
	    int start,twid,skip;
	    
	    start = col;
	    twid = width;
	    dbits = 0;
	    dirty += row*((AN_VIS_COL+U32_BITS-1)/U32_BITS);
	    skip = (start/U32_BITS);
	    dirty += skip;
	    skip -= (skip*32);
	    while ( twid > 0 ) {
		dbits = (twid >= U32_BITS) ? -1U : ((1<<twid)-1);
		dbits <<= (start&0x1F);
		*dirty |= dbits;
		if ( (palette & SETMSK) == AN_BIG_SET ) {
		    /* Need to "dirty" the row above, too. */
		    dirty[-((AN_VIS_COL+U32_BITS-1)/U32_BITS)] |= dbits;
		}
		twid -= (32-(start&0x1F));
		start = 0;
		++dirty;
	    }
	}
    }
    /* If we have a routine for doing a whole string at once, we
     * have not been doing the stamps. Do the whole string now.
     */
    if ( vid_str ) {
	width = vid_str(col,row,string,palette);
    }
    return width;
}

/*	Routine:	txt_stamp(x,y,letter,psmask)
*	Inputs:		int	x,y
*			int	letter,psmask
*	Purpose:	To output a single character to the apha screen. Ignores
*			character-set translation, so psmask must only be color
*/
void
txt_stamp(x,y,letter,psmask)
int	x,y;
int	letter,psmask;
{
    struct txt_alpha_ptr *which;
    VU16 *lp;
    VU32 *dirty;
    which = txt_vsptr(0);
    if ( which == 0 || which->screen == 0 ) return;
    if ( y < 0 ) {
	y = p_posn.col;	
	x = p_posn.row;
    }
    if ( y >= AN_VIS_ROW ) y = AN_VIS_ROW-1;
    if (x < 0 ) {
	/* wants to center */
	x = (AN_VIS_COL+1) >> 1;
	if ( x < 0 ) x = 0;
    }
    if ( x >= AN_VIS_COL ) x = AN_VIS_COL-1;
    psmask &= COLMSK;
    lp = (VU16 *)which->screen;
    lp += (y*AN_TOT_COL);
    lp[x] = letter|psmask;
    dirty = which->dirty;
    if ( dirty ) {
	/* We have a bit-array to maintain.
	 */
	dirty += (y*((AN_TOT_COL+(U32_BITS-1))/U32_BITS));
	dirty[x>>5] |= (1<<(x&0x1F));
    }
    if ( dbg_stamp ) dbg_stamp(x,y,letter,psmask);
    if ( vid_stamp ) vid_stamp(x,y,letter,psmask);
}

void st_font(font_num)
int font_num;
{
    def_font = font_num;
}

int vstxt_str(int col, int row, const char *txt, int palset);

#if DEVELOPMENT
#define pdesc(val) { #val, val }
static const struct pt {
    const char * const name;
    U32 palette;
} pts[] = {
    pdesc(GRY_PAL),
    pdesc(BLU_PAL),
    pdesc(GRN_PAL),
    pdesc(CYN_PAL),
    pdesc(RED_PAL),
    pdesc(VIO_PAL),
    pdesc(YEL_PAL),
    pdesc(WHT_PAL),
    pdesc(GRY_PALB),
    pdesc(BLU_PALB),
    pdesc(GRN_PALB),
    pdesc(CYN_PALB),
    pdesc(RED_PALB),
    pdesc(VIO_PALB),
    pdesc(YEL_PALB),
    pdesc(WHT_PALB),
    pdesc(WHT_PALB|AN_BIG_SET)
};

int st_text_group( const struct menu_d *smp ) {

    int row = 4;
    int strnum;
    int col;
    setancolors();

    row = 9;
    for ( strnum = 0 ; strnum < n_elts(pts) ; ++strnum ) {
	txt_str(3,row,pts[strnum].name,pts[strnum].palette);
	txt_chexnum(pts[strnum].palette,8,RJ_ZF,MNORMAL_PAL);
	row += txt_height(pts[strnum].name,pts[strnum].palette);
    }
    ++row;
    for ( col = 1 ; col < (AN_VIS_COL-2) ; ++col ) {
	txt_stamp(col,row,col+AN_A_STMP-1,MNORMAL_PAL);
    }
    while ( 1 ) {
	if ( ctl_read_sw(SW_NEXT) & SW_NEXT ) break;
	if ( ctl_read_sw(SW_ACTION) & SW_ACTION ) sst_bufswap();
	txt_hexnum(-1,2,eer_rtc&0xFF,3,RJ_ZF,MNORMAL_PAL);
	txt_hexnum(-1,3,(*(U32 *)SST_BASE>>10)&3,1,RJ_ZF,MNORMAL_PAL);
	prc_delay(10);
    }
    return 0;
}

#define XP (0x80000000)
#define RED_GRN ((RED_MSK<<16)|GRN_MSK)
const U32 st_text_palettes[] = {
    BLK_SLT, BLK_BLU, BLK_GRN, BLK_CYN,			/* 0x00 */
    BLK_RED, BLK_VIO, BLK_YEL, BLK_WHT,			/* 0x04 */
    GRY_SLT, GRY_BLU, GRY_GRN, GRY_CYN,			/* 0x08 */
    GRY_RED, GRY_VIO, GRY_YEL, GRY_WHT,			/* 0x0C */
    BLK_SLT|XP, BLK_BLU|XP, BLK_GRN|XP, BLK_CYN|XP,	/* 0x10 */
    BLK_RED|XP, BLK_VIO|XP, BLK_YEL|XP, BLK_WHT|XP,	/* 0x14 */
    RED_GRN|XP						/* 0x18 */
};
#define RG_PAL (0x18)

const char * const z20_pmsg[] = {
    "BLK_SLT", "BLK_BLU", "BLK_GRN", "BLK_CYN",
    "BLK_RED", "BLK_VIO", "BLK_YEL", "BLK_WHT",
    "GRY_SLT", "GRY_BLU", "GRY_GRN", "GRY_CYN",
    "GRY_RED", "GRY_VIO", "GRY_YEL", "GRY_WHT",
    "BLK_SLT|XP", "BLK_BLU|XP", "BLK_GRN|XP", "BLK_CYN|XP",
    "BLK_RED|XP", "BLK_VIO|XP", "BLK_YEL|XP", "BLK_WHT|XP",
    "RED_GRN|XP"
};

const int z20_nelts_st_text_palettes = n_elts(st_text_palettes);
#endif /* DEVELOPMENT */

int sst_text_init( unsigned int tex_offs, unsigned int col_offs)
{
    struct txt_alpha_ptr *which;

    which = txt_vsptr(0);
    if ( which == 0 || which->screen == 0 ) {
	/* as good a time as any... */
	which = &default_screen;
	which->screen = &fake_screen[0][0];
	which->dirty = dirty_bits;
	which->texture_offset = tex_offs;
	which->color_offset = col_offs;
	txt_vsptr(which);
    }

    return 0;
}

#define ASCII_OFFSET (((&an_norm_a-&an_stamps)>>3)-'A')

void sst_text2fb_ptr(VU32 *scrptr, int flags) {
    VU16 *colptr, *vs, stamp;
    struct txt_alpha_ptr *which;
    int	row, col;
    U32 db;
    U32 *dp, *prev, dirty;

    which = txt_vsptr(0);
    if ( which == 0 || (vs=which->screen) == 0 ) return;

    for ( row = 0 ; row < AN_VIS_ROW ; ++row ) {
	U32 x, y, *pixelptr;
	/* convert cell row into screen y offset bytes */
	y = (VIS_V_PIX-8 - (U32)(row << 3)) * TOT_H_PIX * BPPIXEL;
	prev = dp = which->dirty + row*DIRTY_U32_PER_ROW;
	dirty = 0;
	db = 0;
	for ( col = 0 ; col < AN_VIS_COL ; ++col, db <<= 1, ++vs) {
	    if (!db) {
		prev = dp;
		if ((dirty = *dp++) == 0) {
		    col += 32-1;
		    if (col > AN_VIS_COL-1) {
			vs += (AN_VIS_COL&31)-1;
		    } else {
			vs += 32-1;
		    }
		    continue;
		}
		db = 1;
	    }
	    if (!(dirty&db)) continue;
	    if ((stamp = *vs) != 0 ) {
		U16 *stamps;
		int subrow, bg;

		if ((flags&T2FB_FLAG_FIFO)) {
		    bg = (*(VU32*)SST_BASE>>12)&0xFFFF;
		    if (bg < sst_memfifo_min) sst_memfifo_min = bg;
		    if (bg < FIFO_LIMIT) while ( ((*(VU32*)SST_BASE>>12)&0xFFFF) < FIFO_LIMIT ) { ; } /* wait for 3dfx board */
		}

		if ((flags&T2FB_FLAG_CLEAR)) {
		    *prev &= ~db;
		}
		/* convert cell column into screen x offset in bytes */
		x = (U32)(col << 3) * BPPIXEL;
		/* calculate the starting screen memory address for the text */
		pixelptr = (U32*)((U32)scrptr + x + y);
#ifdef DRSHBIT
		colptr = an_pal[(stamp & (AN_PAL_MSK-DRSHBIT)) >> AN_PAL_SHF];
		bg = stamp&(BGBIT|DRSHBIT);
		stamp = stamp&((1<<AN_PAL_SHF)-1);
		stamps = AN_STAMPS+(stamp<<3)+7;
		if ( bg & DRSHBIT ) {
		    /* stamps with drop-shadows have to be
		     * painted top-to-bottom.
		     */
		    unsigned int above;		/* "remember" pixels above */
		    unsigned int curr;		/* Current line */
		    unsigned int mask;
		    stamps -= (AN_STMP_HT-1);
		    above = 0;
		    pixelptr += (TOT_H_PIX*AN_STMP_HT*BPPIXEL)/sizeof(U32);
		    bg &= ~DRSHBIT;
		    for ( subrow = AN_STMP_HT-1 ; subrow >= 0 ; --subrow ) {
			unsigned int scan;
			curr = *stamps++;
			/* Form mask with ones where the current line
			 * has non-background. "01" is code for "outline"
			 * color of font.
			 */
			mask = (curr | (curr>>1)) & 0x5555;

			/* We will "inherit" Similar mask for line above.
			 * combine the two to form word with "01" wherever
			 * a non-zero pixel is above-left, or left
			 * of the pixel "marked".
			 */
			above = ((above << 2)|(mask << 2));

			/* set possibly-shadowed pixels to outline color */
			scan = (above & ~mask);
			above = mask;	/* "inherit" for next time */
			mask |= (mask << 1);
			scan |= (mask & curr);
			if (bg) stampout(pixelptr, colptr, scan);
			else stampout_nbg(pixelptr, colptr, scan); 
			pixelptr += (-TOT_H_PIX-AN_STMP_WID)/2;
		    }
		    continue;
		}
		/* If we did not have a drop-shadow, we did not
		 * hit the "continue" above, so we draw a regular
		 * stamp.
		 */
#else
		colptr = an_pal[(stamp & AN_PAL_MSK) >> AN_PAL_SHF];
		bg = stamp&BGBIT;
		stamp = stamp&((1<<AN_PAL_SHF)-1);
		stamps = AN_STAMPS+(stamp<<3)+7;
#endif
		if (bg) {
		    for ( subrow = AN_STMP_HT-1 ; subrow >= 0 ; --subrow ) {
			U16 scan;
			scan = *stamps--;
			stampout(pixelptr, colptr, scan);
			pixelptr += (TOT_H_PIX-AN_STMP_WID)/2;  /* update dest. ptr into screen memory */
		    }
		} else {
		    for ( subrow = AN_STMP_HT-1 ; subrow >= 0 ; --subrow ) {
			U16 scan;
			scan = *stamps--;
			stampout_nbg(pixelptr, colptr, scan);
			pixelptr += (TOT_H_PIX-AN_STMP_WID)/2;  /* update dest. ptr into screen memory */
		    }
		}
	    }
	}
    }
}

/*		sst_text2fb()
 *	scans a "fake screen" and writes text to frame buffer.
 */

void sst_text2fb(int flag) {
#if GLIDE_VERSION >= 203
    GrState grstate;
#else
    GrLfbWriteMode_t oldmode;
#endif

#if GLIDE_VERSION >= 203
    grGlideGetState(&grstate);
    grDisableAllEffects();
#endif

    grLfbBegin();
    grLfbBypassMode(GR_LFBBYPASS_DISABLE);
#if GLIDE_VERSION < 203
    oldmode =
#endif
        grLfbWriteMode(GR_LFBWRITEMODE_555);
    grLfbConstantAlpha( 0xFF );
    grLfbConstantDepth( GR_WDEPTHVALUE_NEAREST );
    grAlphaTestFunction( GR_CMP_ALWAYS );

    sst_text2fb_ptr(grLfbGetWritePtr( GR_BUFFER_BACKBUFFER ), flag|T2FB_FLAG_FIFO);

    grLfbBypassMode(GR_LFBBYPASS_ENABLE);
    grLfbEnd();
#if GLIDE_VERSION >= 203
    grGlideSetState(&grstate);
#else
    grLfbWriteMode(oldmode);
#endif
}

void sst_tclr(int col, int row, int wid) {
    VU32 *dirty;
    VU16 *vs;
    struct txt_alpha_ptr *which;

    which = txt_vsptr(0);
    if ( which == 0 || (vs=which->screen) == 0 ) return;
    dirty = which->dirty;

    if (row >= AN_VIS_ROW) return;
    if (col >= AN_VIS_COL) return;
    if (col+wid > AN_VIS_COL) wid = AN_VIS_COL-col;
    if (!dirty) {
	memset((char *)vs+row*AN_VIS_COL, 0, wid);
    } else {
	U32 dbits;
	int start,twid,skip;

	start = col;
	twid = wid;
	dbits = 0;
	dirty += row*((AN_VIS_COL+U32_BITS-1)/U32_BITS);
	skip = (start/U32_BITS);
	dirty += skip;
	skip -= (skip*32);
	while ( twid > 0 ) {
	    dbits = (twid >= U32_BITS) ? -1U : ((1<<twid)-1);
	    *dirty &= ~(dbits<<(start&0x1F));
	    twid -= (32-(start&0x1F));
	    start = 0;
	    ++dirty;
	}
    }
}

#ifndef AN_STMP_CNT
#ifndef CHAR_BIT
#define CHAR_BIT (8)
#endif

#define AN_STMP_CNT \
 (((&an_end-&an_stamps)*(CHAR_BIT*sizeof(an_stamps)))/(AN_STMP_WID*AN_STMP_HT*AN_BIT_DEPTH))
#endif

#define	AN_COL_CNT	16
#define	AN_COL_SEP 	2
#define	AN_LFT_COL		((AN_VIS_COL - (AN_COL_CNT * AN_COL_SEP))/2)

#define	AN_ROW_CNT	10
#define	AN_ROW_SEP 	2
#define	AN_TOP_ROW	((AN_VIS_ROW - (AN_ROW_CNT * AN_ROW_SEP))/2)
#define AN_STMP_ROW_CNT ((AN_STMP_CNT+(AN_COL_CNT-1))/AN_COL_CNT)
#define	AN_MAX_ROWS	(AN_STMP_ROW_CNT - AN_ROW_CNT)

#if DEVELOPMENT
int
AN_stamp_test(smp)
const struct menu_d *smp;
{
    U32	edges;
    m_int	i,j,update;
    m_uint	stamp,textPal;
    m_int	baserow;
    m_int	addrow;
    int		bottom;
    struct txt_alpha_ptr *which;

    vid_clear();
    setancolors();

    SetANPal(MNORMAL_PAL,GRY_WHT);

    bottom = st_frame(smp,TITLE_PAL,INSTR_PAL,0);
    bottom = st_insn(bottom,"To see more characters,",t_msg_control,INSTR_PAL);

    which = txt_vsptr(0);
    
    for (i=0; i < AN_COL_CNT; i++)		/* display col #'s	*/
	txt_hexnum(AN_LFT_COL + (i * AN_COL_SEP),AN_TOP_ROW - 2,i,1,RJ_ZF,
	MNORMAL_PAL);
    
    baserow = 0;
    
    ctl_autorepeat(JOY_VERT,30,15);	/* Autorepeat after 1/2secs @ 1/4 */

    update = 1;

    textPal = MNORMAL_PAL;
    while (1)
    {
	prc_delay0();
	edges = ctl_read_sw(JOY_ALL|SW_EXTRA);	/* get edges	*/

	if (edges & SW_NEXT)
	    break;			/* done with this test...	*/

	if (edges & SW_ACTION)
	{
	    textPal ^= BGBIT;
	    if ( (textPal & BGBIT) == 0 ) textPal += AN_NXT_PAL; 
	    update = 1;
	}

	if (ctl_read_sw(0) & SW_EXTRA)		/* SHIFT key?		*/
	    addrow = AN_ROW_CNT;
	else
	    addrow = 1;
#if J_UP
	if (edges & J_UP)
	{
	    update = (baserow != 0);
	    if ((baserow -= addrow) < 0)
		baserow = 0;
	}
	else
#endif
#if J_RIGHT
	if (edges & J_RIGHT)
	{
	    update = (baserow != 0);
	    if ((baserow -= addrow) < 0)
		baserow = 0;
	}
	else
#endif
#if J_DOWN
	if (edges & J_DOWN)
	{
	    update = (baserow != AN_MAX_ROWS);
	    if ((baserow += addrow) > AN_MAX_ROWS)
		baserow = AN_MAX_ROWS;
	}
#endif
#if J_LEFT
	if (edges & J_LEFT)
	{
	    update = (baserow != AN_MAX_ROWS);
	    if ((baserow += addrow) > AN_MAX_ROWS)
		baserow = AN_MAX_ROWS;
	}
#endif
	if (update)
	{
	    stamp = (baserow * AN_COL_CNT);
	    for (j=0; j < AN_ROW_CNT; j++)
	    {
		m_int x,y;

		x = AN_LFT_COL - 3;
		y = AN_TOP_ROW + (j * AN_ROW_SEP);
		txt_hexnum(x,y,stamp/16,2,RJ_ZF,MNORMAL_PAL);

		for (i=0,x = AN_LFT_COL; i < AN_COL_CNT; i++,x += AN_COL_SEP) {
		    txt_stamp(x,y,stamp,textPal);
		    if ( stamp++ >= AN_STMP_CNT ) {
			txt_clr_wid(x,y,(AN_COL_CNT-i)*AN_COL_SEP);
			break;
		    }
		}
	    }
	    txt_str(3,3,"Pal: ",MNORMAL_PAL);
	    txt_chexnum(textPal,8,RJ_ZF,MNORMAL_PAL);
	    txt_str(3,AN_VIS_ROW-5,"tv: ",MNORMAL_PAL);
	    txt_chexnum((U32)which,8,RJ_ZF,MNORMAL_PAL);
	    if ( which ) {
		txt_cstr(" fs: ",MNORMAL_PAL);
		txt_chexnum((U32)which->screen,8,RJ_ZF,MNORMAL_PAL);
		txt_cstr(" db: ",MNORMAL_PAL);
		txt_chexnum((U32)which->dirty,8,RJ_ZF,MNORMAL_PAL);
	    }
	    update = 0;	/* done with update	*/
	}
    }
    return 0;
}
#endif

/*		txt_select(which_method)
 *	Zoid20 (and 3Dfx) has several different methods for getting
 *	text on the screen, none of them completely satisfactory.
 *	Use txt_select() to select the least objectionable
 *	at any given time. Returns previous setting.
 */

static int which_txt;

int txt_select ( method )
int method;
{
    int old_method;

    old_method = vid_str ? which_txt:TXT_HOST;
    switch (method) {
	case TXT_HOST :
	case TXT_VSCREEN :
	    vid_str = 0;
	    vid_stamp = 0;
	    vid_tclr = sst_tclr;
	    vid_refresh = sst_text2fb;
	    break;
	case TXT_NONE :
	    vid_str = 0;
	    vid_stamp = 0;
	    vid_tclr = 0;
	    vid_refresh = 0;
	    break;
	default :
	    method = old_method;
	    break;
    }
    which_txt = method;
    return old_method;
}

#if DEVELOPMENT
int sst_text_test( const struct menu_d *smp ) {
    int do_it, start, ii, jj, old_prc;
    U32 ctls;
    char msg[52];
    int (*old_dbg_str)(int col, int row, const char *string, int font);
    void (*old_dbg_stamp)(int col, int row, int stampno, int pal_font);
    void (*old_dbg_tclr)(int col, int row, int width);

    do_it = 1;
    start = 0;

    old_dbg_str = dbg_str;
    old_dbg_stamp = dbg_stamp;
    old_dbg_tclr = dbg_tclr;
    dbg_str = 0;
    dbg_stamp = 0;
    dbg_tclr = 0;
    old_prc = prc_delay_options(PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_CLEAR);

    while ( ((ctls = ctl_read_sw(SW_NEXT|SW_ACTION)) & SW_NEXT) == 0 ) {
	if ( ctls & SW_ACTION ) do_it = 1;
	if ( do_it ) {
	    int row;
	    jj = start;
	    for (row=6; row < AN_VIS_ROW-6; ++row) {
		for (ii=0; ii < 26; ++ii) {
		    int c;
		    c = 'A' + ii + jj;
		    if (c > 'Z') c = c-'Z' -1 + 'A';
		    msg[ii*2] = c;
		    msg[ii*2+1] = ' ';
		}
		msg[ii*2] = 0;
		txt_str(-1, row, msg, WHT_PAL);
		++jj;
		if (jj > 25) jj = 0;
	    }
	    do_it = 0;
	    ++start;
	    if (start > 25) start = 0;
	}
	prc_delay(0);
    }
    dbg_str = old_dbg_str;
    dbg_stamp = old_dbg_stamp;
    dbg_tclr = old_dbg_tclr;
    prc_delay_options(old_prc);
    return 0;
}
#endif /* DEVELOPMENT */

