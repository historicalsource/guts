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
** $Revision: 1.7 $ 
** $Date: 1997/09/04 23:25:01 $ 
**
*/

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>

#undef FAIL
#include "sst1test.h"

const char * const usage = 
#if MAKE_REFERENCES
    "[-m]"
#endif
#if 0
    "[-o][-l printer]"
#endif
    "[-f fb_mem][-n num_tmus][-t tmu_mem][-rf ver][-rt ver][-s]\n"
#if MAKE_REFERENCES
    "-m<ake reference> - generate a new set of reference images.\n"
#endif
    "-f fb_mem - specify amount of frame buffer memory [1,2,4]\n"
    "   default: 2\n"
    "-n num_tmus - specify number of texelFx chips [1,2]\n"
    "   default: 1\n"
    "-t tmu_mem - specify texture memory per texelFX [1,2,4]\n"
    "   default: 4\n"
    "-rf fbi_revision - specify pixelFx chip revision number [1,2]\n"
    "    default: 1\n"
    "-rt tmu_revision - specify texelFx chip revision number [0,1]\n"
    "    default: 1\n"
    "-s<li> - specify scanline interleave testing.\n"
    "    default: disabled\n"
#if 0
    "-o<utput to mono> - Send all status messages to a monochrome\n"
    "                    monitor as well as stdout.\n"
#endif
;

const char * const doc = 
    "3Dfx Interactive Inc. Obsidian manufacturing test.\n"
    "Version 2.0 $Revision: 1.7 $\n";

extern void testCleanUp(void);

int fx_tests( int argc, char *argv[] ) {
    FxBool       done, reference;
    FxU32        nTests;
    TestFunction *testArray;
    TestConfig   config;
    TestIO       io;
    const char   *appName = 0;
    char 	 c, *s;
    int		 status;

    /*------------------------------------------------
      Initialize Data
      ------------------------------------------------*/
    done          = FXFALSE;
    reference     = FXFALSE;
    nTests        = 0;
    testArray     = 0;
#if !defined(TDFX_FBIREV)
# define TDFX_FBIREV 1
#endif
#if !defined(TDFX_TMUREV)
# define TDFX_TMUREV 1
#endif
#if !defined(TDFX_NTMUS)
# define TDFX_NTMUS 1
#endif
#if !defined(TDFX_TMUMEM)
# define TDFX_TMUMEM 4
#endif
#if !defined(TDFX_FBIMEM)
# define TDFX_FBIMEM 2
#endif
    config.fbiRev = TDFX_FBIREV;
    config.tmuRev = TDFX_TMUREV;
    config.nTMUs  = TDFX_NTMUS;
    config.tmuMem = TDFX_TMUMEM;
    config.fbiMem = TDFX_FBIMEM;
    config.sli    = FXFALSE;

    io.outputDevices = OUTPUT_CONSOLE;
    io.console       = stdout;
    io.monoWindow.x      = 0;
    io.monoWindow.y      = 0;
    io.monoWindow.width  = 80;
    io.monoWindow.height = 25;

    testInit();
    testOutputInit( &io );

    /*-------------------------------------------------
      Scan Command Line
      -------------------------------------------------*/
    appName = argv[0];
    for (--argc, ++argv; argc > 0; --argc, ++argv) {
	char *t, c2;
        s = *argv;
        if (*s++ != '-') break; /* done with options */
	c2 = ' ';
        switch (c = *s++) {
#if MAKE_REFERENCES
        case 'm':
            reference = FXTRUE;
            break;
#endif
#if 0
        case 'o':
            io.outputDevices |= OUTPUT_MONO;
            testOutputInit( &io );
            break;
#endif
        case 'f':
	    if (*s == 0) { ++argv; --argc; s = *argv; }
	    if (argc <= 0 || *s == 0) goto no_arg;
	    config.fbiMem = strtol(s, &t, 10);
	    if (s == t || *t != 0) goto no_arg;
	    continue;
        case 'n':
	    if (*s == 0) { ++argv; --argc; s = *argv; }
	    if (argc <= 0 || *s == 0) goto no_arg;
	    config.nTMUs = strtol(s, &t, 10);
	    continue;
        case 't':
	    if (*s == 0) { ++argv; --argc; s = *argv; }
	    if (argc <= 0 || *s == 0) goto no_arg;
	    config.tmuMem = strtol(s, &t, 10);
	    continue;
        case 'r':
            switch( (c2 = *s++) ) {
            case 'f':
		if (*s == 0) { ++argv; --argc; s = *argv; }
		if (argc <= 0 || *s == 0) goto no_arg;
		config.fbiRev = strtol(s, &t, 10);
		continue;
            case 't':
		if (*s == 0) { ++argv; --argc; s = *argv; }
		if (argc <= 0 || *s == 0) goto no_arg;
		config.tmuRev = strtol(s, &t, 10);
		continue;
            default:
                testOutput( "%s:%s\n", appName, usage );
                return APP_FAIL;
            }
            break;
        case 's':
            config.sli = FXTRUE;
            break;
no_arg:
	    fprintf(stderr, "Expected number after -%c%c\r\n", c, c2);
        default:
            testOutput( "%s:%s\n", appName, usage );
            return APP_FAIL;
        }
    }
    testOutput( "%s\n", doc );

    /*-------------------------------------------------
      Create Test Matrix Based On Config 
      -------------------------------------------------*/
    testCreateTestArray( &nTests, &testArray, &config, reference );

#if 0
    testPrintConfig( &config );
#endif

#ifdef SST1TEST_DEBUG
    testPrintArray( nTests, testArray );
#endif

#if 0
    if ( !reference ) done = testAskBegin();
#endif
    status = 0;
    if (!reference && VIS_V_PIX >= 384) {
	struct	stat st;
	if (stat("/d0/diags/GLIDE00a.sbi", &st) < 0) {
	    testOutput( "UNABLE TO STAT /d0/diags/GLIDE00a.sbi. ABORTING FURTHER TESTS.\n" );
	    status = 1;
	} else {
	    if (st.st_mtime < 0x340A67F0) {		/* EPOCH is 01-SEP-97 */
		testOutput( "DIAG FILES ARE OUT OF DATE. YOU NEED DISK WITH UPDATED DIAG FILES.\n");
		testOutput( "One or more of the tests will probably fail as a result.\n" );
	    }
	}
    }
    if (!status) {
	while( !done ) {
	    FxU32 test;
	    for( test = 0; test < nTests; test++ ) {
		testArray[test].test( &testArray[test].packet );
		if ( testArray[test].packet.result == EXCEPTION )  {
		    testOutput( "TEST EXCEPTION! ABORTING FURTHER TESTS.\n" );
		    testOutput( "%s\n", testGetException() );
		    testContinue();
		    ++status;
		    break;
		}
	    }
	    if ( !reference ) status += testResultMatrix( nTests, testArray );
#if 0
	    if ( reference ) 
		done = FXTRUE;
	    else
		done = testAskBegin();
#else
	    done = FXTRUE;
#endif
	}
    }
    testCleanUp();
    return status;
}
