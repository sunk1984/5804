/*==========================================================================*\
|                                                                            |
|                                                                            |
| JTAG Replicator for the MSP430 Flash-based family devices                  |
|                                                                            |
| Key features:                                                              |
| · Supports JTAG communication to all MSP430 Flash Devices                  |
| · Max. code size of target program: 57kB                                   |
| · Programming speed: ~60kB/10sec (~6kB/sec)                                |
| · Fast Verification and Erase Check: ~60kB/350ms                           |
| · Supports Programming of JTAG Access Protection Fuse                      |
|                                                                            |
   Note: All High Level JTAG Functions are applied here.

   A number of lines have been commented out which are not strictly required
   to successfully program a flash-based MSP430 device using the Replicator
   concept. Please note that these lines have been commented out in order
   to provide reference examples for calling such functions.
   Uncommented lines below represent the minimum requirements to successfully
   program an MSP430 flash device using the Replicator.
   
    The basic routine consists of the following steps:
   
   1.  | Initialize the host MSP430 on the Replicator board
   2.  | Connect to the target device
   3.  | Control the target peripherals directly (optional)
   4.  | Perform a write + read + verify in the target RAM
   5.  | Operations in the device's main memory
   6.  | Blow JTAG access fuse (optional)
   7.  | Release the device from JTAG control and wait for the user to press button "S1"
*/

/****************************************************************************/
/* Includes                                                                 */
/****************************************************************************/

#include "JTAGfunc430.h"           // JTAG functions
#include "LowLevelFunc430.h"       // low level functions
#include "Devices430.h"            // holds Device specific information
     
#include "includes.h"

#define FILE_NAME_LENTH     5   //"2111S"
#define DIR_NAME_LENTH      8   //"\\DUT_FW\\"
#define FILE_EXT_LENTH      4   //".TXT"
#define FILE_FULL_NAME_MAX  18   //"\\DUT_FW\\2111S.TXT"

#define INDEX_FILE      "\\DUT_FW\\PREST_FW.txt"

#define MAX_EPROM_DATA_NUMBER  9000           //Êý¾Ý×î´óÊýÁ¿      
#define MAX_EPROM_SECTION_NUMBER   16           //Êý¾Ý¶ÎµÄ×î´ó¸öÊý
#define CH_FILE_MAX	            (65536)         //×î´ó64K

#define INDEX_FILE      "\\DUT_FW\\PREST_FW.txt"
#define FILE_NAME_MAX     20

U8 Programm_msp430(U8 app_file);
void Record_FileName(U8 * fname);

static unsigned int eprom_sections_number;
static int eprom_sections_length_number;
static unsigned int eprom_number;
static int total_short_length;   //Ò»¶ÎµÄÊý¾Ý³¤¶È£¨ÒÔ2×Ö½ÚÎªµ¥Î»£©

static unsigned short eprom[MAX_EPROM_DATA_NUMBER];  //´æ·ÅepromµÄÊý¾Ý
//! \brief Holds the destination adresses for different sections of the target code
static unsigned long eprom_address[MAX_EPROM_SECTION_NUMBER]; //´æ·Åeprom¸÷¶ÎµÄÊý¾ÝÊ×µØÖ·
//! \brief Holds the length of the different memory sections
static unsigned long eprom_length_of_sections[MAX_EPROM_SECTION_NUMBER];	//´æ·Åeprom¸÷¶ÎµÄÊý¾Ý³¤¶È
//! \brief Holds the number of memory sections
static unsigned long eprom_sections;	//eeprom µÄsectionÊýÄ¿

/****************************************************************************/
/* FUNCTIONS                                                                */
/****************************************************************************/
static void ProcTxtLine(U8 * line)
{
	if(*line == '@')    //Èç¹ûÐÐ¿ªÍ·ÊÇ@
	{
		line++;   //Ìø¹ý*ºÅ
		eprom_address[eprom_sections_number++] = (U32)strtol((char * )line, NULL, 16);  //´æÈësection µØÖ·
		eprom_length_of_sections[eprom_sections_length_number++] = total_short_length;//ÔÙ´ÎÅöµ½@¼ÆËãÉÏÒ»section³¤¶È
		eprom_sections ++;
		total_short_length = 0 ; //ÐÂµÄÒ»¶Î³¤¶È´Ó0¿ªÊ¼Ëã
	}
	else if(*line == 'q')   //Èô¹ûÐÐ¿ªÍ·ÊÇq
	{	
		eprom_length_of_sections[eprom_sections_length_number++] = total_short_length; //×îºóÒ»¶Î³¤¶È¼ÆÈë
	}
	else        //Èç¹ûÐÐ¿ªÍ·ÊÇ16½øÖÆÊý×Ö
	{
		U8 i;
        U8 MyLine[50];
		U8 strLength = strlen((char * )line);
		U8 hexLength = strLength/3;
		
		strcpy((char * )MyLine,(char * )line);
		
		if(hexLength % 2)
		{
		    MyLine[strLength-1] = 0;           //Delete the char '\r'
			sprintf((char * )MyLine, (char * )"%sFF ", (char * )line);    //Concatenate the strings "FF "
            hexLength++;
		}

		for(i = 0; i < hexLength; i += 2)
		{
			eprom[eprom_number++] = (((U16)strtol((char *)&MyLine[(i+1)*3], NULL, 16)) << 8)  //Ð¡¶Ë¸ñÊ½
			                        + (U16)strtol((char *)&MyLine[i*3], NULL, 16);
		}

		total_short_length += hexLength/2;		
	}
}

void Record_FileName(U8 * fname)
{
	FS_FILE *fb;
    U8 DUT_fileName[FILE_FULL_NAME_MAX];
    U8 LCD_ProductName[FILE_FULL_NAME_MAX];  // 201311
    
    strncpy((char *)DUT_fileName, (char *)"\\DUT_FW\\", DIR_NAME_LENTH);
    strncpy((char *)(DUT_fileName+DIR_NAME_LENTH), (char *)fname, FILE_NAME_LENTH);
    strcpy((char *)(DUT_fileName+DIR_NAME_LENTH+FILE_NAME_LENTH), (char *)".txt");
    
	if(fb = FS_FOpen(INDEX_FILE,"w")) // Write the file.
	{
		FS_FWrite(DUT_fileName,1,FILE_FULL_NAME_MAX,fb);
		FS_FClose(fb);
		Dprintf("Present DUT file is %s\r\n", DUT_fileName);
		
        strncpy((char *)LCD_ProductName, (char *)"PN:HRMS ", DIR_NAME_LENTH);  // 201311
        strncpy((char *)(LCD_ProductName+DIR_NAME_LENTH), (char *)fname, FILE_NAME_LENTH);  // 201311
	    HMI_PrintLcd(LCD_LINE2, LCD_ProductName);  // 201311
	    OS_Delay(1500);  // 201311
	}
}


///////////////////////////////////////////////////////////  // 201311
void Display_ProductName(void)  // 201311
{
	FS_FILE *fb;
    char FileName[FILE_NAME_MAX];  // 201311
    U8 LCD_ProductName[FILE_FULL_NAME_MAX];  // 201311

	if(fb = FS_FOpen(INDEX_FILE,"r")) // Read the file.
	{
		FS_FRead(FileName,1,FILE_NAME_MAX,fb);  // 201311
		FS_FClose(fb);
	}
	
    strncpy((char *)LCD_ProductName, (char *)"PN:HRMS ", DIR_NAME_LENTH);  // 201311
    strncpy((char *)(LCD_ProductName+DIR_NAME_LENTH), (char *)FileName+DIR_NAME_LENTH, FILE_NAME_LENTH);// 201311
    HMI_PrintLcd(LCD_LINE2, LCD_ProductName);  // 201311
}
////////////////////////////////////////////////////////////////  // 201311

static int LoadTxtFile(U8 app_file)
{
	U8 * line;
	FS_FILE *fb;
    char FileName[FILE_NAME_MAX];
    static U8 * ptr;
    static U8 ProgArray[CH_FILE_MAX];

	if(app_file)
	{
    	if(fb = FS_FOpen(INDEX_FILE,"r")) // Read the file.
    	{
    		FS_FRead(FileName,1,FILE_NAME_MAX,fb);
    		FS_FClose(fb);
    	}
    }
    else
    {
        strcpy(FileName, "\\DUT_FW\\DUT_FCT.txt");
    }
    
	if(fb = FS_FOpen(FileName,"r")) // Read the file.
	{
		FS_FRead((char * )ProgArray,1,CH_FILE_MAX,fb);
		FS_FClose(fb);
		
        Dprintf("Load the file:%s\r\n", FileName);
	}
	else
	{
        Dprintf("Error!!!Can not find the file %s .\r\n", FileName);
	    return(FALSE);
	}

    eprom_sections = 0;
    eprom_sections_number = 0;
    eprom_sections_length_number = -1;
    eprom_number = 0;
    total_short_length = 0;

    ptr = ProgArray;

	while((line = (U8 * )strtok((char * )ptr, "\n")) != NULL)
	{
		ProcTxtLine(line);	//Process this line.
		ptr += strlen((char * )line) + 1;
	}
	
	return(TRUE);
}


//! \brief The basic Replicator routine
//! \details This function is executed once at startup and can be restarted 
//! by pressing button "S1" on the REP430F board.
U8 Programm_msp430(U8 app_file)
{
    if(LoadTxtFile(app_file) == FALSE)
    {
        return STATUS_ERROR;
    }
	Dprintf("1:Read txt file OK.\r\n");
	
/*------------------------------------------------------------------------------------------------------*/
/*  1. | Initialize host MSP430 (on Replicator board) & target board                                    */
/*------------------------------------------------------------------------------------------------------*/    
    
    Init_JTAG();                         // Initialize JTAG

/*------------------------------------------------------------------------------------------------------*/
/*  2. | Connect to the target device                                                                   */
/*------------------------------------------------------------------------------------------------------*/    
    
    if (GetDevice() != STATUS_OK)         // Set DeviceId
    {
        Dprintf("GetDevice ERROR!\r\n");
        return STATUS_ERROR;
    }                                     // time-out. (error: red LED is ON)
    Dprintf("2:GetDevice OK!\r\n");
    
/*------------------------------------------------------------------------------------------------------*/
/*  3. | Control the target peripherals directly                                                        */
/*------------------------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------------------------*/
/*  4. | Perform a write + read + verify in the target RAM                                              */
/*------------------------------------------------------------------------------------------------------*/

    // Communication test: Write 2 RAM bytes
    WriteMem(F_BYTE, 0x0200, 0x34);
    WriteMem(F_BYTE, 0x0201, 0x12);
    // Read back word
    if (ReadMem(F_WORD, 0x0200) != 0x1234)
    {
        Dprintf("verify RAM 0x0200 ERROR!\r\n");
        return STATUS_ERROR;
    }
    Dprintf("3:Verify RAM OK!\r\n");
/*------------------------------------------------------------------------------------------------------*/
/*  5. | Operations in the device's main memory                                                         */
/*------------------------------------------------------------------------------------------------------*/
    
    // Perform a mass erase  
    EraseFLASH(ERASE_MASS, 0xFE00);     // Mass-Erase Flash (all types)
    
    EraseFLASH(ERASE_SGMT, 0x1080);
    if (!EraseCheck(0x1080, 0x0020))
    {
        Dprintf("verify Erase 0x1080 ERROR!\r\n");
        return STATUS_ERROR;
    }
    
    EraseFLASH(ERASE_SGMT, 0x1040);
    if (!EraseCheck(0x1040, 0x0020))
    {
        Dprintf("verify Erase 0x1040 ERROR!\r\n");
        return STATUS_ERROR;
    }
    
    EraseFLASH(ERASE_SGMT, 0x1000);
    if (!EraseCheck(0x1000, 0x0020))
    {
        Dprintf("verify Erase 0x1000 ERROR!\r\n");
        return STATUS_ERROR;
    }
    Dprintf("4:Erase flash OK!\r\n");

    // Program target code
    if (!WriteFLASHallSections(&eprom[0], &eprom_address[0], &eprom_length_of_sections[0], eprom_sections))
    {
        Dprintf("Write FLAS Hall Sections failed\r\n");
        ReleaseSignals();
        return STATUS_ERROR;
    }

    Dprintf("5:Program ALL flash ok!\r\n");   

/*------------------------------------------------------------------------------------------------------*/
/*  6. | Blow the JTAG access protection fuse                                                           */
/*------------------------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------------------------*/
/*  7. | Release the target device from JTAG control and wait for the user to press button "S1"         */
/*------------------------------------------------------------------------------------------------------*/
    
    ReleaseDevice(V_RESET);         // Perform Reset, release CPU from JTAG control

    return(TRUE);
}



/*------------------------------------------------------------------------------------------------------*/

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
