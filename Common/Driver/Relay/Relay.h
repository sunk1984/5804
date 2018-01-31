#ifndef RELAY__
#define RELAY__

#include "includes.h"

extern BOOL RLY_ON(U32 TotalChan);
extern BOOL RLY_OFF(U32 TotalChan);
extern BOOL RLY_OffAll(U8 board_num);
extern BOOL RLY_SetAdMode(U8 board_num);
extern BOOL RLY_SetCommonMode(U8 board_num);

#endif
