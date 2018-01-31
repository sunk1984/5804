
#include "includes.h"

#include <stdarg.h>
#include "JLINKDCC.h"

#define LCDNUM_ONLY_1 	    1

#define LCDFUNC_WRITE_LCD 	0x61
#define LCDFUNC_CLEAR_LCD	0x62

#define LCD_PERLINE_MAX     24
#define LCD_LINE_MAX        2

const U8 ClearLineData[] = "00";

void LCD_DisplayALine(U8 line, U8 * str);
void LCD_Clear(U8 line);
void LCD_DisplayAItem(U8 * str);
void LCD_DisplayResult(U32 res);

INT32U Dprintf(char *lpszFormat, ...)
{
    char DisplayBuff[100] = {0};
    va_list fmtList;
    
// 1. 字符串合成
    va_start( fmtList, lpszFormat );
    vsprintf( DisplayBuff, lpszFormat, fmtList );
    va_end( fmtList );
  
// 2. 将合成后的字符串压入待发送缓冲区 
    JLINKDCC_SendString((const char * )DisplayBuff);
    UART_WriteStr( (UCHAR *)DisplayBuff ); 
    LOGFILE_AddItem( (UCHAR *)DisplayBuff );
    return 0;
}
    
static void LCD_Printf(U8 * str)
{
    U8 prtStr[50];
    
    sprintf((char *)prtStr, "%s\r\n", (char * )str);
	Dprintf((char * )prtStr);
}

    
static char* StrCpy(char* ds, char* ss)
{
	while(*ss)
		*ds++ = *ss++;
	return ds;
}

static void StrnCpy(char* ds, char* ss, int n)
{
    for(int i=0;i<n;i++)
    {
		*ds++ = *ss++;
	}
}

static BOOL LCD_WriteCmd(U8 func, U8 reg, U8 * WriteStr)
{
    return(MERAK_WriteCmd("LCD", LCDNUM_ONLY_1, func, reg, WriteStr));
}

void LCD_DisplayALine(U8 line, U8 * str)
{
    LCD_WriteCmd(LCDFUNC_WRITE_LCD, line, (U8 * )str);
	LCD_Printf(str);
}

void LCD_Display2Line(U8 startline, U8 * str)
{
    U8 i;
    U8 lcdStr1[LCD_PERLINE_MAX+1],lcdStr2[LCD_PERLINE_MAX+1]={0};
    
    StrnCpy((char * )lcdStr1, (char * )str, LCD_PERLINE_MAX);
    StrCpy((char * )lcdStr2, (char * )(str+LCD_PERLINE_MAX));
    
    lcdStr1[LCD_PERLINE_MAX] = 0;
    lcdStr2[LCD_PERLINE_MAX] = 0;
    
    LCD_DisplayALine(startline, lcdStr1);
    LCD_DisplayALine(startline+1, lcdStr2);
    
	LCD_Printf(lcdStr1);
	LCD_Printf(lcdStr2);
}

void LCD_Clear(U8 line)
{
    LCD_WriteCmd(LCDFUNC_CLEAR_LCD, line, (U8 * )"");
}

void LCD_DisplayAItem(U8 * str)
{
    if(* str == 0)
    {
        return;
    }
    LCD_Clear(LCD_ALL_LINE);
	LCD_DisplayALine(LCD_LINE1, str);
}

void LCD_DisplayResult(U32 res)
{
	if(res == PASS)
	{
		LCD_DisplayALine(LCD_LINE3, (U8 *)"PASS!");
	}
	else
	{
		LCD_DisplayALine(LCD_LINE3, (U8 *)"FAIL!");
	}
}


