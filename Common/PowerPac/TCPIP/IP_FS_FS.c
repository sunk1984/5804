/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : IP_FS_FS.c
Purpose     : Implementation of emFile
---------------------------END-OF-HEADER------------------------------
*/

#include <stdio.h>
#include <string.h>
#include "IP_FS.h"
#include "FS.h"


#define MAX_PATH  128

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8 _IsInited;

/*********************************************************************
*
*       Static const data
*
**********************************************************************
*/
static const char * _sVolumeName = "";

/*********************************************************************
*
*       _InitIfRequired
*/
static void _InitIfRequired(void) {
  if (_IsInited == 0) {
    FS_Init();
    FS_FormatLLIfRequired(_sVolumeName);
    //
    // Check if volume needs to be high level formatted.
    //
    if (FS_IsHLFormatted(_sVolumeName) == 0) {
      //printf("High level formatting: %s\n", _sVolumeName);
      FS_Format(_sVolumeName, NULL);
    }
    //
    // Enable long file name support if LFN package is available.
    // LFN is an optional emFile package!
    //
//    FS_FAT_SupportLFN();
    _IsInited = 1;
  }
}

/*********************************************************************
*
*       _ConvertPath
*
*  Function description
*    Makes sure the file name is absolute.
*    If the file name is relative, it is converted into an absolute file name.
*
*    A fully qualified file name looks like:
*    [DevName:[UnitNum:]][DirPathList]Filename

*
*  Sample
*    _sVolumeName     sFilename         sAbsFilename
*    "ram:\"
*
*  Return value
*    0    O.K.
*  !=0    Error
*/
static int _ConvertPath(const char * sFilename, char * sOutFilename, U32 BufferSize) {
  char c;

  do {
    if (--BufferSize <= 0) {
      break;                  // Buffer full. We have to stop.
    }
    c = *sFilename++;
    if (c == 0) {
      break;                  // End of string
    }
    if (c == '/') {
      c = '\\';
    }
    *sOutFilename++ = c;
  } while (1);
  *sOutFilename++ = 0;
  return 0;
}


/*********************************************************************
*
*       _FS_Open
*/
static void * _FS_Open  (const char *sFilename) {
  char acAbsFilename[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sFilename, acAbsFilename, sizeof(acAbsFilename));
  return FS_FOpen(acAbsFilename, "r");
}

/*********************************************************************
*
*       _Close
*/
static int _Close (void * hFile) {
  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  return FS_FClose((FS_FILE*) hFile);
}

/*********************************************************************
*
*       _ReadAt
*/
static int _ReadAt(void * hFile, void *pDest, U32 Pos, U32 NumBytes) {
  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  FS_FSeek((FS_FILE*) hFile, Pos, FS_SEEK_SET);
  FS_Read((FS_FILE*) hFile, pDest, NumBytes);
  return 0;
}

/*********************************************************************
*
*       _GetLen
*/
static long _GetLen(void * hFile) {
  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  return FS_GetFileSize((FS_FILE*) hFile);

}

/*********************************************************************
*
*       _ForEachDirEntry
*/
static void _ForEachDirEntry (void * pContext, const char * sDir, void (*pf) (void * pContext, void * pFileEntry)) {
  FS_FIND_DATA fd;
  char acDirname[MAX_PATH];
  char acFilename[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sDir, acDirname, sizeof(acDirname));
  if (FS_FindFirstFile(&fd, acDirname, acFilename, sizeof(acFilename)) == 0) {
    do {
      pf(pContext, &fd);
    } while (FS_FindNextFile (&fd));
  }
  FS_FindClose(&fd);
}

/*********************************************************************
*
*       _GetDirEntryFilename
*/
static void _GetDirEntryFilename(void * pFileEntry, char * sFilename, U32 SizeofBuffer) {
  FS_FIND_DATA * pFD;

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  pFD = (FS_FIND_DATA *)pFileEntry;
  strncpy(sFilename, pFD->sFileName, SizeofBuffer);
  * (sFilename + SizeofBuffer - 1) = 0;
}

/*********************************************************************
*
*       _GetDirEntryFileSize
*/
static U32 _GetDirEntryFileSize (void * pFileEntry, U32 * pFileSizeHigh) {
  FS_FIND_DATA * pFD;

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  pFD = (FS_FIND_DATA *)pFileEntry;
  return pFD->FileSize;
}

/*********************************************************************
*
*       _GetDirEntryFileTime
*/
static U32 _GetDirEntryFileTime (void * pFileEntry) {
  FS_FIND_DATA * pFD;

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  pFD = (FS_FIND_DATA *)pFileEntry;
  return pFD->LastWriteTime;
}

/*********************************************************************
*
*       _GetDirEntryAttributes
*
*  Return value
*    bit 0   - 0: File, 1: Directory
*/
static int  _GetDirEntryAttributes (void * pFileEntry) {
  FS_FIND_DATA * pFD;

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  pFD = (FS_FIND_DATA *)pFileEntry;
  return (pFD->Attributes & FS_ATTR_DIRECTORY) ? 1 : 0;
}

/*********************************************************************
*
*       _Create
*/
static void * _Create (const char * sFilename) {
  char acAbsFilename[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sFilename, acAbsFilename, sizeof(acAbsFilename));
  return FS_FOpen(acAbsFilename, "wb");
}


/*********************************************************************
*
*       _DeleteFile
*/
static void * _DeleteFile (const char *sFilename) {
  char acAbsFilename[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sFilename, acAbsFilename, sizeof(acAbsFilename));
  return (void*)FS_Remove(acAbsFilename);
}

/*********************************************************************
*
*       _RenameFile
*/
static int _RenameFile (const char *sOldFilename, const char *sNewFilename) {
  char acAbsOldFilename[MAX_PATH];
  char acAbsNewFilename[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sOldFilename, acAbsOldFilename, sizeof(acAbsOldFilename));
  _ConvertPath(sNewFilename, acAbsNewFilename, sizeof(acAbsNewFilename));
  return FS_Rename(acAbsOldFilename, acAbsNewFilename);
}

/*********************************************************************
*
*       _WriteAt
*/
static int    _WriteAt (void * hFile, void *pBuffer, U32 Pos, U32 NumBytes) {
  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  FS_FSeek((FS_FILE*) hFile, Pos, FS_SEEK_SET);
  return FS_Write((FS_FILE*) hFile, pBuffer, NumBytes);
}

/*********************************************************************
*
*       _MKDir
*/
static int _MKDir (const char * sDirname) {
  char acAbsDirname[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sDirname, acAbsDirname, sizeof(acAbsDirname));
  return FS_MkDir(acAbsDirname);
}

/*********************************************************************
*
*       _RMDir
*/
static int _RMDir (const char * sDirname) {
  char acAbsDirname[MAX_PATH];

  _InitIfRequired();         // Perform automatic initialisation so that explicit call to FS_Init is not required
  _ConvertPath(sDirname, acAbsDirname, sizeof(acAbsDirname));
  return FS_RmDir(acAbsDirname);
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const IP_FS_API IP_FS_FS = {
  _FS_Open,
  _Close,
  _ReadAt,
  _GetLen,
  _ForEachDirEntry,
  _GetDirEntryFilename,
  _GetDirEntryFileSize,
  _GetDirEntryFileTime,
  _GetDirEntryAttributes,
  _Create,
  _DeleteFile,
  _RenameFile,
  _WriteAt,
  _MKDir,
  _RMDir
};

/*************************** End of file ****************************/
