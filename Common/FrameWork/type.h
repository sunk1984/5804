/*******************************************************************************
    type.h
    top type define file

    Copyright(C) 2010, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2010.07.14  ver.1.00    First release
*******************************************************************************/

#ifndef _TYPE_H_
#define _TYPE_H_


#define __inline    inline

#ifndef NULL
#define NULL    ((void *)0)
#endif

#ifndef FALSE
#define FALSE   (0)
#endif

#ifndef TRUE
#define TRUE    (1)
#endif

#ifndef FAIL
#define FAIL    (0)
#endif

#ifndef PASS
#define PASS    (1)
#endif

#ifndef ON
#define ON      (1)
#endif

#ifndef OFF
#define OFF     (0)
#endif

typedef int				INT;
typedef unsigned int    INT32U;
typedef unsigned int	UINT;
typedef unsigned int    BOOL;

/* These types must be 8-bit integer */
typedef char			CHAR;
typedef unsigned char   INT8U;
typedef unsigned char	UCHAR;
typedef unsigned char	BYTE;

/* These types must be 16-bit integer */
typedef short			SHORT;
typedef unsigned short  INT16U;
typedef unsigned short	USHORT;
typedef unsigned short	WORD;
typedef unsigned short	WCHAR;

/* These types must be 32-bit integer */
typedef long			LONG;
typedef unsigned long	ULONG;
typedef unsigned long	DWORD;

/*  These types copy from cpu.h*/
typedef unsigned char  	BOOLEAN;
typedef unsigned long   INT64U;
typedef signed int      INT32S;
typedef unsigned int    uint32_t;

typedef signed char     S8;
typedef signed short    S16;
typedef signed int    	S32;

typedef void (*func)();

#endif//_TYPE_H_
