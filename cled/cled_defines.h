#if !defined(_CLED_DEFINES_H_)
#define _CLED_DEFINES_H

/*********************************************************************************
 * The following #defines set the configuration parameters for cled. You may need
 * or want to adjust them to suit the requirements specific to your system.
 *******************************************************************************/

#ifndef HISTBUFSIZ
# define HISTBUFSIZ	3072	/* size of history buffer */
#endif
#ifndef PROMPTBFSIZ
# define PROMPTBFSIZ	  80	/* max # of chars to keep for prompt */
#endif
#ifndef COMBUFSIZ
# define COMBUFSIZ	 256	/* size of command line buffer */
#endif
#ifndef MAX_LIN_WIDTH
# define MAX_LINE_WIDTH   134	/* maximum chars to save in broadcast buff */
#endif

#if defined(GET_LOCAL_DEFINES)
/************ End of user adjustable paramters **********************************/

#define VERSION "2.00"		/* current cled version */

#define LD_DONE	    0x0001	/* flag indicating read complete */
#define LD_QUIT	    0x0002	/* completed under QUIT char */
#define LD_INTR     0x0004	/* completed under INTR char */
#define LD_EOF	    0x0008	/* completed under EOF */
#define LD_DIRTY    0x0010	/* command buffer has been changed */
#define LD_INSERT   0x0020	/* insert mode */
#define LD_BACKUP   0x0040	/* direction bit (0=advance,1=backup) */
#define LD_COL132   0x0080	/* using 132 column mode */
#define LD_ICANON   0x0100	/* cled is enabled */
#define LD_READING  0x0200	/* cled is reading */
#define LD_DEFERECHO 0x0400	/* no deferred echoing (normal Unix mode) */
#define LD_CLEDIO   0x0800	/* flag indicating the the hisio struct is valid */
#define LD_RFLUSH   0x1000	/* flag indicating we got a read flush */
#define LD_DELLFIX  0x2000	/* Dell issues a read before prompt write */
#define LD_SENDTA   0x4000	/* signal to send typeahead data */
#define LD_USRINP   0x8000	/* user typed a key since the last zap_lb */
/* if last flag >= 0x10000, change led_buf.flags from a short to a long */

#define ctlA	001		/* control characters */
#define ctlB    002
#define ctlE	005
#define ctlF    006
#define BEL	007
#define BKSP	010
#define TAB	011
#define LF	012
#define ctlK	013
#define FF	014
#define CR	015
#define ctlP	020
#define XON	021
#define ctlR	022
#define XOFF	023
#define ctlW	027
#define ESC	033
#define SPACE   040
#define RUB	0177
#define NORMAL   0x000
#define ESCAPE  -0x100
#define KEYPAD  -0x200
#define CURSKEY -0x300
#define NCC_SPC -0x400
#define ESCCHAR -0x500
#define FUNCKEY -0x600

#define FLAG unsigned char

#define YES	1		/* some flag states */
#define NO	0
#define DEL	1
#define NODEL	0

/*****************************************************************************
 * The pre-processor variable M_KERNEL is defined when compiling for STREAMS *
 * mode. If it is not defined, then the program will be made in standalone   *
 * mode which is useful for debugging the edit only portion of the program.  *
 *****************************************************************************/

#if !defined(STATIC)
# if defined(M_KERNEL)
#  define STATIC static
# else
#  define STATIC
# endif
#endif

#endif

#endif 	/* _CLED_DEFINES_H_ */
