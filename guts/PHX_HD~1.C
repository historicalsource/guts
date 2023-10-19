/************************** Things we need ***************************
`ChangeToMole'		x
`ForceMoleToStop'	x
`NumLSN'		x
`Read_Sectors'		x
`Write_Sectors'		x
`InitMole'		x
`AllowMoleInGame'	x
`WrFile'		x
`HDStart'		x
`RdFile'
`read_file_section'
 **********************************************************************/

#include <config.h>
#include <os_proto.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#define QIO_LOCAL_DEFINES	1
#include <qio.h>
#include <nsprintf.h>
#include <fsys.h>

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

#if USE_BUFFERED_IO
#include <errno.h>
#endif

#if defined(WDOG) && !NO_EER_WRITE && !NO_WDOG
# define KICK_WDOG() WDOG = 0
#else
# define KICK_WDOG() do { ; } while (0)
#endif

U32 NumLSN;

int WrFileSegment(void) {
    return 0;
}

int ide_check_devstat(void) {
    return 0;
}

#if SHIM_VFPRINTF_R
int _vfprintf_r (struct _reent *re, FILE *fp, const char *ctl, __VALIST va) {
    return 0;
}
#endif

#if SHIM_VFPRINTF
int vfprintf ( FILE *fp, const char *ctl, __VALIST va) {
    return 0;
}
#endif

int Read_Sectors(U32 * readbuf, U32 lsn, U32 num_sectors) {
    return 0;
}

int Write_Sectors(U32 * readbuf, U32 lsn, U32 num_sectors) {
    return 0;
}

void InitMole(void) {
    return;
}

void AllowMoleInGame(void) {
    return;
}

void ChangeToMole(void) {
    return;
}

void ForceMoleToStop(void) {
    return;
}

int HDStart(void) {
    static int been_here;
    extern void ide_init(void);
#if INCLUDE_HDDNL
    extern void hddnlInit(void);
#endif

    if (!been_here) {
	int sts;
	been_here = 1;
	ide_init();
	sts = fsys_mountw("/rd0", "/d0");
	if (QIO_ERR_CODE(sts)) {
	    char msg[AN_VIS_COL_MAX];
	    qio_errmsg(sts, msg, sizeof(msg));
	    for (sts=AN_VIS_ROW/2-2; sts < AN_VIS_ROW/2+4; ++sts) {
		txt_clr_wid(0, sts, AN_VIS_COL);
	    }
	    vid_clear();
	    prc_delay_options(7);
	    txt_str(-1, AN_VIS_ROW/2, "Error mounting filesystem", WHT_PAL);
	    txt_str(-1, AN_VIS_ROW/2+2, msg, YEL_PAL);
	    while (1) prc_delay(0);
	}
#if INCLUDE_HDDNL
	hddnlInit();
#endif
    }
    return 0;
}
	
static void dump_errors(const char *title, const char *name, int err) {
    int old;
    char emsg[AN_VIS_COL_MAX];

    qio_errmsg(err, emsg, sizeof(emsg));
    old = prc_delay_options(7);
    txt_str(1, AN_VIS_ROW/2, title, WHT_PAL);
    txt_cstr(": Error reading file:", WHT_PAL);
    txt_cstr(name, GRN_PAL);
    txt_str(1, AN_VIS_ROW/2+2, emsg, YEL_PAL);
    while (1) prc_delay(0);
    prc_delay_options(old);
    return;
}

static char *open_file(const char *path_str, const char *filename, QioIOQ *ioq,
    			FsysOpenT *ot, int mode) {
    int ii;
    char *s, *new;

    s = strchr(path_str, ':');
    if (s) path_str = s+1;
    ii = strlen(path_str) + strlen(filename) + 2 + 3 + 1;
    s = new = QIOmalloc(ii);
    if (!s) {
	ii = prc_delay_options(7);
	txt_str(1, AN_VIS_ROW/2, "open_file:", WHT_PAL);
	txt_cstr(" Ran out of memory while trying to open:", YEL_PAL);
	txt_str(1, AN_VIS_ROW/2+2, "/d0", GRN_PAL);
	txt_cstr(path_str, GRN_PAL);
	txt_cstr("/", GRN_PAL);
	txt_cstr(filename, GRN_PAL);
	while (1) prc_delay(0);
	prc_delay_options(ii);
    }	
    strcpy(s, "/d0");
    s += 3;
    if (*path_str != '/') *s++ = '/';
    strcpy(s, path_str);
    s += strlen(path_str);
    if (*filename != '/') *s++ = '/';
    strcpy(s, filename);
    ot->spc.path = new;
    ot->spc.mode = mode;
    ot->fid = 0;
    ii = qio_openspc(ioq, &ot->spc);
    while (!ii) {
	ii = ioq->iostatus;
	KICK_WDOG();
    }
    if (QIO_ERR_CODE(ioq->iostatus)) dump_errors("RdFile", new, ii);
    return new;
}

static void chk_size(int alloc, U32 max_len, char *task, char *name) {
    int old;
    if (alloc > max_len) {		/* oops, file won't fit into memory */
	old = prc_delay_options(7);
	txt_str(1, AN_VIS_ROW/2, task, WHT_PAL);
	txt_str(1, AN_VIS_ROW/2, " buffer too small for ", WHT_PAL);
	txt_cstr(name, GRN_PAL);
	txt_str(1, AN_VIS_ROW/2+2, "Available: ", WHT_PAL);
	txt_cdecnum(max_len, 9, RJ_BF, GRN_PAL);
	txt_cstr("  Need: ", WHT_PAL);
	txt_cdecnum(alloc, 9, RJ_BF, GRN_PAL);
	while (1) prc_delay(0);
	prc_delay_options(old);
    }
#if 0
    old = prc_delay_options(7);
    txt_str(1, AN_VIS_ROW/2, task, WHT_PAL);
    txt_cstr(" reading ", WHT_PAL);
    txt_cstr(name, GRN_PAL);
    txt_str(1, AN_VIS_ROW/2+2, "Available: ", WHT_PAL);
    txt_cdecnum(max_len, 9, RJ_BF, GRN_PAL);
    txt_cstr("  Need: ", WHT_PAL);
    txt_cdecnum(alloc, 9, RJ_BF, GRN_PAL);
    prc_delay(120);
    txt_clr_wid(0, AN_VIS_ROW/2, AN_VIS_COL);
    txt_clr_wid(0, AN_VIS_ROW/2+2, AN_VIS_COL);
    prc_delay_options(old);
#endif

}

#if INCLUDE_HDDNL
extern void hddnl_check(int);
#endif

int RdFile(const char * path_str, const char * filename, U32 max_len, U32 * dest) {
    QioIOQ *ioq;
    FsysOpenT opnt;
    char *b;
    int ii, accum=0;

#if INCLUDE_HDDNL
    hddnl_check(1);			/* say it's unsafe to do hddnl now */
#endif
    max_len &= -512;			/* round size down size to multiple of 512 */
    ioq = qio_getioq();
    ioq->timeout = 5000000;		/* roughly a 5 second timeout */
    b = open_file(path_str, filename, ioq, &opnt, O_RDONLY);
    if (max_len) chk_size(opnt.eof, max_len, "RdFile", b);
    while (1) {
	ii = qio_read(ioq, dest, opnt.eof);
	while (!ii) { 
	    ii = ioq->iostatus;
	    KICK_WDOG();
	}
	accum += ioq->iocount;
	if (QIO_ERR_CODE(ii)) {
	    if (ii == QIO_EOF) break;	/* EOF is a legit error to get */
#if (SST_GAME & (SST_MACE|SST_JUKO))
	    if (opnt.copies == 1) {
		accum += 512;			/* include bad sector in total */
		memset((char *)dest + ioq->iocount, 0, 512); /* zero the bad sector */
		dest += (ioq->iocount+512)/sizeof(U32);	/* advance pointer */
		opnt.eof -= ioq->iocount+512;	/* reduce total by how much we've read so far */
		if (opnt.eof <= 0) break;	/* done */
		ii = qio_lseek(ioq, accum, SEEK_SET); /* seek to one passed bad sector */
		while (!ii) { 
		    ii = ioq->iostatus;
		    KICK_WDOG();
		}
		if (!QIO_ERR_CODE(ii)) continue; /* if seek worked, continue */
	    }
#endif
	    dump_errors("RdFile", b, ii);
	}
	break;
    }
    ii = qio_close(ioq);
    while (!ii) { 
	KICK_WDOG();
	ii = ioq->iostatus;
    }
    if (QIO_ERR_CODE(ii)) {
#if (SST_GAME & (SST_MACE|SST_JUKO))
	if (opnt.copies > 1)
#endif
	    dump_errors("RdFile", b, ii);
    }
    qio_freeioq(ioq);
    QIOfree(b);
#if INCLUDE_HDDNL
    hddnl_check(0);			/* say it's safe to do hddnl now */
#endif
    return accum;
}

void WrFile(const char * path_str, const char * filename, U32 len, U32 * src) {
    QioIOQ *ioq;
    FsysOpenT opnt;
    char *b;
    int ii;

#if INCLUDE_HDDNL
    hddnl_check(1);			/* say it's unsafe to do hddnl now */
#endif
    ioq = qio_getioq();
    ioq->timeout = 5000000;		/* roughly a 5 second timeout */
    b = open_file(path_str, filename, ioq, &opnt, O_WRONLY);
    ii = qio_write(ioq, src, len);
    while (!ii) { 
	ii = ioq->iostatus;
	KICK_WDOG();
    }
    if (QIO_ERR_CODE(ioq->iostatus)) dump_errors("WrFile", b, ii);
    ii = ioq->iocount;
    qio_close(ioq);
    while (!ioq->iostatus) { 
	KICK_WDOG();
    }
    qio_freeioq(ioq);
    QIOfree(b);
#if INCLUDE_HDDNL
    hddnl_check(0);			/* say it's safe to do hddnl now */
#endif
    return;
}

/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
*
*       Read any section from a file
*         Upon entry: path_str -> path name to file
*                     filename -> name of file to read from
*                     start -> byte offset to start reading from
*                     max_len -> number of bytes to read
*                     dest -> pointer to mem space where data will be saved
*         Upon exit: returned -> number of bytes actually read               */

int read_file_section (const char * path_str, const char * filename, U32 start, U32 max_len, U32 * dest) {
    QioIOQ *ioq;
    FsysOpenT opnt;
    char *b;
    int ii, sts;

#if INCLUDE_HDDNL
    hddnl_check(1);			/* say it's unsafe to do hddnl now */
#endif
    ioq = qio_getioq();
    ioq->timeout = 5000000;		/* roughly 5 second timeout */
    b = open_file(path_str, filename, ioq, &opnt, O_RDONLY);

    if ((start&511)) {
#if USE_BUFFERED_IO && defined(JUNK_POOL_SIZE) && JUNK_POOL_SIZE
	FILE *ifp;
	ifp = fdopen(ioq->file, "r");
	ii = fseek(ifp, start, SEEK_SET);
	ii = fread((char *)dest, 1, max_len, ifp);
	if (ii < 0) {
#if (SST_GAME & (SST_MACE|SST_JUKO))
	    if (opnt.copies == 1) {
		ii = max_len;
	    } else {
#endif
		int old;
#if INCLUDE_HDDNL
		hddnl_check(0);			/* say it's safe to do hddnl now */
#endif
		old = prc_delay_options(7);
		txt_str(1, AN_VIS_ROW/2, "read_file_section - error reading file:", WHT_PAL);
		txt_str(1, AN_VIS_ROW/2+1, "While trying to read ", WHT_PAL);
		txt_str(1, AN_VIS_ROW/2+1, b, GRN_PAL);
		qio_errmsg(errno, (char *)dest, max_len);
		txt_str(1, AN_VIS_ROW/2+2, (char *)dest, YEL_PAL);
		while (1) prc_delay(0);
		prc_delay_options(old);
#if (SST_GAME & (SST_MACE|SST_JUKO))
	    }
#endif
	}
	fclose(ifp);
	ioq->complete = 0;
	qio_close(ioq);
	qio_freeioq(ioq);
	QIOfree(b);
#if INCLUDE_HDDNL
	hddnl_check(0);			/* say it's safe to do hddnl now */
#endif
	return ii;
#else
	int old;

#if INCLUDE_HDDNL
	hddnl_check(0);			/* say it's safe to do hddnl now */
#endif
	old = prc_delay_options(7);
	txt_str(1, AN_VIS_ROW/2, "RdFile - seek to non-sector aligned addr ", WHT_PAL);
	txt_cdecnum(start, 9, RJ_BF, RED_PAL);
	txt_str(1, AN_VIS_ROW/2+1, "While trying to read ", WHT_PAL);
	txt_cstr(b, GRN_PAL);
	while (1) prc_delay(0);
	prc_delay_options(old);
#endif
    }
    max_len &= -512;			/* round size down size to multiple of 512 */
    ii = qio_lseek(ioq, start, SEEK_SET);
    while (!ii) {
	ii = ioq->iostatus;
	KICK_WDOG();
    }
    if (QIO_ERR_CODE(ii)) {
#if (SST_GAME & (SST_MACE|SST_JUKO))
	if (opnt.copies > 1) 
#endif
	    dump_errors("read_file_section", b, ii);
    } else {
	int accum = 0;
	if (start < opnt.eof) {
	    if (max_len > opnt.eof-start) max_len = ((opnt.eof - start)+511)&-512;
	    while (1) {
		ii = qio_read(ioq, dest, max_len);
		while (!ii) {
		    ii = ioq->iostatus;
		    KICK_WDOG();
		}
		accum += ioq->iocount;
		if (QIO_ERR_CODE(ii)) {
		    if (ii == QIO_EOF) break;	/* EOF is a legit error */
#if (SST_GAME & (SST_MACE|SST_JUKO))
		    if (opnt.copies == 1) {
			accum += 512;		/* accumulate size of bad sector too */
			memset((char *)dest + ioq->iocount, 0, 512); /* zero the bad sector */
			dest += (ioq->iocount+512)/sizeof(U32);
			max_len -= ioq->iocount+512;	/* reduce max available */
			if (max_len <= 0) break;	/* done */
			ii = qio_lseek(ioq, accum + start, SEEK_SET);
			while (!ii) {
			    ii = ioq->iostatus;
			    KICK_WDOG();
			}
			if (!QIO_ERR_CODE(ii)) continue; /* keep reading */
		    }
#endif
		    dump_errors("read_file_section", b, ii);
		}
		break;
	    }
	}
	ii = accum;
    }
    sts = qio_close(ioq);
    while (!sts) { 
	sts = ioq->iostatus;
	KICK_WDOG();
    }
    if (QIO_ERR_CODE(sts)) {
#if (SST_GAME & (SST_MACE|SST_JUKO))
	if (opnt.copies > 1)
#endif
	    dump_errors("read_file_section", b, sts);
    }
    qio_freeioq(ioq);
    QIOfree(b);
#if INCLUDE_HDDNL
    hddnl_check(0);			/* say it's safe to do hddnl now */
#endif
    return ii;
}

