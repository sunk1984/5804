
#ifndef CFG_FILE__
#define CFG_FILE__

#define CH_PERSTR_MAX	(50)
#define CH_PERCFG_MAX	(8192)

#define CFG_FILE	"TestList.csv"
#define CFG_FILE_B	"TestLisB.csv"

extern U8 TestItemArray[];
extern const TEST_ID TestAppIdTab[];

extern U8 Get_App_IdSum(void);

extern void CFGFILE_Proc(void);

#endif
