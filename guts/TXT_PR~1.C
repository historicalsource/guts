/* $Id: txt_printf.c,v 1.4 1997/11/18 23:16:12 shepperd Exp $ */

#include <config.h>
#include <os_proto.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <nsprintf.h>

struct xypos {
    int row;
    int col;
    int attrib;
};

static int write_char(void *ptr, const char *str, int len) {
    struct xypos *xy = (struct xypos *)ptr;
    int olen = len;
    long newatt;
    char lbuff[AN_VIS_COL_MAX+1];

    if (len > 4 && str[0] == '%' && str[1] == '%') {
	char *s;
	newatt = strtol(str+2, &s, 16);
	if (s > str+2 && s-str+2 == len && s[0] == '%' && s[1] == '%') {
	    xy->attrib = newatt;
	    return 0;
	}
    }
    if (len > sizeof(lbuff)-1) len = sizeof(lbuff)-1;
    memcpy(lbuff, str, len);
    lbuff[len] = 0;
    txt_str(xy->col, xy->row, lbuff, xy->attrib);
    xy->col += txt_width(lbuff, xy->attrib);
    return olen;
}
    
int txt_printf(int col, int row, int attrib, const char *fmt, ...) {
    struct xypos xy;
    int ret;
    va_list ap;

    xy.row = row;
    xy.col = col >= 0 ? col : 0;
    xy.attrib = attrib;

    va_start(ap, fmt);
    ret = twi_vfprintf(write_char, (void *)&xy, 0, fmt, ap);
    va_end(ap);
    
    return ret;
}
