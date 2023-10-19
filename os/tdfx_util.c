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
** $Revision: 1.10 $ 
** $Date: 1997/10/16 05:12:41 $ 
**
*/

#include <config.h>
#undef FAIL
#include "sst1test.h"
#include <nsprintf.h>

#ifndef DIAGS_ROOT
# define DIAGS_ROOT "/d0/diags/"
#endif

extern int getch(void);

static TestIO _testIO;
static FxBool _ioInitialized /* = FXFALSE */;

/*-------------------------------------------------------------
  Output
  -------------------------------------------------------------*/

const char *testCodeToString( TestCode code ) {
    switch( code ) {
        case EXCEPTION:  return "EXCEPTION";
        case FAIL:       return "FAIL";
        case PASS:       return "PASS";
        case NOT_TESTED: return "NOT_TESTED";
        default:         return "UNKNOWN";
    }
}

void testOutputInit( TestIO *io ) {
    _testIO        = *io;
    _ioInitialized = FXTRUE;
    return;
}

static void monoScrollWindow( TestWindow *win ) {

    /* Copy Contents of Window Up One Line */
    memmove( MONO_BASE,
             MONO_BASE+MONO_SCANLINE, 
             MONO_SCANLINE * (win->height - 1) );
    /* Clear Last Line */
    memset( MONO_BASE+(MONO_SCANLINE*(win->height-1)),
            0,
            MONO_SCANLINE );

}

static void monoCharXY( FxU32 x, FxU32 y, char c, char attrib ) {
    *(MONO_BASE + y * MONO_SCANLINE + x * 2)     = c;
    *(MONO_BASE + y * MONO_SCANLINE + x * 2 + 1) = attrib;
}

void monoCls( void ) {
    memset( MONO_BASE, 0, MONO_SCANLINE * 25 );
    _testIO.monoWindow.x = 0;
    _testIO.monoWindow.y = 0;
}

void testOutput( char *fmt, ... ) {
    if ( _ioInitialized ) {
        int     cnt;
        char    buffer[1024];
        va_list argptr;

        /* Decode format into a string */
        va_start( argptr, fmt );
        cnt = twi_vfprintf(0, buffer, sizeof(buffer), fmt, argptr);
/*        cnt = vsprintf( buffer, fmt, argptr ); */
        va_end( argptr );

        if ( _testIO.outputDevices & OUTPUT_CONSOLE )
            fputs( buffer, _testIO.console );

        if ( _testIO.outputDevices & OUTPUT_MONO ) {
            const char *c = buffer;
            while( *c ) {
                switch( *c ) {
                case '\n':
                    _testIO.monoWindow.y++;
                case '\r':
                    _testIO.monoWindow.x = 0;
                    if ( _testIO.monoWindow.y >= 
                         _testIO.monoWindow.height ) {
                        _testIO.monoWindow.y = 
                            _testIO.monoWindow.height - 1;
                        monoScrollWindow( &_testIO.monoWindow );
                    }
                    break;
                default:
                    if ( _testIO.monoWindow.x >= 
                         _testIO.monoWindow.width ) {
                        _testIO.monoWindow.x = 0;
                        _testIO.monoWindow.y++;
                    }
                    monoCharXY( _testIO.monoWindow.x, 
                                _testIO.monoWindow.y, 
                                *c, 
                                0x7 );
                    _testIO.monoWindow.x++;
                    break;
                }
                c++;
            }
        }
    }
}


/*----------------------------------------------------------------
  Formatted Output and User Input
  ----------------------------------------------------------------*/
void _drain( void ) {
    extern int kbhit(void);
    while( kbhit() ) getch();
}

#if 0
FxBool testAskBegin( void ) {
    testOutput( " \n" );
    testOutput( " \n" );
    testOutput( "*************************\n" );
    testOutput( "Insert Board To Be Tested\n" );
    testOutput( "Press any key to begin or\n" );
    testOutput( "'q' to exit.             \n" );
    testOutput( "*************************\n" );
    testOutput( " \n" );
    testOutput( " \n" );
    fflush( stdout );
    _drain();
    if ( getch() == 'q' ) {
        return FXTRUE;
    }
    return FXFALSE;
}
#endif

int testResultMatrix( FxU32 nTests, TestFunction testArray[] ) {
    FxU32  test;
    FxU32  line;
    FxBool passed = 0;
    line = 0;

    for( test = 0; test < nTests; test++ ) {
        if ( testArray[test].packet.result != PASS )
            ++passed;
    }

    if ( !passed ) {
        testOutput( "**********************\n" );
        testOutput( "Board Passed Testing. \n" );
        testOutput( "**********************\n" );
    } else {
        testOutput( "**********************\n" );
        testOutput( "Board Failed Testing. \n" );
        testOutput( "TEST         CONDITION\n" );
        line += 3;
        for( test = 0; test < nTests; test++ ) {
            testOutput( "%-10s   %s\n", 
                        testArray[test].name,
                        testCodeToString( testArray[test].packet.result ) );
            line++;
            if ( line == 24 ) {
                testOutput( "--MORE\n" );
                getch();
                line = 0;
            }
        }
    }
    return passed;
}


void testPrintConfig( TestConfig *config ) {
    testOutput( "fbiRev: %d\n", config->fbiRev );
    testOutput( "tmuRev: %d\n", config->tmuRev );
    testOutput( "nTMUs:  %d\n", config->nTMUs );
    testOutput( "tmuMem: %d\n", config->tmuMem );
    testOutput( "fbiMem: %d\n", config->fbiMem );
    testOutput( "sli:    %s\n", config->sli?"YES":"NO" );
    return;
}

void testPrintArray( FxU32 nTests, TestFunction testArray[] ) {
    FxU32 index;
    for( index = 0; index < nTests; index++ ) {
        testOutput( "%s\n", testArray[index].name );
    }
}

void testContinue( void ) {
    _drain();
    testOutput( "Press Any Key to Continue.\n" );
    getch();
    return;
}

FxBool testYesNo( void ) {
    char inchar = 0;
    _drain();
    while( !inchar ) {
        inchar = getch();
        switch( inchar ) {
            case 'y':
            case 'Y':
                return( FXTRUE );
                break;
            case 'n':
            case 'N':
                return( FXFALSE );
                break;
            default:
                inchar = 0;
                break;
        }
    }
    return FXFALSE;
}

/*--------------------------------------------------------------------
  PCI
  --------------------------------------------------------------------*/
FxBool testSetBaseAddress( FxU32 baseAddress ) {
#if 0
    FxU32 vendorID =  _3DFX_PCI_ID;		/* 3Dfx */
    FxU32 deviceID = 0xFFFF;			/* any card */
    FxI32 sizeOfCard = -1;      		/* some tarolli hack??? */
    FxU32 deviceNumber;
    
    if ( !pciMapCard( vendorID, deviceID, sizeOfCard, &deviceNumber ) ) {
        return FXFALSE;
    }
    pciSetConfigData( PCI_BASE_ADDRESS_0, deviceNumber, &baseAddress );
#endif
    return FXTRUE;
}


/*--------------------------------------------------------------------
  Config File Management
  --------------------------------------------------------------------*/
FxBool testWriteConfigInt( char *file, char *tag, FxU32 value ) {
#if 0
    FILE *infile;
    FILE *outfile;

    infile  = fopen( file, "r" );
    outfile = fopen( "tmp.tmp", "w" );
    if ( !outfile ) {
        testOutput( "writeConfigInt(): Error, couldn't open tmp file tmp.tmp.\n" );
        return FXFALSE;
    }

    if ( !infile ) {
        fprintf( outfile, "%s\n0x%lX\n", tag, value );
        fclose( outfile );
    } else {
        char tmpTag[64];
        FxU32 tmpValue;
        FxBool found = FXFALSE;
	extern int unlink( const char *name );
        while( fscanf( infile, "%s\n0x%lX\n", tmpTag, &tmpValue ) == 2 ) {
            if( strcmp( tmpTag, tag ) ) {
                fprintf( outfile, "%s\n0x%lX\n", tmpTag, tmpValue );
            } else {
                fprintf( outfile, "%s\n0x%lX\n", tag, value );
                found = FXTRUE;
            }
        }
        if ( !found ) {
            fprintf( outfile, "%s\n0x%lX\n", tag, value );
        }
        fclose( infile );
        fclose( outfile );
        if ( unlink( file ) ) {
            testOutput( "Error unlinking %s.\n", file );
            return FXFALSE;
        }
    }
    if ( rename( "tmp.tmp", file ) ) {
        testOutput( "Error renaming %s to %s.\n", "tmp.tmp", file );
        return FXFALSE;
    }
#else
    printf("file = \"%s\", \"%s\" = 0x%08lX\n", file, tag, value);
#endif
    return FXTRUE;
}

#if (SST_GAME&SST_RUSH)
#define MED_CHKSUM 0x00C20EA4
#else
#define MED_CHKSUM 0x00DDF4D9
#endif

static const struct cfgs {
    char *tag;
    FxU32 value;
} cfgs[] = {
    {DIAGS_ROOT "rampstd.sbi", 0x007F01EF},
    {DIAGS_ROOT "rampmed.sbi", MED_CHKSUM},
    {DIAGS_ROOT "ramp640.sbi", 0x004F0533},
    { 0, 0 }
};

FxBool testReadConfigInt( char *file, char *tag, FxU32 *value ) {
    FxBool found = FXFALSE;
    const struct cfgs *cfg = cfgs;
    if (tag) {
	while (cfg->tag) {
	    if (strcmp(tag, cfg->tag) == 0) {
		*value = cfg->value;
		found = FXTRUE;
		break;
	    }
	    ++cfg;
	}
    } else {
	*value = cfg->value;
	found = FXTRUE;
    }
    return found;
}

/*------------------------------------------------------------------
  Exceptions 
  ------------------------------------------------------------------*/
static const char * _exception;

void testSetException( const char *eString ) {
    _exception = eString;
    return;
}

const char *testGetException( void ) {
    if (!_exception) _exception = "no exception registered";
    return _exception;
}


/*--------------------------------------------------------------------
  Textures
  --------------------------------------------------------------------*/
Texture *loadTexture( char *file ) {
    Texture      *t;

    t = calloc( sizeof( Texture ), 1 );
    if (!t) goto no_heap;

    if ( !gu3dfGetInfo( file, &t->info ) ) {
        fprintf( stdout, "loadTexture(): Couldn't load %s.\n", file );
        exit( -1 );
    }

    if ( ( t->info.data = malloc( t->info.mem_required ) ) == 0 ) {
no_heap:
        fprintf( stdout, "loadTexture(): Heap exhausted loading %s.\n", file );
        exit( -1 );
    }

    if ( !gu3dfLoad( file, &t->info ) ) {
        fprintf( stdout, "loadTexture(): Couldn't load %s.\n", file );
        exit( -1 );
    }

    t->mmid = (GrMipMapId_t)GR_NULL_MIPMAP_HANDLE;

    return t;
}

void freeTexture( Texture **ptr ) {
    Texture      *t;

    t = *ptr;
    free(t->info.data);
    free(t);
    *ptr = 0;
    return;
}

void dlTexture( Texture *t, FxU32 tmu, FxU32 mmLevel, FxU32 trilinear ) {
    t->mmid = guTexAllocateMemory( tmu,
                                   (FxU8)mmLevel,
                                   t->info.header.width, t->info.header.height,
                                   t->info.header.format,
                                   GR_MIPMAP_NEAREST,
                                   t->info.header.small_lod, t->info.header.large_lod,
                                   t->info.header.aspect_ratio,
                                   GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP,
                                   GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR,
                                   0.0F,
                                   trilinear );

    if ( t->mmid == GR_NULL_MIPMAP_HANDLE ) {
        fputs( "loadTexture(): Insufficient texture memory to "
                         "load texture.\n", stdout );
        exit( -1 );
    }

    guTexDownloadMipMap( t->mmid, t->info.data, &t->info.table.nccTable );
    return;
}
