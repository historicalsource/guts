/*			cdec_hex.c
 *
 *		Copyright 1991,1992,1993,1994 Atari Games.
 *	Unauthorized reproduction, adaptation, distribution, performance or
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
*/
#ifdef FILE_ID_NAME
const char FILE_ID_NAME[] = "$Id: cdec_hex.c,v 1.4 1997/07/01 22:23:15 albaugh Exp $";
#endif
/*
 *		Binary to string conversion for Decimal and Hexadecimal
 *	The routine __ultodec() is included here, but may be replaced by
 *	an assembly-language routine on machines where this algorithm
 *	is too slow. Don't get your hopes up, though, as this is a
 *	fairly fast algorithm, so, barring a really stupid compiler,
 *	you may not gain much.
 */

#ifdef STAND_ALONE
#include <stdio.h>
#include <stdlib.h>
#endif
#include <config.h>
#include <string.h>
#define GREAT_RENAME (1)
#include <os_proto.h>

/*		utl_chex()
* This routine will convert the number in num to a hexadecimal string. It
* will zero fill to the size in length. It the number is larger then
* length it will display the low order nibbles.
*  The format is == 0 for zero fill, !=0 for blank fill
*/
#ifdef ASM_ULTODEC
extern int __ultodec();
#else

/*
* ultodec: convert a long to a string in radix 10.
*
* CALL:
* __ultodec(num,buffer)
*	unsigned long num; char *buffer;
*
* The routine will convert the number in num to a decimal string. The
* buffer is assumed to be large enough to contain the largest number
* (10 digits + terminating 0)
*/

static const unsigned long powtab[] = {
    4000000000U,
    800000000,
    80000000,
    8000000,
    800000,
    80000,
    8000,
    800,
    80,
    0
};
#define TBUF_LEN (11)

int __ultodec(num, buffer)
unsigned long num;
char *buffer;
{
    char tbuf[TBUF_LEN],*bp,*msp,*lsp;
    unsigned long acc,pow;
    const unsigned long *powpt=powtab;
    int dig,bit,len;
    acc = num;

    bp = tbuf;
    pow = *powpt++;
    dig = 0;
    /* special hack for largest power */
    bit = 4;

    while ( bit ) {
	if ( acc >= pow ) {
	    dig |= bit;
	    acc -= pow;
	}
	pow >>= 1;
	if ( (bit >>= 1) == 0 ) {
	    *bp++ = dig;		/* stash this digit */
	    dig = 0;
	    /* if end of power table, leave */
	    if ( (pow = *powpt++) == 0 ) break;
	    bit = 8;
	}
    }
    /* store last "proto-digit" and scan for most sig dig */
    *bp = acc;
    lsp = bp;		/* Least Sig. Pointer */
    msp = bp;		/* Most sig pointer (default one's place) */
    /* scan left from tens place looking for more significant digit */
    while ( --bp >= tbuf ) if (*bp) msp = bp;
    len = TBUF_LEN-1 - (msp-tbuf);
    while ( msp <= lsp ) *buffer++ = (*msp++) + '0';
    *buffer++ = '\0';
    return len;
}
#endif
static const char hexdig[] = "0123456789ABCDEF";
int
utl_chex(num, buffer, length, format)
unsigned long num; char *buffer; int length, format;
{
    m_int idx = length;
    buffer[idx] = '\0';
    format = format ? ' ' : '0';
    do {
	buffer[--idx] = hexdig[num & 0xF];
	num >>= 4;
    } while ( (idx > 0) && (num != 0));
    length = length - idx;
    while ( idx > 0 ) buffer[--idx] = format;
    return length;
}

/*	 utl_cdec(num,buffer,length,format)
*
* This routine will convert the number in num to a decimal string. It
* will zero fill to the size in length. If the number is larger then
* length it will display the low order digits.
*  The format is:
*		0 = right just, zero fill
*		1 = right just, blank fill
*		2 = left just, blank fill
*		3 = left just, no fill (string is exact length needed)
*/

#define MAXLEN (11)			/* 32 bits gives 10 digits + '\0' */

int
utl_cdec(num,buffer,length,format)
unsigned long num;
char *buffer;
int length, format;
{
    char tmp[MAXLEN+1];
    int slop,digits;

    digits =__ultodec(num,tmp);		/* convert number, return # of digits */
    slop = length - digits;		/* How much space do we need to fill ?*/
    if ( slop <= 0 ) {
	/* Number won't actually fit. We mimic the old behavior by returning
	 * LSDs. This is also used when number exactly fits
	*/
	memcpy(buffer,tmp-slop,length);		/* -slop is +digits-length */
    } else switch (format) {
	case RJ_ZF:
	    memcpy(buffer+slop,tmp,digits);
	    memset(buffer,'0',slop);
	    break;
	case RJ_BF:
	    memcpy(buffer+slop,tmp,digits);
	    memset(buffer,' ',slop);
	    break;
	case LJ_BF:
	    memcpy(buffer,tmp,digits);
	    memset(buffer+digits,' ',slop);
	    break;
	case LJ_NF:
	    memcpy(buffer,tmp,digits);
	    length = digits;
    }
    buffer[length] = '\0';
    return digits;
}

#ifdef STAND_ALONE
int main(argc,argv)
int argc;
char **argv;
{
    char *lp,*rp;
    unsigned long acc;
    char numbuf[16];
    int len;

    while ( --argc ) {
	lp = *++argv;
	acc = strtoul(lp,&rp,0);
	if ( lp == rp ) continue;
	len = __ultodec(acc,numbuf);
	fprintf(stdout,"%d) %s\n",len,numbuf);
    }
    return 0;
}
#endif
