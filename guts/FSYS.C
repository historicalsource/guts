/*
 * $Id: fsys.c,v 1.73 1997/11/01 22:48:35 shepperd Exp $
 *
 *		Copyright 1996,1997 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#include <os_proto.h>
#include <st_proto.h>
#include <phx_proto.h>
#define QIO_LOCAL_DEFINES 1
#include <qio.h>
#include <fsys.h>
#include <eer_defs.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef TEST_DISK_TIMEOUT
#define TEST_DISK_TIMEOUT	0
#endif

#if defined(WDOG) && !BOOT_FROM_DISK && !NO_WDOG
# define KICK_THE_DOG	1
#else
# define KICK_THE_DOG	0
#endif

#if FSYS_SQUAWKING || FSYS_FREE_SQUAWKING || FSYS_SYNC_SQUAWKING
# include <iio_proto.h>
IcelessIO *fsys_iop;
#endif

#if FSYS_SQUAWKING
# define FSYS_SQK(x) iio_printf x
# define OUTWHERE fsys_iop,
#else
# define FSYS_SQK(x) do { ; } while (0)
#endif

#if FSYS_SYNC_SQUAWKING
# define SYNSQK(x) iio_printf x
# define OUTWHERE fsys_iop,
#else
# define SYNSQK(x) do { ; } while (0)
#endif

/* set TEST_DISK_ERRORS non-zero to enable fake disk errors */
/* set TEST_DISK_TIMEOUT non-zero to enable fake disk timeouts */

#if !HOST_BOARD
#include <time.h>
#include <assert.h>
#endif		/* HOST_BOARD */

typedef struct opnfile_t {
    FsysOpenT *details;	/* point to user's details */
    FsysVolume *vol;	/* volume on which to create file */
    QioIOQ *ioq;	/* completion function, etc. */
    const char *path;	/* pointer to path/filename */
    int mode;		/* mode bits as defined in fcntl.h */
} FsysOpenFileT;

FsysVolume volumes[FSYS_MAX_VOLUMES];
#ifndef FSYS_QIO_BATCH
#define FSYS_QIO_BATCH	16
#endif

static FsysQio *fsysqio_pool_head;

#if MALLOC_DEBUG
static int num_fsysqios;
#endif

#if FSYS_UMOUNT
# if MALLOC_DEBUG
#  define QMOUNT_ALLOC(vol, amt) qmount_alloc(vol, amt, __FILE__, __LINE__)
#  define QMOUNT_REALLOC(vol, old, amt) qmount_realloc(vol, old, amt, __FILE__, __LINE__)
#  define QMOUNT_FREE(vol, head) qmount_free(vol, head, __FILE__, __LINE__)
# else
#  define QMOUNT_ALLOC(vol, amt) qmount_alloc(vol, amt)
#  define QMOUNT_REALLOC(vol, old, amt) qmount_realloc(vol, old, amt)
#  define QMOUNT_FREE(vol, head) qmount_free(vol, head)
# endif
#else
# define QMOUNT_ALLOC(vol, amt) QIOcalloc(1, amt)
# define QMOUNT_REALLOC(vol, old, amt) QIOrealloc(old, amt)
# define QMOUNT_FREE(vol, head) QIOfree(head)
#endif

extern QioIOQ *qio_getioq_ptr(QioIOQ **head, int size);
extern int qio_freeioq_ptr(QioIOQ *q, QioIOQ **head);

#ifdef EER_FSYS_USEALT
static void inc_bram(int arg) {
    int t;
    t = eer_gets(arg)+1;
    if (t < 256) eer_puts(arg, t);
}

#define USED_ALT() inc_bram(EER_FSYS_USEALT);
#else
#define USED_ALT() do { ; } while (0)
#endif

/************************************************************
 * fsys_getqio - Get a FsysQio from the system's pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
FsysQio *fsys_getqio(void) {
#if MALLOC_DEBUG
    ++num_fsysqios;
#endif
    return (FsysQio *)qio_getioq_ptr((QioIOQ **)&fsysqio_pool_head, sizeof(FsysQio));
}

/************************************************************
 * fsys_freeqio - Free a FsysQio as obtained from a previous
 * call to fsys_getqio().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	0 if success or 1 if queue didn't belong to pool.
 */
int fsys_freeqio(FsysQio *que) {
#if MALLOC_DEBUG
    --num_fsysqios;
#endif
    return qio_freeioq_ptr((QioIOQ *)que, (QioIOQ **)&fsysqio_pool_head);
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
/************************************************************
 * fsys_get_volume - get pointer to volume struct
 *
 * At entry:
 *	which - file descriptor of device with volume
 *
 * At exit:
 *	pointer to FsysVolume structure or 0 if error.
 */

static FsysVolume *fsys_get_volume(int which) {
    QioFile *file;
    const QioDevice *dvc;
    FsysVolume *vol;

    file = qio_fd2file(which);
    if (!file) return 0;
    dvc = file->dvc;
    if (!dvc) return 0;
    vol = (FsysVolume *)dvc->private;
    if (!vol || vol->id != FSYS_ID_VOLUME) return 0;
    return vol;
}
#endif

typedef struct fsys_ltop {
    unsigned long phys;			/* physical LBA */
    int cnt;				/* max number of sectors */
} FsysLtop;

/************************************************************
 * fsys_ltop - convert a logical sector number/count to a starting
 * physical sector number/count pair. This is an internal support
 * function for the file reader.
 *
 * At entry:
 *	ptrs - pointer to array of retrieval pointers
 *	sector - user's logical sector number
 *	count - user's sector count
 * At exit:
 *	returns a struct (_not_ a pointer to struct) containing
 *	the physical sector number and the maximum number of
 *	sectors that can be read or 0 if the logical sector is
 *	out of bounds.
 */

static FsysLtop fsys_ltop(FsysRamRP *ramp, unsigned long sector, int count) {
    FsysLtop ans;
    FsysRetPtr *ptrs;
    unsigned long lba, lbacnt;
    int nptrs;

    ans.phys = 0;		/* assume no conversion is possible */
    ans.cnt = 0;
    lba = 0;			/* look from the beginning */
    while (ramp) {
	ptrs = ramp->rptrs;
	nptrs = ramp->num_rptrs;
	for (;nptrs; --nptrs, ++ptrs) {    
	    unsigned long diff;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
	/* save doing an integer multiply if not required */
	    if (ptrs->repeat == 1 || ptrs->nblock == 1) {
		if (ptrs->repeat == 1) {
		    lbacnt = ptrs->nblocks; 
		} else {
		    lbacnt = ptrs->repeat;
		}
	    } else {
		lbacnt = ptrs->repeat*ptrs->nblocks;
	    }
	    if (!lbacnt) continue;
#else
	    if (!(lbacnt = ptrs->nblocks)) continue;
#endif
	    if (sector >= lba && sector < lba+lbacnt) {
		diff = sector-lba;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
		if (!ptrs->skip) {			/* if no skip field */
		    ans.phys = ptrs->start + diff;	/* then phys is just start + offset */
		    ans.cnt = lbacnt - diff;		/* and count is amount left in ptr block */
		} else {
		    unsigned long t;
		    t = diff/ptrs->nblocks;		/* otherwise, it gets very complicated */
		    t *= ptrs->nblocks+ptrs->skip;
		    ans.phys = ptrs->start + t;
		    t = diff%ptrs->nblocks;
		    ans.phys += t;
		    ans.cnt = ptrs->nblocks-t;
		}
#else
		ans.phys = ptrs->start + diff;
		ans.cnt = lbacnt - diff;
#endif
		if (ans.cnt > count) ans.cnt = count;	/* maximize the count to the caller's value */
		return ans;
	    }
	    lba += lbacnt;	
	}
#if !FSYS_READ_ONLY
	ramp = ramp->next;
#else
	ramp = 0;
#endif
    }
    return ans;
}

/********************************************************************
 * compute_total_free - walks the free list and computes how many free
 * sectors remain on the disk.
 *
 * At entry:
 *	vol - pointer to volume set to which the file belongs
 *
 * At exit:
 *	returns the number of computed free clusters.
 */
static int compute_total_free(FsysVolume *vol) {
    FsysRetPtr *rp;
    FsysRamFH *rfh;
    int ii, tot, prev;

    rp = vol->free;
    prev = 0;
    for (tot=ii=0; rp->start && ii < vol->free_elems; ++ii, ++rp) {
#if 1				/* for compatibility with old filesystems */
	if (rp->start < prev) break;
	prev = rp->start + rp->nblocks;
#endif
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
	tot += rp->nblocks*rp->repeat;
#else
	tot += rp->nblocks;
#endif
    }

    rfh = vol->files + FSYS_INDEX_FREE; /* point to freelist header */
    rfh->size = (ii*sizeof(FsysRetPtr)+511)&-512;
    vol->free_ffree = ii;
    return tot;
}

#if !FSYS_READ_ONLY
static int collapse_free(FsysVolume *vol, int low);

#ifndef _FSYS_FINDFREE_T_
# define _FSYS_FINDFREE_T_
typedef struct fsys_findfree_t {
    int request;	/* number of sectors to get */
    int skip;		/* number of blocks to skip */
    int exact;		/* .ne. if size must be exact */
    int lo_limit;	/* allocate above this sector */
    int hint;		/* connect to this sector if possible */
    int nelts;		/* number of elements in freelist */
    FsysRetPtr *freelist; /* ptr to freelist */
    FsysRetPtr *reply;	/* ptr to place to deposit answer */
    int which_elem;	/* gets which freelist elem modified */
} FsysFindFreeT;
#endif

/************************************************************
 * fsys_findfree - find the next free n sectors. This function
 * walks the freelist and tries to find a block of sectors that
 * matches the request'ed amount or an amount nearest. This
 * function is used internally by the filesystem and not expected
 * to be used by the casual user.
 * 
 * At entry:
 *	freet - pointer to FsysFindFreeT struct.
 *	hint - find space starting here if possible (0=don't care)
 *
 * At exit:
 *	returns 0 if success, 1 if nothing available.
 *	updates freelist accordingly.
 */
static int fsys_findfree( FsysFindFreeT *ft ) {
    int ii;
    int nearest_over, nearest_under;
    long diff_over, diff_under;
    FsysRetPtr *fp, *free;

#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
# error You need to rewrite fsys_findfree and add repeat/skip support.
#endif

    if (!ft) return 0;

    ft->which_elem = INT_MAX;
    fp = ft->freelist;
    free = ft->reply;

    if (!ft->request || !fp || !free) return 0;

#if 0 && FSYS_FREE_SQUAWKING
    {
	int ii;
	FsysRetPtr *dst;
	dst = ft->freelist;
	iio_printf(fsys_iop, "%6d: findfree before. rqst=%d, hint=%d, lo_lim=%d, items=%d.\n",
	    eer_rtc, ft->request, ft->hint, ft->lo_limit, ft->nelts);
	for (ii=0; ii < ft->nelts; ++ii, ++dst) {
	    iio_printf(fsys_iop, "\t    %3d: start=%7d, nblocks=%7d\n",
		ii, dst->start, dst->nblocks);
	}
    }
#endif

    nearest_over = nearest_under = -1;
    diff_over = LONG_MAX;
    diff_under = LONG_MIN;
    for (ii=0; ii < ft->nelts; ++ii, ++fp) { /* look through the list for one that fits */
	long d;
	int size;
	size = fp->nblocks;
	if (!size) continue;		/* empty entry */
	if (ft->hint && fp->start == ft->hint) { /* entry abuts the request */
	    nearest_over = ii;		/* give it to 'em */
	    break;
	}
	if (fp->start < ft->lo_limit) continue; /* keep looking */
	d = fp->nblocks - ft->request;
	if (d == 0) {			/* if there's one that exactly fits */
	    nearest_over = ii;
	    break;			/* use it directly */
	}
	if (d > 0) {			/* if the size is over.. */
	    if (fp->start > ft->hint) {
		nearest_over = ii;	/* issue out of the nearest largest space next to hint */
		break;
	    }
	    if (d < diff_over) {	/* ..but difference is closer */
		nearest_over = ii;	/* remember this entry */
		diff_over = d;		/* and the difference */
	    }
	} else {
	    if (d > diff_under) {	/* size of under but difference is closer */
		nearest_under = ii;	/* remember this entry */
		diff_under = d;		/* and the difference */
	    }
	}	    
    }
    if (nearest_over >= 0) {		/* there's a nearest over */
	int act;
	fp = ft->freelist + nearest_over;
	act = fp->nblocks;		/* size of free region */
	if (act > ft->request) act = ft->request; /* maximize to requested amount */
	free->start = fp->start;	/* his region starts at the free start */
	free->nblocks = act;		/* he gets what he asked for or the size of the region */
	fp->nblocks -= act;		/* take from the free region size what we gave away */
	if (fp->nblocks) {		/* if there is any left */
	    fp->start += act;		/* advance the starting point of free region */
	} else {
	    fp->start = 0;		/* gave the whole region away, zap it */
	}
	ft->which_elem = nearest_over;	/* tell 'em which one we modified */
    } else if (!ft->exact && nearest_under >= 0) {
	fp = ft->freelist + nearest_under;
	free->start = fp->start;
	free->nblocks = fp->nblocks;
	fp->start = 0;
	fp->nblocks = 0;
	ft->which_elem = nearest_under;
    } else {
	free->start = 0;
	free->nblocks = 0;
    }
    FSYS_SQK(( OUTWHERE "%6d: find_free returns %d. rqst=%d, start=%d, nblocks=%d.\n",
    	eer_rtc, free->start == 0, ft->request, free->start, free->nblocks));
#if 0 && FSYS_FREE_SQUAWKING
    {
	int ii;
	FsysRetPtr *dst;
	dst = ft->freelist;
	iio_printf(fsys_iop, "        after. %d items. Patched entry %d\n",
	    ft->nelts, ft->which_elem);
	for (ii=0; ii < ft->nelts; ++ii, ++dst) {
	    iio_printf(fsys_iop, "\t   %c%3d: start=%7d, nblocks=%7d\n",
		ii == ft->which_elem ? '*' : ' ', ii, dst->start, dst->nblocks);
	}
	iio_printf(fsys_iop, "        Returned to caller: start=%d, nblocks=%d\n",
	    free->start, free->nblocks);
    }
#endif
    return (free->start == 0);
}
        
/********************************************************************
 * extend_file - extends the retrieval pointer set on a file the
 * specified number of sectors.
 *
 * At entry:
 *	vol - pointer to volume set to which the file belongs
 *	fh - pointer to FsysRamFH of file to be extended
 *	rqst - number of sectors to add to the file
 *	rp - pointer to array of ram retrieval pointers
 *	where - area of disk on which to extend file
 *
 * At exit:
 *	returns 0 on success, and one of FSYS_EXTEND_xxx on error.
 *	Retrieval pointer set in the RamFH may have been updated.
 */

static int extend_file(FsysVolume *vol, int rqst, FsysRamRP *rp, int where) {
    int cnt, low;
    FsysRetPtr *arp, ans;
    FsysFindFreeT freet;

/* Check to see if there is room on the disk for the requested
 * extension.
 */
    if (vol->total_free_clusters < rqst) {
	return FSYS_EXTEND_FULL;	/* filesystem is full */
    }

/* Walk through the entire retrieval pointer array and count how many
 * members there are. At the same time, find the last entry in the list.
 */
    cnt = 0;
    while (1) {
	cnt += rp->num_rptrs;
	if (!rp->next) break;
	rp = rp->next;
    }

    freet.skip = 0;
    freet.exact = 0;
    freet.freelist = vol->free;
    freet.nelts = vol->free_ffree;
    low = INT_MAX;

    FSYS_SQK(( OUTWHERE "%6d: extend_file: asked for %d. cnt=%d\n",
    	eer_rtc, rqst, cnt));

/* Keep trying to extend until either the disk fills up, or the request
 * is satisfied.
 */
    while (rqst > 0) {
	if (cnt >= FSYS_MAX_FHPTRS) {
	    return FSYS_EXTEND_2MNYRP;	/* too many retrieval pointers */
	}

	if (rp->num_rptrs > 0) {
	    arp = rp->rptrs + rp->num_rptrs - 1; /* point to last active RP element */
	    freet.hint = arp->start + arp->nblocks; /* try to find a connecting region */
	} else {
	    arp = 0;
	    freet.hint = 0;			/* choose no particular place */
	}
	freet.reply = &ans;
	freet.lo_limit = FSYS_HB_ALG(where, vol->maxlba);
	freet.request = rqst;
	if (fsys_findfree( &freet )) {
	    return FSYS_EXTEND_FULL;		/* disk has become full */
	}
	if (arp && ans.start == freet.hint) {	/* did we get an ajoining region? */
	    arp->nblocks += ans.nblocks;	/* just increase the size of the last one */
	} else {
	    arp = 0;
	}
	if (!arp) {
/* Check to see if there is room in the current retrieval pointer array.
 */
	    if (rp->num_rptrs >= rp->rptrs_size) {

/* There isn't, so create another chunk of retrieval pointers.
 */
		if (!cnt) {				/* at the top level, we don't need a RamRP */
#if FSYS_TIGHT_MEM
# define NUM_RET_PTR_TOP	1		/* one retrevial pointer at top level */
# define NUM_RET_PTR_CHAIN	8		/* overflow into 8 */
#else
# define NUM_RET_PTR_TOP	1		/* one retrevial pointer at top level */
# define NUM_RET_PTR_CHAIN	8		/* overflow into 8 */
#endif
		    FSYS_SQK(( OUTWHERE "        extending top level rptrs by %d\n", NUM_RET_PTR_TOP));
		    rp->rptrs = (FsysRetPtr *)QIOcalloc(NUM_RET_PTR_TOP*sizeof(FsysRetPtr),1);
		    if (!rp->rptrs) return FSYS_EXTEND_NOMEM;
		    rp->rptrs_size = NUM_RET_PTR_TOP;	/* this is how many are in there */
		    rp->mallocd = 1;		/* show rptrs is mallocd */
		} else {
		    FsysRamRP *trp;
		    FSYS_SQK(( OUTWHERE "       extending rptrs chain by %d\n", NUM_RET_PTR_CHAIN));
		    trp = (FsysRamRP *)QIOcalloc(sizeof(FsysRamRP)+NUM_RET_PTR_CHAIN*sizeof(FsysRetPtr),1);
		    if (!trp) {
			return FSYS_EXTEND_NOMEM;	/* oops, ran out of memory */
		    }
		    rp->next = trp;
		    rp = trp;			/* tell last guy about new one */
		    rp->rptrs = (FsysRetPtr *)(trp+1); /* point to an array of FsysRetPtr's */
		    rp->rptrs_size = NUM_RET_PTR_CHAIN; /* this is how many are in there */
		    rp->mallocd = 0;		/* not individually mallocd */
		}
		rp->next = 0;			/* new guy is last on the list */
		rp->num_rptrs = 0;		/* no pointers yet. */
	    }
	    arp = rp->rptrs + rp->num_rptrs;	/* point to RP element */
	    *arp = ans;				/* give 'em the new allocation */
	    ++rp->num_rptrs;			/* count the RP */
	    ++cnt;				/* count the RP */
	}
	rqst -= ans.nblocks;			/* take the amount we obtained from request */
	vol->total_free_clusters -= ans.nblocks; /* take from volume total */
	vol->total_alloc_clusters += ans.nblocks; /* count allocated clusters */
	if (low > freet.which_elem) low = freet.which_elem;
    }
/*
 * Successfully extended the file(s) 
 */
    return collapse_free(vol, INT_MAX);		/* adjust the freelist */
}
#endif

/************************************************************
 * fsys_qread - a file read primitive. This function will read
 * n sectors from a file as specified by the list of retrieval
 * pointers, the starting relative sector number and the sector
 * count. It queue's a read to the I/O subsystem and uses itself
 * as a completion function. It will continue to queue input until
 * the required bytes have been transferred or an error occurs.
 * The I/O subsystem will call this function as a completion
 * routine therefore it will (may) finish asychronously from the
 * calling program. This function is used internally by the
 * filesystem and not expected to be used by the casual user.
 * 
 * At entry:
 *	arg - pointer to read argument list. The list must be in
 *		static memory (i.e., not on the stack)
 * At exit:
 *	returns nothing. The function queue's an input which will
 *	complete asynchronously from the calling program.
 */

static void fsys_qread(QioIOQ *ioq) {
    FsysQio *arg;

    arg = (FsysQio *)ioq;
    while (1) {
	switch (arg->state) {
	    default:
		ioq->iostatus = FSYS_IO_FATAL;	/* fatal internal error */
		break;

	    case 0:
		ioq->complete = fsys_qread;
		if (arg->callers_ioq) {
		    ioq->timeout = arg->callers_ioq->timeout;
#if FSYS_DEFAULT_TIMEOUT
		    if (!ioq->timeout) ioq->timeout = FSYS_DEFAULT_TIMEOUT;
#endif
		}
		arg->total = 0;			/* no bytes xferred */
		if (!arg->u_len) break;		/* a 0 length read simply exits */
		arg->state = 1;
		continue;

	    case 1: {
		FsysLtop ltop;
		int len;
		ltop = fsys_ltop(arg->ramrp, arg->sector, (arg->u_len+511)/512);
		if (!ltop.phys) {
		    if (!ioq->iocount) ioq->iostatus = QIO_EOF;
		    break;
		}
		arg->state = 2;
		arg->count = ltop.cnt;
		len = arg->count*512;
		if (len > arg->u_len) len = arg->u_len;
		if ((len&511) && (len&-512)) len = (len&-512);	/* get multiple of sectors first */
		qio_readwpos(ioq, ltop.phys, arg->buff, len);
		return;
	    }

	    case 2: {
		int t;
		if (!QIO_ERR_CODE(ioq->iostatus)) {
		    t = ioq->iocount;	    
		    arg->total += t;
		    arg->u_len -= t;
		    arg->buff += t;			/* bump buffer by byte count */
		    t = (t+511)/512;			/* round up to sector size */
		    arg->sector += t;			/* advance logical sector number */
		    if (arg->u_len <= 0) break;
		    arg->state = 1;
		    continue;
		}
		break;
	    }
	}
	break;
    }
    arg->state = 0;				/* clean up after ourselves */
    ioq->iocount = arg->total;
    ioq->complete = arg->compl;			/* restore completion rtn address */
    qio_complete(ioq);				/* call completion */
    return;
}

#if !FSYS_READ_ONLY
static int wr_ext_file(FsysQio *arg, QioFile *fp, FsysVolume *v) {
    int ii, jj, fid, def;
    QioIOQ *ioq;
    FsysRamFH *rfh;
    FsysRamRP *rp;

    ioq = (QioIOQ *)arg;
    if (v && fp && fp->dvc) {
	fid = (int)fp->private;
	rfh = v->files + (fid&0x00FFFFFF);
	def = arg->sector - rfh->clusters;
	if (def < 0) def = 0;
	def += rfh->def_extend ? rfh->def_extend : FSYS_DEFAULT_EXTEND;
	if (def*FSYS_MAX_ALTS >= v->total_free_clusters) {	/* maybe there's no room */
	    for (ii=jj=0; ii < FSYS_MAX_ALTS; ++ii) {	/* compute how many copies */
		rp = rfh->ramrp + ii;
		if (rp->num_rptrs) ++jj;
	    }
	    if (def*jj >= v->total_free_clusters) {	/* check again for room */
		ioq->iostatus = FSYS_EXTEND_FULL;	/* no room for extension */
		return 1;
	    }
	}
	for (jj=0; jj < FSYS_MAX_ALTS; ++jj) {
	    rp = rfh->ramrp + jj;
	    if (!rp->num_rptrs) continue;		/* no copy here */
	    ii = extend_file(v, def, rp, jj);
	    if (ii) {
		ioq->iostatus = ii;
		break;				/* ran out of room */
	    }
	}
	rfh->clusters += def;			/* add to total allocation */
	if (ioq->iostatus) return 1;
	return 0;				/* recompute retrieval pointer set */
    } 
    return 2;
}
#endif

/************************************************************
 * fsys_qwrite - a file write primitive. This function will write
 * n sectors to a file as specified by the list of retrieval
 * pointers, the starting relative sector number and the sector
 * count. It queue's a write to the I/O subsystem and uses itself
 * as a completion function. It will continue to queue output until
 * the required bytes have been transferred or an error occurs.
 * The I/O subsystem will call this function as a completion
 * routine therefore it will (may) finish asychronously from the
 * calling program. This function is used internally by the
 * filesystem and not expected to be used by the casual user.
 * 
 * At entry:
 *	arg - pointer to write argument list. The list must be in
 *		static memory (i.e., not on the stack)
 * At exit:
 *	returns nothing. The function queue's an input which will
 *	complete asynchronously from the calling program.
 */

static void fsys_qwrite(QioIOQ *ioq) {
    FsysQio *arg;

    arg = (FsysQio *)ioq;
    while (1) {
	switch (arg->state) {
	    default:
		ioq->iostatus = FSYS_IO_FATAL;	/* fatal internal error */
		break;

	    case 0:
		ioq->complete = fsys_qwrite;
		if (!arg->u_len) break;		/* a 0 length write simply exits */
		if (arg->callers_ioq) {
		    ioq->timeout = arg->callers_ioq->timeout;
#if FSYS_DEFAULT_TIMEOUT
		    if (!ioq->timeout) ioq->timeout = FSYS_DEFAULT_TIMEOUT;
#endif
		}
		arg->state = 1;
		continue;

	    case 1: {
		FsysLtop ltop;
		int len;

		ltop = fsys_ltop(arg->ramrp, arg->sector, (arg->u_len+511)/512);
		if (!ltop.cnt) {
#if !FSYS_READ_ONLY
		    int sts;
		    FsysVolume *v;
		    QioFile *fp;
		    fp = arg->fsys_fp;
		    if (fp && fp->dvc && fp->dvc->private) {
			v = (FsysVolume *)fp->dvc->private;
			arg->state = 3;
			sts = qio_getmutex(&v->mutex, fsys_qwrite, ioq);
			if (!sts) return;
			if (sts == QIO_MUTEX_NESTED) {
			    FSYS_SQK(( OUTWHERE "%6d: fsys_qwrite, extending. Volumue already locked\n",
				eer_rtc));
			    arg->state = 1;
			    sts = wr_ext_file(arg, fp, v);
			    if (sts == 0) continue;	/* recompute lsn */
			    if (sts == 1) break;
			} else {
			    ioq->iostatus = sts;
			    break;
			}
		    } 
#endif
		    ioq->iostatus = QIO_EOF; /* always return an EOF if tried to write too much */
		    break;
		}
		arg->state = 2;
		arg->count = ltop.cnt;
		len = arg->count*512;
		if (len > arg->u_len) len = arg->u_len;
		if ((len&511) && (len&-512)) len = (len&-512);
		qio_writewpos(ioq, ltop.phys, arg->buff, len);
		return;
	    }

	    case 2: {
		int t;
		if (QIO_ERR_CODE(ioq->iostatus) == 0) {
		    t = ioq->iocount;	    
		    arg->total += t;
		    arg->u_len -= t;
		    arg->buff += t;			/* advance buffer pointer by 'word' count */
		    arg->sector += (t+511)/512;		/* advance logical sector number */
		    if (arg->u_len <= 0) break;
		    arg->state = 1;
		    continue;
		}
		break;
	    }

#if !FSYS_READ_ONLY
	    case 3: {
		int sts;
		FsysVolume *v;
		QioFile *fp;
		fp = arg->fsys_fp;
		v = (FsysVolume *)fp->dvc->private;
		arg->state = 1;				
		sts = wr_ext_file(arg, fp, v);
		qio_freemutex(&v->mutex, ioq);		/* free the volume */
		if (sts == 0) continue;			/* recompute lsn */
		if (sts == 1) break;			/* die */
		ioq->iostatus = QIO_EOF; /* always return an EOF if tried to write too much */
		break;
	    }
#endif
	}
	break;
    }
    arg->state = 0;		/* clean up after ourselves */
    ioq->complete = arg->compl;
    qio_complete(ioq);
    return;
}

#if MALLOC_DEBUG
# define ALLOC_EXTRAS ,const char *file, int lineno
# define QM_ALLOC_CALLOC(x,y) guts_calloc_r( (void *)qio_reent, x, y, file, lineno)
# define QM_ALLOC_REALLOC(x,y) guts_realloc_r( (void *)qio_reent, x, y, file, lineno)
# define QM_ALLOC_FREE(x) guts_free_r((void *)qio_reent, x, file, lineno)
#else
# define ALLOC_EXTRAS 
# define QM_ALLOC_CALLOC(x,y) QIOcalloc(x,y)
# define QM_ALLOC_REALLOC(x,y) QIOrealloc(x,y)
# define QM_ALLOC_FREE(x) QIOfree(x)
#endif

#if FSYS_UMOUNT
static void qmount_free(FsysVolume *vol, void *old ALLOC_EXTRAS ) {
    int ii;
    for (ii=0; ii < vol->freem_indx; ++ii) {
	if (vol->freemem[ii] == old) {
	    QM_ALLOC_FREE(old);
	    vol->freemem[ii] = 0;
	    break;
	}
    }
    return;
}

static void *qmount_alloc(FsysVolume *vol, int amt ALLOC_EXTRAS ) {
    void *ans, *nmem;
    ans = QM_ALLOC_CALLOC(amt, 1);
    if (!ans) {
	return ans;
    }
    if (vol->freem_indx >= vol->freem_elems) {
	vol->freem_elems += 10;
	nmem = QIOrealloc(vol->freemem, vol->freem_elems*sizeof(void *));
	if (!nmem) {
	    QIOfree(ans);
	    return 0;
	}
	vol->freemem = nmem;
    }
    vol->freemem[vol->freem_indx++] = ans;
    return ans;
}

static void *qmount_realloc(FsysVolume *vol, void *old, int amt ALLOC_EXTRAS ) {
    void *ans;
    int ii;
    ans = QM_ALLOC_REALLOC(old, amt);
    for (ii=0; ii < vol->freem_indx; ++ii) {
	if (vol->freemem[ii] == old) {
	    vol->freemem[ii] = ans;
	    break;
	}
    }
    if (ans && ii >= vol->freem_indx) {
	if (vol->freem_indx >= vol->freem_elems) {
	    vol->freem_elems += 20;
	    vol->freemem = QIOrealloc(vol->freemem, vol->freem_elems*sizeof(void *));
	    if (!vol->freemem) {
		QIOfree(ans);
		return 0;
	    }
	}
	vol->freemem[vol->freem_indx++] = ans;
    }
    return ans;
}
#endif

#if 0
static volatile int fatal_dir_error;

static void bump_fatal(int arg) {
    ++fatal_dir_error;
    return;
}

static void check_dirs(void) {
    FsysDirEnt *de;
    FsysRamFH *rfh;

    rfh = volumes[0].files;
    if (!rfh) return;			/* volume not mounted yet */
    rfh += 2;				/* advance to root directory */
    if (!rfh->directory) {
	bump_fatal(0);
	return;
    }
    de = rfh->directory[15];		/* point to directory '.' */
    if (!de) {
	bump_fatal(1);
	return;
    }
    if (!de->name || !de->fid || !de->generation) {
	bump_fatal(2);
	return;
    }
    de = rfh->directory[25];		/* point to directory '.' */
    if (!de) {
	bump_fatal(3);
	return;
    }
    if (!de->name || !de->fid || !de->generation) {
	bump_fatal(4);
	return;
    }
    return;
}
#endif

#if FSYS_UMOUNT
static void qmount_freeall(FsysVolume *vol) {
    int ii;
    for (ii=0; ii < vol->freem_indx; ++ii) {
	if (vol->freemem[ii]) {
	    QIOfree(vol->freemem[ii]);
	    vol->freemem[ii] = 0;
	}
    }
    if (vol->freemem) {
	QIOfree(vol->freemem);
	vol->freemem = 0;
    }
    vol->freem_indx = vol->freem_elems = 0;
    return;
}
#endif

static int hashit(const char *string) {
   int hashv=0;
   unsigned char c;
   while ((c= *string++)) {
      hashv = (hashv<<3) + (hashv<<1) + hashv + c;	/* hashv = hashv*11 + c */
   }
   return (hashv %= FSYS_DIR_HASH_SIZE) >= 0 ? hashv : -hashv;
}

static void insert_ent(FsysDirEnt **hash, FsysDirEnt *new) { /* insert into hash table */
    int hashv;
    FsysDirEnt *cur, **prev;

#if 0
    if (!new->name || !new->fid || !new->generation) {
	bump_fatal(10);
	return;
    }
#endif
    hashv = hashit(new->name);		/* compute a hash value */
    prev = hash + hashv;		/* record insertion point */
    while ((cur = *prev)) {		/* walk the chain */
	if (strcmp(cur->name, new->name) > 0) break;
	prev = &cur->next;
    }
    *prev = new;			/* old guy points to new one */
    new->next = cur;			/* new one points to next guy */
    return;				/* and that is all there is to it */
}

static FsysDirEnt *find_ent(FsysDirEnt **hash, const char *name) { /* remove from hash table */
    int hashv;
    FsysDirEnt *cur, **prev;

    hashv = hashit(name);		/* compute a hash value */
    prev = hash + hashv;		/* record insertion point */
    while ((cur = *prev)) {		/* walk the chain */
	if (strcmp(cur->name, name) == 0) return cur;	/* found it, give it to 'em */
	prev = &cur->next;
    }
    return 0;				/* not in the list */
}

#if !FSYS_READ_ONLY
static FsysDirEnt *remove_ent(FsysDirEnt **hash, const char *name) { /* remove from hash table */
    int hashv;
    FsysDirEnt *cur, **prev;

    hashv = hashit(name);		/* compute a hash value */
    prev = hash + hashv;		/* record insertion point */
    while ((cur = *prev)) {		/* walk the chain */
	if (strcmp(cur->name, name) == 0) {
	    *prev = cur->next;		/* pluck this from the list */
	    cur->next = 0;		/* break the link */
	    return cur;
	}
	prev = &cur->next;
    }
    return 0;				/* not in the list */
}
#endif

static int lookup_filename(FsysLookUpFileT *lu) {
    char name[256];				/* room for temp copy of path */
    const char *fname, *path;
    FsysDirEnt *dir;
    FsysRamFH *top;
    int len, fid;

    if (!lu || !lu->vol || !lu->path) {
	return FSYS_LOOKUP_INVARG;		/* it can't be there */
    }
    lu->owner = 0;				/* assume no owner directory */
    lu->file = 0;				/* assume no file either */
    top = lu->top ? lu->top : lu->vol->files + FSYS_INDEX_ROOT;
    fname = lu->path;
    if (*fname == QIO_FNAME_SEPARATOR) ++fname; /* eat leading '/' */
    lu->depth = 0;
    while ((path=strchr(fname, QIO_FNAME_SEPARATOR))) { /* see if we have to traverse a directory */
	if (!top->directory) {
	    return FSYS_LOOKUP_NOPATH;		/* not a directory, so file not found */
	}
	len = path - fname;
	strncpy(name, fname, len);
	name[len] = 0;
	dir = find_ent(top->directory, name);	/* find the directory entry */
	if (!dir || !(fid=dir->fid)) {
	    return FSYS_LOOKUP_NOPATH;		/* directory not found */
	}
	top = lu->vol->files + fid;		/* get pointer to new top */
	if (top->generation != dir->generation) {
	    return FSYS_LOOKUP_NOPATH;
	}
	fname = path + 1;			/* skip to next filename part */
	if (++lu->depth > 31) {
	    return FSYS_LOOKUP_TOODEEP;
	}
    }
    if (!top->directory) {
	return FSYS_LOOKUP_NOPATH;		/* not looking in a directory */
    }
    lu->owner = top;				/* pass back pointer to found directory */
    lu->fname = fname;				/* record name of file we looked for */
    dir = find_ent(top->directory, fname);	/* look up the filename */
    lu->dir = dir;				/* record pointer to directory entry */
    if (dir && (fid=dir->fid)) {
	lu->file = lu->vol->files + fid;	/* pass back the file */ 
	if (lu->file->generation == dir->generation) {
	    return FSYS_LOOKUP_SUCC|SEVERITY_INFO; /* it worked */
	}
    }
    return FSYS_LOOKUP_FNF;			/* file not found */
}

#if !FSYS_READ_ONLY
static int add_to_unused(FsysVolume *vol, int fid) {
    int ii;
    unsigned long *ulp;

    ulp = vol->unused;
    for (ii=0; ii < vol->unused_ffree; ++ii) {
	if (fid == *ulp++) return 0;		/* already in the unused list */
    }
    if (vol->unused_ffree >= vol->unused_elems) {
	vol->unused_elems += 32;		/* room for 32 updates */
	ulp = (U32*)QMOUNT_REALLOC(vol, vol->unused, vol->unused_elems*sizeof(long));
	if (!ulp) {
	    return FSYS_CREATE_NOMEM;
	}
	vol->unused = ulp;
    }
    vol->unused[vol->unused_ffree++] = fid;
    return 0;
}
#endif

#if !FSYS_READ_ONLY || FSYS_UPD_FH
static int fsys_sync(FsysSyncT *f, int how);
static int fatal_dirty_error;

static int add_to_dirty(FsysVolume *vol, int fid, int end) {
    int ii;
    unsigned long *ulp;

    FSYS_SQK(( OUTWHERE "%6d: Adding fid %08lX to dirty list\n", eer_rtc, fid));
    if (fid < 0 || fid >= vol->files_ffree) {
	FSYS_SQK(( OUTWHERE "%6d: add_to_dirty Rejected fid %08lX. files_ffree=%d\n",
    			eer_rtc, fid, vol->files_ffree));
	++fatal_dirty_error;
	return 0;
    }
    if (!end) {
	ulp = vol->dirty;
	for (ii=0; ii < vol->dirty_ffree; ++ii) {
	    if (fid == *ulp++) {
		FSYS_SQK(( OUTWHERE "%6d: add_to_dirty rejected fid %08lX cuz it's already there\n",
				eer_rtc, fid));
		return 0;		/* already in the dirty list */
	    }
	}
    }
    if (vol->dirty_ffree >= vol->dirty_elems) {
	int new;
	new = vol->dirty_elems + 32;
	ulp = (U32*)QMOUNT_REALLOC(vol, vol->dirty, new*sizeof(long));
	if (!ulp) {
	    return FSYS_CREATE_NOMEM;
	}
	vol->dirty = ulp;
	memset((char *)(vol->dirty+vol->dirty_elems), 0, 32*sizeof(long));
	vol->dirty_elems = new;			/* room for 32 updates */
	FSYS_SQK(( OUTWHERE "%6d: Increased size of dirty area\n", eer_rtc));
    }
    FSYS_SQK(( OUTWHERE "%6d: fid %08lx at dirty index %d\n", eer_rtc, fid, vol->dirty_ffree));
    vol->dirty[vol->dirty_ffree++] = fid;
    if (vol->dirty_ffree >= 16) {		/* Getting full, don't wait for timeout */
	FSYS_SQK(( OUTWHERE "%6d: kick started sync task\n", eer_rtc));
	fsys_sync(&vol->sync_work, FSYS_SYNC_BUSY_NONTIMER); /* startup a sync task */
    }
    return 0;
}
#endif

#if !FSYS_READ_ONLY
static int mkroom_free(FsysVolume *vol) {
    FsysRamFH *rfh;
    int ii, size, sts;
    FsysRetPtr *ulp;

    if (vol->free_ffree < vol->free_elems) return 0;	/* there's room */
#if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
    iio_printf(fsys_iop, "%6d: Increasing size of free list. free_ffree=%d, free_elems = %d\n",
		eer_rtc, vol->free_ffree, vol->free_elems);
#endif
    rfh = vol->files + FSYS_INDEX_FREE;		/* point to freelist file */
    vol->free_elems += 512/sizeof(FsysRetPtr);	/* add another sector's worth of elements */
    size = (vol->free_elems*sizeof(FsysRetPtr)+511)&-512;	/* multiple of sector size */
    ulp = (FsysRetPtr *)QMOUNT_REALLOC(vol, vol->free, size);
    if (!ulp) return FSYS_EXTEND_NOMEM;		/* ran out of memory */
    vol->free = ulp;
    size /= 512;				/* get size in sectors */
    if (size > rfh->clusters) {			/* need to add some more sectors */
#if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
	FSYS_SQK(( OUTWHERE "%6d: Increasing size of free list file. size=%d, clust=%d\n",
		    eer_rtc, size, rfh->clusters));
#endif
	rfh->clusters += 10;			/* add 10 more sectors */    
	for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {	/* need to extend the freelist file */
	    sts = extend_file(vol, 10, rfh->ramrp + ii, ii);
	    if (sts) return sts;			/* couldn't extend file */
	}
    }
    add_to_dirty(vol, FSYS_INDEX_FREE, 0);	/* make sure the freelist gets updated */
    return 0;
}
	    		
static int upd_index_bits(FsysVolume *vol, int fid) {
    int lw, le, bits;
    U32 *ulp;

    bits = ((vol->files_ffree+1)*FSYS_MAX_ALTS*sizeof(long)+511)/512; /* number of sectors required to hold index file */
    bits = (bits+31)/32;			/* number of longs rqd to hold bit map */
    FSYS_SQK(( OUTWHERE "%6d: upd_index_bits: fid=%08lX, Bits=%d, elems=%d\n",
		eer_rtc, fid, bits, vol->index_bits_elems));
    if (bits >= vol->index_bits_elems) {
	int newsize, len;
	newsize = vol->index_bits_elems + bits + 8;
	FSYS_SQK(( OUTWHERE "%6d: upd_index_bits: Increasing size. newsize=%d\n", eer_rtc, newsize));
	ulp = (U32*)QMOUNT_REALLOC(vol, vol->index_bits, newsize*sizeof(long));
	if (!ulp) {
	    return FSYS_FREE_NOMEM;
	}
	len = newsize - vol->index_bits_elems; /* size of additional area */
	memset((char *)(ulp + vol->index_bits_elems), 0, len*sizeof(U32));
	vol->index_bits = ulp;
	vol->index_bits_elems = newsize;
    }
    lw = fid*FSYS_MAX_ALTS*sizeof(long); /* byte position in index file of first byte */
    le = lw + FSYS_MAX_ALTS*sizeof(long)-1; /* byte position of last byte in array */
    lw /= 512;				/* sector position in index file of first byte */
    fid = lw&31;			/* bit position in bitmap element of first byte */
    lw /= 32;				/* longword element in bitmap of first byte */
    FSYS_SQK(( OUTWHERE "%6d: upd_index_bits: lw=%d, new=%08lX, old=%08lX\n",
		eer_rtc, lw, 1<<fid, vol->index_bits[lw]));
    vol->index_bits[lw] |= 1<<fid;	/* set bit in bitmap */
    le /= 512;				/* sector position in index file of last byte */
    fid = le&31;			/* bit position in bitmap element of last byte */
    le /= 32;				/* longword element in bitmap of last byte */
    vol->index_bits[le] |= 1<<fid;	/* set bit in bitmap of last byte */
    return 0;
}

static int collapse_free(FsysVolume *vol, int low) {	/* squeeze all the empty space out of free list */
    FsysRetPtr *dst, *src, *lim;
#if FSYS_SQUAWKING
    int old_start = vol->free_start, old_ffree=vol->free_ffree;
#endif

#if 0 && FSYS_FREE_SQUAWKING
    {
	int ii;
	dst = vol->free;
	iio_printf(fsys_iop, "%6d: collapse_free before. %d items\n", eer_rtc, vol->free_ffree);
	for (ii=0; ii < vol->free_ffree; ++ii, ++dst) {
	    iio_printf(fsys_iop, "\t   %3d: start=%7d, nblocks=%7d\n",
		ii, dst->start, dst->nblocks);
	}
    }
#endif
    dst = vol->free;
    src = dst+1;			/* point to free list */
    lim = dst + vol->free_ffree;	/* point to end of list */
    while (src < lim) {			/* walk the whole list */
	unsigned long s, ss, e, ds, de;
	s = src->start;
	ss = src->nblocks;
	e = s + ss;
	ds = dst->start;
	de = ds + dst->nblocks;
	if (ss) {			/* the source has to have a size */
	    if (!de) {			/* if the destination has no size */
		*dst = *src;		/* just copy the source to it */
	    } else {
		if (s <= de) {		/* if the regions touch or overlap */
		    dst->nblocks += ss - (de - s);
		    src->start = src->nblocks = 0;
		    if (dst - vol->free < low) low = dst - vol->free;
		} else {
		    ++dst;			/* advance destination */
		    if (dst != src) *dst = *src; /* copy source if in different locations */
		}
	    }
	}
	++src;				/* advance the source */
    }
    ++dst;
    vol->free_ffree = dst - vol->free; /* compute new length */
    if (dst - vol->free < vol->free_elems) {
	dst->start = dst->nblocks = 0;	/* follow with a null entry if there's room */
    }
    if (low < vol->free_start) vol->free_start = low;
#if FSYS_SQUAWKING
    if (old_start != vol->free_start || old_ffree != vol->free_ffree) {
	FSYS_SQK(( OUTWHERE "%6d: collapse_free entry: start=%d, ffree=%d; exit: start=%d, ffree=%d\n",
	    eer_rtc, old_start, old_ffree, vol->free_start, vol->free_ffree));
    } else {
	FSYS_SQK(( OUTWHERE "%6d: collapse_free, start=%d, ffree=%d. No changes.\n",
	    eer_rtc, vol->free_start, vol->free_ffree));
    }
#endif
#if 0 && FSYS_FREE_SQUAWKING
    {
	int ii;
	dst = vol->free;
	iio_printf(fsys_iop, "        after. %d items\n", vol->free_ffree);
	for (ii=0; ii < vol->free_ffree; ++ii, ++dst) {
	    iio_printf(fsys_iop, "\t   %3d: start=%7d, nblocks=%7d\n",
		ii, dst->start, dst->nblocks);
	}
    }
#endif
    return 0;
}

static int back_to_free(FsysVolume *vol, FsysRetPtr *rp) {
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
# error You need to re-write back_to_free to use the skip repeat feature
#else
    int ii, sts, amt, reuse;
    int old_start, old_end, new_start, new_end;
    FsysRetPtr *vrp;
    
    vrp = vol->free;
    new_start = rp->start;
    new_end = rp->start + rp->nblocks;
#if 0 && FSYS_FREE_SQUAWKING
    {
	int ii;
	FsysRetPtr *dst;
	dst = vol->free;
	iio_printf(fsys_iop, "%6d: back_to_free before. %d items. Adding start=%d, nblocks=%d\n",
    		eer_rtc, vol->free_ffree, new_start, rp->nblocks);
	for (ii=0; ii < vol->free_ffree; ++ii, ++dst) {
	    iio_printf(fsys_iop, "\t   %3d: start=%7d, nblocks=%7d\n",
		ii, dst->start, dst->nblocks);
	}
    }
#endif
    for (amt=ii=0; ii < vol->free_ffree; ++ii, ++vrp) {
	old_start = vrp->start;
	old_end = vrp->start + vrp->nblocks;
	if (new_start > old_end) continue;	/* insertions are maintained in a sorted list */
	if (new_end < old_start) break;		/* need to insert a new entry */
	sts = ((new_start == old_end)<<1) | (new_end == old_start);
	if (sts) {
	    amt = rp->nblocks;
	    vrp->nblocks += amt;
	    if ((sts&1)) vrp->start = new_start;
	    vol->total_free_clusters += amt;
	    vol->total_alloc_clusters -= amt;
	    return collapse_free(vol, ii);
	}
	if (new_start >= old_start) {	/* overlaps are actually errors, but ... */
	    if (new_end >= old_end) {	/* we ignore them as errors, and handle them */
		amt = new_end - old_end;
		vrp->nblocks += amt;	/* just move end pointer in free list */
		vol->total_free_clusters += amt;
		vol->total_alloc_clusters -= amt;
	    }
	    return collapse_free(vol, ii);
	}
	amt = old_start - new_start;
	if (new_end > old_end) amt += new_end - old_end;
	vrp->nblocks += amt;		/* move the end pointer */
	vol->total_free_clusters += amt;
	vol->total_alloc_clusters -= amt;
	vrp->start = new_start;		/* move the start pointer */
	return collapse_free(vol, ii);
    }
    reuse = 0;
    if (vrp > vol->free) {
	--vrp;				/* look back one slot */
	if (!vrp->nblocks) {		/* is it empty? */
	    while (vrp > vol->free) {	/* yes, then keep looking back */
		--vrp;
		if (vrp->nblocks) break;	/* until a non-empty one is found */
	    }
	    if (vrp->nblocks) ++vrp;	/* if we stopped because of a non-empty, advance over it */
	    reuse = 1;
	} else {
	    ++vrp;			/* nevermind */
	}
    }
    if (!reuse) {
	++vol->free_ffree;		/* advance pointer */
	sts = mkroom_free(vol);		/* make sure there is room for insertion */
	if (sts) return sts;		/* there isn't, can't update */
	vrp = vol->free + ii;		/* vol->free might have moved */
	if (ii < vol->free_ffree-1) {	/* if in the middle */
	    memmove((char *)(vrp+1), (char *)vrp, (vol->free_ffree-1-ii)*sizeof(FsysRetPtr));
	}
	if (vol->free_ffree < vol->free_elems) {
	    FsysRetPtr *ep;
	    ep = vol->free + vol->free_ffree;
	    ep->start = ep->nblocks = 0; /* follow with a null if there's room */
	}
    }
    vrp->start = rp->start;
    vrp->nblocks = rp->nblocks;
    vol->total_free_clusters += rp->nblocks;
    vol->total_alloc_clusters -= rp->nblocks;
    return collapse_free(vol, vrp - vol->free);
#endif	
}

static int free_rps(FsysVolume *vol, FsysRamRP *rps) {
    FsysRamRP *cur, *nxt, *prev;
    int ii, jj, sts;

#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
# error You need to rewrite free_rps to get skip/repeat feature
#endif
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
	nxt = cur = rps + ii;
	while (nxt) {
	    FsysRetPtr *retp;
	    retp = nxt->rptrs;
	    for (jj=0; retp && jj < nxt->num_rptrs; ++jj, ++retp) {
		sts = back_to_free(vol, retp);
		if (sts) return sts;
	    }
	    nxt = nxt->next;
	}
	while (1) {
	    nxt = cur->next;
	    if (!nxt) break;
	    prev = cur;
	    while (nxt->next) {
		prev = nxt;
		nxt = nxt->next;
	    }
	    QIOfree(nxt);				/* done with this */
	    prev->next = 0;				/* break the link */
	}
	if (cur->rptrs && cur->mallocd) QIOfree(cur->rptrs);
	cur->rptrs = 0;
	cur->mallocd = 0;
	cur->num_rptrs = 0;
	cur->rptrs_size = 0;
    }
    return 0;
}

static int get_fh(FsysVolume *vol) {
    int fid, gen, ii;
    unsigned long *ulp;
    FsysRamFH *rfh;

    fid = 0;					/* assume failure */
    gen = 1;					/* assume generation of 1 */

    while (vol->unused_ffree) {			/* if something on the unused list */
	--vol->unused_ffree;			/* use it */
	fid = vol->unused[vol->unused_ffree];
	ulp = vol->index + fid*FSYS_MAX_ALTS;
	if (!(*ulp&0x80000000)) {		/* error, not really a free fid */
	    fid = 0;				/* keep looking */
	    continue;
	}
	gen = (*ulp & 0xff)+1;			/* get the new generation number */
	if (gen > 255) gen = 1;			/* wrap it back to 1 */
	break;
    }
    if (!fid) {					/* nothing on the unused list */
	FsysRamFH *nf;
	if (vol->files_ffree >= vol->files_elems) { /* grab a new one from the end of the allocated list */
	    int new_size;
	    new_size = vol->files_elems + 32;
	    nf = (FsysRamFH *)QMOUNT_REALLOC(vol, vol->files, new_size*sizeof(FsysRamFH));
	    if (!nf) {
		return -FSYS_CREATE_NOMEM;	/* ran out of memory */
	    }
	    vol->files = nf;
	    ulp = (U32*)QMOUNT_REALLOC(vol, vol->index, new_size*sizeof(U32)*FSYS_MAX_ALTS);
	    if (!ulp) {
		return -FSYS_CREATE_NOMEM;	/* ran out of room bumping index */
	    }
	    vol->index = ulp;
	    vol->files_elems = new_size;	/* ran out, so allocate some more */
	}
	fid = vol->files_ffree++;
	nf = vol->files + FSYS_INDEX_INDEX;	/* point to index file header */
	nf->size += sizeof(long)*FSYS_MAX_ALTS;	/* increment index file size */
	if (nf->size > nf->clusters*512) {	/* if file has grown out of its britches */
	    FsysRamRP *rp;
	    if (!nf->def_extend) nf->def_extend = FSYS_DEFAULT_DIR_EXTEND;
	    if (nf->def_extend*FSYS_MAX_ALTS >= vol->total_free_clusters) {
		return -FSYS_EXTEND_FULL;	/* no room to extend index file */
	    }
	    for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
		int ans;
		rp = nf->ramrp + ii;
		ans = extend_file(vol, nf->def_extend, rp, ii);
		if (ans) return -ans;		/* some kind of error */
	    }
	    nf->clusters += nf->def_extend;
	}
	add_to_dirty(vol, FSYS_INDEX_INDEX, 0); /* index file header needs updating */
    }
    ulp = vol->index + fid*FSYS_MAX_ALTS;	/* point to index */
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
	*ulp++ = 0x80000000 | gen;		/* make pointers to FH's invalid */
    }
    rfh = vol->files + fid;
    memset((char *)rfh, 0, sizeof(FsysRamFH));	/* zap the entire file header */
    rfh->generation = gen;			/* set the file's generation number */
    return fid;
}
    
static void fsys_mkheader(FsysHeader *hdr, int gen) {
    memset((char *)hdr, 0, sizeof(FsysHeader));
    hdr->id = FSYS_ID_HEADER;
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    hdr->ctime = hdr->mtime = (unsigned long)time(0);
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    hdr->atime = hdr->ctime;
    hdr->btime = 0;
#endif
    hdr->size = 0;			/* file size in bytes */
    hdr->type = FSYS_TYPE_FILE;	/* assume plain file */
    hdr->flags = 0;			/* not used, start with 0 */
    hdr->generation = gen;
}

static int zap_file( FsysLookUpFileT *luf ) {
    unsigned long *ndx;
    int fid, ii, sts;
    FsysRetPtr t;
    FsysVolume *vol;

    vol = luf->vol;
    remove_ent(luf->owner->directory, luf->fname); /* delete the name from the directory */
    add_to_dirty(vol, luf->owner - vol->files, 0); /* put owner directory on the dirty list */
    fid = luf->file - vol->files;
    ndx = vol->index + fid*FSYS_MAX_ALTS;
    t.start = *ndx;
    t.nblocks = 1;
    back_to_free(vol, &t);			/* free the FH */
    *ndx++ = luf->file->generation | 0x80000000;	/* remember the old generation number */
    for (ii=1; ii < FSYS_MAX_ALTS; ++ii) {
	t.start = *ndx;
	t.nblocks = 1;
	back_to_free(vol, &t);		/* free the FH */
	*ndx++ = luf->file->generation | 0x80000000;
    }
    sts = free_rps(vol, luf->file->ramrp);	/* free all the retrieval pointers */
    memset((char *)luf->file, 0, sizeof(FsysRamFH)); /* zap the entire file header */
    if (!sts) {
	sts = add_to_unused(vol, fid);	/* mark FH as unused */
	if (!sts) sts = upd_index_bits(vol, fid); /* need to update index file */
    }
    return sts ? sts : (FSYS_DELETE_SUCC|SEVERITY_INFO);
}

static void delete_q(QioIOQ *ioq) {
    FsysLookUpFileT luf;
    QioFile *file;

    luf.path = (char *)ioq->pparam0;
    luf.vol = (FsysVolume *)ioq->pparam1;
    luf.top = 0;
    ioq->iostatus = lookup_filename(&luf);
    if (ioq->iostatus == (FSYS_LOOKUP_SUCC|SEVERITY_INFO)) {
	if (luf.file->directory) {
	    ioq->iostatus = FSYS_DELETE_DIR;		/* for now, we cannot delete a directory */
	} else {
	    ioq->iostatus = zap_file(&luf);
	}
    }
    file = qio_fd2file(ioq->file);
    file->dvc = 0;
    file->private = 0;
    qio_freefile(file);
    ioq->file = -1;
    qio_freemutex(&luf.vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_delete( QioIOQ *ioq, const char *name ) {
    FsysVolume *vol;
    QioFile *file;

    if ( !name || *name == 0 ) return (ioq->iostatus = FSYS_DELETE_INVARG);
    file = qio_fd2file(ioq->file);		/* get pointer to file */
    vol = (FsysVolume *)file->dvc->private;	/* point to our mounted volume */
    if (!vol) return (ioq->iostatus = FSYS_DELETE_FNF);
    if (!vol->files) (ioq->iostatus = FSYS_DELETE_FNF);
    ioq->pparam0 = (void *)name;
    ioq->pparam1 = (void *)vol;
    ioq->iostatus = 0;
    return qio_getmutex(&vol->mutex, delete_q, ioq);
}

static void fcreate_q(QioIOQ *ioq) {
    FsysOpenFileT *t;
    QioFile *file;
    FsysVolume *vol;
    FsysOpenT *dtls;
    FsysRamFH *rfh;
    FsysLookUpFileT luf;
    FsysDirEnt *dp;
    const char *fname;
    int fid, sts, ii, new, sectors, delete_after=0;

    t = (FsysOpenFileT *)ioq->private;
    if (!t) {
	ioq->iostatus = FSYS_CREATE_FATALNOPRIV;
	qio_complete(ioq);		/* this is certain death since the mutex is being held */
	return;
    }
    ioq = t->ioq;
    vol = t->vol;
    dtls = t->details;
    file = qio_fd2file(ioq->file);	/* get pointer to file */
    vol = luf.vol = t->vol;
    luf.top = 0;			/* If we supported 'cwd', this is where it would go */
    luf.path = t->path;

    do {
	sts = lookup_filename(&luf);
	if (sts != (FSYS_LOOKUP_SUCC|SEVERITY_INFO) && sts != FSYS_LOOKUP_FNF) {
	    ioq->iostatus = sts;
	    break;
	}
	fname = strrchr(t->path, QIO_FNAME_SEPARATOR); /* isolate the name from the path */
	if (!fname) fname = t->path;		/* no path, use name as is */
	if ((rfh=luf.file)) {			/* there's an old file */
	    int gen;
	    if (rfh->directory) {		/* cannot create a new file on top of a directory */
		ioq->iostatus = FSYS_CREATE_NAMEINUSE;
		break;
	    }
	    if (QIO_ERR_CODE((ioq->iostatus=free_rps(vol, rfh->ramrp)))) { /* free all the retrieval pointers */
		break;
	    }
	    gen = rfh->generation;			/* save old FH generation number */
/*
 * NOTE: This is a memory leak that needs plugging. The individual RamRP ptrs _may_
 * have been malloc'd. If they were, they need to be free'd before being erased.
 */
	    memset((char *)rfh, 0, sizeof(FsysRamFH));	/* zap the entire file header */
	    luf.dir->generation = gen;		/* restore generation number in directory */
	    rfh->generation = gen;		/* restore generation number in FH */
	    fid = rfh - vol->files;
	    dp = luf.dir;			/* remember where the directory is */
	    new = 0;				/* signal not to update directory */
	} else {
	    U32 *ndx;
	    int bcnt;
	    char *fname;
	    FsysFindFreeT freet;
	    FsysRetPtr freep;
	    int oowner;				/* need to save this */

	    if (vol->total_free_clusters < FSYS_MAX_ALTS+1) { /* room for some additional FH's? */
		ioq->iostatus = FSYS_CREATE_NOFH;	/* nope, file system is full */
		break;
	    }	    
	    oowner = luf.owner - vol->files;
	    fid = get_fh(vol);				/* get the next available fileheader */
	    if (fid <= 0) {				/* no more file headers */
		ioq->iostatus = fid ? -fid : FSYS_CREATE_NOFH;
		break;
	    }
	    luf.owner = vol->files + oowner;
	    ndx = vol->index + fid*FSYS_MAX_ALTS;
	    rfh = vol->files + fid;
	    freet.skip = 0;
	    freet.exact = 0;
	    freet.nelts = vol->free_ffree;
	    freet.freelist = vol->free;
	    freet.reply = &freep;
	    freet.request = 1;
	    freet.hint = 0;
	    for (sts=bcnt=0; bcnt < FSYS_MAX_ALTS; ++bcnt) {
		freet.lo_limit = FSYS_HB_ALG(bcnt, vol->maxlba);
		if ( fsys_findfree( &freet ) ) {	/* get a free block */
		    ioq->iostatus = FSYS_CREATE_FULL;	/* no room for file headers */
		    break;
		}
		if (vol->free_start > freet.which_elem) vol->free_start = freet.which_elem;
		if (*ndx && !(*ndx&0x80000000)) {	/* just to make sure we didn't screw up */
		    ioq->iostatus = FSYS_CREATE_FATAL;	/* oops, must have double mapped a file header */
		    break;
		}
		*ndx++ = freep.start;			/* load index file with new LBA */
	    }	
	    if (QIO_ERR_CODE(ioq->iostatus)) break;
	    vol->total_free_clusters -= FSYS_MAX_ALTS;
	    vol->total_alloc_clusters += FSYS_MAX_ALTS;
	    dp = (FsysDirEnt *)QMOUNT_ALLOC(vol, sizeof(FsysDirEnt)+strlen(luf.fname)+1);
	    fname = (char *)(dp+1);
	    strcpy(fname, luf.fname);
	    dp->name = fname;
	    dp->generation = rfh->generation;
	    dp->fid = fid;
	    dp->next = 0;
	    upd_index_bits(vol, fid);
	    new = 1;
	}
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
# if HAVE_TIME
	rfh->ctime = dtls->ctime ? dtls->ctime : (U32)time(0);
# else
	rfh->ctime = dtls->ctime;
# endif
	rfh->mtime = dtls->mtime ? dtls->mtime : dtls->ctime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
	rfh->atime = dtls->atime;
	rfh->btime = dtls->btime;
#endif
	add_to_dirty(t->vol, fid, 0);			/* put FH on the dirty list */
	if (!rfh->def_extend) {
	    rfh->def_extend = dtls->mkdir ? FSYS_DEFAULT_DIR_EXTEND : FSYS_DEFAULT_EXTEND;
	}
	if (dtls->alloc) {
	    sectors = ((dtls->alloc+511)/512);
	} else {
	    sectors = rfh->def_extend;
	}
	luf.file = rfh;					/* in case of errors */
	if (vol->total_free_clusters < sectors*dtls->copies) { /* room for file? */
	    zap_file( &luf );				/* remove all traces of this file */
	    ioq->iostatus = FSYS_CREATE_FULL;		/* file system is full */
	    break;
	}	    
	rfh->clusters = sectors;
	for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {		/* add retrieval pointers to the file */
	    FsysRamRP *rp;
	    int where;

	    rp = rfh->ramrp + ii;

	    if (ii < dtls->copies) {
		where = dtls->placement + ii;
		if (where >= FSYS_MAX_ALTS) where = FSYS_MAX_ALTS-1;
		sts = extend_file(vol, rfh->clusters, rp, where);
		if (sts) {
		    zap_file( &luf );			/* remove all traces of this file */
		    ioq->iostatus = sts;
		    break;
		}
	    }
	}
	if (QIO_ERR_CODE(ioq->iostatus)) break;
	if (new) {
	    insert_ent(luf.owner->directory, dp);	/* insert new file into parent directory */
	    add_to_dirty(t->vol, luf.owner - t->vol->files, 0); /* put owner directory on the dirty list too */
	}
	rfh->valid = RAMFH_VALID;		/* mark entry as valid and useable */
	ioq->iostatus = FSYS_CREATE_SUCC|SEVERITY_INFO; /* assume success */
	dtls->fid = ((rfh->generation<<24) | (rfh - vol->files));
	file->private = (void *)dtls->fid;
	dtls->parent = ((luf.owner->generation<<24) | (luf.owner - vol->files));
	while (dtls->mkdir) {			/* are we to make it into a directory? */
	    FsysDirEnt *inddir;			/* pointer to list of directory ents */
	    char *strings;			/* pointer to place to put strings */

	    delete_after = 1;
	    rfh->directory = (FsysDirEnt **)QMOUNT_ALLOC(vol, FSYS_DIR_HASH_SIZE*sizeof(FsysDirEnt*) +
    					3+2+	/* + room for strings (. and ..) */
					2*sizeof(FsysDirEnt)); /* + room for directory items */
	    if (!rfh->directory) {
		ioq->iostatus = FSYS_CREATE_NOMEM;
		break;
	    }

	    inddir = (FsysDirEnt *)(rfh->directory + FSYS_DIR_HASH_SIZE);
	    strings = (char *)(inddir + 2);
	    inddir->name = strings;
	    *strings++ = '.';
	    *strings++ = '.';
	    *strings++ = 0;
	    inddir->fid = luf.owner - vol->files;
	    inddir->generation = luf.owner->generation;
	    insert_ent(rfh->directory, inddir);
	    ++inddir;
	    inddir->name = strings;
	    *strings++ = '.';
	    *strings = 0;
	    inddir->fid = rfh - vol->files;
	    inddir->generation = rfh->generation;
	    insert_ent(rfh->directory, inddir);
	    break;
	}
	break;
    } while (0);
    file->pos = 0;			/* !FIXME!!! start at beginning of file */
    qio_freemutex(&vol->mutex, ioq);	/* done with volume mutex */
    if (delete_after || QIO_ERR_CODE(ioq->iostatus)) {	/* if there were open errors */
	qio_freefile(file);		/* done with the file descriptor */
	ioq->file = -1;			/* file didn't open */
	dtls->fid = -1;
	dtls->parent = -1;
    }
    QIOfree(ioq->private);		/* done with this memory */
    ioq->private = 0;			/* burn our bridges */
    qio_complete(ioq);			/* call user's completion routine if any */
    return;
}
#endif				/* !FSYS_READ_ONLY */

static void fopen_q(QioIOQ *ioq) {
    FsysOpenFileT *t;
    QioFile *file;
    FsysVolume *vol;
    FsysOpenT *dtls;
    FsysRamFH *rfh;
    FsysLookUpFileT luf;
    int fid, gen;

    t = (FsysOpenFileT *)ioq->private;

#if !FSYS_READ_ONLY
    if (t->mode&_FCREAT) {
	fcreate_q(ioq);			/* open for writing */
	return;
    }
#endif

    vol = t->vol;
    dtls = t->details;
    file = qio_fd2file(ioq->file);		/* get pointer to file */
    if (!file) {
	ioq->iostatus = FSYS_OPEN_NOTOPEN;
    } else do {
	if ((fid=dtls->fid)) {			/* if to open by FID */
	    gen = (fid >> 24)&0xFF;
	    fid &= 0x00FFFFFF;
	    if (fid >= vol->files_ffree) {
		ioq->iostatus = FSYS_OPEN_NOFID; /* fid out of range */
		break;
	    }
	    rfh = vol->files + fid;
	    if (rfh->generation != gen) {
		ioq->iostatus = FSYS_OPEN_NOGEN; /* generation numbers don't match */
		break;
	    }
	    dtls->parent = 0;			/* don't know a parent if open by FID */
	} else {				/* else we're to open by name */
	    FsysRamFH *own;
	    luf.vol = vol;
	    luf.top = 0;			/* start looking at root directory */
	    luf.path = t->path;			/* point to adjusted name */
	    ioq->iostatus = lookup_filename(&luf);
	    if (QIO_ERR_CODE(ioq->iostatus)) {	/* if didn't open */
		break;				/* just die */
	    }
	    rfh = luf.file;
	    own = luf.owner;
	    dtls->fid = (rfh->generation<<24) | (rfh - vol->files); /* pass back FID of opened file */
	    dtls->parent = (own->generation<<24) | (own - vol->files); /* pass back FID of parent */
	}		
	dtls->alloc = rfh->clusters * 512;	/* number of bytes allocated to file */
	dtls->eof = rfh->size;			/* size of file in bytes */
	if (!(file->mode&_FAPPEND)) { 
	    file->pos = 0;			/* assume start at position 0 */
	    if ((file->mode&(_FWRITE|_FREAD)) == _FWRITE) { /* if open only for write ... */
		rfh->size = 0;			/* ... move the file's eof mark too */
	    }
	} else {
	    file->pos = dtls->eof;		/* unless supposed to append */
	}
	file->size = rfh->size;			/* note the size of the file */
	dtls->placement = 0;			/* cannot determine placement */
	for (gen=0; gen < FSYS_MAX_ALTS; ++gen) {
	    if (!rfh->ramrp[gen].rptrs) break;
	}
	if ((file->mode&O_OPNCPY)) {
	    file->mode &= ~FSYS_OPNCPY_M;	/* zap all the bits */
	    if (dtls->copies+1 > gen) {
		ioq->iostatus = FSYS_LOOKUP_FNF; /* cannot open that version */
		break;
	    }
	    file->mode |= (dtls->copies+1) << FSYS_OPNCPY_V;
	}
	dtls->copies = gen;			/* give 'em copies */
	dtls->mkdir = rfh->directory ? 1 : 0;	/* tell 'em if file is directory */
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
	dtls->ctime = rfh->ctime;
	dtls->mtime = rfh->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
	dtls->atime = rfh->atime;
	dtls->btime = rfh->btime;
#endif
	file->private = (void*)dtls->fid;	/* remember FID of open file for ourself */
	ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
	break;
    } while (0);
    if (QIO_ERR_CODE(ioq->iostatus)) {	/* if there were open errors */
	qio_freefile(file);		/* done with the file descriptor */
	ioq->file = -1;			/* file didn't open */
	dtls->fid = -1;
	dtls->parent = -1;
    }
    ioq->private = 0;			/* burn our bridges */
    qio_freemutex(&vol->mutex, ioq);	/* done with volume mutex */
    qio_complete(ioq);			/* call user's completion routine if any */
    QIOfree(t);				/* done with this memory */
    return;
}

/*********************************************************************
 * fsys_open - open a file for input or output. Expected to be called
 * from qio_open.
 *
 * At entry:
 *	ioq - pointer to QioIOQ struct.
 *	name - pointer to null terminated string with path/filename.
 *
 * NOTE: If O_QIOSPC is set in 'mode', then ioq->spc points to a
 * FsysOpenT struct with additional details about the open:
 *	path - pointer to null terminated string of whole name including
 *		volume name.
 *	fid - if .ne. and mode is O_RDONLY, will open the file by FID
 *		ignoring the 'name' parameter. Regardless of mode bits, open
 *		will set this field to the FID of the opened file at completion
 *		(or -1 if open failed).
 *	parent - Regardless of mode bits, open will set this field to the FID
 *		of the parent directory (or -1 if open failed).
 *	alloc - if mode set to O_WRONLY, specifies the amount of disk to
 *		pre-allocate to the file. Regardless of mode bits, open will
 *		set this field to the amount of space allocated to the file.
 *	eof - if mode set to O_WRONLY, specifies where the EOF marker is
 *		to be set. Regardless of mode bits, open will set this field
 *		to the position of the EOF marker at completion.
 *	placement - number from 0 to FSYS_MAX_ALTS-1 indicating the preferred
 *		placement of the file. Only relevant during file creation.
 *	copies - if mode contained O_WRONLY, specifies the number of copies
 *		of the file that are to be maintained (value must be from 1
 *		to FSYS_MAX_ALTS). Regardless of mode bits, open will set this
 *		field to the copies of files that are present in the file system
 *		at completion.
 *	mkdir - if mode contains O_CREAT and this field is not 0, the created
 *		file will made into a directory.

 * At exit:
 *	returns one of FSYS_CREATE_xxx signalling success or failure
 *	*file gets pointer to newly created FsysRamFH struct.
 */

static int fsys_open( QioIOQ *ioq, const char *name ) {
    FsysVolume *vol;
    FsysOpenFileT *ours=0;
    FsysOpenT *his;
    QioFile *file;
    int sts = 0;

    if (!ioq) return FSYS_OPEN_INVARG;
    do {
	file = qio_fd2file(ioq->file);		/* get pointer to file */
	his = (FsysOpenT *)file->private;
	vol = (FsysVolume *)file->dvc->private;	/* point to our mounted volume */
	if (!vol) {
	    ioq->iostatus = FSYS_OPEN_NOTMNT;
	    break;
	}
	if (!vol->files) {
	    ioq->iostatus = FSYS_OPEN_NOTMNT;
	    break;
	}
	if ( (!his || !his->fid) && (!name || *name == 0) ) {
	    ioq->iostatus = FSYS_OPEN_INVARG;
	    break;
	}
	if ((file->mode&_FCREAT)) {
#if FSYS_READ_ONLY
	    ioq->iostatus = FSYS_CREATE_NOSUPP;
	    break;
#else
	    if (his && ((his->copies < 1 || his->copies > FSYS_MAX_ALTS) ||
	                (his->placement < 0 || his->placement >= FSYS_MAX_ALTS)) ) {
		ioq->iostatus = FSYS_OPEN_INVARG;
		break;
	    }
#endif
	}
	ours = QIOcalloc(sizeof(FsysOpenFileT)+sizeof(FsysOpenT),1);
	if (!ours) {
	    ioq->iostatus = FSYS_OPEN_NOMEM;
	    break;
	}
	if (!his) {
	    his = (FsysOpenT *)(ours+1);
	    his->spc.path = name;
	    his->spc.mode = file->mode;
	    his->copies = 1;
	}
	ours->details = his;
	ours->vol = vol;
	ours->ioq = ioq;
	ours->path = name;
	ours->mode = file->mode;
	ioq->private = (void *)ours;
	sts = qio_getmutex(&vol->mutex, fopen_q, ioq);
	if (sts) {
	    ioq->iostatus = sts;
	    break;
	}
	return 0;
    } while (0);
    if (ours) QIOfree(ours);
    qio_freefile(file);
    ioq->file = -1;
    ioq->private = 0;
    return ioq->iostatus;
}

#if !FSYS_READ_ONLY
/*********************************************************************
 * fsys_mkdir - create a directory. This function creates a directory on the
 * specified volume.
 *
 * At entry:
 *	ioq - pointer to QioIOQ struct.
 *	name - pointer to null terminated string with path/filename
 *	mode - mode to apply to directory (not used)
 *
 * At exit:
 *	returns one of FSYS_CREATE_xxx signalling success or failure.
 */

static int fsys_mkdir( QioIOQ *ioq, const char *name, int mode ) {
    FsysVolume *vol;
    FsysOpenFileT *ours=0;
    FsysOpenT *his;
    QioFile *file;
    int sts = 0;

    if (!ioq) return FSYS_OPEN_INVARG;
    do {
	file = qio_fd2file(ioq->file);		/* get pointer to file */
	vol = (FsysVolume *)file->dvc->private;	/* point to our mounted volume */
	if (!vol) {
	    ioq->iostatus = FSYS_OPEN_NOTMNT;
	    break;
	}
	ours = QIOcalloc(sizeof(FsysOpenFileT)+sizeof(FsysOpenT),1);
	if (!ours) {
	    ioq->iostatus = FSYS_OPEN_NOMEM;
	    break;
	}
	his = (FsysOpenT *)(ours+1);
	his->spc.path = name;
	his->spc.mode = O_CREAT;
	his->copies = FSYS_MAX_ALTS;	/* there are always max copies of directories */
	his->mkdir = 1;
	ours->details = his;
	ours->vol = vol;
	ours->ioq = ioq;
	ours->path = name;
	ours->mode = _FCREAT;
	ioq->private = (void *)ours;
	sts = qio_getmutex(&vol->mutex, fopen_q, ioq);
	if (sts) {
	    ioq->iostatus = sts;
	    break;
	}
	return 0;
    } while (0);
    if (ours) QIOfree(ours);
    qio_freefile(file);
    ioq->file = -1;
    return ioq->iostatus;
}
#endif

static int mk_ramdir(FsysVolume *vol) {
    unsigned char *s, *lim, gen;
    unsigned long fid;
    int qty, len, totstr;
    FsysDirEnt *inddir, **dir;		/* pointer to list of directory ents */
    unsigned char *strings;		/* pointer to place to put strings */
    FsysRamFH *rfh;

    rfh = vol->files + vol->files_indx;
    dir = rfh->directory;		/* pointer to our hash table */
    if (!dir) {
	vol->status = FSYS_MOUNT_FATAL;
	return 1;
    }
    s = (unsigned char *)vol->buff;
#if 1
    lim = s + vol->rw_amt;
    qty = totstr = 0;			/* start with nothing */    		
    while (s < lim) {			/* walk the list to get lengths and counts */
	fid = (s[2]<<16) | (s[1]<<8) | *s;
	if (!fid) break;		/* fid of 0 is end of list */
	s += 4;				/* skip fid and generation number */
	len = *s++;
	if (!len) len = 256;
	totstr += len;
	++qty;
	s += len;
    }
    inddir = (FsysDirEnt *)QMOUNT_ALLOC(vol, totstr+	/* room for strings */
    			  	       qty*sizeof(FsysDirEnt)); /* room for directory items */
    if (!inddir) {
	vol->status = FSYS_MOUNT_NOMEM;
	return 1;
    }
    strings = (unsigned char *)(inddir + qty);

    s = (unsigned char *)vol->buff;
    while (s < lim) {
	fid = (s[2]<<16) | (s[1]<<8) | *s;
	if (!fid) break;		/* fid of 0 is end of list */
	s += 3;
	gen = *s++;
	len = *s++;
	if (!len) len = 256;
	strcpy((char *)strings, (char *)s);
	inddir->name = (char *)strings;
	inddir->fid = 0;		/* assume invalid entry */
	if (fid < vol->files_ffree) {	/* fid is in bounds */
	    rfh = vol->files + fid;	/* point to file header */
	    inddir->fid = fid;
	    inddir->generation = gen;
	}
	insert_ent(dir, inddir);	/* put filename into directory */
	s += len;
	strings += len;
	++inddir;
    }
#else
    lim = s + vol->rw_amt;
    while (s < lim) {			/* walk the list to get lengths and counts */
	totstr = 0;				/* start with nothing */    		
	fid = (s[2]<<16) | (s[1]<<8) | *s;
	if (!fid) break;		/* fid of 0 is end of list */
	s += 3;
	gen = *s++;
	len = *s++;
	if (!len) len = 256;
	inddir = (FsysDirEnt *)QMOUNT_ALLOC(vol, len+	/* room for string */
					   sizeof(FsysDirEnt)); /* room for directory item */
	if (!inddir) {
	    vol->status = FSYS_MOUNT_NOMEM;
	    return 1;
	}
	strings = (unsigned char *)(inddir + 1);
	strcpy((char *)strings, (char *)s);
	inddir->name = (char *)strings;
	inddir->fid = 0;		/* assume invalid entry */
	if (fid < vol->files_ffree) {	/* fid is in bounds */
	    inddir->fid = fid;
	    inddir->generation = gen;
	}
	insert_ent(dir, inddir);	/* put filename into directory */
	s += len;
    }
#endif
    return 0;
}

static int mk_ramfh(FsysVolume *vol, FsysRamFH *rfh) {
    FsysHeader *hdr;
    FsysRamRP *rrp;
    FsysRetPtr *dst, *src;
    int ii, jj, kk, amt, nblk=0;

    hdr = (FsysHeader *)vol->buff;
    memset((char *)rfh, 0, sizeof(FsysRamFH));
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_EXTENSION_HEADER)
    if (hdr->extension) {
	vol->status = FSYS_MOUNT_NOSUPP;	/* extension headers not supported */
	return 1;
    }
#endif
    rfh->clusters = hdr->clusters;
    vol->total_alloc_clusters += FSYS_MAX_ALTS;
    rfh->size = hdr->size;
    rfh->generation = hdr->generation;		/* keep a copy of generation number */
    if (hdr->type == FSYS_TYPE_DIR) {		/* if file is a directory */
	rfh->directory = (FsysDirEnt **)QMOUNT_ALLOC(vol, FSYS_DIR_HASH_SIZE*sizeof(FsysDirEnt*));
	if (!rfh->directory) {
	    vol->status = FSYS_MOUNT_NOMEM;
	    return 1;
	}
	rfh->def_extend = FSYS_DEFAULT_DIR_EXTEND;
    } else {
	rfh->def_extend = FSYS_DEFAULT_EXTEND;
    }
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    rfh->mtime = hdr->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_PERMS)
    rfh->perms = hdr->perms;
    rfh->owner = hdr->owner;
#endif
    rrp = rfh->ramrp;
    for (amt=kk=jj=0; jj < FSYS_MAX_ALTS; ++jj, ++rrp) {
	for (ii=0; ii < FSYS_MAX_FHPTRS; ++ii) {
	    if (!hdr->pointers[jj][ii].start) break;
	}
	if (ii) ++kk;
	if (ii > amt) amt = ii;
    }
    if (kk) {
#if 0 && !FSYS_READ_ONLY && !FSYS_TIGHT_MEM
	amt += 8;
	if (amt > FSYS_MAX_FHPTRS) amt = FSYS_MAX_FHPTRS;
#endif
	jj = kk*amt;
	rrp = rfh->ramrp;
#if FSYS_TIGHT_MEM
	if (vol->rp_pool_size >= jj) {
	    dst = vol->rp_pool;
	    vol->rp_pool += jj;
	    vol->rp_pool_size -= jj;
	} else
#endif
	    dst = (FsysRetPtr *)QMOUNT_ALLOC(vol, jj*sizeof(FsysRetPtr));
	if (!dst) {		/* ooops, ran out of memory */
	    vol->status = FSYS_MOUNT_NOMEM ;
	    return 1;
	}
	for (jj=0; jj < kk; ++jj, ++rrp, dst += amt) {
	    FsysRetPtr *lcl_dst;
	    if (hdr->pointers[jj][0].start == 0) break; /* nothing left to do */
	    rrp->rptrs = dst;
#if !FSYS_READ_ONLY
	    rrp->rptrs_size = amt;
#endif
	    src = hdr->pointers[jj];
	    lcl_dst = dst;
	    for (ii=0; ii < amt; ++ii, ++lcl_dst, ++src) {
		if (!src->start) break;
		lcl_dst->start = src->start;
		lcl_dst->nblocks = src->nblocks;
		nblk += src->nblocks;
	    }
	    rrp->num_rptrs = ii;
	}
    }
    vol->total_alloc_clusters += nblk;
    rfh->valid = RAMFH_VALID;
    return 0;
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
enum syn_state {
    SYNC_BEGIN,
    SYNC_WALK_DIRTY,
    SYNC_READFH_COMPLETE,
    SYNC_WRITE_FH,
# if !FSYS_READ_ONLY
    SYNC_UPD_INDEX,
    SYNC_INDEX_WRCOMPL,
    SYNC_UPD_FREE,
    SYNC_FREE_WRCOMPL,
    SYNC_UPD_DIRECTORY,
    SYNC_WRITE_DIRECTORY,
# endif
    SYNC_READ_FIRSTFH,
    SYNC_DONE
};
#endif

#if !FSYS_READ_ONLY || FSYS_UPD_FH
static int get_sync_buffers(FsysVolume *vol, FsysSyncT *syn) {
    int old_size;
    unsigned char *old_ptr, *old_head;
    old_size = syn->buffer_size;
    old_ptr = (U8*)syn->output;
    old_head = (U8*)syn->buffers;
    syn->buffer_size = (syn->buffer_size + FSYS_SYNC_BUFFSIZE + 511) & -512;
    SYNSQK((OUTWHERE "%6d: get_sync_buffers: Increasing buffer size from %d to %d\n",
		eer_rtc, old_size, syn->buffer_size));
    syn->buffers = (unsigned long *)QMOUNT_ALLOC(vol, syn->buffer_size+QIO_CACHE_LINE_SIZE);
    if (!syn->buffers) {
	SYNSQK((OUTWHERE "        Ran out of memory\n"));
	return (syn->status = FSYS_SYNC_NOMEM);
    }
    syn->output = (unsigned long *)QIO_ALIGN(syn->buffers, QIO_CACHE_LINE_SIZE);
    memcpy(syn->output, old_ptr, old_size);
    QMOUNT_FREE(vol, old_head);
    return 0;
}

static void fsys_sync_q(QioIOQ *ioq) {
    FsysVolume *vol;
    FsysSyncT *syn;
    int sts;
    FsysRamFH *rfh=0;
    FsysQio *qio;
    int fid, ii;
    unsigned long *ndx;

    syn = (FsysSyncT *)ioq;
    qio = (FsysQio *)ioq;
    vol = syn->vol;
    syn->status = 0;			/* assume success */

    while (1) {
	switch (syn->state) {
	    default:
		SYNSQK((OUTWHERE "%6d: fsys_sync_q: Illegal state of %d\n", syn->state));
		syn->status = FSYS_SYNC_FATAL;	/* something terrible happened */
		break;
	    case SYNC_BEGIN: {
		if (!vol->dirty_ffree) {
		    break;			/* nothing in the dirty list, we're done */
		}
		if (!syn->output) {		/* need to get a buffer */
		    SYNSQK(( OUTWHERE "%6d: sync: Begin, getting buffers.\n", eer_rtc));
		    if (get_sync_buffers(vol, syn)) break;
		    qio->compl = fsys_sync_q;	/* completion routine is us */
		    ioq->file = vol->iofd;
		    syn->ramrp.rptrs = &syn->rptr;
# if !FSYS_READ_ONLY
		    syn->ramrp.rptrs_size = 1;
		    syn->ramrp.next = 0;
# endif
		    syn->ramrp.num_rptrs = 1;
		    syn->rptr.start = 0;	/* start at block 0 on device */
		    syn->rptr.nblocks = vol->maxlba; /* the whole disk is one file */
		}
		syn->substate = 0;
		syn->state = SYNC_WALK_DIRTY;	/* walk the dirty list */
		syn->alts = 0;			/* start with the first copy */
		SYNSQK(( OUTWHERE "%6d: sync: Begin alt 0.\n", eer_rtc));
		continue;
	    }

	    case SYNC_WALK_DIRTY: {
		if (syn->substate >= vol->dirty_ffree) {
# if !FSYS_READ_ONLY
		    if (!syn->alts) {			/* check this on the first pass */
			rfh = vol->files + FSYS_INDEX_FREE;
			collapse_free(vol, INT_MAX);
			ii = (vol->free_ffree * sizeof(FsysRetPtr) + 511) & -512;
			if (ii > rfh->size) {
			    rfh->size = ii;			/* record high water mark */
			    add_to_dirty(vol, FSYS_INDEX_FREE, 1); /* add to end of dirty list */
			    continue;			/* re-iterate */
			}
		    }
# endif
		    SYNSQK(( OUTWHERE "%6d: sync: dirty list %d processing complete. substate=%d\n",
		    	eer_rtc, syn->alts, syn->substate));

		    if (++syn->alts < FSYS_MAX_ALTS) {
			syn->substate = 0;		/* restart the whole procedure again */
			continue;
		    }

# if !FSYS_READ_ONLY
		    syn->state = SYNC_UPD_INDEX; /* start updating index file */
		    syn->alts = 0;
# else
		    syn->state = SYNC_DONE;
# endif
		    syn->substate = 0;
		    continue;
		}
		fid = vol->dirty[syn->substate]; /* get fid of file to process next */
		rfh = vol->files + fid;		/* point to FH */
		SYNSQK(( OUTWHERE "%6d: sync: processing dirty fid: %08lX.%d. valid=%d\n",
		    eer_rtc, fid, syn->alts,
    		   (fid >= FSYS_INDEX_ROOT) && (fid < vol->files_ffree) ? rfh->valid : 0));
		if ((fid > FSYS_INDEX_ROOT && !rfh->valid) || fid >= vol->files_ffree) { /* it isn't valid anymore */
		    SYNSQK(( OUTWHERE "%6d: sync: fid %08lX invalid. ffree=%d, valid=%d\n",
		    	eer_rtc, fid, vol->files_ffree, fid <= FSYS_INDEX_ROOT ? rfh->valid : 0));
# if !FSYS_READ_ONLY
		    ndx = vol->index + fid*FSYS_MAX_ALTS;
		    if (!syn->alts && fid < vol->files_ffree && !(*ndx&0x80000000)) { /* check that the index file is ok with an invalid RFH */
			int jj;
			for (jj=0; jj < FSYS_MAX_ALTS; ++jj) ndx[jj] = 0x80000001;
			upd_index_bits(vol, fid); /* signal we have to write the index file */
		    }
# endif
		    ++syn->substate;	/* next file */
		    continue;
		}
# if !FSYS_READ_ONLY
		if (rfh->directory) {
		    syn->state = SYNC_UPD_DIRECTORY;
		} else {
		    syn->state = SYNC_READ_FIRSTFH;
		}
# else
		syn->state = SYNC_READ_FIRSTFH;
# endif
		continue;
	    }				/* -- case WALK_DIRTY */

# if !FSYS_READ_ONLY
	    case SYNC_UPD_DIRECTORY: {
		char *dst, *lim;
		FsysDirEnt **hash, *dir;
		int sects;

		dst = (char *)syn->output;
		lim = dst + syn->buffer_size;
		hash = rfh->directory;		/* point to hash table */
		SYNSQK(( OUTWHERE "%6d: Writing directory\n", eer_rtc));
		for (ii=0; ii < FSYS_DIR_HASH_SIZE; ++ii, ++hash) {
		    dir = *hash;		/* point to directory entry */
		    while (dir) {
			int len;
			if (lim-dst <= (len=strlen(dir->name)+1)+1+4+4) {
			    int olen;
			    olen = dst - (char *)syn->output; /* remember how much we have used */
			    if (get_sync_buffers(vol, syn)) break;
			    dst = (char *)syn->output + olen;
			    lim = (char *)syn->output + syn->buffer_size;
			    continue;
			}
			*dst++ = dir->fid;
			*dst++ = dir->fid>>8;
			*dst++ = dir->fid>>16;
			*dst++ = dir->generation;
			*dst++ = len;
			strcpy(dst, dir->name);
			dst += len;
			dir = dir->next;
		    }
		    if (syn->status) break;
		}
		if (lim-dst < 4) {
		    int olen;
		    olen = dst - (char *)syn->output; /* remember how much we have used */
		    if (get_sync_buffers(vol, syn)) break;
		    dst = (char *)syn->output + olen;
		}
		*dst++ = 0;			/* need a null fid to end the list */
		*dst++ = 0;
		*dst++ = 0;
		*dst++ = 0;
#  if FSYS_SQUAWKING
		if (!syn->alts && (rfh - vol->files) == FSYS_INDEX_ROOT) {
		    extern void fsys_dump_bytes(IcelessIO *iop, U8*s, int siz);
		    SYNSQK(( OUTWHERE "       Dump of root directory (%d bytes)\n", rfh->size));
		    fsys_dump_bytes(fsys_iop, (U8*)syn->output, dst - (char *)syn->output);
		}
#  endif
		sects = dst - (char *)syn->output; /* compute size of data */
		if (sects&511) {
		    memset(dst, 0, 512 - (sects&511)); /* clear the rest of the buffer to sector boundary */
		}
		if (syn->status) break;
		rfh->size = dst - (char *)syn->output;
		sects = (rfh->size+511)/512;
		if (!syn->alts && sects > rfh->clusters) {
		    int def;
		    def = sects - rfh->clusters;
		    def += rfh->def_extend ? rfh->def_extend : FSYS_DEFAULT_EXTEND;
		    sts = 0;
		    if (def*FSYS_MAX_ALTS < vol->total_free_clusters) {
			for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
			    sts = extend_file(vol, def, rfh->ramrp+ii, ii);
			    if (sts) break;
			}
			if (!sts) rfh->clusters += def;
		    }
		    if (sts) {
			syn->state = SYNC_READ_FIRSTFH;
			continue;
		    }
		}
		syn->state = SYNC_WRITE_DIRECTORY;
		qio->ramrp = rfh->ramrp+syn->alts; /* point to retrieval pointer set */
		qio->u_len = rfh->size;
		qio->buff = (U8*)syn->output;	/* point to output buffer */
		qio->state = 0;
		qio->sector = 0;
		SYNSQK(( OUTWHERE "%6d: sync, directory upd: Queuing write %d. u_len=%d, sector=0\n",
	    		eer_rtc, syn->alts, qio->u_len));
		fsys_qwrite(ioq);
		return;
	    }			/* -- case SYNC_UPD_DIRECTORY */
	    case SYNC_WRITE_DIRECTORY: {
		if (QIO_ERR_CODE(ioq->iostatus)) { /* All we can do is log write errors */
		    syn->errlog[syn->err_in++] = ioq->iostatus;
		    if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
		    ++syn->errcnt;
		}
		syn->state = SYNC_READ_FIRSTFH; /* go update the directory's file header */
		continue;
	    }		/* -- case SYNC_WRITE_DIRECTORY */
# endif
	    case SYNC_READ_FIRSTFH: {
# if 0	/* someday we might have to pre-read the old file header before updating */
		if ((rfh->valid&RAMFH_NEW)) {	/* if it is a brand new fileheader */
		    syn->state = SYNC_READFH_COMPLETE;
		    ioq->iostatus = 0;		/* pretend we got no errors */
		    continue;			/* and goto next state */
		}
		qio->rptrs = &syn->ramrp;	/* point to fake retrieval pointer */
		qio->u_len = 512;
		qio->buff = syn->output;		/* point to input buffer */
		qio->state = 0;
		qio->sector = *ndx;		/* fh's lba */
		qio->ioq->file = vol->iofd;
		ioq->private = (void *)qio;	/* cross connect these structs */
		syn->state = SYNC_READFH_COMPLETE;
		syn->alts = 0;
		fsys_qread(ioq);
		return;
# else
		syn->state = SYNC_READFH_COMPLETE;
		ioq->iostatus = 0;		/* no errors */
		continue;			/* goto next state */
# endif
	    }			/* -- case SYNC_READ_FIRSTFH */
	    case SYNC_READFH_COMPLETE: {
		FsysHeader *fh;
		FsysRetPtr *dst;
		FsysRamRP *src;
		int alts;

		fid = vol->dirty[syn->substate]; /* get fid of file to process */
# if 0	/* someday we might have to pre-read the old file header before updating */
		if (QIO_ERR_CODE(qio->iostatus)) { /* no read errors on file headers allowed */
		    ++syn->alts;		/* advance to alternate fileheader pointer */
		    if (syn->alts >= FSYS_MAX_ALTS) {	/* tried all of them and none worked */
			syn->errlog[syn->err_in++] = ioq->iostatus;
			if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
			++syn->errcnt;
			syn->state = SYNC_WALK_DIRTY;
			++syn->substate;	/* Oh well, just skip this file then */
			continue;
		    }
		    ndx = vol->index + fid*FSYS_MAX_ALTS + syn->alts;
		    qio->u_len = 512;
		    qio->sector = *ndx;		/* select new relative sector */
		    qio->buff = syn->input;	/* point to input buffer */
		    qio->state = 0;
		    ioq->private = (void *)qio;	/* cross connect these structs */
		    fsys_qread(ioq);		/* read the next alternate */
		    return;		    
		}
# endif
		fh = (FsysHeader *)syn->output;
		rfh = vol->files + fid;
		fh->id = fid == FSYS_INDEX_INDEX ? FSYS_ID_INDEX : FSYS_ID_HEADER;
		fh->type = rfh->directory ? FSYS_TYPE_DIR : FSYS_TYPE_FILE;
		fh->flags = 0;			/* not used at this time, but 0 it anyway */
# if (FSYS_FEATURES&FSYS_FEATURES_CMTIME)
#  if (FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
		fh->ctime = rfh->ctime;
		fh->mtime = rfh->mtime;
#  else
		fh->ctime = fh->mtime = 0;	/* times not supported in this version */
#  endif
# endif
# if (FSYS_FEATURES&FSYS_FEATURES_ABTIME)
#  if (FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
		fh->atime = rfh->atime;
		fh->btime = rfh->btime;
#  else
		fh->atime = fh->btime = 0;
#  endif
# endif
		fh->generation = rfh->generation;
		fh->size = rfh->size;
		fh->clusters = rfh->clusters;
# if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_PERMS)
		fh->perms = rfh->perms;
		fh->owner = rfh->owner;
# endif
		for (alts=0; alts < FSYS_MAX_ALTS; ++alts) {
		    src = rfh->ramrp + alts;
		    dst = fh->pointers[alts];
		    for (ii=0; ii < FSYS_MAX_FHPTRS;) {
			int lim;
			lim = FSYS_MAX_FHPTRS - ii;
			if (lim > src->num_rptrs) lim = src->num_rptrs;
			memcpy((char *)dst, (char *)src->rptrs, lim*sizeof(FsysRetPtr));
# if !FSYS_READ_ONLY
			ii += lim;
			dst += lim;			
			if (ii < FSYS_MAX_FHPTRS && !src->next) {
			    lim = FSYS_MAX_FHPTRS - ii;
			    memset((char *)dst, 0, lim*sizeof(FsysRetPtr));
			    break;
			}
			src = src->next;
# else
			break;
# endif
		    }				/* -- for FSYS_MAX_FHPTRS */
		}				/* -- for FSYS_MAX_ALTS */
		ndx = vol->index + fid*FSYS_MAX_ALTS + syn->alts;
		syn->state = SYNC_WRITE_FH;
		qio->ramrp = &syn->ramrp;	/* point to fake retrieval pointer */
		qio->u_len = 512;
		qio->buff = (U8*)syn->output;	/* point to output buffer */
		qio->state = 0;
		qio->sector = *ndx;
		SYNSQK(( OUTWHERE "%6d: sync, FH upd: Queuing write %d. u_len=%d, sector=%d\n",
	    		eer_rtc, syn->alts, qio->u_len, qio->sector));
		fsys_qwrite(ioq);
		return;
	    }

	    case SYNC_WRITE_FH: {
		if (QIO_ERR_CODE(ioq->iostatus)) { /* All we can do is log write errors */
		    syn->errlog[syn->err_in++] = ioq->iostatus;
		    if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
		    ++syn->errcnt;
		}
		++syn->substate;		/* next fid */
		syn->state = SYNC_WALK_DIRTY;	/* back to top of loop */
		continue;
	    }		/* -- case SYNC_WRITE_FH */

# if !FSYS_READ_ONLY
	    case SYNC_UPD_INDEX: {
		unsigned long bit, *bp;
		int elem, sects, sects_bytes, start, start_bytes;
		int nelems, len;
		unsigned long *bits;

		nelems = vol->index_bits_elems;
		bits   = vol->index_bits;
#if FSYS_SYNC_SQUAWKING
		if (!syn->substate && !syn->alts) {
		    int ii;
		    SYNSQK(( OUTWHERE "%6d: sync: UPD_INDEX: nelms=%d, bits = %08lX\n",
			eer_rtc, nelems, bits));
		    for (ii=0; ii < nelems; ++ii) {
			SYNSQK(( OUTWHERE "             Bits[%2d]: %08lX\n", ii, bits[ii]));
		    }
		}
#endif
		if (!nelems || !bits) {
		    syn->state = SYNC_UPD_FREE;
		    continue;
		}
		elem = syn->substate/32;
		bit = syn->substate&31;
		bp = bits + elem;
		while (elem < nelems && !*bp) {
		    ++bp;
		    ++elem;
		    bit = 0;
		    syn->substate = (syn->substate & ~31) + 32;
		}
		if (elem >= nelems) {
		    syn->substate = 0;
		    syn->state = SYNC_UPD_FREE;
		    continue;
		}
		while (bit < 32 && !(*bp & (1<<bit))) { ++bit; }
		if (bit >= 32) {
		    syn->substate = (syn->substate & ~31) + 32;
		    continue;
		}
		start = elem*32 + bit;		/* relative starting sector */
		elem = (start+1)/32;		/* next element */
		bit = (start+1)&31;		/* next bit */
		bp = bits + elem;
		sects = 0;
		do {
		    if (elem >= nelems) {
			sects = elem*32 - start;
			break;
		    }
		    while (bit < 32 && (*bp & (1<<bit))) { ++bit; }
		    if (bit < 30 && ( (*bp & (1<<(bit+1))) || (*bp & (1<<(bit+2))) ) ) {
			bit += 3;
			continue;
		    } else if (bit < 31 && (*bp & (1<<(bit+1))) ) {
			bit += 2;
			continue;
		    }
		    if (bit >= 32) {
			++elem;
			++bp;
			bit = 0;
			continue;
		    }
		    sects = elem*32 + bit - start;
		} while (!sects);
		start_bytes = start*512;
		len = syn->buffer_size/512;
		if (sects > len) sects = len;
		sects_bytes = sects*512;
    		syn->substate = start + sects;	/* advance substate for next time */
		memcpy((char *)syn->output, (char *)(vol->index)+start_bytes, sects_bytes);
		len = vol->files_ffree*FSYS_MAX_ALTS*sizeof(U32); /* length of index file */
		if (start_bytes+sects_bytes > len) {
		    len -= start_bytes;
		    if (len > 0) {
			memset((char *)syn->output+len, 0, sects_bytes-len);
		    }
		}
		syn->state = SYNC_INDEX_WRCOMPL;
		qio->ramrp = (vol->files+FSYS_INDEX_INDEX)->ramrp + syn->alts; /* point to first retrieval pointer set */
		syn->sects = sects;		/* number of sectors to write */
		qio->u_len = sects_bytes;
		qio->buff = (U8*)syn->output;	/* point to output buffer */
		qio->state = 0;
		qio->sector = syn->start = start;
		SYNSQK(( OUTWHERE "%6d: sync, INDEX upd: Queuing write %d. u_len=%d, sector=%d\n",
	    		eer_rtc, syn->alts, qio->u_len, start));
		fsys_qwrite(ioq);
		return;
	    }

	    case SYNC_INDEX_WRCOMPL:
		if (QIO_ERR_CODE(ioq->iostatus)) { /* All we can do is log write errors */
#  if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
		    char emsg[AN_VIS_COL];
		    qio_errmsg(ioq->iostatus, emsg, sizeof(emsg));
		    iio_printf(fsys_iop, "%6d: sync error writing INDEX %d. u_len=%d, sector=%d\n",
			    eer_rtc, syn->alts, qio->u_len, syn->start);
		    iio_printf(fsys_iop, "        Reason: \"%s\"\n", emsg);
#  endif
		    syn->errlog[syn->err_in++] = ioq->iostatus;
		    if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
		    ++syn->errcnt;
		}
		syn->state = SYNC_UPD_INDEX;
		continue;

	    case SYNC_UPD_FREE:
#  if 0
		if (vol->free_start >= vol->free_ffree) {
		    syn->state = SYNC_DONE;
		    continue;
		}
#  endif
#  if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
		{
		    FsysRetPtr *rp;
		    rp = vol->free;
		    iio_printf(fsys_iop, "%6d: sync_upd_free: %d entries\n", eer_rtc, vol->free_ffree);
		    for (ii=0; ii < vol->free_ffree; ++ii, ++rp) {
			iio_printf( fsys_iop, "        %3d: start=%7d, nblocks=%6d\n",
				ii, rp->start, rp->nblocks);
		    }
		}
#  endif
		syn->state = SYNC_FREE_WRCOMPL;
    		syn->start = 0; 
		syn->sects = (vol->free_ffree*sizeof(FsysRetPtr) + 511)/512; 
		qio->ramrp = (vol->files+FSYS_INDEX_FREE)->ramrp+syn->alts; /* point to first retrieval pointer set */
		qio->u_len = syn->sects*512;
		qio->buff = (U8*)vol->free;
		qio->state = 0;
		qio->sector = syn->start;
		SYNSQK(( OUTWHERE "%6d: sync, FREE upd: Queuing write %d. u_len=%d, sector=%d\n",
	    		eer_rtc, syn->alts, qio->u_len, syn->start));
		fsys_qwrite(ioq);
		return;

	    case SYNC_FREE_WRCOMPL:
		if (QIO_ERR_CODE(ioq->iostatus)) { /* All we can do is log write errors */
#  if FSYS_FREE_SQUAWKING || FSYS_SQUAWKING
		    char emsg[AN_VIS_COL];
		    qio_errmsg(ioq->iostatus, emsg, sizeof(emsg));
		    iio_printf(fsys_iop, "%6d: sync error writing FREE %d. u_len=%d, sector=%d\n",
			    eer_rtc, syn->alts, qio->u_len, syn->start);
		    iio_printf(fsys_iop, "        Reason: \"%s\"\n", emsg);
#  endif
		    syn->errlog[syn->err_in++] = ioq->iostatus;
		    if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
		    ++syn->errcnt;
		}
		if (++syn->alts < FSYS_MAX_ALTS) {
		    syn->state = SYNC_UPD_INDEX;
		} else {
		    syn->state = SYNC_DONE;
		}
		continue;

# endif
	    case SYNC_DONE:
# if !FSYS_READ_ONLY
		if (vol->index_bits) memset((char *)vol->index_bits, 0, vol->index_bits_elems*sizeof(long));
		vol->free_start = INT_MAX;
# endif
		vol->dirty_ffree = 0;
		SYNSQK(( OUTWHERE "%6d: Sync done\n", eer_rtc));
		break;
	}		/* -- switch sync->state */
	break;
    }			/* -- while forever */

    ii = syn->busy;				/* save this for later */
    syn->busy = 0;				/* not busy anymore */
    syn->state = SYNC_BEGIN;			/* reset ourself */
    if (!syn->status) syn->status = FSYS_SYNC_SUCC|SEVERITY_INFO;
    sts = qio_freemutex(&vol->mutex, ioq);	/* done with the mutex */
    if (sts || syn->status != (FSYS_SYNC_SUCC|SEVERITY_INFO)) {
	syn->errlog[syn->err_in++] = sts ? sts : syn->status;
	if (syn->err_in > n_elts(syn->errlog)) syn->err_in = 0;
	++syn->errcnt;
    }
#if 0 && defined(IO_MAIN_LED_ON) && defined(IO_MAIN_CTL_T)
    IO_MAIN_CTL_T &= ~IO_MAIN_LED_ON;
#endif
    return;
}

static int fsys_sync(FsysSyncT *f, int how) {
    FsysVolume *vol;
    int sts = 0, oldipl;

    vol = f->vol;
    oldipl = prc_set_ipl(INTS_OFF);
    if (!f->busy && vol->dirty_ffree) {	/* if it not already running and there is something to do */
	f->busy = how;			/* say it is busy */
	prc_set_ipl(oldipl);
	SYNSQK(( OUTWHERE "%6d: sync: started. getting volume mutex\n", eer_rtc));
#if 0 && defined(IO_MAIN_LED_ON) && defined(IO_MAIN_CTL_T)
	IO_MAIN_CTL_T |= IO_MAIN_LED_ON;
#endif
	sts = qio_getmutex(&vol->mutex, fsys_sync_q, (QioIOQ *)f);
	if (sts) {
	    f->errlog[f->err_in++] = sts;
	    ++f->errcnt;
	    if (f->err_in >= n_elts(f->errlog)) f->err_in = 0;
	}
    	return sts;
    }
    prc_set_ipl(oldipl);
    SYNSQK(( OUTWHERE "%6d: sync: busy=%d, dirty_ffree=%d. Back to waiting\n",
		eer_rtc, f->busy, vol->dirty_ffree));
    return -1;
}

static int fsys_sync_time;

static void fsys_sync_t(QioIOQ *ioq) {
    FsysVolume *vol;
    FsysSyncT *f;

    f = (FsysSyncT *)ioq;
    vol = f->vol;
    fsys_sync(f, FSYS_SYNC_BUSY_TIMER);
    f->sync_t.delta = fsys_sync_time ? fsys_sync_time : FSYS_SYNC_TIMER; /* requeue the sync timer */
    tq_ins(&f->sync_t);
    return;
}

int fsys_sync_delay(int new) {
    int old, ii, oldps;
    FsysVolume *vol;

    oldps = prc_set_ipl(INTS_OFF);
    old = fsys_sync_time ? fsys_sync_time : FSYS_SYNC_TIMER;
    fsys_sync_time = new ? new : FSYS_SYNC_TIMER;
    vol = volumes;
    for (ii=0; ii < FSYS_MAX_VOLUMES; ++ii, ++vol) {
	struct tq *tq;
	tq = &vol->sync_work.sync_t;
	if (tq->que) {
	    tq_del(tq);
	    tq->delta = fsys_sync_time;
	    tq_ins(tq);
	    prc_set_ipl(oldps);
	    fsys_sync(&vol->sync_work, 0);	/* force a sync */
	    prc_set_ipl(INTS_OFF);
	}
    }
    prc_set_ipl(oldps);
    return old;
}

static int fsys_fsync( QioIOQ *ioq ) {
    FsysVolume *vol;

    if (!ioq) return QIO_INVARG;
    vol = fsys_get_volume(ioq->file);
    qio_freefile(qio_fd2file(ioq->file)); /* done with the file descriptor */
    ioq->file = -1;
    if (!vol) return (ioq->iostatus = FSYS_SYNC_NOMNT);
    fsys_sync(&vol->sync_work, FSYS_SYNC_BUSY_NONTIMER);
    ioq->iostatus = FSYS_SYNC_SUCC|SEVERITY_INFO;	/* never report a sync error */
    return 0;
}
#endif

enum mount_e {
    MOUNT_BEGIN,
    MOUNT_RD_HOME,
    MOUNT_CK_HOME,
    MOUNT_SEARCH_HOME,
    MOUNT_RD_FH,
    MOUNT_PROC_FH,
    MOUNT_RD_FILE,
    MOUNT_PROC_FILE
};

/*****************************************************************
 * fsys_qmount() - mount a volume. This function performs the steps necessary to
 * read the home blocks, index.sys, freemap.sys and directory files. It
 * places portions of those files into the 'volumes' array described above.
 * This routine is expected to run entirely as an AST function.
 *
 * At entry:
 *	vol - pointer to element in volumes array to which to mount.
 * At exit:
 *	returns nothing. 
 */
void fsys_qmount(QioIOQ *ioq) {
    FsysQio *qio;
    FsysHomeBlock *hb;
    FsysHeader *hdr;
    FsysRamFH *rfh;
    FsysRamRP *rrp;
    FsysVolume *vol;
    int ii, sts;
    unsigned long *ulp;

    vol = (FsysVolume *)ioq;
    qio = (FsysQio *)ioq;
    while (1) {
	switch (vol->state) {			/* next? */
	    default:			/* Better never get here */
		ioq = qio->callers_ioq;
		vol->state = MOUNT_BEGIN;
		ioq->iostatus = vol->status = FSYS_MOUNT_FATAL;
		qio_complete(ioq);
		return;
/*****************************************************************************
 * First we init some variables, grab the mutex and switch to AST level.
 */
	    case MOUNT_BEGIN: {			/* setup to read home blocks */
		struct act_q *q;
#if !FSYS_READ_ONLY || FSYS_UPD_FH
		struct tq *qt;
		FsysSyncT *sw;

		sw = &vol->sync_work;
		qt = &sw->sync_t;
		qt->func = (void (*)(void *))fsys_sync_t;
		qt->vars = (void *)sw;
		sw->vol = vol;
#endif
		if (!vol->buff) {
		    vol->buff_size = 512;
		    vol->buff = (U32*)QMOUNT_ALLOC(vol, 512);	/* get a temporary sector buffer */
		    if (!vol->buff) {
			vol->status = FSYS_MOUNT_NOMEM;
			goto clean_up;
		    }
		}
		vol->substate = 0;		/* substate */
		vol->status = 0;		/* start with no status */
		vol->files_indx = 0;		/* start at index file */
		/* build a dummy retrieval pointer to get file headers... */
		vol->tmprp.start = 0;		/* "file header"'s are found in a a pseudo file starting at 0 */
		vol->tmprp.nblocks = vol->maxlba; /* ...and comprising the whole disk */
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
		vol->tmprp.repeat = 1;
		vol->tmprp.skip = 0;
#endif
		vol->tmpramrp.rptrs = &vol->tmprp;
		vol->tmpramrp.num_rptrs = 1;
#if !FSYS_READ_ONLY
		vol->tmpramrp.next = 0;
		vol->tmpramrp.rptrs_size = 1;
#endif
		qio->ramrp = &vol->tmpramrp;	/* init the reader stuff */
		qio->u_len = 512;
		qio->buff = (U8*)vol->buff;	/* into our buffer */
		qio->compl = fsys_qmount;	/* come back to us when done */
		q = &vol->tmpq;
		q->action = (void (*)(void *))fsys_qmount;
		q->param = (void *)ioq;
		q->next = q->que = 0;
		vol->state = MOUNT_RD_HOME;	/* switch to next state */
		vol->total_free_clusters = 0;
		vol->total_alloc_clusters = 1 + FSYS_MAX_ALTS;
		sts = qio_getmutex(&vol->mutex, fsys_qmount, ioq);
		if (sts) {
		    vol->status = sts;
		    goto clean_up;
		}
		return;
	    }

/*****************************************************************************
 * Loop however many times it takes to read a home block without error or until
 * all the home blocks have been tried. If it cannot find a home block where one
 * is expected, it reads every 256'th sector (+1, skipping sector 1) looking for
 * a sector that it recognizes as a home block. The read procedure hops back and
 * forth between states MOUNT_RD_HOME, MOUNT_CK_HOME and MOUNT_SEARCH_HOME until
 * a home block is successfully read.
 */
	    case MOUNT_RD_HOME: 		/* loop reading home blocks until a good one is found */
		qio->sector = FSYS_HB_ALG(vol->substate, vol->maxlba); /* relative sector number */
		qio->u_len = 512;
		qio->buff = (U8*)vol->buff;	/* into our buffer */
		vol->state = MOUNT_CK_HOME;
		fsys_qread(ioq);
		return;				/* wait for I/O to complete */

	    case MOUNT_CK_HOME: {
		unsigned long cs;
		hb = (FsysHomeBlock *)vol->buff;
		ulp = vol->buff;
		for (cs=0, ii=0; ii < 128; ++ii) cs += *ulp++; /* checksum the home block */
		if (cs || 			/* checksum is expected to be 0 */
		    QIO_ERR_CODE(ioq->iostatus) ||	/* no errors are expected or accepted */
		    hb->id != FSYS_ID_HOME) {	/* block is wrong kind */
		    if (++vol->substate < FSYS_MAX_ALTS) {
			vol->state = MOUNT_RD_HOME;
			continue;		/* recurse (read next home block) */
		    }
		    vol->state = MOUNT_SEARCH_HOME;	/* search for home block */
		    continue;
		}
		if (vol->substate > 0) USED_ALT();
		for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
		    vol->index_lbas[ii] = hb->index[ii];	/* remember lba's for index file */
		}
		vol->substate = 0;
		vol->state = MOUNT_RD_FH;		/* goto to next state */
		continue;
	    }

	    case MOUNT_SEARCH_HOME: {
		int nxt;
		nxt = (vol->substate-FSYS_MAX_ALTS+1)*256 + 1;
		if (nxt >= vol->maxlba) {
		    vol->status = FSYS_MOUNT_NOHBLK ;	/* could not read any of the home blocks */
clean_up:
#if FSYS_UMOUNT
		    qmount_freeall(vol);
#endif
		    vol->files = 0;
		    vol->files_elems = vol->files_ffree = 0;
		    vol->buff = 0;
		    vol->index = 0;
		    vol->free = 0;
		    vol->free_ffree = vol->free_elems = 0;
		    vol->state = MOUNT_BEGIN;
		    qio_freemutex(&vol->mutex, ioq);
		    ioq = qio->callers_ioq;
		    ioq->iostatus = vol->status;
		    qio_complete(ioq);
		    return;
		}
		qio->sector = nxt;		/* relative sector number */
		qio->u_len = 512;
		qio->buff = (U8*)vol->buff;	/* into our buffer */
		vol->state = MOUNT_CK_HOME;
		fsys_qread(ioq);
		return;				/* wait for I/O to complete */
	    }

/*****************************************************************************
 * The home block has the LBA's to find the file header(s) for the index file.
 *
 * The next step is to read all the file headers on the disk starting with the
 * index file. As file headers are read, their contents are stuffed into a
 * 'malloc'd FsysRamFH struct. The FsysRamFH structs are maintained as a linear
 * array, one element per file, the 'array index' of which matches the file's
 * position in the index file (its FID). This allows one to access both the pointer
 * to the LBA's in the index file and the ram copies of the file header with the
 * file's FID. 
 *
 * The read procedure hops back and forth between states MOUNT_RD_FH and MOUNT_PROC_FH
 * until a file header is read successfully. At this time, an unrecoverable error
 * reading a file header is fatal.
 */
	    case MOUNT_RD_FH: {
		int bad, ii;
		unsigned long sect;

		if (vol->files_indx == FSYS_INDEX_INDEX) {	/* reading index file header */
		    ulp = vol->index_lbas;		/* lba's are in a special spot while reading index.sys */
		} else {
		    if (vol->files_indx >= vol->files_ffree) {	/* if we've reached the end */
			break;				/* we're done with all I/O */
		    }
		    ulp = vol->index + vol->files_indx*FSYS_MAX_ALTS; /* any other file header */
		}
		if (!vol->substate) {			/* on the first pass of a file header read */
		    unsigned long *p;
		    bad = 0;
		    p = ulp;
		    for (ii=0; ii < FSYS_MAX_ALTS; ++ii, ++p) { /* check to see if any of them are bad */
			sect = *p;
			if (!sect || (!(sect&0x80000000) && sect >= vol->maxblocks)) {	/* if value is illegal */
			    sect = ii ? 0x80000000 : 0x80000001; /* record it as unused */
			}
			if ((sect&0x80000000)) {		/* if unused */
#if !FSYS_READ_ONLY
			    if (!ii) {			/* and the first one */
				add_to_unused(vol, vol->files_indx);
			    }
#endif
			    ++bad;			/* record a bad index */
			}
			if (bad) {
			    *p = sect;			/* replace with unused flag */
			}
		    }
		    if (bad >= FSYS_MAX_ALTS) {		/* if all are bad */
			if (vol->files_indx <= FSYS_INDEX_ROOT) {
			    if (vol->files_indx == FSYS_INDEX_INDEX) {
				vol->status = FSYS_MOUNT_NOINDX;
			    } else if (vol->files_indx == FSYS_INDEX_FREE) {
				vol->status = FSYS_MOUNT_NOFREE;
			    } else {
				vol->status = FSYS_MOUNT_NOROOT;
			    }
			    goto clean_up;		/* can't continue */
			}
			++vol->files_indx;		/* skip this fileheader */
			continue;			/* loop */
		    }
		}
		while (vol->substate < FSYS_MAX_ALTS) {
		    qio->sector = ulp[vol->substate++];
		    qio->ramrp = &vol->tmpramrp;	/* init the reader stuff */
		    qio->u_len = 512;
		    qio->buff = (U8*)vol->buff;		/* into our local header buffer */
		    vol->state = MOUNT_PROC_FH;
		    fsys_qread(ioq);			/* read the next copy of file header */
		    return;
		}		
		vol->status = FSYS_MOUNT_FHRDERR;	/* for now, an unreadable header is fatal */
		goto clean_up;
	    }

	    case MOUNT_PROC_FH: {		/* process the file header */
		unsigned long id;
		hdr = (FsysHeader *)vol->buff;
		id = (vol->files_indx == FSYS_INDEX_INDEX) ? FSYS_ID_INDEX : FSYS_ID_HEADER;
		if (QIO_ERR_CODE(ioq->iostatus) || 	/* can't have any input errors */
		    hdr->id != id) {		/* and the id needs to be the correct type */
		    vol->state = MOUNT_RD_FH;	/* try to read the next one */
		    continue;
		}
		if (vol->substate > 1) USED_ALT();
		if (vol->files_indx == FSYS_INDEX_INDEX) { /* index file is done first */
		    ii = hdr->size/(FSYS_MAX_ALTS*sizeof(long));
		    vol->files_ffree = ii;		/* point to end of active list */
#if !FSYS_READ_ONLY 
# if !FSYS_TIGHT_MEM
		    ii += 512;			/* allow easy "growth" of 512 files */
# else
		    ii += 32;			/* allow easy "growth" of 32 files */
# endif
#endif
		    vol->files = (FsysRamFH *)QMOUNT_ALLOC(vol, ii*sizeof(FsysRamFH));
#if !FSYS_READ_ONLY
		    if (!vol->files) {
# if !FSYS_TIGHT_MEM
			ii -= 512-16;		/* running tight on space, try something smaller */
# else
			ii -= 32-4;		/* running tight on space, try something smaller */
# endif
			vol->files = (FsysRamFH *)QMOUNT_ALLOC(vol, ii*sizeof(FsysRamFH));
		    }		
#endif
		    if (!vol->files) {
			vol->status = FSYS_MOUNT_NOMEM;	/* ran out of memory */
			goto clean_up;
		    }
		    vol->files_elems = ii;		/* total available elements */
		    vol->index = (U32*)QMOUNT_ALLOC(vol,
    				(ii*FSYS_MAX_ALTS*sizeof(long)+511)&-512); /* room to read the whole index file */
		    if (!vol->index) {
			vol->status = FSYS_MOUNT_NOMEM;
			goto clean_up;
		    }
		    vol->contents = vol->index;
#if FSYS_TIGHT_MEM
		    vol->rp_pool = (FsysRetPtr *)QMOUNT_ALLOC(vol, FSYS_MAX_ALTS*ii*sizeof(FsysRetPtr));
		    if (vol->rp_pool) vol->rp_pool_size = ii;
#endif
		} else if (vol->files_indx == FSYS_INDEX_FREE) {
#if !FSYS_READ_ONLY
		    vol->free_elems = ((hdr->size+1023)&-512)/sizeof(FsysRetPtr);
#else
		    vol->free_elems = ((hdr->size+511)&-512)/sizeof(FsysRetPtr);
#endif
		    vol->free = (FsysRetPtr *)QMOUNT_ALLOC(vol,
			    (vol->free_elems*sizeof(FsysRetPtr)+511)&-512); /* room to read the whole file */
		    if (!vol->free) {
			vol->status = FSYS_MOUNT_NOMEM;
			goto clean_up;
		    }
		    vol->contents = (unsigned long *)vol->free;
		} 
		vol->rw_amt = (hdr->size+511)&-512;
		rfh = vol->files + vol->files_indx;
		if (mk_ramfh(vol, rfh)) goto clean_up;
		if (vol->files_indx <= FSYS_INDEX_FREE) {
		    rfh->def_extend = FSYS_DEFAULT_DIR_EXTEND;
		}
		if (vol->files_indx > FSYS_INDEX_FREE && hdr->type != FSYS_TYPE_DIR) {
		    ++vol->files_indx;
		    vol->substate = 0;
		    vol->state = MOUNT_RD_FH;		/* read the next file header */
		    continue;				/* loop */
		}
		if (hdr->type == FSYS_TYPE_DIR) {
		    int need;
		    need = (hdr->size+511)&-512;
		    if (!need) {			/* empty directory!!!! This is a bug */
			++vol->files_indx;		/* so skip it. */
			vol->substate = 0;
			vol->state = MOUNT_RD_FH;	/* read the next file header */
			continue;			/* loop */
		    }			
		    if (vol->buff_size < need) {
			U32 *bp;
			vol->buff_size = need;
			bp = (U32*)QMOUNT_REALLOC(vol, (void *)vol->buff, vol->buff_size); /* give back old buffer */
			if (!bp) {
			    vol->status = FSYS_MOUNT_NOMEM;
			    goto clean_up;
			}
			vol->buff = bp;
		    }
		    vol->contents = vol->buff;
		}
		vol->state = MOUNT_RD_FILE;	/* next state */
		vol->substate = 0;		/* substate */
		continue;
	    }

/*****************************************************************************
 * The file header has been read. If it is a type INDEX, FREEMAP or DIRECTORY,
 * the contents need to be obtained. The read procedure hops between states MOUNT_RD_FILE
 * and MOUNT_PROC_FILE until the contents are successfully obtained. Note that these three
 * types of files are always duplicated FSYS_MAX_ALTS times on the disk.
 *
 * At this time, an unrecoverable error reading the contents of one of these
 * files is fatal.
 */
	    case MOUNT_RD_FILE: 		/* loop reading files until a good one is found */
		rfh = vol->files + vol->files_indx;
		while (vol->substate < FSYS_MAX_ALTS) {
		    rrp = rfh->ramrp + vol->substate++;
		    if (!rrp->num_rptrs) continue;
		    qio->ramrp = rrp;
		    qio->sector = 0;		/* read file starting at relative sector 0 */
		    qio->u_len = vol->rw_amt;
		    qio->buff = (U8*)vol->contents;
		    vol->state = MOUNT_PROC_FILE;
		    fsys_qread(ioq);		/* read the whole damn file at once */
		    return;				/* wait for I/O to complete */
		}		
		vol->status = FSYS_MOUNT_RDERR;	/* for now, an unreadable file is fatal */
		goto clean_up;

	    case MOUNT_PROC_FILE: 
		if (QIO_ERR_CODE(ioq->iostatus)) {	/* can't have any input errors */
		    vol->state = MOUNT_RD_FILE; /* try to read the alternates */
		    continue;
		}

		if (vol->files_indx == FSYS_INDEX_INDEX) {
		    U32 *ulp;
		    int jj, bad=0;
		    ulp = (U32 *)vol->contents;
		    for (jj=0; jj < FSYS_MAX_ALTS; ++jj) {
			if (!jj && vol->index_lbas[jj] != ulp[jj]) {
			    ++bad;
			    continue;
			}
			if (!(vol->index_lbas[jj]&0x80000000) &&
				ulp[0] != vol->index_lbas[0]) {
			    ++bad;
			    continue;
			}
		    }
		    if (bad) {
			vol->state = MOUNT_RD_FILE;	/* doesn't match, reject it */
			continue;
		    }
		}

		if (vol->substate > 1) USED_ALT();
/*
 * If the file is of type DIRECTORY, a FsysDir struct is created for it and its
 * contents copied into that struct.
 */
		if (vol->files_indx > FSYS_INDEX_FREE) {	/* Not index or free, so has to be dir */
		    if (mk_ramdir(vol)) goto clean_up;	/* create a directory for it */
		}
		++vol->files_indx;
		vol->state = MOUNT_RD_FH;	/* read the next file */
		vol->substate = 0;
		continue;
	}				/* -- switch state */
	break;
    }					/* -- while forever */
#if !FSYS_READ_ONLY || FSYS_UPD_FH
    vol->sync_work.sync_t.delta = FSYS_SYNC_TIMER;
    tq_ins(&vol->sync_work.sync_t);	/* start a sync timer */
#endif
    QIOfree(vol->buff);			/* done with this buffer */
    vol->total_free_clusters = compute_total_free(vol);
    vol->buff = 0;
    vol->state = MOUNT_BEGIN;		/* done */
    vol->status = FSYS_MOUNT_SUCC|SEVERITY_INFO; /* mounted with success */
#if !FSYS_READ_ONLY
    vol->free_start = INT_MAX;		/* start it at max */
#endif
    sts = qio_freemutex(&vol->mutex, ioq);
    ioq = qio->callers_ioq;
    ioq->iostatus = vol->status;
    qio_complete(ioq);
    return;
}

#if KICK_THE_DOG
static void kick_the_dog(void *arg) {
    struct tq *tq;

    WDOG = 0;			/* kick the dog */
    tq = (struct tq *)arg;
    tq->delta = 500000;		/* 500,000 usecs or 0.5 secs */
    tq_ins(tq);			/* requeue ourself */
}
#endif
    
extern void ide_squawk(int row, int col);
extern void ide_unsquawk(void);

/*****************************************************************
 * fsys_mountw() - mount a volume and wait for completion. 
 *
 * At entry:
 *	where - null terminated string with name of physical device 
 *	what - null terminated string with name of virtual device
 *
 * At exit:
 *	returns  0 if volume is successfully mounted.
 *	returns  FSYS_MOUNT_xxx if other errors.
 */

int fsys_mountw(const char *where, const char *what) {
    FsysVolume *vol;
    const QioDevice *d;
    QioIOQ *ioq, *fioq;
    int sts;
    struct stat fs;
#if KICK_THE_DOG
    struct tq timer;
#endif
    
    if (prc_get_astlvl() >= 0) return FSYS_MOUNT_BADLVL;
    ide_init();				/* make sure drive is init'd */
    d = qio_lookupdvc(what);		/* see if our mount point is installed */
    if (!d) return FSYS_MOUNT_NSV;	/* no such volume */
    vol = (FsysVolume *)d->private;	/* get ptr to our volume */
    if (!vol) return FSYS_MOUNT_FATAL;
    if (vol->state) return FSYS_MOUNT_BUSY; /* volume is already mounting */
    if (vol->files) return FSYS_MOUNT_MOUNTED;	/* volume is already mounted */
#if KICK_THE_DOG
    timer.next = timer.que = 0;
    timer.func = kick_the_dog;
    timer.vars = (void *)&timer;
    timer.delta = 500000;
    tq_ins(&timer);
#endif
    ioq = qio_getioq();			/* get an IOQ struct */
    ioq->timeout = 5000000;
    sts = qio_open(ioq, where, O_RDWR);
    while (!sts) { sts = ioq->iostatus; }	/* wait for it */
    if (QIO_ERR_CODE(sts)) {
	qio_freeioq(ioq);
#if KICK_THE_DOG
	tq_del(&timer);
#endif
	return sts;			/* didn't work */
    }
    sts = qio_fstat(ioq, &fs);		/* get stats about the physical device */
    while (!sts) { sts = ioq->iostatus; }	/* wait for it */
    if (QIO_ERR_CODE(sts)) {
	int o;
	o = qio_close(ioq);
	while (!o) { o = ioq->iostatus; }
	qio_freeioq(ioq);
#if KICK_THE_DOG
	tq_del(&timer);
#endif
	return sts;			/* could not stat the physical device */
    }
    vol->iofd = ioq->file;		/* remember the FD we're to use */
    vol->maxlba = fs.st_size - (fs.st_size % FSYS_MAX_ALTS); /* keep device size in sectors */
#if !WATCOM_LIBC
    vol->maxblocks = fs.st_blocks;	/* get physical limit */
#else
    vol->maxblocks = vol->maxlba;	/* get physical limit (same as max sectors in DOS/Windows) */
#endif
    vol->reader.callers_ioq = ioq;
    fioq = &vol->reader.our_ioq;
    fioq->file = ioq->file;
    ioq->iostatus = 0;
    ioq->iocount = 0;
    ioq->timeout = 10000000;		/* give 'em 10 seconds or so to mount */
    fsys_qmount(fioq);			/* queue the mount */
    while (!(sts=ioq->iostatus)) { ; }	/* wait for mount to complete */
    qio_freeioq(ioq);			/* we're done with the ioq */
#if KICK_THE_DOG
    tq_del(&timer);
#endif
    return sts;				/* return with whatever status was reported */
}

#if !FSYS_READ_ONLY
static void mk_freelist(FsysRetPtr *list, int nelts, int size) {
    int ii;
    unsigned long prev;

    memset((char *)list, 0, nelts*sizeof(FsysRetPtr));
    prev = 1;				/* start allocating free blocks at 1 */
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
	int amt;
	unsigned long hb;

	hb = FSYS_HB_ALG(ii, size);
	amt = hb - prev;
	if (amt) {
	    list->start = prev;
	    list->nblocks = amt;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
	    list->repeat = 1;
	    list->skip = 0;
#endif
	    ++list;
	}
	prev = hb+1;
    }
    if (prev < size) {
	list->start = prev;
	list->nblocks = size-prev;
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
	list->repeat = 1;
	list->skip = 0;
#endif
    }
    return;
}

static int fsys_mkalloc( FsysFindFreeT *freet ) {
    int ii, alloc;

    alloc = freet->request;
    for (ii=0; ii < FSYS_MAX_FHPTRS; ++ii) {
	if (fsys_findfree( freet )) break; /* no more free space */
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
# error You need to rewrite fsys_mkalloc and add repeat/skip support.
#endif
	alloc -= freet->reply->nblocks;
	if (alloc <= 0) break;
	freet->reply += 1;
    }
    return alloc;
}

typedef struct {
    FsysQio q;			/* !!! this absolutely needs to be the first member !!! */
    FsysInitVol *iv;
    struct act_q act;
    int state;
    int substate;
    int status;
    FsysHomeBlock *hb;
    FsysHeader ihdr, fhdr, rhdr;
    FsysRetPtr *fake_free;
    FsysRetPtr freep, tmprp;
    FsysRamRP ramrp;
    unsigned long *dir;
    unsigned long *index;
    unsigned long ilbas[FSYS_MAX_ALTS];
    unsigned long max_lba;
    int free_nelts;
} FsysFinit;

static void compute_home_blocks(FsysFinit *fin) {
    FsysHomeBlock *hb;
    FsysInitVol *iv;
    int size;
    U32 *ips;
    int ii;
    unsigned long sa, *tmp;

    hb = fin->hb;
    size = fin->max_lba;
    ips = fin->ilbas;
    iv = fin->iv;
    hb->id = FSYS_ID_HOME;
    hb->hb_minor = FSYS_VERSION_HB_MINOR;
    hb->hb_major = FSYS_VERSION_HB_MAJOR;
    hb->hb_size = sizeof(FsysHomeBlock);
    hb->fh_minor = FSYS_VERSION_FH_MINOR;
    hb->fh_major = FSYS_VERSION_FH_MAJOR;
    hb->fh_size = sizeof(FsysHeader);
    hb->fh_ptrs = FSYS_MAX_FHPTRS;
#if (FSYS_FEATURES&FSYS_FEATURES_EXTENSION_HEADER)
    hb->efh_minor = FSYS_VERSION_EFH_MINOR;
    hb->efh_major = FSYS_VERSION_EFH_MAJOR;
    hb->efh_size = sizeof(FsysEHeader);
    hb->efh_ptrs = FSYS_MAX_EFHPTRS;
#else
    hb->efh_minor = 0;
    hb->efh_major = 0;
    hb->efh_size = 0;
    hb->efh_ptrs = 0;
#endif
    hb->rp_minor = FSYS_VERSION_RP_MINOR;
    hb->rp_major = FSYS_VERSION_RP_MAJOR;
    hb->rp_size = sizeof(FsysRetPtr);
    hb->cluster = FSYS_CLUSTER_SIZE;
    hb->maxalts = FSYS_MAX_ALTS;
    hb->atime = hb->ctime = hb->mtime = (unsigned long)time(0);
    hb->btime = 0;
    hb->options = FSYS_OPTIONS;
    hb->features = FSYS_FEATURES;
    for (ii=0; ii < FSYS_MAX_ALTS; ++ii) {
	hb->index[ii] = *ips++;
    }
    hb->def_extend = iv->def_extend;
    for (tmp=(unsigned long *)hb, sa=0, ii=0; ii < 512/4; ++ii) sa += *tmp++;
    hb->chksum = 0-sa;
    return;
}

enum init_e {
    INIT_BEGIN,
    INIT_MKFREE,
    INIT_WRITE_INDEX_FH,
    INIT_WRITE_INDEX,
    INIT_WRITE_FREE_FH,
    INIT_WRITE_FREE,
    INIT_WRITE_ROOT_FH,
    INIT_WRITE_ROOT,
    INIT_WRITE_HOME,
    INIT_NEXT_ALT
};

static void init_file_system(QioIOQ *ioq) {
    int ii, bcnt, sts=0;
    FsysFinit *fin;
    FsysInitVol *iv;
    unsigned long *indxp;

    fin = (FsysFinit *)ioq;
    iv = fin->iv;
    while (1) {
	switch (fin->state) {
	    case INIT_BEGIN: {
		QioIOQ *eioq;
		eioq = fin->q.callers_ioq;
		if (!iv) {
		    sts = FSYS_INITFS_NOIV;
initfs_error:
		    eioq->iostatus = fin->status = sts;
		    return;
		}
		if (iv->cluster != 1) {
		    sts = FSYS_INITFS_CLUSTERNOT1;
		    goto initfs_error;
		}
		if (iv->index_sectors + iv->free_sectors + iv->root_sectors >= fin->max_lba) {
		    sts = FSYS_INITFS_TOOMANYCLUST;
		    goto initfs_error;
		}
		if (!iv->cluster) iv->cluster = FSYS_CLUSTER_SIZE;
		if (!iv->free_sectors) iv->free_sectors = 50;
		if (!iv->index_sectors) iv->index_sectors = 50;
		if (!iv->root_sectors) iv->root_sectors = 10;
		if (!iv->def_extend) iv->def_extend = FSYS_DEFAULT_EXTEND;
		fin->fake_free = (FsysRetPtr *)QIOmalloc(iv->free_sectors*512);
		if (!fin->fake_free) {
		    sts = FSYS_INITFS_NOMEM;
		    goto initfs_error;
		}
		fin->free_nelts = (iv->free_sectors*512)/sizeof(FsysRetPtr);
		fin->index = (unsigned long *)QIOcalloc(iv->index_sectors*512, 1);	    
		if (!fin->index) {
		    QIOfree(fin->fake_free);
		    sts = FSYS_INITFS_NOMEM;
		    goto initfs_error;
		}
		fin->dir = (unsigned long *)QIOcalloc(iv->root_sectors*512, 1);
		if (!fin->dir) {
		    QIOfree(fin->fake_free);
		    QIOfree(fin->index);
		    sts = FSYS_INITFS_NOMEM;
		    goto initfs_error;
		}
		fin->hb = (FsysHomeBlock *)QIOcalloc(512, 1);
		if (!fin->hb) {
		    sts = FSYS_INITFS_NOMEM;
		    QIOfree(fin->fake_free);
		    QIOfree(fin->index);
		    QIOfree(fin->dir);
		    goto initfs_error;
		}
		fin->state = INIT_MKFREE;
		fin->act.action = (void (*)(void *))init_file_system;
		fin->act.param = (void *)ioq;
		prc_q_ast(QIO_ASTLVL, &fin->act);	/* jump to AST level */
		return;
	    }

	    case INIT_MKFREE: {
		FsysFindFreeT freet;
		unsigned char *s;

		/* First create a free list that accounts for the holes caused by home blocks */
		mk_freelist(fin->fake_free, fin->free_nelts, fin->max_lba);	/* assume a n block filesystem */

		/* start an index.sys file */
		fsys_mkheader(&fin->ihdr, 1);	/* make an index file header */
		fin->ihdr.size = 3*FSYS_MAX_ALTS*sizeof(long);
		fin->ihdr.id = FSYS_ID_INDEX;
		fin->ihdr.type = FSYS_TYPE_INDEX;
		fsys_mkheader(&fin->fhdr, 1);	/* make a freemap file header */
		fsys_mkheader(&fin->rhdr, 1);	/* make a root file header */
		fin->rhdr.type = FSYS_TYPE_DIR;

		freet.skip = 0;
		freet.exact = 0;
		freet.nelts = fin->free_nelts;
		freet.freelist = fin->fake_free;
		freet.hint = 0;

		for (sts=bcnt=0; bcnt < FSYS_MAX_ALTS; ++bcnt) {
		    /* get a free block for the index.sys file header */

		    freet.lo_limit = FSYS_HB_ALG(bcnt, fin->max_lba);
		    freet.request = 1;
		    freet.reply = &fin->freep;
		    if (fsys_findfree( &freet )) { /* get a free block */
no_free:
			sts = FSYS_INITFS_NOFREE;
			break;
		    }
		    /* point index to itself */
		    indxp = fin->index+(FSYS_INDEX_INDEX*FSYS_MAX_ALTS);
		    indxp[bcnt] = fin->freep.start;

		    /* tell home block where index.sys's file header is */
		    fin->ilbas[bcnt] = fin->freep.start;

		    /* allocate some blocks for the index file itself */
		    fin->ihdr.clusters = freet.request = iv->index_sectors;
		    freet.reply = fin->ihdr.pointers[bcnt];
		    if (fsys_mkalloc( &freet )) goto no_free;

		    freet.request = 1;
		    freet.reply = &fin->freep;
		    /* get a free block for the freelist.sys file header */
		    if (fsys_findfree( &freet )) goto no_free;

		    /* point index file to freelist.sys's file header */
		    indxp = fin->index+(FSYS_INDEX_FREE*FSYS_MAX_ALTS);
		    indxp[bcnt] = fin->freep.start;
		    
		    /* allocate some blocks for the freelist file itself */
		    fin->fhdr.clusters = freet.request = iv->free_sectors;
		    freet.reply = fin->fhdr.pointers[bcnt];
		    if (fsys_mkalloc( &freet )) goto no_free;

		    freet.request = 1;
		    freet.reply = &fin->freep;
		    /* get a free block for the root directory file header */
		    if (fsys_findfree( &freet )) goto no_free;

		    /* point index file to root directory file */
		    indxp = fin->index+(FSYS_INDEX_ROOT*FSYS_MAX_ALTS);
		    indxp[bcnt] = fin->freep.start;
		    
		    /* create a file header for root directory and allocate some blocks for the file itself */
		    freet.request = iv->root_sectors;
		    fin->rhdr.clusters = freet.request = iv->root_sectors;
		    freet.reply = fin->rhdr.pointers[bcnt];
		    if (fsys_mkalloc( &freet )) goto no_free;

		}

		if (sts) break;

		compute_home_blocks(fin);

		s = (unsigned char *)fin->dir;
		*s++ = FSYS_INDEX_ROOT;		/* ".." points to parent */
		*s++ = FSYS_INDEX_ROOT>>8;
		*s++ = FSYS_INDEX_ROOT>>16;
		*s++ = 1;			/* parent's generation number starts a 1 */
		*s++ = 3;			/* string length (3) */
		*s++ = '.';			/* filename (..) */
		*s++ = '.';
		*s++ = 0;			/* null terminate the string */
		*s++ = FSYS_INDEX_ROOT;		/* "." points to ourself */
		*s++ = FSYS_INDEX_ROOT>>8;
		*s++ = FSYS_INDEX_ROOT>>16;
		*s++ = 1;			/* owner's generation number starts at 1 */
		*s++ = 2;			/* string length (2) */
		*s++ = '.';			/* filename */
		*s++ = 0;			/* null terminate the string */
		*s++ = 0;			/* index of 0 means end of list */
		*s++ = 0;
		*s++ = 0;
		fin->rhdr.size = s - (U8*)fin->dir;

		for (ii=0; ii < fin->free_nelts; ++ii) {
		    if (fin->fake_free[ii].start == 0 && fin->fake_free[ii].nblocks == 0) break;
		}
		fin->fhdr.size = (ii*sizeof(FsysRetPtr)+511)&-512;
		fin->substate = 0;
		fin->tmprp.start = 0;		/* whole disk starts at sector 0 */
		fin->tmprp.nblocks = fin->max_lba;	/* the whole damn disk is one file */
#if (FSYS_OPTIONS&FSYS_FEATURES&FSYS_FEATURES_SKIP_REPEAT)
		fin->tmprp.repeat = 1;
		fin->tmprp.skip = 0;
#endif
		fin->ramrp.rptrs = &fin->tmprp;
		fin->ramrp.num_rptrs = 1;
		fin->ramrp.next = 0;
		fin->ramrp.rptrs_size = 0;
		fin->q.ramrp = &fin->ramrp;
		fin->q.compl = init_file_system;
		fin->state = INIT_WRITE_HOME;
		continue;
	    }

	    case INIT_WRITE_HOME:
		/* write a home block */
		fin->ramrp.rptrs = &fin->tmprp;
		fin->q.buff = (U8*)fin->hb;
		fin->q.sector = FSYS_HB_ALG(fin->substate, fin->max_lba);
		fin->q.u_len = 512;
		fin->state = INIT_WRITE_INDEX_FH;
		fsys_qwrite(ioq);
		return;

	    case INIT_WRITE_INDEX_FH:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADHB;
		    break;
		}
		/* write the file header for index.sys */
		indxp = fin->index+(FSYS_INDEX_INDEX*FSYS_MAX_ALTS);
		fin->q.buff = (U8*)&fin->ihdr;
		fin->q.sector = indxp[fin->substate];
		fin->q.u_len = 512;
		fin->state = INIT_WRITE_INDEX;
		fsys_qwrite(ioq);
		return;

	    case INIT_WRITE_INDEX:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADINDX;
		    break;
		}
		/* write the index.sys file */
		fin->ramrp.rptrs = fin->ihdr.pointers[fin->substate];
		fin->q.buff = (U8*)fin->index;
		fin->q.sector = 0;
		fin->q.u_len = iv->index_sectors*512;
		fin->state = INIT_WRITE_FREE_FH;
		fsys_qwrite(ioq);
		return;

	    case INIT_WRITE_FREE_FH:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADINDXF;
		    break;
		}
		/* write the file header for freelist.sys */
		fin->ramrp.rptrs = &fin->tmprp;
		indxp = fin->index+(FSYS_INDEX_FREE*FSYS_MAX_ALTS);
		fin->q.buff = (U8*)&fin->fhdr;
		fin->q.sector = indxp[fin->substate];
		fin->q.u_len = 512;
		fin->state = INIT_WRITE_FREE;
		fsys_qwrite(ioq);
		return;

	    case INIT_WRITE_FREE:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADFREE;
		    break;
		}
		/* write the freelist.sys file */
		fin->ramrp.rptrs = fin->fhdr.pointers[fin->substate];
		fin->q.buff = (U8*)fin->fake_free;
		fin->q.sector = 0;
		fin->q.u_len = iv->free_sectors*512;
		fin->state = INIT_WRITE_ROOT_FH;
		fsys_qwrite(ioq);
		return;

	    case INIT_WRITE_ROOT_FH:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADFREEF;
		    break;
		}
		/* write the file header for root directory */
		fin->ramrp.rptrs = &fin->tmprp;
		indxp = fin->index+(FSYS_INDEX_ROOT*FSYS_MAX_ALTS);
		fin->q.buff = (U8*)&fin->rhdr;
		fin->q.sector = indxp[fin->substate];
		fin->q.u_len = 512;
		fin->state = INIT_WRITE_ROOT;
		fsys_qwrite(ioq);
		return;

	    case INIT_WRITE_ROOT:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADROOT;
		    break;
		}
		/* write the root directory file */
		fin->ramrp.rptrs = fin->rhdr.pointers[fin->substate];
		fin->q.buff = (U8*)fin->dir;
		fin->q.sector = 0;
		fin->q.u_len = iv->root_sectors*512;
		fin->state = INIT_NEXT_ALT;
		fsys_qwrite(ioq);
		return;

	    case INIT_NEXT_ALT:
		if (QIO_ERR_CODE(ioq->iostatus)) {
		    fin->status = FSYS_INITFS_BADROOTF;
		    break;
		}
		fin->state = INIT_WRITE_HOME;
		++fin->substate;
		if (fin->substate < FSYS_MAX_ALTS) continue;
		sts = FSYS_INITFS_SUCC|SEVERITY_INFO;	/* normal success */
		break;

	    default:
		sts = FSYS_INITFS_FATAL;
		break;
	}			/* -- switch state */
	break;
    }				/* -- while (1) */
    QIOfree(fin->fake_free);
    QIOfree(fin->index);
    QIOfree(fin->dir);
    QIOfree(fin->hb);
    fin->status = sts;
    fin->state = 0;
    ioq = fin->q.callers_ioq;
    ioq->iostatus = sts;
    qio_complete(ioq);
    return;
}

static int fsys_rmdir( QioIOQ *ioq, const char *name ) {
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

/* rename file named 'name1' to 'name2' */

static void rename_q(QioIOQ *ioq) {
    FsysLookUpFileT src, dst;
    FsysVolume *vol;
    int sts;
    const char *fname;

    vol = (FsysVolume *)ioq->private;
    dst.vol = vol;
    dst.top = 0;
    dst.path = (const char *)ioq->pparam1;
    do {
	FsysDirEnt *dp;
	char *s;
	sts = lookup_filename(&dst);		/* make sure dst is legal */
	if (sts != FSYS_LOOKUP_NOPATH) {
	    if (sts == (FSYS_LOOKUP_SUCC|SEVERITY_INFO)) {
		ioq->iostatus = FSYS_CREATE_NAMEINUSE;	/* name is already in use */
	    } else {
		ioq->iostatus = FSYS_CREATE_NOPATH;	/* no path */
	    }
	    break;
	}
	src.vol = vol;
	src.top = 0;
	src.path = ioq->pparam0;
	sts = lookup_filename(&src);
	if ( sts != (FSYS_LOOKUP_SUCC|SEVERITY_INFO)) {
	    ioq->iostatus = sts;		/* no source */
	    break;
	}
	fname = strrchr((char *)ioq->pparam1, QIO_FNAME_SEPARATOR); /* get name of dest */
	if (!fname) fname = (const char *)ioq->pparam1;
	dp = (FsysDirEnt *)QMOUNT_ALLOC(vol, sizeof(FsysDirEnt)+strlen(fname)+1);
	s = (char *)(dp+1);
	strcpy(s, fname);
	dp->name = s;
	dp->generation = src.dir->generation;
	dp->fid = src.dir->fid;
	dp->next = 0;
/* !!Memory leak here!!!*/
	remove_ent(src.owner->directory, src.fname);
	insert_ent(dst.owner->directory, dp);
	add_to_dirty(vol, src.owner - vol->files, 0); /* put old directory on the dirty list */
	add_to_dirty(vol, dst.owner - vol->files, 0); /* put new directory on the dirty list */
    } while (0);
    ioq->private = 0;
    qio_freefile(qio_fd2file(ioq->file));
    ioq->file = -1;
    qio_freemutex(&vol->mutex, ioq);
    qio_complete(ioq);
    return;
}

static int fsys_rename( QioIOQ *ioq, const char *source, const char *dest ) {
    FsysVolume *vol;
    QioFile *file;

    if ( !source || *source == 0 ) return (ioq->iostatus = QIO_INVARG);
    if ( !dest || *dest == 0 ) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);		/* get pointer to file */
    vol = (FsysVolume *)file->dvc->private;	/* point to our mounted volume */
    if (!vol) return (ioq->iostatus = FSYS_LOOKUP_FNF);
    if (!vol->files) (ioq->iostatus = FSYS_LOOKUP_FNF);
    ioq->pparam0 = (void *)source;
    ioq->pparam1 = (void *)dest;
    ioq->private = (void *)vol;
    ioq->iostatus = 0;
    return qio_getmutex(&vol->mutex, rename_q, ioq);
}

#endif			/* !FSYS_READ_ONLY */

static void lseek_q(QioIOQ *ioq) {
    QioFile *fp;
    off_t where;
    int whence;

    fp = qio_fd2file(ioq->file);
    where = ioq->iparam0;
    whence = ioq->iparam1;
    switch (whence) {
	case SEEK_SET:
	    fp->pos = where;
	    break;

	case SEEK_END:
	    fp->pos = fp->size + where;
	    break;

	case SEEK_CUR:
	    fp->pos += where;
	    break;
    }
    if (fp->pos > fp->size) fp->pos = fp->size;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    ioq->iocount = fp->pos;
    qio_complete(ioq);
    qio_freemutex(&fp->mutex, ioq);
    return;
}

/* fsys_lseek - is called directly as a result of the user calling qio_lseek()
 * To protect the pos field of the file, this function has to be serialized
 * with other calls to lseek and read/write. This is done by loading the 
 * parameters into the IOQ, waiting on the mutex for the file then calling
 * lseek_q().
 */

static int fsys_lseek( QioIOQ *ioq, off_t where, int whence ) {
    QioFile *fp;
    int sts;

    if (!ioq) return QIO_INVARG;

    ioq->iparam0 = where;
    ioq->iparam1 = whence;
    fp = qio_fd2file(ioq->file);
    sts = qio_getmutex(&fp->mutex, lseek_q, ioq);
    return sts;
}

static void fsys_read_done( QioIOQ *ioq ) {
    FsysQio *q;
    QioIOQ *hisioq;
    QioFile *fp;
    FsysVolume *v;
    FsysRamFH *rfh;
    int fid;

    q = (FsysQio *)ioq;
    hisioq = q->callers_ioq;
    hisioq->iocount += q->total;		/* record total bytes xferred so far */
    fp = qio_fd2file(hisioq->file);
    v = (FsysVolume *)fp->dvc->private;
    fid = (int)fp->private;
    rfh = v->files + (fid&0x00FFFFFF);
    if (QIO_ERR_CODE(ioq->iostatus) && ioq->iostatus != QIO_EOF && !(fp->mode&FSYS_OPNCPY_M)) {
	++q->o_which;
	if (q->o_which < FSYS_MAX_ALTS) {	/* if we haven't tried all reads */
	    FsysRamRP *rp;
	    rp = rfh->ramrp + q->o_which;	/* get next retrieval pointer set */
	    if (rp->num_rptrs) {		/* if there are alternate retrevial pointers */
		USED_ALT();
		rfh->active_rp = q->o_which;
		q->o_where = q->sector = q->o_where + q->total/512;
		q->o_len = q->u_len = q->o_len - q->total;
		q->o_buff = q->buff = (void *)((U32)q->o_buff + q->total);
		q->ramrp = rp;			/* point to new retreival pointers */
		q->state = 0;			/* make sure we start at beginning */
		q->total = 0;			/* start the read over at 0 */
		fsys_qread(ioq);		/* and re start the read */
		return;
	    }
	}
    }
    hisioq->iostatus = ioq->iostatus;	/* record last status */
    fp->pos = q->o_where*512 + q->total; /* advance the files' sector pointer */
    v = (FsysVolume *)fp->dvc->private;
    fsys_freeqio(q);
    qio_complete(hisioq);
    qio_freemutex(&fp->mutex, hisioq);
    return;
}

static void readwpos_q(QioIOQ *ioq) {
    FsysQio *q;
    QioFile *fp;
    FsysRamFH *rfh;
    off_t where;

    q = (FsysQio *)ioq->pparam0;
    rfh = (FsysRamFH *)ioq->pparam1;
    fp = q->fsys_fp;

    if (!(fp->mode&O_CREAT) && (fp->mode&FSYS_OPNCPY_M)) { /* if reading a specific file */
	int t;
	t = (((U32)fp->mode&FSYS_OPNCPY_M)>>FSYS_OPNCPY_V) - 1;
	q->ramrp = rfh->ramrp + t;
    } else {
	q->ramrp = rfh->ramrp + rfh->active_rp;
    }
    q->o_which = rfh->active_rp;

    where = ioq->iparam0;
    if (where + q->o_len > rfh->size) {
	if (where >= rfh->size) {
	    fp->pos = rfh->size;		/* set position to end of file */
	    ioq->iostatus = QIO_EOF;		/* signal end of file */
	    ioq->iocount = 0;			/* no data xferred */
	    fsys_freeqio(q);			/* done with this */
	    qio_complete(ioq);			/* done */
	    qio_freemutex(&fp->mutex, ioq);
	    return;
	}
	q->o_len = q->u_len = rfh->size - where; /* max out the length */
    }
    fsys_qread(&q->our_ioq);
    return;
}

static int validate_read( QioIOQ *ioq, void *buf, long len) {
    FsysQio *q;
    QioFile *fp;
    FsysVolume *v;
    unsigned int fid;
    FsysRamFH *rfh;

    if (!ioq || len < 0) return QIO_INVARG;
    fp = qio_fd2file(ioq->file);
    v = (FsysVolume *)fp->dvc->private;
    fid = (int)fp->private;
    if ((fid&0x00FFFFFF) >= v->files_ffree) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = v->files + (fid&0x00FFFFFF);
    if (!rfh->valid) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>24)) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    q = fsys_getqio();
    if (!q) return (ioq->iostatus = FSYS_IO_NOQIO);
    q->our_ioq.file = v->iofd;			/* physical I/O to this channel */
    q->callers_ioq = ioq;
    q->o_buff = q->buff = buf;
    q->o_len = q->u_len = len;
    q->compl = fsys_read_done;
    q->fsys_fp = fp;
    ioq->pparam0 = (void *)q;
    ioq->pparam1 = (void *)rfh;
    return 0;
}

/* fsys_readwpos - is called directly as a result of the user calling qio_readwpos()
 * To protect the pos field of the file, this function has to be serialized
 * with other calls to read/write/lseek before using or updating anything in the
 * file struct. This is done by loading the caller's parameters into the IOQ and
 * waiting on the mutex for the file then calling readwpos_q().
 */

static int fsys_readwpos( QioIOQ *ioq, off_t where, void *buf, long len ) {
    FsysQio *q;
    int sts;

    sts = validate_read( ioq, buf, len );
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    ioq->iparam0 = where;
    q->o_where = q->sector = where/512;
    sts = qio_getmutex(&q->fsys_fp->mutex, readwpos_q, ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

static void read_q( QioIOQ *ioq ) {
    FsysQio *q;
    off_t where;

    q = (FsysQio *)ioq->pparam0;
    where = q->fsys_fp->pos;
    q->o_where = q->sector = where/512;
    ioq->iparam0 = where;
    readwpos_q(ioq);
    return;
}
    
/* fsys_read - is called directly as a result of the user calling qio_read()
 * To protect the pos field of the file, this function has to be serialized
 * with other calls to read/write/lseek before using or updating anything in the
 * QioFile struct. This is done by loading the caller's parameters into the IOQ and
 * waiting on the mutex for the file then calling read_q().
 */

static int fsys_read( QioIOQ *ioq, void *buf, long len ) {
    FsysQio *q;
    int sts;

    sts = validate_read( ioq, buf, len );
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    sts = qio_getmutex(&q->fsys_fp->mutex, read_q, ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

static void fsys_write_done( QioIOQ *ioq ) {
    FsysQio *q;
    QioIOQ *hisioq;
    QioFile *fp;
    FsysVolume *v;
    unsigned int fid;
    FsysRamFH *rfh;
    int lim;

    q = (FsysQio *)ioq;
    if (QIO_ERR_CODE(ioq->iostatus)) q->o_iostatus = ioq->iostatus;
    fp = q->fsys_fp;
    v = (FsysVolume *)fp->dvc->private;
    hisioq = q->callers_ioq;
    fp = qio_fd2file(hisioq->file);
    fid = (int)fp->private;
    rfh = v->files + (fid&0x00FFFFFF);
    if (!(fp->mode&FSYS_OPNCPY_M)) {
	++q->o_which;
	if (q->o_which < FSYS_MAX_ALTS) {	/* if we haven't tried all writes */
	    FsysRamRP *rp;
	    rp = rfh->ramrp + q->o_which;	/* get next retrieval pointer set */
	    if (rp->num_rptrs) {		/* if there are alternate retrevial pointers */
		q->ramrp = rp;		/* point to new batch */
		q->sector = q->o_where;	/* rewind the significant pointers */
		q->u_len = q->o_len;
		q->buff = (void *)q->o_buff;
		q->total = 0;		/* reset the total */
		q->state = 0;		/* make sure we start at beginning */
		fsys_qwrite(ioq);		/* and re start the write */
		return;
	    }
	}
    }
    lim = q->total;			/* total number of bytes xferred */
    if (lim > q->o_len) lim = q->o_len;	/* limit it to the number the user asked */
    hisioq->iocount = lim;		/* record total bytes xferred */
    if (q->o_iostatus) {
	hisioq->iostatus = q->o_iostatus;	/* record last error status */
    } else {
	hisioq->iostatus = ioq->iostatus;
    }
    fp->pos = q->o_where*512 + lim;	/* advance the files' sector pointer */
    if (rfh->size < fp->pos) {
	rfh->size = fp->pos;
	rfh->valid |= RAMFH_UPD;	/* update FH at close */
    }
    fsys_freeqio(q);
    qio_complete(hisioq);
    qio_freemutex(&fp->mutex, &q->our_ioq);
    return;
}

static int validate_write( QioIOQ *ioq, const void *buf, long len ) {
    FsysQio *q;
    QioFile *fp;
    FsysVolume *v;
    unsigned int fid;
    FsysRamFH *rfh;

    if (!ioq) return QIO_INVARG;
    fp = qio_fd2file(ioq->file);
    v = (FsysVolume *)fp->dvc->private;
    fid = (int)fp->private;
    if ((fid&0x00FFFFFF) >= v->files_ffree) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = v->files + (fid&0x00FFFFFF);
    if (!rfh->valid) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>24)) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    q = fsys_getqio();
    if (!q) return (ioq->iostatus = FSYS_IO_NOQIO);

    q->o_which = 0;
    q->o_len = q->u_len = len;
    q->o_buff = q->buff = (void *)buf;
    q->o_iostatus = 0;				/* assume no errors */
    q->our_ioq.file = v->iofd;			/* physical I/O to this channel */
    q->callers_ioq = ioq;
    q->ramrp = rfh->ramrp;
    q->compl = fsys_write_done;
    q->fsys_fp = fp;
    ioq->pparam0 = (void *)q;
    return 0;
}

/* fsys_writewpos - is called directly as a result of the user calling qio_writewpos()
 * To protect the pos field of the file, this function has to be serialized
 * with other calls to read/write/lseek before using or updating anything in the
 * file struct. This is done by loading the caller's parameters into the IOQ,
 * waiting on the mutex for the file then calling writewpos_q().
 */

static int fsys_writewpos( QioIOQ *ioq, off_t where, const void *buf, long len ) {
    FsysQio *q;
    int sts;

    sts = validate_write(ioq, buf, len);
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    q->o_where = q->sector = where/512;
    sts = qio_getmutex(&q->fsys_fp->mutex, fsys_qwrite, &q->our_ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

static void write_q( QioIOQ *ioq ) {
    FsysQio *q;
    off_t where;

    q = (FsysQio *)ioq;
    where = q->fsys_fp->pos;
    q->o_where = q->sector = where/512;
    fsys_qwrite(&q->our_ioq);
    return;
}
    
static int fsys_write( QioIOQ *ioq, const void *buf, long len ) {
    int sts;
    FsysQio *q;

    sts = validate_write( ioq, buf, len);
    if (sts) return sts;
    q = (FsysQio *)ioq->pparam0;
    sts = qio_getmutex(&q->fsys_fp->mutex, write_q, &q->our_ioq);
    if (sts) fsys_freeqio(q);
    return sts;
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
static void ioctl_setfh( QioIOQ *ioq ) {
    QioFile *fp;
    FsysVolume *vol;
    FsysFHIoctl *ctl;
    FsysRamFH *rfh;
    int fid, afid;
# if !FSYS_READ_ONLY
    FsysRamRP *rp;
    int ii, jj, copies;
# endif

    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
    fid = (int)fp->private;
    afid = fid & 0x00FFFFFF;
    rfh = vol->files + afid;
    ctl = (FsysFHIoctl *)ioq->pparam0;
    if ((ctl->fields&FSYS_FHFIELDS_CTIME)) rfh->ctime = ctl->ctime;
    if ((ctl->fields&FSYS_FHFIELDS_MTIME)) rfh->mtime = ctl->mtime;
# if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    if ((ctl->fields&FSYS_FHFIELDS_ATIME)) rfh->atime = ctl->atime;
    if ((ctl->fields&FSYS_FHFIELDS_BTIME)) rfh->btime = ctl->btime;
# endif    
# if !FSYS_READ_ONLY
    for (ii=copies=0; ii < FSYS_MAX_ALTS; ++ii) {	/* compute how many copies */
	rp = rfh->ramrp + ii;
	if (rp->num_rptrs) ++copies;
    }
    if ((ctl->fields&FSYS_FHFIELDS_COPIES)) {
	for (; copies < ctl->copies; ++copies) {
	    rp = rfh->ramrp + copies;
	    if (rp->num_rptrs) continue;		/* already have a copy here */
	    ii = extend_file(vol, rfh->clusters, rp, copies);
	    if (ii) {
		ioq->iostatus = ii;
		goto ioctl_done;
	    }
	}
    }
    if ((ctl->fields&FSYS_FHFIELDS_ALLOC)) {
	int def;
	def = (ctl->alloc+511)/512;
	if (def > rfh->clusters) {
	    def -= rfh->clusters;
	    if (def*copies >= vol->total_free_clusters) {
		ioq->iostatus = FSYS_EXTEND_FULL;	/* no room on disk */
		goto ioctl_done;
	    }
	    for (jj=0; jj < copies; ++jj) {
		rp = rfh->ramrp + jj;
		if (!rp->num_rptrs) continue;		/* no copy here */
		ii = extend_file(vol, def, rp, jj);
		if (ii) {
		    ioq->iostatus = ii;
		    break;				/* ran out of room */
		}
	    }
	    rfh->clusters += def;			/* bump allocation record */
	}
    }	    
    if ((ctl->fields&FSYS_FHFIELDS_DEFEXT)) {
	rfh->def_extend = (ctl->def_extend+511)/512;
    }
# endif
    if ((ctl->fields&FSYS_FHFIELDS_SIZE)) {
	if (ctl->size > rfh->clusters*512) {
	    rfh->size = fp->size = rfh->clusters*512;
	} else {
	    rfh->size = fp->size = ctl->size;
	}
    }
    add_to_dirty(vol, afid, 0);				/* record changed FH */
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
# if !FSYS_READ_ONLY
ioctl_done:
# endif
    qio_freemutex(&vol->mutex, ioq);
    qio_complete(ioq);
}
#endif

static int fsys_ioctl( QioIOQ *ioq, unsigned int cmd, void *arg ) {
    int sts, ii, jj;
    QioFile *fp;
    FsysFHIoctl *ctl;
    FsysVolume *vol;

    if (!ioq) return QIO_INVARG;
    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
    switch (cmd) {
	default: sts = FSYS_IO_NOSUPP;
	    break;
	case FSYS_IOC_GETFH: {
	    int fid, afid;
	    FsysRamFH *rfh;
	    FsysRamRP *rp;

	    if (!(ctl = (FsysFHIoctl *)arg)) {
		sts = QIO_INVARG;
		break;
	    }
	    fid = (int)fp->private;
	    afid = fid & 0x00FFFFFF;
	    rfh = vol->files + afid;
	    ctl->fields = FSYS_FHFIELDS_CTIME|FSYS_FHFIELDS_MTIME|FSYS_FHFIELDS_ALLOC
	    		  |FSYS_FHFIELDS_DEFEXT|FSYS_FHFIELDS_SIZE|FSYS_FHFIELDS_COPIES
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
	    	          |FSYS_FHFIELDS_ATIME|FSYS_FHFIELDS_BTIME
#endif
	    		  ;
	    ctl->ctime = rfh->ctime;
	    ctl->mtime = rfh->mtime;
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
	    ctl->atime = rfh->atime;
	    ctl->btime = rfh->btime;
#else
	    ctl->atime = ctl->btime = 0;
#endif
	    ctl->alloc = rfh->clusters*512;
	    ctl->def_extend = rfh->def_extend*512;
	    ctl->size = fp->size;
	    ctl->pos = fp->pos;
	    ctl->fid = fid;
	    for (ii=jj=0; ii < FSYS_MAX_ALTS; ++ii) {	/* compute how many copies */
		rp = rfh->ramrp + ii;
		if (rp->num_rptrs) ++jj;
	    }
	    ctl->copies = jj;
	    ctl->dir = rfh->directory != 0;
	    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
	    qio_complete(ioq);
	    return 0;
	}
	case FSYS_IOC_SETFH:
	    if (!(ctl = (FsysFHIoctl *)arg)) {
		sts = QIO_INVARG;
		break;
	    }
#if !FSYS_READ_ONLY || FSYS_UPD_FH
# if FSYS_READ_ONLY && FSYS_UPD_FH
	    if ((ctl->fields&(FSYS_FHFIELDS_ALLOC|FSYS_FHFIELDS_COPIES|FSYS_FHFIELDS_DEFEXT))) {
		sts = FSYS_IO_NOSUPP;
		break;
	    }
# else
	    if ((ctl->fields&FSYS_FHFIELDS_COPIES) && ctl->copies > FSYS_MAX_ALTS) {
		sts = QIO_INVARG;
		break;
	    }
# endif
	    ioq->iparam0 = cmd;
	    ioq->pparam0 = arg;
	    sts = qio_getmutex(&vol->mutex, ioctl_setfh, ioq);
	    if (sts) break;
	    return 0;
#else
	    sts = FSYS_IO_NOSUPP;
	    break;
#endif
    }
    return (ioq->iostatus = sts);
}

#if !FSYS_READ_ONLY || FSYS_UPD_FH
static void fsys_closeq(QioIOQ *ioq) {
    QioFile *file;
    int fid, afid;
    FsysRamFH *rfh;
    FsysVolume *vol;

    file = qio_fd2file(ioq->file);
    vol = (FsysVolume *)file->dvc->private;
    fid = (int)file->private;
    afid = fid & 0x00FFFFFF;
    rfh = vol->files + afid;
# if !FSYS_READ_ONLY || FSYS_UPD_FH
    if ((rfh->valid&RAMFH_UPD)) {
	add_to_dirty(vol, afid, 0);
	rfh->valid &= ~RAMFH_UPD;	/* not dirty anymore */
    }
# endif
    file->private = 0;
    file->dvc = 0;
    qio_freefile(file);		/* put file back on free list */
    ioq->file = -1;		/* not open anymore */
    qio_freemutex(&vol->mutex, ioq);
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return;
}
#endif

static int fsys_close( QioIOQ *ioq) {
    QioFile *file;
    FsysVolume *vol;
    int fid, afid;
    FsysRamFH *rfh;

    if (!ioq) return QIO_INVARG;
    file = qio_fd2file(ioq->file);
    if (!file) return (ioq->iostatus = QIO_NOTOPEN);

    vol = (FsysVolume *)file->dvc->private;
    if (!vol) return (ioq->iostatus = FSYS_OPEN_NOTOPEN);
    fid = (int)file->private;
    afid = fid & 0x00FFFFFF;
    if (afid >= vol->files_ffree) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = vol->files + afid;
    if (!rfh->valid) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>24)) return (ioq->iostatus = FSYS_IO_NOTOPEN);
#if !FSYS_READ_ONLY || FSYS_UPD_FH
    return qio_getmutex(&vol->mutex, fsys_closeq, ioq);
#else
    file->private = 0;
    file->dvc = 0;
    qio_freefile(file);		/* put file back on free list */
    ioq->file = -1;		/* not open anymore */
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
#endif
}

static int fsys_trunc( QioIOQ *ioq ) {
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

static int fsys_statfs( QioIOQ *ioq, const char *name, void *fsarg ) {
    struct statfs *sfs;
    sfs = (struct statfs *)fsarg;
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

static int fsys_fstat( QioIOQ *ioq, struct stat *st ) {
    QioFile *file;
    FsysVolume *vol;
    FsysRamFH *rfh;
    int fid, afid;

    file = qio_fd2file(ioq->file);		/* get pointer to file */
    if (!file) return (ioq->iostatus = FSYS_OPEN_NOTOPEN);
    vol = (FsysVolume *)file->dvc->private;
    if (!vol) return (ioq->iostatus = FSYS_OPEN_NOTOPEN);
    fid = (int)file->private;
    afid = fid & 0x00FFFFFF;
    if (afid >= vol->files_ffree) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    rfh = vol->files + afid;
    if (!rfh->valid) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    if (rfh->generation != (fid>>24)) return (ioq->iostatus = FSYS_IO_NOTOPEN);
    st->st_mode = rfh->directory ? S_IFDIR : S_IFREG;
    st->st_size = file->size;
#if !WATCOM_LIBC
    st->st_blksize = 512;
    st->st_blocks = (file->size+511)/512;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_CMTIME)
    st->st_ctime = rfh->ctime;
    st->st_mtime = rfh->mtime;
#endif
#if (FSYS_FEATURES&FSYS_OPTIONS&FSYS_FEATURES_ABTIME)
    st->st_atime = rfh->atime;
#endif
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_cancel( QioIOQ *ioq ) {
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

static int fsys_isatty( QioIOQ *ioq ) {
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

static DIR *dir_pool_head;
static int num_dirs;

/************************************************************
 * fsys_getDIR - Get a DIR from the system's pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
static DIR *fsys_getDIR(void) {
    ++num_dirs;
    return (DIR*)qio_getioq_ptr((QioIOQ **)&dir_pool_head, sizeof(DIR));
}

/************************************************************
 * fsys_freeDIR - Free a DIR as obtained from a previous
 * call to fsys_getDIR().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	0 if success or 1 if queue didn't belong to pool.
 */
static int fsys_freeDIR(DIR *que) {
    --num_dirs;
    return qio_freeioq_ptr((QioIOQ *)que, (QioIOQ **)&dir_pool_head);
}

static int fsys_opendir( QioIOQ *ioq, void **dirp, const char *name ) {
    FsysVolume *vol;
    const QioDevice *dvc;
    DIR *dp;
    int sts;
    FsysLookUpFileT luf;

    dvc = (const QioDevice *)ioq->private;
    vol = (FsysVolume *)dvc->private;		/* point to our mounted volume */
    if (!vol || !vol->files) return (ioq->iostatus = FSYS_OPEN_NOTMNT);
    dp = fsys_getDIR();
    if (!dp) return (ioq->iostatus = FSYS_IO_NODIRS);
    luf.vol = vol;
    luf.top = 0;
    luf.path = name;
    sts = lookup_filename(&luf);
    if (QIO_ERR_CODE(sts)) {
	fsys_freeDIR(dp);
	return (ioq->iostatus = sts);
    }
    if (!(dp->hash=luf.file->directory)) {
	fsys_freeDIR(dp);
	return (ioq->iostatus = FSYS_LOOKUP_NOTDIR);
    }
    dp->vol = vol;
    *dirp = dp;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_readdir( QioIOQ *ioq, void *dirp, void *directp ) {
    DIR *dp;
    struct direct *direct;

    dp = (DIR *)dirp;
    direct = (struct direct *)directp;
    if (dp->current) dp->current = dp->current->next;
    if (!dp->current) {
	for (;dp->entry < FSYS_DIR_HASH_SIZE; ++dp->entry) {
	    if ((dp->current = dp->hash[dp->entry])) break;
	}
	++dp->entry;			/* this always needs bumping, even if we found one */
    }
    if (dp->current) {
	direct->name = dp->current->name;
	direct->fid = (dp->current->generation << 24) | dp->current->fid;
	ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    } else {
	direct->name = 0;
	direct->fid = 0;
	ioq->iostatus = QIO_EOF;
    }
    qio_complete(ioq);
    return 0;
}

static int fsys_rewdir( QioIOQ *ioq, void *dirp ) {
    DIR *dp;
    dp = (DIR *)dirp;
    dp->current = 0;
    dp->entry = 0;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_closedir( QioIOQ *ioq, void *dirp ) {
    DIR *dp;
    dp = (DIR *)dirp;
    fsys_freeDIR(dp);
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int fsys_seekdir( QioIOQ *ioq, void *dirp ) {
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

static int fsys_telldir( QioIOQ *ioq, void *dirp ) {
    if (!ioq) return QIO_INVARG;
    return (ioq->iostatus = FSYS_IO_NOSUPP);
}

static const QioFileOps fsys_fops = {
    fsys_lseek,	/* lseek to specific byte in file */
    fsys_read, 	/* Read from a file */
    fsys_write,	/* Write to a file */
    fsys_ioctl,	/* I/O control */
    fsys_open,	/* open file */
    fsys_close,	/* close previously opened file */
#if !FSYS_READ_ONLY
    fsys_delete,/* delete a file */
    fsys_fsync,	/* sync the file system */
    fsys_mkdir,	/* make a directory */
    fsys_rmdir,	/* remove a directory */
    fsys_rename,/* rename a directory */
#else
    0,		/* no delete */
# if FSYS_UPD_FH
    fsys_fsync,
# else
    0,		/* no fsync */
# endif
    0,		/* no mkdir */
    0,		/* no rmdir */
    0,		/* no rename */
#endif
    fsys_trunc,	/* truncate a file */
    fsys_statfs,/* stat a file system */
    fsys_fstat,	/* stat a file */
    fsys_cancel, /* cancel I/O */
    fsys_isatty, /* maybe a tty */
    fsys_readwpos,	/* read with combined lseek */
    fsys_writewpos,	/* write with combined lseek */
    fsys_opendir,	/* open directory */
    fsys_seekdir,	/* seek directory */
    fsys_telldir,	/* tell directory */
    fsys_rewdir,	/* rewind directory */
    fsys_readdir,	/* read directory entry */
    fsys_closedir	/* close directory */
};

static const QioDevice vol0_dvc = {
    "d0",				/* device name */
    2,					/* length of name */
    &fsys_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for null device */
    0,					/* unit 0 */
    (void *)(volumes+0)			/* volume 0 */
};

#if FSYS_MAX_VOLUMES > 1
static const QioDevice vol1_dvc = {
    "d1",				/* device name */
    2,					/* length of name */
    &fsys_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for null device */
    1,					/* unit 1 */
    (void *)(volumes+1)			/* volume 1 */
};
#endif

#if FSYS_MAX_VOLUMES > 2
static const QioDevice vol2_dvc = {
    "d2",				/* device name */
    2,					/* length of name */
    &fsys_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for null device */
    2,					/* unit 2 */
    (void *)(volumes+2)			/* volume 2 */
};
#endif

#if FSYS_MAX_VOLUMES > 3
static const QioDevice vol3_dvc = {
    "d3",				/* device name */
    2,					/* length of name */
    &fsys_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for null device */
    3,					/* unit 3 */
    (void *)(volumes+3)			/* volume 3 */
};
#endif

#if !FSYS_READ_ONLY
int fsys_initfs(const char *what, FsysInitVol *iv) {
    volatile FsysFinit fin;
    int sts;
    QioIOQ *ioq;
    QioFile *file;
    const QioDevice *dvc;

    if (prc_get_astlvl() >= 0) return FSYS_INITFS_BADLVL;	/* cannot call this at AST level */
    memset((char *)&fin, 0, sizeof(fin));
    ioq = qio_getioq();
    if (ioq) {
	sts = qio_open(ioq, (void *)what, O_RDWR);
	while (!sts) { sts = ioq->iostatus; }
	if (!QIO_ERR_CODE(sts)) {
	    struct stat drvstat;
	    file = qio_fd2file(ioq->file);
	    if (!file || !(dvc=file->dvc)) {
		sts = QIO_FATAL;
	    } else {
		fin.q.our_ioq.file = ioq->file;
		fin.iv = iv;
		sts = qio_fstat(ioq, &drvstat);
		while (!sts) { sts = ioq->iostatus; }
		if (!QIO_ERR_CODE(sts)) {
		    fin.max_lba = drvstat.st_size;
		    fin.q.callers_ioq = ioq;
		    ioq->iostatus = 0;
		    ioq->iocount = 0;
		    init_file_system((QioIOQ *)&fin);
		    while (!(sts=ioq->iostatus)) { ; }
		}
	    }
	    qio_close(ioq);
	}
	qio_freeioq(ioq);
	return sts;
    }
    return QIO_NOIOQ;
}
#endif

void fsys_init(void) {
    int ii;
#if TEST_DISK_TIMEOUT
    {
	int oldopt = prc_delay_options(PRC_DELAY_OPT_TEXT2FB|PRC_DELAY_OPT_SWAP|PRC_DELAY_OPT_CLEAR);
	txt_str(-1, AN_VIS_ROW/2, "Fake disk timeouts enabled", RED_PAL);
	prc_delay(4*60);
	prc_delay_options(oldopt);
    }
#endif
    for (ii=0; ii < n_elts(volumes); ++ii) volumes[ii].id = FSYS_ID_VOLUME;
#if FSYS_SQUAWKING || FSYS_FREE_SQUAWKING || FSYS_SYNC_SQUAWKING
    if (!fsys_iop) fsys_iop = iio_open(1);	/* open a connection to thread 1 */
#endif
    qio_install_dvc(&vol0_dvc);
#if FSYS_MAX_VOLUMES > 1
    qio_install_dvc(&vol1_dvc);
#endif
#if FSYS_MAX_VOLUMES > 2
    qio_install_dvc(&vol2_dvc);
#endif
#if FSYS_MAX_VOLUMES > 3
    qio_install_dvc(&vol3_dvc);
#endif
    return;
}

#if !NO_FSYS_TEST
#if 1
# ifndef LED_ON
#  define LED_ON(x)	do { *(VU32*)LED_OUT &= ~(1<<B_LED_##x); } while (0)
# endif
# ifndef LED_OFF
#  define LED_OFF(x)	do { *(VU32*)LED_OUT |= (1<<B_LED_##x); } while (0)
# endif
#else
# ifndef LED_ON
#  define LED_ON(x)	do { ; } while (0)
# endif
# ifndef LED_OFF
#  define LED_OFF(x)	do { ; } while (0)
# endif
#endif

#if 0
#include <glide.h>
static void (*oldvb)(void);
static struct tq vb_q;

static void fsys_vb_qr(void *p) {
    U32 cookie;

    cookie = txt_setpos(0);
    txt_hexnum(-1, 3, ctl_read_sw(0), 8, RJ_ZF, WHT_PAL);
    txt_hexnum(-1, 4, ctl_read_debug(0), 8, RJ_ZF, WHT_PAL);
    txt_setpos(cookie);
    sst_text2fb(0);
    sst_bufswap();
    grBufferClear( 0, 0, GR_WDEPTHVALUE_FARTHEST );
    return;
}

static void fsys_vb_t(void *p) {
    vb_q.func = fsys_vb_qr;
#if 1
    prc_q_ast(QIO_ASTLVL, (struct act_q *)&vb_q );
#else
    prc_q_action((struct act_q *)&vb_q );
#endif
}

static void fsys_vb(void) {
    oldvb();
    if (!vb_q.que) {
	vb_q.func = fsys_vb_t;
	vb_q.vars = 0;
	vb_q.delta = 5000;
	tq_ins(&vb_q);
    }
    return;
}
#endif

#if 0 && MALLOC_DEBUG
static int alloc_amt;

static void *get_mem(int amt) {
    alloc_amt += amt;
    return QIOmalloc(amt);
}

static void free_mem(int amt, void *ptr) {
    alloc_amt -= amt ;
    QIOfree(ptr);
    return;
}

#define GET_MEM(x) get_mem(x)
#define FREE_MEM(a, x) free_mem(a, x)
#else
#define GET_MEM(x) QIOmalloc(x)
#define FREE_MEM(a, x) QIOfree(x)
#endif

typedef struct chksums {
    U32 fid;
    U32 chksum;
} ChkSum;

typedef struct dir_walker {
    DIR *dp;
    QioIOQ *ioq;
    char *data_buff;
    int data_buff_size;
    int errors;
    int cs_errors;
    int cs;
    int file_size;
    ChkSum *cksums;
    ChkSum *this_cs;
    int numb_cksums;
    int banners;
    int correctables;
    int used_alts;
} Walker;

#define WALK_EXIT_OK	0	/* walk completed with success */
#define WALK_EXIT_ABORT	1	/* operator requests abort */
#define WALK_EXIT_ERR	2	/* walk completed with error */

#define WALK_ANN_ROW	(AN_VIS_ROW/3)
#define WALK_ERR_ROW	(AN_VIS_ROW/3+4)
#define WALK_COL	(-1)

#define WALK_CKSUM_ROW	(WALK_ERR_ROW+7)
#define WALK_CKSUM_MSG	"File checksum errors:          "
#define WALK_CKSUM_COL	((AN_VIS_COL-sizeof(WALK_CKSUM_MSG)-1-4)/2)

#define WALK_UCORR_ROW	(WALK_ERR_ROW+8)
#define WALK_UCORR_MSG	"Uncorrectable disk errors:     "
#define WALK_UCORR_COL	((AN_VIS_COL-sizeof(WALK_UCORR_MSG)-1-4)/2)

#define WALK_CORR_ROW	(WALK_ERR_ROW+9)
#define WALK_CORR_MSG	"Correctable disk read errors:  "
#define WALK_CORR_COL	((AN_VIS_COL-sizeof(WALK_CORR_MSG)-1-4)/2)

#define WALK_ALTS_ROW	(WALK_ERR_ROW+10)
#define WALK_ALTS_MSG	"Alternates used:               "
#define WALK_ALTS_COL	((AN_VIS_COL-sizeof(WALK_ALTS_MSG)-1-4)/2)

static void find_cs(int fid, Walker *wp) {
    int ii;
    ChkSum *csp;
    csp = wp->cksums;
    wp->this_cs = 0;
    if (csp) for (ii=0; ii < wp->numb_cksums; ++ii, ++csp) {
	if (csp->fid == fid) {
	    wp->this_cs = csp;
	    break;
	}
    }
    return;
}

#ifndef QUIET_CHECK
#define QUIET_CHECK	1	/* don't display filenames */
#endif

static int read_check(QioIOQ *ioq, Walker *wp) {
    int sts, cs;
    U32 *up;
#ifdef EER_FSYS_USEALT
    int used_alts;
#endif
#ifdef EER_DSK_CORR
    int corrs;
#endif
    cs = 0;
    wp->file_size = 0;
    while (1) {
	if ((ctl_read_sw(0)&SW_NEXT)) return WALK_EXIT_ABORT;
#ifdef EER_FSYS_USEALT
	used_alts = eer_gets(EER_FSYS_USEALT);
#endif
#ifdef EER_DSK_CORR
	corrs = eer_gets(EER_DSK_CORR);
#endif
#if TEST_DISK_TIMEOUT
	if ((ctl_read_sw(SW_ACTION)&SW_ACTION)) {
	    ioq->timeout = 1000;		/* short timeout */
	} else {
	    ioq->timeout = 0;
	}
#endif
	sts = qio_read(ioq, wp->data_buff, wp->data_buff_size);
	while (!sts) { sts = ioq->iostatus; }
#ifdef EER_FSYS_USEALT
	if (used_alts != eer_gets(EER_FSYS_USEALT)) ++wp->used_alts;
#endif
#ifdef EER_DSK_CORR
	if (corrs != eer_gets(EER_DSK_CORR)) wp->correctables += eer_gets(EER_DSK_CORR)-corrs;
#endif
	if (QIO_ERR_CODE(sts)) {
	    if (sts == QIO_EOF) {
		wp->cs = cs;
		return 0;
	    }
	    qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
	    txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
	    break;
	}
	wp->file_size += ioq->iocount;
	up = (U32*)wp->data_buff;
	if (wp->this_cs) {
	    if ((ioq->iocount&3)) {
		char *scp;
		scp = wp->data_buff + ioq->iocount;
		switch (ioq->iocount&3) {
		    case 1: *scp++ = 0;
		    case 2: *scp++ = 0;
		    case 3: *scp++ = 0;
		}
	    }
	    for (sts=0; sts < ioq->iocount; sts += 4) {
		cs += *up++;
	    }
	}
    }	
    ++wp->errors;
    return WALK_EXIT_ERR;
}

#if !QUIET_CHECK
static int walk_directory(Walker *wp, const char *dir_name) {
    int sts, dnlen, memamt;
    struct direct dir;
    char *new_name, *cp;
    QioIOQ *ioq, *nioq;
    FsysOpenT ot;

    ioq = wp->ioq;
    dnlen = strlen(dir_name);
    
    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    while (1) {
	U32 pos_cookie;
	if (ctl_read_sw(SW_NEXT)&SW_NEXT) {
	    sts = WALK_EXIT_ABORT;
	    break;
	}
	sts = qio_readdir(ioq, wp->dp, &dir);
	while (!sts) { sts = ioq->iostatus; }
	if (QIO_ERR_CODE(sts)) {
	    if (sts == QIO_EOF) {
		sts = 0;
	    } else {
		qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
		txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
		++wp->errors;
	    }
	    break;
	}
	if (strcmp(dir.name, ".") == 0 || strcmp(dir.name, "..") == 0) continue;
	sts = strlen(dir.name);
	memamt = sts+dnlen+2;
	new_name = GET_MEM(memamt);
	strcpy(new_name, dir_name);	/* start with old name */
	cp = new_name + strlen(new_name);
	*cp++ = QIO_FNAME_SEPARATOR;
	strcpy(cp, dir.name);		/* filename */
	txt_str(4, WALK_ANN_ROW, "Checking:", WHT_PAL);
	txt_clr_wid(4, WALK_ANN_ROW+2, AN_VIS_COL-6);
	if (sts+dnlen+1 > AN_VIS_COL-2-4-5) {
	    txt_str(4, WALK_ANN_ROW+2, "... ", WHT_PAL);
	    txt_cstr(new_name + sts+dnlen+1 - (AN_VIS_COL-6), GRN_PAL);
	} else {
	    txt_str(4, WALK_ANN_ROW+2, new_name, GRN_PAL);
	}
	pos_cookie = txt_setpos(0);
	prc_delay(0);
	nioq = qio_getioq();
	if (!nioq) {
	    txt_str(-1, WALK_ERR_ROW, "Ran out of IOQ's", RED_PAL);
	    ++wp->errors;
	    sts = WALK_EXIT_ERR;	/* procedure error */
	    break;
	}	    
	nioq->timeout = ioq->timeout;	/* set same as caller's timeout */
	ot.spc.path = new_name;
	ot.spc.mode = O_RDONLY;
	ot.fid = 0;
	sts = qio_openspc(nioq, &ot.spc);
	while (!sts) { sts = nioq->iostatus; }
	if (QIO_ERR_CODE(sts)) {
	    qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
	    txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
	    ++wp->errors;
	    sts = WALK_EXIT_ERR;
	} else {
	    if (ot.mkdir) {	/* if new file is a directory */
		DIR *olddp;

		qio_close(nioq);	/* don't need this resource */
		olddp = wp->dp;
		sts = qio_opendir(nioq, (void *)&wp->dp, new_name); /* open directory */
		while (!sts) { sts = nioq->iostatus; }
		if (QIO_ERR_CODE(sts)) {
		    wp->dp = olddp;
		    qio_errmsg(sts, wp->data_buff, wp->data_buff_size);
		    txt_str(-1, WALK_ERR_ROW, wp->data_buff, YEL_PAL);
		    ++wp->errors;
		    sts = WALK_EXIT_ERR;
		} else {
		    sts = walk_directory(wp, new_name);	/* then walk the directory */
		    qio_closedir(nioq, wp->dp);
		    wp->dp = olddp;
		}
	    } else {
		ChkSum *csp;
		find_cs(ot.fid, wp);		/* find checksum in checksum file */
		if (wp->this_cs) {
		    txt_setpos(pos_cookie);
		    txt_cstr(" (CS)", WHT_PAL);
		    prc_delay(0);
		}
		sts = read_check(nioq, wp);	/* read check the file */
		if (!sts && (csp=wp->this_cs)) {
		    if (csp->chksum != wp->cs || FAKE_CS_ERROR) {
#define BAD_CS_MSG1 "Checksum error on file."
#define BAD_CS_MSG2 "expected"
#define BAD_CS_MSG3 "computed"
			txt_str(-1, WALK_ERR_ROW, BAD_CS_MSG1, YEL_PAL);
			txt_str((AN_VIS_COL-3-sizeof(BAD_CS_MSG2)-1-8)/2,
    				WALK_ERR_ROW+1, BAD_CS_MSG2, YEL_PAL);
			txt_cstr(" 0x", GRN_PAL);
			txt_chexnum(csp->chksum, 8, RJ_ZF, GRN_PAL);
			txt_str((AN_VIS_COL-3-sizeof(BAD_CS_MSG3)-1-8)/2,
    				WALK_ERR_ROW+2, BAD_CS_MSG3, YEL_PAL);
			txt_cstr(" 0x", RED_PAL);
			txt_chexnum(wp->cs, 8, RJ_ZF, RED_PAL);
#if 0
txt_str(-1, WALK_ERR_ROW+3, "FID  ", WHT_PAL);
txt_chexnum(ot.fid, 8, RJ_ZF, WHT_PAL);
txt_str(-1, WALK_ERR_ROW+4, "Size ", WHT_PAL);
txt_chexnum(ot.eof, 10, RJ_ZF, WHT_PAL);
#endif
		    	sts = WALK_EXIT_ERR;
			++wp->errors;
		    }
		}
#ifdef EER_DSK_CORR
		if (wp->correctables) {
		    if (!(wp->banners&1)) {
			txt_str(WALK_CORR_COL, WALK_CORR_ROW, WALK_CORR_MSG, MNORMAL_PAL);
			wp->banners |= 1;
		    }
		    txt_decnum(WALK_CORR_COL+sizeof(WALK_CORR_MSG)-1,
			    WALK_CORR_ROW, wp->correctables, 4, RJ_BF, YEL_PAL);
		}
#endif
#ifdef EER_FSYS_USEALT
		if (wp->used_alts) {
		    if (!(wp->banners&2)) {
			txt_str(WALK_ALTS_COL, WALK_ALTS_ROW, WALK_ALTS_MSG, MNORMAL_PAL);
			wp->banners |= 2;
		    }
		    txt_decnum(WALK_ALTS_COL+sizeof(WALK_ALTS_MSG)-1,
			    WALK_ALTS_ROW, wp->used_alts, 4, RJ_BF, YEL_PAL);
		}
#endif
		qio_close(nioq);	/* done with this */
	    }
	}
	qio_freeioq(nioq);
	FREE_MEM(memamt, new_name);
	if (sts == WALK_EXIT_ABORT) break;
	if (sts == WALK_EXIT_ERR) {
	    st_insn(AN_VIS_ROW-3, "To continue,", t_msg_action, INSTR_PAL);
	    while (!(sts=ctl_read_sw(SW_ACTION|SW_NEXT)&(SW_ACTION|SW_NEXT))) { prc_delay(0); }
	    txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
	    if ((sts&SW_NEXT)) {
		sts = WALK_EXIT_ABORT;
		break;
	    }
	    txt_clr_wid(4, WALK_ERR_ROW+0, AN_VIS_COL-6);
	    txt_clr_wid(4, WALK_ERR_ROW+1, AN_VIS_COL-6);
	    txt_clr_wid(4, WALK_ERR_ROW+2, AN_VIS_COL-6);
	    txt_clr_wid(4, WALK_ERR_ROW+3, AN_VIS_COL-6);
	}
    }
    return sts;
}
#endif

#if QUIET_CHECK
static int wait_for_reply(void) {
    int sts, rtc;

    st_insn(AN_VIS_ROW-3, "To continue,", t_msg_action, INSTR_PAL);
    rtc = eer_rtc;
    while (!(sts=ctl_read_sw(SW_ACTION|SW_NEXT)&(SW_ACTION|SW_NEXT)) &&
	(eer_rtc - rtc) < 3*60) { prc_delay(0); }
    txt_clr_wid(2, AN_VIS_ROW-3, AN_VIS_COL-4);
    if ((sts&SW_NEXT)) {
	return WALK_EXIT_ABORT;
    }
    txt_clr_wid(4, WALK_ERR_ROW+0, AN_VIS_COL-6);
    txt_clr_wid(4, WALK_ERR_ROW+1, AN_VIS_COL-6);
    txt_clr_wid(4, WALK_ERR_ROW+2, AN_VIS_COL-6);
    txt_clr_wid(4, WALK_ERR_ROW+3, AN_VIS_COL-6);
    return 0;
}
#endif

#if FSYS_MAX_VOLUMES > 1
extern int ide_choose_drv(int choices);
#endif

int fsys_test(const struct menu_d *smp) {
    QioIOQ *ioq;
    int sts, row, active;
    static int been_here;
    FsysOpenT ot;
    char data_buffer[65536+QIO_CACHE_LINE_SIZE];
    char rawfn[10], volfn[10];
    Walker walk;
    char *msg;
    FsysVolume *vol;
    QioFile *fp;

#if !QUIET_CHECK
    DIR *dp;
#endif

    memset((char *)&walk, 0, sizeof(walk));
    walk.data_buff = (char *)(((int)data_buffer + QIO_CACHE_LINE_SIZE-1) & -QIO_CACHE_LINE_SIZE);
    walk.data_buff_size = 65536;
    memset(walk.data_buff, 0, walk.data_buff_size);

#if FSYS_MAX_VOLUMES > 1
    active = ide_choose_drv((1<<FSYS_MAX_VOLUMES)-1);
#else
    active = 0;
#endif
#if FSYS_MAX_VOLUMES > 9
# error You need to rework this code
#endif
    strcpy(rawfn, "/rd");
    rawfn[3] = '0' + active;
    rawfn[4] = 0;
    strcpy(volfn, "/d");
    volfn[2] = '0' + active;
    volfn[3] = 0;
    if (!(been_here&(1<<active))) {
#ifdef EER_FSYS_USEALT
	int alts;
	alts = eer_gets(EER_FSYS_USEALT);
#endif
	if (!been_here) {
	    ide_squawk(AN_VIS_ROW/2, (AN_VIS_COL-39-FSYS_MAX_VOLUMES-1)/2);
	    ide_init();
	    ide_unsquawk();
	}
#define MOUNTING_MSG "Mounting filesystem"
	if (smp) txt_str((AN_VIS_COL-sizeof(MOUNTING_MSG)-1-4-(FSYS_MAX_VOLUMES>1?2:0))/2,
			  AN_VIS_ROW/2, MOUNTING_MSG, WHT_PAL);
#if FSYS_MAX_VOLUMES > 1
	txt_cstr(" ", WHT_PAL);
	txt_cdecnum(active, 1, RJ_BF, WHT_PAL);
#endif
	txt_cstr(" ...", WHT_PAL);
	prc_delay(0);
	sts = fsys_mountw(rawfn, volfn);
	txt_clr_wid(2, AN_VIS_ROW/2, AN_VIS_COL-4);
	if (QIO_ERR_CODE(sts) && sts != FSYS_MOUNT_MOUNTED) {
	    txt_str(-1, WALK_ERR_ROW, "Failed to mount filesystem.", WHT_PAL);
	    qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
	    txt_str(-1, WALK_ERR_ROW+2, walk.data_buff, YEL_PAL);
	    st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
	    while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) { prc_delay(0); }
	    return 1;
	}
#ifdef EER_FSYS_USEALT
	walk.used_alts += eer_gets(EER_FSYS_USEALT) - alts;
#endif
	been_here |= 1<<active;
    }
    if (smp) st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
    ioq = qio_getioq();
    if (!ioq) {
	txt_str(-1, WALK_ERR_ROW, "Ran out of IOQ's", RED_PAL);
	goto err_exit;
    }	    
    strcat(volfn, "/.");
    sts = qio_open(ioq, volfn, O_RDONLY);
    while (!sts) sts = ioq->iostatus;
    if (QIO_ERR_CODE(sts)) {
	strcpy(data_buffer, "Error opening ");
	strcat(data_buffer, volfn);
	msg = data_buffer;
	qio_freeioq(ioq);
	txt_str(-1, AN_VIS_ROW-7, msg, WHT_PAL);
	qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
	txt_str(-1, AN_VIS_ROW-6, walk.data_buff, YEL_PAL);
err_exit:
	st_insn(AN_VIS_ROW-2, t_msg_ret_menu, t_msg_next, INSTR_PAL);
	while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) { prc_delay(0); }
	return 1;
    }
    fp = qio_fd2file(ioq->file);
    vol = (FsysVolume *)fp->dvc->private;
#define DISK_DRIVE_SIZ "Disk space available:  "
#define SPACE_USED_MSG "Filesystem space used: "
#define FILES_USED_MSG "Total files used:      "
    row = 4;
    if (vol->maxlba != vol->maxblocks) {
	txt_str((AN_VIS_COL-sizeof(DISK_DRIVE_SIZ)-1-7)/2, row++, DISK_DRIVE_SIZ, WHT_PAL);
	sts = (vol->maxblocks+100000)/200000;
	txt_cdecnum(sts/10, 3, RJ_BF, WHT_PAL);
	txt_cstr(".", WHT_PAL);
	txt_cdecnum(sts - (sts/10)*10, 1, RJ_ZF, WHT_PAL);
	txt_cstr("GB", WHT_PAL);
	txt_str((AN_VIS_COL-sizeof(SPACE_USED_MSG)-1-7)/2, row++, SPACE_USED_MSG, WHT_PAL);
	sts = (vol->maxlba+100000)/200000;
	txt_cdecnum(sts/10, 3, RJ_BF, WHT_PAL);
	txt_cstr(".", WHT_PAL);
	txt_cdecnum(sts - (sts/10)*10, 1, RJ_ZF, WHT_PAL);
	txt_cstr("GB", WHT_PAL);
    }
    txt_str((AN_VIS_COL-sizeof(FILES_USED_MSG)-1-7)/2, row++, FILES_USED_MSG, WHT_PAL);
    txt_cdecnum(vol->files_ffree, 5, RJ_BF, WHT_PAL);
    sts = qio_close(ioq);
    while (!sts) sts = ioq->iostatus;
    ioq->timeout = 3000000;		/* 3 second timeout */
    strcpy(data_buffer, volfn);
    strcat(data_buffer, "/diags/checksums");
    ot.spc.path = data_buffer;
    ot.spc.mode = O_RDONLY;
    ot.fid = 0;
    sts = qio_openspc(ioq, &ot.spc);
    while (!sts) { sts = ioq->iostatus; }
    walk.cksums = 0;
    walk.numb_cksums = 0;
    walk.errors = 0;
    if (!QIO_ERR_CODE(sts)) {
	if (walk.data_buff_size <= ot.alloc) {	/* no room for file */
	    txt_str(-1, AN_VIS_ROW-7, "Not enough memory to hold checksum file.", WHT_PAL);
	} else {
	    sts = qio_read(ioq, walk.data_buff, walk.data_buff_size);
	    while (!sts) { sts = ioq->iostatus; }
	    if (QIO_ERR_CODE(sts)) {
		txt_str(-1, AN_VIS_ROW-7, "Error reading checksum file.", WHT_PAL);
		qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
		txt_str(-1, AN_VIS_ROW-6, walk.data_buff, YEL_PAL);
	    } else {
		walk.cksums = (ChkSum *)walk.data_buff;
		walk.numb_cksums = ioq->iocount/sizeof(ChkSum);
		walk.data_buff += (ioq->iocount+QIO_CACHE_LINE_SIZE-1)&-QIO_CACHE_LINE_SIZE;
		walk.data_buff_size -= ioq->iocount;
		walk.data_buff_size &= -512;
	    }
	}
	qio_close(ioq);		/* we're done with this */
    }    
    if (walk.data_buff_size <= 4096) {
#if QUIET_CHECK
no_mem:
#endif
	txt_str(-1, WALK_ERR_ROW, "Sorry, not enough free memory to run this test", RED_PAL);
	while (!(ctl_read_sw(SW_ACTION|SW_NEXT)&(SW_ACTION|SW_NEXT))) { prc_delay(0); }
	ctl_read_sw(-1);
	qio_freeioq(ioq);
	return 0;
    }	
#if QUIET_CHECK
    do {
	sts = qio_open(ioq, volfn, O_RDONLY);		/* get a pointer to volume */
	while (!sts) { sts = ioq->iostatus; }
	if (QIO_ERR_CODE(sts)) {
	    qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
	    txt_str(-1, WALK_ERR_ROW, walk.data_buff, YEL_PAL);
	    sts = 0;
	} else {
#if FSYS_MAX_ALTS > 4
# error *** You will have to fix this!!!
#endif
	    U8 *history;
	    int fid, pass, row;
	    qio_close(ioq);
	    history = QIOcalloc(vol->files_ffree, 1);	/* one byte per file */
	    if (!history) goto no_mem;
#define CHECK_MSG "Checking file: "
	    txt_str((AN_VIS_COL-sizeof(CHECK_MSG)-5)/2, WALK_ANN_ROW, CHECK_MSG, WHT_PAL);
	    for (pass=0; pass < FSYS_MAX_ALTS; ++pass) {
		for (fid=0; fid < vol->files_ffree; ++fid) {
		    FsysOpenT ot;
		    FsysRamFH *rfh;
		    int afid;
		    
		    rfh = vol->files + fid;
		    if (!rfh->valid) continue;
		    if (!rfh->ramrp[pass].rptrs) {
			history[fid] |= 1 << (pass*2);	/* this copy is unusable */
			continue; /* has no alternate */
		    }
		    memset((char *)&ot, 0, sizeof(ot));
		    ot.spc.path = volfn;
		    ot.spc.mode = O_RDONLY|O_OPNCPY;
		    afid = ot.fid = (rfh->generation << 24) | fid;
		    ot.copies = pass;
		    if (walk.cksums) find_cs(afid, &walk);
		    txt_decnum((AN_VIS_COL-sizeof(CHECK_MSG)-5)/2+sizeof(CHECK_MSG),
    				WALK_ANN_ROW, fid, 5, RJ_BF, GRN_PAL);
		    txt_cstr(".", GRN_PAL);
		    txt_cdecnum(pass, 1, RJ_ZF, GRN_PAL);
		    prc_delay(0);
		    sts = qio_openspc(ioq, &ot.spc);
		    while (!sts) { sts = ioq->iostatus; }
		    if (QIO_ERR_CODE(sts)) {
			qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
			txt_str(-1, WALK_ERR_ROW, walk.data_buff, YEL_PAL);
			history[fid] |= 1 << (pass*2);
			++walk.errors;
			sts = wait_for_reply();
		    } else {
			ChkSum *csp;
			sts = read_check(ioq, &walk);
			qio_close(ioq);
			if (sts == WALK_EXIT_ERR) {
			    history[fid] |= 1 << (pass*2);
			}
#if TEST_DISK_ERRORS
# define FAKE_CS_ERROR (ctl_read_sw(IO_MISC6 << SH_MISC)&(IO_MISC6 << SH_MISC))
#else
# define FAKE_CS_ERROR 0
#endif
			if (!sts && ((csp=walk.this_cs) || FAKE_CS_ERROR )) {
			    if (!csp || (csp->chksum != walk.cs)) {
#define BAD_CS_MSG1 "Checksum error on file."
				txt_str(-1, WALK_ERR_ROW, BAD_CS_MSG1, YEL_PAL);
				history[fid] |= 2 << (pass*2);
				++walk.cs_errors;
				sts = 1;
			    }
			}
			if (walk.errors) {
			    if (!(walk.banners&1)) {
				txt_str(WALK_UCORR_COL, WALK_UCORR_ROW, WALK_UCORR_MSG, MNORMAL_PAL);
				walk.banners |= 1;
			    }
			    txt_decnum(WALK_UCORR_COL+sizeof(WALK_UCORR_MSG)-1,
				    WALK_UCORR_ROW, walk.errors, 4, RJ_BF, RED_PAL);
			}
			if (walk.cs_errors) {
			    if (!(walk.banners&2)) {
				txt_str(WALK_CKSUM_COL, WALK_CKSUM_ROW, WALK_CKSUM_MSG, MNORMAL_PAL);
				walk.banners |= 2;
			    }
			    txt_decnum(WALK_CKSUM_COL+sizeof(WALK_CKSUM_MSG)-1,
				    WALK_CKSUM_ROW, walk.cs_errors, 4, RJ_BF, RED_PAL);
			}
#ifdef EER_DSK_CORR
			if (walk.correctables) {
			    if (!(walk.banners&4)) {
				txt_str(WALK_CORR_COL, WALK_CORR_ROW, WALK_CORR_MSG, MNORMAL_PAL);
				walk.banners |= 4;
			    }
			    txt_decnum(WALK_CORR_COL+sizeof(WALK_CORR_MSG)-1,
				    WALK_CORR_ROW, walk.correctables, 4, RJ_BF, YEL_PAL);
			}
#endif
#ifdef EER_FSYS_USEALT
			if (walk.used_alts) {
			    if (!(walk.banners&8)) {
				txt_str(WALK_ALTS_COL, WALK_ALTS_ROW, WALK_ALTS_MSG, MNORMAL_PAL);
				walk.banners |= 8;
			    }
			    txt_decnum(WALK_ALTS_COL+sizeof(WALK_ALTS_MSG)-1,
				    WALK_ALTS_ROW, walk.used_alts, 4, RJ_BF, YEL_PAL);
			}
#endif
			if (sts) sts = wait_for_reply();
		    }
		    if (sts == WALK_EXIT_ABORT) break;
		}
		if (sts == WALK_EXIT_ABORT) break;
	    }
	    if (sts != WALK_EXIT_ABORT) {
		for (row=WALK_ANN_ROW; row <= WALK_ERR_ROW; ++row) {
		    txt_clr_wid(2, row, AN_VIS_COL-4);
		}
#if 0
		for (row=AN_VIS_ROW-8; row <= AN_VIS_ROW-5; ++row) {
		    txt_clr_wid(2, row, AN_VIS_COL-4);
		}
#endif
		if (sts == WALK_EXIT_OK) {
		    if (!walk.errors && !walk.cs_errors && !walk.used_alts) {
			if (!walk.correctables) {
#define AOK_MSG "Filesystem is "
			    txt_str((AN_VIS_COL-sizeof(AOK_MSG)-1-4)/2, AN_VIS_ROW/2, AOK_MSG, WHT_PAL);
			    txt_cstr("A-OK", GRN_PAL);
			} else {
			    txt_str(-1, AN_VIS_ROW/2-1, "The errors listed will be corrected by h/w.",
					    WHT_PAL);
			    txt_str((AN_VIS_COL-16)/2+1, AN_VIS_ROW/2+1, "Filesystem is ", WHT_PAL);
			    txt_cstr("OK", GRN_PAL);
			}
		    } else {
			int cs_err=0, fatal=0, jj;
			for (sts=0; sts < vol->files_ffree; ++sts) {
			    int hw = 0;
			    if ((history[sts]&0xAA)) {
				++cs_err;
			    } else {
				for (jj=0; jj < FSYS_MAX_ALTS; ++jj) {
				    if ((history[sts]&(1<<jj*2))) ++hw;
				}
				if (hw >= FSYS_MAX_ALTS) ++fatal;
			    }
			}
			if (fatal || cs_err) {
			    int c;
			    char *str;
			    str = (fatal+cs_err > 1) ?
			    	"Some of the errors listed cannot be corrected." :
			    	"The errors listed cannot be corrected.";
			    txt_str(-1, AN_VIS_ROW/2-1, str, YEL_PAL);
			    if (fatal) {
				txt_str(-1, AN_VIS_ROW/2+1, "The filesystem is broken and is not usable", RED_PAL);
			    } else {
#define NFG_MSG_P " files are corrupt. The game might not work."
#define NFG_MSG_S " file is corrupt. The game might not work."
				if (cs_err > 1) {
				    c = (AN_VIS_COL-sizeof(NFG_MSG_P)-1-4)/2;
				    str = NFG_MSG_P;
				} else {
				    c = (AN_VIS_COL-sizeof(NFG_MSG_S)-1-4)/2;
				    str = NFG_MSG_S;
				}
				txt_decnum(c, AN_VIS_ROW/2+1, cs_err, 4, RJ_BF, RED_PAL);
				txt_cstr(str, YEL_PAL);
			    }
			} else {
			    txt_str(-1, AN_VIS_ROW/2-1, "The errors listed can be corrected by s/w.", WHT_PAL);
			    txt_str(-1, AN_VIS_ROW/2+1, "The filesystem is unhealthy but remains usable", YEL_PAL);
			}
		    }
		} else {
		    txt_str(-1, AN_VIS_ROW/2-1, "Due to the severity of the errors detected,", WHT_PAL);
		    txt_str(-1, AN_VIS_ROW/2, "the filesystem is probably not usable", YEL_PAL);
		}
		sts = 0;
	    }
	    QIOfree(history);
	}
    } while (0);
#else
    sts = qio_opendir(ioq, (void *)&dp, volfn);	/* open root directory */
    while (!sts) { sts = ioq->iostatus; }
    if (QIO_ERR_CODE(sts)) {
	qio_errmsg(sts, walk.data_buff, walk.data_buff_size);
	txt_str(-1, WALK_ERR_ROW, walk.data_buff, YEL_PAL);
	sts = 0;
    } else {
	if (walk.numb_cksums) txt_str(6, AN_VIS_ROW-5, "(CS) = File's checksum is being computed and compared", WHT_PAL);
	walk.dp = dp;
	walk.ioq = ioq;
	sts = walk_directory(&walk, volfn);
	qio_closedir(ioq, (void *)dp);
	if (sts != WALK_EXIT_ABORT) {
	    int row;
	    for (row=WALK_ANN_ROW; row <= WALK_ERR_ROW; ++row) {
		txt_clr_wid(2, row, AN_VIS_COL-4);
	    }
	    for (row=AN_VIS_ROW-8; row <= AN_VIS_ROW-5; ++row) {
		txt_clr_wid(2, row, AN_VIS_COL-4);
	    }
	    if (sts == WALK_EXIT_OK && !walk.errors) {
		txt_str(-1, AN_VIS_ROW/2, "No uncorrectable errors detected.", WHT_PAL);
		txt_str(-1, AN_VIS_ROW/2+1, "Filesystem is ", WHT_PAL);
		txt_cstr("OK", GRN_PAL);
	    } else {
		txt_str(-1, AN_VIS_ROW/2, "Filesystem is not OK", YEL_PAL);
		txt_str((AN_VIS_COL-17-6-21)/2, AN_VIS_ROW/2+2, "Found a total of ", WHT_PAL);
		txt_cdecnum(walk.errors, 6, RJ_BF, RED_PAL);
		txt_cstr(" uncorrectable errors", WHT_PAL);
	    }
	    sts = 0;
	}
    }
    if (walk.numb_cksums) txt_clr_wid(6, AN_VIS_ROW-5, AN_VIS_COL-6-2);
#endif
    if (!sts) while (!(ctl_read_sw(SW_NEXT)&SW_NEXT)) { prc_delay(0); }
    qio_freeioq(ioq);
    ctl_read_sw(-1);
    return 0;
}
#endif			/* !NO_FSYS_TEST */
