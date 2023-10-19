#ifndef _CLED_PROTO_H_
# define _CLED_PROTO_H_

# include <cled_defines.h>
# include <cled_io.h>
# include <iio_proto.h>

/******************************************************************************************
 * cled_init() - establishes memory pool, buffer and other limits for the cled
 * interface s/w to use.
 *
 * At entry:
 *	ip - pointer to CledHeader struct containing the parameters the cled
 *		i/f needs to continue. If this parameter is 0, it only causes
 *		the function to return the current value.
 * At exit:
 * Returns:
 *	pointer to previous value.
 */

/******************************************************************************************
 * cled_flush() - flushes any output saved up in the output buffer and forces output over
 * the network.
 *
 * At entry:
 *	lb - points to the LedBuf struct which (indirectly) contains the output
 * At exit:
 *	output flushed to network via pktQueSend.
 * Returns:
 *	nothing
 */

  extern void cled_flush(LedBuf *lb);

/******************************************************************************************
 * cled_putc() - put a character into output buffer. This is a support routine for and is used
 * heavily by cled, but anyone else is free to use it too.
 *
 * At entry:
 *	c  - character to write
 *	lb - pointer to the LedBuf struct which (indirectly) contains the output
 * At exit:
 *	Character saved in output buffer. Buffer may have been flushed if it became full
 *	as a result of the save.
 * Returns:
 *	nothing
 */

  extern void cled_putc(char c, LedBuf *lb);	/* put a single character to output */

/******************************************************************************************
 * cled_puts() - puts a null terminated string (but not the null) in the output buffer. This is
 * a support routine for and is used heavily by cled, but anyone else is free to use it too.
 *
 * At entry:
 *	s  - pointer to null terminated string
 *	lb - points to the LedBuf struct which (indirectly) contains the output
 * At exit:
 *	String copied to output buffer. Buffer may have been flushed (as many times
 *	as it took) if it became full as a result of the copy.
 * Returns:
 *	nothing
 */

  extern void cled_puts(char *s, LedBuf *lb);	/* send a null terminated string to output */

/******************************************************************************************
 * cled_readline() - Read a whole line from the network. This function is intended to be the
 * main interface between cled and the user's program. It returns 0 if no line is ready to
 * receive and 1 if a whole line has been received. This function is a sort of "Poll for
 * input" except it will hand over a complete edited line if one is preset. The calling of
 * this function frequently is _required_ to get command line editing to work properly.
 *
 * At entry:
 *	lb - points to the LedBuf struct which will contain the input
 * At exit:
 *	Some input may have been sampled and read from the network (one or more pktPoll()'s
 *	will have been executed). If an EOL or EOF character is among the received data,
 *	this function will return true (1) indicating same. lb->buf points to the edited
 *	string. If no EOL or EOF character has been received, the function returns 0. 
 *	NOTE: EOF is indicated by ((lb->flags&LD_EOF) != 0).
 * Returns:
 *	TRUE (1) if the line pointed to by lb->buf is complete else FALSE (0).
 */

  extern int cled_readline(LedBuf *lb);		/* read a line from stdin */

/******************************************************************************************
 * cled_open() - Initialise a network connection and return pointers to the cled and packet
 * I/O structures required to use the cled system. This function must be called _once_ for
 * each different thread number wanted by the programmer to get a pointer to the LedBuf.
 * A pointer to a LedBuf is required by all the other cled I/O functions. This is equivalent
 * to a file open. It is not necessary to close (pressing your system's reset button will
 * do that for you).
 *
 * At entry:
 *	thread - the integer thread number (1-254) with which to identify incoming and outgoing
 *		packets. Thread numbers 0 and 255 are reserved for use by gdb.
 * At exit:
 *	Structures have been initialised, although the network connection will not complete
 *	until the first packet arrives over the network with a destination thread number that
 *	matches the one specified. For this reason, output written will be discarded until
 *	the first packet arives from the network.
 * Returns:
 *	Pointer to LedBuf to use in all subsequent I/O operations wrt this thread or 0 if
 *	thread number already in use or all LedBuf pointers have already been issued.
 */

  extern LedBuf *cled_open(int thread);

#endif
