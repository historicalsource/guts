#ifndef OUTPUT_SIZE
#define OUTPUT_SIZE	500	/* output buffer (used only to save network bandwidth) */
#endif
#ifndef MAX_CLEDS
#define MAX_CLEDS	1	/* set to the maximum number of threads (connections) allowed at the same time */
#endif
#ifndef USE_FUNC
#define USE_FUNC	0	/* if not zero, assign a completion routine to handle input */
#endif

#include <cled_proto.h>

typedef struct {
   IcelessIO *iop;			/* all iceless I/O goes here */
   int tain;				/* put text into tabuf starting here */
   int taout;				/* extract text from tabuf starting here */
   int oindx;				/* put text into output starting here */
   int tossed;				/* number of tossed packets */
   LedBuf cled;				/* command line editor stuff */
   unsigned char tabuf[COMBUFSIZ];	/* a small typeahead buffer */
   unsigned char input0[COMBUFSIZ];	/* network input buffers */
   unsigned char input1[COMBUFSIZ];
   unsigned char output[OUTPUT_SIZE];	/* network output buffer */
} CledPkts;

static CledPkts packets[MAX_CLEDS];
static int max_cleds;			/* keeps track of next available entry in packets */

/******************************************************************************************
 * cled_flush() - flushes any output saved up in the output buffer and forces output over
 * the network.
 *
 * At entry:
 *	lb - points to the LedBuf struct which (indirectly) contains the output
 * At exit:
 *	output flushed to network via pktQueSend.
 * Returns:
 *	nothing
 */

void cled_flush(LedBuf *lb) {
   CledPkts *clps;
   if (!lb) return;			/* no pointer */
   clps = (CledPkts *)lb->user;		/* point to our output buffer */
   if (clps->iop && 			/* the channel is open */
       clps->iop->pkt.to &&		/* there's a place to write */
       clps->oindx) {			/* and there's something to write */
       iio_write(clps->iop, (char *)clps->output, clps->oindx);	/* write it */
       iio_flush(clps->iop);
   }
   clps->oindx = 0;			/* buffer is now empty */
}

/******************************************************************************************
 * cled_putc() - put a character into output buffer. This is a support routine for and is used
 * heavily by cled, but anyone else is free to use it too.
 *
 * At entry:
 *	c  - character to write
 *	lb - pointer to the LedBuf struct which (indirectly) contains the output
 * At exit:
 *	Character saved in output buffer. Buffer may have been flushed if it became full
 *	as a result of the save.
 * Returns:
 *	nothing
 */

void cled_putc(char c, LedBuf *lb) {
   CledPkts *clps;
   if (!lb) return;
   clps = (CledPkts *)lb->user;		/* point to our output buffer */
   clps->output[clps->oindx++] = c;	/* copy the character */
   if (clps->oindx >= OUTPUT_SIZE) cled_flush(lb);
   return;				/* return ok */
}

/******************************************************************************************
 * cled_puts() - puts a null terminated string (but not the null) in the output buffer. This is
 * a support routine for and is used heavily by cled, but anyone else is free to use it too.
 *
 * At entry:
 *	s  - pointer to null terminated string
 *	lb - points to the LedBuf struct which (indirectly) contains the output
 * At exit:
 *	String copied to output buffer. Buffer may have been flushed (as many times
 *	as it took) if it became full as a result of the copy.
 * Returns:
 *	nothing
 */

void cled_puts(char *s, LedBuf *lb) {
   CledPkts *clps;
   int len;
   if (!lb) return;
   clps = (CledPkts *)lb->user;		/* point to our output buffer */
   len = strlen(s);			/* length of message */
   while (len > 0) {
      int tlen;
      tlen = OUTPUT_SIZE - clps->oindx;
      if (tlen > len) tlen = len;
      memcpy(clps->output+clps->oindx, s, tlen);
      s += tlen;
      len -= tlen;
      clps->oindx += tlen;
      if (clps->oindx >= OUTPUT_SIZE) cled_flush(lb);
   }
}

/******************************************************************************************
 * cled_netinput() - Decodes packet arriving from the network. This routine should be suitable
 * for use as a completion routine as it is. This function is meant to be called by support
 * routines found in this file or by the packet reception code.
 *
 * At entry:
 *	pkt - points to the PktIOStruct which contains the incoming packet
 * At exit:
 *	As much of the incoming text as would fit has been deposited into the typeahead
 *	buffer. If the typeahead buffer did become full, this routine will have sent an
 *	ICEnak reply back to the sender along with an ASCII message including a bell
 *	code indicating same.
 * Returns:
 *	nothing
 */

static void cled_netinput(PktIOStruct *pkt) {
   CledPkts *clps;
   LedBuf *lb;
   int len;
   unsigned char *bp;
   IcelessIO *iop;
   PktIOStruct *opkt;

   clps = (CledPkts *)pkt->user;
   lb = &clps->cled;
   iop = clps->iop;
   opkt = &iop->pkt;
   if (opkt->to == 0) {			/* this is the first receive packet */
      opkt->to = pkt->from;		/* remember which channels we need to use for output */
      opkt->from = pkt->to;
      if (lb->ansi[ANSI_SETUP]) {
	  cled_puts((char *)lb->ansi[ANSI_SETUP],lb);	/* and sent the startup message */
	  cled_flush(lb);			/* and blast it */
      }
   }
   if (pkt->len > 2 && pkt->buf[PKTcmd] == ICEreply) {
      pkt->len -= 2;			/* subtract two from the actual packet length */
      bp = pkt->buf+PKTcmd+1;		/* point to actual user data */
      len = COMBUFSIZ-clps->tain;		/* how much space from tain to end of buffer */
      if (clps->tain < clps->taout) len = clps->taout - clps->tain;	/* limit it to where taout is */
      if (len > pkt->len) len = pkt->len;	/* limit it to the amount in the packet */
      memcpy(clps->tabuf+clps->tain, bp, len);	/* copy the message to the typeahead buffer */
      clps->tain += len;			/* advance the input */
      bp += len;				/* advance the buffer pointer */
      pkt->len -= len;
      if (clps->tain >= sizeof(clps->tabuf)) {	/* if we ran off the end of the buffer */
	 clps->tain = 0;			/* modulo tain to sizeof buffer */
	 len = clps->taout - clps->tain;	/* compute distance to output index */
	 if (pkt->len && len > 0) {		/* if there's still more to copy and there's room in the buffer */
	    memcpy(clps->tabuf, bp, len);	/* copy as much as we can. */
	    clps->tain += len;			/* move the in pointer */
	    pkt->len -= len;			/* take from total */
	 }
      }
      if (pkt->len) {			/* ta buffer is full, so echo a nack */
#define TA_FULL_MSG "Typeahead full\007\n"
	 iio_write(iop, TA_FULL_MSG, sizeof(TA_FULL_MSG)-1);
	 iio_flush(iop);
      }
   } else {
      if (pkt->len > 2) ++clps->tossed;
   }
   pktQueRecv(pkt);			/* requeue the packet for receive */
   return;
}

/******************************************************************************************
 * cled_process_char() - Processes an individual incoming character handling the special case
 * stuff not directly handled by cled. This function is meant only to be called by supporting
 * routines also found in this file.
 *
 * At entry:
 *	c  - is the character to process.
 *	lb - points to the LedBuf struct which contains the editor stuff.
 * At exit:
 *	character is handled. lb->flags will have been updated to reflect the new
 *	condition of the edit buffer.
 * Returns:
 *	nothing
 */

static void cled_process_char(int c, LedBuf *lb) {
   if (lb->state == NORMAL && c != 0) {
      if (c == lb->cledio.c_cc[VEOF]) {	/* got an EOF? */
	 lb->flags |= LD_EOF;	/* say we got an EOF */
	 lb->state = NCC_SPC|VEOL; /* and eol */
	 c = 0;
      } else if (c == lb->cledio.c_cc[VERASE]) {	 /* erase? */
	 lb->state = NCC_SPC|VERASE; /* signal special mode */
	 c = 0;
      } else if (c == lb->cledio.c_cc[VKILL]) { /* line kill? */
	 lb->state = NCC_SPC|VKILL;
	 c = 0;
      } else if ((lb->cledio.c_cc[VEOL] == 0 && c == CR) || 
		  (c == lb->cledio.c_cc[VEOL])) { /* end of line? */
	 lb->state = NCC_SPC|VEOL;
	 c = 0;
      } else  if (c == lb->cledio.c_cc[VINTR] || c == lb->cledio.c_cc[VQUIT]) {
	 lb->flags |= LD_EOF;	/* say we got an EOF */
	 lb->state = NCC_SPC|VEOL; /* and eol */
	 c = 0;
      }
   }
   lb->c = c;			/* pass the char */
   cled_parse_it(lb);		/* parse what we've got */
   cled_flush(lb);		/* flush any echo */
}

/******************************************************************************************
 * zero_cled() - init the cled elements at start of entry. This function is meant to be
 * called when a new line is expected to be input and should only be used by the support
 * routines found elsewhere in this file.
 *
 * At entry:
 *	lb - points to the LedBuf struct which will contain the input
 * At exit:
 *	key members of the lb struct have been initialised
 * Returns:
 *	nothing
 */

static void zero_cled(LedBuf *lb) {
   lb->maxlin = COMBUFSIZ-1;		/* max length of command line (can never be > 255) */
   lb->end_posn = lb->maxlin - 1;	/* set the max pointer */
   lb->lcurs = lb->buf;			/* set the left side ptrs */
   lb->rcurs = lb->bufend = lb->buf + lb->end_posn; /* and right side ptrs */
   lb->state = NORMAL;			/* start out normal */
   lb->oldndx = 0;			/* reset the index to the history file */
   lb->oldmatlen = 0;			/* for match history */
   lb->flags &= ~(LD_DONE|LD_QUIT|LD_INTR|LD_EOF|LD_BACKUP|LD_INSERT);
   lb->flags |= (LD_DIRTY);		/* buffer is dirty and insert mode */
   lb->flags |= LD_INSERT; 		/* set default mode */
   lb->c_posn = 0;			/* at left side */
   *lb->lcurs = *lb->rcurs = 0;		/* put nulls at both ends of the buffer */
}

/******************************************************************************************
 * cled_readline() - Read a whole line from the network. This function is intended to be the
 * main interface between cled and the user's program. It returns 0 if no line is ready to
 * receive and 1 if a whole line has been received. This function is a sort of "Poll for
 * input" except it will hand over a complete edited line if one is preset. The calling of
 * this function frequently is _required_ to get command line editing to work properly.
 *
 * At entry:
 *	lb - points to the LedBuf struct which will contain the input
 * At exit:
 *	Some input may have been sampled and read from the network (one or more pktPoll()'s
 *	will have been executed). If a EOL character is among the received data, this function
 *	will return true (1) indicating same. The lb->buf member points to the edited string.
 *	If no EOL character is received, the function returns 0. 
 * Returns:
 *	TRUE (1) if the line pointed to by lb->buf is complete else FALSE (0).
 */

int cled_readline(LedBuf *lb) {		/* wait for a whole line to show up */
   CledPkts *clps;
   PktIOStruct *pkt;

   if (!pktPoll || !lb) return 0;	/* stub not loaded or null ptr, he can't have any */
   clps = (CledPkts *)lb->user;
   if (lb->eflags == 0) {
      zero_cled(lb);			/* init some stuff on the first time through */
      lb->eflags = 1;
   }      
   while (1) {
      int c;
      if (!clps->iop->ipkt0.func) {	/* if not to service this function via AST */
	  pkt = pktPoll(0, PKT_CHK_THREAD, 0, clps->iop->ipkt0.tothread);
	  if (pkt == 0) break;
	  if (clps->tain == clps->taout) continue;	/* typeahead is full, nothing to do, eat the packet */
	  cled_netinput(pkt);			/* process the packet */
      } else {
          pkt = 0;
      }
      while (clps->tain != clps->taout) {	/* as long as there is data in the typeahead buffer */
	  c = clps->tabuf[clps->taout];
	  clps->taout += 1;
	  if (clps->taout >= COMBUFSIZ) clps->taout = 0;
	  cled_process_char(c, lb);
	  if ((lb->flags&LD_DONE) != 0) {	/* got a complete line */
	     lb->eflags = 0;		/* remember to init the next time through */
	     return 1;			/* and tell the user there is a line present */
	  }      
      }
      if (pkt == 0) break;
   }
   return 0;				/* no line present */
}

/******************************************************************************************
 * cled_open() - Initialise a network connection and return pointers to the cled and packet
 * I/O structures required to use the cled system. This function must be called _once_ for
 * each different thread number wanted by the programmer to get a pointer to the LedBuf.
 * A pointer to a LedBuf is required by all the other cled I/O functions. This is equivalent
 * to a file open. It is not necessary to close (pressing your system's reset button will
 * do that for you).
 *
 * At entry:
 *	thread - the integer thread number (1-254) with which to identify incoming and outgoing
 *		packets. Thread numbers 0 and 255 are reserved for use by gdb.
 * At exit:
 *	Structures have been initialised, although the network connection will not complete
 *	until the first packet arrives over the network with a destination thread number that
 *	matches the one specified. For this reason, output written will be discarded until
 *	the first packet arives from the network.
 * Returns:
 *	Pointer to LedBuf to use in all subsequent I/O operations wrt this thread or 0 if
 *	thread number already in use or all LedBuf pointers have already been issued.
 */

LedBuf *cled_open(int thread) {		/* init the cled stuff and establish a network connection */
   LedBuf *lb;
   PktIOStruct *pkt;
   CledPkts *clps;
   IcelessIO *iop;

   if (pktQueRecv == 0) return 0;	/* network s/w not loaded, he can't have any */
   if (max_cleds >= MAX_CLEDS) return 0; /* already have the max */
   iop = iio_open(thread);		/* open an iceless connection */
   if (iop == 0) return 0;		/* can't */
   clps = packets+max_cleds;		/* point to next entry to use */
   clps->iop = iop;			/* iceless I/O */
   ++max_cleds;				/* spend it */
   lb = &clps->cled;			/* get pointers to sub-structs */
   pkt = &iop->ipkt0;
   pkt->buf = clps->input0;		/* change input buffer */
   pkt->size = COMBUFSIZ;
   pkt->user = (void *)clps;		/* point back to ourself so async I/O knows what to do */
   pkt->func = cled_netinput;		/* network read completion routine */
   pkt = &iop->ipkt1;			/* init the packet(s) */
   pkt->buf = clps->input1;		/* change input buffer */
   pkt->size = COMBUFSIZ;
   pkt->user = (void *)clps;		/* point back to ourself so async I/O knows what to do */
   pkt->func = cled_netinput;		/* network read completion routine */
   lb->cledio.c_cc[VINTR] = 'C'&0x1f;	/* init the special chars */
   lb->cledio.c_cc[VQUIT] = 'Y'&0x1f;
   lb->cledio.c_cc[VERASE] = 0x7f;
   lb->cledio.c_cc[VKILL] = 'U'&0x1f;
   lb->cledio.c_cc[VEOF] = 'Z'&0x1f;
   lb->old[0] = 2;			/* init the history buffer */
   lb->old[1] = 0;
   lb->old[2] = 0;
   lb->old[3] = 2;
   cled_setup_key_defaults(lb);
   cled_setup_ansi_defaults(lb);
   lb->user = (void *)clps;		/* point back to ourself so I/O routines can know what to do */
   return lb;
}

