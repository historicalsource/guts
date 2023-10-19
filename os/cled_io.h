#if !defined(_CLED_STRUCTS_H_)
#define _CLED_STRUCTS_H_

#if defined(M_UNIX) || defined(M_KERNEL)
# include "cled_ioctl.h"
# include "cled_defines.h"
# include "cled_proto.h"
# include <sys/termio.h>
# include <sys/termios.h>
# include <sys/types.h>
# include <sys/param.h>
# include <sys/sysmacros.h>
# include <sys/stream.h>
# include <sys/stropts.h>
#  if defined(DELL_UNIX)
#    include <sys/tty.h>
#    include <sys/conf.h>
#  endif
# include <sys/errno.h>
#else
# include <cled_ioctl.h>
# include <cled_defines.h>
# include <cled_termios.h>
#endif

#if defined(M_KERNEL)
# include <sys/var.h>
# include <sys/dir.h>
# include <sys/signal.h>
# include <sys/user.h>
# include <sys/kmem.h>
# if defined(ftop)
#  undef ftop
# endif
# include <sys/ioctl.h>
# include <sys/file.h>
# include <sys/cmn_err.h>
#endif

typedef unsigned char uchar;
#if !defined(M_UNIX) && !defined(M_KERNEL)
typedef unsigned char ushort;
#endif

#if defined(PRAGMA_PACKED_OK)
# pragma pack(1)
#endif

#if defined(M_KERNEL) || defined(CLED_IOCTL)
#define TA_BUFF_SIZE (1<<3)	/* number of entries in the TA_BUFF */

typedef struct ta_buff {
   mblk_t *cooked[TA_BUFF_SIZE];
   mblk_t *raw[TA_BUFF_SIZE];
   int input;
   int output;
} TA_buff;
#endif

typedef struct led_buf {
   struct termios cledio;	/* our io flags */
   uchar *bufend; 		/* ptr to end of working command buffer */
   uchar *lcurs;		/* left cursor postion */
   uchar *rcurs;		/* right cursor postion */
   char *ansi[ANSI_COUNT];	/* place for escape sequence ptrs */
#if !defined(M_UNIX) && !defined(M_KERNEL)
   void *user;			/* reserved for use by the caller */
   int eflags;			/* used for async I/O */
#endif
#if defined(M_KERNEL) | defined(CLED_IOCTL)
   mblk_t *broadcast;		/* any broadcast message text */
   mblk_t *echob;		/* echo buffer */	
   mblk_t *ta_first;		/* first entry in the ta buff chain */
   mblk_t *ta_last;		/* last (current) entry in the ta buff chain */
   mblk_t *memp;		/* ptr to mblk containing this LedBuf */
   mblk_t *ansib;		/* ptr to ansi buffer block */
   queue_t *wqp;		/* write queue ptr */
   queue_t *rqp;		/* read queue ptr */
   int echo_size;		/* size remaining in echo buffer */   
   uint ioctl_id;		/* id of ioctl message */
   uid_t ruid;			/* user's real uid during open */
#endif
   int noise;			/* debug noise flag */
   int maxlin;			/* max length of input */
   int oldndx;			/* index into old com area for recall */
   int oldmatlen;		/* length of match string entered */
   int indx;			/* index of current text */
   int tmpsize;			/* temp buffer size */
   int rqstlen;			/* read request length */
   short ansi_bufsize;		/* number of bytes in the ansi buffer */
   short flags;			/* ld flags */
   short modes;			/* steady state modes */
   short c_posn;		/* terminal cursor column address */
   short end_posn;		/* terminal eol column address */
   short state;			/* current state */
   ushort keynum;		/* function key number */
   uchar keymap[CLEKEY_MAX];	/* key mappings */
   uchar buf[COMBUFSIZ];	/* working command buffer */
   uchar prmpt[PROMPTBFSIZ];	/* prompt string held here */
   uchar old[HISTBUFSIZ]; 	/* command history located in this buffer */
   uchar c;			/* current char */
   uchar lastc;			/* character last time */
   uchar prmptsz;		/* length of prompt string */
   uchar signal;		/* send signal message upon reciept of FLUSHW */
} LedBuf;

#if defined(PRAGMA_PACKED_OK)
# pragma pack()
#endif

#if !defined(__GNUC__)
# if defined(NULL)
#  undef NULL
#  define NULL 0
# endif
#endif
#define NULLP ((char *)0)

#if !defined(NO_CLEDINIT)
#define NO_CLEDINIT 0
#endif

#if NO_CLEDINIT
static int cledinit(void);
#endif

#if defined(M_UNIX)
# if !defined(M_KERNEL)
#  include <stdio.h>
#  if !defined(CLED_IOCTL)
#   define cle_puts(str,lb) fputs((char *)(str),stderr)
#   define cle_putc(chr,lb) fputc((char)(chr),stderr)
#   define cle_getc(lb) fgetc(stdin)
#  endif
# else
#  define CLE_FUNC_TYPE static
# endif
# define cle_flush(x)
#else
# define CLE_FUNC_TYPE extern
  CLE_FUNC_TYPE void cle_flush(LedBuf *);		/* flush the output */
#endif

#define min(a,b) ((a) < (b) ? (a) : (b))

#if !defined(M_UNIX) && !defined(M_KERNEL)
# include <cled_proto.h>
  extern void cled_parse_it(LedBuf *lb);
  extern void cled_setup_key_defaults(LedBuf *lb);
  extern void cled_setup_ansi_defaults(LedBuf *lb);
# define cle_flush(x) cled_flush(x)
# define cle_putc(x, y) cled_putc(x, y)
# define cle_puts(x, y) cled_puts(x, y)
#endif

#define Oldend(lb) (lb->key-1)

#endif		/* _CLED_STRUCTS_H_ */
