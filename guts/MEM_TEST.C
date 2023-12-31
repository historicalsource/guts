/*		mem_test.c
 *		Copyright 1994 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
*/
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: mem_test.c,v 1.13 1997/07/01 22:01:01 albaugh Exp $";
#endif
/*	This file serves as a "reference implementation" of the
 *	GUTS ram tests. The assembly-level register-call versions
 *	have been omitted, for obvious reasons. This code could
 *	be used as is, in a non-time-critical diagnostic, or the
 *	assembly code resulting from its compilation could be used
 *	as a starting point, or one could merely view it as a
 *	slightly more readable version of the algoithm. Those tests
 *	marked AMS were derived from the Applied Microsystems "SF" tests,
 *	with their permission. Use outside Atari Games (TWI) should
 *	probably be avoided unless permission is once again sought.
 *
 *
 *	Changes to this file should be forwarded to Daves Shepperd and Akers
 */
/*	While bringing up new hardware, it is often helpful to have some
 * simple "strobe" as soon as possible after an error is detected. The
 * "BLIP" macros are intended to allow this to be customized relatively
 * painlessly.
 */
#if MINH_WATCH
/* THIS HACK STRICTLY FOR USE BY Minh and Brian, debugging Williams-produced
 * Area51/JackHammer boards.
 */
extern int cojag_rev;
extern unsigned long *kluge_watch;

#define BLIP_LOCALS \
    volatile unsigned char *latp; \
    unsigned long lval,lvalb;

#define BLIP_INIT \
    do { if ( cojag_rev == 4 ) { latp = &COJAG_2_CF_REG; \
         lval = (*latp ^ CM_VOLCTRL_DATA) & ~CM_VCR_REC;	\
    	 lvalb = lval | CM_VCR_REC;}			 \
	else { latp = (unsigned char *)&lval; lval = lvalb = 0; } } while (0)

#define BLIP \
    do { *latp = lvalb; *latp = lval; } while (0)
#define PARANOID_FILL (1)
#endif /* MINH_WATCH */

#ifdef ZRE_CSR
/* For example, If there is a ZRE on the stack, the following might be used
 * to blink the XLED on the ZRE.
 */

#define BLIP_LOCALS \
    volatile unsigned long *latp; \
    unsigned long lval,lvalb;

#define BLIP_INIT \
    do { latp = &ZRE_CSR; \
         lval = *latp;	\
    	 lvalb = lval ^ (1<<B_ZRE_XLED); } while (0)

#define BLIP \
    do { *latp = lvalb; *latp = lval; } while (0)
    
#else
#ifdef ZTV_CTL
/* For example, on a ZTV stack, the following might be used
 */
extern unsigned long ztv_mod_latch();

#define BLIP_LOCALS \
    volatile unsigned long *latp; \
    unsigned long lval,lvalb;

#define BLIP_INIT \
    do { latp = &ZTV_CTL;		\
         lval = ztv_mod_latch(0);	\
         lvalb = lval | 0x4000; } while (0)

#define BLIP \
    do { *latp = lvalb; *latp = lval; } while (0)
    
#endif
#endif

#ifndef BLIP
#define BLIP do {;} while (0)
#define BLIP_INIT do {;} while (0)
#define BLIP_LOCALS
#endif

#define STAND_ALONE (0)
#if STAND_ALONE
/*
 *	Structure to describe RAM area to test
 */
struct rdb {
	unsigned long * rd_base;	/*  Starting address  */
	unsigned long rd_len;	/*  Length in bytes  */
	unsigned long rd_misc;	/*  Which bits don't exist */
};
/*
 *	Structure for optional return of RAM test results
 */
struct rrb {
	unsigned long * rr_addr;	/*  Where it choked  */
	unsigned long rr_expected;	/*  What it wanted  */
	unsigned long rr_got;	/*  What it got */
	int rr_test_no;	/*  Which test  */
};
#define watchdog() do {;} while (0)
#else
/*	Get structure definitions from config.h
 */
#include <config.h>
#ifndef watchdog
#ifdef WDOG
#define watchdog() do {WDOG = 0;} while (0)
#else
#define watchdog() do {;} while (0)
#endif /* WDOG defined */
#endif /* watchdog not defined */
#include <os_proto.h>
#endif /* STAND_ALONE */


#ifndef WDOG_LOOP_CNT
#ifdef WDI_MASK
#define WDOG_LOOP_CNT (WDI_MASK)
#else
#define WDOG_LOOP_CNT (0x1000)
#endif	/* base LOOP_CNT on old WDI_MASK */
#endif /* WDOG_LOOP_CNT defined? */

/*		UNDER CONSTRUCTION:
 */

/*		f_ram_test(desc,result)
 *	Test ram area described by desc, with optional detailed result.
 *	Returns:
 *		==0 for success,
 *		!=0 for failure (1 in erroneous bit location(s))
 *	Optionally fills in struct rrb with more detailed information.
 */
#define CUSHION (42)	/* longwords of "unexpected" stack frame */
#define NOISY (0)

static void logit(test_no)
int test_no;
{
#if NOISY
    if ( test_no > 1 )txt_cstr(" Passed",MNORMAL_PAL);
    txt_str(2,test_no+5,"Test ",MNORMAL_PAL);
    txt_cdecnum(test_no,2,LJ_NF,MNORMAL_PAL);
#endif
}

unsigned long
f_ram_test(desc, result)
struct rdb *desc;
struct rrb *result;
{
    /* Pointers within, to first loc., to last loc. */
    volatile unsigned long *addr,*start,*end;

    /* len starts out in bytes, but both it and t_len are
     * in longwords for most of the test.
     */
    unsigned long len,t_len;

    /* ignore and check are complements of each other, to
     * "spoon feed" some stupid compilers which would otherwise
     * re-complement ignore every time check is needed, even
     * on machines with AND_NOT !
     */
    unsigned long ignore,check;

    /* w_len is used to limit the length of the "inner loop" to
     * what can safely be done between watchdog()s.
     */
    unsigned long w_len;

    /* Allocate variables, if any, needed for simple error
     * indication.
     */
    BLIP_LOCALS
    unsigned long expect,got,retval,flipper;
    int test_no = 0;
    unsigned long marker;	/* _may_ be "bottom" of our stack frame */
#if MINH_WATCH
    volatile unsigned long *wp;
    unsigned long wval;
#endif

    BLIP_INIT;

    start = desc->rd_base;
    len = desc->rd_len;
    if ( len == 0 ) {
	/* Kluge for when we test our own working RAM. Try to stay well
	 * below our own stack frame.
	 */
	len = (&marker - start)-CUSHION;
	len *= sizeof(unsigned long);
    }
    len /= sizeof(unsigned long);
    end = start+len-1;
#if NOISY
    txt_str(2,3,"Testing 0x",MNORMAL_PAL);
    txt_chexnum(len,8,RJ_ZF,MNORMAL_PAL);
    txt_cstr(" longwords @ 0x",MNORMAL_PAL);
    txt_chexnum((U32)start,8,RJ_ZF,MNORMAL_PAL);
#endif

    ignore = 0;
    if ( (check = desc->rd_misc) != 0 ) {
	/* "test with gaps" */
	ignore = ~check;
#if NOISY
	txt_str(2,4," Except bits: 0x ",MNORMAL_PAL);
	txt_chexnum(ignore,8,RJ_ZF,MNORMAL_PAL);
#endif
    } else check = ~0;

#if MINH_WATCH
    wp = (VU32 *)kluge_watch;
    wval = 0;
#endif

    retval = 0;
    expect = 0;
    got = ~expect;
    /* In reality, the while loops below are so constructed that flipper
     * is never really "used before set", but the flow-analysis is not
     * quite bright enough to notice, so we initialize it to mute the
     * warning.
     */
    flipper = 0;		/* Keeps "used before set" warning away */
    /* First, fill RAM with 0, to provide "expectation" for test 1
     */
    addr = start;
    t_len = len;
    while ( t_len ) {
	w_len = t_len;
	if ( w_len > WDOG_LOOP_CNT ) w_len = WDOG_LOOP_CNT;
	t_len -= w_len;
#if !(PARANOID_FILL)
	while ( w_len-- ) *addr++ = 0;
#else
	expect = 0;
	while ( w_len-- ) {
	    *addr = 0;
	    if ( (got = *addr++) != 0) BLIP;
	}
#endif
	watchdog();
    }
    /* Test 1-2:  Starting with presumed zeroes, check for word being zero,
     *		then flip one bit at a time, starting from MSB, checking
     *		between each write. Moves from start..end, then scans
     *		back by words, looking for new expected value (~0)
     */
    ++test_no;
    addr = start;
    end = start+len-1;
    t_len = len;
    logit(test_no);
    if ( ignore ) {
	/* We need to skip over some bits in the word.
	 */
	while ( addr <= end ) {
	    expect = 0;
	    flipper = 0x80000000;
	    while ( flipper & ignore ) flipper >>= 1;
	    while ( flipper ) {
		if ( ((got = *addr) ^ expect) & check ) break;
		expect ^= flipper;
		do  { flipper >>= 1; } while ( flipper & ignore );
		*addr = expect;
	    }
	    if ( flipper ) {
		BLIP;
		test_no = -test_no;
		break;
	    }
	    if ( ++addr == 0 ) break;
	}
    } else {
	/* More or less the same code, but we can ignore 'ignore'
	 */
	while ( t_len ) {
	    w_len = t_len;
	    if ( w_len > (WDOG_LOOP_CNT>>5) ) w_len = (WDOG_LOOP_CNT>>5);
	    t_len -= w_len;
	    while ( w_len-- ) {
		expect = 0;
		flipper = 0x80000000;
		while ( flipper ) {
		    if ( (got = *addr) != expect ) break;
		    expect ^= flipper;
		    flipper >>= 1;
		    *addr = expect;
		}
#if WANTS_AMS_BUG
		/* The original AMS test did not check last write.
		 */
		if ( flipper )
#else
		/* We might want to check the last write _before_ moving
		 * on to the next word.
		 */
		if ( flipper || ((got = *addr) != expect) )
#endif
		{
		    BLIP;
		    test_no = -test_no;
		    break;
		}
#if MINH_WATCH
		/* Trying to find bizarre RAM problems with
		 * Single-board CoJag's, Minh asked for a modification
		 * to allow "watching" a specified location for
		 * unexpected changes. This is specified in the
		 * CUSTOM TEST. Note that we have to change our
		 * "watch value" (wval) as the address to test
		 * passes our "watch point" (wp).
		 */
		if ( wp ) {
		    if ( wp == addr ) wval = ~wval;
		    if ( *wp != wval ) {
			BLIP;
			test_no = -test_no;
			break;
		    }
		}
#endif
		if ( ++addr == 0 ) break;
	    }
	    if ( test_no < 0 ) break;
	    watchdog();
	}
    } /* endif ( ignore ) */
    if ( test_no >= 0 ) { 
	++test_no;
	logit(test_no);
	/* this is phrased oddly to avoid problems when either start or
	 * end is 0.
	 */
	addr = end+1;
	if ( ignore ) {
	    /* Need to ignore some bits, so need to 'AND' with 'check'
	     */
	    while ( addr != start ) {
		got = *--addr;
		if ( (got ^ expect) & check ) {
		    BLIP;
		    test_no = -test_no;
		    break;
		}
	    }
	    if ( (addr == start)
	      && ( ((got = *addr) ^ expect) & check )
	      && (test_no >= 0) ) {
		/* failed on very last word...
		 */
		BLIP;
		test_no = - test_no;
	    }
	} else {
	    /* Ignoring 'ignore', whole-word test.
	     */
	    t_len = len;
	    while ( t_len ) {
		w_len = t_len;
		if ( w_len > WDOG_LOOP_CNT ) w_len = WDOG_LOOP_CNT;
		t_len -= w_len;
		while ( w_len-- ) {
		    got = *--addr;
		    if ( got != expect ) {
			BLIP;
			test_no = -test_no;
			break;
		    }
		}
		watchdog();
		if ( got != expect ) break;
	    }
	} /* endif ( ignore ) */
    } /* endif (testno >= 0) */

    if ( test_no >= 0 ) {
	/* Test 3-4: 	Starting with presumed "all ones", check each word,
	 *		then flip one bit at a time, starting from MSB, checking
	 *		between each write. Moves from start..end
	 */
	++test_no;
	logit(test_no);
	addr = start;
	end = start+len-1;

	if ( ignore ) {
	    /* Need to ignore some bits in the word.
	     */
	    while ( addr <= end ) {
		expect = ~0;
		flipper = 0x80000000;
		while ( flipper & ignore ) flipper >>= 1;
		while ( flipper ) {
		    if ( ((got = *addr) ^ expect) & check ) break;
		    expect ^= flipper;
		    do  { flipper >>= 1; } while ( flipper & ignore );
		    *addr = expect;
		}
		if ( flipper ) {
		    BLIP;
		    test_no = -test_no;
		    break;
		}
		if ( ++addr == 0 ) break;
	    }
	} else {
	    /* ignoring ignore, and testing whole words.
	     */
	    t_len = len;
	    while ( t_len ) {
		w_len = t_len;
		if ( w_len > (WDOG_LOOP_CNT>>5) ) w_len = WDOG_LOOP_CNT>>5;
		t_len -= w_len;
		while ( w_len-- ) {
		    expect = ~0;
		    flipper = 0x80000000;
		    while ( flipper ) {
			if ( (got = *addr) != expect ) break;
			expect ^= flipper;
			flipper >>= 1;
			*addr = expect;
		    }
#if WANTS_AMS_BUG
		    /* The original AMS test did not check last write.
		     */
		    if ( flipper )
#else
		    /* We might want to check the last write _before_ moving
		     * on to the next word.
		     */
		    if ( flipper || ((got = *addr) != expect) )
#endif
		    {
			BLIP;
			test_no = -test_no;
			break;
		    }
#if MINH_WATCH
		    /* Trying to find bizarre RAM problems with
		     * Single-board CoJag's, Minh asked for a modification
		     * to allow "watching" a specified location for
		     * unexpected changes. This is specified in the
		     * CUSTOM TEST. Note that we have to change our
		     * "watch value" (wval) as the address to test
		     * passes our "watch point" (wp).
		     */
		    if ( wp ) {
			if ( wp == addr ) wval = ~wval;
			if ( *wp != wval ) {
			    BLIP;
			    test_no = -test_no;
			    break;
			}
		    }
#endif
		    if ( ++addr == 0 ) break;
		}
		watchdog();
		if ( test_no < 0 ) break;
	    } /* end while ( t_len ) */
	} /* end if ( ignore ) */
    } /* endif (testno >= 0) */
    if ( test_no >= 0 ) {
	++test_no;
	logit(test_no);
	/* this is phrased oddly to avoid problems when either start or
	 * end is 0.
	 */
	addr = end+1;
	if ( ignore ) {
	    /* ignoring some bits in a word.
	     */
	    while ( addr != start ) {
		got = *--addr;
		if ( (got ^ expect) & check ) {
		    BLIP;
		    test_no = -test_no;
		    break;
		}
	    }
	    if ( (addr == start)
	      && ( ((got = *addr) ^ expect) & check )
	      && (test_no >= 0) ) {
		/* failed on very last word...
		 */
		BLIP;
		test_no = - test_no;
	    }
	} else {
	    /* Testing only whole words.
	     */
	    t_len = len;
	    while ( t_len ) {
		w_len = t_len;
		if ( w_len > WDOG_LOOP_CNT ) w_len = WDOG_LOOP_CNT;
		t_len -= w_len;
		while ( w_len-- ) {
		    got = *--addr;
		    if ( got != expect ) {
			BLIP;
			test_no = -test_no;
			break;
		    }
		}
		watchdog();
		if ( got != expect ) break;
	    }
	} /* endif ( ignore ) */
    } /* endif (testno >= 0) */
#if NOISY
    if ( test_no > 0 ) {
	txt_str(2,test_no+6,"RAM Passed all tests so far",MNORMAL_PAL);
    } else {
	txt_cstr("FAILED",ERROR_PAL);
    }
    prc_delay(240);
#endif
    if ( result ) {
	result->rr_test_no = test_no < 0 ? -test_no : test_no ;
	result->rr_addr = (unsigned long *)addr;
	result->rr_expected = expect & check;
	result->rr_got = got & check;
    }
    return (expect ^ got)& check;
}

unsigned long
bram_test(base, len, misc, result)
U32 *base,len,misc;
struct rrb *result;
{
    struct rdb desc;

    desc.rd_base = base;
    desc.rd_len = len;
    desc.rd_misc = misc;
    return f_ram_test(&desc,result);
}

/* Rather than simply pare the full test, this new quick test is
 * based on an algorithm brought to my (MEA) attention by Senthil
 * Vinayagam. Memory is "backgrounded" in a pattern with length
 * of three, which is mutually prime to any "naturally occuring"
 * likely problem in addressing. The three patterns are then
 * "rolled up" via exclusive-or, and should cancel at the end
 * of memory. By repeating the test in all three "phases", every
 * bit is written at least once as a one or zero, with each
 * neighbor in the oposite state. While this test is unlikely
 * to be as effective as the full test in locating noise or
 * timing related problems, it should find the majority of
 * "stuck at" faults.
 *				19OCT95 Mike Albaugh
 */

#define PAT1 (0x6DDBB66D)
#define PAT2 (0xDBB66DDB)
#define PAT3 (0xB66DDBB6)

unsigned long
q_ram_test(desc, result)
struct rdb *desc;
struct rrb *result;
{
    U32 wdata,rdata,check;
    U32 pat1,pat2,pat3;
    VU32 *dp;
    int wd_len;
    int loop_len,loop_cnt;
    int pass = 0;

    pat1 = PAT1;
    pat2 = PAT2;
    pat3 = PAT3;
    wdata = 0;
    /* rd_misc field for RAM is bits to check, unless it
     * is zero, in which case we check them all. Don't Ask!
     */
    check = desc->rd_misc;
    if ( !check ) check = ~0;
    while ( pass < 3 ) {
	int inner_len;
	/* get length in (32-bit) words, then trim it to
	 * 0, mod 3. This allows an unrolled loop, although
	 * it costs a divde by three. On the 68EC020, the
	 * cost of the divide is "paid for" by the lower
	 * number of branches at a loop_len of about 20,
	 * corresponding to a memory area of 240 bytes. Since
	 * the quick-test will likely be nearly instantaneous
	 *  on such small areas, I'm losing no sleep (MEA)
	 */
	wd_len = (desc->rd_len + sizeof(U32)-1)/sizeof(U32);
	loop_len = wd_len/3;
	wd_len -= (loop_len*3);

	dp = desc->rd_base;

	for ( loop_cnt = loop_len ; loop_cnt ; ) {
	    inner_len = loop_cnt;
	    /* make inner_len the largest multiple of three that
	     * does not exceed WDOG_LOOP_CNT, so we can maintain
	     * alignment with our patterns of three.
	     */
	    if ( inner_len > ((WDOG_LOOP_CNT/3)*3) ) inner_len = (WDOG_LOOP_CNT/3)*3;
	    loop_cnt -= inner_len;
	    while ( inner_len ) {
		*dp++ = pat1;
		*dp++ = pat2;
		*dp++ = pat3;
		--inner_len;
	    }
	    watchdog();
	}
	/* There are at most two words left to store.
	 */
	if ( wd_len ) {
	    *dp++ = pat1;
	    --wd_len;
	}
	if ( wd_len ) {
	    *dp++ = pat2;
	    --wd_len;
	}
	wd_len = (desc->rd_len + sizeof(U32)-1)/sizeof(U32);
	dp = desc->rd_base;
	while ( wd_len > 1 ) {
	    inner_len = wd_len-1;
	    if ( inner_len > WDOG_LOOP_CNT ) inner_len = WDOG_LOOP_CNT;
	    wd_len -= inner_len;
	    while ( --inner_len >= 0 ) {
		rdata = *dp++;
		wdata = *dp;
		wdata ^= rdata;
		*dp = wdata;
	    }
	    watchdog();
	}
	if ( wd_len ) {
	    rdata = *dp;
	    wdata ^= rdata;
	    --wd_len;
	}
	if ( ++pass < 3 ) {
	    U32 hold = pat1;
	    pat1 = pat2;
	    pat2 = pat3;
	    pat3 = hold;
	}
	if ( wdata &= check ) break;
    }
    if ( wdata && result ) {
	/* Error somewhere, we report it at base */
	result->rr_addr = desc->rd_base;
	result->rr_expected = 0;
	result->rr_got = wdata;
	result->rr_test_no = pass;
    }
    return wdata;
}

unsigned long
rom_cksum(desc)
struct rdb *desc;
{
    U32 hi,lo;
    U32 hi_mask, lo_mask;
    U32 *rp,data;
    int loop_len,wd_len;

    hi_mask = 0xFF00FF00;
    lo_mask = ~hi_mask;

    hi = desc->rd_misc & hi_mask;
    lo = desc->rd_misc & lo_mask;

    wd_len = (desc->rd_len + sizeof(U32)-1)/sizeof(U32);
    rp = desc->rd_base;
    while ( wd_len ) {
	loop_len = wd_len;
/* Limit loop to 255, regardless of WDOG_LOOP_CNT, so that
 * carries do not mess us up.
 */
	if ( loop_len > 255 ) loop_len = 255;
	wd_len -= loop_len;
	while ( loop_len-- ) {
	    data = *rp++;
	    hi += (data & hi_mask);
	    lo += (data & lo_mask);
	}
	watchdog();
	hi &= hi_mask;
	lo &= lo_mask;
    }

    return hi | lo;
}

