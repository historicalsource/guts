/*		eedrive.c
 *		Copyright 1991,1992,1993,1994 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
*/
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: eedrive.c,v 1.19 1997/07/01 22:23:15 albaugh Exp $";
#endif
/*	This is a "completely C" driver for eeprom/nvram/etc. The lowest-level
*	stuff in assembly is included here as eer_write_byte(), eliminating
*	the need for the file eeprom.asm, which was formerly auto-magically
*	produced from Heeprom.mac and hll68kp.mac.
*
*	This version, in particular, has been re-worked to deal with
*	variable-length records.
*/
#include <config.h>
#include <string.h>
#include <os_proto.h>

/* the files "stat_defs.h" and "stat_map.h" are generated from the user's
*  "stat_map.mac" file. The former contains definitions "distilled" from
*  The entirety of that file, while the latter contains (generally) lines
*  corresponding to individual DEF_xxx statements. "stat_map.h" is #included
*  several times below, each time with different definitions for the macros
*  it contains. In this way, descriptor tables for the various classes
*  of entities that may be stored in eeprom are built.
*/
#ifdef PSX_GAME
#include <stat_def.h>	/* Bill Gates strikes again! */
#else
/* We need to hide the following line from Frank Kwan's lame
 * dependancy builder, which does not understand #if, so it probably
 * also doesn't implement correct CPP syntax.
 */
/* Real compiler will see this line */ # include <stat_defs.h>
#endif

/* The first table cross_references stat_numbers (as defined by EER_xxx
*  in "eer_defs.h") with position and length for the various "scalar" or
*  "simple" stats. These are complicated somewhat by the practice of
*  keeping the LSByte of any "fast" stats in a movable "bucket", to equalize
*  "wear" on the eeprom.
*/
#define DESC_HISTOGRAM(bins,first,later,legend,posn,ram_offs,len,sibs)/* nada */
#define DESC_HST(Total_ents,Perm_ents,Score_bytes,Init_bytes,posn,ram_offs,len)
#define DESC_USER(posn,ram_posn,len) /* nothing */

/*	Here are the shadow locations for the "ordinary" statistics, ala
*	System I.
*/

static unsigned char home[HOME_BYTES];
#ifdef RUN_BYTES
static unsigned char run[RUN_BYTES];
#endif

static const MAP_TYPE stat_map[] = {
#include <stat_map.h>
};

/* Now re-define the "active" macros to do nothing. */
#undef DEF_CTR
#undef DEF_OPT
#define DEF_CTR(Home_size,Home_posn,Run_posn,label) /* nothing */
#define DEF_OPT(Home_size,Home_posn,Run_posn,label) /* nothing */

/* The next table describes the High-Score tables (if any). These are
*  arrays of (initials,score) pairs, stored with the highest score
*  first. Routines are provided to read a given entry (eer_hstr()),
*  insert an entry (eer_hstw()), and find the proper place for an entry
*  (eer_rank()) below.
*/

#ifdef HST_TABLES
#undef DESC_HST
#define DESC_HST(Total_ents,Perm_ents,Score_bytes,Init_bytes,posn,ram_offs,len)\
    { Total_ents,Perm_ents,Score_bytes,Init_bytes},
static const struct score_descrip {
    unsigned short entries;	/* total entries of this table */
    unsigned short perm;	/* first "perm" entries go in eeprom */
    unsigned short Score_bytes;	/* For one entry */
    unsigned short Init_bytes;	/* For one entry */
} high_scores[HST_TABLES] = {
#include <stat_map.h>
    };

static void init_hst();
static unsigned char hst_shadow[HST_BYTES];

#undef DESC_HST
#define DESC_HST(Total_ents,Perm_ents,Score_bytes,Init_bytes,posn,ram_offs,len)
#endif /* HST_TABLES */

/* The next table describes the Histograms (if any). These are
*  arrays of one-byte "bins", with provision for stating the size of the first
*  and later "bins". A call to eer_tally_hist will use these to determine which
*  bin to increment. The "sibs" entry indicates (with a bit-map) which
*  histograms are "tied" to each other and should therefore be scaled whenever
*  any one of them is.
*  A "legend" is included to indicate the title of the histogram, and optionally
*  to provide labels for the left edge of the display.
*/

#ifdef HIST_TABLES
#undef DESC_HISTOGRAM
#define DESC_HISTOGRAM(bins,first,later,legend,posn,ram_offs,len,sibs) \
    { legend, first, later, sibs, bins },
static const struct histo_descrip {
    char *legend;
    unsigned long first;
    unsigned long later;
#if HIST_TABLES > 16
    unsigned long sibs;
#else
    unsigned short sibs;
#endif
    unsigned char bins;
} histo_table[HIST_TABLES] = {
#include <stat_map.h>
    };

static void init_histograms();
static unsigned char hist_shadow[HISTOGRAM_BYTES];

#undef DESC_HISTOGRAM
#define DESC_HISTOGRAM(bins,first,later,legend,posn,ram_offs,len,sibs)/* nada */
#endif /* HIST_TABLES */

/* The next table describes the "User records" (if any). These are
*  the most amorphous of the managed records. Each has a ram shadow
*  and an assigned position in eeprom. All other structure is up to
*  the user.
*/

#ifdef USER_RECS
static unsigned char user_shadows[USER_BYTES];
static void init_user();
#else
#define USER_RECS (0)
#define FIRST_USER_REC (0)
#endif /* USER_RECS */

/* This all gets put together in a "master index" of records. Each entry
*  describes one record (region of eeprom). The first one is "special",
*  in that it has no real "position" (offset in eeprom), but "floats"
*  in whatever space is left over after everything else is allocated. This
*  is because we want to equalize the "wear" on all portions of eeprom.
*
*  The macro definitions below create index entries for each of the non-
*  stat records.
*/
#undef DESC_HISTOGRAM
#define DESC_HISTOGRAM(bins,first,later,legend,posn,ram_offs,len,sibs)\
    { FIRST_HISTOGRAM_OFFSET + posn, bins, hist_shadow+ram_offs },

#undef DESC_HST
#define DESC_HST(Total_ents,Perm_ents,Score_bytes,Init_bytes,posn,ram_offs,len)\
    { FIRST_HST_OFFSET + posn,	/* start of eeprom record */\
	(Perm_ents*(Score_bytes+Init_bytes)),	/* length of ram */ \
 hst_shadow+ram_offs /* address of ram shadow */},

#undef DESC_USER
#define DESC_USER(posn,ram_posn,len)\
 { FIRST_USER_OFFSET+posn, len, user_shadows+ram_posn },

static const struct toc {
    unsigned short offset;
    unsigned short ram_length;
    unsigned char *shadow;
} master_index [] = {
#ifdef RUN_BYTES
    { 0, RUN_BYTES, run },		/* special "running" record */
#endif
#ifdef HOME_BYTES
    { 0, HOME_BYTES, home },		/* "slow" parts of stats */
    { ALT_OFFS, HOME_BYTES, home },	/* duplicate copy of "home" */
#endif
#include <stat_map.h>
};

/* The number of bytes in an eeprom. 2804's have 512, 2816's have 2048 */
#ifndef EEPROM_SIZE
#define EEPROM_SIZE (512)
#endif

/* The number of times to shift an index into eeprom to get an address. On an
*  8-bit machine it would be 0. On a 16-bit machine, 1, on a 32-bit machine, 2
*/
#ifndef STRIDE
#ifdef __asap__
#define STRIDE (2)
#else
#define STRIDE (1)
#endif
#endif

#ifndef HOSTED
# if defined(vax) || defined(__i386__) || defined(__unix__)
#  define HOSTED (1)
# endif
#endif

#if HOSTED
# if defined(EEPROM_ADDR)
#  undef EEPROM_ADDR
# endif
# if defined(EEPROM)
#  undef EEPROM
# endif
#endif

/* The actual name given to the address of the EEPROM is up to you. It is used
*  exactly once, to initialize the struct config below.
*/
#ifndef EEPROM_ADDR
extern unsigned char EEPROM[];
#define EEPROM_ADDR (EEPROM)
#endif

/* The address below is also used exactly once, also to initialize the struct
*  config. It should be the location that needs to be written to unlock
*  (allow writes to) the EEPROM.
*/
#ifndef UNLK_EP
extern unsigned char UNLK_EP;
#endif

/* You need to provide a routine to handle "can't happen" Usually this is
*  simply a STOP #$2700 on a 68K system.
*/
extern void prc_panic();

/* You need to provide a routine to initialize the game options to the default
*  values. It may also want to, e.g. clear the high_score table.
*/
extern void default_values();

/* these would come from string.h on a hosted system */
extern void *memset(),*memmove(),*memcpy();
extern int memcmp();

/* the only assembly actually needed is the code to actually write a byte
*  into the EEPROM. It must disable interrupts so that the data write can
*  immediately follow a write to "unlock".
*/
#if HOSTED
# ifndef __m_int_TYPE_DEFINED
#  define __m_int_TYPE_DEFINED
typedef int m_int;
# endif /* __m_int_TYPE_DEFINED */
# ifndef __m_uint_TYPE_DEFINED
#  define __m_uint_TYPE_DEFINED
typedef unsigned int m_uint;
# endif /* __m_uint_TYPE_DEFINED */
#include <stdio.h>
static void
eer_write_byte(where,unlock,what)
unsigned char *where,*unlock,what;
{
    /* on the vax, we are simulating anyway, so ignore unlock, but do
    *  check that "where" is in the EEPROM
    */
    (void) unlock;	/* "use" it to keep lint happy */
    if ( where < &EEPROM[0] || where >= &EEPROM[EEPROM_SIZE] ) {
	fprintf(stderr,"attempt to write outside eeprom, offset = %d\n",
	    where - &EEPROM[0]);
    } else *where = what;
}
#else
#ifndef __m68k__
#ifndef __m_int_TYPE_DEFINED
#define __m_int_TYPE_DEFINED
typedef int m_int; /* largest integer type we can multiply */
#endif /* __m_int_TYPE_DEFINED */
#ifndef __m_uint_TYPE_DEFINED
#define __m_uint_TYPE_DEFINED
typedef unsigned int m_uint; /* largest unsigned type we can multiply */
#endif /* __m_uint_TYPE_DEFINED */

extern void eer_write_byte(volatile unsigned char *where,unsigned char *unlock,int what);
#else
/* for now, assume __m68k__ will be defined when appropriate, and embed
*  the assembly in a static array.
*/
#ifndef __m_int_TYPE_DEFINED
#define __m_int_TYPE_DEFINED
typedef short m_int; /* largest integer type we can multiply */
#endif /* __m_int_TYPE_DEFINED */
#ifndef __m_uint_TYPE_DEFINED
#define __m_uint_TYPE_DEFINED
typedef unsigned short m_uint; /* largest unsigned type we can multiply */
#endif /* __m_uint_TYPE_DEFINED */
static const unsigned short kluge[] = {
    0x206F, /* MOVE.L	4(SP),A0 ; Points to EEPROM byte to write */
     0x0004,
    0x226F, /* MOVE.L	8(SP),A1 ; Points to "UNLOCK" location (write strobe) */
     0x0008,
    0x102F, /* MOVE.B	15(SP),D0 ; Byte to write */
     0x000F,
    0x7218, /* MOVEQ.L	#24,D1 */
    0xE3A0, /* ASL.L	D1,D0	  ; Shift byte to top for Brian */
    0x40C1, /* MOVE.W  SR,D1		*/
    0x007C, /* ORI.W   #INT_OFF,SR 	*/
     0x0700,
    0x1281, /* MOVE.B  D1,(A1)	*/
    0x2080, /* MOVE.L  D0,(A0)	*/
    0x46C1, /* MOVE.W  D1,SR	*/
    0x4E75  /* RTS */
};
#define eer_write_byte(where,unlock,what) ((void(*)())kluge)(where,unlock,what)
#endif	/* 68k  versus other embedded */
#endif	/* hosted or not */

volatile unsigned long eer_rtc;
static int eer_read();	 /* ( int from_offset, U8* to_ptr, int n_data_bytes) */

struct state {
	unsigned char * sptr;	/* pointer to RAM source */
	unsigned short wrtrec;	/* record number to write */
	unsigned char todo;	/* Flag for "something to do" */
	unsigned char rtry;	/* Retry count */
	unsigned char * read;	/* pointer to RAM dst (read) */
	  signed char * rdflag;	/* pointer to completion flag */
	unsigned short rdrec;	/* Record number to read */
	unsigned short rdlen;	/* Length of record to read */
	unsigned short wndx;	/* Byte index (during write) */
	unsigned short datalen;	/* Length of data portion (during write) */
	unsigned char serr;	/* Soft errors */
	unsigned char herr;	/* Hard errors */
	unsigned char prev_byte;	/* last byte written (for verify) */
	unsigned char inited;	/* != 0 if eer_init() done */
	unsigned char wrcb[MAX_CHECK];	/* Write check bytes */
};
struct config {
	unsigned char * eeprom_base;	/* Pointer to base of EEPROM */
	unsigned char n_tries;	/* Number of retrys for write-verify */
	unsigned char data_bytes;	/* # data bytes per record (8..11) */
	unsigned char * unlock;	/* Where to hit to unlock EEPROM */
};

/* All the state information that needs to be carried from one invocation
*  of eer_hndl() to another is contained in the struct state eer_state.
*  The idea that multiple eeproms might be serviced at once has been abandoned,
*  but C is still happier with the state "bundled". eer_hndl(), eer_read(),
*  and eer_write() now set their own pointers to eer_state and eer_config.
*/
struct state eer_state;

#ifndef PSX_GAME
const struct config eer_config = { (U8*)EEPROM_ADDR, N_TRIES, 0, (U8*)&UNLK_EP };
#else
/* Sony requires use of their lib to access EEPROM...
 */
extern unsigned long WriteBackup(
    const unsigned char *src,	/* Data to write */
    unsigned long size,		/* In bytes */
    unsigned long offset
);

extern unsigned long ReadBackup(
    unsigned char *dst,		/* Where to put data read. */
    unsigned long size,		/* In bytes */
    unsigned long offset
);

/* Unfortunately, their library routines are non-rentrant and
 * yet do not mask interrupts. WriteBackup() is also capable
 * of hanging forever if presented with a hardware error, or
 * up to twenty seconds under "normal conditions".
 * For these reasons, we substitute the following:
 */
#define MYSTERY_REG (*(VU32 *)0x1F80100C)
#define KLUGE_AND (~(0x10FFL))
#define KLUGE_OR (0x55)
#define PSX_EEPROM_ADDR (0x1FAF0000)
#define SCRUB_EEPROM (1)

void eer_write_byte(where,unlock,what)
volatile unsigned char *where;
unsigned char *unlock,what;
{
    VU32 *kluge_vec;
    U32 ctl;
    int old_ipl;

    kluge_vec = &MYSTERY_REG;
    old_ipl = prc_set_ipl(INTS_OFF);
    ctl = *kluge_vec;
    *kluge_vec = (ctl & KLUGE_AND) | KLUGE_OR;
    *where = what;
    *kluge_vec = ctl;
    prc_set_ipl(old_ipl);
}

static unsigned int eer_read_byte(where)
volatile unsigned char *where;
{
    VU32 *kluge_vec;
    U32 ctl;
    int old_ipl;
    int val;

    kluge_vec = &MYSTERY_REG;
    old_ipl = prc_set_ipl(INTS_OFF);
    ctl = *kluge_vec;
    *kluge_vec = (ctl & KLUGE_AND) | KLUGE_OR;
    val = *where;
    *kluge_vec = ctl;
    prc_set_ipl(old_ipl);
    return val;
}
/* Take no chances with left-over STRIDE definitions */
#undef STRIDE
#define STRIDE (0)
const struct config eer_config = { (U8*)PSX_EEPROM_ADDR, N_TRIES, 0, (U8*)0 };
#endif

/*	A "helper" routine: reclen() computes the total record
*	length, given the number of data bytes.
*/
static m_uint
reclen(databytes)
int databytes;
{
    int nbytes,bmask;

    nbytes = databytes;
    bmask = 1;

    /* loop stops when bmask becomes >= number of bytes so far, that is,
    *  when the number of check bytes+data bytes is <= max bit in the
    *  max index.
    */
    while ( bmask < ++nbytes ) bmask <<= 1;
    return nbytes;
}

#ifdef RUN_BYTES
/*	Another "helper", run_offs() computes the offset of the
*	current bucket from a bucket number.
*/
static int
run_offs(bucket)
m_uint bucket;
{
    m_uint offset;
    offset = bucket * reclen(RUN_BYTES);
    return offset + FIRST_BUCKET;
}
#endif /* RUN_BYTES */

/* eer_hndl() will "call back" to eer_get_write() to get the next record to
*  write. The following byte arrays keep track of the status of the individual
*  records:
*  Rec_Busy:	Record is being modified. Set by eer_puts, eer_puth, etc.
*		Cleared by eer_write.
*  Server_Phase/Client_phase:
*		These two are each only modified by either the "server"
*		(interrupt level) or the "client" (mainline level). If they
*		differ, the record is "dirty" (needs to be written), while if
*		they are the same it is "clean" (outstanding writes done)
*/

static void eer_write(int);

#define JUKE_ENTS (sizeof(master_index)/sizeof(struct toc))
#define JUKE_BYTES ((JUKE_ENTS+7)>>3)

static unsigned char rec_busy[JUKE_BYTES];
#define make_busy(recno) (rec_busy[(recno)>>3] |= (1 << ((recno) & 7)))
static unsigned char server_phase[JUKE_BYTES];
static unsigned char client_phase[JUKE_BYTES];

#define DONT_PANIC (42)
#ifndef DONT_PANIC
#define SCAN_EEPROM (1)
#endif

#if SCAN_EEPROM | SCRUB_EEPROM
/*	When eer_get_write is unable to get a new write, it will call this
*	routine to scan the records for possible mis-match between shadow and
*	eeprom.	This check will call panic() if it detects an inconsistency,
*	as these are normally bugs. It also takes some time. To avoid it,
*	#define DONT_PANIC (42?)
*
*	In addition, scan_juke can be made to "scrub" the EEPROM by re-writing
*	any records with soft errors in them. This was added for the Sony
*	PSX system, which aparently has a flakier than usual EEPROM.
*/
static int scan_ent;			/* the "state" for background check */
void scan_juke()			/* check one record for consistency */
{
    unsigned char got[MAX_DATA],*shadow;
    int recoff,status,nbytes;
    const struct toc *rp;

    if ( !eer_state.inited ) return;

    if ( ++scan_ent >= JUKE_ENTS ) scan_ent = 0;

    /* skip "current" record if it is marked "busy" */
    if ( rec_busy[scan_ent >> 3] & (1 << (scan_ent & 7)) ) return;

    rp = &master_index[scan_ent];
    recoff = rp->offset;
#ifdef RUN_BYTES
    if ( scan_ent == 0 ) recoff = run_offs(home[BUCKET_NUM]);
#endif

    shadow = rp->shadow;
    nbytes = rp->ram_length;
    status = eer_read(recoff,got,nbytes);
    if ( status < -1 ) {
	/* hard error detected by check-bytes */
#ifndef DONT_PANIC
	prc_panic( "hard eeprom error" );
#endif
    } else if ( status < 0 ) {
	/* soft error detected by check-bytes */
#ifndef DONT_PANIC
	if ( memcmp(shadow, got, nbytes) != 0 ) {
	    /* We claim to have fixed it, but it
	     * doesn't match the shadow.
	     */
	    prc_panic("mis-corrected soft EEPROM error");
	}
#endif
#if SCRUB_EEPROM
	eer_write(scan_ent);	/* re-write suspect record */
#endif
    }
#ifndef DONT_PANIC
    else if ( memcmp(shadow,got,nbytes) != 0 ) {
	/* data in EEPROM different from shadow */
	prc_panic("stale EEPROM");
    }
#endif
}
#endif /* SCAN_EEPROM | SCRUB_EEPROM */

static int
eer_get_write()
{
    struct state *esp;
    int nbytes;

    const struct toc *jd;
    int jdrec;
    m_uint recno;
    int bit,dirty_byte;

    esp = &eer_state;

    for ( recno = 0 ; recno < JUKE_BYTES ; ++recno ) {
	/* scan for "dirty" records that aren't "busy"
	*/
	if ( (dirty_byte = server_phase[recno] ^ client_phase[recno])
	     &&  (dirty_byte & ~rec_busy[recno]) ) break;
    }
    if ( recno >= JUKE_BYTES ) {
	/* nothing in jukebox, try scanning */
#if SCAN_EEPROM | SCRUB_EEPROM
	/* if asked, re-read a record (of those the juke-box knows
	*  about) and compare it to the purported value in the
	*  shadow.
	*/
	scan_juke();
#endif
	return 0;
    }

    recno <<= 3;	/* prepare to look for bit within byte */
    bit = 1;

    while ( (dirty_byte & bit) == 0) {
	/* scan bits of active byte for record to be written */
	++recno;
	bit <<= 1;
    }
    jd = master_index + recno;	/* point to record descriptor */
    jdrec = jd->offset;
#ifdef RUN_BYTES
    if ( recno == 0 ) {
	/* record refers to running bytes, compute current offset */
	jdrec = run_offs(home[BUCKET_NUM]);
    }
#endif
    esp->wrtrec = jdrec;
    esp->sptr = jd->shadow;
    esp->datalen = jd->ram_length;
    esp->sptr += esp->datalen;	/* offset source pointer for pre-dec */
    nbytes = reclen(esp->datalen);
    if ( (esp->wrtrec + nbytes) >= EEPROM_SIZE ) {
	prc_panic("Write extends past end of EEPROM");
	return 0;
    }
    esp->wndx = nbytes - 1;
    esp->wrtrec <<= STRIDE;
    memset(&esp->wrcb[0],0,nbytes-esp->datalen);
    server_phase[recno>>3] ^= bit;		/* mark as "done" when set up */
    return 1;
}

/*	"queue" a record for writing by toggling its client_phase bit.
*	This is only done if there is not already a pending write.
*	We also clear the busy bit (_after_ setting "dirty").
*	This includes _minimal_ error checking!!
*/
static void
eer_write(recno)
int recno;
{
    struct state *esp;
    m_uint bit;

    esp = &eer_state;
    if ( recno < 0 || recno >= JUKE_ENTS ) prc_panic("Record number out of bounds");

    bit = 1 << (recno & 7);
    recno >>= 3;

    /* Check for already pending write (server_phase != client_phase), and
    *  toggle client phase if not.
    */
    if ( ((server_phase[recno] ^ client_phase[recno]) & bit) == 0) {
	/* not already "dirty", toggle client phase */
	client_phase[recno] ^= bit;
    }
    rec_busy[recno] &= ~bit;
}

static void eer_hndl();

#if HOST_BOARD_CLASS && ((HOST_BOARD&HOST_BOARD_CLASS) == PHOENIX)

#undef WRAP_VAL			/* No moving buckets on BatRam-only system */

static struct act_q eer_actq;
static int eer_busy_stuck;		/* we need to figure out what to do here */

void eer_action(void *arg) {
    struct state *esp = &eer_state;
    unsigned char *rdp;
    int status;

    ++eer_rtc;
    if ( (rdp = esp->read) != 0 ) {
	/* need to read a record before we make the EEPROM unavailable by
	*  writing.
	*/
	status = eer_read(esp->rdrec,rdp,esp->rdlen);
	if ( esp->rdflag ) *(esp->rdflag) = status;
	esp->read = 0;
    }
    if ( esp->inited ) {
	int ii;
	for (ii=0; ii < (EEPROM_SIZE + EEPROM_SIZE/10); ++ii) {
	    /* Write one byte to EEPROM, or BATRAM, or frog-brain,
	     * or whatever. Then check if we still have pending
	     * writes (eer_busy() == 2). Keep trying if so.
	     */
	    eer_hndl();
	    if ( eer_busy() < 2) break;
	}
	if (ii >= EEPROM_SIZE) {
	    ++eer_busy_stuck;		/* eer_busy is stuck. We need to figure out how to
					   correct this situation */
	}
    }
    return;
}
#endif

/*	following needs to be called at "v-blank" (at most once every
*	10 milliseconds) Lots of places in SYSTEM_1 stat routines assume
*	that a "tick" of eer_rtc is exactly 1/60th of a second... BEWARE
*	In this version, the read part of of the assembly eer_hndl has been
*	replaced with C code at eer_read()
*/
void
eer_hwt()
{
#if HOST_BOARD_CLASS && ((HOST_BOARD&HOST_BOARD_CLASS) == PHOENIX) 
    eer_actq.action = eer_action;
    prc_q_action(&eer_actq);
#else
    struct state *esp = &eer_state;
    unsigned char *rdp;
    int status;

    ++eer_rtc;
    if ( (rdp = esp->read) != 0 ) {
	/* need to read a record before we make the EEPROM unavailable by
	*  writing.
	*/
	status = eer_read(esp->rdrec,rdp,esp->rdlen);
	if ( esp->rdflag ) *(esp->rdflag) = status;
	esp->read = 0;
    }
#ifndef PSX_GAME
    if ( esp->inited ) eer_hndl();
#endif
#endif
}


#ifdef PSX_GAME
static unsigned long psx_old_rtc;
#endif

/*	following can be called to determine if a write is in progress
*	We are stricter now about writes not being "queued" from interrupt
*	level, so we do not need to disable them here.
*	This is for compatability with SYSTEM_1, so it doesn't know about
*	multiple state structs...
*
*	On PSX, because of lingering doubts about the wisdom of writing
*	to EEPROM from interrupt level, we also run eer_hndl() from here,
*	rather than eer_hwt(). This allows the rest of GUTS to
*	be more-or-less unaware of the loss of interrupt-driven
*	writes, as any routine which spins on eer_busy() will
*	be making progress. A PSX game should also call eer_busy()
*	periodically, perhaps in its main game loop, to assure
*	writing everything out in a timely fashion.
*
*	Returns:
*	0: All done.
*	1: One or more locked records
*	2: Write in progress
*/
int
eer_busy()
{
    int i,how_busy;
    how_busy = 0;
    if ( eer_state.sptr ) how_busy = 2;	/* quick check catches majority */
    else {
	for ( i = JUKE_BYTES-1 ; i >= 0 ; --i ) {
	    if ( rec_busy[i] ) how_busy = 1;
	    if ( server_phase[i] != client_phase[i] ) {
		how_busy = 2;
		break;
	    }
	}
	/* no records busy or "dirty", except _possibly_ one which
	*  "snuck through" while we were scanning. So check sptr again.
	*/
	if ( eer_state.sptr ) how_busy = 2;
    }
#ifdef PSX_GAME
    if ( how_busy > 1 && eer_state.inited && psx_old_rtc != eer_rtc ) {
	eer_hndl();
	psx_old_rtc = eer_rtc;
    }
#endif
    return how_busy;
}


int eer_incs();			/* forward decl. */

/*		eer_init()
*	Called to initialize the shadows from EEPROM, and also to re-write
*	them if any errors were detected. If the user has defined a stat
*	named EER_ERRORS and operation is "more or less normal", the value
*	is returned. If there was a read error and eer_config.n_tries is zero,
*	(only used when debugging on the bench), the status (-1 for correctable
*	errors, -2 for uncorrectable errors) will be returned AND THE EEPROM
*	WILL NOT BE INITED. Otherwise returns zero.
*/
int
eer_init()
{
    int home_status,alt_status;
    int no_read = 0;			/* flag for non-stat initialization */
    int i;
    unsigned long signature;
    unsigned char alt[HOME_BYTES];
#if MAX_STAT

    eer_state.serr = eer_state.herr = 0;

    home_status = eer_read(0,home,HOME_BYTES);
    alt_status = eer_read(reclen(HOME_BYTES),alt,HOME_BYTES);
    if ( (home_status < 0 || alt_status < 0 )
	&& eer_config.n_tries == 0 ) {
	return (home_status < alt_status) ? home_status : alt_status;
    }
    if ( alt_status > home_status ) {
	/* alt was in better shape */
	make_busy(HOME_REC);
	(void) memcpy(home,alt,HOME_BYTES);
	if ( eer_config.n_tries ) {
	    eer_write(HOME_REC);	/* re-write in home */
	}
	if ( alt_status < 0 && eer_config.n_tries ) {
	    /* there were soft errors, so re-write in alt too */
	    eer_write(ALT_REC);
	}
	home_status = alt_status;	/* both are now effectively same */
    }
    else if ( home_status > alt_status ) {
	/* home was in better shape */
	make_busy(ALT_REC);	/* WHY?? */
	if ( eer_config.n_tries ) eer_write(ALT_REC); /* re-write in alt */
	if ( home_status < 0 && eer_config.n_tries ) {
	    /* there were soft errors, so re-write in home too */
	    eer_write(HOME_REC);
	}
    }

    /* If the signature in the (possibly corrected) home block does not match,
    *  we need a total initialization, so pretend we had hard errors.
    */
    signature = 0;
    for ( i = 3 ; i >= 0 ; --i ) {
	signature <<= 8 ;
	signature |= home[SIGNAT_POSN+i];
    }

    /* if hard errors on both home and alt, or bad signature, initialize
    *  all stats to default, instead of reading from EEPROM.
    */
    if ( (signature != SIGNATURE) || (home_status < -1) ) no_read = 1;

    if ( no_read ) {
	/* re-init all "scalar" stats.
	*/
	make_busy(HOME_REC);
	make_busy(ALT_REC);
	memset(home,0,sizeof(home));
#ifdef RUN_BYTES
	make_busy(RUN_REC);
	memset(run,0,sizeof(run));	/* default for run _is_ 0 */
	home[BUCKET_NUM] = 0;
#endif
	default_values();	/* set default values in home block */
	signature = SIGNATURE;
	for ( i = 0 ; i < 4 ; ++i ) {
	    home[SIGNAT_POSN+i] = signature;
	    signature >>= 8 ;
	}
	if ( eer_config.n_tries ) {
	    eer_write(HOME_REC);
	    eer_write(ALT_REC);
#ifdef RUN_BYTES
	    eer_write(RUN_REC);
#endif
	}
    }
#ifdef RUN_BYTES
    else {
	/* we did not need to set default values, so we _do_ need to
	*  get the saved data from run (lsbs).
	*/
	m_uint recoff = run_offs(home[BUCKET_NUM]);
	alt_status = eer_read(recoff,run,RUN_BYTES);
	if ( alt_status < -1 ) {
	    /* had hard errors. We definitely need to clear all of run,
	    *  but should we clear home too? That's a policy decision...
	    */
	    memset(run,0,RUN_BYTES);
	}
	if ( alt_status < 0 && eer_config.n_tries ) eer_write(RUN_REC);
    }
#endif /* RUN_BYTES */
#endif /* MAX_STAT */
#if HST_BYTES
    init_hst(no_read);
#endif
#if HISTOGRAM_BYTES
    init_histograms(no_read);
#endif
#if USER_RECS
    init_user(no_read);
#endif
    if ( home_status < 0 && eer_config.n_tries == 0 ) return home_status;
    eer_state.inited = 1;	/* flag it for vblank */
#ifdef EER_ERRORS
    home_status = eer_gets(EER_ERRORS);
    /* add any new errors to the error log for return value
     */
    i = eer_state.herr+eer_state.serr;
    if ( i ) home_status = eer_incs(EER_ERRORS,i);
#endif
    return home_status;
}

#if MAX_STAT
int
eer_gets(stat_num)
unsigned int stat_num;
{
    int value;
    int offset;
    MAP_TYPE mapent;
    unsigned home_size;

    if ( stat_num > MAX_STAT ) return -1;
    mapent = stat_map[stat_num];
    /* first the part (always some?) in home */
    offset = (mapent >> HOME_POSN) & HOME_MASK;
    home_size = ((mapent >> SIZ_POSN) & SIZ_MASK)+1;
    if ( (offset += home_size) & 1 ) {
	/* Nybble above highest nybble is on odd boundary, which means
	*  highest nybble is on even boundary. We must grab only 4 bits
	*/
	value =  home[offset>>=1] & 0xF;
	--home_size;
    } else {
	/* Highest nybble is on odd boundary. The case where this is
	*  the _only_ nybble will be handled in the while loop below.
	*  other cases can just grab a byte at a time.
	*/
	value = 0;
	offset >>= 1;
    }
    while ( home_size != 0 ) {
	if ( home_size == 1 ) {
	    /* last nybble, can get only 4 bits */
	    value <<= 4;
	    value |= ((home[--offset] >> 4) & 0xF);
	    break;
	} else {
	    value <<= 8;
	    value |= home[--offset];
	    home_size -= 2;
	}
    }

#ifdef RUN_BYTES
    /* now the part (if any) in run[] */
#if RUN_POS != 0
    offset = (mapent >> RUN_POS) & RUN_MASK;
#else
    offset = mapent & RUN_MASK;
#endif /* RUN_POS */
    if ( offset != 0 ) value = (value << 8) | run[offset-1];
#endif /* RUN_BYTES */
    return value;
}

/*		eer_puts(stat_num,value)
*	Write "value" at stat position selected by "stat_num". Returns -1 for
*	erroneous input, else 0;
*/
int
eer_puts(stat_num,value)
unsigned int stat_num;
unsigned int value;
{
    int offset;
    MAP_TYPE mapent;
    unsigned home_size;

    if ( stat_num > MAX_STAT ) return -1;
    mapent = stat_map[stat_num];
#ifdef RUN_BYTES
    offset = (mapent >> RUN_POSN) & RUN_MASK;
    if ( offset ) {
	/* part lives in run[] */
	make_busy(RUN_REC);
	run[--offset] = value & 0xFF;
	eer_write(RUN_REC);
	value >>= 8;
    }
#endif
    /* now the part in home */
    make_busy(HOME_REC);
    make_busy(ALT_REC);
    offset = (mapent >> HOME_POSN) & HOME_MASK;
    home_size = ((mapent >> SIZ_POSN) & SIZ_MASK)+1;
    if ( offset & 1 ) {
	/* lowest nybble is on odd boundary, stash only four bits */
	offset >>= 1;
	home[offset] = (home[offset] & 0xF) | ((value & 0xF) << 4);
	value >>= 4;
	--home_size;
	++offset;
    } else {
	offset >>= 1;
    }
    while ( home_size != 0 ) {
	if ( home_size == 1 ) {
	    /* only one nybble left. */
	    home[offset] = (home[offset] & 0xF0) | (value & 0xF);
	    break;
	}
	home[offset] = value & 0xFF;
	++offset;
	value >>= 8;
	home_size -= 2;
    }
    eer_write(HOME_REC);		/* HOME record */
    eer_write(ALT_REC);			/* ALT record */
    return 0;
}

/*		eer_incs(stat_num,value)
*	Adds "value" to stat at position "stat_num" Returns -1 for bogus
*	params, else returns resulting value.
*/
int
eer_incs(stat_num,value)
unsigned int stat_num;
unsigned int value;
{
    /* later we will make this a routine in its own right */
    int accum,old_accum;

    if ( (accum = eer_gets(stat_num)) < 0 ) return accum;
    old_accum = accum;
    accum += value;
#ifdef WRAP_VAL
    if ( stat_num == WRAP_VAL && ( (old_accum ^ accum) >= 0x100)) {
	/* time to rotate the bucket */
	int new_bucket = home[BUCKET_NUM] + 1;
	if ( (run_offs(new_bucket) + reclen(RUN_BYTES)) >= EEPROM_SIZE ) {
	    /* new bucket would overlap end of memory, wrap it */
	    new_bucket = 0;
	}
	make_busy(HOME_REC);
	make_busy(ALT_REC);
	home[BUCKET_NUM] = new_bucket;
    }
#endif /* WRAP_VAL */
    eer_puts(stat_num,accum);		/* do the update */
    return accum;
}

/*		eer_p_incs(stat_num,value)
*	Adds "value" to stat at position "stat_num" Returns -1 for bogus
*	params, else returns resulting value.
*	Pins incremented value at 65535.
*/
int
eer_p_incs(stat_num,value)
unsigned int	stat_num;
int		value;
{
    int	temp;

    if ((temp = eer_gets(stat_num)) != -1)
    {
	if (value > (0xFFFF - temp))
	    value = 0xFFFF - temp;
	if (value)
	    temp = eer_incs(stat_num,value);
    }

    return (temp);
}
#endif

#if HST_BYTES
/*	Code for handling new "plastic" high-score-tables. Each table
*	is now described by an entry in the array "high_scores", giving
*	the total and permanent size of the table, and the number of bytes
*	of score and initials.
*/

/*	Internal routine hst_ptr() converts table and rank into a pointer
*	to the desired entry, or 0 if table or rank are "out of bounds".
*	The size (in bytes) of an entry and the length (in bytes) of that
*	part of the table from the selected entry to the end of the table
*	are returned indirectly, via siz_ptr and len_ptr respectively.
*	It is intended to be used by eer_hstr(), eer_hstw(), and eer_rank();
*/
static unsigned char*
hst_ptr(rank,table,len_ptr,siz_ptr)
m_uint table,rank;
int *len_ptr;
int *siz_ptr;
{
    m_int len;
    m_uint ent_size;
    const struct score_descrip *hst;

    if ( table >= HST_TABLES ) return 0;
    hst = high_scores + table;
    if ( rank >= hst->entries ) return 0;
    len = hst->entries - rank;
    ent_size = hst->Score_bytes + hst->Init_bytes;
    if ( len_ptr ) *len_ptr = len * (m_int)ent_size;
    if ( siz_ptr ) *siz_ptr = ent_size;
    return master_index[table+FIRST_HST_REC].shadow + rank * ent_size;
}

/*	eer_hstr(rank,table) returns a pointer to a static struct
*	containing the re-constituted entry for entry "rank" in table
*	"table", or 0 if given bad params.
*/

struct hst_ent
*eer_hstr(rank,table)
unsigned int rank,table;
{
    unsigned char *ptr;
    static struct hst_ent mytemp;
    int i;
    unsigned long accum;
    char *inits;
    const struct score_descrip *hst;


    if ( (ptr = hst_ptr(rank,table,(int *)0,(int*)0) ) == 0) return 0;

    hst = high_scores + table;
    accum = 0;
    for ( i = hst->Score_bytes; i != 0 ; --i ) {
	accum <<= 8;
	accum |= *ptr++;
    }
    mytemp.score = accum;

    inits = &mytemp.initials[0];
#if HST_INITIAL_COMPRESS == 1
    for ( i = hst->Init_bytes ; i != 0 ; i -= 2 ) {
	/* each two bytes encodes two initials */
	accum = (ptr[1] << 8) | *ptr;
	ptr += 2;
	*inits++ = (accum & 0x1f) ? ('A' - 1 + (accum & 0x1f)) : ' ';
	accum >>= 5;
	*inits++ = (accum & 0x1f) ? ('A' - 1 + (accum & 0x1f)) : ' ';
	accum >>= 5;
	*inits++ = (accum & 0x1f) ? ('A' - 1 + (accum & 0x1f)) : ' ';
	}
#endif
#if HST_INITIAL_COMPRESS == 0
    memcpy(inits,ptr,hst->Init_bytes);
    inits += hst->Init_bytes;
#endif
    *inits = '\0';		/* help for the thinking impaired */
    return &mytemp;
}

/*		eer_hstw(rank,entptr,table)
*	pack and writes data from struct hst_ent at entptr in "table"
*	at "rank". Returns 0 if bad params, else inserts entry, pushing
*	any higher numbered (lower scores) entries down and returns non-zero.
*/
int
eer_hstw(rank,entptr,table)
unsigned int rank;
struct hst_ent *entptr;
unsigned int table;
{
    unsigned char *ptr;
    int tail,ent_size,i;
    char *inits;
    const struct score_descrip *hst;


    unsigned long accum;

    /* first validate entry and get pointer to correct slot */
    if ( (ptr = hst_ptr(rank,table,&tail,&ent_size)) == 0) return 0;

    hst = high_scores + table;

    if ( rank < hst->perm ) {
	/* Mark shadow as "busy" */
	make_busy(table+FIRST_HST_REC);
    }
    /* If more than one entry "below" this point, ripple down */
    if (tail > ent_size) memmove(ptr + ent_size,ptr,tail-ent_size);

    /* Stash the score bytes */

    accum = entptr->score;
    i = hst->Score_bytes;
    ptr += i;
    for (  ; i != 0 ; --i ) {
	*--ptr = accum;
	accum >>= 8;
    }

    /* now point to initials */
    ptr += hst->Score_bytes;

    inits = &entptr->initials[0];
#if HST_INITIAL_COMPRESS == 1
    /* Each two bytes of eeprom (or shadow) holds three initials */
    for ( i = hst->Init_bytes ; i != 0 ; i -= 2 ) {
	m_uint letter;
	accum = 0;
	letter = *inits;
	if ( letter && letter != ' ' ) {
	    accum = (letter & 0x1f);
	    ++inits;
	} else if ( letter == ' ' ) ++inits;

	letter = *inits;
	if ( letter && letter != ' ' ) {
	    accum |= (letter & 0x1f) << 5;
	    ++inits;
	} else if ( letter == ' ' ) ++inits;
	letter = *inits;
	if ( letter && letter != ' ' ) {
	    accum |= (letter & 0x1f) << 10;
	    ++inits;
	} else if ( letter == ' ' ) ++inits;

	*ptr++ = accum;
	*ptr++ = accum >> 8;
    }
#endif
#if HST_INITIAL_COMPRESS == 0
    memcpy(ptr,inits,hst->Init_bytes);
#endif
    if ( rank < hst->perm ) {
	/* Write those portions which live in EEPROM */
	eer_write(table+FIRST_HST_REC);
    }
    return 1;
}

/*		eer_rank(score,table)
*	returns the rank that a score "deserves" in table. Returns -1 if
*	score does not qualify. (this will be interpreted as a very big
*	unsigned number if passed to eer_hstw, so it will be safe)
*/
int eer_rank(score,table)
unsigned long score;
unsigned int table;
{
    unsigned char *ptr;
    int i,tail,rank,sc_size,ent_size;

    unsigned char b_score[sizeof(unsigned long)];

    if ( (ptr = hst_ptr(0,table,&tail,&ent_size)) == 0) return -1;

    sc_size = high_scores[table].Score_bytes;

    /* spread the bytes of score into a "Big Endian" array */

    for ( i = sc_size ; i ; ) {
	b_score[--i] = score;
	score >>= 8;
    }

    rank = 0;

    do {
	for ( i = 0 ; i < sc_size ; ++i ) {
	    if ( b_score[i] != ptr[i] ) break;
	}
#if (HST_TIE_TO_NEWER)
/*
 *	added by BF so that tie goes to newest player
 *	MA hates this!
 */
	if ( (i == sc_size) || (b_score[i] >= ptr[i]) ) break;
#else
/*
 *	MA likes this!
 */
	if ( (i != sc_size) && (b_score[i] > ptr[i]) ) break;
#endif
	ptr += ent_size;
	++rank;
    } while ((tail -= ent_size) >= 0);
    if ( tail < 0 ) return -1;
    return rank;
}
/*		init_hst()
*	read in high-score-table and initialize shadows
*	We re-write hst entries that are totally zapped, even though the
*	game will be explicitly writing them when it sees zero scores. This
*	is to prevent racking up error counts if the game _doesn't_ do it
*	right.  Soft errors will also be re-written to "scrub" memory.
*	If the parameter "no_read" is non-zero, all high-scores will
*	just be cleared. This is to allow a total initialization in the
*	face of plausible garbage in the eeprom.
*/
static void
init_hst(no_read)
int no_read;
{
    m_uint table;
    int len,status;
    unsigned char *ptr;
    const struct toc *rec;

    rec = &master_index[FIRST_HST_REC];
    for ( table = 0 ; table < HST_TABLES ; ++table ) {
	if ( !(ptr = hst_ptr(0,table,&len,(int *)0))) {
	    prc_panic("Bad High-score-table");
	}
	memset(ptr,0,len);		/* pre-clear whole table */
	if ( no_read ) status = -2;	/* pretend it failed */
	else status = eer_read(rec->offset,ptr,rec->ram_length);
	if ( status < -1 ) {
	    /* hard errors or first time, clear shadow */
	    memset(ptr,0,rec->ram_length);
	}
	if ( status < 0 ) {
	    /* any errors (or first time), re-write in place */
	    eer_write(table+FIRST_HST_REC);
	}
	++rec;
    }
}
#endif
#if HISTOGRAM_BYTES
/*		init_histograms()
*	Simply reads all histograms into shadow. Again, (no_read != 0)
*	suppresses the reading of plausible garbage. Tables with errors
*	are written back immediately.
*/
static void
init_histograms(no_read)
int no_read;
{
    unsigned char *ptr;
    m_uint table;
    const struct toc *rec;
    int status;

    rec = &master_index[FIRST_HISTOGRAM_REC];
    for ( table = 0 ; table < HIST_TABLES ; ++table ) {
	ptr = rec->shadow;
	if ( no_read ) status = -2;	/* pretend it failed */
	else status = eer_read(rec->offset,ptr,rec->ram_length);
	if (status < -1 ) {
	    /* hard errors, zap it */
	    memset(ptr,0,rec->ram_length);
	}
	if ( status < 0 ) eer_write(table+FIRST_HISTOGRAM_REC);
	++rec;
    }
}

/*		eer_geth(table,bin)
*
*/
int
eer_geth(table,bin)
unsigned int table,bin;
{
    const struct toc *rec;

    if ( table >= HIST_TABLES ) return -1;

    rec = &master_index[table+FIRST_HISTOGRAM_REC];
    if ( bin >= rec->ram_length ) return -1;
    return rec->shadow[bin];
}

int
eer_puth(table,bin,value)
unsigned int table,bin;
int value;
{
    const struct toc *rec;

    if ( table >= HIST_TABLES ) return -1;

    rec = &master_index[table+FIRST_HISTOGRAM_REC];
    if ( bin >= rec->ram_length ) return -1;
    make_busy(table+FIRST_HISTOGRAM_REC);
    rec->shadow[bin] = (value &= 0xFF);

    eer_write(table+FIRST_HISTOGRAM_REC);
    return value;
}

/*			scale_table(table)
*	Divides all entries in specified histogram table by 2 (Shifts
*	right. More_or_less internal routine for use by eer_bumph.
*	Returns -1 for bogus table number, else 0;
*/
static int
scale_table(table)
m_uint table;
{
    unsigned char *ptr;
    int k;
    const struct toc *rec;

    if ( table >= HIST_TABLES ) return -1;

    rec = &master_index[table+FIRST_HISTOGRAM_REC];
    ptr = rec->shadow;
    make_busy(table+FIRST_HISTOGRAM_REC);
    for ( k = rec->ram_length-1 ; k>= 0 ; --k,++ptr) *ptr >>= 1;

    eer_write(table+FIRST_HISTOGRAM_REC);
    return 0;
}

/*			eer_clrhist(table)
*	Clear histogram table "table"
*	Returns -1 for bogus table number, else 0;
*/
int
eer_clrhist(table)
unsigned int table;
{
    const struct toc *rec;

    if ( table >= HIST_TABLES ) return -1;

    rec = &master_index[table+FIRST_HISTOGRAM_REC];
    make_busy(table+FIRST_HISTOGRAM_REC);

    (void) memset(rec->shadow,0,rec->ram_length);
    eer_write(table+FIRST_HISTOGRAM_REC);
    return 0;
}

/* 			eer_bumph(table,bin,bump)
*	Added to allow game-maintained histograms. Adds bump to bin (0..19)
*	of table (0..5). If the result is > 255, scales all tables indicated
*	in sibs (D0 means table 0, D1 means table 1 etc..) and the current
*	table by shifting all bins in these tables down one bit.
*
*	NOTE: The old version of this routine used to get "sibs" as its
*	last parameter. This one uses the "sibs" field of histo_table[].
*/
int
eer_bumph(p_table,p_bin,p_bump)
int p_table, p_bin, p_bump;
{
    m_uint table = p_table;
    m_uint bin = p_bin;
    m_uint bump = p_bump;
    m_int j;
    unsigned long sibs;
    int val;

    if ( (val = eer_geth(table,bin)) < 0 ) return val;
    if ( (val += bump) > 255 ) {
	/* here we only scale the indicated histogram and its "siblings" */
	sibs = histo_table[table].sibs;	/* get "tied" tables */
	for ( j = HIST_TABLES-1 ; j >= 0 ; --j ) {
	    if ( (sibs & ( 1L << j )) == 0 ) continue;
	    if ( scale_table(j) < 0 ) return -1;
	}
	val >>= 1;
    }
    (void) eer_puth(table,bin,val);
    return val;
}

/*		eer_hist_title(table, buff, buflen, maxpt, medpt, bar)
*  returns title of histogram table in caller's buff, max length buflen
*  optionally stores max bin value (if maxpt != 0) and median bin number
*  (if medpt != 0). returns length of title, or -1 = error.
*
*	There was apparently some confusion as to how to get/suppress
*  the "MEDIAN" indication. The code below was amended 1OCT96 to meet
*  some expectation that a colon at the end of the title would suppress
*  the MEDIAN indication. This actually makes more sense than the old
*  way which seems never to have been used anyway. (MEA)
*
*	This routine is intended to be used in conjunction with eer_hist_line
*  and must be called before any call to eer_hist_line. It sets up the "state"
*  needed for formatting the individual lines.
*/

static struct hist_state {
    int table;			/* current histogram table (+1) */
    int max;			/* max value in any bin of current table */
    int total;
    int legend_len;		/* Max length of any label */
    int left_max;		/* length of left value in legend */
    int right_max;		/* length of right value in legend */
    char *labels;		/* point to label part of legend, if any */
    const char *bar_chars;	/* point to chars for drawing bar */
    m_uint bar_divs;		/* number of bar_chars */
} curr_hist;

#ifndef STR_HIST_TO
#define STR_HIST_TO "-"
#endif

#ifndef STR_AND_UP
#define STR_AND_UP "& UP"
#endif

#ifndef STR_COLON
#define STR_COLON  ": "
#endif

#define SIZ_HIST_TO (sizeof(STR_HIST_TO)-1)
#define SIZ_AND_UP (sizeof(STR_AND_UP)-1)
#define SIZ_COLON (sizeof(STR_COLON)-1)

static int
declen(val)
int val;
{
    int digit,accum;

    accum = 10;
    for ( digit = 1 ; digit < 10 ; ++digit ) {
	/* keep moving the digit up until we hit 10 or are greater than value
	*/
	if ( val < accum ) break;
	accum = ((accum << 2) + accum) << 1;	/* for brain-dead compilers */
    }
    return digit;
}

int
eer_hist_title(table, buff, buflen, maxpt, medpt, bar)
unsigned int table;
char *buff;
int buflen;
int *maxpt, *medpt;
const char *bar;
{
    m_uint idx,max;		/* index and max_so_far for various scans */
    m_int accum;		/* accumulated total of bin values */
    char *lp,*rp;		/* left and right substring pointers */
    const struct histo_descrip *hd;	/* descriptor for current table */
    struct hist_state *hs = &curr_hist;

    hs->table = 0;		/* default to invalid, for error return */
    /* first validate parameters */
    if ( (table >= HIST_TABLES) || ( buff == 0 ) || ( buflen < 1 ) ) {
	return -1;
    }

    if ( bar == 0 ) bar = "#:";		/* default bar */
    hs->bar_chars = bar;
    hs->bar_divs = strlen(bar);

    hd = histo_table + table;

    lp = hd->legend;

    for ( idx = 0 ; idx < buflen ; ++idx ) {
	/* scan legend for end or first '\t' */
	if ( lp[idx] && lp[idx] != '\t' ) buff[idx] = lp[idx];
	else break;
    }

    if ( idx >= buflen ) return -1; 	/* title too long for buffer */

    /* If the very last character of the title is a colon,
     * suppress it and the computation of the median.
    */ 
    if ( buff[idx-1] == ':' ) {
	buff[idx-1] = '\0';
	if ( medpt ) *medpt = -1;
	medpt = 0;
    }
    buff[idx] = '\0';

    hs->labels = 0;
    if ( lp[idx] ) {
	/* programmer-provided legends, save pointer and scan for longest */
	hs->labels = rp = (lp + idx);
	max = 0;
	for ( idx = 0 ; idx < hd->bins ; ++idx ) {
	    lp = rp++;			/* skip over leading '\t' */
	    while ( *rp && *rp != '\t' ) ++rp;
	    if ( (rp - lp) > max ) max = rp - lp;
	    if ( *rp == '\0' && idx != (hd->bins - 1) ) {
		/* labels ended too soon */
		return -1;
	    }
	    else if ( *rp != '\0' && idx == (hd->bins - 1) ) {
		/* too many labels */
		return -1;
	    }
	}
	hs->legend_len = max;		/* save length of largest legend */
    } else {
	/* calculate max label size for generated labels.
	* this has to include the 0-first line, several xxx-xxx lines,
	*  and the xxx &UP line
	*/
	int right_last;
	if ( hd->bins < 2 ) { return -1; }

/*	 calculate last number (possibly) needed on right side of a label
*  The following is _really_ the formula, but GreenHills geeks (uses long mul),
*  so I had to split it into two lines.
*	right_last = hd->first + (hd->bins-2) * (hd->later) - 1;
*/
	right_last = (m_uint)(hd->bins-2) * hd->later;
	right_last += hd->first - 1;

	hs->left_max = declen(right_last + 1);

	/* If later bin size == 1, no "right side" will be neded for them */
	if ( hd->later == 1 ) right_last = hd->first - 1;

	if ( right_last == 0 ) {
	    /* degenerate first bin too. No ranges, so legend len starts
	    *  as all "left parts"
	    */
	    hs->right_max = 0;
	    hs->legend_len = hs->left_max;
	} else {
	    /* at least one range, so length needs to account for left, '-',
	    *  and right.
	    */
	    hs->right_max = declen(right_last);
	    hs->legend_len = hs->left_max + SIZ_HIST_TO + hs->right_max ;
	}

	if ( hs->legend_len < (hs->left_max + SIZ_AND_UP) ) {
	    /* largest "range" is shorter than the last number (on the left)
	    *  plus the phrase "& UP". Do we want to expand legend_len,
	    *  or use "+"?
	    */
	    if ( (hs->legend_len - hs->left_max) > 2) {
		/* Already 3 or 4, expand legend to leave room for "& UP"
		*/
		hs->legend_len = hs->left_max + SIZ_AND_UP;
	    } else if ( hs->legend_len < (hs->left_max + 1) ) {
		/* expand just enough for '+' */
		hs->legend_len = hs->left_max + 1;
	    }
	}
#if NO_COLON_ON_USER_LEGEND
	hs->legend_len += SIZ_COLON;  /* they all need room for this */
#endif
    }
#if !(NO_COLON_ON_USER_LEGEND)
    hs->legend_len += SIZ_COLON;  /* they all need room for this */
#endif

    max = 0;
    accum = 0;

    for ( idx = 0 ; idx < hd->bins ; ++idx ) {
	/* scan all bins, gathering median, and max */
	int bin_value;

	bin_value = eer_geth(table,idx);
	if ( bin_value < 0 ) { return -1; }
	accum += bin_value;
	if (bin_value > max ) max = bin_value;
    }

    hs->max = max;

    hs->total = accum;
    if ( maxpt ) *maxpt = max;
    if ( medpt ) {
	/* user wants median bin number, scan to find it.
	*/
	int bin_val;
	accum = (accum+1) >> 1;
	for ( idx = 0 ; idx < hd->bins ; ++idx ) {
	    bin_val = eer_geth(table,idx);
	    if ( accum <= bin_val ) break;
	    accum -= bin_val;
	}
	*medpt = idx;
    }

    hs->table = table + 1;	/* so 0 can mean not-inited */

    if ( hs->labels ) return hs->labels - 1 - hd->legend;
    else return strlen(hd->legend);
}

/*		eer_hist_line(bin, buff, buflen)
*  format a line into caller's buff, max length buflen. Uses static values
*  saved by previous call to eer_hist_title.
*  ret: bin value or -1 = error
*/

int
eer_hist_line( bin, buff, buflen)
unsigned int bin;
char *buff;
int buflen;
{

    int bin_value,idx,tmp;
    int bin_start,bin_end;
    m_uint bar_pix;		/* in "pixels" == chars * bar_divs */
    m_int bar_len;		/* in chars */
    m_uint max;			/* max value in any bin, minimum 4 */
    m_uint pix;
    char *lp,*rp;
    const struct histo_descrip *hd;
    struct hist_state *hs = &curr_hist;

    bin_value = eer_geth(hs->table-1,bin);	/* bails if table == 0 */
    if ( bin_value < 0 ) return -1;

    if ( buff == 0 ) return -1;

    hd = histo_table + (hs->table - 1);

    /* find out how much of the buffer is left after allowing for:
    *  the legend,
    *  the max value,
    *  the space before the percentage
    *  the 4 for the maximum percentage value
    *  the space before the bar,
    *  the terminating NUL.
    */
    bar_len = buflen - hs->legend_len - declen(hs->max) - 7;
    if ( bar_len < 1 ) return -1;

    /* Lay in the label for this line.
    */
    if ( (rp = hs->labels) != 0 ) {
	/* user-supplied labels */
	idx = 0;
	do {
	    lp = ++rp;
	    while ( *rp && *rp != '\t' ) ++rp;
	} while ( ++idx <= bin );
	/* ends with lp pointing at left end of correct label, rp pointing
	*  at '\t' or '\0' just past right end.
	*/

#if NO_COLON_ON_USER_LEGEND
	if ( rp - lp < hs->legend_len ) {
	    /* need to pad on left */
	    memset(buff,' ',hs->legend_len - (rp - lp));
	    buff += (hs->legend_len - (rp - lp));
	}
	while ( lp < rp ) *buff++ = *lp++;
#else
	if ( ((rp - lp)+SIZ_COLON) < hs->legend_len ) {
	    /* need to pad on left */
	    memset(buff,' ',hs->legend_len - (rp - lp)-SIZ_COLON);
	    buff += (hs->legend_len - (rp - lp)-SIZ_COLON );
	}
	while ( lp < rp ) *buff++ = *lp++;
	buff += SIZ_COLON;
	memcpy(buff-SIZ_COLON,STR_COLON,SIZ_COLON);
#endif
    } else {
	/* generated labels */
	int mid_pad;

	mid_pad = SIZ_HIST_TO;
	if ( hs->right_max == 0 ) mid_pad = 0;

	bin_start = 0;
	if ( bin == 0 ) {
	    /* special case for first bin */
	    bin_end = hd->first - 1;
	} else {
	    /* bins after first start with non-zero
	    */
	    bin_start = (m_uint)(bin-1) * hd->later;
	    bin_start += hd->first;
	    bin_end = bin_start + hd->later - 1;
	}
	utl_cdec(bin_start,buff,hs->left_max,RJ_BF);
	buff += hs->left_max;
	memset(buff,' ',hs->legend_len-hs->left_max);
	if ( (bin_start != bin_end) && (bin != (hd->bins - 1)) ) {
	    /* need to show a range */
	    memcpy(buff,STR_HIST_TO,mid_pad);
	    utl_cdec(bin_end,buff+mid_pad,hs->right_max,RJ_BF);
	    buff[mid_pad+hs->right_max] = ' ';		/* knock out the NUL */
	} else if ( bin == (hd->bins - 1) ) {
	    /* special case for last bin Calculate how much extra room we have
	    */
	    int slop;

	    slop = hs->legend_len - (hs->left_max + SIZ_COLON);
	    slop -= SIZ_AND_UP;

	    if ( slop < 0 ) {
		*buff = '+';
	    } else {
		/* center, favoring right */
		slop = (slop + 1) >> 1;
		memcpy(buff+slop,STR_AND_UP,SIZ_AND_UP);
	    }

	}
	/* space over end of legend, then "back up" and lay in the colon */
	buff += hs->legend_len - hs->left_max;
	memcpy(buff-SIZ_COLON,STR_COLON,SIZ_COLON);
    }

    /* now the value */

    idx = declen(hs->max);
    utl_cdec(bin_value,buff,idx,RJ_BF);
    buff += idx;

    *buff++ = ' ';
    /* now the percentage value */
    if ( hs->total )
	tmp  = (bin_value*100)/hs->total;
    else
	tmp = 0;
    utl_cdec(tmp,buff,3,RJ_BF);
    buff += 3;
    *buff++ = '%';
    *buff++ = ' ';

    bar_pix = bar_len * hs->bar_divs;   /* number of "pixels" in max bar */

    /* compute how many pixels this value needs: (value * bar_pix)/max_value */

    pix = (m_uint)bin_value * bar_pix;
    max = hs->max;
    /* avoid absurd bars */
    if ( max < (hs->bar_divs << 2) ) max = hs->bar_divs << 2;
    pix += (max >> 1);					/* round */

    bar_pix = hs->bar_divs * max;	/* get "divisor" for whole bar pixels */

    /* Draw whole bar chars for each bar_pix pixels until only a fraction
    *  remains.
    */
    /* The following code cannot possibly work on systems where the ASCII characters
     * are moved from their "native" locations, which has been true for most
     * of our games for quite a while. I do not know how or when this botch occured,
     * but this code is doomed anyway, so I'll kluge it. 17JUN95 MEA
     */
    while ( pix >= bar_pix ) {
	int barint;
	barint = *(hs->bar_chars);
	*buff++ = barint - (AN_A_STMP- 'A');
	pix -= bar_pix;
    }

    if ( pix ) {
	/* Some fractional part remains. Select a partial bar character.
	*/
	int barint;
	pix /= max;
	if ( pix ) {
	    barint = hs->bar_chars[pix];
	    *buff++ = barint - (AN_A_STMP- 'A');
	}
    }
    *buff++ = '\0';

    return bin_value;
}

/*		eer_tally_hist(table,value)
*	Increment appropriate bin of table for value. Uses eer_bumph, so
*	"siblings" may get scaled.
*/

int
eer_tally_hist(table,value)
unsigned int table,value;
{
    int idx;
    const struct histo_descrip *hd;

    if ( table > HIST_TABLES ) return -1;
    hd = histo_table + table;
    idx = 0;
    if ( value >= hd->first ) {
	value -= hd->first;
	for ( idx = 1 ; idx < hd->bins-1 ; ++idx ) {
	    if ( value < hd->later ) break;
	    else value -= hd->later;
	}
    }
    return eer_bumph(table,idx,1);
}
#endif

#if USER_RECS
/*		init_user()
*	Simply reads all user records into shadow. Again, (no_read != 0)
*	suppresses the reading of plausible garbage. Any records with
*	errors need to be written back immediately.
*/
static void
init_user(no_read)
int no_read;
{
    unsigned char *ptr;
    m_uint recno;
    const struct toc *rec;
    int status;

    rec = &master_index[FIRST_USER_REC];
    for ( recno = 0 ; recno < USER_RECS ; ++recno ) {
	ptr = rec->shadow;
	if ( no_read ) status = -2;	/* pretend it failed */
	else status = eer_read(rec->offset,ptr,rec->ram_length);
	if (status < -1 ) {
	    /* hard errors, zap it */
	    memset(ptr,0,rec->ram_length);
	}
	if ( status < 0 ) eer_write(recno+FIRST_USER_REC);
	++rec;
    }
}
#endif

/*		eer_user_rd(recno,lenpt)
*	returns a pointer to the shadow for user record 'recno'. Also stores
*	the length of the shadow, via lenpt (if not 0). Returns 0 for non-
*	existant record. NOTE: the user record will be marked "busy" to prevent
*	scan_juke from complaining about shadow<->EEPROM mismatch. Either
*	eer_user_wrt() or eer_user_free() must eventually be called or
*	eer_busy() will continue to return TRUE.
*/
unsigned char *
eer_user_rd(recno, lenpt)
int recno;
int *lenpt;
{
    const struct toc *tp;

    if ( recno < 0 || recno >= USER_RECS ) return 0;
    recno+= FIRST_USER_REC;
    make_busy(recno);
    tp = &master_index[recno];
    if ( lenpt ) *lenpt = tp->ram_length;
    return tp->shadow;
}

/*		eer_user_wrt(recno)
*	"writes" the user record 'recno', making it not-busy and setting up to
*	write it to EEPROM. The EEPROM may not be immediately updated, so if you
*	care, you must loop on eer_busy(). Returns 0 for success, -1 for error.
*/
int
eer_user_wrt(recno)
int recno;
{

    if ( recno < 0 || recno >= USER_RECS ) return -1;
    eer_write(recno+FIRST_USER_REC);
    return 0;
}

/*		eer_user_flush()
*	Writes any busy user records. Should not be called from interrupt level,
*	as it calls the "client side" eer_write()
*/
void
eer_user_flush()
{
    int recno;

    for ( recno = FIRST_USER_REC; recno < (FIRST_USER_REC+USER_RECS); ++recno ){
	if ( rec_busy[recno >> 3] & (1 << (recno & 7)) ) {
	    eer_write(recno);
	}
    }
    return;
}

int
eer_user_free(recno)
int recno;
{
    int byte;
    int bit;

    if ( recno < 0 || recno >= USER_RECS ) return -1;
    byte = (recno+=FIRST_USER_REC) >> 3;
    bit = 1 << (recno & 7);
    rec_busy[byte] &= ~bit;
    return 0;
}

/*		eer_user_purge()
*	Frees any busy user records. Should not be called from interrupt level,
*	as it messes with "client side" variable rec_busy.
*/
void
eer_user_purge()
{
    int recno;

    for ( recno = FIRST_USER_REC; recno < (FIRST_USER_REC+USER_RECS); ++recno) {
	rec_busy[recno >> 3] &= ~(1 << (recno & 7));
    }
    return;
}

/*		eer_user_refresh(recno)
*	re-reads from eeprom to shadow, but does not alter busy. This
*	routine can cause a delay, as it may need to wait for eeprom
*	to recover from a write. It should not be called with interrupts
*	masked, as a deadly embrace could result.
*/
int
eer_user_refresh(recno)
int recno;
{
    struct state *esp = &eer_state;
    signed char status;

    const struct toc *tp;

    if ( recno < 0 || recno >= USER_RECS ) return 0;

    recno+=FIRST_USER_REC;

    tp = &master_index[recno];

    while ( esp->read ) {;}	/* spin until free */
    esp->rdflag = &status;
    status = 0;
    esp->rdrec = recno;
    esp->rdlen = tp->ram_length;
    esp->read = tp->shadow;
    while ( status == 0 ) {;}	/* spin until done */
    if ( status == -1 ) eer_write(recno);	/* "scrub" soft errors */
    return status;
}


/*		eer_read()
*	An experimental C version of the routine that reads back records
*	from EEPROM into shadow ram. It checks for/corrects errors. I can't
*	really imagine using more than 16 check bytes (corresponding to a
*	single EEPROM record with 32752 data bytes, but the routine should
*	be readily extensible... (watching out for machines with 16bit ints)
*/
#define MAX_CHECK_BYTE (MAX_CHECK-1)

static int
eer_read(offset,ram,databytes)
m_uint offset;
unsigned char *ram;
int databytes;
{
    struct state *esp;
    unsigned char check[MAX_CHECK];		/* holds check bytes */
    unsigned char *cbp,*dbp;			/* point to check/data bytes */
    int nbytes,bmask;
    unsigned char data;
    unsigned char *eeprom;
    esp = &eer_state;
    eeprom = eer_config.eeprom_base + (offset << STRIDE);

    /* first compute total (check + data) number of bytes, and highest bit
    *  used in index.
    */
    nbytes = databytes;
    bmask = 1;

    /* loop stops when bmask becomes >= number of bytes so far, that is,
    *  when the number of check bytes+data bytes is <= max bit in the
    *  max index.
    */
    while ( bmask < ++nbytes ) bmask <<= 1;

    bmask >>= 1;		/* back bmask off to MSB of nbytes */

    dbp = ram + databytes ;	/* point just past highest data byte used */

    /* initialize check bytes to all zeroes
    */
    memset(check,0,MAX_CHECK);
    while ( --nbytes >= 0 ) {
	/* loop down through eeprom, copying data bytes and eoring check bytes
	*  as needed.
	*/
#ifdef PSX_GAME
	/* Use Sony lib to get a byte, hard error if Sony lets us down */
	/* Actually, we need to use our own version... */
	data = eer_read_byte(eeprom+nbytes);
#else
	data = eeprom[nbytes << STRIDE];
#endif
	/* actual data is stored at "addresses" with more than one bit set */
	if ( nbytes & (nbytes - 1) ) *--dbp = data;
	cbp = check;
	*cbp++ ^= data;		/* _all_ bytes contribute to ct */

	for ( bmask = 1 ; bmask <= nbytes ; bmask <<= 1) {
	    /* Scan for check bytes that this byte is included in. That is,
	    *  if the index within a record has bit 'n' set, eor the data
	    *  into check[n+1];
	    */
	    if ( bmask & nbytes ) *cbp ^= data;
	    ++cbp;
	}
    } /* end while */
    /* now we have read all bytes into shadow ram, and should have all zeroes
    *  in the check-byte array.
    */
    data = 0;
    for ( nbytes = MAX_CHECK_BYTE ; nbytes >= 0 ; --nbytes ) {
	data |= check[nbytes];
    }

    if ( !data ) return 1;	/* successfull read */

    /* OK, so we got some errors. Don't panic, maybe they are recoverable */
    for ( bmask = 1 ; bmask < 0x100 ; bmask <<= 1 ) {
	/* for each bit-slice, gather the syndrome. That is, pick up
	*  the bits from the individual bytes of check[].
	*/
	int syndrome = 0;
	for ( nbytes = MAX_CHECK_BYTE ; nbytes >= 0 ; --nbytes ) {
	    syndrome <<= 1;
	    if ( check[nbytes] & bmask ) ++syndrome;
	}
	if ( syndrome == 0 ) continue;	/* problem not with this bit */
	if ( (syndrome & 1) == 0 ) {
	    /* a non-zero syndrome with a zero ct is a double-bit error
	    *  we will just return a hard error indication.
	    */
	    ++(esp->herr);
	    return -2;
	}
	else {
	    /* single-bit error, log and correct it */
	    ++(esp->serr);
	    syndrome >>= 1;
	    if ( (syndrome & (syndrome-1)) == 0 ) {
		/* syndrome has only one bit set, refers to check-byte */
		continue;	/* no correction to data needed */
	    }
	    dbp = ram;
	    /* "count" the data bytes between "0" and "syndrome" */
	    while ( syndrome ) {
		if ( syndrome & (syndrome-1) ) ++dbp;
		--syndrome;
	    }
	    if ( (dbp-ram) > databytes ) {
		/* "correcting" a non-existant data byte, hard error */
		++(esp->herr);
		return -2;
	    }
	    *--dbp ^= bmask;	/* correct single-bit error */
	}
    }
    return -1;		/* return "correctable errors" */
}

static void
eer_hndl()
{
    struct state *esp;
    const struct config *ecp;
    int index = -1;
    int cbi;		/* check byte index */

    unsigned char to_write,already,*src,*dst;

    esp = &eer_state;
    ecp = &eer_config;

    if ( (src = esp->sptr) == 0 ) {
	/* no write in progress, can we get one? */
	if ( !eer_get_write() ) return;
	esp->rtry = ecp->n_tries;		/* fresh count of retries */
	index = esp->wndx;
	src = esp->sptr;
	dst = ecp->eeprom_base + esp->wrtrec; /* point to new record */
    } else {
	/* write in progress, check byte previously written */
	dst = ecp->eeprom_base + esp->wrtrec; /* point to record in progress */
	index = esp->wndx;
#ifdef PSX_GAME
	/* Sony complicated, and their lib is buggy, so we need to use our
	* own version...
	*/
	already = eer_read_byte(dst+index);
#else /* Non-PSX */
	already = dst[index << STRIDE];
#endif /* PSX */
	if ( already != (to_write = esp->prev_byte) ) {
	    /* last write didn't "take", at least a soft error */
	    if ( esp->serr < 255 ) ++(esp->serr);
	    if ( esp->rtry == 0 ) {
		/* no retries left */
		if ( esp->herr < 255 ) ++(esp->herr);
		if ( ecp->n_tries ) --index;
		else { prc_panic("Write-verify error, no retries"); return; };
	    } else {
		/* count a re-try and try writing again */
		--(esp->rtry);
		dst += (index << STRIDE);	/* point to EEPROM byte */
		eer_write_byte(dst,		/* Where to write */
		    eer_config.unlock,		/* Hit to unlock */
		    to_write);			/* Byte to write */
		return;	/* better luck next time */
	    }
	} /* end if (write-verify error) */

	/* get here when we suceeded in writing, or gave up */
	esp->rtry = ecp->n_tries;		/* fresh count of retries */
	if ( --index < 0 ) {
	    /* finished with that record, try for another */
	    esp->sptr = 0;
	    if ( !eer_get_write() ) return;
	    dst = ecp->eeprom_base + esp->wrtrec; /* point to new record */
	    src = esp->sptr;
	    index = esp->wndx;
	}
    } /* end "else" (sptr != 0, write in progress) */

    /* next byte of record in progress, or first byte of new one */
    if ( index & (index - 1) ) {
	/* more than one bit set in index, it is a data byte */
	to_write = *--src;
	esp->sptr = src;
	for ( cbi = 0 ; cbi < MAX_CHECK_BYTE ; ++cbi ) {
	    /* eor data byte with each appropriate check byte */
	    if ( index & (1 << cbi) ) esp->wrcb[cbi+1] ^= to_write;
	}
    } else if (index) {
	/* if only one bit set, need to store a check byte */
	for ( cbi = 0 ; index != (1 << cbi) ; ++cbi) {;}
	to_write = esp->wrcb[cbi+1];
    } else {
	/* index == 0, write ct (wrcb[0]) */
	to_write = esp->wrcb[0];
    }
    esp->wrcb[0] ^= to_write;			/* redundant on last byte */
    esp->wndx = index;
    dst += (index << STRIDE);			/* point into EEPROM */
#ifdef PSX_GAME
    already = eer_read_byte(dst);
#else
    already = *dst;
#endif /* PSX */
    if ( already != to_write )  {
	/* EEPROM _not_ already correct */
	eer_write_byte(dst,			/* Where to write */
	    eer_config.unlock,			/* Hit to unlock */
	    to_write);				/* Byte to write */
    }
    esp->prev_byte = to_write;			/* save for R-A-W check */
}
