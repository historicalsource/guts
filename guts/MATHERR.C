#include <config.h>
#include <os_proto.h>
#include <nsprintf.h>
#include <math.h>

#ifndef AN_VIS_COL_MAX
# define AN_VIS_COL_MAX AN_VIS_COL
#endif
#ifndef AN_VIS_ROW_MAX
# define AN_VIS_ROW_MAX AN_VIS_ROW
#endif

double __matherror (void *ptr, char *name, double arg1, double arg2, int type, double ret) {
	char err[AN_VIS_COL_MAX+1];
        struct ROM_VECTOR_STR *romv = (struct ROM_VECTOR_STR *)DRAM_BASEnc;

	switch(type) {
		case SING: 	nsprintf(err, sizeof(err), "%s(%g): SING error", name, arg1); break;
		case DOMAIN:	nsprintf(err, sizeof(err), "%s(%g): DOMAIN error", name, arg1); break;
		default:	nsprintf(err, sizeof(err), "%s(%g)",name,arg1); break;
	}

        if (romv->ROMV_STUB_INIT) {		/* if the stub is loaded, ... */
            while (1) __asm__("BREAK");
        } else {
	    prc_panic(err);
        }
	return 0.0;
}
