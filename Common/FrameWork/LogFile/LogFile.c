
#include "includes.h"

#define LOG_LEN_1_PCS           (15000)
#define LOG_FILE_CUT_MAX        (400000)
#define LOG_FILE_TOTAL_MAX      (1000000)

#define LOG_FILE	"TestLog.txt"

static U8 logStr[LOG_LEN_1_PCS] = {0};

void LOGFILE_AddItem(U8 * str)
{
	strcat((char * )logStr, (char * )str);
}

void LOGFILE_Write(void)
{
	I32 pos;
	FS_FILE *fb;

    static char fileStr[LOG_FILE_TOTAL_MAX];

	if(fb = FS_FOpen(LOG_FILE,"a+"))
	{
	    FS_FWrite(logStr, 1, strlen((char * )logStr), fb);

        FS_FSeek(fb, 0, FS_SEEK_END);
	    pos = FS_FTell(fb);
	    if(pos > LOG_FILE_TOTAL_MAX-LOG_LEN_1_PCS)
	    {
            FS_FSeek(fb, 0, FS_SEEK_SET);
            FS_FRead(fileStr,1,pos,fb);
            FS_FSeek(fb, 0, FS_SEEK_SET);
            FS_FWrite(fileStr+LOG_FILE_CUT_MAX, pos-LOG_FILE_CUT_MAX, 1, fb);
            FS_SetEndOfFile(fb);
	    }
        FS_FClose(fb);
    }
	
    memset((char * )logStr, 0, sizeof(logStr));
}



