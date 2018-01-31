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

    //�Բ��������ж�
    if (usart.usartport == 4)  // DBGU ���ò���У��
    {
        if( (usart.usartmode != 0) | (usart.databit != 3) | (usart.parity > 7)
             | (usart.stopbit != 0) | (usart.baudrate > 115200) )

            return (FALSE);
    }
    else                      //USART0~3 ���ò���У��
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

            if (usart.usartmode)  //RS485 ģʽ�����ʼ��RTS
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

            if (usart.usartmode)  //RS485 ģʽ�����ʼ��RTS
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

            if (usart.usartmode)  //RS485 ģʽ�����ʼ��RTS
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

            if (usart.usartmode)  //RS485 ģʽ�����ʼ��RTS
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
             //  AT91C_BASE_PMC->PMC_PCER   = 1 << AT91C_ID_SYS; //ϵͳĬ��Ϊ��

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
         //�ر�PDC
        AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_RXTDIS;

        // ��ʼ��PDC
        AT91C_BASE_DBGU->DBGU_RPR = (INT32U) USDBGU_rx_buf;
        AT91C_BASE_DBGU->DBGU_RCR = USDBGU_RX_BUF_MAX;

        // ʹ���ж�
        AT91C_BASE_DBGU->DBGU_IER = ((unsigned int) 0x1 <<  3);// ENDRX

        //ʹ��PDC
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
    //�ر�PDC
    us->US_PTCR = AT91C_PDC_RXTDIS;

    // ��ʼ��PDC
    us->US_RPR = (INT32U)pbuf;
    us->US_RCR = bufmax;

    // ʹ���ж�
    us->US_IER = ((unsigned int) 0x1 <<  3);// ENDRX

    //ʹ��PDC
    us->US_PTCR =  AT91C_PDC_RXTEN;

    //�����ж�
    return TRUE;
}

/*
********************************************************************************
                            UsartRecvReset

function: ��ʼ��PDC�Ĵ������ͽ���ָ�룬��ͷ��ʼ���պͶ�ȡ

parameters:usart�� USART0,USART1,USART2,USART3,USDBGU

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

function: ��PDC �� RX_BUF ��ȡ��һ֡�����ݴ浽FRAME BUFF����û��������һ֡���򷵻ش���
            ֡�Ľ�����Ϊ 1byte

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            *recv_char, ���յ����ַ��浽��ָ��ָ��ĵ�ַ��

return: RECV_ERR�� û���յ�
        RECV_OK�� �յ�
        PARAMETER_ERR, ָ��Ϊ�ջ��ߴ������Ʋ���

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

    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//ȷ����ǰ���ջ���BUF�Ľ���ָ��
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

function: ��PDC �� RX_BUF ��ȡ��һ֡�����ݴ浽FRAME BUFF����û��������һ֡���򷵻ش���
            ֡�]�н�����

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            frame_end_char, Frame �����ַ���1 byte
            *pframe, frame buff ����ʼ��ַ
            frame_buf_size�� frame buff �Ĵ�С
            *recv_bytes�� �յ���һ֡��С�����ص���ָ��ָ��ĵ�ַ��

return: RECV_ERR�� û���յ�һ֡��֡�����ݲ�������BUF��
        RECV_OK�� �յ���һ֡����֡�����ݿ�����BUF��
        PARAMETER_ERR, ָ��Ϊ�ջ��ߴ������Ʋ���
        RECV_FRAME_BUF_FULL, FRAME BUF��С�����洢һ֡�����ݣ�������֡����

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
    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//ȷ����ǰ���ջ���BUF�Ľ���ָ��
    //ptrtemp = RXOutPtr;

    // �ж���û���յ�һ֡
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    while(1)
    {
        if(ptrtemp == rxinptr)// û���յ�һ֡
            break;
        ptrtemp++;
        recvcount++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
    }
    

    //�յ�һ֡����ȷ��֡BUF�Ƿ���
    if(recvcount > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    // ��һ֡������COPY��֡BUF��
    for(i = 0; i < recvcount; i ++)
    {
        *(pframe + i) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // ���BUF�����ռ�
    for(i = recvcount; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }

    *recv_bytes = recvcount;
    // �����ȡָ��

    return RECV_OK;

}

/*********************************************************************************
                            UsartGetFrame_by_1BytesEnd

function: ��PDC �� RX_BUF ��ȡ��һ֡�����ݴ浽FRAME BUFF����û��������һ֡���򷵻ش���
            ֡��һ���ֽ�Ϊ������

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            frame_end_char, Frame �����ַ���1 byte
            *pframe, frame buff ����ʼ��ַ
            frame_buf_size�� frame buff �Ĵ�С
            *recv_bytes�� �յ���һ֡��С�����ص���ָ��ָ��ĵ�ַ��

return: RECV_ERR�� û���յ�һ֡��֡�����ݲ�������BUF��
        RECV_OK�� �յ���һ֡����֡�����ݿ�����BUF��
        PARAMETER_ERR, ָ��Ϊ�ջ��ߴ������Ʋ���
        RECV_FRAME_BUF_FULL, FRAME BUF��С�����洢һ֡�����ݣ�������֡����

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
    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//ȷ����ǰ���ջ���BUF�Ľ���ָ��
    //ptrtemp = RXOutPtr;

    // �ж���û���յ�һ֡
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    do
    {
        if(ptrtemp == rxinptr)// û���յ�һ֡
            return RECV_ERR;
        temp = pbuf[ptrtemp++];
        recvcount ++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
    }
    while(temp != frame_end_char);

    //�յ�һ֡����ȷ��֡BUF�Ƿ���
    if(recvcount > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    // ��һ֡������COPY��֡BUF��
    for(i = 0; i < recvcount; i ++)
    {
        *(pframe + i) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // ���BUF�����ռ�
    for(i = recvcount; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }

    *recv_bytes = recvcount;
    // �����ȡָ��

    return RECV_OK;

}


/*********************************************************************************
                            UsartGetFrame_by_2BytesEnd

function: ��PDC �� RX_BUF ��ȡ��һ֡�����ݴ浽FRAME BUFF����û��������һ֡���򷵻ش���
           ֡��2���ַ�Ϊ������

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            frame_end_char1, Frame �����ĵ�һ���ַ�
            frame_end_char1, Frame �����ĵڶ����ַ�
            *pframe, frame buff ����ʼ��ַ
            frame_buf_size�� frame buff �Ĵ�С
            *recv_bytes�� �յ���һ֡��С�����ص���ָ��ָ��ĵ�ַ��

return: RECV_ERR�� û���յ�һ֡��֡�����ݲ�������BUF��
        RECV_OK�� �յ���һ֡����֡�����ݿ�����BUF��
        PARAMETER_ERR, ָ��Ϊ�ջ��ߴ������Ʋ���
        RECV_FRAME_BUF_FULL, FRAME BUF��С�����洢һ֡�����ݣ�������֡����

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

    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//ȷ����ǰ���ջ���BUF�Ľ���ָ��
    //ptrtemp = RXOutPtr; //����BUF��ȡ��ָ��

    // �ж���û���յ�һ֡
    ptrtemp = *rxoutptr;
    if(rxinptr >= bufmax)
        rxinptr = 0;
    while(1)
    {
        if(ptrtemp == rxinptr)// û���յ�һ֡
            return RECV_ERR;
        temp = pbuf[ptrtemp++];
        recvcount ++;
        if(ptrtemp >= bufmax)
            ptrtemp = 0;
        if(temp == frame_end_char1)//�յ���һ��֡β
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
            {   //ָ����һ��λ�ã��ӵ����ڶ���λ�������ж�
                ptrtemp--;
                recvcount--;
            }
        }
    }

    //�յ�һ֡����ȷ��֡BUF�Ƿ���
    if(recvcount > frame_buf_size)
        return RECV_FRAME_BUF_FULL;

    // ��һ֡������COPY��֡BUF��
    for(i = 0; i < recvcount; i ++)
    {
        *(pframe + i) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // ���BUF�����ռ�
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

function: ��PDC �� RX_BUF ��ȡ��һ֡�����ݴ浽FRAME BUFF����û��������һ֡���򷵻ش���
          ֡�Զ���ȡ��

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            frame_len, ȡ���೤��һ֡
            *pframe, frame buff ����ʼ��ַ
            frame_buf_size�� frame buff �Ĵ�С

return: RECV_ERR�� û���յ�һ֡��֡�����ݲ�������BUF��
        RECV_OK�� �յ���һ֡����֡�����ݿ�����BUF��
        PARAMETER_ERR, ָ��Ϊ�ջ��ߴ������Ʋ���
        RECV_FRAME_BUF_FULL, FRAME BUF��С�����洢һ֡�����ݣ�������֡����

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
    //RXInPtr = US0_RX_BUF_MAX - (AT91C_BASE_DBGU->DBGU_RCR);//ȷ����ǰ���ջ���BUF�Ľ���ָ��
    //ptrtemp = RXOutPtr; //����BUF��ȡ��ָ��

    //�ж���û���յ�������֡
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

    //��frame_len�ֳ���֡COPY�� frame buf ��
    for(i = 0; i < frame_len; i ++)
    {
        *(pframe + i ) = pbuf[(*rxoutptr)++];
        if(*rxoutptr >= bufmax)
            *rxoutptr = 0;
    }
    // ���BUF�����ռ�
    for(i = frame_len; i < frame_buf_size; i ++)
    {
         *(pframe + i) = 0;
    }
    return RECV_OK;
}


/*
********************************************************************************
                            UsartPutChar

function: �����ַ��������ַ���8λ�����Ʊ�ʾ��������Ϻ󣬷��ء�
          ��Ҫ���͵��ַ��ŵ�THR���ȴ�������ϣ����ء�

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            c, Ҫ���͵��ַ�

return:     TRUE
            FALSE, �������ô���

********************************************************************************
*/
BOOL UsartPutChar(INT32U usart, INT8U c)
{
    AT91S_USART *us;
    if(usart == USDBGU)
    {
        // ȷ��THR ���Ѿ�û��Ҫ���͵�����
        while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x02) );

        //����THR��
        AT91C_BASE_DBGU->DBGU_THR = c;

        // ����ʹ��
        AT91C_BASE_DBGU->DBGU_CR = ((unsigned int) 0x1 <<  6) ;// TXEN

        //�ȴ��������
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

    // ȷ��THR ���Ѿ�û��Ҫ���͵�����
    while( !((us->US_CSR) & 0x02) );

    //����THR��
    us->US_THR = c;

    // ����ʹ��
    us->US_CR = ((unsigned int) 0x1 <<  6) ;// TXEN

    //�ȴ��������
    while( !((us->US_CSR) & 0x02) );

    return TRUE;
}


/*
********************************************************************************
                            UsartPutStr

function: �����ַ����������ַ������Ȳ����ơ�������Ϻ󣬷��ء�
           ��Ҫ���͵��ַ���COPY��TX_BUF�Ȼ������PDC���ȴ��������
           BUF ��С����С���ַ������ȣ�����Ϊ�˷���Ч�ʣ�����BUF��С�����ַ�������

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            pstr, Ҫ���͵��ַ�������λ��ַ����������0x00��β���ַ���

return: TRUE
        FALSE, �������Ʋ���

********************************************************************************
*/
BOOL UsartPutStr(INT32U usart, INT8U *pstr)
{
    INT32U      i = 0,  OverFlag = 0;
    INT32U      bufmax;
    INT8U       *pbuf;
    AT91S_USART *us;
    //ѡ�񴮿�
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
    // �����ַ���
    do
    {

        while(*pstr) // �����ַ�����ֵ����COPY ��TX_BUF��
        {
            pbuf[i] = *pstr;
            pstr++;
            i++;
            if(i >= bufmax) // TX_BUF ��������ͷCOPY
            {
                i = 0;
                break;
            }
        }
        // �ַ���COPY ��ɻ��߷���BUF��������ʼ����

         // ��ʼ��PDC
        if(usart == USDBGU)
        {
            AT91C_BASE_DBGU->DBGU_TPR  = (INT32U) pbuf;
            if(i == 0)
                AT91C_BASE_DBGU->DBGU_TCR  = bufmax;
            else
                AT91C_BASE_DBGU->DBGU_TCR  = i;

            //ʹ��PDC tx
            AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_TXTEN;

            //�ȴ��������
            while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x10) );
        }
        else
        {
            us->US_TPR  = (INT32U) pbuf;
            if(i == 0)
                us->US_TCR  = bufmax;
            else
                us->US_TCR  = i;

            //ʹ��PDC tx
            us->US_PTCR = AT91C_PDC_TXTEN;

            //�ȴ��������
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

function: ���͹̶�����֡������������Ϻ󷵻ء�
          ��Ҫ���͵�֡COPY��TX_BUF�Ȼ������PDC���ȴ�������ϣ����ء�

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            pstr, Ҫ���͵��׵�ַ
            length, ���͵ĳ���

return:    TRUE, ���ͳɹ�
           FALSE, �������ô���

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
    // ����PDC����ʼ����
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
        // �ַ���COPY ��ɻ��߷���BUF��������ʼ����

        // ��ʼ��PDC
        if(usart == USDBGU)
        {
            AT91C_BASE_DBGU->DBGU_TPR  = (INT32U) pbuf;
            //if(j == 0)
               // AT91C_BASE_DBGU->DBGU_TCR  = bufmax;
            //else
                AT91C_BASE_DBGU->DBGU_TCR  = i;

            //ʹ��PDC tx
            AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_TXTEN;

            //�ȴ��������
            while( !((AT91C_BASE_DBGU->DBGU_CSR) & 0x10) );
        }
        else
        {
            us->US_TPR  = (INT32U) pbuf;
           // if(i == 0)
             //   us->US_TCR  = bufmax;
           // else
                us->US_TCR  = i;

            //ʹ��PDC tx
            us->US_PTCR = AT91C_PDC_TXTEN;

            //�ȴ��������
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

function: ���͹̶�����֡����, ��֡����ȫ������PDC BUF�У�����PDC�����ء�
          ���ȴ����ͽ����������Ҫ����״��������CommSendFrameCallback ����

parameters:usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
           pstr, Ҫ���͵��׵�ַ
           length, ���͵ĳ���

return:   TRUE�� ���ͳɹ�
          FALSE�� �������ô������BUF ��С����
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

    //�����ж�RX BUF �ܷ����
    if(length > bufmax)
        return FALSE;

    //��Ҫ���͵�����COPY��TX BUF��
    for(i = 0; i < length; i++)
    {
        pbuf[i] = *pstr++;
    }

    if(usart == USDBGU)
    {
        // ��ʼ��PDC
        AT91C_BASE_DBGU->DBGU_TPR  = (INT32U) pbuf;
        AT91C_BASE_DBGU->DBGU_TCR  = length;

        //ʹ��PDC tx
        AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_TXTEN;
    }
    else
    {
        // ��ʼ��PDC
        us->US_TPR  = (INT32U) pbuf;
        us->US_TCR  = length;

        //ʹ��PDC tx
        us->US_PTCR = AT91C_PDC_TXTEN;
    }

    return TRUE;
}


/*
********************************************************************************
                            UsartSendFrameCallback

function: ���Ҫ���͵�֡�Ƿ�ɹ����͡���CommSendFrameStart����ʹ��

parameters: usart, ����ͨ��,USART0,USART1,USART2,USART3,USDBGU
            *unsendcount, ���ػ��ж����ַ�δ����

return: PDC_TX_DISABLE, // PDC ����δʹ��
        PDC_TX_END // PDC �������
        PDC_TX_NO_END // PDC δ������
        PARAMETER_ERR// �������ô���

********************************************************************************
*/
INT32U UsartSendFrameCallback(INT32U usart, INT32U *unsendcount)
{
    AT91S_USART *us;

    if(usart == USDBGU)
    {
        //�����ж��Ƿ�ʹ����PDC����
        if( !((AT91C_BASE_DBGU->DBGU_PTSR) & 0x100) ) // TXEN δʹ��
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

        //�����ж��Ƿ�ʹ����PDC����
        if( !((us->US_PTSR) & 0x100) ) // TXEN δʹ��
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

void US0_ISR_Handler() // US0 �жϴ���
{
    if ( (AT91C_BASE_US0->US_CSR) & 0x08) // ENDRX �ж�
    {
         // ��ʼ��PDC
        AT91C_BASE_US0->US_RPR = (INT32U) US0_rx_buf;
        AT91C_BASE_US0->US_RCR = US0_RX_BUF_MAX;
    }
}


void US1_ISR_Handler() // US1 �жϴ���
{
    if ( (AT91C_BASE_US1->US_CSR) & 0x08) // ENDRX �ж�
    {
         // ��ʼ��PDC
        AT91C_BASE_US1->US_RPR = (INT32U) US1_rx_buf;
        AT91C_BASE_US1->US_RCR = US1_RX_BUF_MAX;
    }
}


void US2_ISR_Handler() // US2 �жϴ���
{
    if ( (AT91C_BASE_US2->US_CSR) & 0x08) // ENDRX �ж�
    {
         // ��ʼ��PDC
        AT91C_BASE_US2->US_RPR = (INT32U) US2_rx_buf;
        AT91C_BASE_US2->US_RCR = US2_RX_BUF_MAX;
    }
}


void US3_ISR_Handler() // US3 �жϴ���
{
    if ( (AT91C_BASE_US3->US_CSR) & 0x08) // ENDRX �ж�
    {
         // ��ʼ��PDC
        AT91C_BASE_US3->US_RPR = (INT32U) US3_rx_buf;
        AT91C_BASE_US3->US_RCR = US3_RX_BUF_MAX;
    }
}


void USDBGU_ISR_Handler() // USDBGU �жϴ���
{
    if ( (AT91C_BASE_DBGU->DBGU_CSR) & 0x08) // ENDRX �ж�
    {
         // ��ʼ��PDC
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


