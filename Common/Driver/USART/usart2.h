#ifndef _USART_H_
#define _USART_H_

// BUF ×î´ó65535
/*
#define US0_RX_BUF_MAX      400
#define US0_TX_BUF_MAX      100
#define US1_RX_BUF_MAX      800
#define US1_TX_BUF_MAX      100
#define US2_RX_BUF_MAX      400
#define US2_TX_BUF_MAX      100
#define US3_RX_BUF_MAX      1
#define US3_TX_BUF_MAX      1
#define USDBGU_RX_BUF_MAX   400
#define USDBGU_TX_BUF_MAX   100
*/
#define US0_RX_BUF_MAX      512
#define US0_TX_BUF_MAX      256
#define US1_RX_BUF_MAX      256
#define US1_TX_BUF_MAX      256
#define US2_RX_BUF_MAX      256	//256
#define US2_TX_BUF_MAX      256	//256
#define US3_RX_BUF_MAX      10	//256
#define US3_TX_BUF_MAX      30	//256
#define USDBGU_RX_BUF_MAX   256
#define USDBGU_TX_BUF_MAX   256

// recv errcode
#define RECV_OK             0x00
#define RECV_ERR            0x01
#define RECV_FRAME_BUF_FULL 0x02
#define PARAMETER_ERR       0x03
//send errcode
#define PDC_TX_END          0x00
#define PDC_TX_NO_END       0x01
#define PDC_TX_DISABLE      0x02
// usart config define
#define USART0                  0
#define USART1                  1
#define USART2                  2
#define USART3                  3
#define USDBGU                  4

#define US_RS232                0
#define US_RS485                1

#define CHRL_5                  0
#define CHRL_6                  1
#define CHRL_7                  2
#define CHRL_8                  3

#define US_NONE                 4
#define US_ODD                  1
#define US_EVEN                 0

#define STOP_1                  0
#define STOP_1_5                1
#define STOP_2                  2


typedef struct _USART_CONFIG {
    INT8U  usartport;
    INT32U usartmode;
    INT32U databit;
    INT8U  parity;
    INT32U stopbit;
    INT32U baudrate;
} USART_CONFIG;


//public functions

extern void    US0_ISR_Handler();
extern void    US1_ISR_Handler();
extern void    US2_ISR_Handler();
extern void    US3_ISR_Handler();
extern void    USDBGU_ISR_Handler();

extern BOOL    UsartInit(USART_CONFIG usart, INT32U masterclock);
extern BOOL    UsartRecvStart(INT32U usart);
extern BOOL    UsartRecvReset(INT32U usart);

extern INT32U  UsartGetChar(INT32U usart,INT8U *recv_char);
extern INT32U  UsartGetFrame(INT32U usart, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes);
extern INT32U  UsartGetFrame_by_1BytesEnd(INT32U usart, INT8U frame_end_char, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes);
extern INT32U  UsartGetFrame_by_2BytesEnd(INT32U usart,INT8U frame_end_char1, INT8U frame_end_char2, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes);
extern INT32U  UsartGetFrame_by_Len(INT32U usart,INT32U frame_len, INT8U *pframe, INT32U frame_buf_size);
extern BOOL    UsartPutChar(INT32U usart, INT8U c);
extern BOOL    UsartPutStr(INT32U usart, INT8U *pstr);
extern BOOL    UsartPutFrame(INT32U usart, INT8U *pstr, INT32U length);
extern BOOL    UsartSendFrameStart(INT32U usart, INT8U *pstr, INT32U length);
extern INT32U  UsartSendFrameCallback(INT32U usart, INT32U *unsendcount);

extern INT32U UART_WriteStr( unsigned char * ptrChar );

#endif
