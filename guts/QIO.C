/*
 * $Id: qio.c,v 1.37 1997/11/02 03:04:57 forrest Exp $
 *
 *		Copyright 1996,1997 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include "config.h"
#include "os_proto.h"
#include "phx_proto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#define QIO_LOCAL_DEFINES 1
#include "qio.h"

#if defined(__GNUC__) && __GNUC__
#ifndef inline
# define inline __inline__
#endif
#else
# define inline 
#endif

#ifndef n_elts
# define n_elts(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifndef QIO_GENERATION_MASK
# define QIO_GENERATION_MASK (0xFFFF)	/* libc uses short to hold fd's, so we mask our use of it */
#endif

static U32 inx_mask;
    
#if HOST_BOARD

#else		/* HOST_BOARD */

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>

#endif		/* HOST_BOARD */

static QioFile files[QIO_MAX_FILES];	/* A file descriptor indexes to one of these */
static QioFile *files_freelist;

static const QioDevice *device_table[QIO_MAX_DEVICES]; /* array of devices */
static int num_devices;			/* number of entries in device_table */

/************************************************************
 * qio_complete - call user's completion routine.
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	returns nothing. May have queued the user's completion
 *	routine on the AST queue.
 */
void qio_complete(QioIOQ *ioq) {
    int sts;
    if (ioq->complete) {
#if 0
	if (prc_get_astlvl() >= 0) {	/* if we're already at AST level ... */
	    ioq->complete(ioq);		/* just call the user's code immediately */
	    return;			/* and we're done */
	}
#endif
	ioq->aq.action = ioq->complete;	/* else we need to queue it up */
	ioq->aq.param = (void *)ioq;
	sts = prc_q_ast(QIO_ASTLVL, &ioq->aq);
	if (!ioq->iostatus && sts) ioq->iostatus = sts;	/* couldn't queue it for some reason */
    }
    return;
}

static int null_readwpos(QioIOQ *ioq, off_t where, void *buf, long len) {
    ioq->iostatus = QIO_EOF;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static int null_read(QioIOQ *ioq, void *buf, long len) {
    return null_readwpos(ioq, 0, buf, len);
}

static int null_writewpos(QioIOQ *ioq, off_t where, const void *buf, long len) {
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    ioq->iocount = len;
    qio_complete(ioq);
    return 0;
}

static int null_write(QioIOQ *ioq, const void *buf, long len) {
    return null_writewpos(ioq, 0, buf, len);
}

static int null_lseek(QioIOQ *ioq, off_t where, int whence) {
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    qio_complete(ioq);
    return 0;
}

static int null_open(QioIOQ *ioq, const char *path) {
    QioFile *file;
    ioq->iostatus = FSYS_IO_SUCC|SEVERITY_INFO;
    file = qio_fd2file(ioq->file);
    file->private = 0;
    file->pos = 0;
    file->size = 0;
    file->flags = 0;
    file->next = 0;
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);
    return 0;
}

static int null_fstat( QioIOQ *ioq, struct stat *stat ) {
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    ioq->iocount = 0;
    stat->st_mode = S_IFCHR;
    stat->st_size = 0;
#if !WATCOM_LIBC
    stat->st_blksize = 0;
    stat->st_blocks = 0;
#endif
    qio_complete(ioq);
    return 0;
}

static const QioFileOps null_fops = {
    null_lseek,	/* a dummy lseek is allowed but it doesn't do anything */
    null_read, 	/* FYI: read always returns EOF */
    null_write,	/* FYI: writes always succeed without error */
    0, 		/* ioctl not allowed on null device */
    null_open,	/* use open on null device */
    0,		/* use default close on null device */
    0,		/* delete not allowed */
    0,		/* fsync not allowed on null device */
    0,		/* mkdir not allowed */
    0,		/* rmdir not allowed */
    0,		/* rename not allowed */
    0,		/* truncate not allowed */
    0,		/* statfs not allowed */
    null_fstat,	/* fstat allowed */
    0,		/* nothing to cancel on this device */
    0,		/* cannot be a tty */
    null_readwpos, /* read with built-in lseek */
    null_writewpos /* write with built-in lseek */
};

static const QioDevice null_dvc = {
    "null",				/* device name */
    4,					/* length of name */
    &null_fops,				/* list of operations allowed on this device */
    0,					/* no mutex required for null device */
    0,					/* unit 0 */
};

/************************************************************
 * qio_fd2file - convert fd to pointer to QioFile
 * 
 * At entry:
 *	fd - file descriptor
 *
 * At exit:
 *	returns pointer to QioFile or 0 if error
 */
QioFile *qio_fd2file( int fd ) {
    QioFile *file = files + ( fd & inx_mask );
    if (file >= (files + n_elts(files))) return 0;
    if ( file->gen == ( fd & ~inx_mask & QIO_GENERATION_MASK ) ) return file;
    return 0;
}

/************************************************************
 * qio_file2fd - convert pointer to QioFile to fd
 * 
 * At entry:
 *	file - pointer to QioFile struct
 *
 * At exit:
 *	returns fd or -1 if error
 */
int qio_file2fd( QioFile *file ) {
    if (!file || file < files || file >= (files + n_elts(files))) return -1;
    return (file - files) | file->gen;
}

/************************************************************
 * qio_install_dvc - Add a new device to the device table
 * 
 * At entry:
 *	dvc - pointer to new device 
 *
 * At exit:
 *	device number installed or negative if failed to install
 *	because no room or already installed.
 */
int qio_install_dvc(const QioDevice *dvc) {
    const QioDevice **dp, *d, *sts;
    int ii, oldipl;

    dp = device_table;
    sts = 0;
    oldipl = prc_set_ipl(INTS_OFF);
    for (ii=0; ii < num_devices; ++ii) {
	d = *dp++;
	if (strcmp(d->name, dvc->name) == 0) {
	    prc_set_ipl(oldipl);
	    return -QIO_ALREADY_INST;	/* already in the table */
	}
    }
    if (num_devices >= QIO_MAX_DEVICES) {
	prc_set_ipl(oldipl);
	return -QIO_TOO_MANY_DVCS;
    }
    *dp = dvc;
    ++num_devices;
    prc_set_ipl(oldipl);
    return ii;
}

#ifndef QIO_IOQ_BATCH
# define QIO_IOQ_BATCH	16
#endif

static QioIOQ *ioq_pool_head;
#if defined(DEBUG_MALLOC) && DEBUG_MALLOC
static int num_ioqs;
#endif

QioIOQ *qio_getioq_ptr(QioIOQ **head, int size) {
    int oldipl, ii;
    QioIOQ *q;

    oldipl = prc_set_ipl(INTS_OFF); /* this cannot be interrupted */
    q = *head;			/* get next item */
    if (!q) {			/* need to get more ioq's */
	QioIOQ *new;
	prc_set_ipl(oldipl);	/* interrupts are ok now */
	new = QIOcalloc(QIO_IOQ_BATCH, size);
	if (!new) {
	    return 0;		/* no more */
	}
	for (q=new, ii=0; ii < QIO_IOQ_BATCH-1; ++ii) {
	    QioIOQ *prev;
	    q->owner = head;
	    prev = q;
	    q = (QioIOQ *)((char *)q + size);
	    prev->next = q;
	}
	q->owner = head;
	prc_set_ipl(INTS_OFF);	/* interrupts not allowed for the following */
	q->next = *head;
	q = new;
    }
    *head = q->next;
    prc_set_ipl(oldipl);	/* interrupts ok again */
    memset((void *)q, 0, size);	/* zap everything in ioq */
    q->owner = head;		/* reset the owner field */
    q->file = -1;		/* assume FD is nfg */
    return q;
}

/************************************************************
 * qio_getioq - Get a QioIOQ from the system's QIO queue pool
 * 
 * At entry:
 *	no requirements
 *
 * At exit:
 *	returns pointer to queue or 0 if none available.
 */
QioIOQ *qio_getioq(void) {
#if defined(DEBUG_MALLOC) && DEBUG_MALLOC
    ++num_ioqs;
#endif
    return qio_getioq_ptr(&ioq_pool_head, sizeof(QioIOQ));
}

int qio_freeioq_ptr(QioIOQ *que, QioIOQ **head) {
    int oldipl;

    if (!que || !head) return 1;
    if (que->owner != head) return 1;
    que->head = head;
    oldipl = prc_set_ipl(INTS_OFF); /* this cannot be interrupted */
    que->next = *head;
    *head = que;
    prc_set_ipl(oldipl);
    return 0;
}

/************************************************************
 * qio_freeioq - Free a QioIOQ as obtained from a previous
 * call to qio_getioq().
 * 
 * At entry:
 *	que - pointer to queue element to put back in pool.
 *
 * At exit:
 *	0 if success or 1 if queue didn't belong to pool.
 */
int qio_freeioq(QioIOQ *que) {
#if defined(DEBUG_MALLOC) && DEBUG_MALLOC
    --num_ioqs;
#endif
    return qio_freeioq_ptr(que, &ioq_pool_head);
}

/************************************************************
 * qio_getfile - Get a free QioFile from the system's pool
 * 
 * At entry:
 *	No requirements.
 *
 * At exit:
 *	returns pointer to QioFile or 0 if none available.
 */
static QioFile *qio_getfile(void) {
    int oldipl;
    QioFile *f;

    oldipl = prc_set_ipl(INTS_OFF); /* this cannot be interrupted */
    f = files_freelist;		/* get next item */
    if (f) {
	files_freelist = f->next;
	f->next = 0;		/* take this guy from queue */
    }
    prc_set_ipl(oldipl);
    return f;
}

/************************************************************
 * qio_freefile - Put an unused QioFile back into the system's pool
 * 
 * At entry:
 *	file - pointer to QioFile which to free
 *
 * At exit:
 *	returns 0 if success or 1 if failure.
 */
int qio_freefile(QioFile *file) {
    int oldipl, pos, cnt;
    QioFile **prev, *f;

    if (file < files || file > files+QIO_MAX_FILES) return 1;
    if (file->next) return 1;
    file->gen = (file->gen + 1 + inx_mask) & QIO_GENERATION_MASK;
    prev = &files_freelist;
    pos = file - files;
    oldipl = prc_set_ipl(INTS_OFF); /* this should not be interrupted */
    cnt = 0;
    while ((f = *prev)) {
	if (f-files > pos) {
	    file->next = f;
	    *prev = file;
	    prc_set_ipl(oldipl);
	    return 0;
	}
	if (++cnt >= QIO_MAX_FILES) {	/* prevent runaway loops with ints off */
	    prc_set_ipl(oldipl);
	    return 1;
	}
	prev = &f->next;
    }
    *prev = file;
    prc_set_ipl(oldipl);
    return 0;
}

/***********************************************************************
 * qio_init - Initialize the QIO data structs. To be called once during
 * boot sequence.
 * 
 * At entry:
 *	No requirements.
 *
 * At exit:
 *	Returns 0 for success or QIO_xxx if error.
 */
int qio_init(void) {
    int ii;
    QioFile *file;
    QioIOQ *ioq;

    inx_mask = -1;
    while ( inx_mask & QIO_MAX_FILES ) inx_mask <<= 1;
    inx_mask = ~inx_mask;

    files_freelist = file = files;
    for (ii=0; ii < QIO_MAX_FILES-1; ++ii, ++file) file->next = file+1;

    qio_install_dvc(&null_dvc);		/* install the null device */
    ioq = qio_getioq();
    ioq->complete = 0;		/* no completion required for this */
    ioq->timeout = 0;		/* no timeout required either */
    qio_open(ioq, "/null", O_RDONLY);	/* stdin to FD 0 */
    qio_open(ioq, "/null", O_WRONLY);	/* stdout to FD 1 */
    qio_open(ioq, "/null", O_WRONLY);	/* stderr to FD 2 */
    qio_freeioq(ioq);
    return 0;
}

/************************************************************
 * qio_lookupdvc - Get a pointer to device
 * 
 * At entry:
 *	name - pointer to null terminated string with device name
 *
 * At exit:
 *	Returns pointer to QioDevice if one is found or 0 if not.
 */

const QioDevice *qio_lookupdvc(const char *name) {
    int ii, len;
    const QioDevice **dvc, *d;

    if (name && *name == QIO_FNAME_SEPARATOR) {
	++name;
	len = strlen(name);
	dvc = device_table;
	for (ii=0; ii < QIO_MAX_DEVICES; ++ii) {
	    d = *dvc++;
	    if (!d) continue;
	    if (len < d->name_len) continue;
	    if (strncmp(name, d->name, d->name_len) == 0) {
		if (len > d->name_len && name[d->name_len] != QIO_FNAME_SEPARATOR) continue;
		return d;
	    }
	}
    }
    return 0;
}

/************************************************************
 * qio_fstat - stat a file or device
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct (ioq->file must be open)
 *	stat - pointer to struct stat into which the stats are to
 *		be placed.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when stat completes.
 *	non-zero if unable to queue the stat and completion routne
 *	will _not_ be called in that case.
 */
int qio_fstat(QioIOQ *ioq, struct stat *stat) {
    const QioDevice *dvc;
    QioFile *file;

    if (!ioq) return QIO_INVARG;		/* must have an ioq ptr */
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!stat) return (ioq->iostatus = QIO_INVARG); /* must have a param */
    memset((void *)stat, 0, sizeof(struct stat));
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    dvc = file->dvc;
    if (!dvc->fio_ops->fstat) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    return (dvc->fio_ops->fstat)(ioq, stat);
}

/****************************************************************
 * qio_readwpos - read bytes from file after positioning to where
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	where - position of place in file to begin reading
 *	buf - pointer to buffer into which to read
 *	len - number of bytes to read
 * 
 * This function is equivalent to a
 *	qio_lseek(ioq, where, SEEK_SET) followed by a
 *	qio_read(ioq, buf, len);
 *
 * NOTE: the following members in the ioq must have already been
 *	set before calling this function:
 *	ioq->file - must be a valid file descriptor
 *	ioq->complete - if completion required else 0
 *	ioq->user (and/or ioq->user2) - if ioq->complete also set
 *	ioq->timeout - if a timeout (in microseconds) is desired else 0
 *	(other members are don't cares).
 *
 * At exit:
 *	returns 0 if success or non-zero if error
 */
int qio_readwpos(QioIOQ *ioq, off_t where, void *buf, long len) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!buf) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FREAD)) return (ioq->iostatus = QIO_NOT_ORD); /* not open for read */
    ops = file->dvc->fio_ops;
    if (!ops->readwpos) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;				/* assume we're to queue */
    return (ops->readwpos)(ioq, where, buf, len);
}

/************************************************************
 * qio_read - read bytes from file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	buf - pointer to buffer into which to read
 *	len - number of bytes to read
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when read completes.
 *	non-zero if unable to queue the read and completion routne
 *	will _not_ be called in that case.
 */
int qio_read(QioIOQ *ioq, void *buf, long len) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!buf) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FREAD)) return (ioq->iostatus = QIO_NOT_ORD); /* not open for read */
    ops = file->dvc->fio_ops;
    if (!ops->read) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;		/* assume we're to queue */
    return (ops->read)(ioq, buf, len);
}

/****************************************************************
 * qio_writewpos - write bytes to file after position to 'where'
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	where - position in file/device to start writing
 *	buf - pointer to buffer which to write
 *	len - number of bytes to write
 * 
 * This function is equivalent to a
 *	qio_lseek(ioq, where, SEEK_SET) followed by a
 *	qio_write(ioq, buf, len);
 *
 * NOTE: the following members in the ioq must have already been
 *	set before calling this function:
 *	ioq->file - must be a valid file descriptor
 *	ioq->complete - if completion required else 0
 *	ioq->user (and/or ioq->user2) - if ioq->complete also set
 *	ioq->timeout - if a timeout (in microseconds) is desired else 0
 *	(other members are don't cares).
 *
 * At exit:
 *	returns 0 if success or non-zero if error
 */
int qio_writewpos(QioIOQ *ioq, off_t where, const void *buf, long len) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!buf) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FWRITE)) return (ioq->iostatus = QIO_NOT_OWR); /* not open for write */
    ops = file->dvc->fio_ops;
    if (!ops->writewpos) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    return (ops->writewpos)(ioq, where, buf, len);
}

/************************************************************
 * qio_write - write bytes to file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	buf - pointer to buffer which to write
 *	len - number of bytes to write
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when read completes.
 *	non-zero if unable to queue the read and completion routne
 *	will _not_ be called in that case.
 */
int qio_write(QioIOQ *ioq, const void *buf, long len) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!buf) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FWRITE)) return (ioq->iostatus = QIO_NOT_OWR); /* not open for write */
    ops = file->dvc->fio_ops;
    if (!ops->write) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    return (ops->write)(ioq, buf, len);
}

/************************************************************
 * qio_open - Open a device or file
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	path - pointer to a null terminated string with dvc/path/name.
 *	mode - the logical 'or' of one or more of the O_xxx flags found
 *		in fcntl.h
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will contain the FD of the newly
 *	opened file or -1 if the open failed.
 */
int qio_open(QioIOQ *ioq, const char *path, int mode) {
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->file = -1;			/* assume failure */
    ioq->iocount = 0;			/* just on general principle */
    if (!path) return (ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
    if (!dvc) {				/* name not in device table */
	dvc = qio_lookupdvc("/d0");	/* assume open to filesystem on drive 0 */
	fake = 1;
    }
    if (!dvc) return (ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->open) return (ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();		/* get a free file */
    if (!file) return (ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;			/* no special */
    file->dvc = dvc;			/* remember which device we're talking to */
    file->mode = mode+1;		/* remember I/O mode */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    return (fops->open)(ioq, path+fake);
}

/************************************************************
 * qio_delete - delete a file
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	path - pointer to a null terminated string with dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int qio_delete(QioIOQ *ioq, const char *path) {
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->file = -1;			/* assume failure */
    ioq->iocount = 0;			/* just on general principle */
    if (!path) return (ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
    if (!dvc) {				/* name not in device table */
	dvc = qio_lookupdvc("/d0");	/* assume open to filesystem on drive 0 */
	fake = 1;
    }
    if (!dvc) return (ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->delete) return (ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();		/* get a free file */
    if (!file) return (ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;			/* no special */
    file->dvc = dvc;			/* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    return (fops->delete)(ioq, path+fake);
}

/************************************************************
 * qio_rename - rename a file
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	source - pointer to a null terminated string with src dvc/path/name.
 *	dest - pointer to a null terminated string with dst dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int qio_rename(QioIOQ *ioq, const char *source, const char *dest) {
    const QioFileOps *fops;
    const QioDevice *dvcs, *dvcd;
    QioFile *file;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->file = -1;			/* assume failure */
    ioq->iocount = 0;			/* just on general principle */
    if (!source || !*source || !dest || !*dest) return (ioq->iostatus = QIO_INVARG);
    dvcs = qio_lookupdvc(source);
    if (!dvcs) return (ioq->iostatus = QIO_NSD);
    dvcd = qio_lookupdvc(dest);
    if (!dvcd) return (ioq->iostatus = QIO_NSD);
    if (dvcs != dvcd) return (ioq->iostatus = QIO_NOTSAMEDVC);
    fops = dvcs->fio_ops;
    if (!fops || !fops->rename) return (ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();		/* get a free file */
    if (!file) return (ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = 0;			/* no special */
    file->dvc = dvcs;			/* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    return (fops->rename)(ioq, source+dvcs->name_len+1, dest+dvcs->name_len+1);
}

/************************************************************
 * qio_openspc - Open a device or file with special parameters
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	spc - pointer to a QioOpenSpc struct which may be defined
 *		differently depending on the device which is being opened.
 *		The first two members of the struct _MUST_ be path and
 *		mode respectively.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will contain the FD of the newly
 *	opened file or -1 if the open failed.
 */
int qio_openspc(QioIOQ *ioq, QioOpenSpc *spc) {
    const QioFileOps *fops;
    const char *path;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->file = -1;			/* assume failure */
    ioq->iocount = 0;			/* general principle */
    if (!spc) return (ioq->iostatus = QIO_INVARG);
    path = spc->path;
    if (!path) return (ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
    if (!dvc) {				/* name not in device table */
	dvc = qio_lookupdvc("/d0");	/* assume open to filesystem on drive 0 */
	fake = 1;
    }
    if (!dvc) return (ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->open) return (ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();		/* get a free file */
    if (!file) return (ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->private = spc;		/* remember ptr to user's special if any */
    file->dvc = dvc;			/* remember which device we're talking to */
    file->mode = spc->mode+1;
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    return (fops->open)(ioq, path+fake);
}

/************************************************************
 * qio_ioctl - issue an ioctl to specified file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	cmd - command argument
 *	arg - argument defined by command and device
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when close completes.
 *	non-zero if unable to queue the close and completion routne
 *	will _not_ be called in that case.
 */
int qio_ioctl(QioIOQ *ioq, unsigned int cmd, void *arg) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (!ops->ioctl) return (ioq->iostatus = QIO_NOTSUPP);
    return (ops->ioctl)(ioq, cmd, arg); /* call driver's ioctl routine */
}

/************************************************************
 * qio_close - close a file
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when close completes.
 *	non-zero if unable to queue the close and completion routne
 *	will _not_ be called in that case.
 */
int qio_close(QioIOQ *ioq) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (ops->close) return (ops->close)(ioq); /* call driver's close routine */
    file->dvc = 0;		/* use the default close */
    ioq->file = -1;		/* tell 'em fd is no good anymore */
    qio_freefile(file);	/* put file back on freelist */
    ioq->iostatus = QIO_SUCC|SEVERITY_INFO;
    qio_complete(ioq);	/* call his completion routine */
    return 0;
}

/************************************************************
 * qio_isatty - check if device is a tty
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when close completes.
 *	non-zero if unable to queue the close and completion routne
 *	will _not_ be called in that case.
 */
int qio_isatty(QioIOQ *ioq) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (ops->isatty) return (ops->isatty)(ioq); /* call driver's isatty routine */
    return (ioq->iostatus = QIO_NOTSUPP);
}

/************************************************************
 * qio_lseek - seek a file to a specific position
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *	where - new position
 *	whence - one of SEEK_SET, SEEK_END or SEEK_CUR (as defined
 *		in <unistd.h>
 *
 * At exit:
 *	0 if successfully queued, although it normally does not
 *	require that any queuing take place. iostatus contains
 *	error code if any and iocount contains the new position.
 */
int qio_lseek(QioIOQ *ioq, off_t where, int whence ) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (ops->lseek) return (ops->lseek)(ioq, where, whence); /* call driver's close routine */
    return QIO_NOTSUPP;		/* function is not supported by that driver */
}

/************************************************************
 * qio_mkdir - make a directory 
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	arg - pointer to null terminated string with dvc/path/dirname
 *	mode - not used at this time.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will be set to -1 in any case.
 */
int qio_mkdir(QioIOQ *ioq, const char *name, int mode) {
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;
    int fake = 0;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->file = -1;			/* assume failure */
    ioq->iocount = 0;
    if (!name || *name == 0) return (ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(name);
    if (!dvc) {				/* name not in device table */
	dvc = qio_lookupdvc("/d0");	/* try filesystem on drive 0 */
	fake = 1;
    }
    if (!dvc) return (ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->mkdir) return (ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();		/* get a free file */
    if (!file) return (ioq->iostatus = QIO_NOFILE); /* ran out of files */
    file->dvc = dvc;			/* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    return (fops->mkdir)(ioq, name+fake, mode);
}

/************************************************************
 * qio_fsync - sync a filesystem
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	arg - pointer to null terminated string with dvc/path/dirname
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when fsync completes.
 *	non-zero if unable to queue the fsync open and completion routne
 *	will _not_ be called in that case.
 *	The file member of ioq will be set to -1 in any case.
 */
int qio_fsync(QioIOQ *ioq, const char *name) {
    const QioFileOps *fops;
    const QioDevice *dvc;
    QioFile *file;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->file = -1;			/* assume failure */
    ioq->iocount = 0;
    if (!name || *name == 0) return (ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(name);
    if (!dvc) {				/* name not in device table */
	dvc = qio_lookupdvc("/d0");	/* try filesystem on drive 0 */
    }
    if (!dvc) return (ioq->iostatus = QIO_NSD);
    fops = dvc->fio_ops;
    if (!fops || !fops->fsync) return (ioq->iostatus = QIO_NOTSUPP);
    file = qio_getfile();		/* get a free file */
    if (!file) return (ioq->iostatus = QIO_NOFILE); /* ran out of file descriptors */
    file->dvc = dvc;			/* remember which device we're talking to */
    ioq->file = qio_file2fd(file);
    ioq->iostatus = 0;
    return (fops->fsync)(ioq);
}

/************************************************************
 * qio_opendir - Open a directory 
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	dirp - pointer to pointer to type DIR (as defined in fsys.h for fsys directories)
 *	path - pointer to a null terminated string with dvc/path/name.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 *	A pointer to a kernel provided struct (DIR in the case of fsys files)
 *	will have been placed into the location pointed to by the dirp parameter
 *	or 0 if an error prevented the open.
 */
int qio_opendir(QioIOQ *ioq, void **dirp, const char *path) {
    const QioFileOps *fops;
    const QioDevice *dvc;
    int fake = 0;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;			/* just on general principle */
    if (!dirp) return (ioq->iostatus = QIO_INVARG);
    *dirp = 0;				/* assume failure */
    if (!path) return (ioq->iostatus = QIO_INVARG);
    dvc = qio_lookupdvc(path);
    if (!dvc) {				/* name not in device table */
	dvc = qio_lookupdvc("/d0");	/* assume open to filesystem on drive 0 */
	fake = 1;
    }
    if (!dvc) return (ioq->iostatus = QIO_NSD);
    fake = fake ? 0 : dvc->name_len + 1;
    fops = dvc->fio_ops;
    if (!fops || !fops->opendir) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    ioq->private = (void *)dvc;
    return (fops->opendir)(ioq, dirp, path+fake);
}

/************************************************************
 * qio_readdir - read a directory entry.
 * 
 * At entry:
 *	que - a pointer to a QioIOQ struct
 *	dirp - pointer to pointer to type DIR (as defined in fsys.h for fsys directories)
 *	path - pointer to a type 'struct direct' into which the results will be placed.
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when open completes.
 *	non-zero if unable to queue the open and completion routne
 *	will _not_ be called in that case.
 */
int qio_readdir(QioIOQ *ioq, void *dirp, void *direct) {
    const QioDevice *dvc;
    const QioFileOps *fops;
    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;			/* just on general principle */
    if (!dirp) return (ioq->iostatus = QIO_INVARG);
    if (!(dvc=(const QioDevice *)ioq->private)) return (ioq->iostatus = QIO_NOTOPEN);
    fops = dvc->fio_ops;
    if (!fops || !fops->readdir) return (ioq->iostatus = QIO_NOTSUPP);
    return fops->readdir(ioq, dirp, direct);
}

int qio_rewdir(QioIOQ *ioq, void *dirp) {
    const QioDevice *dvc;
    const QioFileOps *fops;
    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    if (!dirp) return (ioq->iostatus = QIO_INVARG);
    if (!(dvc = (const QioDevice *)ioq->private)) return (ioq->iostatus = QIO_NOTOPEN);
    fops = dvc->fio_ops;
    if (!fops || !fops->rewdir) return (ioq->iostatus = QIO_NOTSUPP);
    return fops->rewdir(ioq, dirp);
}

int qio_closedir(QioIOQ *ioq, void *dirp) {
    const QioDevice *dvc;
    const QioFileOps *fops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;			/* just on general principle */
    if (!dirp) return (ioq->iostatus = QIO_INVARG);
    if (!(dvc = (const QioDevice *)ioq->private)) return (ioq->iostatus = QIO_NOTOPEN);
    fops = dvc->fio_ops;
    if (!fops || !fops->closedir) return (ioq->iostatus = QIO_NOTSUPP);
    return fops->closedir(ioq, dirp);
}

int qio_seekdir(QioIOQ *ioq, void *dirp, long loc) {
    if (!ioq) return QIO_INVARG;
    if (!dirp) return (ioq->iostatus = QIO_INVARG);
    return (ioq->iostatus = QIO_NOTSUPP);
}

int qio_telldir(QioIOQ *ioq, void *dirp) {
    if (!ioq) return QIO_INVARG;
    if (!dirp) return (ioq->iostatus = QIO_INVARG);
    return (ioq->iostatus = QIO_NOTSUPP);
}

/************************************************************
 * qio_getmutex - get mutex and switch to AST level.
 * 
 * At entry:
 *	mutex - pointer to mutex struct
 *	func - ptr to AST function
 *	arg - argument to pa
 *
 * At exit:
 *	returns 0 if success or one of QIO_MUTEX_xxx if not.
 *
 * The function is queued at the specified AST level. If the mutex is
 * busy, the function is put on a wait queue and will be queued
 * when the mutex is free'd.
 */
int qio_getmutex(QioMutex *mutex, void (*func)(QioIOQ *), QioIOQ *ioq) {
    int oldipl, sts;
    struct act_q *q;
        
    if (!mutex || !func || !ioq) return QIO_MUTEX_INVARG;
    q = &ioq->aq;
    q->que = q->next = 0;		/* ready this for execution */
    q->action = func;			/* remember caller's function */
    q->param = (void *)ioq;		/* and ptr to QioIOQ */
    oldipl = prc_set_ipl(INTS_OFF);	/* stop all activity */
    if (!mutex->current) {		/* if the mutex is available */
	mutex->current = q;		/* claim the mutex */
#if 0
	if (prc_get_astlvl() >= 0) {	/* if we are already at AST level, just do it ... */
	    prc_set_ipl(oldipl);	/* interrupts ok now */
	    func(ioq);			/* call his function immediately */
	    return 0;			/* this cannot fail */
	}
#endif
	sts = prc_q_ast(QIO_ASTLVL, q);	/* queue his function at AST level */
	if (sts) {
	    mutex->current = 0;		/* and not busy */
	    prc_set_ipl(oldipl);	/* interrupts ok now */
	    return QIO_MUTEX_FATAL;	/* couldn't queue AST for some reason */
	}
	prc_set_ipl(oldipl);
	return 0;			/* success */
    }
    if (mutex->current->param == (void *)ioq) {
	prc_set_ipl(oldipl);
	return QIO_MUTEX_NESTED;	/* cannot claim mutex with identical parameter */
    }
    if (mutex->tail) {			/* if there is a tail pointer */
	mutex->tail->next = q;		/* put the new guy at the end of waiting list */
    } else {
	mutex->waiting = q;		/* else first is also the last */
    }
    mutex->tail = q;			/* new guy becomes the new tail */
    q->que = (struct act_q *)&mutex->waiting;	/* this is the head of this queue */
    prc_set_ipl(oldipl);		/* interrupts ok now */
    return 0;				/* success */    
}

/************************************************************
 * qio_freemutex - free a previously claimed volume mutex.
 * 
 * At entry:
 *	mutex - pointer to mutex struct
 *	valid - pointer to QioIOQ that was passed to the getmutex
 *		function. (this parameter is used just to validate that
 *		the 'free' is being done by the same I/O that
 *		claimed the mutex).
 *
 * At exit:
 *	returns 0 if success or one of QIO_MUTEX_xxx if error.
 */
int qio_freemutex(QioMutex *mutex, QioIOQ *ioq) {
    int oldipl, sts;
    struct act_q *q;

    if (!mutex) return QIO_MUTEX_INVARG;		/* no mutex pointer */
    if (!(q=mutex->current)) return QIO_MUTEX_NONE;	/* no mutex claimed */
    if (q->param != (void *)ioq) return QIO_MUTEX_NOTOWN; /* mutex claimed by someone else */
    oldipl = prc_set_ipl(INTS_OFF);	/* can't have interrupts for the following */
    q = mutex->waiting;			/* pluck the next guy off the waiting list */
    mutex->current = q;			/* make it current */
    if (!q) {				/* if nobody is waiting */
	mutex->tail = 0;		/* call me paranoid */
	prc_set_ipl(oldipl);		/* interrupts ok now */
	return 0;			/* success */
    }
    if (!(mutex->waiting = q->next)) {	/* next guy in list is first one waiting */
	mutex->tail = 0;		/* if we took the last one, then we also took the tail */
    }
    prc_set_ipl(oldipl);		/* interrupts ok now */
    q->que = q->next = 0;		/* ready the queue for execution */
    sts = prc_q_ast(QIO_ASTLVL, q);	/* jump to ast level */
    if (sts) return QIO_MUTEX_FATAL;	/* I do not believe it is possible for prc_q_ast to fail */
    return 0;
}

/************************************************************
 * qio_cancel - cancel a pending qio
 * 
 * At entry:
 *	ioq - pointer to QioIOQ struct
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when cancel completes.
 *	Non-zero if unable to queue the cancel; completion routine
 *	will _not_ be called in that case.
 */
int qio_cancel(QioIOQ *ioq) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    ops = file->dvc->fio_ops;
    ioq->iostatus = 0;
    if (ops->cancel) return (ops->cancel)(ioq); /* call driver's cancel routine */
    return (ioq->iostatus = QIO_NOTSUPP);
}

/************************************************************
 * qio_ioq_enq - used by qio_cleanup below
 */
static inline void qio_ioq_enq( QioIOQ **head, QioIOQ **tail, QioIOQ *new )
{
 new->aq.next = 0;
 new->aq.que = (struct act_q *)head;	/* point back to head */

 if ( *tail )				/* list has members */
 {
  (*tail)->aq.next = (struct act_q *)new;
  *tail = new;
 }
 else *head = *tail = new;
}
/*
 * qio_cleanup - remove QioIOQ structures associated with "file"
 *		 from mutex waiting list specified by "pqm", and
 *		 complete them with "status".
 * 
 * At entry:
 *	pqm    - pointer to QioMutex structure
 *	file   - file id to match during search
 *	status - completion iostatus
 *
 * At exit:
 *	Non-zero if QioIOQ structure on mutex.current is associated
 *	with "file" -- pointer to QioIOQ but mutex is not released.
 *	Zero if QioIOQ structure on mutex.current is not associated
 *	with "file"
 *
 *	Used in qio_eth.c by eth_cancel_ast.
 */
QioIOQ *qio_cleanup( QioMutex *pqm, int file, long status )
{
 int old_ipl = prc_set_ipl(INTS_OFF);
 struct act_q **head = &pqm->waiting;
 QioIOQ *ph = 0;
 QioIOQ *pt = 0;
 QioIOQ *pioq;

 pqm->tail = 0;
/*
 * Remove any QioIOQ associated with this file and place
 * it on a temporary list while interrupts are disabled.
 */
 while ( *head )
 {
  pioq = (QioIOQ *)(*head)->param;
  if ( pioq->file == file )
  {
   *head = (*head)->next;
   qio_ioq_enq( &ph, &pt, pioq );
  }
  else
  {
   pqm->tail = *head;
   head = &(*head)->next;
  }
 }
 prc_set_ipl(old_ipl);
/*
 * Process any QioIOQ on the temporary
 * list while interrupts are enabled.
 */
 while ( ( pioq = ph ) )
 {
  ph = (QioIOQ *)pioq->aq.next;
  pioq->aq.next = 0;
  pioq->aq.que = 0;
  pioq->iostatus = status;
  qio_complete(pioq);
 }
 if ( pqm->current )
 {
  pioq = (QioIOQ *)pqm->current->param;
  if ( pioq->file == file ) return pioq;
 }
 return 0;
}

/************************************************************
 * qio_readv - read input to scattered buffers
 *
 * At entry:
 *	ioq    - pointer to QioIOQ struct
 *	iov    - pointer to array of IOVect struct
 *	iovcnt - number of IOVect struct in array
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when readv completes.
 *	Non-zero if unable to queue the readv; completion routine
 *	will _not_ be called in that case.
 */

int qio_readv(QioIOQ *ioq, const IOVect *iov, long iovcnt) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!iov || !iovcnt) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FREAD)) return (ioq->iostatus = QIO_NOT_ORD); /* not open for read */
    ops = file->dvc->fio_ops;
    if (!ops->readv) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;		/* assume we're to queue */
    return (ops->readv)(ioq, iov, iovcnt);
}

/************************************************************
 * qio_writev - write output from scattered buffers
 *
 * At entry:
 *	ioq    - pointer to QioIOQ struct
 *	iov    - pointer to array of IOVect struct
 *	iovcnt - number of IOVect struct in array
 *
 * At exit:
 *	0 if function successfully queued and completion routine
 *	will be called, if one is provided, when writev completes.
 *	Non-zero if unable to queue the writev; completion routine
 *	will _not_ be called in that case.
 */

int qio_writev(QioIOQ *ioq, const IOVect *iov, long iovcnt) {
    QioFile *file;
    const QioFileOps *ops;

    if (!ioq) return QIO_INVARG;
    if (ioq->aq.que) return QIO_IOQBUSY;	/* ioq already in use */
    ioq->iocount = 0;
    if (!iov || !iovcnt) return (ioq->iostatus = QIO_INVARG);
    file = qio_fd2file(ioq->file);
    if (!file || !file->dvc) return (ioq->iostatus = QIO_NOTOPEN);
    if (!(file->mode&_FWRITE)) return (ioq->iostatus = QIO_NOT_OWR); /* not open for write */
    ops = file->dvc->fio_ops;
    if (!ops->writev) return (ioq->iostatus = QIO_NOTSUPP);
    ioq->iostatus = 0;
    return (ops->writev)(ioq, iov, iovcnt);
}


/*************************************************************
 * The following functions are UNIX(tm) shims for the functions
 * found in stdio (libc). See the man pages for details about
 * these commands. Errors are reported as -QIO_xxx errors
 * in addition to the value placed in errno.
 *
 * NOTE: Being shims doing blocking wait-mode I/O, they cannot
 * be called at ASTLVL.
 */

int stat(const char *name, struct stat *stat) {
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    if (!name || !stat) {
	errno = QIO_INVARG;
	return -QIO_INVARG;
    }
    memset((void *)stat, 0, sizeof(struct stat));
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    sts = qio_open(ioq, (void *)name, O_RDONLY);
    while (!sts) { sts = ioq->iostatus; }
    if (!QIO_ERR_CODE(sts)) {
	qio_fstat(ioq, stat);
	while (!(sts=ioq->iostatus)) { ; }
    }
    sts = qio_close(ioq);
    while (!sts) { sts = ioq->iostatus; }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return 0;
}

int unlink(const char *name) {
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    if (!name || !*name) {
	errno = QIO_INVARG;
	return -QIO_INVARG;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    sts = qio_delete(ioq, name);
    while (!sts) { sts = ioq->iostatus; }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return 0;
}

int rename(const char *source, const char *dest) {
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    if (!source || !*source || !dest || !*dest) {
	errno = QIO_INVARG;
	return -QIO_INVARG;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    sts = qio_rename(ioq, source, dest);
    while (!sts) { sts = ioq->iostatus; }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return 0;
}

int open(const char *name, int mode, ...) {
    QioIOQ *ioq;
    int sts, fd;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    if (!name) {
	errno = QIO_INVARG;
	return -QIO_INVARG;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    sts = qio_open(ioq, (void *)name, mode);
    while (!sts) { sts = ioq->iostatus; }	/* wait for complete */
    fd = ioq->file;			/* save assigned fd */
    qio_freeioq(ioq);    		/* done with ioq */
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	fd = -sts;
    }
    return fd;
}

int close(int fd) {
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_close(ioq);
    while (!sts) { sts = ioq->iostatus; }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return 0;
}

off_t lseek(int fd, off_t off, int wh) {
    QioIOQ *ioq;
    int sts;
    off_t new;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_lseek(ioq, off, wh);
    while (!sts) { sts = ioq->iostatus; }
    new = ioq->iocount;
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return new;
}

int read(int fd, void *buf, int len) {
    QioIOQ *ioq;
    int sts, val;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_read(ioq, buf, len);
    while (!sts) { sts = ioq->iostatus; }
    val = ioq->iocount;
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return val;
}

int write(int fd, const void *buf, int len) {
    QioIOQ *ioq;
    int sts, val;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_write(ioq, buf, len);
    while (!sts) { sts = ioq->iostatus; }
    val = ioq->iocount;
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return val;
}

int ioctl(int fd, unsigned int cmd, unsigned long arg) {
    QioIOQ *ioq;
    int sts, val;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_ioctl(ioq, cmd, (void *)arg);
    while (!sts) { sts = ioq->iostatus; }
    val = ioq->iocount;
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return val;
}

int fstat(int fd, struct stat *stat) {
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    if (!stat) {
	errno = QIO_INVARG;
	return -QIO_INVARG;
    }
    memset((void *)stat, 0, sizeof(struct stat));
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_fstat(ioq, stat);
    while (!sts) { sts = ioq->iostatus; }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return 0;
}

int isatty(int fd) {
    QioIOQ *ioq;
    int sts;

    if (prc_get_astlvl() >= 0) return 0;
    ioq = qio_getioq();
    if (!ioq) return 0;
    ioq->file = fd;
    sts = qio_isatty(ioq);
    while (!sts) { sts = ioq->iostatus; }
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) return 0;
    return 1;
}

int readv(int fd, const void *iov, int iovcnt) {
    QioIOQ *ioq;
    int sts, val;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_readv(ioq, iov, iovcnt);
    while (!sts) { sts = ioq->iostatus; }
    val = ioq->iocount;
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return val;
}

int writev(int fd, const void *iov, int iovcnt) {
    QioIOQ *ioq;
    int sts, val;

    if (prc_get_astlvl() >= 0) {
	errno = QIO_BADLVL;
	return -QIO_BADLVL;
    }
    ioq = qio_getioq();
    if (!ioq) {
	errno = QIO_NOIOQ;
	return -QIO_NOIOQ;
    }
    ioq->file = fd;
    sts = qio_writev(ioq, iov, iovcnt);
    while (!sts) { sts = ioq->iostatus; }
    val = ioq->iocount;
    qio_freeioq(ioq);
    if (QIO_ERR_CODE(sts)) {
	errno = sts;
	return -sts;
    }
    return val;
}
