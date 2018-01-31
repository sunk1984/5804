/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2007   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_ParseIP.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Portions Copyright 2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1993 NetPort Software, all rights reserved. */


#include "IP_Int.h"
#include "IP_socket.h"


/* FUNCTION: parse_ipad()
*
* parse_ipad(long_out, string_in); Looks for an IP address in
* string_in buffer, makes an IP address (in big endian) in long_out.
* returns NULL if OK, else returns a pointer to a string describing
* problem.
*
*
* ipout      pointer to IP address to set
* sbits      default subnet bit number
* stringin  buffer with ascii to parse
*/
char * IP_ParseIPAddr(ip_addr * ipout,  unsigned *  sbits, char *   stringin) {
   char *   cp;
   int   dots  =  0; /* periods imbedded in input string */
   int   number;
   union
   {
      U8    c[4];
      U32   l;
   } retval;
   char *   toobig   = "each number must be less than 255";

   cp = stringin;
   while (*cp) {
     if (*cp > '9' || *cp < '.' || *cp == '/') {
       return("all chars must be digits (0-9) or dots (.)");
     }
     if (*cp == '.') {
       dots++;
     }
     cp++;
   }

   if ( dots < 1 || dots > 3 )
      return("string must contain 1 - 3 dots (.)");

   cp = stringin;
   if ((number = atoi(cp)) > 255)   /* set net number */
      return(toobig);

   retval.c[0] = (U8)number;

   while (*cp != '.')cp++; /* find dot (end of number) */
      cp++;             /* point past dot */

      if (dots == 1 || dots == 2) {
        retval.c[1] = 0;
      } else {
        number = atoi(cp);
        while (*cp != '.') {
          cp++; /* find dot (end of number) */
        }
        cp++;             /* point past dot */
        if (number > 255) {
          return(toobig);
        }
        retval.c[1] = (U8)number;
   }

   if (dots == 1) {
     retval.c[2] = 0;
   } else {
      number = atoi(cp);
      while (*cp != '.')cp++; /* find dot (end of number) */
         cp++;             /* point past dot */
      if (number > 255) return(toobig);
         retval.c[2] = (U8)number;
   }

   if ((number = atoi(cp)) > 255) {
      return(toobig);
   }
   retval.c[3] = (U8)number;

   if (retval.c[0] < 128) *sbits = 8;
      else if(retval.c[0] < 192) *sbits = 16;
      else *sbits = 24;

      *ipout = retval.l;      /* everything went OK, return number */
   return(NULL);        /* return OK code (no error string) */
}



/*************************** End of file ****************************/
