/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : OS_USBH_Start.c
Purpose : Sample program for embOS & USBH USB host stack.
          Demonstrates use of the USBH stack.
--------- END-OF-HEADER --------------------------------------------*/


#include "includes.h"

/*********************************************************************
*
*       Defines configurable
*
**********************************************************************
*/
#define MAX_KEY_DATA_ITEMS        10

/*********************************************************************
*
*       Defines non-configurable
*
**********************************************************************
*/
#define MOUSE_EVENT       (1 << 0)
#define KEYBOARD_EVENT    (1 << 1)

#define COUNTOF(a)  (sizeof(a)/sizeof(a[0]))

#define USBH_RECV_OK        0
#define USBH_RECV_NO_DATA   1
#define RECV_RECV_NOT_FULL  2
#define USBH_PARAMETER_ERR  3


/*********************************************************************
*
*       Local data definitions
*
**********************************************************************
*/
enum {
  TASK_PRIO_APP,
  TASK_PRIO_USBH_MAIN,
  TASK_PRIO_USBH_ISR
};

const  SCANCODE_TO_CH _aScanCode2ChTable[] = {  
  { 0x04, 'A'},
  { 0x05, 'B'},
  { 0x06, 'C'},
  { 0x07, 'D'},
  { 0x08, 'E'},
  { 0x09, 'F'},
  { 0x0A, 'G'},
  { 0x0B, 'H'},
  { 0x0C, 'I'},
  { 0x0D, 'J'},
  { 0x0E, 'K'},
  { 0x0F, 'L'},
  { 0x10, 'M'},
  { 0x11, 'N'},
  { 0x12, 'O'},
  { 0x13, 'P'},
  { 0x14, 'Q'},
  { 0x15, 'R'},
  { 0x16, 'S'},
  { 0x17, 'T'},
  { 0x18, 'U'},
  { 0x19, 'V'},
  { 0x1A, 'W'},
  { 0x1B, 'X'},
  { 0x1C, 'Y'},
  { 0x1D, 'Z'},
  
  { 0x1E, '1'},
  { 0x1F, '2'},
  { 0x20, '3'},
  { 0x21, '4'},
  { 0x22, '5'},
  { 0x23, '6'},
  { 0x24, '7'},
  { 0x25, '8'},
  { 0x26, '9'},
  { 0x27, '0'},
//  { 0x28, '^'},
  { 0x2D, '-'},
};

char  _ScanCode2Ch(unsigned Code) {
  unsigned i;
  //Dprintf("%02x-",Code);
  for (i = 0; i < COUNTOF(_aScanCode2ChTable); i++) {
    if (Code == _aScanCode2ChTable[i].KeyCode) {
      return  _aScanCode2ChTable[i].ch;
    }
  }
  return 0;
}


/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static OS_STACKPTR int         _StackMain[1024/sizeof(int)];
static OS_TASK                 _TCBMain;
static OS_STACKPTR int         _StackIsr[1024/sizeof(int)];
static OS_TASK                 _TCBIsr;

static OS_EVENT                        _Event;
//static USBH_HID_KEYBOARD_DATA          _aKeyData[MAX_KEY_DATA_ITEMS];
static USBH_HID_MOUSE_DATA             _MouseData;
static USBH_HID_SCANGUN_DATA           _scanGunData;

static volatile U8                     _EventOccurred;
static volatile int                    _KeybCnt;
static volatile int                    _MouseCnt;

#define RX_BUF_SIZE                 30

U8  ScanRxBuffer[RX_BUF_SIZE];      //接收队列
U32 rx_rd_index = 0;                //读指针
U32 rx_wr_index = 0;                //写指针
U32 rx_counter = 0;                 //在队列中已经接收到的字符个数
U32 rx_buffer_overflow = 0;         //接收缓冲区溢出标志

/*********************************************************************
*
*       _LoadKeyData
*/
/*
static USBH_HID_KEYBOARD_DATA  * _LoadKeyData(void) {
  USBH_HID_KEYBOARD_DATA  * pKeyData;
  
  if (_KeybCnt - 1 < 0) {
    return NULL;
  }
  pKeyData = &_aKeyData[_KeybCnt--];
  return pKeyData;
}
*/


static void _OnScanGunChange(USBH_HID_SCANGUN_DATA  * pScanGunData) {
    _scanGunData = *pScanGunData;
  
    //存放数据
    ScanRxBuffer[rx_wr_index] = _scanGunData.data;
    if(++ rx_wr_index == RX_BUF_SIZE){
        rx_wr_index = 0;
    }
    if(++ rx_counter == RX_BUF_SIZE){
        rx_counter = 0;
        rx_buffer_overflow  =1;
    }
            
    _EventOccurred |= KEYBOARD_EVENT;
    OS_EVENT_Pulse(&_Event);
}

/*********************************************************************
*
*       _OnKeyboardChange
*/
/*
static void _OnKeyboardChange(USBH_HID_KEYBOARD_DATA  * pKeyData) {
  _StoreKeyData(pKeyData);
  _EventOccurred |= KEYBOARD_EVENT;
  OS_EVENT_Pulse(&_Event);
}
*/
/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       MainTask
*/
//static int scanGunTime =0;
void USBH_HID_Task(void);
void USBH_HID_Task(void) {
//    USBH_HID_KEYBOARD_DATA  * pKeyData;
  
    USBH_Init();
    OS_SetPriority(OS_GetTaskID(), TASK_PRIO_APP);                                       // This task has highest prio for real-time application
    OS_CREATETASK(&_TCBMain, "USBH_Task", USBH_Task, TASK_PRIO_USBH_MAIN, _StackMain);   // Start USBH main task
    OS_CREATETASK(&_TCBIsr, "USBH_isr", USBH_ISRTask, TASK_PRIO_USBH_ISR, _StackIsr);    // Start USBH main task
    OS_EVENT_Create(&_Event);
    USBH_HID_Init();
    USBH_HID_SetOnScanGunStateChange(_OnScanGunChange);
//    USBH_HID_SetOnMCMStateChange(_OnScanGunChange);
    while (1) {
        //BSP_ToggleLED(1);
        OS_EVENT_Wait(&_Event);
        if ((_EventOccurred & (MOUSE_EVENT)) == MOUSE_EVENT) {
            USBH_Logf_Application("Received mouse change notification: xRelative: %d, yRelative: %d, WheelRelative: %d, ButtonState: %d",
                                _MouseData.xChange, _MouseData.yChange, _MouseData.WheelChange, _MouseData.ButtonState);
            _EventOccurred &= ~(MOUSE_EVENT);
        } 
        else if ((_EventOccurred & (KEYBOARD_EVENT)) == KEYBOARD_EVENT) {
            _EventOccurred &= ~(KEYBOARD_EVENT);
        }
    }
}

static INT8U ScanGun_readChar(INT8U *return_data){
    //关中断
    OS_DI();    
    //接收数据队列中无数据可取，退出
    if(rx_counter ==0)
        return USBH_RECV_NO_DATA;    
    
    *return_data = ScanRxBuffer[rx_rd_index];
    if(++ rx_rd_index == RX_BUF_SIZE)
        rx_rd_index =0 ;
    

    rx_counter --;
    
    //开中断
    OS_EI();
    
    return USBH_RECV_OK;
    
}

static INT8U ScanGun_readFrame(INT8U *pframe,INT32U frame_length,INT32U *recv_bytes){
    INT8U temp =0;
    INT32U frame_number =0 ;
    if(pframe == NULL)
        return USBH_PARAMETER_ERR;
    
    for(frame_number =0;frame_number < frame_length; frame_number++ ){

        if( USBH_RECV_OK == ScanGun_readChar(&temp)){
            *(pframe + frame_number) = temp;
            OS_Delay(50);
        }
        else {
            if(frame_number ==0)  {//完全没有收到
                *recv_bytes = 0; 
                return USBH_RECV_NO_DATA;
            }
            break;     //未收满规定的字节数
        }
        
    }
    *recv_bytes = frame_number;
    if(*recv_bytes != frame_length)  //未收满规定的字节数
        return RECV_RECV_NOT_FULL;
    return USBH_RECV_OK;
        
    
}

void SCANGUN_GetBarCode(U8 * barCode, U8 len)
{
    INT32U rbyte = 0;
    U8 status = USBH_RECV_NO_DATA;
    U8 cData[20] = {0};
    
    ScanGun_readFrame((U8 * )cData, len, &rbyte);
    
    while(1)
    {
        status = ScanGun_readFrame((U8 * )cData, len, &rbyte);
        
        if( status == USBH_RECV_OK || status == RECV_RECV_NOT_FULL )
        {
	        strcpy((char * )barCode, (char * )cData);
            Dprintf("BarCode: %s\r\n", barCode);
            break;
        }
        
        OS_Delay(100);
    }
}

U8 SCANGUN_BarCode_PutDUT(U8 * barCode, U8 len)
{
    INT32U rbyte = 0;
    U8 status = USBH_RECV_NO_DATA;
    U8 cData[20] = {0};
    U8 ret = FALSE;
    
    ScanGun_readFrame((U8 * )cData, len, &rbyte);
    
    while(OS_GetCSemaValue(&DutReady_Sem) == FALSE)
    {
        status = ScanGun_readFrame((U8 * )cData, len, &rbyte);
        
        if( status == USBH_RECV_OK || status == RECV_RECV_NOT_FULL )
        {
	        strcpy((char * )barCode, (char * )cData);
            Dprintf("BarCode: %s\r\n", barCode);
            ret = TRUE;
            break;
        }
        
        OS_Delay(100);
    }
    return(ret);
}


INT8U test_data[13]={0};
void USBH_GUN_Task(){
    INT32U length =0;
    INT32U status =0;
    
    OS_Delay(10000);
    
    while(1){
 
        status = ScanGun_readFrame(test_data,sizeof(test_data) ,&length);
        
        if( (status == USBH_RECV_OK) || (status == RECV_RECV_NOT_FULL)){
            Dprintf("status is %d\n\r",status);
            Dprintf("data is ");
            for(INT32U i =0;i< length ;i++){
                Dprintf("%c ",test_data[i] );
            }
            Dprintf("\n\r");
        }

        
        OS_Delay(100);
            
    }
}


