/* Establish Date and Time of last OS build */
#ifndef __STDC__
#define __STDC__ (0)
#endif

#define revstr(str) #str
#define reveval(rev) revstr(rev)
#if __STDC__
const char GUTS_LINK_TIME[] =
#ifdef MAJOR_REV
reveval(MAJOR_REV)
#ifdef MINOR_REV
"."
reveval(MINOR_REV)
#endif
" "
#endif
 __DATE__ " " __TIME__;
#else
unsigned const char * const GUTS_LINK_TIME[2] = 
{
	(const unsigned char * const)__DATE__,
	(const unsigned char * const)__TIME__
};
#endif
