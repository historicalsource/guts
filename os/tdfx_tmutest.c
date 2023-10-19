/*
** Copyright (c) 1996, 3Dfx Interactive, Inc.
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
** $Revision: 1.15 $
** $Date: 1997/09/05 02:43:49 $
*/

/* Fast memory test for TMU memory */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nsprintf.h>
#include <phx_proto.h>

#undef FAIL
#include <sst.h>
#include <3dfx.h>
#include <sst1init.h>
#include <sst1test.h>

#define SSTVIDEOTIMINGSTRUCT	1	/* say this has already been defined */
#include <sst_tdfx.h>

static int tmuMemErrors[2][2][4], anyTmuErrors;

static void clearScreen(FxU32 *, FxU32);
static void drawRect(FxU32 *, FxU32, FxU32, FxU32, FxU32, FxU32);
static void drawRectUsingTris(FxU32 *, int x, int y, int);
static FxBool checkTmuMem(FxU32 *, int, int, FxU32, FxU32 (*)(FxU32), int);
static FxBool checkTmuMemWhite(FxU32 *, int, int);
static FxBool checkTmuMemBlack(FxU32 *, int, int);
static FxBool checkTmuMem55aa(FxU32 *, int, int);
static FxBool checkTmuMemaa55(FxU32 *, int, int);
static FxBool checkTmuMemRndm(FxU32 *, int, int);
static FxBool checkTmuMemFlatRndm(FxU32 *, int, int);
extern FxBool initSST1( GrScreenResolution_t res, GrScreenRefresh_t refresh );

#if !defined(SST_RESOLUTION)
# error Need to define SST_RESOLUTION
#endif

#ifndef REPORT_TMU_MEM_ERRORS
# define REPORT_TMU_MEM_ERRORS 1
#endif
#ifndef SCOPE_LOOP
# define SCOPE_LOOP REPORT_TMU_MEM_ERRORS
#endif

#if SCOPE_LOOP && !REPORT_TMU_MEM_ERRORS
# undef REPORT_TMU_MEM_ERRORS
# define REPORT_TMU_MEM_ERRORS 1
#endif

#if SCOPE_LOOP
#include <tdfx_diags.h>
int scope_loop_q;
#endif

#if HOLLER_DURING_TMUTEST
extern int shims_putc_disable;
extern void (*shims_putc_vec)(char c);
static void tmu_putc(char c) {
    fputc(c, stdout);
    return;
}
#endif

#if HOST_BOARD != PHOENIX
static const unsigned char coord[2][2][4] = {
    {
	{ 80, 81, 79, 78 },	/* TMU 0, BNK 0 */
	{ 84, 83, 82, 85 },	/* TMU 0, BNK 1 */
    },
    {
	{ 73, 88, 72, 71 },	/* TMU 1, BNK 0 */
	{ 94, 93, 92, 95 },	/* TMU 1, BNK 1 */
    }
};
static const unsigned char tmu_coord[2] = { 87, 97 };
#endif

void sst1Idle(FxU32 *sstbase)
{
    FxU32 cntr;
    U32 stime;
    volatile Sstregs *sst = (Sstregs *) sstbase;

#if GLIDE_VERSION > 203
    ISET(sst->nopCMD, 0x0);
#else
    sst->nopCMD = 0;
#endif
    cntr = 0;
    stime = prc_get_count();
    while(1) {
        if(!(sst1InitReturnStatus(sstbase) & SST_BUSY)) {
            if(++cntr >= 3)
                break;
        } else {
            cntr = 0;
	    if ((prc_get_count()-stime) > CPU_SPEED*4) {
		fprintf(stderr, "\r\nThe 3dfx chipset appears to be hung...");
		while (1) { ; }
	    }
	}	    
    }
}

int tmu_memtest (int argc, char **argv)
{
    int i;
    FxU32 *sstbase;
    volatile Sstregs *sst;
    sst1DeviceInfoStruct deviceInfo;
    int numberTmus = -1, tmuMemSize = -1;
    FxBool retVal;

    for(i=1; i<argc; i++) {
	if(!strcmp(argv[i], "-n"))
	    numberTmus = atoi(argv[++i]);
	else if(!strcmp(argv[i], "-t"))
	    tmuMemSize = atoi(argv[++i]);
	else if(!strcmp(argv[i], "-?")) {
	    printf("usage: %s [-n NumberOfTMUs] [-t TextureMemorySizeInMBytes]\n",
		    argv[0]);
	    printf("       Default number of TMUs is 1\n");
	    printf("       Default Texture Memory is 2 MBytes\n");
	    return 0;
	}
    }

#if HOLLER_DURING_TMUTEST
shims_putc_disable = 0;		/* enable startup blabbing */
shims_putc_vec = tmu_putc;
#endif

#if GLIDE_VERSION > 203
    {
	extern void _GlideInitEnvironment(void);
	_GlideInitEnvironment();
    }
#endif

#ifdef DEFAULT_RES_METHOD
    DEFAULT_RES_METHOD();
#endif

    if(!(sstbase = sst1InitMapBoard(0))) {
	printf("ERROR: Could not map SST-1 board...\n");
	return 1;
    }
    if(!(sst1InitRegisters(sstbase))) {
	printf("ERROR: Could not initialize SST-1 registers...\n");
	return 1;
    }
    if(!(sst1InitGamma(sstbase, 1.0))) {
	printf("ERROR: Could not adjust gamma for SST-1 board...\n");
	return 1;
    }
    sst = (Sstregs *) sstbase;
    if(sst1InitGetDeviceInfo(sstbase, &deviceInfo) == FXFALSE) {
	printf("ERROR: Could not get SST-1 Device Info...\n");
	return 1;
    }
    if(numberTmus < 0) numberTmus = (int) deviceInfo.numberTmus;
    if(tmuMemSize < 0) tmuMemSize = (int) deviceInfo.tmuMemSize[0];

#if 0
    if(!(sst1InitVideo(sstbase, SST_RESOLUTION /* GR_RESOLUTION_640x480 */, GR_REFRESH_60Hz,
	      (sst1VideoTimingStruct *) NULL))) {
	printf("ERROR: Could not initialize SST-1 video...\n");
	return 1;
    }
#else
    if (!initSST1( SST_RESOLUTION, GR_REFRESH_60Hz )) {
	printf("ERROR: Could not initialize SST-1 video...\n");
	return 1;
    }
#endif
    /* Turn off memory fifo so that if FBI memory is corrupt the diag will */
    /* still run without hanging */
    sst1Idle(sstbase);
    sst->fbiInit0 &= ~SST_MEM_FIFO_EN;
    sst1Idle(sstbase);

    /* Initialize error structure */
    memset((char *)tmuMemErrors, 0, sizeof(tmuMemErrors));
    anyTmuErrors = 0;

    printf("Checking %d TMU%swith %d MBytes memory %s...\n", 
	numberTmus, numberTmus > 1 ? "s " : " ",
	tmuMemSize,
	numberTmus > 1 ? "each" : "");

    checkTmuMemBlack(sstbase, numberTmus, tmuMemSize);
    checkTmuMemWhite(sstbase, numberTmus, tmuMemSize);
    checkTmuMemaa55(sstbase, numberTmus, tmuMemSize);
    checkTmuMem55aa(sstbase, numberTmus, tmuMemSize);
#if !COBBLE_TMU_TEST
# define RANDOM_TMU_TEST numberTmus
#else
# define RANDOM_TMU_TEST 1
#endif
    checkTmuMemFlatRndm(sstbase, RANDOM_TMU_TEST, tmuMemSize);
    checkTmuMemRndm(sstbase, RANDOM_TMU_TEST, tmuMemSize);

    retVal = 0;
    if (anyTmuErrors) {
#if HOST_BOARD != PHOENIX
	retVal = 1;
	for (i=0; i < numberTmus; ++i) {
	    int j, k;
	    for (j=0; j < 2; ++j) {
		for (k=0; k < 4; ++k) {
		    int amt;
		    if ((amt=tmuMemErrors[i][j][k])) {
			printf("%4d%s Error%s in TMU %d, bnk %d, addr grp %d. Suspect chips at U%02d and/or U%02d\n",
    				amt > 9999 ? 9999 : amt, amt > 9999 ? "+":" ", amt > 1 ? "s":" ",
    				i, j, k, tmu_coord[i], coord[i][j][k]);
		    }
		}
	    }
	}
#else
	printf("\nTMU0 Error Summary:\n");
	printf("-------------------\n");
	printf("tex_data_0 --> Bank0: %d\t\tBank1: %d\n", tmuMemErrors[0][0][0], 
		tmuMemErrors[0][1][0]);
	printf("tex_data_1 --> Bank0: %d\t\tBank1: %d\n", tmuMemErrors[0][0][1], 
		tmuMemErrors[0][1][1]);
	printf("tex_data_2 --> Bank0: %d\t\tBank1: %d\n", tmuMemErrors[0][0][2], 
		tmuMemErrors[0][1][2]);
	printf("tex_data_3 --> Bank0: %d\t\tBank1: %d\n", tmuMemErrors[0][0][3], 
		tmuMemErrors[0][1][3]);
	if(numberTmus > 1) {
	    printf("\nTMU1 Error Summary:\n");
	    printf("-------------------\n");
	    printf("tex_data_0 --> Bank0: %d\t\tBank1: %d\n", 
		    tmuMemErrors[1][0][0], tmuMemErrors[1][1][0]);
	    printf("tex_data_1 --> Bank0: %d\t\tBank1: %d\n", 
		    tmuMemErrors[1][0][1], tmuMemErrors[1][1][1]);
	    printf("tex_data_2 --> Bank0: %d\t\tBank1: %d\n", 
		    tmuMemErrors[1][0][2], tmuMemErrors[1][1][2]);
	    printf("tex_data_3 --> Bank0: %d\t\tBank1: %d\n", 
		    tmuMemErrors[1][0][3], tmuMemErrors[1][1][3]);
	}
#endif
    }

#if HOLLER_DURING_TMUTEST
shims_putc_disable = 1;		/* turn off blabbing again */
shims_putc_vec = 0;
#endif

    return retVal;
}

static void clearScreen(FxU32 *sstbase, FxU32 color)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;

    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_FRONT;
    drawRect(sstbase, 0, 0, VIS_H_PIX, VIS_V_PIX, color);
}

static void drawRect(FxU32 *sstbase, FxU32 x, FxU32 y, FxU32 xsize, FxU32 ysize,
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

static void drawRectUsingTris(FxU32 *sstbase, int x, int y, int tSize)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;

    sst->vA.x = (x<<SST_XY_FRACBITS);
    sst->vA.y = (y<<SST_XY_FRACBITS);
    sst->vB.x = ((x+tSize)<<SST_XY_FRACBITS);
    sst->vB.y = (y<<SST_XY_FRACBITS);
    sst->vC.x = ((x+tSize)<<SST_XY_FRACBITS);
    sst->vC.y = ((y+tSize)<<SST_XY_FRACBITS);
    sst->s = 0;
    sst->t = 0;
    sst->w = 0;
    sst->r = (0xff<<SST_XY_INTBITS);
    sst->g = 0;
    sst->b = 0;
    sst->dsdx = (0x1<<SST_ST_FRACBITS);
    sst->dtdx = 0;
    sst->dwdx = 0;
    sst->dsdy = 0;
    sst->dtdy = (0x1<<SST_ST_FRACBITS);
    sst->dwdy = 0;
    sst->triangleCMD = 0;
    sst->vB.x = (x<<SST_XY_FRACBITS);
    sst->vB.y = ((y+tSize)<<SST_XY_FRACBITS);
    sst->triangleCMD = 0xFFFFFFFF;
}

static FxBool checkTmuMemBlack(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    return(checkTmuMem(sstbase, numberTmus, tmuMemSize, 0x0, 0, 0));
}

static FxBool checkTmuMemWhite(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    return(checkTmuMem(sstbase, numberTmus, tmuMemSize, 0xffffffff, 0, 1));
}

static FxBool checkTmuMemaa55(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    return(checkTmuMem(sstbase, numberTmus, tmuMemSize, 0xaa5555aa, 0, 2));
}

static FxBool checkTmuMem55aa(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    return(checkTmuMem(sstbase, numberTmus, tmuMemSize, 0x55aaaa55, 0, 3));
}

static FxBool checkTmuMemFlatRndm(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    FxU32 i, dataExpect = 0xbaddead;
    FxBool retVal = FXTRUE;

    for(i=0; i<5; i++) {
	if(checkTmuMem(sstbase, numberTmus, tmuMemSize, dataExpect, 0, 4) ==
	  FXFALSE)
		retVal = FXFALSE;
	dataExpect += 0x74978193;
    }

    return(retVal);
}

static FxU32 rndm(FxU32 dataExpect) {
    return dataExpect += 0x34972195;
}

static FxBool checkTmuMemRndm(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    FxBool retVal = FXTRUE;

    if (checkTmuMem(sstbase, numberTmus, tmuMemSize, 0x0BADDEAD, rndm, 5) ==
      FXFALSE)
	    retVal = FXFALSE;

    return(retVal);
}

#if REPORT_TMU_MEM_ERRORS
static void report_tmu_error(volatile Sstregs *sst, FxU32 dataExpect, FxU32 dataRead, int numTmu, int x, int y, int bank, int offset) {
    FxU32 errbits = dataExpect^dataRead;
    volatile FxU32 *texAddr;

    fprintf(stderr, "At U%2d addr=0x%05X, bank %d, offset %d, x %3d, y %3d\r\n"
	            "\tExpect 0x%04lX, read 0x%04lX, err bit%s0x%04lX\r\n",
	coord[numTmu][bank][offset], ((y<<8)+x)<<1, bank, offset, x, y,
	dataExpect, dataRead, ((errbits&-errbits)==errbits) ? "= " : "s=", errbits);
# if SCOPE_LOOP
    if (DONE_ONCE) scope_loop_q = 2;		/* don't ask on subsequent passes */
    if (scope_loop_q == 0) {
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
	fprintf(stderr, "Do you want to scope loop writes to this address? (Y/N): ");
	if (testYesNo()) {
	    FxU32 what;
	    int loop=0;
	    fprintf(stderr, "Y\r\nPress any key to abort loop and continue test...");
	    texAddr = (numTmu<<(21-2)) + (FxU32 *)SST_TEX_ADDRESS(sst) + (y*0x200/4);
	    what = (offset&1) ? (dataExpect<<16) : dataExpect;
	    while (1) {
		texAddr[(x>>1)] = what;
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
		drawRectUsingTris((FxU32 *)sst, 0, 0, 256);
		sst1Idle((FxU32 *)sst);
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
# endif
}
#endif

static const char * const test_names[] = {
    "All 0's",
    "All 1's",
    "0xAA5555AA",
    "0x55AAAA55",
    "Addresses (+random #)",
    "Random numbers"
};

static FxBool checkTmuMem(FxU32 *sstbase, int numberTmus, int tmuMemSize,
	FxU32 dataExpect, FxU32 (*expupd)(FxU32), int which_test)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;
    FxBool retVal = FXTRUE;
    int numTmu;
    FxU32 texBaseAddr;
    volatile FxU32 *lfbAddr, *texAddr;
    FxU32 x, y, i, dataRead;

    for(numTmu = 0; numTmu<numberTmus; numTmu++) {
	for(texBaseAddr = 0; texBaseAddr < ((FxU32 ) tmuMemSize << 20);
		texBaseAddr += (1<<17)) {
	    /*
	    printf("texBaseAddr = 0x%x, 0x%x\n", texBaseAddr, (tmuMemSize<<20));
	    */

	    clearScreen(sstbase, 0x0);

	    for(i=0; i<2; i++) {
		if(!i) {
		    /* Fill Texture with psuedo-Random Values */
		    sst->textureMode = SST_RGB565 | SST_TC_REPLACE | SST_TCA_ZERO;
		    sst->tLOD = 0x0;
		    sst->tDetail = 0x0;
		    sst->texBaseAddr = (texBaseAddr>>3);
		    texAddr = (numTmu<<(21-2)) + (FxU32 *)SST_TEX_ADDRESS(sst);
		    for(y=0; y<256; y++) {
			for(x=0; x<256;x+=2) {
			    texAddr[(y<<7)+(x>>1)] = (y<<8) + x;
			}
			texAddr += (0x200 >> 2);
		    }
		} else {
		    FxU32 data = dataExpect;
		    /* Download Texture */
		    sst->textureMode = SST_RGB565 | SST_TC_REPLACE | SST_TCA_ZERO;
		    sst->tLOD = 0x0;
		    sst->tDetail = 0x0;
		    sst->texBaseAddr = (texBaseAddr>>3);
		    texAddr = (numTmu<<(21-2)) + (FxU32 *)SST_TEX_ADDRESS(sst);
		    for(y=0; y<256; y++) {
			for(x=0; x<256; x+=2) {
			    texAddr[(x>>1)] = data;
			    if (expupd) data = expupd(data);
			}
			texAddr += (0x200 >> 2);
		    }
		}
	    }

	    /* Render Rectangle for testing (two tris) */
	    if(numTmu == 0) {
		sst->textureMode = SST_RGB565 | SST_TC_REPLACE | SST_TCA_ZERO;
	    } else if(numTmu == 1) {
		/* Force downstream TMU to passthrough upstream data */
		SST_TREX(sst,0)->textureMode = SST_RGB565 | SST_TC_PASS |
		  SST_TCA_PASS;
		SST_TREX(sst,1)->textureMode = SST_RGB565 | SST_TC_REPLACE |
		  SST_TCA_ZERO;
	    }
	    sst->tLOD = 0x0;
	    sst->tDetail = 0x0;
	    sst->texBaseAddr = (texBaseAddr>>3);
	    sst->fogMode = 0x0;
	    sst->alphaMode = 0x0;
	    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_FRONT;
	    sst->fbzColorPath = SST_RGBSEL_TREXOUT | SST_CC_PASS |
		    SST_ENTEXTUREMAP;
	    drawRectUsingTris(sstbase, 0, 0, 256);
	    sst1Idle(sstbase);

	    sst->lfbMode = SST_LFB_565 | SST_LFB_READFRONTBUFFER;
	    sst1Idle(sstbase);
	    lfbAddr = (unsigned long *) sstbase;
	    for(y=0; y<256; y++) {
		for(x=0; x<256; x+=2) {
		    dataRead = lfbAddr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];
		    if(dataRead != dataExpect) {
			FxU32 bank = (texBaseAddr < (2<<20)) ? 0 : 1;
			FxU32 offset = (y & 1) ? 2 : 0;
			int even=0, odd=0;
			retVal = FXFALSE;
			if ((dataRead&0xffff) != (dataExpect&0xffff)) {
			    even = ++tmuMemErrors[numTmu][bank][offset];
			}
			if(((dataRead>>16)&0xffff) != ((dataExpect>>16)&0xffff)) {
			    odd = ++tmuMemErrors[numTmu][bank][offset+1];
			}
#if REPORT_TMU_MEM_ERRORS
			if (!even && !odd) {
			    fprintf(stderr, "TMU %d failed test %d (%s)...\r\n", 
				numTmu, which_test, test_names[which_test]);
			}
			if (even && even < 5) {
			    report_tmu_error(sst, dataExpect&0xFFFF, dataRead&0xFFFF, numTmu, x, y, bank, offset);
			}			    			    
			if (odd && odd < 5) {
			    report_tmu_error(sst, (dataExpect>>16)&0xFFFF, (dataRead>>16)&0xFFFF,
					numTmu, x, y, bank, offset+1);
			}			    			    
#endif
			++anyTmuErrors;
		    }
		    if (expupd) dataExpect = expupd(dataExpect);
		}
	    }
	}
    }
    return retVal;
}

#if 0
static FxBool checkTmuMemRndm(FxU32 *sstbase, int numberTmus, int tmuMemSize)
{
    volatile Sstregs *sst = (Sstregs *) sstbase;
    FxBool retVal = FXTRUE;
    int numTmu;
#if REPORT_TMU_MEM_ERRORS
    int numerrors;
#endif
    FxU32 texBaseAddr;
    volatile FxU32 *lfbAddr, *texAddr;
    FxU32 x, y, i, dataRead, dataExpect;

    for(numTmu = 0; numTmu<numberTmus; numTmu++) {
#if REPORT_TMU_MEM_ERRORS
	numerrors = 0;
#endif
	for(texBaseAddr = 0; texBaseAddr < ((FxU32 ) tmuMemSize << 20);
		  texBaseAddr+=(1<<17)) {
	    /*
	    printf("texBaseAddr = 0x%x, 0x%x\n", texBaseAddr, (tmuMemSize<<20)); 
	    */

	    clearScreen(sstbase, 0x0);

	    for(i=0; i<2; i++) {
		if(!i) {
		    /* Fill Texture with Random Values */
		    sst->textureMode = SST_RGB565 | SST_TC_REPLACE | SST_TCA_ZERO;
		    sst->tLOD = 0x0;
		    sst->tDetail = 0x0;
		    sst->texBaseAddr = (texBaseAddr>>3);
		    texAddr = (numTmu<<(21-2)) + (FxU32 *)SST_TEX_ADDRESS(sst);
		    for(y=0; y<256; y++) {
			for(x=0; x<256;x+=2) {
			    texAddr[(y<<7)+(x>>1)] = (y<<8) + x;
			}
			texAddr += (0x200 >> 2);
		    }
		} else {
		    /* Download Texture */
		    sst->textureMode = SST_RGB565 | SST_TC_REPLACE | SST_TCA_ZERO;
		    sst->tLOD = 0x0;
		    sst->tDetail = 0x0;
		    sst->texBaseAddr = (texBaseAddr>>3);
		    texAddr = (numTmu<<(21-2)) + (FxU32 *)SST_TEX_ADDRESS(sst);
		    dataExpect = 0xbaddead;
		    for(y=0; y<256; y++) {
			for(x=0; x<256; x+=2) {
			    texAddr[(x>>1)] = dataExpect;
			    dataExpect += 0x34972195;
			}
			texAddr += (0x200 >> 2);
		    }
		}
	    }

	    /* Render Rectangle for testing (two tris) */
	    if(numTmu == 0) {
		sst->textureMode = SST_RGB565 | SST_TC_REPLACE | SST_TCA_ZERO;
	    } else if(numTmu == 1) {
		/* Force downstream TMU to passthrough upstream data */
		SST_TREX(sst,0)->textureMode = SST_RGB565 | SST_TC_PASS |
		  SST_TCA_PASS;
		SST_TREX(sst,1)->textureMode = SST_RGB565 | SST_TC_REPLACE |
		  SST_TCA_ZERO;
	    }
	    sst->tLOD = 0x0;
	    sst->tDetail = 0x0;
	    sst->texBaseAddr = (texBaseAddr>>3);
	    sst->fogMode = 0x0;
	    sst->alphaMode = 0x0;
	    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_FRONT;
	    sst->fbzColorPath = SST_RGBSEL_TREXOUT | SST_CC_PASS |
		    SST_ENTEXTUREMAP;
	    drawRectUsingTris(sstbase, 0, 0, 256);
	    sst1Idle(sstbase);

	    sst->lfbMode = SST_LFB_565 | SST_LFB_READFRONTBUFFER;
	    sst1Idle(sstbase);
	    lfbAddr = (unsigned long *) sstbase;
	    dataExpect = 0xbaddead;
	    for(y=0; y<256; y++) {
		for(x=0; x<256; x+=2) {
		    dataRead = lfbAddr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];
		    if(dataRead != dataExpect) {
			FxU32 bank = (texBaseAddr < (2<<20)) ? 0 : 1;
			FxU32 offset = (y & 1) ? 2 : 0;
			retVal = FXFALSE;
#if REPORT_TMU_MEM_ERRORS
if (numerrors++ < 8) {
    printf("At TMU %d, bank %ld, offset %ld, x %3ld, y %3ld, Expect 0x%08lX, read 0x%08lX\n",
    	numTmu, bank, offset, x, y, dataExpect, dataRead);
}
#endif
			++anyTmuErrors;
			if((dataRead&0xffff) != (dataExpect&0xffff)) {
			    tmuMemErrors[numTmu][bank][offset]++;
			}
			if(((dataRead>>16)&0xffff) != ((dataExpect>>16)&0xffff)) {
			    tmuMemErrors[numTmu][bank][offset+1]++;
			}
		    }
		    dataExpect += 0x34972195;
		}
	    }
	}
    }
    return retVal;
}
#endif
