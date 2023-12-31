#if !defined(_IIO_PROTO_H_)
#define _IIO_PROTO_H_

# include <icelesspkt.h>

# define ICELESS_MAX_BUFSIZ (1200)

# if !defined(ICELESS_THREADS)
#   define ICELESS_THREADS	1 /* set to the maximum number of threads (connections) allowed at the same time */
# endif
# if !defined(ICELESS_USE_FUNC)
#   define ICELESS_USE_FUNC	0  /* if not zero, assign a completion routine to handle input */
# endif
# if !defined(ICELESS_DOUBLE_BUFFER)
#   define ICELESS_DOUBLE_BUFFER 0 /* if not zero, double buffer the input */
# endif

# if !defined(ICELESS_BUFFER_SIZE)
#  define ICELESS_BUFFER_SIZE ICELESS_MAX_BUFSIZ
# endif

# if !defined(ICELESS_IBUFFER_SIZE)
#  define ICELESS_IBUFFER_SIZE 8 /* default to buffers big enough only to accept probe packets */
# endif

# if ICELESS_BUFFER_SIZE > ICELESS_MAX_BUFSIZ
#  undef ICELESS_BUFFER_SIZE
#  define ICELESS_BUFFER_SIZE ICELESS_MAX_BUFSIZE
# endif

typedef struct icelessio {
   PktIOStruct pkt;		/* packet output stuff */
   PktIOStruct ipkt0;		/* packet input stuff */
   PktIOStruct ipkt1;		/* (double buffered input) */
   void (*ifunc)(PktIOStruct *, void *param); /* pointer to function to call when packet received */
   void *user;			/* reserved for use by application programs (also passed to ifunc) */
   int sequence;		/* packet sequence number to use for output */
   int oindx;			/* place to deposit next byte in output buffer */
   int pktcnt;			/* total packet count sent */
} IcelessIO;

/******************************************************************************************
 * iio_open() - Initialise a network connection and return pointers to the packet
 * I/O structures required to use the iceless I/O system. This function must be called
 * _once_ for each different thread number wanted by the programmer.
 *
 * At entry:
 *	thread - the integer thread number (1-254) with which to outgoing
 *		packets. Thread numbers 0 and 255 are reserved for use by gdb.
 * At exit:
 *	Structures have been initialised, although the network connection will not complete
 *	until the first packet arrives over the network with a destination thread number that
 *	matches the one specified. For this reason, output written will be discarded until
 *	the first packet arives from the network.
 * Returns:
 *	Pointer to IcelessIO to use in all subsequent I/O operations wrt this thread or 0 if
 *	thread number already in use or all IcelessIO pointers have already been issued.
 */

    extern IcelessIO *iio_open(int thread);

/******************************************************************************************
 * iio_write() - writes a buffer to the output buffer.
 *
 * At entry:
 *	iop - points to the IcelessIO struct which contains the output buffer
 *	s   - pointer to buffer
 *	len - number of bytes to write 
 * At exit:
 *	String copied to output buffer. Buffer may have been flushed (as many times
 *	as it took) if it became full as a result of the copy.
 * Returns:
 *	number of characters written
 */

    extern int iio_write(IcelessIO *iop, const char *buf, int len);

/******************************************************************************************
 * iio_iprintf() - writes a printf string to the network. See the Unix man pages under printf
 * for syntax of the format string. This function will perform as many iio_writes as required
 * to emit all the expanded text. A final iio_flush() is performed after all the text has been
 * converted (i.e., this function does automatic flush after each write). This function
 * includes only integer type conversions. Use iio_printf() _instead_ if you want floating
 * point conversions too.
 *
 * At entry:
 *	iop - points to the IcelessIO struct which contains the output buffer
 *	fmt - pointer to format specifier
 *	... - 0 or more arguments as described in the format string
 * At exit:
 *	Expanded string copied to network. Buffer will have been flushed. Many iio_writes()
 *	may have been required to dump the entire string.
 * Returns:
 *	total number of characters written
 */

    extern int iio_iprintf(IcelessIO *iop, const char *fmt, ...);

/******************************************************************************************
 * iio_printf() - writes a printf string to the network. See the Unix man pages under printf
 * for syntax of the format string. This function will perform as many iio_writes as required
 * to emit all the expanded text. A final iio_flush() is performed after all the text has been
 * converted (i.e., this function does automatic flush after each write). This function
 * includes integer and floating point data type conversions and will, as a result, cause the
 * entire floating point library to be included in your executable (quite large). Use
 * iio_iprintf() _instead_ if you have no need for floating point data conversions and you
 * want to save space in your code.
 *
 * At entry:
 *	iop - points to the IcelessIO struct which contains the output buffer
 *	fmt - pointer to format specifier
 *	... - 0 or more arguments as described in the format string
 * At exit:
 *	Expanded string copied to network. Buffer will have been flushed. Many iio_writes()
 *	may have been required to dump the entire string.
 * Returns:
 *	total number of characters written.
 */

    extern int iio_printf(IcelessIO *iop, const char *fmt, ...);

/******************************************************************************************
 * iceless_putc() - put a character into output buffer (does not automatically
 * write it to the network).
 *
 * At entry:
 *	iop - pointer to the IcelessIO struct which contains the output buffer
 *	c  - character to write
 * At exit:
 *	Character saved in output buffer. Buffer may have been flushed if it became full
 *	as a result of the save.
 * Returns:
 *	1, the number of characters written
 */

    extern int iio_putc(IcelessIO *iop, char c);

/******************************************************************************************
 * iio_flush() - flushes any output saved up in the output buffer and forces output over
 * the network.
 *
 * At entry:
 *	iop - points to the IcelessIO struct which contains the output
 * At exit:
 *	output flushed to network via pktQueSend.
 * Returns:
 *	nothing
 */

    extern void iio_flush(IcelessIO *iop);

/******************************************************************************************
 * iceless_puts() - puts a null terminated string (but not the null) in the output buffer.
 *
 * At entry:
 *	iop - points to the IcelessIO struct which contains the output buffer
 *	s   - pointer to null terminated string
 * At exit:
 *	String copied to output buffer. Buffer may have been flushed (as many times
 *	as it took) if it became full as a result of the copy.
 * Returns:
 *	number of characters written
 */

    extern int iio_puts(IcelessIO *iop, char *str);

#endif
