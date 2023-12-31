
/*
 * $Id: picfuncs.c,v 1.4 1997/09/13 02:29:54 shepperd Exp $
 *
 *	Forrest Miller -- September 1996
 *
 *	This file contains several routines to do things with the WMS PIC.
 *	Rewritten for GUTS from the WMS (cbk) file of the same name.
 *
 *
 *		Copyright 1996,1997 Atari Games Corporation
 *	Unauthorized reproduction, adaptation, distribution, performance or 
 *	display of this computer program or the associated audiovisual work
 *	is strictly prohibited.
 */

#include <config.h>
#define GREAT_RENAME (1)
#include <os_proto.h>
#include <wms_proto.h>
#include <pic_defs.h>
#include <nsprintf.h>

#define STATIC static

#ifdef IO_H2MIC_CMD_T
# define HOST_TO_PIC IO_H2MIC_CMD_T
#else
# define HOST_TO_PIC *(U32 *)(IO_H2MIC_CMD)
#endif
#ifdef IO_MIC2H_DTA_T
# define PIC_TO_HOST IO_MIC2H_DTA_T
#else
# define PIC_TO_HOST *(U32 *)(IO_MIC2H_DTA)
#endif

#define PIC_ACK 0x0100
#define PIC_REQ 0x0010

#ifndef PIC_XOR_CMD
# define PIC_XOR_CMD 0
#endif

/*
 * Define the commands that the PIC understands
 */

#define PIC_NOP         0x000
#define PIC_SERIAL      0x001		/* Returns the Serial Number        */
#define PIC_TIME_PREP   0x002		/* Prepares for Date/Time Read      */
#define PIC_TIME_READ   0x003		/* Does the actual Date/Time Read   */
#define PIC_TIME_SET    0x004		/* Sets the Date/Time               */
#define PIC_NVRAM_WRITE 0x005		/* Writes to RTC-NVRAM              */
#define PIC_NVRAM_READ  0x006		/* Reads RTC-NVRAM location         */

/*
 * Some PIC Error returns
 */

#define ERROR_TIMEOUT   0x81            /* Timeout after writing command    */
#define ERROR_BOGUS_CMD 0x82            /* Incorrect echoed command         */

/*
 * Name:        WritePICCommand
 * Synopsis:    int WritePICCommand (U8 Command) ;
 * Description: This low level routine will write a command to the PIC
 *              from the Host.  Included is all of the handshaking.
 *              It also reads back the command from the PIC.
 * Inputs:      Command contains the OP Nibble to the PIC.
 * Outputs:     If everything went OK, then 0 is returned.
 *              If the PIC doesn't assert its ACK for a while, then 1 is
 *              returned.
 *              If the echoed command isn't the same as the one sent, then
 *              a 2 will be returned.
 * Notes:
 */

STATIC int WritePICCommand (U8 Command)
{
    U32 TimeCount ;
    U16 TempCommand ;
    U16 PICReturn ;

    TempCommand = Command & 0x00f ;             /* Just mask any crap */
    HOST_TO_PIC = (TempCommand | PIC_REQ) ^ PIC_XOR_CMD;    /* Assert Request to PIC */
    TimeCount = 0 ;
    while (1) {
        PICReturn = PIC_TO_HOST ;
        if (PICReturn & PIC_ACK)
            break ;
        prc_delay (0) ;
        if (++TimeCount > 9) return (ERROR_TIMEOUT) ;
    }

    HOST_TO_PIC = 0x000 ^ PIC_XOR_CMD;

    if ((TempCommand | 0x80) != (PICReturn & 0x0ff))
        return (ERROR_BOGUS_CMD) ;

/*
 * Finally, wait for PIC to clear the PIC_ACK line.
 */

    TimeCount = 0 ;
    while (1) {
        PICReturn = PIC_TO_HOST ;
        if (!(PICReturn & PIC_ACK))
            break ;
        prc_delay (0) ;
        if (++TimeCount > 9) return (ERROR_TIMEOUT) ;
    }
    return (SUCCESS) ;
}

/*
 * Name:        ReadPICData
 * Synopsis:    int ReadPICData (U8 *Data) ;
 * Description: This routine will read returned data from the PIC.  It
 *              implements all of the handshaking and such.
 * Inputs:      *Data is a pointer to where to stash the data after it is
 *              read.
 * Outputs:     If everything went OK, then 0, else 1 indicating a timeout
 *              error.
 * Notes:
 */

STATIC int ReadPICData (U8 *Data)
{
    U16 TimeCount ;
    U16 PICReturn ;

    HOST_TO_PIC = PIC_REQ ^ PIC_XOR_CMD;

    TimeCount = 0 ;
    while (1) {
        PICReturn = PIC_TO_HOST ;
        if (PICReturn & PIC_ACK)
            break ;
        prc_delay (0) ;
        if (++TimeCount > 9) return (ERROR_TIMEOUT) ;
    }

    *Data = (PICReturn & 0xff) ;
    HOST_TO_PIC = 0 ^ PIC_XOR_CMD ;

    TimeCount = 0 ;
    while (1) {
        PICReturn = PIC_TO_HOST ;
        if (!(PICReturn & PIC_ACK))
            break ;
        prc_delay (0) ;
        if (++TimeCount > 9) return (ERROR_TIMEOUT) ;
    }
    return (SUCCESS) ;

}

/*
 * Name:        WriteTwoNibbles
 * Synopsis:    int WriteTwoNibbles (U8 c) ;
 * Description: This routine writes the given byte as two nibbles to the PIC.
 *              The data goes out least then most significant nibble.
 * Inputs:      c is the byte to write.
 * Outputs:     Standard return codes.
 * Notes:
 */

STATIC int WriteTwoNibbles (U8 c)
{
    if (WritePICCommand ((U8) (c & 0x0f)) != SUCCESS) return (FAILURE) ;
    return (WritePICCommand ((U8) ((c>>4)&0x0f))) ;
}

/*
 * Name:        GetPICSerial
 * Synopsis:    int GetPICSerial (U8 *SerialNumberBuffer) ;
 * Description: Reads the internal serial number of the PIC, and returns
 *              it in a buffer.
 * Inputs:      Pointer to the buffer.
 * Outputs:     Usual return codes.
 * Notes:
 */

STATIC int GetPICSerial (U8 *SerialNumberBuffer)
{
    U8 ByteCount, TempByte ;

    if (WritePICCommand (PIC_SERIAL) != SUCCESS)
        return (FAILURE) ;

    for (ByteCount = 0; ByteCount < SERIAL_NUMBER_SIZE; ByteCount++) {
        if (ReadPICData (&TempByte) != SUCCESS)
            return (FAILURE) ;
        SerialNumberBuffer [ByteCount] = TempByte ;
    }

    return (SUCCESS) ;
}

/*
 * Name:        SetPICDateTime
 * Synopsis:    int SetPICDateTime ((U8 *) DateTimeBuffer) ;
 * Description: This routine is called in order to set the time from
 *              the inputted buffer.
 * Inputs:      DateTimeBuffer is a buffer containing the data to use
 *              to set the clock.
 * Outputs:     The standard return codes.
 * Notes:
 */

int SetPICDateTime (U8 *DateTimeBuffer)
{
 int Response ;

    if ((Response = WritePICCommand (PIC_TIME_SET)) != SUCCESS)
        return (Response) ;

    WriteTwoNibbles (DateTimeBuffer [SECONDS]) ;
    WriteTwoNibbles (DateTimeBuffer [MINUTES]) ;
    WriteTwoNibbles (DateTimeBuffer [HOURS]) ;
    WriteTwoNibbles (DateTimeBuffer [DAY_OF_WEEK]) ;
    WriteTwoNibbles (DateTimeBuffer [DAY_OF_MONTH]) ;
    WriteTwoNibbles (DateTimeBuffer [MONTH]) ;
    WriteTwoNibbles (DateTimeBuffer [YEAR]) ;
    return (SUCCESS) ;
}

/*
 * Name:        GetPICDateTime
 * Synopsis:    int GetPICDateTime (U8 *DateTimeBuffer) ;
 * Description: This routine is called in order to retrieve the Real Time
 *              Clock Registers.  It initially tells the PIC to get the
 *              registers with the 0x02 command, and then gets the actual
 *              registers with the 0x03 command.
 * Inputs:      Pointer to DateTimeBuffer
 * Outputs:     Fills in the 7 bytes of data.  If everything goes OK, then
 *              0 is returned, else an error from a lower level.
 * Notes:
 */

int GetPICDateTime (U8 *DateTimeBuffer)
{
 static const bmask[] = { 0x7f, 0x7f, 0x3f, 0x07, 0x3f, 0x3f, 0xff };
    U16 ByteCount ;

    if (WritePICCommand (PIC_TIME_PREP) != SUCCESS)
        return (FAILURE) ;

    if (WritePICCommand (PIC_TIME_READ) != SUCCESS)
        return (FAILURE) ;

    for (ByteCount = 0; ByteCount < 7; ByteCount++) {

        if (ReadPICData (&DateTimeBuffer [ByteCount]) != SUCCESS)
            return (FAILURE) ;
        DateTimeBuffer [ ByteCount ] &= bmask[ ByteCount ];
    }
    return (SUCCESS) ;
}

/*
 * Name:        ReadPICNVRAM
 * Synopsis:    int ReadPICNVRAM (U8 Address, U8 *PICData) ;
 * Description: This function will send the commands to the PIC for it to
 *              return NVRAM contents at a given location.
 * Inputs:      Address is the NVRAM address within the RTC chip.  There are
 *              64 bytes (0 - 63), of which the first 8 are used by the RTC.
 *              PICData points to a place to put the data.
 * Outputs:     If all OK, 0, else an error message will have printed, and
 *              a non-zero return will be returned. Returned value is byte
 *              at given address.
 * Notes:
 */

int ReadPICNVRAM (U8 Address, U8 *PICData)
{
    if ((WritePICCommand (PIC_NVRAM_READ) != SUCCESS) ||
        (WritePICCommand (Address & 0x0f) != SUCCESS) ||
        (WritePICCommand ((Address & 0xf0)>>4) != SUCCESS) ||
        (ReadPICData (PICData) != SUCCESS))
        return (FAILURE) ;
    else
        return (SUCCESS) ;
}

/*
 * Name:        WritePICNVRAM
 * Synopsis:    int WritePICNVRAM (U8 Address, U8 Data) ;
 * Description: This routine is called in order to write a given NVRAM
 *              byte within the RTC address space to a given value.
 *              The address is between 0 and 63.  The first 8 are used by
 *              the RTC, so the user may play around with 8-63.
 * Inputs:      An address and a value !
 * Outputs:     Same return codes as usual.
 * Notes:
 */

int WritePICNVRAM (U8 Address, U8 Data)
{
    if ((WritePICCommand (PIC_NVRAM_WRITE) != SUCCESS) ||
            (WritePICCommand (Address & 0x0f) != SUCCESS) ||
            (WritePICCommand ((Address & 0xf0)>>4) != SUCCESS) ||
            (WritePICCommand (Data & 0x0f) != SUCCESS) ||
            (WritePICCommand ((Data & 0xf0)>>4) != SUCCESS))
        return (FAILURE) ;
    else
        return (SUCCESS) ;
}

/*
 * From pic.c received 9/20/96 from Cary Mednick at Williams.
 * To quote the sender:  "Please keep this in trusted hands only."
 * Modified for GUTS by Forrest Miller.
 */

STATIC char serial_str[16];
STATIC char type_str[16];
STATIC char date_str[16];

STATIC char *wms_ptrs[3];

/***************************************************************************/
/*                                                                         */
/* FUNCTION: decode_serial_number                                          */
/*                                                                         */
/* Decodes the 16-byte serial number returned from the PIC into the 3      */
/* digit game type, the 6 digit game serial number, and the day/month/year */
/* that the PIC was programmed.                                            */
/*                                                                         */
/* The nomenclature (C)(D)(E) and (X) and so forth for the PIC serial      */
/* number bytes and [0] and [1] and so on for the decoded digits is        */
/* described in the documents serial1.doc, serial2.doc and serial3.doc.    */
/* These describe the encode process, and are from pinball.                */
/*                                                                         */
/* The decode process is described in "Decoding the V+ Video PIC Serial    */
/* Number".                                                                */
/*                                                                         */
/* Copyright (c) 1995 Midway Manufacturing Company                         */
/* by Matt Booty                                                           */
/*                                                                         */
/***************************************************************************/

STATIC void decode_serial_number (unsigned char *serial_number)
{ /* decode_serial_number() */

 unsigned int decoded_digits [SERIAL_NUMBER_SIZE];
 unsigned int coded_hex;

 int year;
 int month;
 int day;

 char temp_str[6];

/***************************************************************************/

   /* step 1 */
   /* (C)(D)(E) --> [B][3][5][9] */

	/* extract (C)(D)(E) */
   coded_hex = (serial_number [2] << 16) +
               (serial_number [1] << 8) +
               (serial_number [0]);

	/* apply the magic numbers */

   coded_hex = coded_hex - 15732;

   coded_hex = coded_hex / 581;

   coded_hex = coded_hex - serial_number [12]; /* (X) */


	/* easy hex to BCD conversion */

	nsprintf (temp_str, sizeof(temp_str), "%04d", coded_hex);

	decoded_digits [0xB] = (temp_str[0] - '0');
	decoded_digits [0x3] = (temp_str[1] - '0');
	decoded_digits [0x5] = (temp_str[2] - '0');
	decoded_digits [0x9] = (temp_str[3] - '0');


   /* step 2 */
   /* (F)(G)(H)(I) --> [2][A][0][8][6] */

	/* extract (F)(G)(H)(I) */
   coded_hex = (serial_number [6] << 24) +
               (serial_number [5] << 16) +
               (serial_number [4] << 8) +
               (serial_number [3]);

	/* apply the magic numbers */

   coded_hex = coded_hex - 7463513;

   coded_hex = coded_hex / 4223;

   coded_hex = coded_hex - serial_number [13]; /* (Y) */

   coded_hex = coded_hex - serial_number [13]; /* (Y) */

   coded_hex = coded_hex - serial_number [12]; /* (X) */


	/* easy hex to BCD conversion */

	nsprintf (temp_str, sizeof(temp_str), "%05d", coded_hex);

	decoded_digits [0x2] = (temp_str[0] - '0');
	decoded_digits [0xA] = (temp_str[1] - '0');
	decoded_digits [0x0] = (temp_str[2] - '0');
	decoded_digits [0x8] = (temp_str[3] - '0');
	decoded_digits [0x6] = (temp_str[4] - '0');


   /* step 3 */
   /* (J)(K)(L) --> [1][7][4] */

	/* extract (J)(K)(L) */
   coded_hex = (serial_number [9] << 16) +
               (serial_number [8] << 8) +
               (serial_number [7]);

	/* apply the magic numbers */

   coded_hex = coded_hex - 127984;

   coded_hex = coded_hex / 7117;

   coded_hex = coded_hex - (5 * serial_number [13]); /* 5 x (Y) */


	/* easy hex to BCD conversion */

	nsprintf (temp_str, sizeof(temp_str), "%03d", coded_hex);

	decoded_digits [0x1] = (temp_str[0] - '0');
	decoded_digits [0x7] = (temp_str[1] - '0');
	decoded_digits [0x4] = (temp_str[2] - '0');


   /***** assemble decoded digits into actual strings *****/

   
	nsprintf (type_str, sizeof(type_str), "%d%d%d", decoded_digits[0], decoded_digits[1],
           decoded_digits[2]);

 	nsprintf (serial_str, sizeof(serial_str), "%d%d%d%d%d%d", decoded_digits[3], decoded_digits[4],
           decoded_digits[5], decoded_digits[6], decoded_digits[7],
           decoded_digits[8]);


   /* decode the date code */


	/* extract (Q)(R) */
   coded_hex = (serial_number [10] << 8) +
               (serial_number [11]);

   year = (int) (coded_hex / 372) + 1980;

   month = (int) ((coded_hex % 372) / 31) + 1;

   day = (coded_hex % 31);

   nsprintf (date_str, sizeof(date_str), "%02d/%02d/%02d", month, day, year);

} /* decode_serial_number() */

char **GetMFG( void )
{
 U8 tmp[SERIAL_NUMBER_SIZE];

 if ( wms_ptrs[0] == 0 )
 {
  if ( GetPICSerial( tmp ) != SUCCESS )
  {
   memcpy(&serial_str,"serial number!",15);
   memcpy(&type_str,"Cannot read ",13);
   memcpy(&date_str,"PIC Error",10);
  }
  else
  {
   decode_serial_number( tmp );
  }
  wms_ptrs[ MFG_SERIAL ] = serial_str;
  wms_ptrs[ MFG_TYPE   ] = type_str;
  wms_ptrs[ MFG_DATE   ] = date_str;
 }
 return wms_ptrs;
}
