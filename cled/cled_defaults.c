#if !defined(M_KERNEL)
#include "cled_io.h"
#endif

/***********************************************************************************
 * Set the keymap to defaults.
 */
    	STATIC void cled_setup_key_defaults(LedBuf *lb)
/*
 * At entry:
 *	lb - ptr to LedBuf into which to set the defaults
 * At exit:
 *	keymap initialised in struct 
 */
{
    int cnt;
    unsigned char *chr;
    for (chr = lb->keymap,cnt = 0; cnt<CLEKEY_MAX; ++cnt) {
	*chr++ = CLEFUN_CHAR;			/* default all keys to simply insert in buf */
    }
/******************************************************************************
 * The following are defaults for VT100 and VMS (sort of) type line editing. I
 * set them as would be found on VT100 but you might not want these in
 * your environment.
 */
    lb->keymap[CLEKEY_NULL] = CLEFUN_ESCAPE;	/* escape char */
    lb->keymap[CLEKEY_CTLA] = CLEFUN_INSERT;	/* toggle insert/overstrike mode */
    lb->keymap[CLEKEY_CTLB] = CLEFUN_GOTOBOL;	/* goto bol */
    lb->keymap[CLEKEY_CTLE] = CLEFUN_GOTOEOL;	/* goto eol */
    lb->keymap[CLEKEY_CTLF] = CLEFUN_FIND_FWD;	/* find string */
    lb->keymap[CLEKEY_CTLM] = CLEFUN_NEWLINE;	/* eol */
    lb->keymap[CLEKEY_CTLR] = CLEFUN_REFRESH;	/* repaint */
    lb->keymap[CLEKEY_CTLW] = CLEFUN_132;	/* toggle 80/132 mode */
    lb->keymap[CLEKEY_CTLX] = CLEFUN_PURGE;	/* dele everything */
/*******************************************************************************
 * The following are defaults for VMS and are mostly VT100 specific keys.
 */
#if !defined(WYSE)
    lb->keymap[CLEKEY_CTLH] = CLEFUN_GOTOBOL;	/* goto bol */
    lb->keymap[CLEKEY_CTLJ] = CLEFUN_DELWLFT;	/* dele word left */
    lb->keymap[CLEKEY_UP] = CLEFUN_FIND_FWD;	/* up arrow */
    lb->keymap[CLEKEY_DOWN] = CLEFUN_FIND_REV;	/* down arrow */
    lb->keymap[CLEKEY_LEFT] = CLEFUN_CURSL;	/* left arrow */
    lb->keymap[CLEKEY_RIGHT] = CLEFUN_CURSR;	/* right arrow */
    lb->keymap[CLEKEY_PF1] = CLEFUN_BELL;	/* no gold, ring bell to remind me */
    lb->keymap[CLEKEY_PF2] = CLEFUN_BELL;	/* nothing */
    lb->keymap[CLEKEY_PF3] = CLEFUN_FIND_FWD;	/* find string (same as ^F) */
    lb->keymap[CLEKEY_PF4] = CLEFUN_DELEOL;	/* dele to eol */
    lb->keymap[CLEKEY_KPMINUS] = CLEFUN_DELWRIT; /* del word right */
    lb->keymap[CLEKEY_KPCOMMA] = CLEFUN_DELCRIT; /* del char under cursor */
    lb->keymap[CLEKEY_ENTER] = CLEFUN_NEWLINE;	/* eol */
    lb->keymap[CLEKEY_DOT] = CLEFUN_BELL;	/* nothing */
    lb->keymap[CLEKEY_KP0] = CLEFUN_SKIPL;	/* skip line per direction bit */
    lb->keymap[CLEKEY_KP1] = CLEFUN_SKIPW;	/* skip word per direction bit */
    lb->keymap[CLEKEY_KP2] = CLEFUN_SKIPTOL;	/* goto eol or bol per direction bit */
    lb->keymap[CLEKEY_KP3] = CLEFUN_SKIPC;	/* skip char per direction bit */
    lb->keymap[CLEKEY_KP4] = CLEFUN_ADVANCE;	/* set direction forward */
    lb->keymap[CLEKEY_KP5] = CLEFUN_BACKUP;	/* set direction backward */
    lb->keymap[CLEKEY_KP6] = CLEFUN_BELL;	/* key does nothing */
    lb->keymap[CLEKEY_KP7] = CLEFUN_BELL;	/* key does nothing */
    lb->keymap[CLEKEY_KP8] = CLEFUN_BELL;	/* key does nothing */
    lb->keymap[CLEKEY_KP9] = CLEFUN_BELL;	/* key does nothing */
    lb->keymap[CLEKEY_HOME] = CLEFUN_GOTOBOL;	/* goto begininning of line */
    lb->keymap[CLEKEY_END] = CLEFUN_GOTOEOL;	/* goto end of line */
    lb->keymap[CLEKEY_F12] = CLEFUN_BELL;	/* for test purposes */
#else
/***********************************************************************
 * The following are defaults for WYSE 150 line editing. You will most
 * probably want to change these since I just guessed at what they should be
 * and don't have a WYSE terminal hooked up to my Unix system to test it.
 */
    lb->keymap[CLEKEY_CTLH] = CLEFUN_DELCLFT;	/* BS = delete char to the left */
    lb->keymap[CLEKEY_CTLK] = CLEFUN_PREVIOUS;	/* up arrow */
    lb->keymap[CLEKEY_CTLJ] = CLEFUN_NEXT;	/* down arrow */
    lb->keymap[CLEKEY_CTLH] = CLEFUN_CURSL;	/* left arrow */
    lb->keymap[CLEKEY_CTLL] = CLEFUN_CURSR;	/* right arrow */
    lb->keymap[CLEKEY_DEL]  = CLEFUN_DELCRIT;	/* DEL = delete char under cursor */
    lb->keymap[CLEKEY_F47]  = CLEFUN_DELCLFT;	/* (del char) */
    lb->keymap[CLEKEY_F45]  = CLEFUN_DELEOL;	/* (del line) */
#endif
    return;
}

/***************************************************************************
 * The following init's setup the strings required to make a terminal do the 
 * given functions.
 */
    STATIC void cled_setup_ansi_defaults(LedBuf *lb)
/* 
 * At entry:
 *	lb - ptr to LedBuf into which to stick the defaults 
 * At exit:
 *	ansi[] array filled with defaults 
 */
{
    lb->ansi[ANSI_UP] = ANSI_UP_STR;		/* up-arrow */
    lb->ansi[ANSI_CLREOL] = ANSI_CLREOL_STR;	/* clear to eol */
    lb->ansi[ANSI_CLRLINE] = ANSI_CLRLINE_STR;	/* clear whole line */
    lb->ansi[ANSI_SETINV] = ANSI_SETINV_STR;	/* set inverse video */
    lb->ansi[ANSI_SETNORM] = ANSI_SETNORM_STR;	/* set normal video */
    lb->ansi[ANSI_SAVE] = ANSI_SAVE_STR;	/* save cursor pos and attr's */
    lb->ansi[ANSI_RESTORE] = ANSI_RESTORE_STR;	/* restore cursor pos and attr's */
    lb->ansi[ANSI_MSGEOF] = ANSI_MSGEOF_STR;	/* EOF message */
    lb->ansi[ANSI_MSGINTR] = ANSI_MSGINTR_STR; /* INTR message */
    lb->ansi[ANSI_MSGQUIT] = ANSI_MSGQUIT_STR; /* QUIT message */
    lb->ansi[ANSI_SETUP] = ANSI_SETUP_STR;	/* set terminal to app mode */
    lb->ansi[ANSI_80COL] = ANSI_80COL_STR;	/* set to 80 cols */
    lb->ansi[ANSI_132COL] = ANSI_132COL_STR;	/* set to 132 cols */
    return;
}
