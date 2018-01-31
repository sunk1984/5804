/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : main.c
Purpose : Skeleton program for OS
--------  END-OF-HEADER  ---------------------------------------------
*/

#include "includes.h"


/*********************************************************************
*
*       main
*
*********************************************************************/

int main(void) {
	OS_IncDI();                      /* Initially disable interrupts  	*/
	OS_InitKern();                   /* initialize OS				*/
	OS_InitHW();                     /* initialize Hardware for OS    	*/
	TASK_Init();				     /* Initialize tasks and sem      	*/
	
	OS_Start();                      /* Start multitasking            	*/
	return 0;
}
