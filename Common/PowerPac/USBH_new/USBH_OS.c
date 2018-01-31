/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OS_embOS.c
Purpose     :
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include <stdlib.h>
#include "USBH_Int.h"
#include "RTOS.h"

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

static OS_RSEMA _Lock;
static OS_RSEMA _LockSys;
static OS_EVENT _EventNet;
static OS_EVENT _EventISR;
static OS_EVENT _aEventUserTask[1];

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_OS_DisableInterrupt
*/
void USBH_OS_DisableInterrupt(void) {
  OS_IncDI();
}

/*********************************************************************
*
*       USBH_OS_EnableInterrupt
*/
void USBH_OS_EnableInterrupt(void) {
  OS_DecRI();
}


/**********************************************************
*
*       USBH_OS_Init
*
*  Function description
*    Initialize (create) all objects required for task syncronisation.
*/
void USBH_OS_Init(void) {
  OS_EVENT_Create(&_EventNet);
  OS_EVENT_Create(&_EventISR);
  OS_EVENT_Create(&_aEventUserTask[0]);
  OS_CREATERSEMA(&_Lock);
  OS_CREATERSEMA(&_LockSys);
}


/*********************************************************************
*
*       USBH_OS_Lock
*
*  Function description
*    The stack requires a single lock, typically a resource semaphore or mutex.
*    This function locks this object, guarding sections of the stack code
*    against other threads.
*    If the entire stack executes from a single task, no functionality is required here.
*/
void USBH_OS_Lock(void) {
  OS_Use(&_Lock);
}

/*********************************************************************
*
*       USBH_OS_Unlock
*
*  Function description
*    Unlocks the single lock used locked by a previous call to USBH_OS_Lock().
*/
void USBH_OS_Unlock(void) {
  OS_Unuse(&_Lock);
}

/*********************************************************************
*
*       USBH_OS_LockSys
*
*  Function description
*    The stack requires a single lock, typically a resource semaphore or mutex.
*    This function locks this object, guarding memory operations.
*    If the entire stack executes from a single task, no functionality is required here.
*/
void USBH_OS_LockSys(void) {
  OS_Use(&_LockSys);
}

/*********************************************************************
*
*       USBH_OS_UnlockSys
*
*  Function description
*    Unlocks the single lock used locked by a previous call to USBH_OS_LockMemory().
*/
void USBH_OS_UnlockSys(void) {
  OS_Unuse(&_LockSys);
}



/*********************************************************************
*
*       USBH_OS_GetTime32
*
*  Function description
*    Return the current system time in ms.
*    The value will wrap around after app. 49.7 days. This is taken into
*    account by the stack.
*/
U32  USBH_OS_GetTime32(void) {
  return OS_GetTime32();
}

/*********************************************************************
*
*       USBH_OS_Delay
*
*  Function description
*    Blocks the calling task for a given time.
*/
void USBH_OS_Delay(unsigned ms) {
  OS_Delay(ms + 1);
}

/*********************************************************************
*
*       USBH_OS_WaitNetEvent
*
*  Function description
*    Called from USBH_MainTask() only.
*    Blocks until the timeout expires or a USBH-event occurs,
*    meaning USBH_OS_SignalNetEvent() is called from an other task or ISR.
*/
void USBH_OS_WaitNetEvent(unsigned ms) {
  if ((ms + 1) >= 0x80000000) {
    USBH_PANIC("Invalid timeout value");
  }
  OS_EVENT_WaitTimed(&_EventNet, ms + 1);
}

/*********************************************************************
*
*       USBH_OS_SignalNetEvent
*
*  Function description
*    Wakes the USBH_MainTask() if it is waiting for a event or timeout in
*    the fucntion USBH_OS_WaitNetEvent().
*/
void USBH_OS_SignalNetEvent(void) {
  OS_EVENT_Set(&_EventNet);
}

/*********************************************************************
*
*       USBH_OS_WaitISR
*
*  Function description
*    Called from USBH_ISRTask() only.
*    Blocks until USBH_OS_WaitISR(), called from ISR, wakes the task.
*/
void USBH_OS_WaitISR(void) {
  OS_EVENT_Wait(&_EventISR);
}

/*********************************************************************
*
*       USBH_OS_SignalISR
*
*  Function description
*    Wakes the USBH_ISRTask()
*/
void USBH_OS_SignalISR(void) {
  OS_EVENT_Set(&_EventISR);
}

/*********************************************************************
*
*       USBH_OS_AllocEvent
*
*  Function description
*    Allocates and returns an event object.
*/
USBH_OS_EVENT_OBJ * USBH_OS_AllocEvent(void) {
  USBH_OS_EVENT_OBJ * p;

  p = (USBH_OS_EVENT_OBJ *)&_aEventUserTask[0];
  return p;
}

/*********************************************************************
*
*       USBH_OS_FreeEvent
*
*  Function description
*    Releases an object event.
*/
void USBH_OS_FreeEvent(USBH_OS_EVENT_OBJ * pEvent) {
  USBH_USE_PARA(pEvent);
}

/*********************************************************************
*
*       USBH_OS_SetEvent
*
*  Function description
*    Sets the state of the specified event object to signaled.
*/
//
void USBH_OS_SetEvent(USBH_OS_EVENT_OBJ * pEvent) {
  OS_EVENT_Set((OS_EVENT *)pEvent);
}

/*********************************************************************
*
*       USBH_OS_ResetEvent
*
*  Function description
*    Sets the state of the specified event object to none-signaled.
*/
void USBH_OS_ResetEvent(USBH_OS_EVENT_OBJ * pEvent) {
  OS_EVENT_Reset((OS_EVENT *)pEvent);
}

/*********************************************************************
*
*       USBH_OS_WaitEvent
*
*  Function description
*    Wait for the specific event.
*/
void USBH_OS_WaitEvent(USBH_OS_EVENT_OBJ * pEvent) {
  OS_EVENT_Wait((OS_EVENT *)pEvent);
}

/*********************************************************************
*
*       USBH_OS_WaitEventTimed
*
*  Function description
*    Wait for the specific event within a given time-out.
*/
int USBH_OS_WaitEventTimed(USBH_OS_EVENT_OBJ * pEvent, U32 milliSeconds) {
  return OS_EVENT_WaitTimed((OS_EVENT *)pEvent, milliSeconds);
}

/******************************* EOF ********************************/
