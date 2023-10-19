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
**
** $Revision: 1.14 $ 
** $Date: 1997/09/05 02:42:57 $ 
**
*/

#include <config.h>
#include <os_proto.h>
#include <phx_proto.h>
#include <nsprintf.h>

#undef FAIL
#define n_elts(x) (sizeof(x)/sizeof(x[0]))

#define QUIET 1

#define PAUSE() do { /* testYesNo() */ ; } while (0)

#include "sst1test.h"
#define SSTVIDEOTIMINGSTRUCT	1	/* say this has already been defined */
#include <sst_tdfx.h>

#ifndef DIAGS_ROOT
# define DIAGS_ROOT "/d0/diags/"
#endif

#define MAX_F_X ((float)VIS_H_PIX)
#define MAX_F_Y ((float)VIS_V_PIX)
#define MAX_X VIS_H_PIX
#define MAX_Y VIS_V_PIX

/* Locals */
static TestFunction _tests[MAX_NUM_TESTS];

#if !QUIET
static Gu3dfInfo _texInfo;
#endif
FxU16     _texPtr[256][256];
FxU16     _region[256][256];

static FxU32     _width, _height;

static GrState _state;
static Texture *_decal;
static Texture *_alpha;
static Texture *_marble;
static Texture *_palette;
static Texture *_yiq;

extern void sleep(int);
extern void sst1Idle(FxU32 *);

/* PCI Register definitions for Direct Access To Hardware */
const PciRegister PCI_INIT_ENABLE  = { 0x40, 4, READ_WRITE };
const PciRegister PCI_BUS_SNOOP0   = { 0x44, 4, WRITE_ONLY };
const PciRegister PCI_BUS_SNOOP1   = { 0x48, 4, WRITE_ONLY };
const PciRegister PCI_CFG_STATUS   = { 0x4C, 4, READ_ONLY };

/* Buffer for loading test images */
static Buffer _buffer;
FxU16  _imgData[VIS_H_PIX_MAX*VIS_V_PIX_MAX];

static Buffer *frontBuffer;
static Buffer *backBuffer;
static Buffer *auxBuffer;


static FxBool initSST1_org( GrScreenResolution_t res, GrScreenRefresh_t refresh, 
    			    GrOriginLocation_t org, GrOriginLocation_t lfborg);
FxBool initSST1( GrScreenResolution_t res, GrScreenRefresh_t refresh );
void   shutdownSST1( void );
void grabRect( void *memory, 
               FxU32 minx, FxU32 miny, 
               FxU32 maxx, FxU32 maxy );
Buffer   *allocBuffer( FxU32 width, FxU32 height );
Buffer   *readImage( const char *file );
FxBool   writeImage( const Buffer *img, const char *file );
FxBool   grabImage( Buffer *img, GrBuffer_t buffer );
FxBool   pasteImage( const Buffer *img, GrBuffer_t buffer );
FxU32    compareImage( const Buffer *a, const Buffer *b );
TestCode testColorChecksum( GrScreenResolution_t res, 
                            char *image, 
                            FxBool reference );
static void clearBuffers( void );
static TestCode testBuffers( const char *front, const char *aux, FxBool reference );

void testCreateTestArray( FxU32              *nTests, 
                          TestFunction       *testArray[],
                          TestConfig         *config,
                          FxBool             reference ) {

    TestPacket packet;
    packet.tmu       = 0;
    packet.reference = reference;
    packet.config    = config;
    packet.result    = NOT_TESTED;
    packet.initHardware = FXTRUE;
    packet.shutdownHardware = FXTRUE;
    *nTests          = 0;

    *testArray = _tests;

#define ADD_TEST( X ) {                       \
    _tests[ *nTests ].name    = #X ;        \
    _tests[ *nTests ] .test   = test ## X ; \
    _tests[ *nTests ].packet  = packet;     \
    (*nTests)++; }

    /*----------------------------------------------------------
      Based on board configuration, create a stream of tests
      to execute.
      ----------------------------------------------------------*/

    if ( !config->sli ) {
        ADD_TEST( PCI );
        ADD_TEST( CONFIG );
        ADD_TEST( DAC512 );
        ADD_TEST( CHK512 );
        packet.initHardware = FXTRUE;
        packet.shutdownHardware = FXFALSE;
        ADD_TEST( GLIDE00 );
        packet.initHardware = FXFALSE;
        ADD_TEST( GLIDE01 );
        ADD_TEST( GLIDE02 );
        ADD_TEST( GLIDE03 );
        ADD_TEST( GLIDE04 );
        ADD_TEST( GLIDE05 );
        ADD_TEST( GLIDE06 );
        ADD_TEST( GLIDE07 );
        ADD_TEST( GLIDE08 );
        ADD_TEST( GLIDE09 );
        packet.shutdownHardware = FXTRUE;
        ADD_TEST( GLIDE10 );
        packet.initHardware = FXTRUE;
        if ( reference ) return;
        if ( config->nTMUs == 2 ) {
            packet.tmu = 1;
            packet.initHardware = FXTRUE;
            packet.shutdownHardware = FXFALSE;
            ADD_TEST( GLIDE00 );
            ADD_TEST( GLIDE02 );
            ADD_TEST( GLIDE04 );
            ADD_TEST( GLIDE05 );
            ADD_TEST( GLIDE07 );
            packet.shutdownHardware = FXTRUE;
            ADD_TEST( GLIDE10 );
            packet.initHardware = FXTRUE;
        }
    }
    return;
#undef ADD_TEST
}

void testClearArray( FxU32 nTests, TestFunction testArray[] ) {
    FxU32 index;
    for( index = 0; index < nTests; index++ ) {
        testArray[index].packet.result = NOT_TESTED;
    }
}


#define BEGIN_TEST( X )                      \
    void test ## X ( TestPacket *packet ) {  \
        const char *TEST_FRONT = DIAGS_ROOT #X "f.sbi"; \
        const char *TEST_AUX   = DIAGS_ROOT #X "a.sbi"; \
        TEST_FRONT = TEST_FRONT;             \
        TEST_AUX = TEST_AUX;                 \
        {

#define END_TEST }}
#define TEST_EXIT( CODE ) { packet->result = CODE; return; }

/*------------------------------------------------------------------
  Test Definitions
  ------------------------------------------------------------------*/

#define INITVIDEO_EXTRA ,0

BEGIN_TEST( PCI ) 
    FxU32   *base;
    volatile Sstregs *baseRegs;
    FxU32 alphaMode, stipple, c0, c1;
    const FxU32 oddEvenOdd  = 0x55aaaa55;
    const FxU32 evenOddEven = 0xaa5555aa;

    base     = sst1InitMapBoard(0);
    if( !base || 
        !sst1InitRegisters(base) || 
        !sst1InitGamma( base, 1.0 ) ||
        !sst1InitVideo(base, SST_RESOLUTION, GR_REFRESH_60Hz INITVIDEO_EXTRA ) ) {
        testSetException( "Failed to do low level board init.\n" );
        TEST_EXIT( FAIL );
    }

    baseRegs = (Sstregs*)base;

    /* static register test */
    baseRegs->alphaMode = oddEvenOdd;
    baseRegs->stipple      = evenOddEven;
    baseRegs->c0           = oddEvenOdd;
    baseRegs->c1           = evenOddEven;

    sst1Idle( base );

    if ( oddEvenOdd  != baseRegs->alphaMode || 
         evenOddEven != baseRegs->stipple      || 
         oddEvenOdd  != baseRegs->c0           || 
         evenOddEven != baseRegs->c1 ) {
         sst1InitShutdown( base );
         TEST_EXIT( FAIL );
    }

    /* random register test */

    alphaMode    = rand() << 16 | rand();
    stipple      = rand() << 16 | rand();
    c0           = rand() << 16 | rand(); 
    c1           = rand() << 16 | rand();

    baseRegs->alphaMode = alphaMode;
    baseRegs->stipple      = stipple;
    baseRegs->c0           = c0;
    baseRegs->c1           = c1;

    sst1Idle( base );

    if ( alphaMode != baseRegs->alphaMode || 
         stipple      != baseRegs->stipple      || 
         c0           != baseRegs->c0           || 
         c1           != baseRegs->c1 ) {
         sst1InitShutdown( base );
         TEST_EXIT( FAIL );
    }
    
    sst1InitShutdown( base );
    TEST_EXIT( PASS );
END_TEST

BEGIN_TEST( CONFIG ) 
    FxU32 *sstbase;
    volatile Sstregs *sst;
    sst1DeviceInfoStruct deviceInfo;
    static char emsg[132];
    FxU32 tmu;

    if( !(sstbase = sst1InitMapBoard(0)) ||
        !(sst1InitRegisters(sstbase))    ||
        !(sst1InitGamma(sstbase, 1.0))   || 
        !(sst1InitVideo(sstbase, SST_RESOLUTION, GR_REFRESH_60Hz INITVIDEO_EXTRA ))) {
        testSetException( "failed low level board initialization" );
        sst1InitShutdown(sstbase);
        TEST_EXIT( EXCEPTION );
    }

    sst = (Sstregs *) sstbase;
    if(sst1InitGetDeviceInfo(sstbase, &deviceInfo) == FXFALSE) {
        testSetException( "could not get SST-1 device info");
        sst1InitShutdown(sstbase);
        TEST_EXIT( EXCEPTION );
        return;
    }

    packet->config->tmuRev = deviceInfo.tmuRevision;
    packet->config->fbiRev = deviceInfo.fbiRevision;
    testPrintConfig ( packet->config );

/*  Don't enforce chip revisions
    if ( deviceInfo.fbiRevision != packet->config->fbiRev ) {
        testSetException( "pixelFx revision mismatch" );
        sst1InitShutdown(sstbase);
        TEST_EXIT( EXCEPTION );
    }

    if ( deviceInfo.tmuRevision != packet->config->tmuRev ) {
        testSetException( "texelFx revision mismatch" );
        sst1InitShutdown(sstbase);
        TEST_EXIT( EXCEPTION );
    }
*/

#if !NO_ENFORCE_NUMTMUS
    if(deviceInfo.numberTmus != packet->config->nTMUs ) {
	sprintf(emsg, "Expected %ld TMU%sfound %ld", packet->config->nTMUs,
		packet->config->nTMUs > 1 ? "s, ":", ",
		deviceInfo.numberTmus);
	testSetException( emsg );
        sst1InitShutdown(sstbase);
        TEST_EXIT( EXCEPTION );
    }
#endif

    for( tmu = 0; tmu < packet->config->nTMUs; tmu++ ) {
        if(deviceInfo.tmuMemSize[tmu] != packet->config->tmuMem ) {
	    sprintf(emsg, "Expected tmu memory size to be %ld, found %ld",
		packet->config->tmuMem, deviceInfo.tmuMemSize[tmu]);
            testSetException( emsg );
            sst1InitShutdown(sstbase);
            TEST_EXIT( EXCEPTION );
        }
    }

    if ( deviceInfo.fbiMemSize != packet->config->fbiMem ) {
	sprintf(emsg, "Expected fbi memory size to be %ld, found %ld",
	    packet->config->fbiMem, deviceInfo.fbiMemSize);
        testSetException( emsg );
        sst1InitShutdown(sstbase);
        TEST_EXIT( EXCEPTION );
    }

    sst1InitShutdown(sstbase);
    TEST_EXIT( PASS );
END_TEST

#define RD_MSK		RED_MSK
#define GN_MSK		GRN_MSK
#define BL_MSK		BLU_MSK
#define RD_LSB		RED_LSB
#define GN_LSB		GRN_LSB
#define BL_LSB		BLU_LSB
#define RD_QTR		((RD_MSK+RD_LSB)>>2)
#define GN_QTR		((GN_MSK+GN_LSB)>>2)
#define BL_QTR		((BL_MSK+BL_LSB)>>2)
#define YL_MSK		(RD_MSK|GN_MSK)	/* yellow */
#define CY_MSK		(GN_MSK|BL_MSK) /* cyan */
#define VI_MSK		(RD_MSK|BL_MSK) /* violet */
#define WH_MSK		(RD_MSK|GN_MSK|BL_MSK) /* white */
#define BK_MSK		0x0000		/* black */
#define SL_MSK		(WH_MSK-(3*(RED_QTR+GRN_QTR+BLU_QTR)))

static const struct cols {
    U16 color;
    int row;
    int col;
    int an_row;
    int an_col;
    char *name;
} color_table[] = {
    {BK_MSK, 3, 1, 0, 0, "Black "},
    {BL_MSK, 3, 3, 0, 0, " Blue "},
    {GN_MSK, 3, 5, 0, 0, "Green "},
    {CY_MSK, 3, 7, 0, 0, " Cyan "},
    {RD_MSK, 1, 1, 0, 0, " Red  "},
    {VI_MSK, 1, 3, 0, 0, "Violet"},
    {YL_MSK, 1, 5, 0, 0, "Yellow"},
    {WH_MSK, 1, 7, 0, 0, "White "}
};

extern void setancolors(void);
    
static void makeColorBlocks(void) {
    FxU16 *lfbPtr, *dst;
    int row, col, color, oldtxt;
    extern void sst_text2fb_ptr(VU32*ptr, int flags);
    extern void vid_clr_alphas(void);
    char tmp[AN_VIS_COL_MAX];

    oldtxt = txt_select(TXT_HOST);
    vid_clr_alphas();
    setancolors();

    grLfbWriteMode( GR_LFBWRITEMODE_555 );
    dst = lfbPtr = grLfbGetWritePtr( GR_BUFFER_FRONTBUFFER );
    grLfbBegin();
    for (row=0; row < VIS_V_PIX; ++row) {
	for (col=0; col < VIS_H_PIX; ++col) {
	    *dst++ = BK_MSK;
	}
	dst += 1024-VIS_H_PIX;
    }
    for (color=0; color < n_elts(color_table); ++color) {
	for (row=0; row < VIS_V_PIX/5; ++row) {
	    dst = lfbPtr + ((color_table[color].row*VIS_V_PIX/5)+row)*1024 +
			    (color_table[color].col*VIS_H_PIX/9);
	    for (col=0; col < VIS_H_PIX/9; ++col) {
		*dst++ = color_table[color].color;
	    }
	}
	if (VIS_V_PIX == 256) {
	    row = color < n_elts(color_table)/2 ? AN_VIS_ROW/2-3 : 4*AN_VIS_ROW/5+1;
	} else {
	    row = color < n_elts(color_table)/2 ? AN_VIS_ROW/2-4 : 4*AN_VIS_ROW/5+1;
	}
	col = (((color&3)*2+1)*AN_VIS_COL/9) + 1;
	txt_str(col, row, color_table[color].name, WHT_PAL);
    }
    nsprintf(tmp, sizeof(tmp), "%dx%d 60HZ", VIS_H_PIX, VIS_V_PIX);
    txt_str(-1, 3, tmp, WHT_PAL|AN_BIG_SET);
    sst_text2fb_ptr((VU32*)lfbPtr, 0);
    grLfbEnd();
    txt_select(oldtxt);
    return;
}

extern int done_it_once;

BEGIN_TEST( DAC512 ) 
    TestCode code;
    int retry;
    if ( packet->reference ) 
        TEST_EXIT( PASS );

    for (retry=0; retry < 2; ++retry) {		/* For some reason, 2.1.1 needs two starts */
	if (retry) shutdownSST1();
	if (!initSST1_org( SST_RESOLUTION, GR_REFRESH_60Hz, GR_ORIGIN_LOWER_LEFT, GR_ORIGIN_LOWER_LEFT )) {
	    testOutput( "Unable to init the h/w" );
	    TEST_EXIT( FAIL );
	}
    }
    makeColorBlocks();
    testOutput( "***************************************\n" );
    testOutput( "THE MONITOR SHOULD BE DISPLAYING EIGHT \n" );
    testOutput( "COLORED SQUARES. EACH SQUARE IS LABELED\n" );
    testOutput( "WITH ITS COLOR.\n" );
    testOutput( "***************************************\n" );
    if (!(done_it_once&1)) {
	fprintf(stderr, "Is what the monitor displaying correct? (Y/N): " );
	if ( testYesNo() ) {
	    fprintf(stderr, "Y\r\n");
	    code = PASS;
	} else {
	    fprintf(stderr, "N\r\n");
	    code = FAIL;
	}
    } else {
	sleep(2);
	code = PASS;
    }
    shutdownSST1();
    TEST_EXIT( code );
END_TEST

BEGIN_TEST( CHK512 ) 
    TEST_EXIT( testColorChecksum( SST_RESOLUTION,
                                  0, 
                                  packet->reference ) );
END_TEST

/*-------------------------------------------------------------------
  GLIDE00
  [FBI+TMU]

  Tests ITRGB + TEX + ALPHA
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE00 )
    TestCode retval;

    testOutput( "Test 00: checking Iterated RGB + Texture + Alpha\n" );
    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        /* This test has been reduced to a test of itrgb * tex + alpha */
        GrVertex vtx1, vtx2, vtx3;

        GrColorCombineFnc_t cc_fnc = 
            GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_ADD_ALPHA;

        guAlphaSource( GR_ALPHASOURCE_ITERATED_ALPHA );

        guTexMemReset();

        dlTexture( _decal, packet->tmu, GR_MIPMAPLEVELMASK_BOTH, FXFALSE );

        grConstantColorValue( 0xFF808000 );
        grConstantColorValue4( 255.0F, 128.0F, 0.0F, 128.0F );

        grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );

        guColorCombineFunction( cc_fnc );

        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        } else {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        }

        vtx1.x        = MAX_F_X*0.25;
        vtx1.y        = MAX_F_Y*0.5;
        vtx1.r        = (float) 0xff;
        vtx1.g        = 0.0F;
        vtx1.b        = 0.0F;
        vtx1.a        = 25.0F;
	vtx1.oow = 1.0f;
        vtx1.tmuvtx[0].oow = 1.0F;
        vtx1.tmuvtx[0].sow = 0.0F;
        vtx1.tmuvtx[0].tow = 0.0F;
        vtx1.tmuvtx[1].oow = 1.0F;
        vtx1.tmuvtx[1].sow = 0.0F;
        vtx1.tmuvtx[1].tow = 0.0F;


        vtx2.x        = MAX_F_X*0.75;
        vtx2.y        = MAX_F_Y*0.5;
        vtx2.r        = 0.0F;
        vtx2.g        = (float) 0xff;
        vtx2.b        = 0.0F;
        vtx2.a        = 25.0F;
	vtx2.oow = 1.0f;
        vtx2.tmuvtx[0].oow = 1.0F;
        vtx2.tmuvtx[0].sow = 255.0F;
        vtx2.tmuvtx[0].tow = 0.0F;
        vtx2.tmuvtx[1].oow = 1.0F;
        vtx2.tmuvtx[1].sow = 255.0F;
        vtx2.tmuvtx[1].tow = 0.0F;


        vtx3.x        = MAX_F_X*0.5;
        vtx3.y        = MAX_F_Y*0.75;
        vtx3.r        = 0.0F;
        vtx3.g        = 0.0F;
        vtx3.b        = (float) 0xff;
        vtx3.a        = 255.0F;
	vtx3.oow = 1.0f;
        vtx3.tmuvtx[0].oow = 1.0F;
        vtx3.tmuvtx[0].sow = 128.0F;
        vtx3.tmuvtx[0].tow = 255.0F;
        vtx3.tmuvtx[1].oow = 1.0F;
        vtx3.tmuvtx[1].sow = 128.0F;
        vtx3.tmuvtx[1].tow = 255.0F;

        guTexSource( _decal->mmid );

        grDrawTriangle( &vtx1, &vtx2, &vtx3 );

        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    PAUSE();

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST


/*-------------------------------------------------------------------
  GLIDE01
  [FBI ONLY]

  Tests Fogging 
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE01 )
    TestCode retval;

    testOutput( "Test 01: checking fogging\n" );
    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex vtx1, vtx2, vtx3;
        GrFog_t  fogtable[GR_FOG_TABLE_SIZE];

        guColorCombineFunction( GR_COLORCOMBINE_ITRGB );
  
        grFogMode( GR_FOG_WITH_TABLE );
        grFogColorValue( 0xff00ff00 );
        guFogGenerateExp( fogtable, .01f );
        grFogTable( fogtable );

        grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);

        grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );
        vtx1.x = 0.f;
        vtx1.y = 0.f;
        vtx1.oow = vtx1.tmuvtx[0].oow = vtx1.tmuvtx[1].oow = 1.f / 20.f;
        vtx1.r = 0.f;
        vtx1.g = 0.f;
        vtx1.b = 64.f;
    
        vtx2.x = 10.f;
        vtx2.y = MAX_F_Y-1.0;
        vtx2.oow = vtx2.tmuvtx[0].oow = vtx2.tmuvtx[1].oow = 1.f / 2000.f;
        vtx2.r = 0.f;
        vtx2.g = 0.f;
        vtx2.b = 128.f;
    
        vtx3.x = MAX_F_X-1.0;
        vtx3.y = 10.f;
        vtx3.oow = vtx3.tmuvtx[0].oow = vtx3.tmuvtx[1].oow = 1.f / 20.f;
        vtx3.r = 0.f;
        vtx3.g = 0.f;
        vtx3.b = 64.f;
    
        grDrawTriangle( &vtx1, &vtx2, &vtx3 );

        grBufferSwap( 1 );

    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST


/*-------------------------------------------------------------------
  Test02
  [FBI+TMU]

  Tests Trilinear Texture Mapping
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE02 )
    TestCode retval;

    testOutput( "Test 02: checking trilinear Texture Mapping\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex a,b,c;
        float scale, reps;
        Texture *tex = _marble;

        guTexMemReset();

        grBufferClear( 0x000000FF, 0, GR_WDEPTHVALUE_FARTHEST );


        dlTexture( tex, packet->tmu, GR_MIPMAPLEVELMASK_ODD, FXTRUE );
        guTexSource( tex->mmid );

        guColorCombineFunction( GR_COLORCOMBINE_DECAL_TEXTURE );



        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        } else {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_BLEND_LOCAL,
                              GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION,
                              GR_COMBINE_FUNCTION_BLEND_LOCAL,
                              GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION,
                              FXFALSE,
                              FXFALSE );
            }
        }


        scale = 16.0f;
        reps  = scale;

        a.x = 10.0f, a.y = 10.0f, a.oow = 1.0f;
        a.tmuvtx[0].oow = 1.0f;
        a.tmuvtx[0].sow = 0.0f;
        a.tmuvtx[0].tow = 255.0f * reps;
        a.tmuvtx[1].oow = 1.0f;
        a.tmuvtx[1].sow = 0.0f;
        a.tmuvtx[1].tow = 255.0f * reps;

        b.x = MAX_F_X/2.0-1.0, b.y = MAX_F_Y/2.0, b.oow = 1.0f/scale;
        b.tmuvtx[0].oow = 1.0f/scale;
        b.tmuvtx[0].sow = (127.0f/scale);
        b.tmuvtx[0].tow = (0.0f/scale) * reps;
        b.tmuvtx[1].oow = 1.0f/scale;
        b.tmuvtx[1].sow = (127.0f/scale);
        b.tmuvtx[1].tow = (0.0f/scale) * reps;

        c.x = MAX_F_X-10.0, c.y = 10.0f, c.oow = 1.0f;
        c.tmuvtx[0].oow = 1.0f;
        c.tmuvtx[0].sow = 255.0f;
        c.tmuvtx[0].tow = 255.0f * reps;
        c.tmuvtx[1].oow = 1.0f;
        c.tmuvtx[1].sow = 255.0f;
        c.tmuvtx[1].tow = 255.0f * reps;

        grDrawTriangle( &a, &b, &c );
        

        dlTexture( tex, packet->tmu, GR_MIPMAPLEVELMASK_EVEN, FXTRUE );
        guTexSource( tex->mmid );
        guColorCombineFunction( GR_COLORCOMBINE_DECAL_TEXTURE );


        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_LOD_FRACTION,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_LOD_FRACTION,
                          FXFALSE,
                          FXFALSE );
        } else {
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_BLEND_LOCAL,
                              GR_COMBINE_FACTOR_LOD_FRACTION,
                              GR_COMBINE_FUNCTION_BLEND_LOCAL,
                              GR_COMBINE_FACTOR_LOD_FRACTION,
                              FXFALSE,
                              FXFALSE );
            }
        }

        grAlphaBlendFunction( GR_BLEND_ONE, GR_BLEND_ONE,
                              GR_BLEND_ZERO, GR_BLEND_ZERO );

        grDrawTriangle( &a, &b, &c );
        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE03
  [FBI ONLY]

  Tests simple output/triangle drawing
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE03 )
    TestCode retval;

    testOutput( "Test 03: checking simple output/triangle drawing\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex vtx1, vtx2;
        int i;

        guColorCombineFunction( GR_COLORCOMBINE_CCRGB );
        grConstantColorValue( ~(unsigned long)0 );

        grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );

        for( i = 0; i < MAX_Y; i += 4 )
        {
            vtx1.x = (float)i;  vtx1.y = (float)i;
            grDrawPoint( &vtx1 );
        }
        for( i = 0; i < MAX_X; i += 6 )
        {
            vtx1.x = (float)i;   vtx1.y = 0.f;
            vtx2.x = 0.f;        vtx2.y = (float)( ( MAX_X-1 - i ) * MAX_Y ) / MAX_F_X;

            grDrawLine( &vtx1, &vtx2 );

            vtx1.x = (float)i;   vtx1.y = 0.f;
            vtx2.x = MAX_F_X-1;  vtx2.y = (float)i;

            grDrawLine( &vtx1, &vtx2 );

            vtx1.x = (float)i;   vtx1.y = MAX_F_Y-1.0;
            vtx2.x = 0.f;        vtx2.y = (float)i;

            grDrawLine( &vtx1, &vtx2 );

            vtx1.x = (float)i;     vtx1.y = MAX_F_Y-1.0;
            vtx2.x = MAX_F_X-1.0;  vtx2.y = (float)( ( MAX_X-1 - i ) * MAX_Y ) / MAX_F_X;

            grDrawLine( &vtx1, &vtx2 );
        }
        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE04
  [FBI+TMU]

  Tests Detail Texturing
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE04 )
    TestCode retval;

    testOutput( "Test 04: checking Detail Texturing\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex a,b,c;
        float scale, reps;
        Texture *texMain   = _marble;
        Texture *texDetail = _alpha;

        guTexMemReset();

        grBufferClear( 0x000000FF, 0, GR_WDEPTHVALUE_FARTHEST );


        dlTexture( texMain, packet->tmu, GR_MIPMAPLEVELMASK_BOTH, FXFALSE );
        guTexSource( texMain->mmid );

        guColorCombineFunction( GR_COLORCOMBINE_DECAL_TEXTURE );

        grTexDetailControl( packet->tmu, 4, 7, 1.0f );

        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_DETAIL_FACTOR,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_DETAIL_FACTOR,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        } else {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_BLEND_LOCAL,
                              GR_COMBINE_FACTOR_DETAIL_FACTOR,
                              GR_COMBINE_FUNCTION_BLEND_LOCAL,
                              GR_COMBINE_FACTOR_DETAIL_FACTOR,
                              FXFALSE,
                              FXFALSE );
            }
        }


        scale = 16.0f;
        reps  = scale;

        a.x = 10.0f, a.y = 10.0f, a.oow = 1.0f;
        a.tmuvtx[0].oow = 1.0f;
        a.tmuvtx[0].sow = 0.0f;
        a.tmuvtx[0].tow = 255.0f * reps;
        a.tmuvtx[1].oow = 1.0f;
        a.tmuvtx[1].sow = 0.0f;
        a.tmuvtx[1].tow = 255.0f * reps;

        b.x = MAX_F_X/2, b.y = MAX_F_Y/2, b.oow = 1.0f/scale;
        b.tmuvtx[0].oow = 1.0f/scale;
        b.tmuvtx[0].sow = (127.0f/scale);
        b.tmuvtx[0].tow = (0.0f/scale) * reps;
        b.tmuvtx[1].oow = 1.0f/scale;
        b.tmuvtx[1].sow = (127.0f/scale);
        b.tmuvtx[1].tow = (0.0f/scale) * reps;

        c.x = MAX_F_X-9.0, c.y = 10.0f, c.oow = 1.0f;
        c.tmuvtx[0].oow = 1.0f;
        c.tmuvtx[0].sow = 255.0f;
        c.tmuvtx[0].tow = 255.0f * reps;
        c.tmuvtx[1].oow = 1.0f;
        c.tmuvtx[1].sow = 255.0f;
        c.tmuvtx[1].tow = 255.0f * reps;

        grDrawTriangle( &a, &b, &c );
        

        dlTexture( texDetail, packet->tmu, GR_MIPMAPLEVELMASK_BOTH, FXFALSE );
        guTexSource( texDetail->mmid );
        guColorCombineFunction( GR_COLORCOMBINE_DECAL_TEXTURE );

        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR,
                          FXFALSE,
                          FXFALSE );
        } else {
            grTexCombine( GR_TMU1,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR,
                          GR_COMBINE_FUNCTION_BLEND_LOCAL,
                          GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR,
                          FXFALSE,
                          FXFALSE );
        }


        grAlphaBlendFunction( GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO );

        grDrawTriangle( &a, &b, &c );
        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE05
  [FBI+TMU]

  Tests YIQ Compressed Textures
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE05 )
    TestCode retval;

    testOutput( "Test 05: checking YIQ Compressed Textures\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex vtx1, vtx2, vtx3;
        GrColorCombineFnc_t cc_fnc = GR_COLORCOMBINE_DECAL_TEXTURE;

        guTexMemReset();

        dlTexture( _yiq, packet->tmu, GR_MIPMAPLEVELMASK_BOTH, FXFALSE );

        grConstantColorValue( 0xFF808000 );
        grConstantColorValue4( 255.0F, 128.0F, 0.0F, 128.0F );

        grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );

        guColorCombineFunction( cc_fnc );

        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        } else {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        }

        vtx1.x        = MAX_F_X*0.25;
        vtx1.y        = MAX_F_Y*0.25;
        vtx1.r        = (float) 0xff;
        vtx1.g        = 0.0F;
        vtx1.b        = 0.0F;
        vtx1.a        = 25.0F;
	vtx1.oow      = 1.0f;
        vtx1.tmuvtx[0].oow = 1.0F;
        vtx1.tmuvtx[0].sow = 0.0F;
        vtx1.tmuvtx[0].tow = 0.0F;
        vtx1.tmuvtx[1].oow = 1.0F;
        vtx1.tmuvtx[1].sow = 0.0F;
        vtx1.tmuvtx[1].tow = 0.0F;


        vtx2.x        = MAX_F_X*0.75;
        vtx2.y        = MAX_F_Y*0.25;
        vtx2.r        = 0.0F;
        vtx2.g        = (float) 0xff;
        vtx2.b        = 0.0F;
        vtx2.a        = 25.0F;
	vtx2.oow      = 1.0f;
        vtx2.tmuvtx[0].oow = 1.0F;
        vtx2.tmuvtx[0].sow = 255.0F;
        vtx2.tmuvtx[0].tow = 0.0F;
        vtx2.tmuvtx[1].oow = 1.0F;
        vtx2.tmuvtx[1].sow = 255.0F;
        vtx2.tmuvtx[1].tow = 0.0F;


        vtx3.x        = MAX_F_X*0.5;
        vtx3.y        = MAX_F_Y*0.75;
        vtx3.r        = 0.0F;
        vtx3.g        = 0.0F;
        vtx3.b        = (float) 0xff;
        vtx3.a        = 255.0F;
	vtx3.oow      = 1.0f;
        vtx3.tmuvtx[0].oow = 1.0F;
        vtx3.tmuvtx[0].sow = 128.0F;
        vtx3.tmuvtx[0].tow = 255.0F;
        vtx3.tmuvtx[1].oow = 1.0F;
        vtx3.tmuvtx[1].sow = 128.0F;
        vtx3.tmuvtx[1].tow = 255.0F;

        guTexSource( _yiq->mmid );

        grDrawTriangle( &vtx1, &vtx2, &vtx3 );

        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE06
  [FBI ONLY]

  Tests RGB Iterators, Triangle Drawing
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE06 )
    TestCode retval;

    testOutput( "Test 06: checking RGB Iterators, triangle drawing\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
      GrVertex vtx1, vtx2, vtx3;

      guColorCombineFunction( GR_COLORCOMBINE_ITRGB );
      grTexCombineFunction(GR_TMU0, GR_TEXTURECOMBINE_ZERO);

      grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );

      vtx1.x = 0.f;    vtx1.y = 0.f;   vtx1.r = 255.f; vtx1.g = 0.f;   vtx1.b = 0.f;
      vtx2.x = 10.f;   vtx2.y = MAX_F_Y-1.0; vtx2.r = 0.f;   vtx2.g = 255.f; vtx2.b = 0.f;
      vtx3.x = MAX_F_X-1.0;  vtx3.y = 10.f;  vtx3.r = 0.f;   vtx3.g = 0.f;   vtx3.b = 255.f;

      grDrawTriangle( &vtx1, &vtx2, &vtx3 );

      grBufferSwap( 1 );

    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE07
  [FBI+TMU]

  Tests texture filter Modes
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE07 )
    TestCode retval;

    testOutput( "Test 07: checking texture filter Modes\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex a,b,c;
        Texture *tex = _marble;

        guTexMemReset();

        grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );

        dlTexture( tex, packet->tmu, GR_MIPMAPLEVELMASK_BOTH, FXFALSE );
        guTexSource( tex->mmid );

        guColorCombineFunction( GR_COLORCOMBINE_DECAL_TEXTURE );

        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        } else {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        }

        a.tmuvtx[0].oow =   1.0f;
        a.tmuvtx[0].sow =   0.0f;
        a.tmuvtx[0].tow =   0.0f;

        b.tmuvtx[0].oow =   1.0f;
        b.tmuvtx[0].sow = 255.0f;
        b.tmuvtx[0].tow = 255.0f;

        c.tmuvtx[0].oow =   1.0f;
        c.tmuvtx[0].sow =   0.0f;
        c.tmuvtx[0].tow = 255.0f;


        a.tmuvtx[1].oow =   1.0f;
        a.tmuvtx[1].sow =   0.0f;
        a.tmuvtx[1].tow =   0.0f;

        b.tmuvtx[1].oow =   1.0f;
        b.tmuvtx[1].sow = 255.0f;
        b.tmuvtx[1].tow = 255.0f;

        c.tmuvtx[1].oow =   1.0f;
        c.tmuvtx[1].sow =   0.0f;
        c.tmuvtx[1].tow = 255.0f;


        a.x   =  10.0f;
        a.y   = MAX_F_Y/2.0;
        a.oow =   1.0f;

        b.x   = MAX_F_X-11.0;
        b.y   =  10.0f;
        b.oow =   1.0f;

        c.x   =  10.0f;
        c.y   =  10.0f;
        c.oow =   1.0f;

        grTexFilterMode( packet->tmu,
                         GR_TEXTUREFILTER_POINT_SAMPLED,
                         GR_TEXTUREFILTER_POINT_SAMPLED );

        grDrawTriangle( &a, &b, &c );

        b.x   = MAX_F_X-11.0;
        b.y   = MAX_F_Y/2.0;
        b.oow =   1.0f;

        c.x   = MAX_F_X-11.0;
        c.y   =  10.0f;
        c.oow =   1.0f;

        grTexFilterMode( packet->tmu,
                         GR_TEXTUREFILTER_BILINEAR,
                         GR_TEXTUREFILTER_BILINEAR );

        grDrawTriangle( &a, &b, &c );
                
        a.x   =  10.0f;
        a.y   = MAX_F_Y-11.0;
        a.oow =   1.0f;

        b.x   =  30.0f;
        b.y   = MAX_F_Y-31.0;
        b.oow =   1.0f;

        c.x   =  10.0f;
        c.y   = MAX_F_Y-31.0;
        c.oow =   1.0f;

        grTexFilterMode( packet->tmu,
                         GR_TEXTUREFILTER_POINT_SAMPLED,
                         GR_TEXTUREFILTER_POINT_SAMPLED );

        grDrawTriangle( &a, &b, &c );

        b.x   =  30.0f;
        b.y   = MAX_F_Y-11.0;
        b.oow =   1.0f;

        c.x   =  30.0f;
        c.y   = MAX_F_Y-31.0;
        c.oow =   1.0f;

        grTexFilterMode( packet->tmu,
                         GR_TEXTUREFILTER_BILINEAR,
                         GR_TEXTUREFILTER_BILINEAR );

        grDrawTriangle( &a, &b, &c );
                

        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE08
  [FBI ONLY]

  Tests depth buffer, comparison functions
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE08 )
    TestCode retval;

    testOutput( "Test 08: checking depth buffer, comparison functions\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        GrVertex vtxlist[3];
        
        if ( packet->config->fbiMem >= 2 ) {
            grDepthBufferMode( GR_DEPTHBUFFER_WBUFFER );
            grDepthBufferFunction( GR_CMP_LESS );
            grDepthMask( FXTRUE );
        }

        guColorCombineFunction( GR_COLORCOMBINE_CCRGB );
  
        grBufferClear( 0, 0, GR_WDEPTHVALUE_FARTHEST );

        vtxlist[0].x   = MAX_F_X*0.25;
        vtxlist[0].y   = MAX_F_Y*0.21;
        vtxlist[0].oow = (1.f/10.f);
        vtxlist[1].x   = MAX_F_X*0.75;
        vtxlist[1].y   = MAX_F_Y*0.21;
        vtxlist[1].oow = (1.f/10.f);
        vtxlist[2].x   = MAX_F_X*0.50;
        vtxlist[2].y   = MAX_F_Y*0.79;
        vtxlist[2].oow = (1.f/10.f);
        grConstantColorValue( 0x000000FF );

        grDrawTriangle( &vtxlist[0], &vtxlist[2], &vtxlist[1] );

        vtxlist[0].x   = MAX_F_X*0.86;
        vtxlist[0].y   = MAX_F_Y*0.21;
        vtxlist[0].oow = (1.f/12.5f);
        vtxlist[1].x   = MAX_F_X*0.86;
        vtxlist[1].y   = MAX_F_Y*0.79;
        vtxlist[1].oow = (1.f/12.5f);
        vtxlist[2].x   = MAX_F_X*0.14;
        vtxlist[2].y   = MAX_F_Y*0.50;
        vtxlist[2].oow = (1.f/7.5f);
        grConstantColorValue( 0x0000FF00 );

        grDrawTriangle( &vtxlist[0], &vtxlist[1], &vtxlist[2] );

        grBufferSwap( 1 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE09
  [FBI ONLY]

  Tests alpha blending
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE09 )
    TestCode retval;

    testOutput( "Test 09: checking alpha blending\n" );

    /*----------------------------------------------------------------
      Initialize Hardware
      ----------------------------------------------------------------*/
    if ( packet->initHardware )
        initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

    /*----------------------------------------------------------------
      Save Glide State
      ----------------------------------------------------------------*/
    grGlideGetState( &_state );

    /*----------------------------------------------------------------
      Clear All Buffers
      ----------------------------------------------------------------*/
    clearBuffers();

    /*----------------------------------------------------------------
      Run Test
      ----------------------------------------------------------------*/
    {
        static GrVertex vtxlist[3];

        guAlphaSource( GR_ALPHASOURCE_CC_ALPHA );
        grAlphaBlendFunction( GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO );
        grColorCombine(
            GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ONE, 
            GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE);

        grBufferClear( 0, 0, 0 );

        vtxlist[0].x   = MAX_F_X*0.25;
        vtxlist[0].y   = MAX_F_Y*0.21;
        vtxlist[1].x   = MAX_F_X*0.75;
        vtxlist[1].y   = MAX_F_Y*0.21;
        vtxlist[2].x   = MAX_F_X*0.50;
        vtxlist[2].y   = MAX_F_Y*0.79;

        grConstantColorValue4(255.0f, 0.0f, 0.0f, 255.0f);
        grConstantColorValue( 0xFF0000FF );

        grDrawTriangle( &vtxlist[0], &vtxlist[2], &vtxlist[1] );

        vtxlist[0].x   = MAX_F_X*0.25;
        vtxlist[0].y   = MAX_F_Y*0.79;
        vtxlist[1].x   = MAX_F_X*0.75;
        vtxlist[1].y   = MAX_F_Y*0.79;
        vtxlist[2].x   = MAX_F_X*0.50;
        vtxlist[2].y   = MAX_F_Y*0.21;

        grConstantColorValue4(128.0f, 255.0f, 0.0f, 0.0f);
        grConstantColorValue( 0x80FF0000 );

        grDrawTriangle( &vtxlist[0], &vtxlist[1], &vtxlist[2] );

        grBufferSwap( 0 );
    }

    /*----------------------------------------------------------------
      Grab Buffers From SST1 and Compare Against Refimgs
       or Dump Reference Images
      ----------------------------------------------------------------*/
    retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

    /*----------------------------------------------------------------
      Restore Glide State
      ----------------------------------------------------------------*/
    grGlideSetState( &_state );

    PAUSE();

    /*----------------------------------------------------------------
      Shut down Hardware
      ----------------------------------------------------------------*/
    if ( packet->shutdownHardware )
        shutdownSST1();

    TEST_EXIT( retval );
END_TEST

/*-------------------------------------------------------------------
  GLIDE10
  [FBI+TMU]

  Tests Palletized Textures
  -------------------------------------------------------------------*/
BEGIN_TEST( GLIDE10 )
    TestCode retval;

    if ( packet->config->tmuRev > 0 ) {
        GrVertex vtx1, vtx2, vtx3;
        GrColorCombineFnc_t cc_fnc = GR_COLORCOMBINE_DECAL_TEXTURE;

	testOutput( "Test 10: checking Palletized Textures\n" );

	/*----------------------------------------------------------------
	  Initialize Hardware
	  ----------------------------------------------------------------*/
	if ( packet->initHardware )
	    initSST1( SST_RESOLUTION, GR_REFRESH_60Hz );

	/*----------------------------------------------------------------
	  Save Glide State
	  ----------------------------------------------------------------*/
	grGlideGetState( &_state );

	/*----------------------------------------------------------------
	  Clear All Buffers
	  ----------------------------------------------------------------*/
	clearBuffers();

	/*----------------------------------------------------------------
	  Run Test
	  ----------------------------------------------------------------*/

        guTexMemReset();

        dlTexture( _palette, packet->tmu, GR_MIPMAPLEVELMASK_BOTH, FXFALSE );

        grConstantColorValue( 0xFF808000 );
        grConstantColorValue4( 255.0F, 128.0F, 0.0F, 128.0F );

        grBufferClear( 0x00000000, 0, GR_WDEPTHVALUE_FARTHEST );

        guColorCombineFunction( cc_fnc );

        if ( packet->tmu == 0 ) {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_LOCAL,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_ZERO,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        } else {
            grTexCombine( GR_TMU0,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          GR_COMBINE_FUNCTION_SCALE_OTHER,
                          GR_COMBINE_FACTOR_ONE,
                          FXFALSE,
                          FXFALSE );
            if ( packet->config->nTMUs == 2 ) {
                grTexCombine( GR_TMU1,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_LOCAL,
                              GR_COMBINE_FACTOR_ONE,
                              FXFALSE,
                              FXFALSE );
            }
        }

        vtx1.x        = MAX_F_X*0.25;
        vtx1.y        = MAX_F_Y*0.50;
        vtx1.r        = (float) 0xff;
        vtx1.g        = 0.0F;
        vtx1.b        = 0.0F;
        vtx1.a        = 25.0F;
	vtx1.oow      = 1.0f;
        vtx1.tmuvtx[0].oow = 1.0F;
        vtx1.tmuvtx[0].sow = 0.0F;
        vtx1.tmuvtx[0].tow = 0.0F;
        vtx1.tmuvtx[1].oow = 1.0F;
        vtx1.tmuvtx[1].sow = 0.0F;
        vtx1.tmuvtx[1].tow = 0.0F;


        vtx2.x        = MAX_F_X*0.75;
        vtx2.y        = MAX_F_Y*0.50;
        vtx2.r        = 0.0F;
        vtx2.g        = (float) 0xff;
        vtx2.b        = 0.0F;
        vtx2.a        = 25.0F;
	vtx2.oow      = 1.0f;
        vtx2.tmuvtx[0].oow = 1.0F;
        vtx2.tmuvtx[0].sow = 255.0F;
        vtx2.tmuvtx[0].tow = 0.0F;
        vtx2.tmuvtx[1].oow = 1.0F;
        vtx2.tmuvtx[1].sow = 255.0F;
        vtx2.tmuvtx[1].tow = 0.0F;


        vtx3.x        = MAX_F_X*0.50;
        vtx3.y        = MAX_F_Y*0.75;
        vtx3.r        = 0.0F;
        vtx3.g        = 0.0F;
        vtx3.b        = (float) 0xff;
        vtx3.a        = 255.0F;
	vtx3.oow      = 1.0f;
        vtx3.tmuvtx[0].oow = 1.0F;
        vtx3.tmuvtx[0].sow = 128.0F;
        vtx3.tmuvtx[0].tow = 255.0F;
        vtx3.tmuvtx[1].oow = 1.0F;
        vtx3.tmuvtx[1].sow = 128.0F;
        vtx3.tmuvtx[1].tow = 255.0F;

        grTexDownloadTable( packet->tmu, 
                            GR_TEXTABLE_PALETTE,
                            _palette->info.table.palette.data );
        guTexSource( _palette->mmid );

        grDrawTriangle( &vtx1, &vtx2, &vtx3 );

        grBufferSwap( 1 );

	/*----------------------------------------------------------------
	  Grab Buffers From SST1 and Compare Against Refimgs
	   or Dump Reference Images
	  ----------------------------------------------------------------*/
	retval = testBuffers( TEST_FRONT, TEST_AUX, packet->reference );

	/*----------------------------------------------------------------
	  Restore Glide State
	  ----------------------------------------------------------------*/
	grGlideSetState( &_state );

	PAUSE();

	/*----------------------------------------------------------------
	  Shut down Hardware
	  ----------------------------------------------------------------*/
	if ( packet->shutdownHardware )
	    shutdownSST1();

    } else {
	testOutput( "Test 10: Skipped because Rev 0 TMU doesn't have Palletized Textures\n" );
	retval = TRUE;
    }

    TEST_EXIT( retval );
END_TEST


#undef BEGIN_TEST
#undef END_TEST


/*-------------------------------------------------------------
  Helper Functions
  -------------------------------------------------------------*/


#define PAT0_24BPP 0x5848d0
#define PAT0_16BPP 0x5a5a
#define PAT1_24BPP 0xa0b428
#define PAT1_16BPP 0xa5a5

void drawRect(FxU32 *sstbase, 
              FxU32 x, FxU32 y, 
              FxU32 xsize, FxU32 ysize,
              FxU32 color) {
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

#if 0
FxBool checkScreen(FxU32 *sstbase, FxU32 color16, FxU32 resX, FxU32 resY) {
    volatile unsigned long *lfbptr = (unsigned long *) sstbase;
    FxU32 x, y, data;

    sst1Idle(sstbase);
    
    for(x=0; x<resX; x+=2) {
        for(y=0; y<resY; y++) {
            data = lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];
            if((data & 0xffff) != color16)
                return FXFALSE;
            if(((data >> 16) & 0xffff) != color16)
                return FXFALSE;
        }
    }
    return(FXTRUE);
}

FxU32 checkFB(FxU32 *sstbase, FxU32 resX, FxU32 resY, FxBool aux )
{
    volatile Sstregs *sst = (Sstregs *) sstbase;
    volatile unsigned long *lfbptr;
    FxU32 x, y, data, rcv;

    /*/// */
    /* Check front, back, and Z are all 0x0 (cleared in InitVideo) */
    /*/// */
    sst->lfbMode = SST_LFB_READFRONTBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, 0x0, resX, resY) == FXFALSE) {
        return FAIL;
    }
    sst->lfbMode = SST_LFB_READBACKBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, 0x0, resX, resY) == FXFALSE) {
        return FAIL;
    }

    if ( aux ) {
        sst->lfbMode = SST_LFB_READDEPTHABUFFER;
        sst1Idle(sstbase);
        if(checkScreen(sstbase, 0x0, resX, resY) == FXFALSE) {
            return FAIL;
        }
    }

    /*/// */
    /* Check White... */
    /*/// */
    sst->fbzMode = SST_RGBWRMASK | SST_DRAWBUFFER_FRONT | 
                   (( aux ) ? SST_ZAWRMASK : 0x0);

    sst->zaColor = 0xffff;
    drawRect(sstbase, 0, 0, resX, resY, 0xffffff);
    sst->lfbMode = SST_LFB_READFRONTBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, 0xffff, resX, resY) == FXFALSE) {
        return FAIL;
    }

    sst->fbzMode = SST_RGBWRMASK |  SST_DRAWBUFFER_BACK;
    drawRect(sstbase, 0, 0, resX, resY, 0xffffff);
    sst->lfbMode = SST_LFB_READBACKBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, 0xffff, resX, resY) == FXFALSE) {
        return FAIL;
    }

    if ( aux ) {
        sst->lfbMode = SST_LFB_READDEPTHABUFFER;
        sst1Idle(sstbase);
        if(checkScreen(sstbase, 0xffff, resX, resY) == FXFALSE) {
            return FAIL;
        }
    }

    /*/// */
    /* Check Patterns... */
    /*/// */
    sst->fbzMode = SST_RGBWRMASK |  SST_DRAWBUFFER_FRONT;
    drawRect(sstbase, 0, 0, resX, resY, PAT0_24BPP);
    sst->lfbMode = SST_LFB_READFRONTBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, PAT0_16BPP, resX, resY) == FXFALSE) {
        return FAIL;
    }
    drawRect(sstbase, 0, 0, resX, resY, PAT1_24BPP);
    sst->lfbMode = SST_LFB_READFRONTBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, PAT1_16BPP, resX, resY) == FXFALSE) {
        return FAIL;
    }
    sst->fbzMode = SST_RGBWRMASK |  SST_DRAWBUFFER_BACK;
    drawRect(sstbase, 0, 0, resX, resY, PAT0_24BPP);
    sst->lfbMode = SST_LFB_READBACKBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, PAT0_16BPP, resX, resY) == FXFALSE) {
        return FAIL;
    }
    drawRect(sstbase, 0, 0, resX, resY, PAT1_24BPP);
    sst->lfbMode = SST_LFB_READBACKBUFFER;
    sst1Idle(sstbase);
    if(checkScreen(sstbase, PAT1_16BPP, resX, resY) == FXFALSE) {
        return FAIL;
    }

    if ( aux ) {
        sst->fbzMode = SST_ZAWRMASK;
        sst->zaColor = PAT0_16BPP;
        drawRect(sstbase, 0, 0, resX, resY, PAT0_24BPP);
        sst->lfbMode = SST_LFB_READDEPTHABUFFER;
        sst1Idle(sstbase);
        if(checkScreen(sstbase, PAT0_16BPP, resX, resY) == FXFALSE) {
            return FAIL;
        }
        sst->zaColor = PAT1_16BPP;
        drawRect(sstbase, 0, 0, resX, resY, PAT1_24BPP);
        sst->lfbMode = SST_LFB_READDEPTHABUFFER;
        sst1Idle(sstbase);
        if(checkScreen(sstbase, PAT1_16BPP, resX, resY) == FXFALSE) {
            return FAIL;
        }
    }

    /*/// */
    /* Check Random... */
    /*/// */
    lfbptr = (unsigned long *) sstbase;
    sst->lfbMode = SST_LFB_565 | SST_LFB_WRITEFRONTBUFFER |
        SST_LFB_READFRONTBUFFER;
    data = 0xbaddead;
    for(x=0; x<resX; x+=2) {
        for(y=0; y<resY; y++) {
            lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)] = data;
            data += 0x34972195;
        }
    }
    data = 0xbaddead;
    sst1Idle(sstbase);
    for(x=0; x<resX; x+=2) {
        for(y=0; y<resY; y++) {
            rcv = lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];
            if(rcv != data) {
                return FAIL;
            }
            data += 0x34972195;
        }
    }
    sst->lfbMode = SST_LFB_565 | SST_LFB_WRITEBACKBUFFER |
        SST_LFB_READBACKBUFFER;
    data = 0xbaddead;
    for(x=0; x<resX; x+=2) {
        for(y=0; y<resY; y++) {
            lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)] = data;
            data += 0x74978193;
        }
    }
    data = 0xbaddead;
    sst1Idle(sstbase);
    for(x=0; x<resX; x+=2) {
        for(y=0; y<resY; y++) {
            rcv = lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];
            if(rcv != data) {
                return FAIL;
            }
            data += 0x74978193;
        }
    }

    if ( aux ) {
        sst->fbzMode = SST_ZAWRMASK;
        sst->lfbMode = SST_LFB_ZZ | SST_LFB_READDEPTHABUFFER;
        data = 0xabddadd;
        for(x=0; x<resX; x+=2) {
            for(y=0; y<resY; y++) {
                lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)] = data;
                data += 0x77976393;
            }
        }
        data = 0xabddadd;
        sst1Idle(sstbase);
        for(x=0; x<resX; x+=2) {
            for(y=0; y<resY; y++) {
                rcv = lfbptr[((SST_LFB_ADDR+(x<<1)+(y<<11))>>2)];
                if(rcv != data) {
                    return FAIL;
                }
                data += 0x77976393;
            }
        }
    }

    sst1Idle(sstbase);

    return PASS;
}
#endif

void grabRect( void *memory, 
               FxU32 minx, FxU32 miny, 
               FxU32 maxx, FxU32 maxy ) {
    FxU16 *lfbPtr;
    FxU16 *dstPtr;
    FxU32 scanline;
    FxU32 slop;
    FxU32 width;

    grLfbBegin();
    width  = maxx - minx + 1;
    lfbPtr = (FxU16 *)grLfbGetReadPtr( GR_BUFFER_FRONTBUFFER );
    lfbPtr += ( miny * 1024 ) + minx;
    dstPtr = (FxU16*)memory;
    slop = 1024 - width;
    sst1Idle(sst1InitGetBaseAddr(0));
    for( scanline = miny; scanline <= maxy; scanline++ ) {
        FxU16 *end = dstPtr + width;
        while( dstPtr < end ) {
            *dstPtr++ = *lfbPtr++;
        }
        lfbPtr+=slop;
    }
    grLfbEnd();
    sst1Idle(sst1InitGetBaseAddr(0));
    return;
}


/*--------------------------------------------------------------------
  Startup and Shutdown
  --------------------------------------------------------------------*/
static FxBool initSST1_org( GrScreenResolution_t res, GrScreenRefresh_t refresh, 
    			    GrOriginLocation_t org, GrOriginLocation_t lfborg) {
#if GLIDE_VERSION > 203
    {
	extern void _GlideInitEnvironment(void);
	_GlideInitEnvironment();
	grResetTriStats();
    }
#else
    {
	static GrHwConfiguration hwconfig;
	grGlideInit();
	if ( !grSstQueryHardware( &hwconfig ) )
	    return FXFALSE;
    }
#endif

    DEFAULT_RES_METHOD();

    grSstSelect( 0 );
    if ( !grSstOpen( res,
                     refresh,
                     GR_COLORFORMAT_ARGB,
                     org, 
                     GR_SMOOTHING_DISABLE,
                     2 ) ) {
        return FXFALSE;
     }

    _width  = grSstScreenWidth();
    _height = grSstScreenHeight();

    grBufferClear( 0xFF, 0, 0 );
    grBufferSwap( 0 );

    grLfbOrigin( lfborg );
    grLfbWriteMode( GR_LFBWRITEMODE_565 );
    grLfbGetWritePtr( GR_BUFFER_BACKBUFFER );
    grLfbBypassMode( GR_LFBBYPASS_ENABLE );
    /* This is in here because I was getting garblage without it */
    sst1Idle(sst1InitGetBaseAddr(0));
    return FXTRUE;
}

FxBool initSST1( GrScreenResolution_t res, GrScreenRefresh_t refresh ) {
    return initSST1_org( res, refresh, GR_ORIGIN_LOWER_LEFT, GR_ORIGIN_UPPER_LEFT);
}

void shutdownSST1( void ) {
    grGlideShutdown();
}

/*------------------------------------------------------------------
  Image Buffers
  ------------------------------------------------------------------*/
Buffer *allocBuffer( FxU32 width, FxU32 height ) {
    Buffer *r;
    r = calloc( sizeof( Buffer ), 1 );
    if (!r) {
	fprintf(stderr, "allocBuffer: Out of heap asking for %d bytes\r\n",
	    sizeof( Buffer ));
	exit(-1);
    }
    r->width = width;
    r->height = height;
    r->data = calloc( width * height, 2 );
    if (!r->data) {
	fprintf(stderr, "allocBuffer: Out of heap asking for %ldx%ld (%ld) bytes\r\n",
	    width, height, width*height);
	exit(-1);
    }
    return r;
}

void freeBuffer( Buffer **ptr ) {
    Buffer *r;
    r = *ptr;
    free(r->data);
    free(r);
    *ptr = 0;
    return;
}

Buffer *readImage( const char *file ) {
    char lineBuffer[128];
    FILE *infile;

    _buffer.data   = _imgData;

    infile = fopen( file, "rb" );
    if ( !infile ) {
        return 0;
    }
    fgets( lineBuffer, 128, infile ); /* P9 */
    if ( !strstr( lineBuffer, "P9" ) ) {
        return 0;
    }
    fgets( lineBuffer, 128, infile ); /* Y+ */
    fgets( lineBuffer, 128, infile ); /* width height */
    if ( 2 != sscanf( lineBuffer, "%li %li", 
                      &_buffer.width, &_buffer.height ) ) {
        return 0;
    }
    fgets( lineBuffer, 128, infile ); /* bit depth */
    fread( _buffer.data, 
           sizeof( FxU16 ), 
           _buffer.width * _buffer.height,
           infile );
    fclose( infile );
    return &_buffer;
}

FxBool writeImage( const Buffer *img, const char *file ) {
    FILE *outfile;

    outfile = fopen( file, "wb" );
    if ( !outfile ) {
        return FXFALSE;
    }

    fputs( "P9\n", outfile );
    fputs( "Y+\n", outfile );
    fprintf( outfile, "%ld %ld\n", img->width, img->height );
    fputs( "R 5 G 6 B 5\n", outfile );
    fwrite( img->data, sizeof( FxU16 ) * img->width * img->height,
            1, outfile );
    fclose( outfile );
    return FXTRUE;
}

FxBool pasteImage( const Buffer *img, GrBuffer_t buffer ) {
    FxU16 *lfbPtr;
    FxU16 *srcPtr;
    FxU32 scanline;
    FxU32 slop;
    if ( _width != img->width || _height != img->height ) {
        return FXFALSE;
    }

    if ( buffer == GR_BUFFER_AUXBUFFER ) {
        grLfbWriteMode( GR_LFBWRITEMODE_DEPTH_DEPTH );
        lfbPtr = grLfbGetWritePtr( GR_BUFFER_FRONTBUFFER );
    } else {    
        grLfbWriteMode( GR_LFBWRITEMODE_565 );
        lfbPtr = grLfbGetWritePtr( buffer );
    }
    srcPtr = img->data;
    slop = 1024 - _width;
    grLfbBegin();
    if ( buffer == GR_BUFFER_BACKBUFFER ) grDepthMask( FXTRUE );
    for( scanline = 0; scanline < _height; scanline++ ) {
        FxU16 *end = srcPtr + _width;
        while( srcPtr < end ) {
            *lfbPtr++ = *srcPtr++;
        }
        lfbPtr += slop;
    }
    
    grLfbEnd();
    return FXTRUE;
}

FxBool grabImage( Buffer *img, GrBuffer_t buffer ) {
    if ( img->width != _width || img->height != _height ) {
        return FXFALSE;
    }

    grLfbBegin();
    grLfbGetReadPtr( buffer );
    if ( buffer == GR_BUFFER_BACKBUFFER ) grDepthMask( FXTRUE );
    guFbReadRegion( 0, 0, img->width, img->height, img->data, img->width * 2);
    grLfbEnd();
    return FXTRUE;
}

FxU32 compareImage( const Buffer *a, const Buffer *b ) {
    FxU16 *aData = a->data;
    FxU16 *bData = b->data;
    FxU16 *end;
    FxU32 retval = 0;
    
    if ( a->width != b->width || a->height != b->height ) {
        return 1;
    }

    end = aData + a->width * a->height;
    while( aData < end ) {
        retval += ( *aData++ != *bData++ );
    }

    return retval;
}


/*------------------------------------------------------------
  CHECKSUM CODE
  ------------------------------------------------------------*/
FxBool waitForActiveVsync(FxU32 *sstbase)
{
    int   cntr;
    unsigned long timeout;

    cntr = 0;
    timeout = 0;
    while(1) {
        if(!(sst1InitReturnStatus(sstbase) & SST_VRETRACE)) {
            if(++cntr > 3)
                return FXTRUE;
        } else
            cntr = 0;
            if ( (++timeout) > 1660000l ) 
                return FXFALSE;
    }
}

FxBool waitForInactiveVsync(FxU32 *sstbase)
{
    int cntr;
    unsigned long timeout;

    cntr = 0;
    timeout = 0;
    while(1) {
        if(sst1InitReturnStatus(sstbase) & SST_VRETRACE) {
            if(++cntr > 3)
                return FXTRUE;
        } else
            cntr = 0;
            if ( (++timeout) > 1660000l ) 
                return FXFALSE;
    }
}


int getInvalidVideoChecksum(FxU32 *sstbase)
{
    FxU32 n, pciData, videoChecksumBase, videoChecksum;
    FxU32 sst1DeviceNumber;
    sst1DeviceInfoStruct sstInfo;
    volatile Sstregs *sst = (Sstregs *) sstbase;

    sst1Idle(sstbase);
    sst1InitGetDeviceInfo(sstbase, &sstInfo);
    sst1DeviceNumber = sstInfo.deviceNumber;

    /* Remap read addresses to read video checksum register... */
    PCICFG_RD(PCI_INIT_ENABLE, pciData);
    PCICFG_WR(PCI_INIT_ENABLE, (pciData | SST_FBIINIT23_REMAP));
    sst1Idle(sstbase);

    if ( !waitForActiveVsync(sstbase) )
        return -1;
    videoChecksumBase = sst->fbiInit3 & 0xffffff;

    if ( !waitForInactiveVsync(sstbase) ) 
        return -1;

    if ( !waitForActiveVsync(sstbase) ) 
        return -1;

    videoChecksum = sst->fbiInit3 & 0xffffff;

    /* Restore INIT_ENABLE configuration register */
    PCICFG_WR(PCI_INIT_ENABLE, pciData);
    sst1Idle(sstbase);

    return((videoChecksum - videoChecksumBase) & 0xffffff);
}

int computeVideoChecksum(FxU32 *sstbase)
{
    int i, j;
    FxU32 videoChecksum, tmp;

    /* Because NT and Windows95 are not realtime operating systems, we cannot */
    /* guarantee that the code which calculates the video checksum can */
    /* accurately wait for frame-by-frame video sync signals.  To compensate, */
    /* we require the same result to be returned 5 times in a row to be used */
    /* as a valid checksum (what a hack!) */

    videoChecksum = 0x4000000;
    i = j = 0;
    while(1) {
        tmp = getInvalidVideoChecksum(sstbase);
        if(tmp == videoChecksum) {
            i++;
            if(i == 5)
                break;
        } else {
            videoChecksum = tmp;
            i = 0;
        }
        if(++j > 20)
            return(-1);
    }
    return(videoChecksum);
}

TestCode testColorChecksum( GrScreenResolution_t res, char *image, FxBool reference ) {
    FxU32    *sstbase;
    volatile Sstregs *sst;
    FxU32     checksum;

    testOutput( "Testing FBI color checksum...\n" );

    initSST1( res, GR_REFRESH_60Hz );

    if ( !(sstbase = sst1InitGetBaseAddr(0)) ) {
        testSetException( "couldn't get base pointer" );
        shutdownSST1();
        return EXCEPTION;
    }

    sst = (Sstregs *) sstbase;
    if(sst->fbiInit1 & SST_VIDEO_RESET) {
        testSetException( "failed low level board init" );
        shutdownSST1();
        return EXCEPTION;
    }
    if(sst->fbiInit0 & SST_GRX_RESET) {
        testSetException( "failed low level board init" );
        shutdownSST1();
        return EXCEPTION;
    }
    if(!(sst->fbiInit1 & SST_VIDEO_24BPP_EN)) {
        testSetException( "failed low level board init" );
        shutdownSST1();
        return EXCEPTION;
    }
    grGammaCorrectionValue( 1.0f );

    /* Make sure the OS isn't doing anything noisy */
    fflush( stdout );
    sleep( 1 );

    if ( reference ) {
        sst1Idle(sstbase);
	if ( !image ) {
	    makeColorBlocks();
	} else if ( !pasteImage( readImage( image ), GR_BUFFER_FRONTBUFFER ) ) {
            testSetException( "could not display image" );
            shutdownSST1();
            return EXCEPTION;
        }
        if((checksum = computeVideoChecksum(sstbase)) == -1) {
            testSetException( "could not compute checksum" );
            shutdownSST1();
            return EXCEPTION;
        }
        shutdownSST1();
        testWriteConfigInt( "sst1test.cfg", image, checksum );
        return PASS;
    } else {
        FxU32 value;
        sst1Idle(sst1InitGetBaseAddr(0));
	if ( !image ) {
	    makeColorBlocks();
	} else {
	    pasteImage( readImage( image ), GR_BUFFER_FRONTBUFFER );
	}
        if((checksum = computeVideoChecksum(sstbase)) == -1) {
            testSetException( "could not compute checksum" );
            shutdownSST1();
            return EXCEPTION;
        }
        if (!image) {
            if (VIS_V_PIX == 256) image = DIAGS_ROOT "rampstd.sbi";
            if (VIS_V_PIX == 384) image = DIAGS_ROOT "rampmed.sbi";
            if (VIS_V_PIX == 480) image = DIAGS_ROOT "ramp640.sbi";
        }
        if ( !testReadConfigInt( "sst1test.cfg", image, &value ) ) {
            testSetException( "could not find reference checksum" );
            shutdownSST1();
            return EXCEPTION;
        }
        shutdownSST1();
        if ( value == checksum ) return PASS;
        else return FAIL;
    }
}

/*------------------------------------------------------------------
  Texture and Image Support
  ------------------------------------------------------------------*/

void testInit( void ) {
#if GLIDE_VERSION > 203
    {
	extern void _GlideInitEnvironment(void);
	_GlideInitEnvironment();
    }
#endif

#ifdef DEFAULT_RES_METHOD
    DEFAULT_RES_METHOD();
#endif

    /* Initialize Frame Buffers */

    frontBuffer = allocBuffer( MAX_X, MAX_Y );
    backBuffer  = allocBuffer( MAX_X, MAX_Y );
    auxBuffer   = allocBuffer( MAX_X, MAX_Y );

    /* Load Textures */
    _decal   = loadTexture( DIAGS_ROOT "decal.3df" );
    _alpha   = loadTexture( DIAGS_ROOT "alpha.3df" );
    _marble  = loadTexture( DIAGS_ROOT "marble.3df" );
    _palette = loadTexture( DIAGS_ROOT "palette.3df" );
    _yiq     = loadTexture( DIAGS_ROOT "yiq.3df" );

    return;
}

extern void freeTexture(Texture **ptr);

void testCleanUp( void ) {
    if (frontBuffer) freeBuffer(&frontBuffer);
    if (backBuffer) freeBuffer(&backBuffer); 
    if (auxBuffer) freeBuffer(&auxBuffer);
    if (_decal) freeTexture(&_decal);
    if (_alpha) freeTexture(&_alpha);
    if (_marble) freeTexture(&_marble);
    if (_palette) freeTexture(&_palette);
    if (_yiq) freeTexture(&_yiq);
}

static void clearBuffers( void ) {
    grDepthMask( FXTRUE );
    grBufferClear( 0x0, 0x0, 0x0 );
    grBufferSwap( 0 );
    grBufferClear( 0x0, 0x0, 0x0 );
    return;
}

static TestCode testBuffers( const char *front, const char *aux, FxBool reference ) {
    grabImage( frontBuffer, GR_BUFFER_FRONTBUFFER );
    grabImage( auxBuffer,   GR_BUFFER_AUXBUFFER );
    if ( reference ) {
        writeImage( frontBuffer, front );
        writeImage( auxBuffer,   aux );
    } else {
        Buffer *tmp;
        tmp = readImage(front);
        if ( !tmp ) {
            testSetException( "couldn't load image file" );
            return EXCEPTION;
        }
        if ( compareImage( tmp, frontBuffer ) ) 
            return FAIL;
        tmp = readImage(aux);
        if ( !tmp ) {
            testSetException( "couldn't load image file" );
            return EXCEPTION;
        }
        if ( compareImage( tmp, auxBuffer ) ) 
            return FAIL;
    }
    return PASS;
}



