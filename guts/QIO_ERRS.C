/*
 *	errmsgs.c
 *
 *		Copyright 1996 Atari Games, Corp.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#include <os_proto.h>
#include <string.h>
#include <qio.h>
#if HOST_BOARD
# include <nsprintf.h>
#else
# include <stdio.h>
#endif

#ifndef n_elts
# define n_elts(x) (sizeof(x)/sizeof(x[0]))
#endif

typedef struct errmsg {
    int sts;
    const char *msg;
} ErrMsg;

static const ErrMsg errmsgs[] = {
#define QIO_ERR_MSG(name, init, text) { name, text }
#include <qio.h>
#undef QIO_ERR_MSG
    { 0, 0 }
};

/* NOTE: The order of the following must match exactly the
 * order as listed in the enum error_codes found in qio.h.
 * If not, then the facility name listed in message won't match
 * the faciltiy that actually generated the code .
 */

static const char * const facility[] = {
    0,
#define QIO_FACILITY_MSG(name,text) text
#include <qio.h>
#undef QIO_FACILITY_MSG
    0
};

static const char * const sev_msg[] = {
    " error, ",
    " fyi, ",
    " fatal, ",
    " warning, "
};

int qio_errmsg(int sts, char *ans, int size) {
    char *s;
    int ii, fac, sev, len;

    if (size && ans) {
#if !HOST_BOARD
	char tmp[80];
#endif
	ans[size-1] = 0;
	--size;
	fac = sts>>FACILITY_SHIFT;
	sev = (sts>>SEVERITY_SHIFT)&3;
	sts &= ~(3<<SEVERITY_SHIFT);
	if (fac > 0 && fac < n_elts(facility)-1) {
	    strncpy(ans, facility[fac], size);
        } else {
	    strncpy(ans, "???", size);
	}
	len = strlen(ans);
	s = ans + len;
	size -= len;
	if (size > 0) {
	    strncpy(s, sev_msg[sev], size);
	    len = strlen(s);
	    size -= len;
	    s += len;
	}
	if (size > 0) {
	    if (sts) {
		for (ii=0; ii < n_elts(errmsgs); ++ii) {
		    if (sts == errmsgs[ii].sts) {
			strncpy(s, errmsgs[ii].msg, size);
			return strlen(ans);
		    }
		}
	    }
#if HOST_BOARD
	    nsprintf(s, size, "Unknown status of 0x%08X", sts);
#else
	    sprintf(tmp, "Unknown status of %08X", sts);
	    strncpy(s, tmp, size-1);
	    s[size-1] = 0;
#endif
	}
    }
    return ans ? strlen(ans) : 0;
}


