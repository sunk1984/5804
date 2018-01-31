
#ifndef TEST_LIB_ITEM__
#define TEST_LIB_ITEM__

#define MCU_GPIO_OUT  	    AT91C_BASE_PIOC

#define DUMMY		        AT91C_PIO_PC17

#define MCU_POUT_P7_3		AT91C_PIO_PC24
#define MCU_POUT_P7_4		AT91C_PIO_PC25
#define MCU_POUT_P7_5		AT91C_PIO_PC26
#define MCU_POUT_P7_6		AT91C_PIO_PC27
#define MCU_POUT_P7_7	    AT91C_PIO_PC28
#define MCU_POUT_P7_8		AT91C_PIO_PC29
#define MCU_POUT_P7_9		AT91C_PIO_PC30

#define MCU_POUT_P4_5		AT91C_PIO_PB20
#define MCU_POUT_P4_6		AT91C_PIO_PB21
#define MCU_POUT_P4_7		AT91C_PIO_PA0
#define MCU_POUT_P4_8		AT91C_PIO_PA1
#define MCU_POUT_P4_9	    AT91C_PIO_PA2
#define MCU_POUT_P4_10		AT91C_PIO_PA3
#define MCU_POUT_P4_11		AT91C_PIO_PB0
#define MCU_POUT_P4_12		AT91C_PIO_PB1
#define MCU_POUT_P4_13		AT91C_PIO_PB2
#define MCU_POUT_P4_14		AT91C_PIO_PB3
#define MCU_POUT_P4_15		AT91C_PIO_PB16
#define MCU_POUT_P4_16	    AT91C_PIO_PB17
#define MCU_POUT_P4_17		AT91C_PIO_PB18
#define MCU_POUT_P4_18		AT91C_PIO_PB19

#define POUT_CH_MAX		    10

#define ITEM_STR_MAX	24
#define CMD_STR_MAX	    48
#define ID_STR_MAX	    8
#define LCD_LSTR_MAX	ITEM_STR_MAX+ITEM_STR_MAX


typedef struct
{
	U8 item[ITEM_STR_MAX+1];

	U8 TestCmd[CMD_STR_MAX+1];
	U8 RspCmdPass[CMD_STR_MAX+1];
	U8 RspCmdFail[CMD_STR_MAX+1];

	U32 lower;
	U32 upper;

	U8 id[ID_STR_MAX+1];
	U8 lcdPrt[LCD_LSTR_MAX+1];

	U8 Channel;
	U8 Param;

	U32 retResult;

} ITEM_T, * P_ITEM_T;

typedef void (*TEST_FUNC)(P_ITEM_T pitem);

typedef struct
{
	char * TestIdStr;
	TEST_FUNC TestFunc;

} TEST_ID, * P_TEST_ID;

extern U8 Get_IdSum(void);

extern const TEST_ID TestIdTab[];

extern U32 DUT_CMD(P_ITEM_T pitem);

#endif


