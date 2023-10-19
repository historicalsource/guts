#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: decompboot.c,v 1.10 1997/10/16 01:52:21 shepperd Exp $";
#endif

#include <config.h>
#include <os_proto.h>
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wms_proto.h>
#include <phx_proto.h>

#define report_error(x, y, z) do { ; } while(1)

const char boot_version[] = 
"\r\nEPROM Boot code. Version: " __DATE__ " " __TIME__ "\r\n"
"Copyright 1996,1997 Atari Games, Corp.\r\n";

#if !JUST_TEST
void z_error(const char *msg) {
    while (1) { ; }
}

voidpf zcalloc (voidpf opaque, unsigned int items, unsigned int size) {
    return (voidpf)calloc(items, size);
}

void  zcfree (voidpf opaque, voidpf ptr) {
    free(ptr);
    return;
}

/* ===========================================================================
     Decompresses the source buffer into the destination buffer.  sourceLen is
   the byte length of the source buffer. Upon entry, destLen is the total
   size of the destination buffer, which must be large enough to hold the
   entire uncompressed data. (The size of the uncompressed data must have
   been saved previously by the compressor and transmitted to the decompressor
   by some mechanism outside the scope of this compression library.)
   Upon exit, destLen is the actual size of the compressed buffer.
     This function can be used to decompress a whole file at once if the
   input file is mmap'ed.

     uncompress returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_BUF_ERROR if there was not enough room in the output
   buffer, or Z_DATA_ERROR if the input data was corrupted.
*/
static int uncomp (Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen) {
    z_stream stream;
    int err;

    memset((U8*)&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;

    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

    err = inflateInit(&stream);
    if (err != Z_OK) {
	report_error("uncomp", err, stream.msg);
	return err;
    }

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
	report_error("uncomp", err, stream.msg);
        inflateEnd(&stream);
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    if (err != Z_OK) report_error("uncomp", err, stream.msg);
    return err;
}

extern void malloc_init(void);
extern void copy_and_go(U32 *dst, U32 *src, U32 cnt);
extern int copy_and_go_size(void);
extern void flush_cache(void);

#define DREF_IT(x, y) \
    extern U8 y[], y ## _comp_size[], y ## _decomp_size[]; \
    static U8 * const x ## _ptr = y; \
    static const int x ## _d_size = (int) y ## _decomp_size; \
    static const int x ## _c_size = (int) y ## _comp_size

#if !BOOT_GAME_ONLY
DREF_IT(ep, epromst);		/* P2 = name of eprom selftest image variables */
#endif
#if !BOOT_GUTS_ONLY
DREF_IT(vm, vmunix);		/* P2 = name of game image variables */
#endif
#endif

void BootUp(void) {
#if !JUST_TEST
    int sts, copylen;
    uLongf dstlen, srclen;
    Bytef *dst, *copy_space;
    const Bytef *src;
    void (*copy)(U32 *dst, U32 *src, U32 cnt);

#if !BOOT_GUTS_ONLY && !BOOT_GAME_ONLY
#if HOST_BOARD == PHOENIX
   *(VU32*)IO_RESET = -1;      /* l=unleash the I/O ASIC */
   *(VU32*)PCI_RESET = -1;     /* unleash the PCI */
#endif
#if (HOST_BOARD == PHOENIX_AD) || (HOST_BOARD == SEATTLE) || (HOST_BOARD == VEGAS)
    /* Un-reset everything */
   *(VU32*)RESET_CTL = (1<<B_RESET_EXP)|(1<<B_RESET_IOASIC)|(1<<B_RESET_IDE)|
    			(1<<B_RESET_3DFX)|(1<<B_RESET_NSS)|(1<<B_RESET_WIDGET);
#endif
#if HOST_BOARD == FLAGSTAFF
    /* Un-reset everything */
   *(VU32*)RESET_CTL = (1<<B_RESET_EXP)|(1<<B_RESET_IOASIC)|(1<<B_RESET_IDE)|
    			(1<<B_RESET_3DFX)|(1<<B_RESET_ENET)|(1<<B_RESET_WIDGET);
#endif

    UnLock();			/* free up the I/O ASIC */

    prc_wait_n_usecs(1000000);	/* wait 1 second */

    if (!(*(VU32*)IO_DIPSW & IO_DIPSW1)) {
	src = (const Bytef *)ep_ptr;	/* source */
	srclen = (int)ep_c_size;	/* compressed length */
	dstlen = (int)ep_d_size;	/* decompressed length */
    } else {
	src = (const Bytef *)vm_ptr;	/* source */
	srclen = (int)vm_c_size;	/* compressed length */
	dstlen = (int)vm_d_size;	/* decompressed length */
    }
#else
# if BOOT_GAME_ONLY
    src = (const Bytef *)vm_ptr;	/* source */
    srclen = (int)vm_c_size;		/* compressed length */
    dstlen = (int)vm_d_size;		/* decompressed length */
# else
    src = (const Bytef *)ep_ptr;	/* source */
    srclen = (int)ep_c_size;		/* compressed length */
    dstlen = (int)ep_d_size;		/* decompressed length */
# endif
#endif

    malloc_init();
    dst = malloc(dstlen);
    sts = uncomp(dst, &dstlen, src, srclen);
    copylen = (copy_and_go_size()+3)&-4;
    copy_space = malloc(copylen);
    flush_cache();
    memcpy((U8*)((U32)copy_space|0x20000000), (char *)copy_and_go, copylen);
    copy = (void (*)(U32 *dst, U32 *src, U32 cnt))copy_space;
    copy((U32*)0xa0000000, (U32*)dst, dstlen); 
    while (1) { ; }
#else
    int walk=0;
    while (1) {
	*(VU32 *)LED_OUT = walk;
	++walk;
	prc_wait_n_usecs(500000);
    }
#endif
}

void __main() {
    return;
}

#if !NO_ST_GETENV
const struct st_envar *st_getenv(const char *name, const struct st_envar *hook) {
    return 0;
}
#endif
