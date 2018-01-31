
#ifndef INIT_FILE__
#define INIT_FILE__

//�������ڵĶ���
#define DBGU_COMM_PORT  USDBGU
#define MERAK_COMM_PORT USART0
#define DUT_COMM_PORT   USART1
#define AUX_COMM_PORT   USART2

#define MERAK_COMM_ID  AT91C_ID_US0
#define DUT_COMM_ID  AT91C_ID_US1
#define AUX_COMM_ID  AT91C_ID_US2

#define CH_INITSTR_MAX	(18)
#define CH_INITFILE_MAX	(500)

#define INIT_FILE	"Init.csv"

extern U8 InitArray[CH_INITFILE_MAX];

extern float  verify_coef_range_s;  // �޷�ѹ����ϵ��
extern float  verify_coef_range_m;  // 10:1 ��ѹ����ϵ��
extern float  verify_coef_range_l;  // 100:1 ��ѹ����ϵ��

extern void INITFILE_Proc(void);

extern void Volt_Calibration(void);


#endif
