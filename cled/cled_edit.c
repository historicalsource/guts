/*
* The old commands (history) buffer is arranged as follows:

	|<----------------- HISTBUFSIZ = n   ------------------->|
	|		   					|
	*********************************************************
	[     old comm buffer      				]
	*********************************************************
	^			   				^
	 \_old		   				oldend_/ 

* oldndx starts at 0 (an always empty line with length 1). The strings
* are maintained where the first byte is the length of the string and
* the second byte is the length of the previous string. The third and
* subsequent bytes contain the string itself. I.e., if s points to the
* first byte of a string, then "s += *s" will point to the next
* string and "s -= *(s+1)" will point to the start of the previous
* string. The last string of the buffer will have a 0 length.
* 
*
* The new command buffer is arranged with a bubble at the cursor for
* insertion of new text:

	|<---------------------- maxlin ----------------------->|
	|		   					|
	*********************************************************
	[     front     ]      (bubble)		[    back	]
	*********************************************************
	^		^			^		^
	 \_buf		 \_lcurs=NULL	 rcurs_/   bufend=NULL_/
*
* The text from buf to lcurs is what is displayed left of the cursor.
* The text from rcurs to bufend is what is displayed right of the cursor.
* Normally, lcurs points to the NULL at the end of the string. bufend
* doesn't move and always points to a NULL at the end of the buffer.
* When the buffer fills up (lcurs = rcurs), no more data is accepted
* and a BELL code is sent to the terminal.
*
* The behavior of the terminal echo under this line discipline is 
* different than what you might be used to in the sense that characters
* are only echoed if there is a read pending on the device (ala VMS, MS-DOS
* and other O/S's). Characters input are held in a typeahead buffer.
* When a read to the port is executed, the characters are removed from the
* typeahead buffer and processed by the parser. The parser determines what
* (if any) characters should be echoed for each character input. This also
* means that if an ioctl function is performed that turns off echo after
* characters have been placed in the typeahead buffer, but before a read
* is issued, those characters may not be echoed at all by the driver
* (they will, however, be delivered to the process that issues the first
* read which may choose to echo them itself). 
*
*/

#if !defined(M_KERNEL)
#include "cled_io.h"
#endif

#define FORWARD 0
#define REVERSE 1

/* These will keep lint and gcc from complaining */

static int pfront(LedBuf *,int,int),pback(LedBuf *,int,int);
static void eol(LedBuf *),bol(LedBuf *),reprint(LedBuf *);

/************************************************************************
 * The following n routines comprise the editor proper. The process	*
 * context in all cases is task time. None of these routines are 	*
 * (nor should they ever be) called at interrupt time.			*
 ************************************************************************/

/************************************************************************
 * Echo a char to the terminal
 */
	 static int echo(
	 unsigned char chr,	/* char to be echoed */
	 LedBuf *lb,		/* ptr to led_buf containing cmd_buf */
   	 int flush_it)		/* not 0, if we're to flush afterward */

/* At exit:
 *	chr is written to the output que and may be enclosed with
 *	inverse video escape sequences if the char is non-printable. Tabs
 *	are expaned to an appropriate number of spaces.
 *	Several members in the lb struct will have been changed.
 */
{
    char **cp;
    cp = lb->ansi;
    if (chr < ' ' || chr == 0177) {
        if (chr == 0) return 0;
	if (chr == TAB) {
	     lb->c_posn += ((chr = (8 - (lb->c_posn & 7))) - 1);
	     while (chr--) cle_putc(' ',lb);
	     chr = TAB;
	} else {
             if (chr == 0177) chr = '?'-'@';	/* convert DEL to inverse ? */
	     cle_puts(cp[ANSI_SETINV],lb); 	/* inverse video for VT100 */
	     cle_putc(chr + '@',lb);
	     cle_puts(cp[ANSI_SETNORM],lb);
	}
    } else {
	cle_putc(chr,lb);
    }
    lb->c_posn++;
    if (flush_it) cle_flush(lb);
    return (chr);
}

/***********************************************************************
 * Put a string of text into the command buffer, echoing them as they
 * are inserted.
 */
	 static void putt(
	 char *cp,		/* ptr to string of chars to be copied */
	 int cnt,		/* count of chars to copy */
	 LedBuf *lb)		/* ptr to led_buf into which to place the text */

/* At exit:
 *	chars moved into lb->buf[lb->lcurs].
 *	lb->lcurs adjusted as required
 *	lb->rcurs adjusted if overstrike mode
 *	lb->flags may be set
 *	chars echoed as required
 */
{
    register unsigned char **rc, **lc;
   
    rc = &lb->rcurs;		/* point to right side of hole */
    lc = &lb->lcurs;		/* point to left side of hole */

    while ((*lc < *rc - 1) && cnt--) { /* while chars and no overlap */
   	char c;
	c = *cp;
	if ((lb->cledio.c_iflag&ISTRIP) != 0) c &= 0x7F;
#if defined(IUCLC)
	if ((lb->cledio.c_iflag&IUCLC) != 0 && (c >= 'A') && (c <= 'Z')) c += 'a'-'A';
#endif
	lb->flags |= LD_DIRTY;	/* buffer has been modified */
	**lc = echo((*cp++ = c),lb, NO);	/* stuff the char and echo it without flushing */
	*lc += 1;		/* move left pointer */
	if ((lb->flags&LD_INSERT) == 0 && **rc) {	/* move right ptr if overstrike... */
	    *rc += 1 ;			/* ...mode and not already on null */
	}
    }
    **lc = 0;			/* make sure left side is pointing to 0 */
    if (cnt >= 0) cle_putc(BEL,lb);
    pback(lb,NO,YES);
}

/*************************************************************************
 * Find pointer to n'th string in history buffer.
 */
	 static unsigned char *ptr(
	 struct led_buf *lb,	/* ptr to ledbuf */
	 unsigned char *start,	/* ptr into buffer where search is to begin */
	 int ndx)		/* the number of the string to find */

/* At exit:
 *	returns ptr to first char of requested string if present 
 *	  else it returns ptr to null string at end of buffer.
 */
{
    while (ndx-- && *start) start += *start;
    return (start);
}

/*************************************************************************
 * Find the pointer to matching string in history buffer.
 */
	 static unsigned char *match_ptr(
	 struct led_buf *lb,	/* ptr to ledbuf */
	 unsigned char *src,	/* ptr to place in buffer to begin the search. */
	 unsigned char *patt,	/* ptr to string to match against */
	 int len,		/* len of string pointed to by patt */
	 int *indx,		/* ptr to int into which the number of the found string in the */
	 			/* buffer is placed. */
	 int exact,		/* if YES, the lengths must match as well as the pattern. */
	 			/* if NO, only 'len' chars are compared. */
	 int direction)		/* direction to search */
	 
/* At exit:
 *	returns ptr to first char in found string or ptr to null string
 *	at end of buffer if no match.
 *	*indx is updated if the string is found otherwise it is not changed.
 */
{
    int nxtndx;
    nxtndx = *indx;
    while(*src) {
	if (*src-2 == len || (exact == NO && (int)(*src)-2 > len)) {
	    int result,tlen;
	    unsigned char *a,*b;
	    a = src+2;
	    b = patt;
	    tlen = len;
	    result = 0;
	    while(tlen--) if ((result = *a++ - *b++) != 0) break;
	    if (result == 0) break;
	}
        if (direction == FORWARD) {
	   src += *src;
	   ++nxtndx;
	} else {
	   src -= *(src+1);
	   if (--nxtndx <= 0) break;
	}
    }
    if (*src <= 2) {
        src = (unsigned char *)0;
    } else {
        *indx = nxtndx;
    }
    return src;
}

/****************************************************************************
 * Move the cursor n places to the left.
 */
	    static void curs_l(
	    int cnt,		/* number of column positions to move left */
	    FLAG delete,	/* if 1, delete the characters under the cursor as it is moved */
	    			/* if 0, don't delete the chars under the cursor */
	    LedBuf *lb)		/* ptr to led_buf which holds the cursor info */

/* At exit:
 *	cursor is moved left appropriately unless it is already at the left
 *	margin in which case nothing will happen. Characters may be echoed
 *	or the right half of the line may be redrawn if deleting.
 *	A number of members of the lb struct will have been changed.
 */
{
    register unsigned char *rc, *lc;
    register FLAG atab;
   
    atab = NO;
    rc = lb->rcurs;
    lc = lb->lcurs;
    while ((lc > lb->buf) && cnt--) {
	if ((*--rc = *--lc) == TAB) atab |= YES;
	lb->c_posn--;
	cle_putc(BKSP,lb);
    }
    *(lb->lcurs = lc) = 0;
    if (atab) pfront(lb,NO,NO);
    if (delete) {
	lb->flags |= LD_DIRTY;
	pback(lb,YES,NO);
    } else {
        lb->rcurs = rc;
        if (atab) pback(lb,NO,NO);
    }
    cle_flush(lb);
    return;
}
   
/****************************************************************************
 * Move the cursor n places to the right.
 */
	 static void curs_r(
	 int cnt,		/* number of column positions to move right */
	 int delete,		/* if 1, delete the characters under the cursor as it is moved */
	 			/* if 0, don't delete the chars under the cursor */
	 LedBuf *lb)		/* ptr to led_buf which holds the cursor info */

/* At exit:
 *	cursor is moved right appropriately unless it is already at the right
 *	margin in which case nothing will happen. Characters may be echoed
 *	or the right half of the line may be redrawn if deleting.
 *	A number of members of the lb struct will have been changed.
 */
{
    register unsigned char *rc, *lc;
   
    rc = lb->rcurs;
    lc = lb->lcurs;
   
    while (*rc && cnt--) {
	if (delete) rc++;
	else *lc++ = echo(*rc++, lb, NO);
    }
    lb->rcurs = rc;
    if (delete) {
	lb->flags |= LD_DIRTY;
	pback(lb,YES,NO);
    } else *(lb->lcurs = lc) = 0;
    cle_flush(lb);
    return;
}
   
/************************************************************************
 * Test a char for being alpha-numeric.
 */
	 static int isaln(
	 unsigned char chr)	/* chr to test */

/* At exit:
 *	returns YES if chr is alpha-numeric or '_'
 */
{
    return (
	     ((chr >= 'A') && (chr <= 'Z')) ||
	     ((chr >= '0') && (chr <= '9')) ||
	     ((chr >= 'a') && (chr <= 'z')) ||
	     chr == '_' 
	   );
}
   
/************************************************************************
 * Test a char for being white-space.
 */
	 static int isws(
	 unsigned char chr)	/* chr to test */

/* At exit:
 *	returns YES if chr is white-space
 */
{
    return (
    		chr == ' '   ||
		chr == '\t'  ||
		chr == '\n'
	   );
}
   
/************************************************************************
 * Test a char for being delimiter.
 */
	 static int isdelim(
	 unsigned char chr)	/* chr to test */

/* At exit:
 *	returns YES if chr is delimiter
 */
{
    return (!isaln(chr) && !isws(chr));
}
   
/***********************************************************************
 * Skip to end of "word" on the left and optionally 'eat' it.
 */
   	static void skipwl(
	LedBuf *lb)		/* ptr to led_buf */

/* At exit:
 *	cursor moved. "word" skipped 
 */
{
    register unsigned char *cp;
   
    cp =  lb->lcurs;
    while (cp > lb->buf && isws(*(cp-1))) --cp;
    while (cp > lb->buf && isaln(*(cp-1))) --cp;
    while (cp > lb->buf && isdelim(*(cp-1))) --cp;
    curs_l(lb->lcurs - cp, NO,lb);
    return;
}

/***********************************************************************
 * Skip to end of "word" on the right and optionally 'eat' it.
 */
   	static void skipwr(
	LedBuf *lb)		/* ptr to led_buf */

/* At exit:
 *	cursor moved. "word" either skipped or eaten
 */
{
    register unsigned char *cp;
   
    cp = lb->rcurs;
    while (*cp && isdelim(*cp)) ++cp;
    while (*cp && isaln(*cp)) ++cp;
    while (*cp && isws(*cp)) ++cp;
    curs_r(cp - lb->rcurs, NO,lb);
    return;
}
   
/************************************************************************
 * Delete 'word' to left of cursor
 */
      static void d_lwrd(
      LedBuf *lb)		/* ptr to led_buf containing cmd_buf */

/* At exit:
 *	'word' to left of cursor is deleted unless cursor is already at left
 *	margin in which case nothing happens.
 *	Several members of the lb struct are updated.
 */
{
    register unsigned char *cp;
   
    cp =  lb->lcurs;
    while (cp > lb->buf && isws(*(cp-1))) --cp;
    curs_l(lb->lcurs - cp, NO,lb);
    while (cp > lb->buf && isaln(*(cp-1))) --cp;
    while (cp > lb->buf && isdelim(*(cp-1))) --cp;
    curs_l(lb->lcurs - cp, YES,lb);
    return;
}

/************************************************************************
 * Delete 'word' to right of cursor
 */
	 static void d_rwrd(
	 LedBuf *lb)		/* ptr to led_buf containing cmd_buf */

/* At exit:
 *	'word' to right of cursor is deleted unless cursor is already at right
 *	margin in which case nothing happens.
 *	Several members of the lb struct are updated.
 */
{
    register unsigned char *cp;
   
    cp = lb->rcurs;
    while (*cp && isws(*cp)) ++cp;
    curs_r(cp - lb->rcurs, NO,lb);
    while (*cp && isdelim(*cp)) ++cp;
    while (*cp && isaln(*cp)) ++cp;
    curs_r(cp - lb->rcurs, YES,lb);
    return;
}

/************************************************************************
 * Skip 'word' according to current direction bit
 */
	 static void skipw(
	 LedBuf *lb)		/* ptr to led_buf containing cmd_buf */

/* At exit:
 *	'word' to left of cursor is skipped unless cursor is already at right
 *	margin in which case nothing happens.
 *	Several members of the lb struct are updated.
 */
{
   if (lb->flags&LD_BACKUP) skipwl(lb);
   else skipwr(lb);
   return;
}

/************************************************************************
 * Skip to one end of the line or the other depending on direction bit
 */
	 static void skiptoe(
	 LedBuf *lb)		/* ptr to led_buf containing cmd_buf */

/* At exit:
 *	cursor is positioned at either end of the line depending on direction bit
 *	Several members of the lb struct may be updated.
 */
{
   if (lb->flags&LD_BACKUP) bol(lb);
   else eol(lb);
   return;
}

/****************************************************************************
 * Fix the history buffer by setting the forward and backward lengths.
 */
 	static int fix_history(
/*
 * At entry:
 */
		LedBuf *lb)	/* pointer to ledbuf */
/*
 * At exit:
 *	lb->old has had the forward and backwards length fields corrected
 *	always returns 0
 */
{
    int last_len;
    uchar *cp1;
    last_len = 0;
    for (cp1 = lb->old; *cp1 && (cp1 + *cp1 <= lb->old+HISTBUFSIZ-2);) {
	*(cp1+1) = last_len;
	last_len = *cp1;
	cp1 += last_len;
    }
    *cp1++ = 0;			/* terminate the last string */
    *cp1 = last_len;		/* and tell last string how long the previous string was */
    return 0;
}
 
/****************************************************************************
 * Copy current contents of command buf to history buffer
 */
	 static void copycom(
	 unsigned char *cp,	/* ptr to place in history buffer to deposit cmd buf */
	 int len,		/* number of chars to move */
	 LedBuf *lb,		/* ptr to led_buf containing the cmd buf */
         int prev_len)		/* length of previous string */

/* At exit:
 *	command string is moved to requested spot in buffer
 *	Old strings in the history buffer may be deleted if there's no room.
 *	A number of members of the lb struct will have been changed.
 */
{
    register unsigned char *cp1,*cp2;
    if (cp+len+2 > lb->old+HISTBUFSIZ) return;	/* don't do it if command too big */
    cp1 = cp;			/* ptr to destination */
    cp2 = lb->buf;		/* ptr to source */
    *cp1++ = len;		/* dst gets length (note this is type char!) */
    *(cp+len+1) = len;		/* next string gets our length too */
    *cp1++ = prev_len;		/* len of previous string */
    len -= 1;			/* subract the length byte from the length */
    while (--len) *cp1++ = *cp2++;	/* move the text */
    fix_history(lb);		/* fixup the history buffer */
}

/************************************************************************
 * Complete the read (insert a newline in command buffer)
 */
	 static void newline(
	 LedBuf *lb)		/* ptr to led_buf containing cmd_buf */

/* At exit:
 *	lb struct is cleaned up and a LD_DONE is set in flags to indicate the
 *	read is complete. The contents of the command line are inserted in
 *	the history buffer. If the contents match a line already in the history
 *	buffer, the dup is removed and the new one is inserted at the front.
 *	Several members of the lb struct are updated.
 */
{
    register unsigned char *end;
    register unsigned char *cp;
    int comlen;
   
    eol(lb);		/* move bubble to end of line */
    comlen = (lb->lcurs - lb->buf) + 2;	/* length of current command */
    if ((comlen > 2) && (comlen < (HISTBUFSIZ - 4))) {
	/*
	* If dirty = NO, then we used an old line without modification
	* so we'll squeeze it out of the old buffer to eliminate the
	* repetition, and recopy it into the front of the old buffer.
	* This allows us to hold older commands longer. If dirty = YES,
   	* then we'll look for a matching command in the buffer and, if
   	* one is found, move it to the head of the buffer. If not, then
   	* we just insert the new string at the head of the buffer.
	*/
	lb->oldndx = 1;
        end = match_ptr(lb,lb->old+2,lb->buf,comlen-2,&lb->oldndx,YES,FORWARD);
	lb->oldmatlen = comlen-2;
	if (end == 0) end = lb->old+HISTBUFSIZ-2 - comlen;
	for (cp = end + comlen; end > lb->old+2; *--cp = *--end);
	copycom(lb->old+2, comlen, lb, 2);	/* put it in the old com buffer  */
    }
    lb->flags |= LD_DONE;
    return;
}

/************************************************************************
 * Print the prompt string to the terminal
 */
	 static int pprompt(
	 LedBuf *lb,		/* ptr to led_buf containing prompt string */
	 int clr_flg)		/* YES if to pre-clear the line before printing */

/* At exit:
 *	prompt string is written to the output que. Some chars may be
 *	converted to displayable ones. 
 */
{
    if (clr_flg) {
       cle_puts(lb->ansi[ANSI_CLRLINE],lb);
    } else {
       cle_putc('\r',lb);	/* else start with cr */
    }
    cle_puts((char *)lb->prmpt,lb);
    cle_flush(lb);
    return 0;
}

/************************************************************************
 * Print the text to the left of the cursor
 */
	 static int pfront(
	 LedBuf *lb,		/* ptr to led_buf containing prompt string */
	 int clr_flg,		/* YES if to pre-clear the line before printing */
         int flush_it)		/* YES if to flush after printing */

/* At exit:
 *	The prompt string and the text from lb->buf[0] through
 *	lb->buf[lb->lcurs] is put on the output que. Some characters
 *	may have been converted to displayable ones. The message completes 
 *	with an escape sequence to turn on the cursor.
 */
{
    register unsigned char *cp;
   
    lb->c_posn = 0;
    pprompt(lb,clr_flg);
    for (cp = lb->buf; echo(*cp++, lb, NO); ); 
    if (flush_it) cle_flush(lb);
    return 0;
}    
   
/***********************************************************************
 * Clear the display from the cursor to the eol
 */
	 static void clreol(
	 LedBuf *lb)		/* ptr to led_buf */
	 
/* At exit:
 *	clreol string output if available else appropriate number of spaces/BS's
 *	output queued to output queue
 */
{
    char *clr;
    if ((clr=lb->ansi[ANSI_CLREOL]) == 0) {
       int siz1,siz2;
       siz1 = siz2 = lb->bufend-lb->rcurs;
       while(siz1-- > 0) cle_putc(' ',lb);
       while(siz2-- > 0) cle_putc(BKSP,lb);
    } else {
       cle_puts(clr,lb);	/* tell terminal to erase to eol */
    }
    cle_flush(lb);
}

/************************************************************************
 * Print the text from the cursor to the end of line
 */
	 static int pback(
	 LedBuf *lb,		/* ptr to led_buf containing prompt string */
	 int deol,		/* if 1, follows message with an escape sequence to delete to */
	 			/* end of line. */
	 			/* if 0, don't delete to end of line */
   	 int flush_it)		/* if YES, flush after writing */

/* At exit:
 *	String from lb->buf[lb->rcurs] to end of buffer is sent to the
 *	output queue.
 */
{
    register unsigned char *cp;
    register tmp1,tmp2;
    char **ansi;

    ansi = lb->ansi;
    if (*lb->rcurs == 0) {	/* if already at the right margin */
       if (deol) clreol(lb);	/* and we're to deol, then do it */
       return 0;		/* else just quietly exit */
    }
    if (ansi[ANSI_SAVE] != 0 && *ansi[ANSI_SAVE] != '\0') {
        cle_puts(lb->ansi[ANSI_SAVE],lb);	/* save cursor posn */
    }
    tmp1 = lb->c_posn;		/* save current cursor postion */
    for (cp = lb->rcurs; echo(*cp++, lb, NO); ); /* output right side of text */
    tmp2 = lb->c_posn;		/* record new cursor position */
    lb->c_posn = tmp1;		/* reset the cursor position */
    if (deol) clreol(lb);	/* deol */
    if (ansi[ANSI_SAVE] == 0 || *ansi[ANSI_SAVE] == '\0') {
        for (tmp1 = tmp2 - tmp1; tmp1-- > 0; cle_putc(BKSP,lb)); /* restore cursor */
    } else {
	cle_puts(ansi[ANSI_RESTORE],lb);		/* restore_cursor */
    }
    cle_flush(lb);
    return 0;
}

/**************************************************************************
 * Send a message to the terminal
 */
	 static int msg(
	 char *str,		/* pointer to null terminated string containing message */
	 LedBuf *lb)		/* ptr to led_buf to which to send the message */

/* At exit:
 *	message is placed in output que wrapped with the
 *	necessary escape sequences to make the message appear on the line
 *	above the current line. Command line is refreshed.
 */
{
    cle_puts(lb->ansi[ANSI_UP],lb);  /* up one line */
    cle_puts(lb->ansi[ANSI_CLRLINE],lb); /* clear line */
    cle_puts(str,lb);		/* user's message */
    cle_putc(LF,lb);		/* generic down-arrow */
    reprint(lb);		/* refresh */
    cle_flush(lb);
    return 0;
}

/*************************************************************************
 * The following n routines are executed as a result of input of the	 *
 * appropriate key. The process context in these cases remain task time. *
 * They should never be called at interrupt time.			 *
 *************************************************************************/

static void c_r(LedBuf *lb) {	/* right arrow, move cursor non-destructively */
    curs_r(1, NODEL, lb);
}
   
static void c_l(LedBuf *lb) {	/* left arrow, move cursor non-destructively */
    curs_l(1, NODEL,lb);
}
   
static void d_eol(LedBuf *lb) {	/* PF4, delete from cursor to end of line */
   if (lb->rcurs != lb->bufend) { /* if not already at eol */
       clreol(lb);		/* zap the display */
       lb->rcurs = lb->bufend;	/* eat all the chars on the left side */
       *lb->rcurs = 0;		/* put null at end of the buffer */
       lb->flags |= LD_DIRTY;	/* say we're dirty */   
   }
}
      
static void d_bol(LedBuf *lb) {	/* NAK, delete from cursor to beginning of line */
   curs_l(lb->maxlin,DEL,lb);
}
   
static void d_rchr(LedBuf *lb) { /* delete char under cursor (right of cursor) */
    curs_r(1, DEL, lb);
}
   
static void d_lchr(LedBuf *lb) { /* delete char to left of cursor */
    curs_l(1, DEL, lb);
}

static void eol(LedBuf *lb) {	/* go to end of line */
    curs_r(lb->maxlin,NODEL,lb);
}
   
static void bol(LedBuf *lb) {	/* go to beginning of line */
    curs_l(lb->maxlin,NODEL,lb);
}
   
static void d_wlin(LedBuf *lb) { /* ^X, delete whole line no matter where cursor is */
   eol(lb);
   d_bol(lb);
}

static void tog_insrt(LedBuf *lb) { /* toggle insert/overstrike mode */
    lb->flags ^= LD_INSERT;
}
   
static void reprint(LedBuf *lb) { /* refresh the input line */
    pfront(lb,NO,NO);
    pback(lb,YES,YES);
}

static void getold_prev(LedBuf *lb) { /* up arrow, recall next line in history */
    register *xptr;
    int cnt;   
    uchar *src;

    xptr = &lb->oldndx;
    pprompt(lb,YES);
    lb->rcurs = lb->bufend;
    lb->lcurs = lb->buf;
    lb->c_posn = 0;		/* set the column position */
    lb->oldmatlen = 0;		/* recall this way eats pattern */
    *xptr += 1;			/* advance to next string */
    src = ptr(lb,lb->old,*xptr);
    if ((cnt = *src) > 2) {
       putt((char *)src+2,cnt-2,lb);
    }
    lb->flags &= ~LD_DIRTY;
}

static void getold_next(LedBuf *lb) { /* down arrow, recall previous line in history */
    register *xptr;
    int cnt;   
    uchar *src;

    xptr = &lb->oldndx;
    pprompt(lb,YES);
    lb->rcurs = lb->bufend;
    lb->lcurs = lb->buf;
    lb->c_posn = 0;		/* set the column position */
    lb->oldmatlen = 0;		/* recall this way eats pattern */
    while (*xptr > 0) {
       *xptr -= 1;
       src = ptr(lb,lb->old,*xptr);
       if ((cnt = *src) > 2) {
	  putt((char *)src+2,cnt-2,lb);
	  break;
       }
    }
    lb->flags &= ~LD_DIRTY;
}

static void getold_str_dir(LedBuf *lb,int direction) {	/* recall next matching line in history */
    int comlen,skip,oldndx;
    unsigned char *new=0;

    eol(lb);			/* move bubble to end of line */
    comlen = (lb->lcurs - lb->buf);	/* length of current command */
    if (comlen == 0 || 		/* if the current command line is empty or.. */
   	((lb->flags&LD_DIRTY) == 0 && /* the line hasn't been changed and.. */
   	  lb->oldmatlen == 0)) { /* the previous search string is empty.. */
       if (direction == FORWARD) {
	  getold_prev(lb);		/* then just pretend it's an up-arrow */
       } else {
	  getold_next(lb);		/* then just pretend it's a down-arrow */
       }
       return;
    }
    oldndx = lb->oldndx;		/* save the current index */
    skip = 1;
    if ((lb->flags&LD_DIRTY) != 0 ) {
	lb->oldmatlen = comlen;
        if (direction == FORWARD) {
	   skip = oldndx = lb->oldndx = 0; /* start looking from beginning */
	   new = lb->old;
	}
    }
    if (skip) {
        new = ptr(lb,lb->old,lb->oldndx);  /* get the last command fetched */
	if (*new > 2) {
	    if (direction == FORWARD) {
	       ++lb->oldndx;	/* start looking at one beyond that */
	       new += *new;	/* move to next string */
	    } else {
	       if (lb->oldndx < 1) {
		  cle_putc(BEL,lb);
		  cle_flush(lb);
		  return;
	       }
	       --lb->oldndx;	/* start looking at one previous to that */
	       new -= *(new+1); /* move to previous string */
	    }
	} else {
	    cle_putc(BEL,lb);
	    lb->oldndx = oldndx; /* put the index back */
	    cle_flush(lb);
	    return;		/* no more */
	}
    }
    new = match_ptr(lb,new,lb->buf,lb->oldmatlen,&lb->oldndx,NO,direction);
    if (new == 0) {
        cle_putc(BEL,lb);	/* beep 'em cuz there ain't anymore */
	lb->oldndx = oldndx;	/* put the index back */
	if ((lb->flags&LD_DIRTY) == 0 && direction == REVERSE) {
	   lb->lcurs = lb->buf;
	   lb->rcurs = lb->bufend;
	   lb->c_posn = 0;		/* set the column position */
	   pprompt(lb,YES);	/* print the prompt */
	   putt((char *)lb->buf,lb->oldmatlen,lb);
	   lb->oldmatlen = 0;	/* now it's dirty, so zap this */
	}
	cle_flush(lb);
        return;
    }
    pprompt(lb,YES);
    lb->rcurs = lb->bufend;
    lb->lcurs = lb->buf;
    lb->c_posn = 0;		/* set the column position */
    if ((comlen = *new) > 2) putt((char *)new+2,comlen-2,lb);
    lb->flags &= ~LD_DIRTY;
    return;
}

static void getold_str_fwd(LedBuf *lb) { /* find next matching line in history */
    getold_str_dir(lb,FORWARD);
}

static void getold_str_rev(LedBuf *lb) { /* find previous matching line in history */
    getold_str_dir(lb,REVERSE);
}

static void getold_str(LedBuf *lb) { /* find previous matching line in history */
   getold_str_dir(lb,(lb->flags&LD_BACKUP)?REVERSE:FORWARD);
}

static void superquote(LedBuf *lb) {
    putt("?",1,lb);			/* insert a qm */
    c_l(lb);				/* backup 1 spot */
    lb->state = ESCCHAR;		/* set the new state */
    return;
}

static void setcol_width(int siz,LedBuf *lb) {
    if (siz == 0) {			/* set to 80 columns */
        cle_puts(lb->ansi[ANSI_80COL],lb);	/* make it 80 */
        lb->flags &= ~LD_COL132;
    } else {
        cle_puts(lb->ansi[ANSI_132COL],lb);	/* make it 132 */
        lb->flags |= LD_COL132;
    }
    cle_flush(lb);
    return;
}
	
static void t132_mode(LedBuf *lb) {
    if ((lb->flags&LD_COL132) != 0) {	/* if already at 132 mode */
	setcol_width(0,lb);		/* set it to 80 cols */
    } else {
	setcol_width(1,lb);		/* set it to 132 cols */
    }
    reprint(lb);
}

static void nop(LedBuf *lb) {
    return;
}

static void advance(LedBuf *lb) {
   lb->flags &= ~LD_BACKUP;
}

static void backup(LedBuf *lb) {
   lb->flags |= LD_BACKUP;
}

static void skipc(LedBuf *lb) {
   if (lb->flags&LD_BACKUP) {
      c_l(lb);		/* backup, cursor left */
   } else {
      c_r(lb);		/* advance, cursor right */
   }
}

static void skipl(LedBuf *lb) {
   if (lb->flags&LD_BACKUP) {
      getold_prev(lb);	/* backup, cursor up */
   } else {
      getold_next(lb);	/* advance, cursor down */
   }
}

static void ring_bell(LedBuf *lb) {
   cle_putc(BEL,lb);
   cle_flush(lb);
}

static void dochr(LedBuf *);

/* DON'T CHANGE THE ORDER OF THE ENTRIES IN THE FOLLLOWING 3 ARRAYS */

static void (* const edit_functions[])() = {
      dochr,			/* insert a char */
      tog_insrt,		/* toggle insert/overstrike mode */      
      bol,			/* goto bol */
      eol,			/* goto eol */
      d_lwrd,			/* delete "word" to the left */
      d_rwrd,			/* delete "word" to the right */
      d_bol, 			/* delete to beginning of line */
      d_eol,			/* delete to end of line */
      c_l,			/* cursor left */
      c_r,			/* cursor right */
      d_lchr,			/* delete character on left */
      d_rchr,			/* delete character on right */
      reprint,			/* refresh the line */
      getold_prev,		/* get previous */
      getold_next,		/* get next */
      getold_str,		/* get matching string (use direction bit) */
      getold_str_fwd,		/* get matching string forward */
      getold_str_rev,		/* get matching string reverse */
      newline,			/* end of input */
      superquote,		/* escape next char */
      t132_mode,		/* toggle 80/132 mode */
      nop,			/* do nothing */
      advance,			/* clear backup bit */
      backup,			/* set backup bit */
      skipw,			/* skip word (uses direction bit) */
      skipc,			/* skip char (uses direction bit) */
      skipl,			/* skip line (uses direction bit) */
      ring_bell,		/* echo a bell */
      skipwl,			/* skip word right */
      skipwr,			/* skip word left */
      skiptoe,			/* skip to end of line per direction */
      nop,			/* actually purge (trapped at interrupt time) */
      d_wlin,			/* delete whole line */
      0				/* trailer */
};

static const unsigned char fkey_to_clekey[] = {	/* input <esc>[n~ where n is: */
    0,				/* 0 is unassigned */
    CLEKEY_DEL,			/* 1 is (or can be) delete key */
    CLEKEY_INSERT,		/* 2 is insert */
    CLEKEY_HOME,		/* 3 is home */
    CLEKEY_END,			/* 4 is end */
    CLEKEY_PGUP,		/* 5 is page-up */
    CLEKEY_PGDN,		/* 6 is page-down */
    0,0,0,0,			/* 7-10 unassigned */
    CLEKEY_F1,			/* 11 */
    CLEKEY_F2,
    CLEKEY_F3,
    CLEKEY_F4,
    CLEKEY_F5,
    0,				/* 16 unassigned */
    CLEKEY_F6,
    CLEKEY_F7,
    CLEKEY_F8,
    CLEKEY_F9,
    CLEKEY_F10,
    0,				/* 22 unassigned */
    CLEKEY_F11,
    CLEKEY_F12,
    CLEKEY_F13,
    CLEKEY_F14,
    0,				/* 27 unassigned */
    CLEKEY_F15,
    CLEKEY_F16,
    0,				/* 30 unassigned */
    CLEKEY_F17,
    CLEKEY_F18,
    CLEKEY_F19,
    CLEKEY_F20,
    0				/* 35 unassigned */
};

static const unsigned char ckey_to_clekey[] = {
    CLEKEY_F38,			/* @ (all prefixed with <esc>[ )*/
    CLEKEY_UP,			/* A */
    CLEKEY_DOWN,		/* B */
    CLEKEY_RIGHT,		/* C */
    CLEKEY_LEFT,		/* D */
    CLEKEY_END,			/* E */
    CLEKEY_PGUP,		/* F */
    CLEKEY_PGDN,		/* G */
    CLEKEY_HOME,		/* H */
    CLEKEY_INSERT,		/* I */
    CLEKEY_F21,			/* J */
    CLEKEY_F22,			/* K */
    CLEKEY_F23,			/* L */
    CLEKEY_F24,			/* M */
    CLEKEY_F25,			/* N */
    CLEKEY_F26,			/* O */
    CLEKEY_F27,			/* P */
    CLEKEY_F28,			/* Q */
    CLEKEY_F29,			/* R */
    CLEKEY_F30,			/* S */
    CLEKEY_F31,			/* T */
    CLEKEY_F32,			/* U */
    CLEKEY_F33,			/* V */
    CLEKEY_F34,			/* W */
    CLEKEY_F35,			/* X */
    CLEKEY_F36,			/* Y */
    CLEKEY_F37			/* Z */
};

static const unsigned char esc_map[] = {	/* this hack is to support Wyse terminals */
   0,		/* space */		/* all chars are preceeded by an escape */
   0,		/* '!' */
   0,		/* '"' */
   0,		/* '#' */
   0,		/* '$' */
   0,		/* '%' */
   0,		/* '&' */
   0,		/* ''' */
   0,		/* '(' */
   0,		/* ')' */
   0,		/* '*' */
   0,		/* '+' */
   0,		/* ',' */
   0,		/* '-' */
   0,		/* '.' */
   0,		/* '/' */
   0,		/* '0' */
   0,		/* '1' */
   0,		/* '2' */
   0,		/* '3' */
   0,		/* '4' */
   0,		/* '5' */
   0,		/* '6' */
   CLEKEY_F52,	/* '7' */
   0,		/* '8' */
   0,		/* '9' */
   0,		/* ':' */
   0,		/* ';' */
   0,		/* '<' */
   0,		/* '=' */
   0,		/* '>' */
   0,		/* '?' */
   0,		/* '@'*/
   0,		/* 'A' */
   0,		/* 'B' */
   0,		/* 'C' */
   0,		/* 'D' */
   CLEKEY_F39,	/* 'E' */
   0,		/* 'F' */
   0,		/* 'G' */
   0,		/* 'H' */
   CLEKEY_F40,	/* 'I' */
   CLEKEY_F41,	/* 'J' */
   CLEKEY_F42,	/* 'K' */
   0,		/* 'L' */
   0,		/* 'M' */
   0,		/* 'N' */
   0xFF,	/* 'O' */
   CLEKEY_F43,	/* 'P' */
   CLEKEY_F44,	/* 'Q' */
   CLEKEY_F45,	/* 'R' */
   0,		/* 'S' */
   CLEKEY_F46,	/* 'T' */
   0,		/* 'U' */
   0,		/* 'V' */
   CLEKEY_F47,	/* 'W' */
   0,		/* 'X' */
   CLEKEY_F48,	/* 'Y' */
   0,		/* 'Z' */
   0xFE,	/* '[' */
   0,		/* '\' */
   0,		/* ']' */
   0,		/* '^' */
   0,		/* '_' */
   0,		/* '`' */
   0,		/* 'a' */
   0,		/* 'b' */
   0,		/* 'c' */
   0,		/* 'd' */
   0,		/* 'e' */
   0,		/* 'f' */
   0,		/* 'g' */
   0,		/* 'h' */
   0,		/* 'i' */
   0,		/* 'j' */
   0,		/* 'k' */
   0,		/* 'l' */
   0,		/* 'm' */
   0,		/* 'n' */
   0,		/* 'o' */
   0,		/* 'p' */
   CLEKEY_F49,	/* 'q' */
   CLEKEY_F50,	/* 'r' */
   0,		/* 's' */
   0,		/* 't' */
   0,		/* 'u' */
   0,		/* 'v' */
   0,		/* 'w' */
   0,		/* 'x' */
   0,		/* 'y' */
   0,		/* 'z' */
   CLEKEY_F51,	/* '{' */
   0,		/* '|' */
   0,		/* '}' */
   0,		/* '~' */
   0		/* DEL */
};

static const unsigned char hex[] = "0123456789ABCDEF";

static unsigned char *itoa(int val,unsigned char *cptr,int radix) {
    unsigned int quo,rem;
    rem = val % radix;
    quo = val / radix;
    if (quo != 0) cptr = itoa(quo,cptr,radix);
    *cptr++ = hex[rem];
    return cptr;
}

/****************************************************************************
* cled_parse_it is the entry point for the editor. It is called for each character
* input from the keyboard. The process context is task time. It must never
* be called from an interrupt routine.
*/
      STATIC void cled_parse_it(LedBuf *lb) 

/* At exit:
 *	the command line is edited.
 */
{
    int fndx;
   
    if (lb->state == NORMAL) {		/* if ordinary character */
        if (lb->c < SPACE) {		/* if char is control */
 	    if (lb->c == ESC) {		/* escape is special case */
	        lb->state = ESCAPE;	/* signal other stuff coming later */
	        return;
	    }
	    fndx = lb->keymap[lb->c];
	    (*edit_functions[fndx])(lb); /* do the edit function */
	    return;
        } else if (lb->c == RUB) {      /* handle DEL key! */
          fndx = lb->keymap[CLEKEY_DEL];
          (*edit_functions[fndx])(lb);   /* do the edit function */
          return;
        }
        putt((char *)(&lb->c),1,lb);		/* ordinary character */
        return;
    }
    if (lb->state == ESCAPE) {		/* we're in an escape sequence */
	if (lb->c >= ' ' && lb->c < 0x7F) {
	   fndx = esc_map[lb->c - ' '];
	   if (fndx == 0xFE) {
	       lb->state = CURSKEY;	/* next one is cursor key */
	       return;
	   }
	   if (fndx == 0xFF) {		/* if keypad sequence */
	       lb->state = KEYPAD;	/* next one is keypad key */
	       return;
	   }
	   if (fndx != 0) {
	      (*edit_functions[lb->keymap[fndx]])(lb); /* do the function */
	      lb->state = NORMAL;		/* back to normal */
	      return;
	   }
	}
        putt("\033",1,lb);		/* stuff in an escape */
        lb->state = NORMAL;		/* back to normal */
        cled_parse_it(lb);		/* do it again */
        return;
    } 
    if (lb->state == CURSKEY) {		/* if cursor sequence */
	if (lb->c >= '0' && lb->c <= '9') {	/* <esc>[n~ key */
	    lb->keynum = lb->c - '0';	/* compute keynum */
	    lb->state = FUNCKEY;	/* and we're doing function keys */
	    return;			/* wait for rest of escape sequence */
	}
	if (lb->c == '=') {		/* <esc>[=x where a <= x <= l */
	    lb->keynum = '=';
	    lb->state = FUNCKEY;
	    return;
	}
	fndx = lb->c - '@';		/* compute index */
	if (fndx >= sizeof(ckey_to_clekey)) { /* if out of range */
	    putt("\033[",2,lb);		/* bad esc sequence, stuff <esc>[ */
	    lb->state = NORMAL;		/* back to normal */
	    cled_parse_it(lb);		/* do it again */
	    return;
	}
	fndx = ckey_to_clekey[fndx];
	(*edit_functions[lb->keymap[fndx]])(lb); /* do the function */
        lb->state = NORMAL;		/* back to normal */
        return;
    }
    if (lb->state == KEYPAD) {		/* if keypad sequence */
	fndx = -1;			/* assume its nfg */
	if (lb->c == 'M') fndx = CLEKEY_ENTER;
	else if (lb->c >= 'P' && lb->c <= 'S') fndx = lb->c-'P'+CLEKEY_PF1;
	else if (lb->c >= 'l' && lb->c <= 'n') fndx = lb->c-'l'+CLEKEY_KPCOMMA;
	else if (lb->c >= 'p' && lb->c <= 'y') fndx = lb->c-'p'+CLEKEY_KP0;
	if (fndx < 0) {
	    putt("\033O",2,lb);		/* bad escape sequence */
	    lb->state = NORMAL;		/* back to normal */
	    cled_parse_it(lb);		/* reparse */
	    return;
	}
	(*edit_functions[lb->keymap[fndx]])(lb);
	lb->state = NORMAL;		/* back to normal */
	return;
    }
    if (lb->state == FUNCKEY) {		/* doing an F key */
	unsigned char str[6];
	if (lb->c >= '0' && lb->c <= '9') {	/* eat all the numbers */
	    lb->keynum = lb->keynum * 10 + (lb->c - '0');
	    return;
	} else if (lb->c == '~') {	/* end of sequence */
	    if (lb->keynum < sizeof(fkey_to_clekey) && 
			(fndx = fkey_to_clekey[lb->keynum]) != 0 &&
   			 lb->keymap[fndx] != CLEFUN_CHAR) {
		(*edit_functions[lb->keymap[fndx]])(lb);
		lb->state = NORMAL;		/* back to normal */
		return;
	    }
	} else if (lb->keynum == '=') {
	    if (lb->c >= 'a' && lb->c <= 'l') {
		fndx = lb->c - 'a' + CLEKEY_F1;
		if (lb->keymap[fndx] != CLEFUN_CHAR) {
		    (*edit_functions[lb->keymap[fndx]])(lb);
		    lb->state = NORMAL;		/* back to normal */
		    return;
		}
	    }
	    putt("\033[=",3,lb);
	    lb->state = NORMAL;
	    cled_parse_it(lb);
	    return;
	}
	putt("\033[",2,lb);		/* bad esc sequence, stuff <esc>[ */
	putt((char *)str,itoa(lb->keynum,str,10)-str,lb);
	lb->state = NORMAL;
	cled_parse_it(lb);		/* reparse it */
	return;
    }	    
    if ((lb->state&~0xFF) == NCC_SPC) {
       switch(lb->state&0xFF) {
	  case VERASE: d_lchr(lb); break;
	  case VKILL: d_bol(lb); break;
	  case VEOF: lb->flags |= LD_EOF;	/* set eof and fall through to newline */
	  case VEOL: newline(lb); break;
	  case VREPRINT: reprint(lb); break;
	  case VWERASE: d_lwrd(lb); break;
	  case VLNEXT: superquote(lb); return;	/* can't change state on this command */
	  default:
             msg("Unknown key and/or key state",lb);
       }
    } else if (lb->state == ESCCHAR) {
	d_rchr(lb);			/* eat what's under the cursor */
        if (lb->c == 0) {
	    msg("NUL's cannot be inserted in the command line",lb);
	} else {
#if 0
	    if (lb->c == '\n') {	/* \n's are special too */
	       msg("Escaped \\n's may not be recognised by applications",lb);
	    }
#endif
	    putt((char *)(&lb->c),1,lb);		/* insert the char as is */
        }
    } else msg("Unknown key and/or key state",lb);
    lb->state = NORMAL;
    return;
}

/************************************************************************
 * process character (or sequence) that didn't match anything in the list
 * of special characters.
 */
	 static void dochr(
	 LedBuf *lb)		/* ptr to led_buf. lb->c has char to be processed */

/* At exit:
 *	command line edited.
 */
{
    switch (lb->state) {
	case ESCAPE:  putt("\033",1,lb);  break;
	case KEYPAD:  putt("\033O",2,lb); break;
	case CURSKEY: putt("\033[",2,lb); break;
	default:      
           if (lb->c == 0) {
	      msg("NUL's cannot be inserted in the command line",lb);
           } else {
	      putt((char *)(&lb->c),1,lb);
	   }
	   return;
    }
    lb->state = NORMAL;
    cled_parse_it(lb);		/* again because previous escape seq bad */
}
