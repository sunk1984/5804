
#ifndef _LCD_H_
#define _LCD_H_

#include "includes.h"

#define LCD_ALL_LINE	(0)

#define LCD_LINE1		(1)
#define LCD_LINE2		(2)
#define LCD_LINE3		(3)
#define LCD_LINE4		(4)

extern INT32U Dprintf(char *lpszFormat, ...);

extern void LCD_DisplayALine(U8 line, U8 * str);
extern void LCD_Display2Line(U8 startline, U8 * str);
extern void LCD_Clear(U8 line);
extern void LCD_DisplayAItem(U8 * str);
extern void LCD_DisplayResult(U32 res);

#endif

