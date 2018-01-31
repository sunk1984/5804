#include "includes.h"

#define UPDATE_CFGFILE  //zjm
//#define SELECTED_FILES  //zjm


/******************************************************************************
    Routine Name    : Get_A_String
    Form            : static U8 Get_A_String(U8 * line, U8 * str)
    Parameters      : line, str
    Return value    : The length of the string. 
    Description     : Take a string from a line by comma or LF char, return the length of the string.
******************************************************************************/
static U8 Get_A_String(U8 * line, U8 * str)
{
	U8 i;

    memset(str, 0, CH_PERSTR_MAX);  // Clear the string.
    
	for(i=0;i<CH_PERSTR_MAX;i++)
	{
		if(line[i] == ',' || line[i] == '\r')  // Look for a comma or a LF char.
		{
			strncpy((char * )str, (char * )line, i);	//Got a string.
			break;
		}
	}
	return(i + 1);
}

/******************************************************************************
    Routine Name    : ProcItem
    Form            : static BOOL ProcItem(P_ITEM_T pItem)
    Parameters      : pItem
    Return value    : TRUE/FALSE
    Description     : Process the item struct, Compare the testing ID with the ID table, Run the relative function.
******************************************************************************/
static BOOL ProcItem(P_ITEM_T pItem)
{
	U8 i;
	BOOL ret = FALSE;
	U8 id_sum;
	U8 app_id_sum;
	
	id_sum = Get_IdSum();    // Get the sum of testing ID from the TestIdTable. 

	for(i=0; i<id_sum; i++)
	{
		if(strcmp(TestIdTab[i].TestIdStr, (char *)pItem->id) == 0)    // Get the sum of testing ID from the TestIdTable. 
		{
		    LCD_DisplayAItem(pItem->item);   // Display the serial number and the name on LCD. 

			(TestIdTab[i].TestFunc)(pItem);     // Testing. 

			LCD_DisplayResult(pItem->retResult);   // Display the result on LCD. 

            ret = pItem->retResult;
            
			break;
		}
	}

    if(i != id_sum)
    {
	    return(ret);
    }
    
	app_id_sum = Get_App_IdSum();    // Get the sum of testing ID from the TestIdTable. 

	for(i=0; i<app_id_sum; i++)
	{
		if(strcmp(TestAppIdTab[i].TestIdStr, (char *)pItem->id) == 0)    // Get the sum of testing ID from the TestIdTable. 
		{
		    LCD_DisplayAItem(pItem->item);   // Display the serial number and the name on LCD. 

			(TestAppIdTab[i].TestFunc)(pItem);     // Testing. 

			LCD_DisplayResult(pItem->retResult);   // Display the result on LCD. 

            ret = pItem->retResult;
            
			break;
		}
	}
	
	return(ret);
}

/******************************************************************************
    Routine Name    : ProcLine
    Form            : static BOOL ProcLine(U8 * line)
    Parameters      : line
    Return value    : TRUE/FALSE
    Description     : Process the line taken from file, Get a item from the line, then process the item.
******************************************************************************/
static BOOL ProcLine(U8 * line)
{
	BOOL ret = FALSE;
    U8 pos = 0;
    U8 str[CH_PERSTR_MAX];

    ITEM_T testItem;
	P_ITEM_T pItem = &testItem;

    memset(pItem, 0, sizeof(ITEM_T));   //Clear item.

	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
	    strcpy((char * )pItem->item, (char * )str);	        // Get the serial number and name.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
	    strcpy((char * )pItem->TestCmd, (char * )str);	    // Get the command.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
	    strcpy((char * )pItem->RspCmdPass, (char * )str);	// Get the response case pass.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX) 	//Get a string. 
	{
	    strcpy((char * )pItem->RspCmdFail, (char * )str);	// Get the response case fail.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX) 	//Get a string. 
	{
	    pItem->lower = (U32)(atof((char * )str) * 1000);	// Get the lower limit.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX) 	//Get a string. 
	{
	    pItem->upper = (U32)(atof((char * )str) * 1000);	// Get the upper limit.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX)	//Get a string. 
	{
    	strcpy((char * )pItem->id, (char * )str);	    // Get the ID.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX)	//Get a string.  
	{
    	strcpy((char * )pItem->lcdPrt, (char * )str);	// Get the display string.
	    line += pos;
	}
	if((pos = Get_A_String(line, str)) < CH_PERSTR_MAX)	//Get a string.  
	{
    	pItem->Channel = (U8)strtod((char * )str, NULL);	// Get the channel on IO/RLY board.
	    line += pos;
	}
	if(Get_A_String(line, str))	                        //Get a string. 
	{
    	pItem->Param = (U8)strtod((char * )str, NULL);	// Get the parameter.
	}

    ret = ProcItem(pItem);
	
	return(ret);
}


char * Get_AB_Switch(void)
{
    if(HMI_PressFuncKey())   //The switch is in position A.
    {
        return(CFG_FILE);
    }
    else
    {
        return(CFG_FILE_B);
    }
}


/******************************************************************************
    Routine Name    : CFGFILE_Proc
    Form            : void CFGFILE_Proc(void)
    Parameters      : none
    Return value    : none
    Description     : Read the config file.
******************************************************************************/
void CFGFILE_Proc(void)
{
	U8 * line;

	FS_FILE *fb;
    static U8 * ptr;

#ifdef UPDATE_CFGFILE
#ifdef SELECTED_FILES
	if(fb = FS_FOpen(Get_AB_Switch(),"w")) // New a file.
#else
	if(fb = FS_FOpen(CFG_FILE,"w")) // New a file.
#endif
	{
	    FS_FWrite(TestItemArray, 1, strlen((char * )TestItemArray), fb);
	    FS_SetEndOfFile(fb);
        FS_FClose(fb);
    }
#endif

#ifdef SELECTED_FILES
	if(fb = FS_FOpen(Get_AB_Switch(),"r")) // Read the file.
#else
	if(fb = FS_FOpen(CFG_FILE,"r")) // Read the file.
#endif
	{
        memset(TestItemArray, 0, CH_PERCFG_MAX);  // Clear the CFG file.
		FS_FRead(TestItemArray,1,CH_PERCFG_MAX,fb);
		FS_FClose(fb);
	}
	
    ptr = TestItemArray;

	while((line = (U8 * )strtok((char * )ptr, "\n")) != NULL)
	{
	    ptr += strlen((char * )line) + 1;

	    //line",,,,,,,,," and line "//" and  line "ITEM"(menu) wil be do not care.
    	if(strlen((char * )line) > strlen(",,,,,,,,,\r") && (* line) != '/' && (* line) != 'I')
    	{
    		if(ProcLine(line) == FALSE)	//Process this line.
    		{
    			break;
    		}
    	}
    }

    PWR_TurnOffDut();
    
    LOGFILE_Write();
    
    if(line == NULL)    //testing pass.
    {
        HMI_ShowPass();
    }
    else
    {
        HMI_ShowFail();
    }
}


