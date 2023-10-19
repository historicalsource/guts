/*
** Copyright (c) 1995, 3Dfx Interactive, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of 3Dfx Interactive, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of 3Dfx Interactive, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished  -
** rights reserved under the Copyright Laws of the United States.
**
** $Revision: 1.5 $
** $Date: 1997/09/05 02:42:00 $
*/

/* Fast memory test for FBI memory */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <sst.h>
#include <3dfx.h>
#include <glide.h>
#include <sst1init.h>
#include <fxpci.h>

int fbiMemErrors[2][4];

static void drawRect(FxU32 *, FxU32, FxU32, FxU32, FxU32, FxU32);
static void setAllBuffers(FxU32 *, FxU32, FxU32, FxU32);
static FxBool checkFbiMemConst(FxU32 *, FxU32, FxU32, FxU32);
static FxBool verifyFbiMemConst(FxU32 *, FxU32, FxU32, FxU32, FxU32);
static FxBool verifyFbiMemRndm(FxU32 *, FxU32, FxU32, FxU32, FxU32, FxU32);
static FxBool checkFbiMemWhite(FxU32 *, FxU32, FxU32);
static FxBool checkFbiMemBlack(FxU32 *, FxU32, FxU32);
static FxBool checkFbiMem55aa(FxU32 *, FxU32, FxU32);
static FxBool checkFbiMemaa55(FxU32 *, FxU32, FxU32);
static FxBool checkFbiMemFlatRndm(FxU32 *, FxU32, FxU32);
static FxBool checkFbiMemRndm(FxU32 *, FxU32, FxU32);
static int printFbiMemErrors();
extern FxBool initSST1( GrScreenResolution_t res, GrScreenRefresh_t refresh );
extern void sst1Idle(FxU32 *);

#define BIT_5TO8(val) ((val&0x1f)<<3) | ((val&0x1c)>>2)
#define BIT_6TO8(val) ((val&0x3f)<<2) | ((val&0x30)>>4)
#define COLOR16_TO_COLOR24(x)	((BIT_5TO8(x&0x1f)) | \
	 ((BIT_6TO8((x>>5)&0x3f))<<8) | \
	 ((BIT_5TO8((x>>11)&0x1f))<<16))

#define EXIT(X) return X
							
#ifndef REPORT_FBI_MEM_ERRORS
# define REPORT_FBI_MEM_ERRORS 1
#endif
#ifndef SCOPE_LOOP
# define SCOPE_LOOP REPORT_FBI_MEM_ERRORS
#endif

#if SCOPE_LOOP && !REPORT_FBI_MEM_ERRORS
# undef REPORT_FBI_MEM_ERRORS
# define REPORT_FBI_MEM_ERRORS 1
#endif

#if SCOPE_LOOP
#include <tdfx_diags.h>
#endif

int fbi_memtest(int argc, char **argv) {
    FxU32 *sstbase;
    volatile Sstregs *sst;
    sst1DeviceInfoStruct deviceInfo;
    int i, fbiMemSize = -1;
    FxU32 xDim=640, yDim=480;
    FxBool retVal = 0;

    for(i=1; i<argc; i++) {
	if(!strcmp(argv[i], "-2"))
	    fbiMemSize = 2;
	else if(!strcmp(argv[i], "-4"))
	    fbiMemSize = 4;
	else if(!strcmp(argv[i], "-1"))
	    fbiMemSize = 1;
	else if(!strcmp(argv[i], "-?")) {
	    printf("usage: %s [-1 -2 -4]\n", argv[0]);
	    printf("       Where 1, 2, or 4 selects the amount of frame buffer memory to test\n");
	    printf("       Default amount of frame buffer memory to test is auto-detected\n");
	    return 0;
	}
    }

#if GLIDE_VERSION > 203
    {
	extern void _GlideInitEnvironment(void);
	_GlideInitEnvironment();
    }
#ifdef DEFAULT_RES_METHOD
    DEFAULT_RES_METHOD();
#endif
#endif

    if(!(sstbase = sst1InitMapBoard(0))) {
	printf("ERROR: Could not map SST-1 board...\n");
	EXIT(1);
    }
    if(!(sst1InitRegisters(sstbase))) {
	printf("ERROR: Could not initialize SST-1 registers...\n");
	EXIT(1);
    }
    if(!(sst1InitGamma(sstbase, 1.0))) {
	printf("ERROR: Could not adjust gamma for SST-1 board...\n");
	EXIT(1);
    }
    sst = (Sstregs *) sstbase;
    if(sst1InitGetDeviceInfo(sstbase, &deviceInfo) == FXFALSE) {
	printf("ERROR: Could not get SST-1 Device Info...\n");
	sst1InitShutdown(sstbase);
	EXIT(1);
    }
    if(fbiMemSize < 0)
	fbiMemSize = (int) deviceInfo.fbiMemSize;

    if(fbiMemSize == 2) {
#if 1
	if(!(sst1InitVideo(sstbase, GR_RESOLUTION_640x480, GR_REFRESH_60Hz,
		  (sst1VideoTimingStruct *) NULL))) {
		printf("ERROR: Could not initialize SST-1 video...\n");
		EXIT(1);
	}
#else
	if (!initSST1( GR_RESOLUTION_640x480, GR_REFRESH_60Hz )) {
		printf("ERROR: Could not initialize SST-1 video...\n");
		EXIT(1);
	}
#endif
#if 0				/* preset above. This keeps the compiler quiet */
	xDim = 640;
	yDim = 480;
#endif
    } else if(fbiMemSize == 4) {
#if 1
	if(!(sst1InitVideo(sstbase, GR_RESOLUTION_800x600, GR_REFRESH_60Hz,
		  (sst1VideoTimingStruct *) NULL))) {
		printf("ERROR: Could not initialize SST-1 video...\n");
		EXIT(1);
	}
#else
	if (!initSST1( GR_RESOLUTION_800x600, GR_REFRESH_60Hz )) {
		printf("ERROR: Could not initialize SST-1 video...\n");
		EXIT(1);
	}
#endif
	xDim = 800;
	yDim = 600;
    } else if(fbiMemSize == 1) {
#if 1
	if(!(sst1InitVideo(sstbase, GR_RESOLUTION_512x384, GR_REFRESH_60Hz,
		  (sst1VideoTimingStruct *) NULL))) {
		printf("ERROR: Could not initialize SST-1 video...\n");
		EXIT(1);
	}
#else
	if (!initSST1( GR_RESOLUTION_512x384, GR_REFRESH_60Hz )) {
		printf("ERROR: Could not initialize SST-1 video...\n");
		EXIT(1);
	}
#endif
	xDim = 512;
	yDim = 384;
    } else {
	printf("ERROR: Only 1, 2 and 4 MBytes memory supported!\n");
	sst1InitShutdown(sstbase);
	EXIT(1);
    }

    /* Initialize error structure */
    memset((char *)fbiMemErrors, 0, sizeof(fbiMemErrors));

    /* Turn off memory fifo so that if FBI memory is corrupt the diag will */
    /* still run without hanging */
    sst1Idle(sstbase);
    sst->fbiInit0 &= ~SST_MEM_FIFO_EN;
    sst1Idle(sstbase);

    /* Force Banking for 4 MByte testing... */
    if(xDim == 800)
	sst->fbiInit2 |= (SST_DRAM_BANKING_CONFIG | SST_EN_DRAM_BANKED);

    printf("Checking %d MBytes of FBI Memory...\n", fbiMemSize);

    sst->fbzColorPath = SST_CC_MONE;
    sst->fogMode = 0x0;

    if(checkFbiMemBlack(sstbase, xDim, yDim) == FXFALSE)
	retVal |= 0x001;
    if(checkFbiMemWhite(sstbase, xDim, yDim) == FXFALSE)
	retVal |= 0x002;
    if(checkFbiMem55aa(sstbase, xDim, yDim) == FXFALSE)
	retVal |= 0x004;
    if(checkFbiMemaa55(sstbase, xDim, yDim) == FXFALSE)
	retVal |= 0x008;
    if(checkFbiMemFlatRndm(sstbase, xDim, yDim) == FXFALSE)
	retVal |= 0x010;
    if(checkFbiMemRndm(sstbase, xDim, yDim) == FXFALSE)
	retVal |= 0x020;

    sst1InitShutdown(sstbase);

    retVal |= printFbiMemErrors()<<8;

    return retVal;
}

#if REPORT_FBI_MEM_ERRORS || (HOST_BOARD != PHOENIX)
static const unsigned char coord[2][4] = {
    { 75, 76, 77, 74 },
    {  0,  0,  0,  0 }		/* none of our boards have a bank 1 */
};
#endif
#define NUM_FBI_BANKS 1

static int printFbiMemErrors(void) {
#if HOST_BOARD != PHOENIX
    int bank, bits, errs=0;
    for (bank=0; bank < NUM_FBI_BANKS; ++bank) {
	for (bits=0; bits < 4; ++bits) {
	    int amt;
	    if ((amt=fbiMemErrors[bank][bits])) {
#if NUM_FBI_BANKS > 1
		char mbnk[20];
		nsprintf(mbnk, sizeof(mbnk), ", bank %d.", bank);
#define BMSG mbnk
#else
#define BMSG "."
#endif
		printf("%4d%s Error%s in FBI DRAM data bits %02d:%02d%s Suspect chips U17 and/or U%02d\n",
		    amt > 9999 ? 9999 : amt, amt > 9999 ? "+":" ", amt > 1 ? "s":" ",
		    ((bits+1)<<4)-1, bits<<4, BMSG, coord[bank][bits]);
		++errs;
	    }
	}
    }
    return errs;
#else
    printf("\nFBI Error Summary:\n");
    printf("------------------\n");
    printf("fbiData[15:0]  --> Bank0: %d\t\tBank1: %d\n", fbiMemErrors[0][0], 
	    fbiMemErrors[1][0]);
    printf("fbiData[31:16] --> Bank0: %d\t\tBank1: %d\n", fbiMemErrors[0][1], 
	    fbiMemErrors[1][1]);
    printf("fbiData[47:32] --> Bank0: %d\t\tBank1: %d\n", fbiMemErrors[0][2], 
	    fbiMemErrors[1][2]);
    printf("fbiData[63:48] --> Bank0: %d\t\tBank1: %d\n", fbiMemErrors[0][3], 
	    fbiMemErrors[1][3]);
    return 0;
#endif
}

FxBool checkFbiMemWhite(FxU32 *sstbase, FxU32 xDim, FxU32 yDim)
{
    return(checkFbiMemConst(sstbase, xDim, yDim, 0xffffffff));
}

FxBool checkFbiMemBlack(FxU32 *sstbase, FxU32 xDim, FxU32 yDim)
{
    return(checkFbiMemConst(sstbase, xDim, yDim, 0x0));
}

FxBool checkFbiMem55aa(FxU32 *sstbase, FxU32 xDim, FxU32 yDim)
{
    return(checkFbiMemConst(sstbase, xDim, yDim, 0x55aa55aa));
}

FxBool checkFbiMemaa55(FxU32 *sstbase, FxU32 xDim, FxU32 yDim)
{
    return(checkFbiMemConst(sstbase, xDim, yDim, 0xaa55aa55));
}

FxBool checkFbiMemFlatRndm(FxU32 *sstbase, FxU32 xDim, FxU32 yDim)
{
    FxU32 i, dataExpect = 0xdead;
    FxBool retVal = FXTRUE;

    for(i=0; i<5; i++) {
	if(checkFbiMemConst(sstbase, xDim, yDim, dataExpect|(dataExpect<<16)) == FXFALSE)
		retVal = FXFALSE;
	dataExpect = (dataExpect + 0x5193) & 0xffff;
    }
    return(retVal);
}

FxBool checkFbiMemConst(FxU32 *sstbase, FxU32 xDim, FxU32 yDim, FxU32 color16)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;
    FxBool retVal = FXTRUE;

    setAllBuffers(sstbase, xDim, yDim, 0x0);
    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_FRONT;
    sst->zaColor = color16;
    drawRect(sstbase, 0, 0, xDim, yDim, COLOR16_TO_COLOR24(color16));
    if(verifyFbiMemConst(sstbase, xDim, yDim, 0, color16) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 1, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 2, 0x0) == FXFALSE)
	retVal = FXFALSE;

    setAllBuffers(sstbase, xDim, yDim, 0x0);
    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_BACK;
    drawRect(sstbase, 0, 0, xDim, yDim, COLOR16_TO_COLOR24(color16));
    if(verifyFbiMemConst(sstbase, xDim, yDim, 0, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 1, color16) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 2, 0x0) == FXFALSE)
	retVal = FXFALSE;

    setAllBuffers(sstbase, xDim, yDim, 0x0);
    sst->fbzMode = SST_ZAWRMASK | SST_DRAWBUFFER_FRONT;
    sst->zaColor = color16;
    drawRect(sstbase, 0, 0, xDim, yDim, 0xdeadbeef);
    if(verifyFbiMemConst(sstbase, xDim, yDim, 0, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 1, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 2, color16) == FXFALSE)
	retVal = FXFALSE;

    return retVal;
}

FxBool checkFbiMemRndm(FxU32 *sstbase, FxU32 xDim, FxU32 yDim)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;
    volatile unsigned long *lfbptr = (unsigned long *) sstbase;
    FxU32 x, y, data;
    FxBool retVal = FXTRUE;

    /* Test Buffer 0 */
    setAllBuffers(sstbase, xDim, yDim, 0x0);
    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_FRONT;
    sst->lfbMode = SST_LFB_565 | SST_LFB_WRITEFRONTBUFFER;
    sst1Idle(sstbase);

    data = 0xbaddead;
    for(y=0; y<yDim; y++) {
	for(x=0; x<xDim; x+=2) {
	    lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)] = data;
	    data += 0x34972195;
	}
    }
    sst1Idle(sstbase);
    if(verifyFbiMemRndm(sstbase, xDim, yDim, 0, 0xbaddead, 0x34972195) ==
      		FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 1, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 2, 0x0) == FXFALSE)
	retVal = FXFALSE;

    /* Test Buffer 1 */
    setAllBuffers(sstbase, xDim, yDim, 0x0);
    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_BACK;
    sst->lfbMode = SST_LFB_565 | SST_LFB_WRITEBACKBUFFER;
    sst1Idle(sstbase);

    data = 0xbaddead;
    for(y=0; y<yDim; y++) {
	for(x=0; x<xDim; x+=2) {
	    lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)] = data;
	    data += 0x74978193;
	}
    }
    sst1Idle(sstbase);
    if(verifyFbiMemConst(sstbase, xDim, yDim, 0, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemRndm(sstbase, xDim, yDim, 1, 0xbaddead, 0x74978193) ==
    	      FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 2, 0x0) == FXFALSE)
	retVal = FXFALSE;

    /* Test Buffer 2 */
    setAllBuffers(sstbase, xDim, yDim, 0x0);
    sst->fbzMode = SST_ZAWRMASK;
    sst->lfbMode = SST_LFB_ZZ;
    sst1Idle(sstbase);

    data = 0xabddadd;
    for(y=0; y<yDim; y++) {
	for(x=0; x<xDim; x+=2) {
	    lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)] = data;
	    data += 0x77976393;
	}
    }
    sst1Idle(sstbase);
    if(verifyFbiMemConst(sstbase, xDim, yDim, 0, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemConst(sstbase, xDim, yDim, 1, 0x0) == FXFALSE)
	retVal = FXFALSE;
    if(verifyFbiMemRndm(sstbase, xDim, yDim, 2, 0xabddadd, 0x77976393) ==
    	      FXFALSE)
	retVal = FXFALSE;

    return retVal;
}

void setAllBuffers(FxU32 *sstbase, FxU32 xDim, FxU32 yDim, FxU32 color16)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;

    sst->fbzMode = SST_RGBWRMASK | SST_ZAWRMASK | SST_DRAWBUFFER_FRONT;
    sst->zaColor = color16;
    drawRect(sstbase, 0, 0, xDim, yDim, COLOR16_TO_COLOR24(color16));
    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_BACK;
    drawRect(sstbase, 0, 0, xDim, yDim, COLOR16_TO_COLOR24(color16));
    sst1Idle(sstbase);
}

void drawRect(FxU32 *sstbase, FxU32 x, FxU32 y, FxU32 xsize, FxU32 ysize,
	FxU32 color)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;
    FxU32 left = x;
    FxU32 right = (x + xsize);
    FxU32 lowy = y;
    FxU32 highy = (y + ysize);

    sst->clipLeftRight = (left<<SST_CLIPLEFT_SHIFT) | right;
    sst->clipBottomTop = (lowy<<SST_CLIPBOTTOM_SHIFT) | highy;
    sst->c1 = color;
    sst->fastfillCMD = 0x0;
    sst1Idle(sstbase);
}

static void report_fbi_error(VU32 *lfbptr, FxU32 data, FxU32 dataExpect, int bank, int iLeave, int x, int y) {
    if (++fbiMemErrors[bank][iLeave] <= 4) {
	printf("Error at U%02d, bank=%d, x=%4d, y=%4d, addr: 0x%05X.\n"
	       "\tExpected 0x%04lX, read 0x%04lX, errbit(s)=0x%04lX\n",
	    coord[bank][iLeave],
	    bank, x, y, 
	    (y<<10)+x,
	    dataExpect,
	    data,
	    dataExpect ^ data);
	if (DONE_ONCE) scope_loop_q = 2;
	if (!scope_loop_q) {
	    fprintf(stderr, "Do you want to be asked to scope loop any errors? (Y/N): ");
	    if (testYesNo()) {
		scope_loop_q = 1;	/* always ask */
		fprintf(stderr, "Y\r\n");
	    } else {
		scope_loop_q = 2;	/* never ask */
		fprintf(stderr, "N\r\n");
	    }
	}
	if (scope_loop_q == 1) {
	    VU16 *vp;
	    extern void prc_wait_n_usecs(int);
	    vp = (VU16*)(lfbptr + ((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)) + (iLeave&1);
	    fprintf(stderr, "Do you want to scope loop writes to this address? (Y/N): ");
	    if (testYesNo()) {
		int loop=0;
		fprintf(stderr, "Y\r\nPress any key to abort loop and continue test...");
		while (1) {
		    *vp = dataExpect;
		    prc_wait_n_usecs(50);
		    ++loop;
		    if ((loop&0x65535)) {
			if (kbhit()) break;
		    }
		}
		getch();		/* eat the character he typed */
		fprintf(stderr, "\r\n");
	    } else {
		fprintf(stderr, "N\r\n");
	    }
	    fprintf(stderr, "Do you want to scope loop reads from this address? (Y/N): ");
	    if (testYesNo()) {
		int loop=0;
		fprintf(stderr, "Y\r\nPress any key to abort loop and continue test...");
		while (1) {
		    U32 junk;
		    junk = *vp;
		    prc_wait_n_usecs(50);
		    ++loop;
		    if ((loop&0x65535)) {
			if (kbhit()) break;
		    }
		}
		getch();		/* eat the character he typed */
		fprintf(stderr, "\r\n");
	    } else {
		fprintf(stderr, "N\r\n");
	    }
	}
    }
}

static FxBool verify(FxU32 *sstbase, FxU32 xDim, FxU32 yDim, FxU32 buffer, FxU32 dataExpect,
	    FxU32 data_upd, FxU32 (*expupd)(FxU32, FxU32)) {
    volatile unsigned long *lfbptr = (unsigned long *) sstbase;
    volatile Sstregs *sst = (Sstregs *) sstbase;
    FxU32 x, y, data;
    int row, interleave, rowStart;
    FxBool retVal = FXTRUE;

    if(buffer == 0)
	sst->lfbMode = SST_LFB_READFRONTBUFFER;
    else if(buffer == 1)
	sst->lfbMode = SST_LFB_READBACKBUFFER;
    else
	sst->lfbMode = SST_LFB_READDEPTHABUFFER;

    sst1Idle(sstbase);

    switch(xDim) {
	case 512:
	    row = buffer * 64;
	    break;
	case 640:
	    row = buffer * 150;
	    break;
	case 800:
	    row = buffer * 247;
	    break;
	default:
	    printf("verifyFbiMemConst: Unsupported xDim=%ld\n", xDim);
	    EXIT(1);
    }
    row--;
    rowStart = row;		/* keeps the compiler quiet */
    x = 0;			/* ditto, have no idea why */
    for(y=0; y<yDim; y++) {
	if(xDim == 800) {
	    interleave = (y & 0x10) ? 1 : 0;
	    if(!(y & 0xf)) {
		if(!(y & 0x1f)) {
		    row++;
		    rowStart = row;
		} else
		    rowStart = row;
	    } else
		row = rowStart;
	} else {
	    interleave = 0;
	    if(!(y & 0xf)) {
		row++;
		rowStart = row;
	    } else
		row = rowStart;
	}
	for(x=0; x<xDim; x+=2) {
	    if(!(x & 0x3f)) {
		if(x > 0) {
		    /* 64-pixel tile boundary */
		    /* Adjust running row and interleave counters */
		    interleave ^= 0x1;
		    if(xDim == 800) {
			if(((!(y & 0x10)) && !(x & 0x40)) ||
			    (( (y & 0x10)) &&  (x & 0x40)))
			    row++;
		    } else {
			if(!(x & 0x40))
			    row++;
		    }
		}
	    }
	    data = lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];

	    if (data != dataExpect) {
		int ileave, bank;
		bank = ((row > 511) ? 1 : 0);
		ileave = ((buffer < 2) ? interleave : (interleave ^ 0x1))<<1;
		if ((data&0xFFFF) != (dataExpect&0xFFFF)) {
		    report_fbi_error(lfbptr, data&0xFFFF, dataExpect, bank, ileave, x, y);
		}
		if (((data>>16)&0xFFFF) != ((dataExpect>>16)&0xFFFF)) {
		    report_fbi_error(lfbptr, (data>>16)&0xFFFF, (dataExpect>>16)&0xFFFF, bank, ileave+1, x, y);
		}
		retVal = FXFALSE;
	    }
	    if (expupd) dataExpect = expupd(dataExpect, data_upd);
	}
    }
    return retVal;
}

static FxU32 upd_rndm(FxU32 old, FxU32 new) {
    return old + new;
}

FxBool verifyFbiMemConst(FxU32 *sstbase, FxU32 xDim, FxU32 yDim, FxU32 buffer,
	FxU32 dataExpect)
{
    return verify(sstbase, xDim, yDim, buffer, dataExpect, 0, upd_rndm);
}

FxBool verifyFbiMemRndm(FxU32 *sstbase, FxU32 xDim, FxU32 yDim, FxU32 buffer,
	FxU32 dataExpect, FxU32 rndmAdd)
{
    return verify(sstbase, xDim, yDim, buffer, dataExpect, rndmAdd, upd_rndm);
}
