#include <config.h>
#define GET_LOCAL_DEFINES
#include <cled_io.h>
#include <stdio.h>
#include <uart.h>

void cled_putc(char c, LedBuf *lb) {
    char str[2];
    str[0] = c;
    fwrite(str, 1, 1, stderr);
}

void cled_puts(char *s, LedBuf *lb) {
    fwrite(s, strlen(s), 1, stderr);
}

void cled_flush(LedBuf *lb) {
    return;
}

extern int getch(void);

int cled_getc(LedBuf *lb) {
    return getch();
}

extern void cled_setup_key_defaults(LedBuf *);
extern void cled_setup_ansi_defaults(LedBuf *);

static int been_setup;
static void cled_setup(LedBuf *lb) {
   been_setup = 1;
   lb->cledio.c_cc[VINTR] = 'C'&0x1f;
   lb->cledio.c_cc[VQUIT] = 'Y'&0x1f;
   lb->cledio.c_cc[VERASE] = 'H'&0x1f;		/* delete character left of cursor */
   lb->cledio.c_cc[VKILL] = 'U'&0x1f;
   lb->cledio.c_cc[VEOF] = 'Z'&0x1f;
   lb->cledio.c_cc[VEOL] = 'M'&0x1f;
   lb->old[0] = 2;
   lb->old[1] = 0;
   lb->old[2] = 0;
   lb->old[3] = 2;
   cled_setup_key_defaults(lb);
   cled_setup_ansi_defaults(lb);
   lb->keymap[CLEKEY_DEL] = CLEFUN_DELCRIT;	/* delete under cursor */
#if 0
   cled_puts("Outputting controls\r\n",lb);
#endif
   cled_puts((char *)lb->ansi[ANSI_SETUP],lb);
}

static int cledread(LedBuf *lb) {
   uchar c,*cp;
   int len;
   
   lb->maxlin = COMBUFSIZ-1;		/* max length of command line (cannot be > 255) */
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
   while ((lb->flags&LD_DONE) == 0) {	/* loop until a whole line is typed in */
      c = cled_getc(lb);
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
   } 					/* for all input chars */
   cp = lb->lcurs;			/* point to end of command line */
#if 0
   if ((lb->flags&LD_EOF) == 0) {
      *cp++ = '\n';			/* stuff in an EOL if _not_ terminated via EOF */
      lb->lcurs = cp;
   }
#endif
   len = cp-lb->buf;
   *cp = 0;				/* and a trailing null */
   lb->prmptsz = 0;			/* kill any existing prompt */
   lb->prmpt[0] = 0;
   return len;
}

LedBuf llb;
extern void strncpy(char *, const char *, int);

char *diag_get_line(const char *prompt) {
    LedBuf *lb = &llb;
    if (!been_setup) cled_setup(lb);
    if (prompt) {
	strncpy((char *)lb->prmpt, prompt, PROMPTBFSIZ-1);
	cled_puts((char *)lb->prmpt, lb);
    } else {
	lb->prmpt[0] = 0;
    }
    cledread(lb);
    return (char *)lb->buf;
}
