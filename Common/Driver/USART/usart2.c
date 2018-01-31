/******************************************************************************
    usart.c
    API functions

    Copyright(C) 2009, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2010.03.04  ver.2.00    First release by chenli
******************************************************************************/
#include "includes.h"

// data types

#define RTOS

// USART buffer and point
INT8U   US0_rx_buf[US0_RX_BUF_MAX];
INT8U   US0_tx_buf[US0_TX_BUF_MAX];
INT32U  US0RxOutPtr;

INT8U   US1_rx_buf[US1_RX_BUF_MAX];
INT8U   US1_tx_buf[US1_TX_BUF_MAX];
INT32U  US1RxOutPtr;

INT8U   US2_rx_buf[US2_RX_BUF_MAX];
INT8U   US2_tx_buf[US2_TX_BUF_MAX];
INT32U  US2RxOutPtr;

INT8U   US3_rx_buf[US3_RX_BUF_MAX];
INT8U   US3_tx_buf[US3_TX_BUF_MAX];
INT32U  US3RxOutPtr;

INT8U   USDBGU_rx_buf[USDBGU_RX_BUF_MAX];
INT8U   USDBGU_tx_buf[USDBGU_TX_BUF_MAX];
INT32U  USDBGURxOutPtr;

// public functions
//driver
BOOL    UsartInit(USART_CONFIG usart, INT32U masterclock);
BOOL    UsartRecvStart(INT32U usart);
BOOL    UsartRecvReset(INT32U usart);
//API
INT32U  UsartGetChar(INT32U usart,INT8U *recv_char);
INT32U  UsartGetFrame(INT32U usart, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes);
INT32U  UsartGetFrame_by_1BytesEnd(INT32U usart,INT8U frame_end_char, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes);
INT32U  UsartGetFrame_by_2BytesEnd(INT32U usart,INT8U frame_end_char1, INT8U frame_end_char2, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes);
INT32U  UsartGetFrame_by_Len(INT32U usart,INT32U frame_len, INT8U *pframe, INT32U frame_buf_size);
BOOL    UsartPutChar(INT32U usart, INT8U c);
BOOL    UsartPutStr(INT32U usart, INT8U *pstr);
BOOL    UsartPutFrame(INT32U usart, INT8U *pstr, INT32U length);
BOOL    UsartSendFrameStart(INT32U usart, INT8U *pstr, INT32U length);
INT32U  UsartSendFrameCallback(INT32U usart, INT32U *unsendcount);

INT32U Dprintf(char *lpszFormat, ...);
/*
********************************************************************************
                            UsartInit

function: config the usart port to USART0~4, DBGU

parameters:  usart, the usart port config structure, including:
                   .usartport, assign the value: USART0,USART1,USART2,USART3,USDBGU
                   .usartmode, assign the value: US_RS232, US_RS485
                   .databit, assigne the value: CHRL_5, CHRL_6, CHRL_7, CHRL_8
                   .parity, assign the value : US_NONE, US_ODD, US_EVEN
                   .stopbit, assign the value: STOP_1, STOP_1_5, STOP_2
                   .baudrate, assign the standard baudrate, eg 115200, 9600 etc
(notice: if the usartport is USDBGU, only the .parity & .baudrate config is available)

            masterclock, the clock provided for BDGU, here is the BOARD_MCLK

return: TRUE
        FALSE
********************************************************************************
*/
BOOL UsartInit(USART_CONFIG usart, INT32U masterclock)
{
    AT91S_USART *us;

    //对参数进行判断
    if (usart.usartport == 4)  // DBGU 配置参数校验
    {
        if( (usart.usartmode != 0) | (usart.databit != 3) | (usart.parity > 7)
             | (usart.stopbit != 0) | (usart.baudrate > 115200) )

            return (FALSE);
    }
    else                      //USART0~3 配置参数校验
    {
        if( (usart.usartport > 3) | (usart.usartmode > 1) | (usart.databit > 3)
            | (usart.parity > 7) | (usart.stopbit > 2) | (usart.baudrate > 115200) )

            return (FALSE);
    }

    switch ( usart.usartport )
    {
        case USART0:
            //initial pins
            AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB4 | AT91C_PIO_PB5;
            AT91C_BASE_PIOB->PIO_ASR = AT91C_PIO_PB4 | AT91C_PIO_PB5;

            if (usart.usartmode)  //RS485 模式，需初始化RTS
            {
                AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB26;
                AT91C_BASE_PIOB->PIO_ASR = AT91C_PIO_PB26;
            }

            //enable PMC clock
            AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_US0);

            us = AT91C_BASE_US0;
            //initial usart buf point
            US0RxOutPtr = 0;

            break;

        case USART1:
             //initial pins
            AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB6 | AT91C_PIO_PB7;
            AT91C_BASE_PIOB->PIO_ASR = AT91C_PIO_PB6 | AT91C_PIO_PB7;

            if (usart.usartmode)  //RS485 模式，需初始化RTS
            {
                AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB28;
                AT91C_BASE_PIOB->PIO_ASR = AT91C_PIO_PB28;
            }

            //enable PMC clock
            AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_US1);

            us = AT91C_BASE_US1;

            //initial usart buf point
            US1RxOutPtr = 0;

            break;

        case USART2:
             //initial pins
            AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB8 | AT91C_PIO_PB9;
            AT91C_BASE_PIOB->PIO_ASR = AT91C_PIO_PB8 | AT91C_PIO_PB9;

            if (usart.usartmode)  //RS485 模式，需初始化RTS
            {
                AT91C_BASE_PIOA->PIO_PDR = AT91C_PIO_PA4;
                AT91C_BASE_PIOA->PIO_ASR = AT91C_PIO_PA4;
            }

             //enable PMC clock
            AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_US2);

            us = AT91C_BASE_US2;

            //initial usart point
            US2RxOutPtr = 0;

            break;

        case USART3:
             //initial pins
            AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB10 | AT91C_PIO_PB11;
            AT91C_BASE_PIOB->PIO_ASR = AT91C_PIO_PB10 | AT91C_PIO_PB11;

            if (usart.usartmode)  //RS485 模式，需初始化RTS
            {
                AT91C_BASE_PIOC->PIO_PDR = AT91C_PIO_PC8;
                AT91C_BASE_PIOC->PIO_ASR = AT91C_PIO_PC8;
            }

             //enable PMC clock
            AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_US3);

            us = AT91C_BASE_US3;

            //initial usart point
            US3RxOutPtr = 0;

            break;

        case USDBGU:
             //initial pins
            AT91C_BASE_PIOB->PIO_PDR = AT91C_PIO_PB15 | AT91C_PIO_PB14;
            AT91C_BASE_PIOB -> PIO_ASR = AT91C_PIO_PB15 | AT91C_PIO_PB14;

             //enable PMC clock
             //  AT91C_BASE_PMC->PMC_PCER   = 1 << AT91C_ID_SYS; //系统默认为开

            AT91C_BASE_DBGU->DBGU_CR   = AT91C_US_RSTSTA | AT91C_US_RSTRX | AT91C_US_RSTTX;

            // Configure baudrate
            AT91C_BASE_DBGU->DBGU_BRGR =  (masterclock / usart.baudrate) / 16;

            // Configure mode
            AT91C_BASE_DBGU->DBGU_MR   = AT91C_US_CHMODE_NORMAL | ((usart.parity) << 9);

            //enable transmition
            AT91C_BASE_DBGU->DBGU_CR   = AT91C_US_RXEN | AT91C_US_TXEN;

            //initial usart point
            USDBGURxOutPtr = 0;

            return (TRUE);

        default:
            return FALSE;
    }

    // Reset and disable receiver & transmitter
    us->US_CR = AT91C_US_RSTRX | AT91C_US_RSTTX
                | AT91C_US_RXDIS | AT91C_US_TXDIS;
    // Configure mode
    us->US_MR = 0;
    us->US_MR = (usart.usartmode) | ((usart.databit) << 6)
                | ((usart.parity) << 9) | ((usart.stopbit) << 12);

    // Configure baudrate
    us->US_BRGR = (masterclock / usart.baudrate) / 16;

    //enable transmition
    us->US_CR =  AT91C_US_RXEN | AT91C_US_TXEN ;

    return (TRUE);
}


/*
********************************************************************************
                            UsartRecvStart

function: start the receiving, including  configure the PDC, enable the rx ISR

parameters:  usart, the usart port number,0,1,2,3,4 or the define USART0,USART1,
             USART2,USART3,USDBGU

return:  TRUE,
         FALSE, wrong USART NAME

********************************************************************************
*/
BOOL  UsartRecvStart(INT32U usart)
{
    INT32U i, bufmax;
    AT91S_USART *us;
    INT8U *pbuf;

    if(usart == USDBGU)
    {
        // clear buffer
        for(i = 0; i < USDBGU_RX_BUF_MAX; i ++)
        {
            USDBGU_rx_buf[i] = 0;
        }
         //关闭PDC
        AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_RXTDIS;

        // 初始化PDC
        AT91C_BASE_DBGU->DBGU_RPR = (INT32U) USDBGU_rx_buf;
        AT91C_BASE_DBGU->DBGU_RCR = USDBGU_RX_BUF_MAX;

        // 使能中断
        AT91C_BASE_DBGU->DBGU_IER = ((unsigned int) 0x1 <<  3);// ENDRX

        //使能PDC
        AT91C_BASE_DBGU->DBGU_PTCR =  AT91C_PDC_RXTEN;

        return TRUE;
    }

    switch(usart)
    {
        case USART0:
            pbuf   = US0_rx_buf;
            bufmax = US0_RX_BUF_MAX;
            us     = AT91C_BASE_US0;
            break;

        case USART1:
            pbuf   = US1_rx_buf;
            bufmax = US1_RX_BUF_MAX;
            us     = AT91C_BASE_US1;
            break;

        case USART2:
            pbuf   = US2_rx_buf;
            bufmax = US2_RX_BUF_MAX;
            us     = AT91C_BASE_US2;
            break;

        case USART3:
            pbuf   = US3_rx_buf;
            bufmax = US3_RX_BUF_MAX;
            us     = AT91C_BASE_US3;
            break;

        default : return FALSE;
    }
    //clear buffer
    for(i = 0; i < bufmax; i ++)
    {
        pbuf[i] = 0;
    }
    //关闭PDC
    us->US_PTCR = AT91C_PDC_RXTDIS;

    // 初始化PDC
    us->US_RPR = (INT32U)pbuf;
    us->US_RCR = bufmax;

    // 使能中断
    us->US_IER = ((unsigned int) 0x1 <<  3);// ENDRX

    //使能PDC
    us->US_PTCR =  AT91C_PDC_RXTEN;

    //配置中断
    return TRUE;
}

/*
********************************************************************************
                            UsartRecvReset

function: 初始化PDC寄存器，和接收指针，从头开始接收和读取

parameters:usart， USART0,USART1,USART2,USART3,USDBGU

return:TRUE/FALSE

********************************************************************************
*/
BOOL  UsartRecvReset(INT32U usart)
{
    switch(usart)
    {
        case USART0:
            // init rx ring buffer points
            US0RxOutPtr = 0;
            //init PDC
            AT91C_BASE_US0->US_RPR = (INT32U)US0_rx_buf;
            AT91C_BASE_US0->US_RCR = US0_RX_BUF_MAX;
            break;

        case USART1:
            // init rx ring buffer points
            US1RxOutPtr = 0;
            //init PDC
            AT91C_BASE_US1->US_RPR = (INT32U)US1_rx_buf;
            AT91C_BASE_US1->US_RCR = US1_RX_BUF_MAX;
            break;

        case USART2:
            // init rx ring buffer points
            US2RxOutPtr = 0;
            //init PDC
            AT91C_BASE_US2->US_RPR = (INT32U)US2_rx_buf;
            AT91C_BASE_US2->US_RCR = US2_RX_BUF_MAX;
            break;

        case USART3:
            // init rx ring buffer points
            US3RxOutPtr = 0;
            //init PDC
            AT91C_BASE_US3->US_RPR = (INT32U)US3_rx_buf;
            AT91C_BASE_US3->US_RCR = US3_RX_BUF_MAX;
            break;

        case USDBGU:
            // init rx ring buffer points
            USDBGURxOutPtr = 0;
            //init PDC
            AT91C_BASE_DBGU->DBGU_RPR = (INT32U)USDBGU_rx_buf;
            AT91C_BASE_DBGU->DBGU_RCR = USDBGU_RX_BUF_MAX;
            break;

        default : return FALSE;
    }
    return TRUE;
}


/*
********************************************************************************
                            UsartGetChar

function: 从PDC 的 RX_BUF 里取出一帧的数据存到FRAME BUFF里，如果没有完整的一帧，则返回错误
            帧的结束符为 1byte

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            *recv_char, 把收到的字符存到该指针指向的地址里

return: RECV_ERR， 没有收到
        RECV_OK， 收到
        PARAMETER_ERR, 指针为空或者串口名称不对

********************************************************************************
*/
INT32U UsartGetChar(INT32U usart,INT8U *recv_char)
{
    INT32U bufmax, rxinptr;
    INT8U  *pbuf;
    INT32U *rxoutptr;

    if( recv_char == NULL )
        return PARAMETER_ERR;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_rx_buf;
            bufmax   = US0_RX_BUF_MAX;
            rxinptr  = US0_RX_BUF_MAX - (AT91C_BASE_US0->US_RCR);
            rxoutptr = &US0RxOutPtr;
            break;

        case USART1:
            pbuf     = US1_rx_buf;
            bufmax   = US1_RX_BUF_MAX;
            rxinptr  = US1_RX_BUF_MAX - (AT91C_BASE_US1->US_RCR);
            rxoutptr = &US1RxOutPtr;
            break;

        case USART2:
            pbuf     = US2_rx_buf;
            bufmax   = US2_RX_BUF_MAX;
            rxinptr  = US2_RX_BUF_MAX - (AT91C_BASE_US2->US_RCR);
            rxoutptr = &US2RxOutPtr;
            break;

        case USART3:
            pbuf     = US3_rx_buf;
            bufmax   = US3_RX_BUF_MAX;
            rxinptr  = US3_RX_BUF_MAX - (AT91C_BASE_US3->US_RCR);
            rxoutptr = &US3RxOutPtr;
            break;

        case USDBGU:
            pbuf     = USDBGU_rx_buf;
            bufmax   = USDBGU_RX_BUF_MAX;
            rxinptr  = USDBGU_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);
            rxoutptr = &USDBGURxOutPtr;
            break;

       default: return PARAMETER_ERR;
    }

    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//确定当前接收环形BUF的接收指针
    if(rxinptr >= bufmax)
        rxinptr = 0;
    if(*rxoutptr == rxinptr)
        return RECV_ERR;
    else
    {
        *recv_char = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
        return RECV_OK;
    }
}
/*********************************************************************************
                            UsartGetFrame

function: 从PDC 的 RX_BUF 里取出一帧的数据存到FRAME BUFF里，如果没有完整的一帧，则返回错误
            帧沒有结束符

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            frame_end_char, Frame 结束字符，1 byte
            *pframe, frame buff 的起始地址
            frame_buf_size， frame buff 的大小
            *recv_bytes， 收到的一帧大小，返回到改指针指向的地址里

return: RECV_ERR， 没有收到一帧，帧的数据不拷贝到BUF里
        RECV_OK， 收到了一帧，把帧的数据拷贝到BUF里
        PARAMETER_ERR, 指针为空或者串口名称不对
        RECV_FRAME_BUF_FULL, FRAME BUF大小不够存储一帧的数据，不拷贝帧数据

*********************************************************************************/
INT32U UsartGetFrame(INT32U usart, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes)
{
    INT32U i, recvcount;
    INT32U ptrtemp, bufmax, rxinptr;
    INT8U  *pbuf;
    INT32U *rxoutptr;

    if( (pframe == NULL) || (recv_bytes == NULL) )
        return PARAMETER_ERR;

    recvcount = 0;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_rx_buf;
            bufmax   = US0_RX_BUF_MAX;
            rxinptr  = US0_RX_BUF_MAX - (AT91C_BASE_US0->US_RCR);
            rxoutptr = &US0RxOutPtr;
            break;

        case USART1:
            pbuf     = US1_rx_buf;
            bufmax   = US1_RX_BUF_MAX;
            rxinptr  = US1_RX_BUF_MAX - (AT91C_BASE_US1->US_RCR);
            rxoutptr = &US1RxOutPtr;
            break;

        case USART2:
            pbuf     = US2_rx_buf;
            bufmax   = US2_RX_BUF_MAX;
            rxinptr  = US2_RX_BUF_MAX - (AT91C_BASE_US2->US_RCR);
            rxoutptr = &US2RxOutPtr;
            break;

        case USART3:
            pbuf     = US3_rx_buf;
            bufmax   = US3_RX_BUF_MAX;
            rxinptr  = US3_RX_BUF_MAX - (AT91C_BASE_US3->US_RCR);
            rxoutptr = &US3RxOutPtr;
            break;

        case USDBGU:
            pbuf     = USDBGU_rx_buf;
            bufmax   = USDBGU_RX_BUF_MAX;
            rxinptr  = USDBGU_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);
            rxoutptr = &USDBGURxOutPtr;
            break;

       default: return PARAMETER_ERR;
    }
    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//确定当前接收环形BUF的接收指针
    //ptrtemp = RXOutPtr;

    // 判断有没有收到一帧
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    while(1)
    {
        if(ptrtemp == rxinptr)// 没有收到一帧
            break;
        ptrtemp++;
        recvcount++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
    }
    

    //收到一帧，先确认帧BUF是否够用
    if(recvcount > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    // 把一帧的数据COPY到帧BUF里
    for(i = 0; i < recvcount; i ++)
    {
        *(pframe + i) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // 清空BUF其它空间
    for(i = recvcount; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }

    *recv_bytes = recvcount;
    // 保存读取指针

    return RECV_OK;

}

/*********************************************************************************
                            UsartGetFrame_by_1BytesEnd

function: 从PDC 的 RX_BUF 里取出一帧的数据存到FRAME BUFF里，如果没有完整的一帧，则返回错误
            帧以一个字节为结束符

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            frame_end_char, Frame 结束字符，1 byte
            *pframe, frame buff 的起始地址
            frame_buf_size， frame buff 的大小
            *recv_bytes， 收到的一帧大小，返回到改指针指向的地址里

return: RECV_ERR， 没有收到一帧，帧的数据不拷贝到BUF里
        RECV_OK， 收到了一帧，把帧的数据拷贝到BUF里
        PARAMETER_ERR, 指针为空或者串口名称不对
        RECV_FRAME_BUF_FULL, FRAME BUF大小不够存储一帧的数据，不拷贝帧数据

*********************************************************************************/
INT32U UsartGetFrame_by_1BytesEnd(INT32U usart, INT8U frame_end_char, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes)
{
    INT32U i, temp, recvcount;
    INT32U ptrtemp, bufmax, rxinptr;
    INT8U  *pbuf;
    INT32U *rxoutptr;

    if( (pframe == NULL) || (recv_bytes == NULL) )
        return PARAMETER_ERR;

    temp      = 0;
    recvcount = 0;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_rx_buf;
            bufmax   = US0_RX_BUF_MAX;
            rxinptr  = US0_RX_BUF_MAX - (AT91C_BASE_US0->US_RCR);
            rxoutptr = &US0RxOutPtr;
            break;

        case USART1:
            pbuf     = US1_rx_buf;
            bufmax   = US1_RX_BUF_MAX;
            rxinptr  = US1_RX_BUF_MAX - (AT91C_BASE_US1->US_RCR);
            rxoutptr = &US1RxOutPtr;
            break;

        case USART2:
            pbuf     = US2_rx_buf;
            bufmax   = US2_RX_BUF_MAX;
            rxinptr  = US2_RX_BUF_MAX - (AT91C_BASE_US2->US_RCR);
            rxoutptr = &US2RxOutPtr;
            break;

        case USART3:
            pbuf     = US3_rx_buf;
            bufmax   = US3_RX_BUF_MAX;
            rxinptr  = US3_RX_BUF_MAX - (AT91C_BASE_US3->US_RCR);
            rxoutptr = &US3RxOutPtr;
            break;

        case USDBGU:
            pbuf     = USDBGU_rx_buf;
            bufmax   = USDBGU_RX_BUF_MAX;
            rxinptr  = USDBGU_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);
            rxoutptr = &USDBGURxOutPtr;
            break;

       default: return PARAMETER_ERR;
    }
    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//确定当前接收环形BUF的接收指针
    //ptrtemp = RXOutPtr;

    // 判断有没有收到一帧
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    do
    {
        if(ptrtemp == rxinptr)// 没有收到一帧
            return RECV_ERR;
        temp = pbuf[ptrtemp++];
        recvcount ++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
    }
    while(temp != frame_end_char);

    //收到一帧，先确认帧BUF是否够用
    if(recvcount > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    // 把一帧的数据COPY到帧BUF里
    for(i = 0; i < recvcount; i ++)
    {
        *(pframe + i) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // 清空BUF其它空间
    for(i = recvcount; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }

    *recv_bytes = recvcount;
    // 保存读取指针

    return RECV_OK;

}


/*********************************************************************************
                            UsartGetFrame_by_2BytesEnd

function: 从PDC 的 RX_BUF 里取出一帧的数据存到FRAME BUFF里，如果没有完整的一帧，则返回错误。
           帧以2个字符为结束符

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            frame_end_char1, Frame 结束的第一个字符
            frame_end_char1, Frame 结束的第二个字符
            *pframe, frame buff 的起始地址
            frame_buf_size， frame buff 的大小
            *recv_bytes， 收到的一帧大小，返回到改指针指向的地址里

return: RECV_ERR， 没有收到一帧，帧的数据不拷贝到BUF里
        RECV_OK， 收到了一帧，把帧的数据拷贝到BUF里
        PARAMETER_ERR, 指针为空或者串口名称不对
        RECV_FRAME_BUF_FULL, FRAME BUF大小不够存储一帧的数据，不拷贝帧数据

*********************************************************************************/
INT32U UsartGetFrame_by_2BytesEnd(INT32U usart,INT8U frame_end_char1, INT8U frame_end_char2, INT8U *pframe, INT32U frame_buf_size, INT32U *recv_bytes)
{
    INT32U i, temp, recvcount;
    INT32U ptrtemp, bufmax, rxinptr;
    INT8U  *pbuf;
    INT32U *rxoutptr;

    temp      = 0;
    recvcount = 0;

    if( (pframe == NULL) || (recv_bytes == NULL) )
        return PARAMETER_ERR;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_rx_buf;
            bufmax   = US0_RX_BUF_MAX;
            rxinptr  = US0_RX_BUF_MAX - (AT91C_BASE_US0->US_RCR);
            rxoutptr = &US0RxOutPtr;
            break;

        case USART1:
            pbuf     = US1_rx_buf;
            bufmax   = US1_RX_BUF_MAX;
            rxinptr  = US1_RX_BUF_MAX - (AT91C_BASE_US1->US_RCR);
            rxoutptr = &US1RxOutPtr;
            break;

        case USART2:
            pbuf     = US2_rx_buf;
            bufmax   = US2_RX_BUF_MAX;
            rxinptr  = US2_RX_BUF_MAX - (AT91C_BASE_US2->US_RCR);
            rxoutptr = &US2RxOutPtr;
            break;

        case USART3:
            pbuf     = US3_rx_buf;
            bufmax   = US3_RX_BUF_MAX;
            rxinptr  = US3_RX_BUF_MAX - (AT91C_BASE_US3->US_RCR);
            rxoutptr = &US3RxOutPtr;
            break;

        case USDBGU:
            pbuf     = USDBGU_rx_buf;
            bufmax   = USDBGU_RX_BUF_MAX;
            rxinptr  = USDBGU_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);
            rxoutptr = &USDBGURxOutPtr;
            break;

       default: return PARAMETER_ERR;
    }

    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//确定当前接收环形BUF的接收指针
    //ptrtemp = RXOutPtr; //接收BUF的取出指针

    // 判断有没有收到一帧
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    while(1)
    {
        if(ptrtemp == rxinptr)// 没有收到一帧
            return RECV_ERR;
        temp = pbuf[ptrtemp++];
        recvcount ++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
        if(temp == frame_end_char1)//收到第一个帧尾
        {
            if(ptrtemp == rxinptr)
                return RECV_ERR;
            temp = pbuf[ptrtemp++];
            recvcount ++;
            if(ptrtemp >= bufmax)
                ptrtemp = 0;
            if(temp == frame_end_char2)
            {
                break;
            }
            else 
            {   //指针退一个位置，从倒数第二个位置重新判断
                ptrtemp--;
                recvcount--;
            }
        }
    }

    //收到一帧，先确认帧BUF是否够用
    if(recvcount > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    // 把一帧的数据COPY到帧BUF里
    for(i = 0; i < recvcount; i ++)
    {
        *(pframe + i) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // 清空BUF其它空间
    for(i = recvcount; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }
    *recv_bytes = recvcount;
    return RECV_OK;
}


/*
********************************************************************************
                            UsartGetFrame_by_Len

function: 从PDC 的 RX_BUF 里取出一帧的数据存到FRAME BUFF里，如果没有完整的一帧，则返回错误
          帧以定长取出

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            frame_len, 取出多长的一帧
            *pframe, frame buff 的起始地址
            frame_buf_size， frame buff 的大小

return: RECV_ERR， 没有收到一帧，帧的数据不拷贝到BUF里
        RECV_OK， 收到了一帧，把帧的数据拷贝到BUF里
        PARAMETER_ERR, 指针为空或者串口名称不对
        RECV_FRAME_BUF_FULL, FRAME BUF大小不够存储一帧的数据，不拷贝帧数据

********************************************************************************
*/
INT32U UsartGetFrame_by_Len(INT32U usart,INT32U frame_len, INT8U *pframe, INT32U frame_buf_size)
{
    INT32U i, ptrtemp;
    INT32U bufmax, rxinptr;
    INT8U  *pbuf;
    INT32U *rxoutptr;

    if( pframe == NULL )
        return PARAMETER_ERR;

    if(frame_len > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_rx_buf;
            bufmax   = US0_RX_BUF_MAX;
            rxinptr  = US0_RX_BUF_MAX - (AT91C_BASE_US0->US_RCR);
            rxoutptr = &US0RxOutPtr;
            break;

        case USART1:
            pbuf     = US1_rx_buf;
            bufmax   = US1_RX_BUF_MAX;
            rxinptr  = US1_RX_BUF_MAX - (AT91C_BASE_US1->US_RCR);
            rxoutptr = &US1RxOutPtr;
            break;

        case USART2:
            pbuf     = US2_rx_buf;
            bufmax   = US2_RX_BUF_MAX;
            rxinptr  = US2_RX_BUF_MAX - (AT91C_BASE_US2->US_RCR);
            rxoutptr = &US2RxOutPtr;
            break;

        case USART3:
            pbuf     = US3_rx_buf;
            bufmax   = US3_RX_BUF_MAX;
            rxinptr  = US3_RX_BUF_MAX - (AT91C_BASE_US3->US_RCR);
            rxoutptr = &US3RxOutPtr;
            break;

        case USDBGU:
            pbuf     = USDBGU_rx_buf;
            bufmax   = USDBGU_RX_BUF_MAX;
            rxinptr  = USDBGU_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);
            rxoutptr = &USDBGURxOutPtr;
            break;

       default: return PARAMETER_ERR;
    }
    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//确定当前接收环形BUF的接收指针
    //ptrtemp = RXOutPtr; //接收BUF的取出指针

    //判断有没有收到定长的帧
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    for(i = 0; i < frame_len; i ++)
    {
        if(ptrtemp == rxinptr)
            return RECV_ERR;
        ptrtemp++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
    }

    //把frame_len字长的帧COPY到 frame buf 里
    for(i = 0; i < frame_len; i ++)
    {
        *(pframe + i ) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // 清空BUF其它空间
    for(i = frame_len; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }
    return RECV_OK;
}


/*
********************************************************************************
                            UsartPutChar

function: 发送字符函数，字符以8位二进制表示。发送完毕后，返回。
          把要发送的字符放到THR，等待发送完毕，返回。

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            c, 要发送的字符

return:     TRUE
            FALSE, 参数配置错误

********************************************************************************
*/
BOOL UsartPutChar(INT32U usart, INT8U c)
{
    AT91S_USART *us;
    if(usart == USDBGU)
    {
        // 确认THR 中已经没有要发送的数据
        while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x02) );

        //放入THR中
        AT91C_BASE_DBGU->DBGU_THR = c;

        // 发送使能
        AT91C_BASE_DBGU->DBGU_CR = ((unsigned int) 0x1 <<  6) ;// TXEN

        //等待发送完成
        while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x02) );

        return TRUE;
    }

    switch(usart)
    {
        case USART0:
            us       = AT91C_BASE_US0;
            break;

        case USART1:
            us       = AT91C_BASE_US1;
            break;

        case USART2:
            us       = AT91C_BASE_US2;
            break;

        case USART3:
            us       = AT91C_BASE_US3;
            break;

       default: return FALSE;
    }

    // 确认THR 中已经没有要发送的数据
    while( !((us->US_CSR) & 0x02) );

    //放入THR中
    us->US_THR = c;

    // 发送使能
    us->US_CR = ((unsigned int) 0x1 <<  6) ;// TXEN

    //等待发送完成
    while( !((us->US_CSR) & 0x02) );

    return TRUE;
}


/*
********************************************************************************
                            UsartPutStr

function: 发送字符串函数，字符串长度不限制。发送完毕后，返回。
           把要发送的字符串COPY到TX_BUF里，然后启动PDC，等待发送完毕
           BUF 大小可以小于字符串长度，不过为了发送效率，建议BUF大小大于字符串长度

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            pstr, 要发送的字符串的首位地址，必须是以0x00结尾的字符串

return: TRUE
        FALSE, 串口名称不对

********************************************************************************
*/
BOOL UsartPutStr(INT32U usart, INT8U *pstr)
{
    INT32U      i = 0,  OverFlag = 0;
    INT32U      bufmax;
    INT8U       *pbuf;
    AT91S_USART *us;
    //选择串口
    switch(usart)
    {
        case USART0:
            pbuf     = US0_tx_buf;
            bufmax   = US0_TX_BUF_MAX;
            us       = AT91C_BASE_US0;
            break;

        case USART1:
            pbuf     = US1_tx_buf;
            bufmax   = US1_TX_BUF_MAX;
            us       = AT91C_BASE_US1;
            break;

        case USART2:
            pbuf     = US2_tx_buf;
            bufmax   = US2_TX_BUF_MAX;
            us       = AT91C_BASE_US2;
            break;

        case USART3:
            pbuf     = US3_tx_buf;
            bufmax   = US3_TX_BUF_MAX;
            us       = AT91C_BASE_US3;
            break;

        case USDBGU:
            pbuf     = USDBGU_tx_buf;
            bufmax   = USDBGU_TX_BUF_MAX;
            break;

       default: return FALSE;
    }
    // 发送字符串
    do
    {

        while(*pstr) // 读出字符串的值，并COPY 到TX_BUF里
        {
            pbuf[i] = *pstr;
            pstr++;
            i++;
            if(i >= bufmax) // TX_BUF 已满，从头COPY
            {
                i = 0;
                break;
            }
        }
        // 字符串COPY 完成或者发送BUF已满，开始发送

         // 初始化PDC
        if(usart == USDBGU)
        {
            AT91C_BASE_DBGU->DBGU_TPR  = (INT32U) pbuf;
            if(i == 0)
                AT91C_BASE_DBGU->DBGU_TCR  = bufmax;
            else
                AT91C_BASE_DBGU->DBGU_TCR  = i;

            //使能PDC tx
            AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_TXTEN;

            //等待发送完成
            while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x10) );
        }
        else
        {
            us->US_TPR  = (INT32U) pbuf;
            if(i == 0)
                us->US_TCR  = bufmax;
            else
                us->US_TCR  = i;

            //使能PDC tx
            us->US_PTCR = AT91C_PDC_TXTEN;

            //等待发送完成
            while( !((us->US_CSR) & 0x10) );
        }

        if(*pstr == 0x00)
            OverFlag = 1;
    }
    while(OverFlag == 0);

    return TRUE;
}


/*********************************************************************************
                            UsartPutFrame

function: 发送固定长的帧函数。发送完毕后返回。
          把要发送的帧COPY到TX_BUF里，然后启动PDC，等待发送完毕，返回。

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            pstr, 要发送的首地址
            length, 发送的长度

return:    TRUE, 发送成功
           FALSE, 参数配置错误

*********************************************************************************/

BOOL UsartPutFrame(INT32U usart, INT8U *pstr, INT32U length)
{
    INT32U 	    i ,OverFlag;
    INT32U      bufmax;
    INT8U       *pbuf, *pstr_start;
    AT91S_USART *us;

    OverFlag = 0;
    pstr_start = pstr;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_tx_buf;
            bufmax   = US0_TX_BUF_MAX;
            us       = AT91C_BASE_US0;
            break;

        case USART1:
            pbuf     = US1_tx_buf;
            bufmax   = US1_TX_BUF_MAX;
            us       = AT91C_BASE_US1;
            break;

        case USART2:
            pbuf     = US2_tx_buf;
            bufmax   = US2_TX_BUF_MAX;
            us       = AT91C_BASE_US2;
            break;

        case USART3:
            pbuf     = US3_tx_buf;
            bufmax   = US3_TX_BUF_MAX;
            us       = AT91C_BASE_US3;
            break;

        case USDBGU:
            pbuf     = USDBGU_tx_buf;
            bufmax   = USDBGU_TX_BUF_MAX;
            break;

       default: return FALSE;
    }
    // 配置PDC，开始发送
    do
    {
        i = 0;
        do
        {
            pbuf[i++] = *pstr++;
            
            if(pstr >= (pstr_start + length))
              OverFlag = 1;
        }
        while((i < bufmax) && (OverFlag == 0));
        // 字符串COPY 完成或者发送BUF已满，开始发送

        // 初始化PDC
        if(usart == USDBGU)
        {
            AT91C_BASE_DBGU->DBGU_TPR  = (INT32U) pbuf;
            //if(j == 0)
               // AT91C_BASE_DBGU->DBGU_TCR  = bufmax;
            //else
                AT91C_BASE_DBGU->DBGU_TCR  = i;

            //使能PDC tx
            AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_TXTEN;

            //等待发送完成
            while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x10) );
        }
        else
        {
            us->US_TPR  = (INT32U) pbuf;
           // if(i == 0)
             //   us->US_TCR  = bufmax;
           // else
                us->US_TCR  = i;

            //使能PDC tx
            us->US_PTCR = AT91C_PDC_TXTEN;

            //等待发送完成
            while( !((us->US_CSR) & 0x10) );
        }
       // if(i >= length)
           // OverFlag = 1;
    }
    while(OverFlag == 0);
    return TRUE;
}





/*
********************************************************************************
                            UsartSendFrameStart

function: 发送固定长的帧函数, 把帧数据全部放入PDC BUF中，启动PDC即返回。
          不等待发送结束。如果需要发送状况，调用CommSendFrameCallback 函数

parameters:usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
           pstr, 要发送的首地址
           length, 发送的长度

return:   TRUE， 发送成功
          FALSE， 参数配置错误或则BUF 大小不够
********************************************************************************
*/
BOOL UsartSendFrameStart(INT32U usart, INT8U *pstr, INT32U length)
{
    INT32U i;
    INT32U      bufmax;
    INT8U       *pbuf;
    AT91S_USART *us;

    switch(usart)
    {
        case USART0:
            pbuf     = US0_tx_buf;
            bufmax   = US0_TX_BUF_MAX;
            us       = AT91C_BASE_US0;
            break;

        case USART1:
            pbuf     = US1_tx_buf;
            bufmax   = US1_TX_BUF_MAX;
            us       = AT91C_BASE_US1;
            break;

        case USART2:
            pbuf     = US2_tx_buf;
            bufmax   = US2_TX_BUF_MAX;
            us       = AT91C_BASE_US2;
            break;

        case USART3:
            pbuf     = US3_tx_buf;
            bufmax   = US3_TX_BUF_MAX;
            us       = AT91C_BASE_US3;
            break;

        case USDBGU:
            pbuf     = USDBGU_tx_buf;
            bufmax   = USDBGU_TX_BUF_MAX;
            break;

       default: return FALSE;
    }

    //首先判断RX BUF 能否放下
    if(length > bufmax)
        return FALSE;

    //把要发送的数据COPY到TX BUF里
    for(i = 0; i < length; i++)
    {
        pbuf[i] = *pstr++;
    }

    if(usart == USDBGU)
    {
        // 初始化PDC
        AT91C_BASE_DBGU->DBGU_TPR  = (INT32U) pbuf;
        AT91C_BASE_DBGU->DBGU_TCR  = length;

        //使能PDC tx
        AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_TXTEN;
    }
    else
    {
        // 初始化PDC
        us->US_TPR  = (INT32U) pbuf;
        us->US_TCR  = length;

        //使能PDC tx
        us->US_PTCR = AT91C_PDC_TXTEN;
    }

    return TRUE;
}


/*
********************************************************************************
                            UsartSendFrameCallback

function: 检查要发送的帧是否成功发送。与CommSendFrameStart配套使用

parameters: usart, 串口通道,USART0,USART1,USART2,USART3,USDBGU
            *unsendcount, 返回还有多少字符未发送

return: PDC_TX_DISABLE, // PDC 发送未使能
        PDC_TX_END // PDC 发送完毕
        PDC_TX_NO_END // PDC 未发送完
        PARAMETER_ERR// 串口配置错误

********************************************************************************
*/
INT32U UsartSendFrameCallback(INT32U usart, INT32U *unsendcount)
{
    AT91S_USART *us;

    if(usart == USDBGU)
    {
        //首先判断是否使能了PDC发送
        if( !((AT91C_BASE_DBGU->DBGU_PTSR) & 0x100) ) // TXEN 未使能
            return PDC_TX_DISABLE;

        if( (AT91C_BASE_DBGU->DBGU_CSR)& 0x10 )
        {
            *unsendcount = 0;
            return PDC_TX_END;
        }
        else
        {
            *unsendcount = AT91C_BASE_DBGU->DBGU_TCR;
            return PDC_TX_NO_END;
        }
    }
    else
    {
        switch(usart)
        {
            case USART0:
                us       = AT91C_BASE_US0;
                break;

            case USART1:
                us       = AT91C_BASE_US1;
                break;

            case USART2:
                us       = AT91C_BASE_US2;
                break;

            case USART3:
                us       = AT91C_BASE_US3;
                break;

            default: return PARAMETER_ERR;
        }

        //首先判断是否使能了PDC发送
        if( !((us->US_PTSR) & 0x100) ) // TXEN 未使能
            return PDC_TX_DISABLE;

        if( (us->US_CSR)& 0x10 )
        {
            *unsendcount = 0;
            return PDC_TX_END;
        }
        else
        {
            *unsendcount = us->US_TCR;
            return PDC_TX_NO_END;
        }
    }
}

void US0_ISR_Handler() // US0 中断处理
{
    if ( (AT91C_BASE_US0->US_CSR) & 0x08) // ENDRX 中断
    {
         // 初始化PDC
        AT91C_BASE_US0->US_RPR = (INT32U) US0_rx_buf;
        AT91C_BASE_US0->US_RCR = US0_RX_BUF_MAX;
    }
}


void US1_ISR_Handler() // US1 中断处理
{
    if ( (AT91C_BASE_US1->US_CSR) & 0x08) // ENDRX 中断
    {
         // 初始化PDC
        AT91C_BASE_US1->US_RPR = (INT32U) US1_rx_buf;
        AT91C_BASE_US1->US_RCR = US1_RX_BUF_MAX;
    }
}


void US2_ISR_Handler() // US2 中断处理
{
    if ( (AT91C_BASE_US2->US_CSR) & 0x08) // ENDRX 中断
    {
         // 初始化PDC
        AT91C_BASE_US2->US_RPR = (INT32U) US2_rx_buf;
        AT91C_BASE_US2->US_RCR = US2_RX_BUF_MAX;
    }
}


void US3_ISR_Handler() // US3 中断处理
{
    if ( (AT91C_BASE_US3->US_CSR) & 0x08) // ENDRX 中断
    {
         // 初始化PDC
        AT91C_BASE_US3->US_RPR = (INT32U) US3_rx_buf;
        AT91C_BASE_US3->US_RCR = US3_RX_BUF_MAX;
    }
}


void USDBGU_ISR_Handler() // USDBGU 中断处理
{
    if ( (AT91C_BASE_DBGU->DBGU_CSR) & 0x08) // ENDRX 中断
    {
         // 初始化PDC
        AT91C_BASE_DBGU->DBGU_RPR = (INT32U) USDBGU_rx_buf;
        AT91C_BASE_DBGU->DBGU_RCR = USDBGU_RX_BUF_MAX;
    }
}

INT32U UART_WriteStr( unsigned char * ptrChar )
{
    int TxBufferCount = strlen((char const*)ptrChar);
    
    while(TxBufferCount --)
    {
        UsartPutChar(USDBGU, *ptrChar ++);
    }
    
    return 0;
}


