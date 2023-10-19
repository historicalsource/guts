
/*
 *	smc_putstr.c -- Forrest Miller -- June 1996
 *
 *	Text string output function needed by SMC driver.
 *
 *
 *		Copyright 1996 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <st_proto.h>

#define	STATIC	static


extern void (*_smc_putstr_vec)(const char *);

void smc_putstr(const char * str)
{
 if (_smc_putstr_vec) (*_smc_putstr_vec)(str);
}
