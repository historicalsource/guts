#if !defined(_CLED_IOCTL_H_)
#define _CLED_IOCTL_H_

enum ansi_names {
   ANSI_UP,
   ANSI_CLREOL,
   ANSI_CLRLINE,
   ANSI_SETINV,
   ANSI_SETNORM,
   ANSI_SAVE,
   ANSI_RESTORE,
   ANSI_MSGEOF,
   ANSI_MSGINTR,
   ANSI_MSGQUIT,
   ANSI_SETUP,
   ANSI_80COL,
   ANSI_132COL,
   ANSI_COUNT
};

#if !defined(WYSE)
#define ANSI_UP_STR		"\033[A"	/* up-arrow */
#define ANSI_CLREOL_STR		"\033[0K"	/* clear to eol */
#define ANSI_CLRLINE_STR	"\r\033[0K"	/* clear whole line */
#define ANSI_SETINV_STR		"\033[7m"	/* set inverse video */
#define ANSI_SETNORM_STR	"\033[0m"	/* set normal video */
#define ANSI_SAVE_STR		"\0337"		/* save cursor pos and attr's */
#define ANSI_RESTORE_STR	"\0338"		/* restore cursor pos and attr's */
#define ANSI_MSGEOF_STR		"*EOF*\r\n"	/* EOF message */
#define ANSI_MSGINTR_STR	"\r\n*INTR*\r\n" /* INTR message */
#define ANSI_MSGQUIT_STR	"\r\n*QUIT*\r\n" /* QUIT message */
#define ANSI_SETUP_STR		"\033="		/* set terminal to app mode */
#define ANSI_80COL_STR		"\033[?3l"	/* set to 80 cols */
#define ANSI_132COL_STR		"\033[?3h"	/* set to 132 cols */
#else
#define ANSI_UP_STR		"\013"		/* rev line feed */
#define ANSI_CLREOL_STR		0		/* clear to eol */
#define ANSI_CLRLINE_STR	"\033R"		/* clear whole line */
#define ANSI_SETINV_STR		"\033G4"	/* set inverse video */
#define ANSI_SETNORM_STR	"\033G0"	/* set normal video */
#define ANSI_SAVE_STR		0		/* save cursor pos and attr's */
#define ANSI_RESTORE_STR	0		/* restore cursor pos and attr's */
#define ANSI_MSGEOF_STR		"*EOF*\r\n"	/* EOF message */
#define ANSI_MSGINTR_STR	"\r\n*INTR*\r\n" /* INTR message */
#define ANSI_MSGQUIT_STR	"\r\n*QUIT*\r\n" /* QUIT message */
#define ANSI_SETUP_STR		0		/* set terminal to app mode */
#define ANSI_80COL_STR		"\033`:"	/* set to 80 cols */
#define ANSI_132COL_STR		"\033`;"	/* set to 132 cols */
#endif

#if defined(M_UNIX) || defined(M_KERNEL)
/* The following structure is passed to cled via an ioctl
 * to assign the key bindings and the ANSI sequences (and other strings)
 * desired for various functions.
 *
 * If the ioctl completes with an error, then the len fields of the 
 * struct will have been changed to an index into the respective buffer
 * at which the error occured (cled sets the value).
 *
 * The key buffer immediately follows the set_key struct and the
 * ANSI buffer immediately follows the key buffer.
 *
 * The key buffer contains pairs of chars; the first is the key number
 * and the second is the function number. The kdbuf_len entry in the
 * set_key struct contains the total number of these pairs of chars.
 * Any of the control keys and/or keypad keys can be set to any function
 * at any time. New definitions replace previous definitions. Keys not
 * explicitly defined in the buffer are left unchanged.
 *
 * The ANSI buffer consists of a stream of null terminated strings
 * preceeded by the number of the sequence to which the string belongs.
 * If any ANSI sequence is defined, then ALL the sequences are first
 * reset to their defaults and then replaced with the new definitions.
 * That is, you can change one or all, but unlike the key definitions,
 * you cannot change just one without affecting all the others.
 */

typedef struct {
   unsigned char key;		/* key number */
   unsigned char func;		/* function to assign to that key */
} SetKey;

typedef struct {
   unsigned char strnum;	/* ansi string number */
   unsigned char string[1];	/* null terminated string */
} SetAnsi;

typedef struct {
   int modes;		/* mode bits */
} SetModes;


#define CLEDIOC			(('C'<<24)|('L'<<16)|('E'<<8))
#define CLEDGETLEDBUF		(1)	/* dump its guts */
#define CLEDGETMODES		(2)	/* get cled modes */
#define CLEDSETMODES		(3)	/* get cled modes */
#define CLEDGETKEYBUF		(4)	/* get keydef buffers */
#define CLEDSETKEYBUF		(5)	/* set keydef buffers */
#define CLEDGETHISTBUF		(6)	/* get history buffer */
#define CLEDSETHISTBUF		(7)	/* set history buffer */
#define CLEDGETANSIBUF		(8)	/* get string buffer */
#define CLEDSETANSIBUF		(9)	/* set the string buffer */
#define CLEDNOISE		(10)	/* set noise flag */

#define NOISE_ENT		0x01	/* announce entry and exit to functions */
#define NOISE_ONOFF		0x02	/* announce on/off functions */
#define NOISE_BRDCST		0x04	/* announce broadcast messages */
#define NOISE_INP		0x08	/* announce input messages */
#define NOISE_DATA		0x10	/* announce M_DATA messages too */
#define NOISE_TASTUFF		0x20	/* announce typeahead details */
#define NOISE_READER		0x40	/* announce read stuff */

/* Error codes returned from ioctl functions */

enum error_names {
ERR_NOTTYBUF =	128,	/* no more ttybufs available */
ERR_NOLEDBUF,		/* no more ledbufs available */
ERR_NOLBASS,		/* no led buf assigned to process */
ERR_BADPARAM,		/* bad paramater value */
ERR_BADIOCTL,		/* bad ioctl function */
ERR_NOCLED		/* cled not installed correctly */
};
#endif

/* Editor functions: (don't change the order of these) */

enum func_names {
CLEFUN_CHAR = 0,	/* insert character into buffer (default) */
CLEFUN_INSERT,		/* toggle insert/overstrike mode */
CLEFUN_GOTOBOL,		/* goto beginning of line */
CLEFUN_GOTOEOL,		/* goto end of line */
CLEFUN_DELWLFT,		/* delete word to left of cursor */
CLEFUN_DELWRIT,		/* delete word to right of cursor */
CLEFUN_DELBOL,		/* delete from cursor to beginning of line */
CLEFUN_DELEOL,		/* delete from cursor to end of line */
CLEFUN_CURSL,		/* move cursor left 1 position */
CLEFUN_CURSR,		/* move cursor right 1 position */
CLEFUN_DELCLFT,		/* delete char left of cursor */
CLEFUN_DELCRIT,		/* delete char under cursor */
CLEFUN_REFRESH,		/* reprint the current line */
CLEFUN_PREVIOUS,	/* recall previous command */
CLEFUN_NEXT,		/* recall next command */
CLEFUN_FIND,		/* find next matching string */
CLEFUN_FIND_FWD,	/* find next matching string */
CLEFUN_FIND_REV,	/* find previous matching string */
CLEFUN_NEWLINE,		/* end of line */
CLEFUN_ESCAPE,		/* "escape" the next character */
CLEFUN_132,		/* toggle between 80 col and 132 col */
CLEFUN_NOP,		/* nop */
CLEFUN_ADVANCE,		/* set direction forward */
CLEFUN_BACKUP,		/* set direction backward */
CLEFUN_SKIPW,   	/* skip word (per direction) */
CLEFUN_SKIPC,		/* skip char (per direction) */
CLEFUN_SKIPL,		/* skip line (per direction) */
CLEFUN_BELL,		/* ring bell */
CLEFUN_SKIPWL,		/* skip word left */
CLEFUN_SKIPWR,		/* skip word right */
CLEFUN_SKIPTOL,		/* skip to bol or eol per direction */
CLEFUN_PURGE,		/* purge all typeahead */
CLEFUN_DELWLINE,	/* delete (kill) whole line */
CLEFUN_MAX		/* number of functions */
};

/* Key definitions: */

enum key_names {
CLEKEY_NULL = 0,	/* null */
CLEKEY_CTLA,		/* control A */
CLEKEY_CTLB,		/* control B */
CLEKEY_CTLC,		/* control C */
CLEKEY_CTLD,		/* control D */
CLEKEY_CTLE,		/* control E */
CLEKEY_CTLF,		/* control F */
CLEKEY_CTLG,		/* control G (bell) */
CLEKEY_CTLH,		/* control H (backspace) */
CLEKEY_CTLI,		/* control I (tab) */
CLEKEY_CTLJ,		/* control J (line feed) */
CLEKEY_CTLK,		/* control K */
CLEKEY_CTLL,		/* control L (form feed) */
CLEKEY_CTLM,		/* control M (carriage return) */
CLEKEY_CTLN,		/* control N */
CLEKEY_CTLO,		/* control O */
CLEKEY_CTLP,		/* control P */
CLEKEY_CTLQ,		/* control Q (xon) */
CLEKEY_CTLR,		/* control R */
CLEKEY_CTLS,		/* control S (xoff) */
CLEKEY_CTLT,		/* control T */
CLEKEY_CTLU,		/* control U (usually kill) */
CLEKEY_CTLV,		/* control V (super quote on some systems) */
CLEKEY_CTLW,		/* control W */
CLEKEY_CTLX,		/* control X */
CLEKEY_CTLY,		/* control Y */
CLEKEY_CTLZ,		/* control Z */
CLEKEY_CTLa,		/* control [ (escape) */
CLEKEY_CTLb,		/* control \ */
CLEKEY_CTLc,		/* control ] */
CLEKEY_CTLd,		/* control ~ */
CLEKEY_CTLe,		/* control ? */
CLEKEY_UP,		/* up arrow */
CLEKEY_DOWN,		/* down arrow */
CLEKEY_RIGHT,		/* right arrow */
CLEKEY_LEFT,		/* left arrow */
CLEKEY_ENTER,		/* keypad enter */
CLEKEY_PF1,		/* PF1 */
CLEKEY_PF2,		/* PF2 */
CLEKEY_PF3,		/* PF3 */
CLEKEY_PF4,		/* PF4 */
CLEKEY_KPCOMMA,		/* KP comma */
CLEKEY_KPMINUS,		/* KP minus */
CLEKEY_DOT,		/* KP period */
CLEKEY_KP0,		/* KP 0 */
CLEKEY_KP1,		/* KP 1 */
CLEKEY_KP2,		/* KP 2 */
CLEKEY_KP3,		/* KP 3 */
CLEKEY_KP4,		/* KP 4 */
CLEKEY_KP5,		/* KP 5 */
CLEKEY_KP6,		/* KP 6 */
CLEKEY_KP7,		/* KP 7 */
CLEKEY_KP8,		/* KP 8 */
CLEKEY_KP9,		/* KP 9 */
CLEKEY_DEL,		/* delete */
CLEKEY_HOME,		/* home key */
CLEKEY_END,		/* end key */
CLEKEY_INSERT,		/* insert key */
CLEKEY_PGUP,		/* page up key */
CLEKEY_PGDN,		/* page down key */
CLEKEY_F1,		/* F key */
CLEKEY_F2,		/* F key */
CLEKEY_F3,		/* F key */
CLEKEY_F4,		/* F key */
CLEKEY_F5,		/* F key */
CLEKEY_F6,		/* F key */
CLEKEY_F7,		/* F key */
CLEKEY_F8,		/* F key */
CLEKEY_F9,		/* F key */
CLEKEY_F10,		/* F key */
CLEKEY_F11,		/* F key */
CLEKEY_F12,		/* F key */
CLEKEY_F13,		/* F key */
CLEKEY_F14,		/* F key */
CLEKEY_F15,		/* F key */
CLEKEY_F16,		/* F key */
CLEKEY_F17,		/* F key */
CLEKEY_F18,		/* F key */
CLEKEY_F19,		/* F key */
CLEKEY_F20,		/* F key */
CLEKEY_F21,		/* <esc>[J (these are not assigned to particular keys) */
CLEKEY_F22,		/* <esc>[K */
CLEKEY_F23,		/* <esc>[L */
CLEKEY_F24,		/* <esc>[M */
CLEKEY_F25,		/* <esc>[N */
CLEKEY_F26,		/* <esc>[O */
CLEKEY_F27,		/* <esc>[P */
CLEKEY_F28,		/* <esc>[Q */
CLEKEY_F29,		/* <esc>[R */
CLEKEY_F30,		/* <esc>[S */
CLEKEY_F31,		/* <esc>[T */
CLEKEY_F32,		/* <esc>[U */
CLEKEY_F33,		/* <esc>[V */
CLEKEY_F34,		/* <esc>[W */
CLEKEY_F35,		/* <esc>[X */
CLEKEY_F36,		/* <esc>[Y */
CLEKEY_F37,		/* <esc>[Z */
CLEKEY_F38,		/* <esc>[@ */
/* The following are mapped to Wyse terminal native mode escape sequences */
CLEKEY_F39,		/* <esc>E (ins line) or (shift PF1) */
CLEKEY_F40,		/* <esc>I (shift tab) */
CLEKEY_F41,		/* <esc>J (prev page) or (shift line feed) */
CLEKEY_F42,		/* <esc>K (page nxt) or (line feed) */
CLEKEY_F43,		/* <esc>P (print) or (shift del) */
CLEKEY_F44,		/* <esc>Q (ins char) or (PF1) */
CLEKEY_F45,		/* <esc>R (del line) or (shift PF2) */
CLEKEY_F46,		/* <esc>T (clr line) or (PF3) */
CLEKEY_F47,		/* <esc>W (del char) or (PF2) */
CLEKEY_F48,		/* <esc>Y (clr scn) or (shift PF3) */
CLEKEY_F49,		/* <esc>q (ins) or (shift PF4) */
CLEKEY_F50,		/* <esc>r (repl) or (PF4) */
CLEKEY_F51,		/* <esc>{ (shift home) */
CLEKEY_F52,		/* <esc>7 (send) or (delete) */
CLEKEY_F53,		/* */
CLEKEY_F54,		/* */
CLEKEY_F55,		/* */
CLEKEY_F56,		/* */
CLEKEY_F57,		/* */
CLEKEY_F58,		/* */
CLEKEY_F59,		/* */
CLEKEY_F60,		/* */
CLEKEY_F61,		/* */
CLEKEY_F62,		/* */
CLEKEY_F63,		/* */
CLEKEY_F64,		/* */
CLEKEY_MAX		/* size of key defines */
};

/* default modes */

enum mode_names {
CLEMODE_INSERT=	0x01,	/* insert mode */
CLEMODE_OVER=   0x02,	/* overstrike mode */
CLEMODE_80=	0x04,	/* 80 column mode */
CLEMODE_132=	0x08,	/* 132 column mode */
CLEMODE_EDEFER= 0x10,	/* defer echo mode */
CLEMODE_EIMMED= 0x20	/* don't defer echo */
};

#endif 	/* _CLED_IOCTL_H_ */
