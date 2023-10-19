/*
 *	$Id: mallocr.c,v 1.28 1997/10/06 22:01:19 shepperd Exp $
 *
 *		Copyright 1996 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#if QIO_POOL_SIZE
#include <qio.h>
#endif
#include <reent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* #define MALLOC_DEBUG 	1 */

/*
 * The structure '_reent' has two members, _nextf and _nmalloc that the GNUC
 * library used for its table of area sizes. Each is an array of 30 elements
 * of which we use four. The 0'th element houses the head of the free list,
 * the 1'st element houses the head of the busy list, the 2'nd element contains
 * a pointer to the SbrkPool entry which identifies this reent struct, the 3'rd
 * element contains a flag indicating we need not protect malloc functions with
 * a prc_set_ipl() and the 4'th element contains the Xinu semaphore to use to
 * protect these data structures. We keep both a busy and a free list so we can
 * verify that the chain of either allocated or free'd memory has not been
 * corrupted by an errant program.
 */

#define FREE 0		/* pointer to head of free list */
#define BUSY 1		/* pointer to head of busy list */
#define POOL 2		/* pointer to SbrkPool for this item */
#define PROT 3		/* if .ne., don't protect malloc functions with prc_set_ipl */
#define SEMA 4		/* if .ne., contains the Xinu semaphore to use to protect malloc calls */

#define CHUNK	8	/* 8 bytes per chunk */

#if INCLUDE_XINU
extern int xi_getpid(void);
extern int xi_signal(int sem);
extern int xi_wait(int sem);
extern int _xinu_malloc_reset(struct _reent *);
#endif

#if !MALLOC_DEBUG
typedef struct pack {
    unsigned int    size;	/* size of area in CHUNKs */
    struct pack     *next;	/* ptr to next elem in list	*/
# if INCLUDE_XINU
    int		    pid;	/* Xinu's pid of task that owns this */
    int		    pad;	/* need to pad the struct size to a double */
# endif
} MemPkt;
#else
typedef struct pack {
    unsigned long   magic;	/* magic number			*/
    int		    line;	/* line number of caller	*/
    const char *    file;	/* filename of caller		*/
    unsigned int    size;	/* size of area in CHUNKs	*/
    int		    flags;	/* misc bits */
    struct pack     *next;	/* ptr to next elem in list	*/
# if INCLUDE_XINU
    int		    pid;	/* Xinu's pid of task that owns this */
    int		    pad;	/* Need to pad struct size to a double */
# endif
} MemPkt;
#endif

#define MEMPKT_CHUNKS ((sizeof(MemPkt)+CHUNK-1)/CHUNK)	/* sizeof MemPkt in chunks */

static void die(const char *msg) {
    while (1) {
	__asm__("BREAK;");
    }
}

#if MALLOC_DEBUG

/************************************************************
 * validate_malloc_list - walks the chain and checks that it
 * is not corrupt.
 *
 * At entry:
 *	rep - pointer to _reent struct
 *	which - into which list to check (BUSY or FREE)
 * At exit:
 *	returns nothing. Dies if fails.
 */
void validate_malloc_list(struct _reent *rep, int which) {
    MemPkt **prev, *cur;
    unsigned long *end;
    int oldps=0;

    if (!rep) die("Called validate_malloc with null param");
    if (which > 1) die ("Called validate_malloc with invalid which");
    prev = (MemPkt **)(&rep->_nextf[which]);
    if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
    while ( (cur = *prev) ) {
	if ((cur->flags&1)) die( "Loop detected in malloc list" );
	cur->flags |= 1;
	if (cur->magic != 0x12345678) die("Header Magic corrupt");
	end = (unsigned long *)((char *)(cur + 1) + cur->size*CHUNK) - 1;
	if (*end != 0x87654321) die("Trailer Magic corrupt");
	prev = &cur->next;
    }
    prev = (MemPkt **)(&rep->_nextf[which]);
    while ( (cur = *prev) ) {
	cur->flags &= ~1;
	prev = &cur->next;
    }
    if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
}
#endif

/************************************************************
 * minsert - inserts a MemPkt into a list. Places the packet
 * 	in the list sorted by size (smaller areas first).
 *
 * At entry:
 *	rep - pointer to _reent struct
 *	which - into which list to insert (BUSY or FREE)
 *	ptr - pointer to MemPkt
 * At exit:
 *	returns 0 on success, 1 if failed
 */

static int minsert(struct _reent *rep, int which, MemPkt *ptr) {
    MemPkt *current, **prev;
    int oldps=0;

    if (!ptr || !rep) return 1;

    if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
    prev = (MemPkt **)(&rep->_nextf[which]);

    while ( (current = *prev) && (current->size < ptr->size) ) {
	prev = &current->next;
    }

    if (prev == (MemPkt **)(&rep->_nextf[which])) { /* Ptr will be first in list */
        ptr->next = *prev;
    } else {			/* else it's in the middle or at the end */
	ptr->next = current;
    }
    *prev = ptr;
    if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
    return 0;
}

/************************************************************
 * mremove - removes a MemPkt from a list. 
 *
 * At entry:
 *	rep - pointer to _reent struct
 *	which - from which list to remove (BUSY or FREE)
 *	ptr - pointer to MemPkt
 * At exit:
 *	returns 0 on success, 1 on failure
 */
static int mremove(struct _reent *rep, int which, MemPkt *ptr) {
    MemPkt  *current, **prev;
    int oldps=0, sts;

    if (!ptr || !rep) return 1;

    if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
    prev = (MemPkt **)(&rep->_nextf[which]);

    while ( (current = *prev) && (current != ptr) ) {
	prev = &current->next;
    }

    if (!current) {
	sts = 1;		/* didn't find it in the list */
    } else {
	*prev = ptr->next;	/* break the link */
	ptr->next = 0;		/* forget where we were */
	sts = 0;
    }
    if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
    return sts;
}

/************************************************************
 * guts_free_r - free a previously allocated area of memory.
 *
 * At entry:
 *	rep - pointer to _reent struct
 *	pkt - pointer to old area
 * At exit:
 *	returns nothing.
 */
void guts_free_r(struct _reent *rep, void *pkt, const char *file, int lineno) {
    MemPkt  *test, *current, **prev, *end;
    int oldps=0;

    if (!rep || !pkt) return;

    current = (MemPkt *)pkt - 1;

#if INCLUDE_XINU
    if (rep->_nextf[SEMA]) xi_wait((int)rep->_nextf[SEMA]);	/* wait on semaphore */
#endif
#if MALLOC_DEBUG
    validate_malloc_list(rep, FREE);
    validate_malloc_list(rep, BUSY);
    if (mremove(rep, BUSY, current)) die("Tried to free area not BUSY");
    current->file = file;
    current->line = lineno;
#else
    if (mremove(rep, BUSY, current)) {
# if INCLUDE_XINU
	xi_signal((int)rep->_nextf[SEMA]);
# endif
	return;					/* not on busy list */
    }
#endif
    
/* collapse new area into ajoining areas (if possible) */

    if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
    prev = (MemPkt **)(&rep->_nextf[FREE]);	/* prepare to walk the free list */
    end = (MemPkt *)((char *)(current+1) + current->size*CHUNK); /* point to one past end of pkt */

    while ( (test = *prev) ) {
	MemPkt *cend;
	cend = (MemPkt *)((char *)(test+1) + test->size*CHUNK); /* point to one past end of MemPkt */
	if ( cend == current ) {		/* area ajoins us at the head */
	    mremove(rep, FREE, test);		/* pluck the one we're to chang from the list */
	    test->size += current->size+MEMPKT_CHUNKS;	/* previous guy gets our size */
#if MALLOC_DEBUG
	    current->magic = 0xAAAA5555;	/* kill this area */
#endif
	    current = test;		/* the changed one becomes the current */
	    prev = (MemPkt **)(&rep->_nextf[FREE]);	/* walk the list from the beginning */
	    continue;			/* look again */	    
	}
	if ( test == end ) {		/* area ajoins us at the rear */
	    mremove(rep, FREE, test);	/* pluck him from the free list */
	    current->size += test->size+MEMPKT_CHUNKS;	/* we get his area + his MemPkt */
#if MALLOC_DEBUG
	    test->magic = 0x5555AAAA;	/* kill this area */
#endif
	    prev = (MemPkt **)(&rep->_nextf[FREE]);	/* walk the list from the beginning */
	    end = (MemPkt *)((char *)(current+1) + current->size*CHUNK); /* point to one past end of pkt */
	    continue;			/* look again */	    
	}
	prev = &test->next;
    }
    if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
    minsert(rep, FREE, current);	/* put it on the free list */
#if MALLOC_DEBUG
    validate_malloc_list(rep, FREE);
    validate_malloc_list(rep, BUSY);
#endif
#if INCLUDE_XINU
    if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
}

#if MALLOC_DEBUG
#define FLAGS_MALLOC	2
#define FLAGS_REALLOC	4
#define FLAGS_FREE	6

static void fixup(struct _reent *rep, MemPkt *pkt, const char *file, int lineno, int what) {
    unsigned long *end;
    pkt->magic = 0x12345678;
    pkt->file = file;
    pkt->line = lineno;
    pkt->flags = what;
# if INCLUDE_XINU
    pkt->pid = xi_getpid();
# endif    
    end = (unsigned long *)((char *)(pkt+1) + pkt->size*CHUNK)-1;
    *end = 0x87654321;
    validate_malloc_list(rep, FREE);
    validate_malloc_list(rep, BUSY);
}
# define FIXUP(v,w,x,y,z) fixup(v,w,x,y,z)
#else
# if INCLUDE_XINU
#  define FIXUP(rent,pkt,f,l,w) do { pkt->pid = xi_getpid(); } while (0)
# else
#  define FIXUP(rent,pkt,f,l,w) do { ; } while (0)
# endif
#endif

/************************************************************
 * guts_malloc_r - allocate some memory.
 *
 * At entry:
 *	rep - pointer to _reent struct
 *	size - amount to malloc.
 * At exit:
 *	returns ptr to area on success, 0 on failure
 *	Returned ptr is aligned to a double.
 */
void *guts_malloc_r(struct _reent *rep, int size, const char *file, int lineno) {
    MemPkt          *current, **prev, *next;
    unsigned int    newsize, oldsize;
    int oldps=0;

#if INCLUDE_XINU
    if (rep->_nextf[SEMA]) xi_wait((int)rep->_nextf[SEMA]);
#endif
#if MALLOC_DEBUG
    validate_malloc_list(rep, FREE);
    validate_malloc_list(rep, BUSY);
#define DEBUG_EXTRA	sizeof(long)
#else
#define DEBUG_EXTRA	0
#endif
    if (size <= 0) {
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
	return 0;			/* nothing to do */
    }
    newsize = (size + DEBUG_EXTRA + CHUNK-1)/CHUNK; /* Round size up to next multiple of CHUNK */

    if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
    prev = (MemPkt **)(&rep->_nextf[FREE]); /* point to free list */
    while ( (current = *prev) ) {
	if (current->size >= newsize) break;	/* give 'em this one */
        prev = &current->next;
    }

    if (!current) {			/* not on the list */
	if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
	current = (MemPkt *)_sbrk_r(rep, (newsize + MEMPKT_CHUNKS)*CHUNK); /* get a new area */
	if ((int)current == -1) {
	    return 0; 			/* there ain't anymore memory to get */
	}
	current->size = newsize;	/* record size of padded area */
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_wait((int)rep->_nextf[SEMA]); /* reclaim the struct */
#endif
    } else {
	*prev = current->next;		/* remove packet from free list */
	if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
	oldsize = current->size;	/* Remember old size */
	if (oldsize - newsize > MEMPKT_CHUNKS) { /* if there is room to break it up */
	    current->size = newsize;	/* make current the one the customer gets */
	    next = (MemPkt *)((char *)(current + 1) + current->size*CHUNK);
	    next->size = oldsize - newsize - MEMPKT_CHUNKS; /* next gets the partial data */
	    FIXUP(rep, next, file, lineno, FLAGS_MALLOC);
	    minsert(rep, FREE, next);	/* put the fractional space back on the free list */
	}
    }
    FIXUP(rep, current, file, lineno, FLAGS_MALLOC);
    minsert(rep, BUSY, current);	/* put new one on the busy list */
#if MALLOC_DEBUG
    if (current->size*CHUNK-DEBUG_EXTRA < size) die("malloc'd smaller area than user asked for");
#endif
#if INCLUDE_XINU
    if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
    return (void *)(current + 1);
}

/************************************************************
 * guts_realloc_r - re-allocate previously allocated memory.
 *
 * At entry:
 *	rep - pointer to _reent struct
 *	pkt - pointer to old area
 *	size - amount to realloc
 * At exit:
 *	returns ptr to new area on success, 0 on failure
 *	Returned ptr is aligned to a double.
 */
void *guts_realloc_r(struct _reent *rep, void *pkt, int size, const char *file, int lineno) {
    int		newsize;			/* New size of packet      */
    int		oldsize;
    int		temp, oldps=0;
    MemPkt	*current, **prev, *next;
    char	*new;

    if (size < 0) return 0;	/* this is illegal. */
    if (pkt == 0) return guts_malloc_r(rep, size, file, lineno); /* no old ptr means simply to malloc */
    if ((int)pkt&(CHUNK-1)) return 0;	/* illegal parameter */
    if (size == 0) {		/* realloc with size of 0 means to free */
        guts_free_r(rep, pkt, file, lineno);
        return 0;
    }

    newsize = (size + DEBUG_EXTRA + CHUNK-1)/CHUNK; /* Round size up to next multiple of CHUNK */
    current = (MemPkt*)pkt - 1;
    oldsize = current->size;
#if INCLUDE_XINU
    if (rep->_nextf[SEMA]) xi_wait((int)rep->_nextf[SEMA]);
#endif

#if MALLOC_DEBUG
    validate_malloc_list(rep, FREE);
    validate_malloc_list(rep, BUSY);
    {
	prev = (MemPkt **)(&rep->_nextf[BUSY]);	/* make sure old area is on busy list */
	if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
	while ((next = *prev) && next != current) {
	    prev = &next->next;
	}
	if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
	if (!next) die("Area to realloc is not on BUSY list");
    }
#endif

    if (newsize == oldsize) {
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
        return pkt;			/* same size, don't do anything */
    }

    if (newsize < oldsize) {
        if (oldsize - newsize > MEMPKT_CHUNKS) {
	    if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
	    next = (MemPkt*)((char *)(current+1)+newsize*CHUNK);
	    oldsize -= newsize + MEMPKT_CHUNKS;	/* Calc trailing size */
	    next->size = oldsize;		/* Set trailing packet */
	    next->next = 0;			/* it is not part of a list */
	    FIXUP(rep, next, file, lineno, FLAGS_REALLOC); /* fixup both MemPkt's */
	    current->size = newsize;		/* Set new packet size*/
	    FIXUP(rep, current, file, lineno, FLAGS_REALLOC);
	    minsert(rep, FREE, next);		/* put fraction on free list */
	    if (!rep->_nextf[PROT]) prc_set_ipl(oldps);
#if MALLOC_DEBUG
	    validate_malloc_list(rep, FREE);
	    validate_malloc_list(rep, BUSY);
#endif
	}
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
        return pkt;			/* give back the old one */
    } else {
	MemPkt *tgt;

	tgt = (MemPkt *)((char *)(current+1)+current->size*CHUNK);
	if (!rep->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
	prev = (MemPkt **)(&rep->_nextf[FREE]);	/* look on freelist for abuting area */
	while ((next = *prev) && next != tgt) {
	    prev = &next->next;
	}
	if (next && ((temp=current->size + next->size + MEMPKT_CHUNKS) >= newsize)) {
	    *prev = next->next;			/* pluck off the free list */
	    if (temp - newsize > MEMPKT_CHUNKS) {	/* if there is room for another header */
		next = (MemPkt *)((char *)(current+1)+newsize*CHUNK);
		next->size = temp-newsize-MEMPKT_CHUNKS; /* how big new area is */
		FIXUP(rep, next, file, lineno, FLAGS_REALLOC);
		minsert(rep, FREE, next);	/* put new section on free list */
		current->size = newsize;	/* give 'em his requested new size */
	    } else {
		current->size = temp;		/* set the new size */
	    }
	    FIXUP(rep, current, file, lineno, FLAGS_REALLOC);	/* fixup new end marker, etc. */
	    if (!rep->_nextf[PROT]) prc_set_ipl(oldps);			/* don't need interlock anymore */
#if MALLOC_DEBUG
	    if (current->size*CHUNK-DEBUG_EXTRA < size) die("malloc'd smaller area than user asked for");
#endif
#if INCLUDE_XINU
	    if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
	    return pkt;				/* give 'em his old one back */
        } else {
#if INCLUDE_XINU
	    if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
            new = (char *)guts_malloc_r(rep, size, file, lineno);
            if (new) {
		memcpy((void *)new, pkt, oldsize*CHUNK - DEBUG_EXTRA );
		guts_free_r(rep, pkt, file, lineno);
#if MALLOC_DEBUG
#if INCLUDE_XINU
		if (rep->_nextf[SEMA]) xi_wait((int)rep->_nextf[SEMA]);
#endif
		validate_malloc_list(rep, FREE);
		validate_malloc_list(rep, BUSY);
#if INCLUDE_XINU
		if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
#endif
	    }
            return new;
        }
    }
    return 0;
}

void *guts_calloc_r(struct _reent *rep, int qty, int size, const char *file, int line) {
    void *ans;
    int t = qty*size;
    ans = guts_malloc_r(rep, t, file, line);
    if (ans) {
	memset(ans, 0, t);
#if MALLOC_DEBUG
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_wait((int)rep->_nextf[SEMA]);
#endif
	validate_malloc_list(rep, FREE);
	validate_malloc_list(rep, BUSY);
#if INCLUDE_XINU
	if (rep->_nextf[SEMA]) xi_signal((int)rep->_nextf[SEMA]);
#endif
#endif
    }
    return ans;
}

void *_malloc_r(struct _reent *rep, int amt) {
    return guts_malloc_r(rep, amt, 0, 0);
}

void *_realloc_r(struct _reent *rep, void *old, int amt) {
    return guts_realloc_r(rep, old, amt, 0, 0);
}

void _free_r(struct _reent *rep, void *old) {
    guts_free_r(rep, old, 0, 0);
    return;
}

extern U8 bss_end[];
extern U8 INIT_SP[];
static U8 *adj_bss_end;
#ifndef BSS_LIMIT
# define BSS_LIMIT INIT_SP
#endif
#ifndef STACK_SIZE
# define STACK_SIZE (64*1024)
#endif

static const void *mgetenv(const char *name, void *def) {
    const struct st_envar *env;
    const void *ans;
    ans = def;
    env = st_getenv(name, 0);
    if (env) ans = env->value;
    return ans;
}

U8 *prc_extend_bss(int amt) {
    U8 *where, *limit;
    int oldps, ssize;

    if (amt < 0) return 0;
    amt = (amt+7)&-8;			/* round up size to sizeof(double) */
    ssize = (int)mgetenv("STACK_SIZE", (void *)STACK_SIZE);
    limit = (U8*)mgetenv("BSS_LIMIT", (void *)BSS_LIMIT);
    if (limit) limit -= ssize;
    oldps = prc_set_ipl(INTS_OFF);
    if (!adj_bss_end) adj_bss_end = bss_end;
    if (!limit || (amt+adj_bss_end <= limit)) {
	where = adj_bss_end;
	adj_bss_end += amt;
    } else {
	where = 0;
    }
    prc_set_ipl(oldps);
    return where;
}

struct _reent *mainline_reent;
struct _reent *interrupt_reent;
struct _reent *ast_reent;
struct _reent *action_reent;
#if INCLUDE_QIO
struct _reent *qio_reent;
#endif

#ifndef JUNK_POOL_SIZE
# define JUNK_POOL_SIZE 0
#endif
#ifndef INTERRUPT_POOL_SIZE
# define INTERRUPT_POOL_SIZE 0
#endif
#ifndef AST_POOL_SIZE
# define AST_POOL_SIZE 0
#endif
#ifndef ACTION_POOL_SIZE
# define ACTION_POOL_SIZE 0
#endif
#ifndef QIO_POOL_SIZE
# define QIO_POOL_SIZE 0
#endif
#ifndef XINU_POOL_SIZE
# define XINU_POOL_SIZE 0
#endif
#if INCLUDE_XINU
extern void *_xinu_sbrk_r(struct _reent *ptr, size_t amt);
extern int _xinu_free_pool_size(struct _reent *ptr);
struct _reent *xinu_reent;
#endif

enum sbrk_name {	/* These are just for information purposes. */
    MAINLINE,		/* There is one of these for each type of memory to be sbrk'd */
    INTERRUPT,
    ACTION,
    AST,
    QIO,
    XINU
};

typedef struct sbrk_init {
    enum sbrk_name name;
    struct _reent **reent;
    const char *env_name;
    int def_size;
} SbrkInit;

static const SbrkInit brk_init_val[] = {
    { MAINLINE, &mainline_reent, "JUNK_POOL_SIZE", JUNK_POOL_SIZE },
    { INTERRUPT, &interrupt_reent, "INTERRUPT_POOL_SIZE", INTERRUPT_POOL_SIZE },
    { ACTION, &action_reent, "ACTION_POOL_SIZE", ACTION_POOL_SIZE },
    { AST, &ast_reent, "AST_POOL_SIZE", AST_POOL_SIZE },
#if INCLUDE_QIO
    { QIO, &qio_reent, "QIO_POOL_SIZE", QIO_POOL_SIZE },
#endif
#if INCLUDE_XINU
    { XINU, &xinu_reent, "XINU_POOL_SIZE", XINU_POOL_SIZE },
#endif
    { 0, 0, 0 }
};

typedef struct sbrk_pool {
    enum sbrk_name name;
    struct _reent *reent;
    U8 *memory;
    int memory_size;
    U8 *brk_value;
    int free_size;
    int inited;
} SbrkPool;

static SbrkPool sbrk_pools[n_elts(brk_init_val)-1];

static void reset_sbrk(struct _reent *ptr) {
    int oldps=0;
    int sem, prot;
    SbrkPool *spp;

#if INCLUDE_XINU
    if (ptr->_nextf[SEMA]) xi_wait((int)ptr->_nextf[SEMA]);
#endif
    if (!ptr->_nextf[PROT]) oldps = prc_set_ipl(INTS_OFF);
    sem = (int)ptr->_nextf[SEMA];
    prot = (int)ptr->_nextf[PROT];
    spp = (SbrkPool *)ptr->_nextf[POOL];
    memset(ptr, 0, sizeof(struct _reent));
    ptr->_nextf[SEMA] = (void *)sem;
    ptr->_nextf[PROT] = (void *)prot;
    ptr->_nextf[POOL] = (void *)spp;
    ptr->_stdin = &ptr->__sf[0];
    ptr->_stdout = &ptr->__sf[1];
    ptr->_stderr = &ptr->__sf[2];
    ptr->_next = 1;
    if (spp) {
	if (spp->memory_size > 0) {
	    spp->brk_value = spp->memory;
	    spp->free_size = spp->memory_size;
	    memset(spp->memory, 0, spp->memory_size); /* clear the area */
	}	    
	spp->inited = 1;
    }
    if (!ptr->_nextf[PROT]) prc_set_ipl(oldps);
#if INCLUDE_XINU
    if (ptr->_nextf[SEMA]) xi_signal((int)ptr->_nextf[SEMA]);
#endif
    return;
}

void malloc_init(void) {
    int ii, last = -1;
    SbrkPool *spp;
    const SbrkInit *init;
    int amt, oldps;

    oldps = prc_set_ipl(INTS_OFF);
    if (!adj_bss_end) {				/* if we haven't done this yet */
	prc_extend_bss(0);				/* init the end pointer */
	spp = sbrk_pools;
	init = brk_init_val;
	for (ii=0; ii < n_elts(sbrk_pools); ++ii, ++spp, ++init) {
	    struct _reent *re;

	    spp->name = init->name;		/* remember name */
	    amt = (int)mgetenv(init->env_name, (void*)init->def_size);
#if INCLUDE_XINU
	    if (ii == XINU && amt == 0) {	/* if XINU_POOL is 0, use QIO_POOL instead */
		continue;			/* by just skipping this one */
	    }
#endif
	    re = (struct _reent *)prc_extend_bss(sizeof(struct _reent)); /* get a reent struct */
	    re->_nextf[POOL] = (void *)spp;	/* point to us */
	    re->_nextf[SEMA] = 0;		/* no semaphore set yet */
	    re->_nextf[PROT] = 0;		/* assume no protection required */
	    if (ii == ACTION || ii == AST) {
		re->_nextf[PROT] = (void*)1;	/* no need to protect this with prc_set_ipl */
	    }
	    spp->reent = re;			/* remember this */
	    *init->reent = re;			/* point to our allocated reent struct */
	    if (amt < 0) {
		if (last >= 0) {
		    die("More than one POOL_SIZE of -1");
		}
		last = ii;			/* remember this and we'll do it last */
		continue;
	    }
	    if (amt) {				/* if there's anything to allocate */
		spp->memory = prc_extend_bss(amt);	/* give it to 'em */
		spp->memory_size = amt;		/* and tell 'em how big it is */
	    }
	}
	if (last >= 0) {			/* if there was a size of -1 */
	    U8 *limit;
	    int ssize;
	    ssize = (int)mgetenv("STACK_SIZE", (void *)STACK_SIZE);
	    limit = (U8 *)mgetenv("BSS_LIMIT", (void *)BSS_LIMIT);
	    if (!limit) limit = (U8*)&last;
	    limit -= ssize;			/* leave room for some stack */
	    amt = limit - prc_extend_bss(0);	/* figure out how much we can have */
	    amt &= -8;				/* round down to sizeof(double) */
	    spp = sbrk_pools + last;
	    spp->memory = prc_extend_bss(amt);
	    spp->memory_size = amt;
	    if (!spp->memory) die("No room for POOL");
	}
#if INCLUDE_XINU
	if (!xinu_reent) xinu_reent = qio_reent; /* Xinu doubles up on QIO */
#endif	    
    }
    prc_set_ipl(oldps);
    spp = sbrk_pools;
    for (ii=0; ii < n_elts(sbrk_pools); ++ii, ++spp) {
	if (spp->reent) reset_sbrk(spp->reent);
    }
    _impure_ptr = mainline_reent;
    return;
}

struct _reent *_impure_ptr;

void *_sbrk_r(struct _reent *ptr, size_t amt) {
    void *old;
    SbrkPool *spp;
#if INCLUDE_XINU
    old = _xinu_sbrk_r(ptr, amt);	/* look for reent in the xinu space */
    if (old) return old;		/* found one, give 'em the answer */
#endif
    spp = (SbrkPool *)ptr->_nextf[POOL];
    if (spp) {
	if ((amt >= 0 && amt <= spp->free_size) ||
	    (amt < 0 && amt+spp->free_size >= 0)) {
	    old = spp->brk_value;
	    spp->brk_value += amt;
	    spp->free_size -= amt;
	    return old;
	}
    }
    ptr->_errno = ENOMEM;
    return (void *)-1;
}

int _get_free_pool_size_r(struct _reent *ptr) {
    SbrkPool *spp;

#if INCLUDE_XINU
    {
	int ii;
	ii = _xinu_free_pool_size(ptr);
	if (ii >= 0) return ii;
    }
#endif
    spp = (SbrkPool *)ptr->_nextf[POOL];
    if (spp) {
	return spp->free_size;
    }
    return 0;
}

int _heap_size_r(struct _reent *rep) {
    int amt=0, oldps;
    MemPkt **prev, *cur;

    prev = (MemPkt **)(&rep->_nextf[FREE]);
    oldps = prc_set_ipl(INTS_OFF);
    while ( (cur = *prev) ) {
	amt += cur->size*CHUNK;
	prev = &cur->next;
    }
    prc_set_ipl(oldps);
    return amt;
}    

int _heap_remaining_r(struct _reent *rep) {
    int amt;

    if (!rep) return 0;
    amt = _get_free_pool_size_r(rep);
    return amt + _heap_size_r(rep);
}

void _reset_malloc_r(struct _reent *ptr) {
    reset_sbrk(ptr);
#if INCLUDE_XINU
    if (_xinu_malloc_reset(ptr)) {
	ptr->_nextf[FREE] = 0;
	ptr->_nextf[BUSY] = 0;
    }
#endif
    return;
}

int heap_remaining(void) {
    return _heap_remaining_r(_impure_ptr);
}

void reset_malloc(void) {
    _reset_malloc_r(_impure_ptr);
}
