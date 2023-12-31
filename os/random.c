/*	This file contains two implementations of the "Minimal Standard"
*	random number generator discussed in CACM vol 31 #10 (October 1988)
*	in the article by Stephen K. Park and Keith W. Miller:
*	"Random Number Generators: good ones are hard to find"
*
*	It is a linear congruential generator of the form:
*		seed = (seed*a)%m
*	where (in this case) a = 16807 (= 7^5) and m=(2^31-1)
*
*	In this particular file, we have jiggered the handling of "seed"
*	so that the Ansi requirements of "behaving as if the target
*	environment calls srand(1) at startup" can be handled by the
*	initialization of seed to 0. This also has the (benificial?) effect
*	of ensuring that 0 is included in the range of outputs, although
*	a seed value of 0 would cause lock-up. The down-side is that RAND_MAX
*	becomes 2^31-3. This is because:
*		m = 2^31-1, hence:
*		seed < 2^31-1 	( or seed <= 2^31-2) hence:
*		(seed-1) < 2^31-2 ( or (seed-1) <= 2^31-3 )
*
*/
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: random.c,v 1.3 1997/07/01 22:40:48 albaugh Exp $";
#endif
#if defined(STAND_ALONE) 
#include <stdio.h>
static unsigned long ref_seed;
#endif

static unsigned long seed_less_one;

#ifdef ORIG_PARAMS
#define A (16807)		/* 7**5 */
#define RAND_10K (1043618065)	/* value of seed at ten-thousandth iteration */
#else
#define A (48271)
#define RAND_10K (399268537)	/* value of seed at ten-thousandth iteration */
#endif
#define M (2147483647)		/* 2**31-1 */

/*	Andrew Hume (andrew@alice.uucp) of AT&T Bell Laboratories recommends
 * 	A = 48271 as a "better number", which yields Q= 44488, R=3399 below
 */

#ifdef STAND_ALONE

/*	This is the implementation described in the article.
*	It includes a clever method for avoiding overflow but requires at
*	least one (possibly two, for a dumb compiler) divides.
*		This is a transliteration from the Pascal source, and runs
*	correctly on a 80386 machine when compiled with GCC 1.37
*/
#define Q (M/A)			/* should end up 127773 */
#define R (M%A)			/* should end up 2836 */

int ref_rand()
{

    unsigned long hi,lo;
    long test;

    hi = ref_seed / Q;
    lo = ref_seed % Q;

    test = (A * lo) - (R * hi);
    if ( test > 0 ) ref_seed = test;
    else ref_seed = test + M;
    return ref_seed;
}
#endif

void
srand (int new_seed)
{
    seed_less_one = new_seed-1;
}

int
rand()
{
    unsigned long hprod,lprod;
    unsigned long seed = seed_less_one + 1;

    /* first do two 16x16 multiplies */
    hprod = ((seed>>16)&0xFFFF) * A;
    lprod = (seed&0xFFFF)*A;
#ifdef STAND_ALONE
#if DEBUG > 1
    fprintf(stderr,"seed = %X\thprod = %X\tlprod = %X\n",seed,hprod,lprod);
#endif
#endif
    /* combine the products (suitably shifted) to form 48-bit product,
    *  with bottom 32 bits in lprod and top 16 bits in hprod.
    */
    hprod += ((lprod>>16) & 0xFFFF);
    lprod = (lprod&0xFFFF)|((hprod&0xFFFF)<<16);
    hprod >>= 16;
#ifdef STAND_ALONE
#if DEBUG > 1
    fprintf(stderr,"sum to\thprod = %X\tlprod = %X\n",hprod,lprod);
#endif
#endif
    /* now subtract the top 17 bits from the bottom 31 bits to implement
    *  a deferred "end-around carry".
    */
    hprod = hprod + hprod + ((lprod>>31)&1);
    lprod += hprod;
#ifdef STAND_ALONE
#if DEBUG > 1
    fprintf(stderr,"e-o-c\thprod = %X\tlprod = %X\n",hprod,lprod);
#endif
#endif
    /* final "and" gives modulo(2^31-1) */
    seed = lprod & 0x7FFFFFFF;
    seed_less_one = seed - 1;
    return seed;
}

#ifdef STAND_ALONE
#if DEBUG > 1
#define LOOPLIM (6)
#else
#define LOOPLIM (10002)
#endif


int main()
{
    int i;
#ifdef DEBUG
    ref_seed = seed_less_one + 1;
#endif
    for ( i = 0 ; i < LOOPLIM ; ++i ) {
	if ( (i > 9995) || (i < 10)) {
#ifdef DEBUG
	    printf("ref_seed[%d] = %lu\t",i,ref_seed);
#endif
	    printf("seed[%d] = %lu\n",i,seed_less_one+1);
	}
    if ( i == 10000 ) {
	if ( (seed_less_one+1) != RAND_10K ) {
	    printf("seed = %lu, should be %u\n",
	      seed_less_one+1,RAND_10K);
	}
	else printf("implementation ok\n");
    }
#ifdef DEBUG
	ref_rand();
#endif
	rand();
    }
    return 0;
}
#endif

