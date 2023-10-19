
/*
 *	ioa_uart.h -- Forrest Miller -- July 1996
 *
 *	Definitions for I/O ASIC uart on Phoenix/Flagstaff.
 *
 *
 *		Copyright 1996 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */


/*
 * Interrupt Controller Definitions
 */
#if defined(IO_MAIN_STS_T)
#define IO_STATUS  ( IO_MAIN_STS_T )
#define IO_CONTROL ( IO_MAIN_CTL_T )
#else
#define IO_STATUS  ( *((VU16 *)IO_MAIN_STS) )
#define IO_CONTROL ( *((VU16 *)IO_MAIN_CTL) )
#endif

#define UART_BRK ( IO_MAIN_UART_BRK_DETECT )
#define UART_ERR ( IO_MAIN_UART_FRAME_ERROR | IO_MAIN_UART_OVER_RUN )
#define UART_RCV ( IO_MAIN_UART_RCV_CHAR )
#define UART_XMT ( IO_MAIN_UART_XMT_EMPTY )

#define UART_INT ( IO_MAIN_UART_BRK_DETECT | IO_MAIN_UART_FRAME_ERROR \
		   | IO_MAIN_UART_OVER_RUN | IO_MAIN_UART_RCV_FULL    \
		   | IO_MAIN_UART_RCV_CHAR | IO_MAIN_UART_XMT_EMPTY )

/*
 * UART Definitions
 */
#define UART_ENABLE   ( IO_UART_CTL_INTERNAL_ENA )

#if defined(IO_UART_CTL_T)
#define UART_CONTROL  ( IO_UART_CTL_T )
#define UART_XMITTER  ( IO_UART_TX_T )
#define UART_RECEIVER ( IO_UART_RCV_T )
#else
#define UART_CONTROL  ( *((VU16 *)IO_UART_CTL) )
#define UART_XMITTER  ( *((VU16 *)IO_UART_TX) )
#define UART_RECEIVER ( *((VU16 *)IO_UART_RCV) )
#endif

#define CHAR_RCV ( IO_UART_RCV_CHAR )
#define BRK      ( IO_UART_RCV_BREAK_DETECT )
#define FRAME    ( IO_UART_RCV_FRAME_ERROR )
#define OVERRUN  ( IO_UART_RCV_OVER_RUN )
#define RCV_FULL ( IO_UART_RCV_FULL )

#define FIFO_SIZE ( IO_UART_RCV_FIFO_SIZE )

#define BAUD_MASK ( 0x7f )
