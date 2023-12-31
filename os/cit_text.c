#ifndef STAND_ALONE
#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <string.h>
extern int putn_ams(const unsigned char *buff, int cnt);
#else
#define CIT_ONLY (1)
#define BOLD_FONT (0x1)
#define UL_FONT (0x2)
#define REVERSE_FONT (0x4)
#define NORMAL_FONT (0x0)
#define BGBIT (0x80000000)
#define AN_VIS_COL (80)
#define AN_VIS_ROW (24)
#define	RJ_ZF	0	/* Right Justify with ZERO fill		*/
#define	RJ_BF	1	/* Right Justify with BLANK fill	*/
#define	LJ_BF	2	/* Left Justify with BLANK fill		*/
#define	LJ_NF	3	/* Left Justify with NO fill		*/
#include <stdio.h>
#include <string.h>
int putn_ams(const unsigned char *buff, int cnt)
{
    return (write(1,buff,cnt));
}
/*		utl_chex()
* This routine will convert the number in num to a hexadecimal string. It
* will zero fill to the size in length. It the number is larger then
* length it will display the low order nibbles.
*  The format is == 0 for zero fill, !=0 for blank fill
*/

static int __ultodec();

static const char hexdig[] = "0123456789ABCDEF";
int
utl_chex(num, buffer, length, format)
unsigned long num; char *buffer; int length, format;
{
    int idx = length;
    buffer[idx] = '\0';
    format = format ? ' ' : '0';
    do {
	buffer[--idx] = hexdig[num & 0xF];
	num >>= 4;
    } while ( (idx > 0) && (num != 0));
    length = length - idx;
    while ( idx > 0 ) buffer[--idx] = format;
    return length;
}

/*	 utl_cdec(num,buffer,length,format)
*
* This routine will convert the number in num to a decimal string. It
* will zero fill to the size in length. If the number is larger then
* length it will display the low order digits.
*  The format is:
*		0 = right just, zero fill
*		1 = right just, blank fill
*		2 = left just, blank fill
*		3 = left just, no fill (string is exact length needed)
*/

#define MAXLEN (11)			/* 32 bits gives 10 digits + '\0' */

int
utl_cdec(num,buffer,length,format)
unsigned long num;
char *buffer;
int length, format;
{
    char tmp[MAXLEN+1];
    int slop,digits;

    digits =__ultodec(num,tmp);		/* convert number, return # of digits */
    slop = length - digits;		/* How much space do we need to fill ?*/
    if ( slop <= 0 ) {
	/* Number won't actually fit. We mimic the old behavior by returning
	 * LSDs. This is also used when number exactly fits
	*/
	memcpy(buffer,tmp-slop,length);		/* -slop is +digits-length */
    } else switch (format) {
	case RJ_ZF:
	    memcpy(buffer+slop,tmp,digits);
	    memset(buffer,'0',slop);
	    break;
	case RJ_BF:
	    memcpy(buffer+slop,tmp,digits);
	    memset(buffer,' ',slop);
	    break;
	case LJ_BF:
	    memcpy(buffer,tmp,digits);
	    memset(buffer+digits,' ',slop);
	    break;
	case LJ_NF:
	    memcpy(buffer,tmp,digits);
	    length = digits;
    }
    buffer[length] = '\0';
    return digits;
}
#endif


static int cit_puts( const char *string )
{
    int width = strlen(string);
    return putn_ams((const unsigned char *)string,width);
}

static struct posn {
    short row;
    short col;
} p_posn;

static int cur_font;
#ifdef CIT_ONLY
static int def_font;
#endif

/* The newer NCD local client xterms seem to no longer understand strings of
 * attributes, so we need to do attributes separately
 */
static const char attrib[] = { "\0011\0024\0047" };
#define N_ATTRIBS (sizeof(attrib)>>1)

static void
cit_font(font_num)
int font_num;
{
    const char *abp;
    char tbuf[3];

    if ( font_num >= (1<<N_ATTRIBS) ) font_num = 0;
#ifdef CIT_ONLY
    if ( font_num == 0 ) font_num = def_font;
#endif
    tbuf[1] = 'm';
    tbuf[2] = '\0';
    if ( ~font_num & cur_font ) {
	/* need to turn an attribute off, so we need to clear them all */
	cit_puts("\033[0m");
	cur_font = 0;
    }
    abp = attrib;
    while ( *abp ) {
	/* scan "attribute-bit"/"selector-char" pairs for one to turn on */
	if ( font_num & ~cur_font & *abp ) {
	    cit_puts("\033[");
	    tbuf[0] = abp[1];
	    cit_puts(tbuf);
	}
	abp += 2;
    }
    cur_font = font_num;
}

static void
cit_posn(col,row)
int col,row;
{
    char numbuf[16];
    cit_puts("\033[");
    utl_cdec(row+1,numbuf,3,LJ_NF);
    cit_puts(numbuf);
    cit_puts(";");
    utl_cdec(col+1,numbuf,3,LJ_NF);
    cit_puts(numbuf);
    cit_puts("H");
}

#ifdef CIT_ONLY
int
txt_cstr(string,psmask)
const char	*string;
int	psmask;
{
    return txt_str(0,-1,string,psmask);
}

int (*vid_str)(int col, int row, const char *string, int font);
void (*vid_stamp)(int col, int row, int stampno, int pal_font);
void (*vid_tclr)(int col, int row, int width);

#else
/* 	While bringing up new hardware, we may want to output
 *	text to both the remote terminal and the screen.
 */
extern int (*dbg_str)(int col, int row, const char *string, int font);
extern void (*dbg_stamp)(int col, int row, int stampno, int pal_font);
extern void (*dbg_tclr)(int col, int row, int width);

int cit_str(int col, int row, const char *string, int font);
void cit_stamp(int col, int row, int stampno, int pal_font);
void cit_clr_wid(int col, int row, int width);

void cit_init()
{
    dbg_str = cit_str;
    dbg_stamp = cit_stamp;
    dbg_tclr = cit_clr_wid;
}

void cit_deinit()
{
    dbg_str = 0;
    dbg_stamp = 0;
    dbg_tclr = 0;
}

int cit_on_off(smp)
const struct menu_d *smp;
{
    const char *lp;
    lp = smp->mn_label;
    if ( *lp == '\n' ) ++lp;
    switch ( smp->mn_label[0] ) {
	case 'E' :
	    cit_init();
	    break;
	case 'D' :
	    dbg_str = 0;
	    dbg_stamp = 0;
	    dbg_tclr = 0;
	    break;
#if (0)
	case 'H' :
	    txt_select(TXT_HOST);
	    break;
	case 'N' :
	    txt_select(TXT_NONE);
	    break;
#endif
    }
    return 0;
}
#endif



int
#ifdef CIT_ONLY
txt_str(col,row,string,palette)
#else
cit_str(col,row,string,palette)
#endif
int	col,row;
const char	*string;
int	palette;
{
    struct posn *p = &p_posn;
    int width;
    int font;

    width = strlen(string);
    if ( row == -1 ) {
	col = p->col;
	row = p->row;
    } else if ( col == -1 ) {
	col = (AN_VIS_COL+1 - width) >> 1;
	if ( col < 0 ) col = 0; 
    }
    font = NORMAL_FONT;
    if ( palette == MHILITE_PAL ) font = UL_FONT;
    else if ( palette & BGBIT ) font = REVERSE_FONT;
#if (TITLE_PAL == ERROR_PAL)
    /* a popular choice that makes parameter entry
     * annoyingly difficult to read, so we tone it down
     * a bit.
     */
    if ( palette == ERROR_PAL ) font = (BOLD_FONT);
#else
    /* If the TITLE palette is not the same as the ERROR
     * palette, we can afford to shout our errors a bit
     * louder.
     */
    if ( palette == ERROR_PAL ) font = (BOLD_FONT|REVERSE_FONT);
#endif
    else {
	int base_pal = palette & ~BGBIT;
	if ( base_pal == (MHILITE_PAL & ~BGBIT) ) font |= BOLD_FONT;
	if ( base_pal == (TITLE_PAL & ~BGBIT) ) font |= UL_FONT;
    }
#if (1)
    /* because of tncat/xterm wierdness, we have to "park"
     * at the bottom of the screen for now, so we will
     * always need to position before writing.
     */
    cit_posn(col,row);
#else
    if ( row != p->row || col != p->col ) cit_posn(col,row);
#endif
    if ( font != cur_font ) cit_font(font);
    cit_puts(string);
#ifdef CIT_ONLY
    if ( vid_str ) vid_str(col, row, string, palette);
#endif
    p->row = row;
    p->col = col+width;
#if (1)
    /* Bogosity intended to compensate for tncat/xterm
     * refusal to look at anything until a '\n'. This also
     * has the effect of "resting" one line below the
     * alleged bottom of the screen.
     */
    cit_posn(0,AN_VIS_ROW-1);
    cit_puts("\n");
#endif
    return width;
}

#ifdef CIT_ONLY
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
#endif

/*	Routine:	txt_stamp(x,y,letter,psmask)
*	Inputs:		int	x,y
*			int	letter,psmask
*	Purpose:	To output a single character to the apha screen. Ignores
*			character-set translation, so psmask must only be color
*/
const char tty_tran[32] = {
    '\0', '#', '#', '#',
    'L', '+', '<'
};
void
#ifdef CIT_ONLY
txt_stamp(x,y,letter,psmask)
#else
cit_stamp(x,y,letter,psmask)
#endif
int	x,y;
int	letter,psmask;
{
    char buf[2];
    int tty_letter = '\0';
    if ( letter >= ' ' && letter < 0x7f ) tty_letter = letter;
    else if ( letter < ' ' ) tty_letter = tty_tran[letter];  
    buf[1] = '\0';
    buf[0] = tty_letter;
#ifdef CIT_ONLY
    txt_str(x,y,buf,psmask);
#else
    cit_str(x,y,buf,psmask);
#endif
#ifdef CIT_ONLY
    if ( vid_stamp ) vid_stamp(x, y, letter, psmask);
#endif
}

#define USE_ESCAPES (1)

void
#ifdef CIT_ONLY
txt_clr_wid(x,y,width)
#else
cit_clr_wid(x,y,width)
#endif
int x,y,width;
{
    if ( x < 0 ) {
	/* wants to center */
	x = (AN_VIS_COL+1-width) >> 1;
	if ( x < 0 ) {
	    x = 0;
	    width = AN_VIS_COL;
	}
    }
#ifdef CIT_ONLY
    if ( vid_tclr ) vid_tclr(x, y, width);
#endif
#if USE_ESCAPES
    if ( x == 0 ) {
	if ( width == AN_VIS_COL ) {
	    /* Use "erase entire line" */
	    cit_posn(x,y);
	    cit_puts("\033[2K");
	} else {
	    /* Use "erase from beginning of line to cursor */
	    cit_posn(x+width-1,y);
	    cit_puts("\033[1K");
	}
	
    } else if ( (x+width) >= AN_VIS_COL ) {
	    /* use "erase from cursor to end of line" */
	    cit_posn(x,y);
	    cit_puts("\033[0K");
    } else
#if (0)
    {
	/* real VT100's do "erase from xx to xx */
	char numbuf[16];
	cit_posn(x,y);
	utl_cdec(x+1,numbuf,3,LJ_NF);
	cit_puts("\033[>3;");
	cit_puts(numbuf);
	cit_puts(";");
	utl_cdec(x+1+width,numbuf,3,LJ_NF);
	cit_puts(numbuf);
	cit_puts("K");
    }
#endif /* deleted "real Vt100" code */
#endif /* USE_ESCAPES */
    {
	/* The hard way, output spaces */
	cit_posn(x,y);
	cit_font(NORMAL_FONT);
	while ( width > 0 ) {
	    int this_time;
	    this_time = width;
	    if (this_time > 20 ) this_time = 20;
	    cit_puts("                    "+20-this_time);
	    width -= this_time;
	}
    }
}

#ifdef CIT_ONLY
void
txt_clr_str(x,y,string,pal)
int x,y,pal;
const char *string;
{
    int width = txt_width(string,pal);
    txt_clr_wid(x,y,width);
}

int
txt_width(text,set)
const char *text;
int set;
{
    return strlen(text);
}

int txt_height(txt,set)
const char *txt;
int set;
{
    return 1;
}

void st_font(font_num)
int font_num;
{
    def_font = font_num;
}
#endif

#ifdef STAND_ALONE
#include <stdio.h>
#include <stdlib.h>
static int __ultodec(val,buff)
unsigned long val;
char *buff;
{
    sprintf(buff,"%lu",val);
    return strlen(buff);
}
/*	character renditions for CIT-101 (superset of VT-100)
 *
 * [0m		begin normal graphic rendition  (resets next 4)
 * [1m		begin bold rendition
 * [4m		begin underline rendition
 * [7m		begin reverse video rendition
 *	Below not supported on Xterms
 * [5m		begin blink rendition
 * [0w		enable blinking
 * [1w		disable blinking in off state
 * [2w		disable blinking in on state
 */

int main(argc,argv)
int argc;
char **argv;
{

    int i,val;
    txt_clr_wid(0,0,AN_VIS_COL+1);
    txt_clr_wid(1,1,AN_VIS_COL+1);
    txt_clr_wid(2,2,42);
    for ( i = 1 ; i < argc ; ++i ) {
	val = strtol(argv[i],0,10);
	txt_clr_wid(0,i+2,12);
	txt_str(2,i+2,"#",0);
	txt_cdecnum(val,2,RJ_ZF,0);
	txt_cstr("Test Text string",val);	
	txt_clr_wid(i+5,i+2,AN_VIS_COL-10-i);
    }
    txt_cstr("",0);
    return 0;
}
#endif
