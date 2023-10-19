/* 	$Id: nsc_defines.h,v 1.1 1996/05/07 00:08:36 shepperd Exp $	 */
/****************************************************************************
    Copyright (C) 1995 National Semiconductor Corp.  All Rights Reserved
*****************************************************************************
*
*   File:               CDEFINES.H
*
*   Purpose:            Common C definitions.
*
*   Update History:      1/10/95 GDV Created
*                        1/19/95 MPF Added BIT0-15 defines
*
****************************************************************************/

#ifndef CDEFINES_H
#define CDEFINES_H

#define SHOW_ROUTINES                   /* comment out to avoid compiling */
					/* in routines for showing output */
#if !defined(FALSE)
#define FALSE               0
#define TRUE                1
#endif

#define MIN(x,y)            (((x) > (y)) ? (y) : (x))
#define MAX(x,y)            (((x) > (y)) ? (x) : (y))
#define INRANGE(a,b,c)      ((((b) <= (a)) && ((a) <= (c))) ? 1 : 0)

#define EXTERN              extern
#define PRIVATE             static
#define PUBLIC              

#define Loop                while(1){
#define End                 }

#ifndef NT_DDK
typedef   signed int        INT;
#else
typedef   signed short      INT;
#endif

typedef   signed char       INT8;
typedef   signed short      INT16;
typedef   signed int        INT32;
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned short      USHORT;
typedef unsigned long       ULONG;
typedef unsigned char       SBOOL;
typedef unsigned char       SFLAGS;
typedef unsigned short      FLAGS;
typedef          char *     STRING;
typedef unsigned char *     BUFFER;

#ifdef _INC_WINDOWS
#define WINDOWS_H
#endif /* _INC_WINDOWS */

#ifdef __WINDOWS_H
#define WINDOWS_H
#endif

#ifndef WINDOWS_H
#ifndef NT_DDK
#define STRICT
typedef const void	*   HANDLE;
typedef void                VOID;
typedef unsigned int        UINT;
#else
#define VOID				void
typedef unsigned short		UINT;
#endif

typedef unsigned short      BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef signed long         LONG;

/* random dos b.s.
#ifndef NT_DDK
#define CALLBACK            _far _pascal
#define WINAPI              _far _pascal
#define WINAPI_C            _far cdecl
#endif
*/

#define LONIB(b)            ((b) & 0x0F)
#define HINIB(b)            (((b) >> 4) & 0x0F)
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((UINT)(w) >> 8) & 0xFF))
#define LOWORD(l)           ((WORD)(DWORD)(l))
#define HIWORD(l)           ((WORD)((((DWORD)(l)) >> 16) & 0xFFFF))
#define MAKELONG(low, high) ((LONG)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
#endif

#ifndef MAKEWORD
#define MAKEWORD(low, high) ((WORD)(((BYTE)(low)) | (((WORD)((BYTE)(high))) << 8)))
#endif

/* more segment crapola
#ifndef MK_FP
#define MK_FP(seg,ofs)      ((void _seg *)(seg) + (void *)(ofs))
#define FP_SEG(fp)          ((WORD)(void _seg *)(void far *)(fp))
#define FP_OFF(fp)          ((WORD)(fp))
#endif
*/

#ifndef NULL
#define NULL    ((void *)0)
#endif 

#endif

