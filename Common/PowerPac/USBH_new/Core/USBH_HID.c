/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_HID.c
Purpose     : API of the USB host stack
---------------------------END-OF-HEADER------------------------------
*/

#include "USBH_Int.h"

/*********************************************************************
*
*       Defines configurable
*
**********************************************************************
*/

#define MAX_TRANSFERS_ERRORS 3 // If the device returns more than 3 errors the application stops reading from the device and waits for removing the device
#define NUM_DEVICES          3
#define OLD_STATE_NUMBYTES   8
#define WRITE_DEFAULT_TIMEOUT   500 // 500ms shall be sufficent enough in order to send a SET_REPORT request to the device.

/*********************************************************************
*
*       Define non-configurable
*
**********************************************************************
*/

#define DEC_HID_REF()      pInst->RefCnt--; USBH_ASSERT((int)pInst->RefCnt >= 0)
#define INC_HID_REF()      pInst->RefCnt++

#define HID_GENERAL        0
#define HID_MOUSE          1
#define HID_KEYBOARD       2
#define NUM_DEVICE_TYPES   3

#define USBH_HID_MAX_FIELDS                16
#define USBH_HID_NUM_REPORT_TYPES           3
#define USBH_HID_MAX_USAGES              1024
#define USBH_HID_DEFAULT_NUM_COLLECTIONS    2
#define USBH_HID_GLOBAL_STACK_SIZE          2
#define USBH_HID_COLLECTION_STACK_SIZE      2

//
// HID report descriptor collection item types
//
#define USBH_HID_COLLECTION_PHYSICAL    0
#define USBH_HID_COLLECTION_APPLICATION 1
#define USBH_HID_COLLECTION_LOGICAL     2

//
// HID report item format
//
#define USBH_HID_ITEM_FORMAT_SHORT    0
#define USBH_HID_ITEM_FORMAT_LONG     1

//
// Special tag indicating long items
//
#define USBH_HID_ITEM_TAG_LONG        15

//
// HID report descriptor item type (prefix bit 2,3)
//
#define USBH_HID_ITEM_TYPE_MAIN           0
#define USBH_HID_ITEM_TYPE_GLOBAL         1
#define USBH_HID_ITEM_TYPE_LOCAL          2
#define USBH_HID_ITEM_TYPE_RESERVED       3

//
// HID report descriptor main item tags
//
#define USBH_HID_MAIN_ITEM_TAG_INPUT             8
#define USBH_HID_MAIN_ITEM_TAG_OUTPUT            9
#define USBH_HID_MAIN_ITEM_TAG_FEATURE          11
#define USBH_HID_MAIN_ITEM_TAG_BEGIN_COLLECTION 10
#define USBH_HID_MAIN_ITEM_TAG_END_COLLECTION   12

//
// HID report descriptor main item contents
//
#define USBH_HID_MAIN_ITEM_CONSTANT           0x001
#define USBH_HID_MAIN_ITEM_VARIABLE           0x002
#define USBH_HID_MAIN_ITEM_RELATIVE           0x004
#define USBH_HID_MAIN_ITEM_WRAP               0x008
#define USBH_HID_MAIN_ITEM_NONLINEAR          0x010
#define USBH_HID_MAIN_ITEM_NO_PREFERRED       0x020
#define USBH_HID_MAIN_ITEM_NULL_STATE         0x040
#define USBH_HID_MAIN_ITEM_VOLATILE           0x080
#define USBH_HID_MAIN_ITEM_BUFFERED_BYTE      0x100

//
// HID report descriptor global item tags
//
#define USBH_HID_GLOBAL_ITEM_TAG_USAGE_PAGE       0
#define USBH_HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM  1
#define USBH_HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM  2
#define USBH_HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM 3
#define USBH_HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM 4
#define USBH_HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT    5
#define USBH_HID_GLOBAL_ITEM_TAG_UNIT             6
#define USBH_HID_GLOBAL_ITEM_TAG_REPORT_SIZE      7
#define USBH_HID_GLOBAL_ITEM_TAG_REPORT_ID        8
#define USBH_HID_GLOBAL_ITEM_TAG_REPORT_COUNT     9
#define USBH_HID_GLOBAL_ITEM_TAG_PUSH            10
#define USBH_HID_GLOBAL_ITEM_TAG_POP             11

//
// HID report descriptor local item tags
//
#define USBH_HID_LOCAL_ITEM_TAG_USAGE               0
#define USBH_HID_LOCAL_ITEM_TAG_USAGE_MINIMUM       1
#define USBH_HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM       2
#define USBH_HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX    3
#define USBH_HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM  4
#define USBH_HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM  5
#define USBH_HID_LOCAL_ITEM_TAG_STRING_INDEX        7
#define USBH_HID_LOCAL_ITEM_TAG_STRING_MINIMUM      8
#define USBH_HID_LOCAL_ITEM_TAG_STRING_MAXIMUM      9
#define USBH_HID_LOCAL_ITEM_TAG_DELIMITER          10

/*********************************************************************
*
*       Data structures
*
**********************************************************************
*/

typedef enum _USBH_HID_STATE {
  StateIdle,  // Initial state, set if UsbhHid_InitDemoApp is called
  StateStop,  // Device is removed or an user break
  StateError, // Application error
  StateInit,  // Set during device initialization
  StateRunning
} USBH_HID_STATE;

typedef struct USBH_HID_INST USBH_HID_INST;

typedef struct HID_FIELD    HID_FIELD;
typedef struct HID_REPORT   HID_REPORT;
typedef struct HID_INPUT    HID_INPUT;
typedef struct HID_DEVICE   HID_DEVICE;

/*
 * We parse each description item into this structure. Short items data
 * values are expanded to 32-bit signed int, long items contain a pointer
 * into the data area.
 */

typedef struct HID_ITEM {
  unsigned  Format;
  U8      Size;
  U8      Type;
  U8      Tag;
  union {
      U8   u8;
      I8   s8;
      U16  u16;
      I16  s16;
      U32  u32;
      I32  s32;
      U8  *longdata;
  } Data;
} HID_ITEM ;

/*
 * This is the global environment of the parser. This information is
 * persistent for main-items. The global environment can be saved and
 * restored with PUSH/POP statements.
 */

typedef struct HID_GLOBAL {
  I32      LogicalMinimum;
  I32      LogicalMaximum;
  I32      PhysicalMinimum;
  I32      PhysicalMaximum;
  I32      UnitExponent;
  unsigned UsagePage;
  unsigned Unit;
  unsigned ReportId;
  unsigned ReportSize;
  unsigned ReportCount;
} HID_GLOBAL;

/*
 * This is the local environment. It is persistent up the next main-item.
 */
typedef struct HID_LOCAL {
  unsigned aUsage[USBH_HID_MAX_USAGES]; /* usage array */
  unsigned aCollectionIndex[USBH_HID_MAX_USAGES]; /* collection index array */
  unsigned UsageIndex;
  unsigned UsageMinimum;
  unsigned DelimiterDepth;
  unsigned DelimiterBranch;
} HID_LOCAL;

/*
 * This is the collection stack. We climb up the stack to determine
 * application and function of each field.
 */

typedef struct HID_COLLECTION {
  unsigned Type;
  unsigned Usage;
  unsigned Level;
} HID_COLLECTION;

typedef struct HID_USAGE {
  unsigned  Hid;      /* hid usage code */
  unsigned  CollectionIndex;  /* index into collection array */
  /* hidinput data */
  U16       Code;       /* input driver code */
  U8        Type;       /* input driver type */
  I8        HatMin;   /* hat switch fun */
  I8        HatMax;   /* ditto */
  I8        HatDir;   /* ditto */
} HID_USAGE;

typedef struct HID_REPORT_ENUM {
  unsigned     Numbered;
  USBH_DLIST        ReportList;
  HID_REPORT * apReportIdHash[8];
} HID_REPORT_ENUM;


typedef struct HID_PARSER {
  HID_GLOBAL     Global;
  HID_GLOBAL     aGlobalStack[USBH_HID_GLOBAL_STACK_SIZE];
  unsigned       GlobalStackPtr;
  HID_LOCAL      Local;
  unsigned       aCollectionStack[USBH_HID_COLLECTION_STACK_SIZE];
  unsigned       CollectionStackPtr;
} HID_PARSER;


struct HID_FIELD {
  unsigned     Physical;            // Physical usage for this field
  unsigned     Logical;             // Logical usage for this field
  unsigned     Application;         // Application usage for this field
  HID_USAGE  * pUsage;              // Usage table for this function
  unsigned     MaxUsage;            // maximum usage index
  unsigned     Flags;               // Main-item flags (i.e. volatile,array,constant)
  unsigned     ReportOffset;        // Bit offset in the report
  unsigned     ReportSize;          // Size of this field in the report
  unsigned     ReportCount;         // Number of this field in the report
  unsigned     ReportType;          // (input,output,feature)
  I32        * pValue;              // Last known value(s)
  I32          LogicalMinimum;
  I32          LogicalMaximum;
  I32          PhysicalMinimum;
  I32          PhysicalMaximum;
  I32          UnitExponent;
  unsigned     Unit;
  unsigned     Index;               // Index into pReport->pField[]
};


struct HID_REPORT {
  USBH_DLIST        List;
  unsigned     Id;                           // ID of this report
  unsigned     Type;                         // Report type
  HID_FIELD  * apField[USBH_HID_MAX_FIELDS]; // Fields of the report
  unsigned     MaxField;                     // Maximum valid field index
  unsigned     Size;                         // Size of the report (bits)
};

struct USBH_HID_INST {
  union {
    USBH_DLIST_ITEM             Link;
    struct USBH_HID_INST *      pInst;
  } Next;
  U8                            IsUsed;
  USBH_HID_STATE                RunningState;
  USBH_INTERFACE_ID             InterfaceID;
  U8                            DevInterfaceID;
  USBH_INTERFACE_HANDLE         hInterface;
  U8                            IntEp;
  U8                            OutEp;
  U16                           InMaxPktSize; // Maximum packet size, important read only with this size from the device
  U16                           OutMaxPktSize; // Maximum packet size, important read only with this size from the device
  USBH_URB                      OutUrb;
  USBH_URB                      InUrb;
  USBH_URB                      AbortUrb;
  USBH_URB                      ControlUrb;
  int                           ReadErrorCount;
  U32                           RefCnt;
  U8                          * pReportBufferDesc;
  U8                          * pInBuffer;
  U8                          * pOutBuffer;
  U8                          * pOldState;
  U8                            ReportDescriptorSize;
  U8                            DeviceType;
  union {
    USBH_HID_ON_KEYBOARD_FUNC * pfOnKeyStateChange;
    USBH_HID_ON_MOUSE_FUNC    * pfOnMouseStateChange;
  } Device;
  USBH_HID_HANDLE               Handle;
  HID_COLLECTION              * pCollection;       // List of HID collections
  U16                           CollectionSize;    // Number of allocated Collections
  U16                           MaxCollection;     // Number of parsed collections
  U16                           MaxApplication;    // Number of applications
  HID_REPORT_ENUM               aReportEnum[USBH_HID_NUM_REPORT_TYPES];
  U8                            DevIndex;         // Device name that is used in order to open the device from outside.
  U8                            IsOpened;
  U8                            ReportDescriptorParsed;
  USBH_OS_EVENT_OBJ           * pInTransactionEvent;
  USBH_OS_EVENT_OBJ           * pOutTransactionEvent;
};

typedef struct {
  union {
    USBH_HID_INST           * pHidFirst;
    USBH_DLIST_HEAD           Head;
  } List;
  U8                          NumDevices;
  USBH_NOTIFICATION_HANDLE    hDevNotification;
  U8                          LedState;
  int                         NextHandle;
  USBH_HID_ON_KEYBOARD_FUNC * pfOnKeyStateChange;
  USBH_HID_ON_MOUSE_FUNC    * pfOnMouseStateChange;
  USBH_NOTIFICATION_FUNC    * pfNotification;
  void                      * pNotifyContext;
  U8                          ControlWriteRetries;
  U32                         ControlWriteTimeout;
} USBH_HID_GLOBAL;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static USBH_HID_GLOBAL           USBH_HID_Global;

/*********************************************************************
*
*       Static prototypes
*
**********************************************************************
*/
static void        _OnResetReadEndpointCompletion(USBH_URB * pUrb);
static USBH_STATUS _SubmitInBuffer(USBH_HID_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_HID_USER_FUNC * pfUser, void * pUserContext);
static USBH_STATUS _CancelIO(USBH_HID_INST * pInst);

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _GetItemUnsigned
*
*  Function description:
*    Returns the unsigned data value from a pItem.
*
*/
static U32 _GetItemUnsigned(HID_ITEM * pItem) {
  switch (pItem->Size) {
    case 1: return pItem->Data.u8;
    case 2: return pItem->Data.u16;
    case 4: return pItem->Data.u32;
  }
  return 0;
}

/*********************************************************************
*
*       _GetItemSigned
*
*  Function description:
*    Returns the signed data value from a pItem.
*
*/
static I32 _GetItemSigned(HID_ITEM * pItem) {
  switch (pItem->Size) {
    case 1: return pItem->Data.s8;
    case 2: return pItem->Data.s16;
    case 4: return pItem->Data.s32;
  }
  return 0;
}

/*********************************************************************
*
*       _RegisterReport
*
*  Function description:
*    Registers a new report for a instance.
*
*/
static HID_REPORT *_RegisterReport(USBH_HID_INST * pInst, unsigned Type, unsigned Id) {
  HID_REPORT_ENUM * pReportEnum;
  HID_REPORT      * pReport;

  pReportEnum = pInst->aReportEnum + Type;
  if (pReportEnum->apReportIdHash[Id]) {
    return pReportEnum->apReportIdHash[Id];
  }
  pReport = (HID_REPORT *)USBH_Malloc(sizeof(HID_REPORT));
  if (!(pReport)) {
    return NULL;
  }
  memset(pReport, 0, sizeof(HID_REPORT));
  if (Id != 0) {
    pReportEnum->Numbered = 1;
  }
  pReport->Id = Id;
  pReport->Type = Type;
  pReport->Size = 0;
  pReportEnum->apReportIdHash[Id] = pReport;
  USBH_DLIST_InsertTail(&pReportEnum->ReportList, &pReport->List);
  return pReport;
}

/*********************************************************************
*
*       _RegisterField
*
*  Function description:
*    Registers a new field for this report.
*
*/
static HID_FIELD * _RegisterField(HID_REPORT * pReport, unsigned Usages, unsigned Values) {
  HID_FIELD * pField;

  if (pReport->MaxField == USBH_HID_MAX_FIELDS) {
    USBH_WARN((USBH_MTYPE_HID, "Too many fields in report"));
    return NULL;
  }
  pField = (HID_FIELD *)USBH_Malloc(sizeof(HID_FIELD) + Usages * sizeof(HID_USAGE) + Values * sizeof(unsigned));
  if (!(pField)) {
    return NULL;
  }
  memset(pField, 0, sizeof(HID_FIELD) + Usages * sizeof(HID_USAGE) + Values * sizeof(unsigned));
  pField->Index = pReport->MaxField++;
  pReport->apField[pField->Index] = pField;
  pField->pUsage = (HID_USAGE *)(pField + 1);
  pField->pValue = (I32 *)(pField->pUsage + Usages);
  return pField;
}

/*********************************************************************
*
*       _OpenCollection
*
*  Function description:
*    Opens a collection. The type/usage is pushed on the stack.
*
*/
static int _OpenCollection(USBH_HID_INST * pInst, HID_PARSER * pParser, unsigned Type) {
  HID_COLLECTION * pCollection;
  unsigned         Usage;

  Usage = pParser->Local.aUsage[0];
  if (pParser->CollectionStackPtr == USBH_HID_COLLECTION_STACK_SIZE) {
    USBH_WARN((USBH_MTYPE_HID, "Collection stack overflow"));
    return -1;
  }
  if (pInst->MaxCollection == pInst->CollectionSize) {
    pCollection = (HID_COLLECTION *)USBH_Malloc(sizeof(HID_COLLECTION) * pInst->CollectionSize * 2);
    if (pCollection == NULL) {
      USBH_WARN((USBH_MTYPE_HID, "Failed to reallocate collection array"));
      return -1;
    }
    memcpy(pCollection, pInst->pCollection, sizeof(HID_COLLECTION) *  pInst->CollectionSize);
    memset(pCollection + pInst->CollectionSize, 0, sizeof(HID_COLLECTION) * pInst->CollectionSize);
    USBH_Free(pInst->pCollection);
    pInst->pCollection = pCollection;
    pInst->CollectionSize *= 2;
  }
  pParser->aCollectionStack[pParser->CollectionStackPtr++] = pInst->MaxCollection;
  pCollection = pInst->pCollection +  pInst->MaxCollection++;
  pCollection->Type = Type;
  pCollection->Usage = Usage;
  pCollection->Level = pParser->CollectionStackPtr - 1;
  if (Type == USBH_HID_COLLECTION_APPLICATION) {
    pInst->MaxApplication++;
  }
  return 0;
}

/*********************************************************************
*
*       _CloseCollection
*
*  Function description:
*    Closes a collection.
*
*/
static int _CloseCollection(HID_PARSER * pParser) {
  if (!pParser->CollectionStackPtr) {
    USBH_WARN((USBH_MTYPE_HID, "Collection stack underflow"));
    return -1;
  }
  pParser->CollectionStackPtr--;
  return 0;
}

/*********************************************************************
*
*       _LookupCollection
*
*  Function description:
*    Climbs up the stack and searches for the specified collection type
*    and return the usage.
*
*/
static unsigned _LookupCollection(USBH_HID_INST * pInst, HID_PARSER * pParser, unsigned Type) {
  int n;
  for (n = pParser->CollectionStackPtr - 1; n >= 0; n--) {
    if (pInst->pCollection[pParser->aCollectionStack[n]].Type == Type) {
      return pInst->pCollection[pParser->aCollectionStack[n]].Usage;
    }
  }
  return 0; /* we know nothing about this usage type */
}

/*********************************************************************
*
*       _AddUsage
*
*  Function description:
*    Adds a usage to the temporary parser table.
*
*/
static int _AddUsage(HID_PARSER * pParser, unsigned Usage) {
  if (pParser->Local.UsageIndex >= USBH_HID_MAX_USAGES) {
    USBH_WARN((USBH_MTYPE_HID, "Usage index exceeded"));
    return -1;
  }
  pParser->Local.aUsage[pParser->Local.UsageIndex] = Usage;
  pParser->Local.aCollectionIndex[pParser->Local.UsageIndex] = pParser->CollectionStackPtr ? pParser->aCollectionStack[pParser->CollectionStackPtr - 1] : 0;
  pParser->Local.UsageIndex++;
  return 0;
}

/*********************************************************************
*
*       _AddField
*
*  Function description:
*    Registers a new field for this report.
*
*/
static int _AddField(USBH_HID_INST * pInst, HID_PARSER * pParser, unsigned ReportType, unsigned Flags) {
  HID_REPORT * pReport;
  HID_FIELD  * pField;
  int          Usages;
  int          i;
  unsigned     Off;
  pReport = _RegisterReport(pInst, ReportType, pParser->Global.ReportId);
  if (!(pReport)) {
    USBH_WARN((USBH_MTYPE_HID, "_RegisterReport failed"));
    return -1;
  }
  if (pParser->Global.LogicalMaximum < pParser->Global.LogicalMinimum) {
    USBH_WARN((USBH_MTYPE_HID, "Logical range invalid %d %d", pParser->Global.LogicalMinimum, pParser->Global.LogicalMaximum));
    return -1;
  }
  Off = pReport->Size;
  pReport->Size += pParser->Global.ReportSize * pParser->Global.ReportCount;
  if (!pParser->Local.UsageIndex) {/* Ignore padding fields */
    return 0;
  }
  Usages = USBH_MAX(pParser->Local.UsageIndex, pParser->Global.ReportCount);
  if ((pField = _RegisterField(pReport, Usages, pParser->Global.ReportCount)) == NULL) {
    return 0;
  }
  pField->Physical    = _LookupCollection(pInst, pParser, USBH_HID_COLLECTION_PHYSICAL);
  pField->Logical     = _LookupCollection(pInst, pParser, USBH_HID_COLLECTION_LOGICAL);
  pField->Application = _LookupCollection(pInst, pParser, USBH_HID_COLLECTION_APPLICATION);
  for (i = 0; i < Usages; i++) {
    int j = i;
    /* Duplicate the last usage we parsed if we have excess values */
    if (i >= (int)pParser->Local.UsageIndex) {
      j = pParser->Local.UsageIndex - 1;
    }
    pField->pUsage[i].Hid = pParser->Local.aUsage[j];
    pField->pUsage[i].CollectionIndex = pParser->Local.aCollectionIndex[j];
  }
  pField->MaxUsage = Usages;
  pField->Flags = Flags;
  pField->ReportOffset = Off;
  pField->ReportType = ReportType;
  pField->ReportSize = pParser->Global.ReportSize;
  pField->ReportCount = pParser->Global.ReportCount;
  pField->LogicalMinimum = pParser->Global.LogicalMinimum;
  pField->LogicalMaximum = pParser->Global.LogicalMaximum;
  pField->PhysicalMinimum = pParser->Global.PhysicalMinimum;
  pField->PhysicalMaximum = pParser->Global.PhysicalMaximum;
  pField->UnitExponent = pParser->Global.UnitExponent;
  pField->Unit = pParser->Global.Unit;
  return 0;
}

/*********************************************************************
*
*       _Parser4GlobalItem
*
*  Function description:
*    Process a global pItem.
*
*/
static int _Parser4GlobalItem(USBH_HID_INST * pInst, HID_PARSER * pParser, HID_ITEM * pItem) {
  USBH_USE_PARA(pInst);
  switch (pItem->Tag) {
  case USBH_HID_GLOBAL_ITEM_TAG_PUSH:
    if (pParser->GlobalStackPtr == USBH_HID_GLOBAL_STACK_SIZE) {
      USBH_WARN((USBH_MTYPE_HID, "Global environment stack overflow"));
      return -1;
    }
    memcpy(pParser->aGlobalStack + pParser->GlobalStackPtr++, &pParser->Global, sizeof(HID_GLOBAL));
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_POP:
    if (!pParser->GlobalStackPtr) {
      USBH_WARN((USBH_MTYPE_HID, "Global environment stack underflow"));
      return -1;
    }
    memcpy(&pParser->Global, pParser->aGlobalStack + --pParser->GlobalStackPtr, sizeof(HID_GLOBAL));
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
    pParser->Global.UsagePage = _GetItemUnsigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
    pParser->Global.LogicalMinimum = _GetItemSigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
    if (pParser->Global.LogicalMinimum < 0)
      pParser->Global.LogicalMaximum = _GetItemSigned(pItem);
    else
      pParser->Global.LogicalMaximum = _GetItemUnsigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
    pParser->Global.PhysicalMinimum = _GetItemSigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
    if (pParser->Global.PhysicalMinimum < 0)
      pParser->Global.PhysicalMaximum = _GetItemSigned(pItem);
    else
      pParser->Global.PhysicalMaximum = _GetItemUnsigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT:
    pParser->Global.UnitExponent = _GetItemSigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_UNIT:
    pParser->Global.Unit = _GetItemUnsigned(pItem);
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
    if ((pParser->Global.ReportSize = _GetItemUnsigned(pItem)) > 32) {
      USBH_WARN((USBH_MTYPE_HID, "Invalid ReportSize %d", pParser->Global.ReportSize));
      return -1;
    }
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
    if ((pParser->Global.ReportCount = _GetItemUnsigned(pItem)) > USBH_HID_MAX_USAGES) {
      USBH_WARN((USBH_MTYPE_HID, "Invalid ReportCount %d", pParser->Global.ReportCount));
      return -1;
    }
    return 0;
  case USBH_HID_GLOBAL_ITEM_TAG_REPORT_ID:
    if ((pParser->Global.ReportId = _GetItemUnsigned(pItem)) == 0) {
      USBH_WARN((USBH_MTYPE_HID, "ReportId 0 is invalid"));
      return -1;
    }
    return 0;
  default:
    USBH_WARN((USBH_MTYPE_HID, "Unknown global tag 0x%x", pItem->Tag));
    return -1;
  }
}

/*********************************************************************
*
*       _Parser4LocalItem
*
*  Function description:
*    Processes a local item.
*
*/
static int _Parser4LocalItem(USBH_HID_INST * pInst, HID_PARSER * pParser, HID_ITEM * pItem) {
  U32      Data;
  unsigned n;

  USBH_USE_PARA(pInst);
  if (pItem->Size == 0) {
    USBH_WARN((USBH_MTYPE_HID, "Item data expected for local item"));
    return -1;
  }
  Data = _GetItemUnsigned(pItem);
  switch (pItem->Tag) {
  case USBH_HID_LOCAL_ITEM_TAG_DELIMITER:
    if (Data) {
      /*
       * We treat items before the first delimiter
       * as global to all usage sets (branch 0).
       * In the moment we process only these global
       * items and the first delimiter set.
       */
      if (pParser->Local.DelimiterDepth != 0) {
        USBH_WARN((USBH_MTYPE_HID, "Nested delimiters"));
        return -1;
      }
      pParser->Local.DelimiterDepth++;
      pParser->Local.DelimiterBranch++;
    } else {
      if (pParser->Local.DelimiterDepth < 1) {
        USBH_WARN((USBH_MTYPE_HID, "Close delimiter"));
        return -1;
      }
      pParser->Local.DelimiterDepth--;
    }
    return 1;
  case USBH_HID_LOCAL_ITEM_TAG_USAGE:
    if (pParser->Local.DelimiterBranch > 1) {
      USBH_WARN((USBH_MTYPE_HID, "Alternative usage ignored"));
      return 0;
    }
    if (pItem->Size <= 2) {
      Data = (pParser->Global.UsagePage << 16) + Data;
    }
    return _AddUsage(pParser, Data);
  case USBH_HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:
    if (pParser->Local.DelimiterBranch > 1) {
      USBH_WARN((USBH_MTYPE_HID, "Alternative usage ignored"));
      return 0;
    }
    if (pItem->Size <= 2) {
      Data = (pParser->Global.UsagePage << 16) + Data;
    }
    pParser->Local.UsageMinimum = Data;
    return 0;
  case USBH_HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:
    if (pParser->Local.DelimiterBranch > 1) {
      USBH_WARN((USBH_MTYPE_HID, "Alternative usage ignored"));
      return 0;
    }

    if (pItem->Size <= 2) {
      Data = (pParser->Global.UsagePage << 16) + Data;
    }
    for (n = pParser->Local.UsageMinimum; n <= Data; n++) {
      if (_AddUsage(pParser, n)) {
        USBH_WARN((USBH_MTYPE_HID, "_AddUsage failed\n"));
        return -1;
      }
    }
    return 0;
  default:
    USBH_WARN((USBH_MTYPE_HID, "Unknown local item tag 0x%x", pItem->Tag));
  }
  return 0;
}

/*********************************************************************
*
*       _Parser4MainItem
*
*  Function description:
*    Processes a main item.
*
*/
static int _Parser4MainItem(USBH_HID_INST * pDevice, HID_PARSER * pParser, HID_ITEM * pItem) {
  U32 Data;
  int r;

  Data = _GetItemUnsigned(pItem);
  switch (pItem->Tag) {
  case USBH_HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
    r = _OpenCollection(pDevice, pParser, Data & 0xff);
    break;
  case USBH_HID_MAIN_ITEM_TAG_END_COLLECTION:
    r = _CloseCollection(pParser);
    break;
  case USBH_HID_MAIN_ITEM_TAG_INPUT:
    r = _AddField(pDevice, pParser, USBH_HID_INPUT_REPORT, Data);
    break;
  case USBH_HID_MAIN_ITEM_TAG_OUTPUT:
    r = _AddField(pDevice, pParser, USBH_HID_OUTPUT_REPORT, Data);
    break;
  case USBH_HID_MAIN_ITEM_TAG_FEATURE:
    r = _AddField(pDevice, pParser, USBH_HID_FEATURE_REPORT, Data);
    break;
  default:
    USBH_WARN((USBH_MTYPE_HID, "Unknown main item tag 0x%x", pItem->Tag));
    r = 0;
  }
  memset(&pParser->Local, 0, sizeof(pParser->Local)); /* Reset the local parser environment */
  return r;
}

/*********************************************************************
*
*       _Parser4Reserved
*
*  Function description:
*    Processes a reserved item.
*
*/
static int _Parser4Reserved(USBH_HID_INST * pDevice, HID_PARSER * pParser, HID_ITEM * pItem) {
  USBH_USE_PARA(pDevice);
  USBH_USE_PARA(pParser);
  USBH_USE_PARA(pItem);
  USBH_WARN((USBH_MTYPE_HID, "Reserved item type, tag 0x%x", pItem->Tag));
  return 0;
}

/*********************************************************************
*
*       _FreeReport
*
*  Function description:
*    Frees a report and all registered fields. The pField->Usage and
*    pField->Value tables are allocated behind the field, so we need
*    only to free(pField) itself.
*
*/
static void _FreeReport(HID_REPORT *pReport) {
  unsigned n;

  for (n = 0; n < pReport->MaxField; n++) {
    USBH_Free(pReport->apField[n]);
  }
  USBH_Free(pReport);
}

/*********************************************************************
*
*       _FetchItem
*
*  Function description:
*    Fetch a report description item from the data stream. Support for long
*    items is enabled, though they are not used yet.
*
*/
static U8 * _FetchItem(U8 * pStart, U8 * pEnd, HID_ITEM * pItem) {
  U8 b;

  if ((pEnd - pStart) <= 0) {
    return NULL;
  }
  b = *pStart++;
  pItem->Type = (b >> 2) & 3;
  pItem->Tag  = (b >> 4) & 15;
  if (pItem->Tag == USBH_HID_ITEM_TAG_LONG) {
    pItem->Format = USBH_HID_ITEM_FORMAT_LONG;
    if ((pEnd - pStart) < 2) {
      return NULL;
    }
    pItem->Size = *pStart++;
    pItem->Tag  = *pStart++;
    if ((pEnd - pStart) < pItem->Size) {
      return NULL;
    }
    pItem->Data.longdata = pStart;
    pStart += pItem->Size;
    return pStart;
  }
  pItem->Format = USBH_HID_ITEM_FORMAT_SHORT;
  pItem->Size = b & 3;
  switch (pItem->Size) {
  case 0:
    return pStart;
  case 1:
    if ((pEnd - pStart) < 1) {
      return NULL;
    }
    pItem->Data.u8 = *pStart++;
    return pStart;
  case 2:
    if ((pEnd - pStart) < 2) {
      return NULL;
    }
    pItem->Data.u16 = USBH_LoadU16LE(pStart);
    pStart = (U8 *)((U16 *)pStart + 1);
    return pStart;
  case 3:
    pItem->Size++;
    if ((pEnd - pStart) < 4) {
      return NULL;
    }
    pItem->Data.u32 = USBH_LoadU32LE(pStart);
    pStart = (U8 *)((U32 *)pStart + 1);
    return pStart;
  }
  return NULL;
}


static int (*_pfParser[])(USBH_HID_INST * pDevice, HID_PARSER * pParser, HID_ITEM * pItem) = {
  _Parser4MainItem,
  _Parser4GlobalItem,
  _Parser4LocalItem,
  _Parser4Reserved
};

/*********************************************************************
*
*       _ParseReport
*
*  Function description:
*    Parses a report descriptor into a instance structure. Reports are
*    enumerated, fields are attached to these reports.
*
*/
static USBH_STATUS _ParseReport(USBH_HID_INST * pInst, U8 * pStart, unsigned NumBytes) {
  HID_ITEM     Item;
  HID_PARSER * pParser;
  U8         * pEnd;
  unsigned     i;

  pInst->pCollection = (HID_COLLECTION *)USBH_Malloc(sizeof(HID_COLLECTION) * USBH_HID_DEFAULT_NUM_COLLECTIONS);
  memset(pInst->pCollection, 0, sizeof(HID_COLLECTION) * USBH_HID_DEFAULT_NUM_COLLECTIONS);
  pInst->CollectionSize = USBH_HID_DEFAULT_NUM_COLLECTIONS;
   for (i = 0; i < USBH_HID_NUM_REPORT_TYPES; i++) {
     USBH_DLIST_Init(&pInst->aReportEnum[i].ReportList);
   }
  pInst->pReportBufferDesc = pStart;
  pParser = (HID_PARSER *)USBH_Malloc(sizeof(HID_PARSER));
  if (pParser == NULL) {
    USBH_Free(pInst->pCollection);
    return USBH_STATUS_MEMORY;
  }
  memset(pParser, 0, sizeof(HID_PARSER));
  pEnd = pStart + NumBytes;
  while ((pStart = _FetchItem(pStart, pEnd, &Item)) != NULL) {
    if (Item.Format != USBH_HID_ITEM_FORMAT_SHORT) {
      USBH_WARN((USBH_MTYPE_HID, "Unexpected long global item"));
      USBH_Free(pInst->pCollection);
      USBH_Free(pParser);
      return USBH_STATUS_INVALID_DESCRIPTOR;
    }
    if (_pfParser[Item.Type](pInst, pParser, &Item)) {
      USBH_WARN((USBH_MTYPE_HID, "Item %u %u %u %u parsing failed\n", Item.Format, (unsigned)Item.Size, (unsigned)Item.Type, (unsigned)Item.Tag));
      USBH_Free(pInst->pCollection);
      USBH_Free(pParser);
      return USBH_STATUS_INVALID_DESCRIPTOR;
    }
    if (pStart == pEnd) {
      if (pParser->CollectionStackPtr) {
        USBH_WARN((USBH_MTYPE_HID, "Unbalanced collection at end of report description"));
        USBH_Free(pInst->pCollection);
        USBH_Free(pParser);
        return USBH_STATUS_INVALID_DESCRIPTOR;
      }
      if (pParser->Local.DelimiterDepth) {
        USBH_WARN((USBH_MTYPE_HID, "Unbalanced delimiter at end of report description"));
        USBH_Free(pInst->pCollection);
        USBH_Free(pParser);
        return USBH_STATUS_INVALID_DESCRIPTOR;
      }
      pInst->ReportDescriptorParsed = 1;
      USBH_Free(pParser);
      return USBH_STATUS_SUCCESS;
    }
  }

  USBH_WARN((USBH_MTYPE_HID, "Item fetching failed at offset %d\n", (int)(pEnd - pStart)));
  USBH_Free(pInst->pCollection);
  USBH_Free(pParser);
  return USBH_STATUS_INVALID_DESCRIPTOR;
}


/*********************************************************************
*
*       _h2p()
*/
static USBH_HID_INST * _h2p(USBH_HID_HANDLE Handle) {
  USBH_HID_INST * pInst;

  if (Handle == 0) {
    return NULL;
  }

  //
  // Check if the first instance is a match (which is so in most cases)
  //
  pInst = (USBH_HID_INST *)(USBH_HID_Global.List.pHidFirst);                // First instance
  if (pInst == NULL) {
    USBH_WARN((USBH_MTYPE_HID, "Instance list is empty"));
    return NULL;
  }
  if (pInst->Handle == Handle) {                                        // Match ?
    return pInst;
  }
  //
  // Iterate over linked list to find a socket with matching handle. Return if found.
  //
  do {
    pInst = (USBH_HID_INST *)pInst->Next.Link.pNext;
    if (pInst == NULL) {
      break;
    }
    if (pInst->Handle == Handle) {                                        // Match ?
      //
      // If it is not the first in the list, make it the first
      //
      if (pInst != (USBH_HID_INST *)(USBH_HID_Global.List.Head.pFirst)) {     // First socket ?
        USBH_DLIST_Remove(&USBH_HID_Global.List.Head, &pInst->Next.Link);
        USBH_DLIST_Add(&USBH_HID_Global.List.Head, &pInst->Next.Link);
      }
      return pInst;
    }
  } while(1);

  //
  // Error handling: Socket handle not found in list.
  //
  USBH_WARN((USBH_MTYPE_HID, "HID_HANDLE: handle %d not in instance list", Handle));
  return NULL;
}

/*********************************************************************
*
*       _IsValueInArray
*
*  Function description:
*    Checks whether a value is in the specified array.
*/
static int _IsValueInArray(const U8 * p, U8 Val, unsigned NumItems) {
  unsigned i;
  for (i = 0; i < NumItems; i++) {
    if (* p == Val) {
      return 1;
    }
  }
  return 0;
}

/*********************************************************************
*
*       _RemoveDevInstance
*
*/
static void _RemoveDevInstance(USBH_HID_INST * pInst) {
  unsigned i,j;
  if (pInst) {
    //
    //  Free all associated EP buffers
    //
    if (pInst->pInBuffer) {
      USBH_Free(pInst->pInBuffer);
    }
    if (pInst->pOutBuffer) {
      USBH_Free(pInst->pOutBuffer);
    }
    //
    //  Free the report descriptor and parsed information
    //
    if (pInst->pReportBufferDesc) {
      USBH_Free(pInst->pReportBufferDesc);
    }
    for (i = 0; i < USBH_HID_NUM_REPORT_TYPES; i++) {
      HID_REPORT_ENUM * pReportEnum = pInst->aReportEnum + i;

      for (j = 0; j < USBH_COUNTOF(pReportEnum->apReportIdHash); j++) {
        HID_REPORT * pReport;

        pReport = pReportEnum->apReportIdHash[j];
        if (pReport) {
          _FreeReport(pReport);
        }
      }
    }
    //
    // In case of a key board the old state information is also freed.
    if (pInst->pOldState) {
      USBH_Free(pInst->pOldState);
    }
    //
    // Remove instance from list
    //
    USBH_DLIST_Remove(&USBH_HID_Global.List.Head, (USBH_DLIST_ITEM *)pInst);
    //
    // Free the memory that is used by the instance
    //
    USBH_Free(pInst);
    pInst = (USBH_HID_INST *)NULL;
  }
}


/*********************************************************************
*
*       _CheckStateAndCloseInterface
*
*  Function description:
*    Close the handle interface when it is not referenced from any application.
*
*  Return value:
*    TRUE   - Handle is closed
*    FALSE  - Error
*/
static USBH_BOOL _CheckStateAndCloseInterface(USBH_HID_INST * pInst) {
  USBH_BOOL r = FALSE;

  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    if (!pInst->RefCnt) {
      if (pInst->hInterface != NULL) {
        _CancelIO(pInst);
        USBH_CloseInterface(pInst->hInterface);
        USBH_HID_Global.NumDevices--;
        pInst->hInterface = NULL;
        pInst->IsUsed     = 0;
        _RemoveDevInstance(pInst);
      }
    }
  }
  if (NULL == pInst->hInterface) {
    r = TRUE;
  }
  return r;
}


/*********************************************************************
*
*       _RemoveAllInstances
*
*/
static USBH_BOOL _RemoveAllInstances(void) {
  USBH_HID_INST * pInst;
  USBH_BOOL       r;

  r = FALSE;
  for (pInst = (struct USBH_HID_INST *)(USBH_HID_Global.List.Head.pFirst); pInst; pInst = (struct USBH_HID_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
     r |= _CheckStateAndCloseInterface(pInst);
  }
  return r;
}

/*********************************************************************
*
*       _CreateDevInstance
*
*/
static USBH_HID_INST * _CreateDevInstance(void) {
  USBH_HID_INST * pInst;
  int i;

  //
  // Check if max. number of sockets allowed is exceeded
  //
  if ((USBH_HID_Global.NumDevices + 1) >= NUM_DEVICES) {
    USBH_PANIC("No instance available for creating a new HID device");
  }
  //
  // Use next available socket handle.
  // Valid handles are positive integers; handles are assigned in increasing order from 1.
  // When the handle reaches a certain limit, we restart at 1.
  // Wrap around if necessary and make sure socket handle is not yet in use
  //
  i = USBH_HID_Global.NextHandle + 1;
SearchDuplicate:
  if (i >= 0xFFFF) {
    i = 1;
  }
  for (pInst = (struct USBH_HID_INST *)(USBH_HID_Global.List.Head.pFirst); pInst; pInst = (struct USBH_HID_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
    if (i == pInst->Handle) {
      i++;
      goto SearchDuplicate;
    }
  }
  USBH_HID_Global.NextHandle = i;
  //
  // We found a valid new handle!
  // Perform the actual allocation
  //
  pInst = (USBH_HID_INST *)USBH_MallocZeroed(sizeof(USBH_HID_INST));
  if (pInst) {
    USBH_DLIST_Add(&USBH_HID_Global.List.Head, (USBH_DLIST_ITEM *)pInst);
    pInst->Handle = i;
  }
  pInst->hInterface     = NULL;
  pInst->RefCnt         = 0;
  pInst->ReadErrorCount = 0;
  pInst->InterfaceID    = 0;
  pInst->IsUsed         = 1;
  pInst->DevIndex = USBH_HID_Global.NumDevices;
  USBH_HID_Global.NumDevices++;
  return pInst;
}

/*********************************************************************
*
*       _OnGeneralCompletion
*
*/
static void _OnGeneralCompletion(USBH_URB * pUrb) {
  USBH_USE_PARA(pUrb);
  USBH_OS_SetEvent((USBH_OS_EVENT_OBJ *)pUrb->Header.pContext);
}

/*********************************************************************
*
*       _UpdateLEDState
*
*  Function description:
*   Is used to send the updated LED status to the keyboard.
*
*/
static void _UpdateLEDState(USBH_HID_INST * pInst) {
  USBH_OS_EVENT_OBJ * pEvent;

  pEvent = USBH_OS_AllocEvent();
  if (pInst->OutEp) {
    pInst->OutUrb.Header.pContext                 = pEvent;
    pInst->OutUrb.Header.pfOnCompletion           = _OnGeneralCompletion;
    pInst->OutUrb.Header.Function                 = USBH_FUNCTION_INT_REQUEST;
    pInst->OutUrb.Request.BulkIntRequest.Endpoint = pInst->OutEp;
    pInst->OutUrb.Request.BulkIntRequest.pBuffer   = &USBH_HID_Global.LedState;
    pInst->OutUrb.Request.BulkIntRequest.Length   = 1;
    USBH_SubmitUrb(pInst->hInterface, &pInst->OutUrb);
  } else {
    pInst->ControlUrb.Header.pContext                      = pEvent;
    pInst->ControlUrb.Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
    pInst->ControlUrb.Header.pfOnCompletion                = _OnGeneralCompletion;
    pInst->ControlUrb.Request.ControlRequest.Setup.Type    = 0x21; // STD, IN, device
    pInst->ControlUrb.Request.ControlRequest.Setup.Request = 0x09;
    pInst->ControlUrb.Request.ControlRequest.Setup.Value   = 0x0200;
    pInst->ControlUrb.Request.ControlRequest.Setup.Index   = 0;
    pInst->ControlUrb.Request.ControlRequest.Setup.Length  = 1;
    pInst->ControlUrb.Request.ControlRequest.pBuffer        = &USBH_HID_Global.LedState;
    pInst->ControlUrb.Request.ControlRequest.Length        = 1;
    USBH_SubmitUrb(pInst->hInterface, &pInst->ControlUrb);
  }
  USBH_OS_WaitEventTimed(pEvent, 5000);
  USBH_OS_FreeEvent(pEvent);
}

/*********************************************************************
*
*       _UpdateKeyState
*
*  Function description:
*   Sends a notification the user application in order to information of
*   of a change of the keyboard status.
*/
static void _UpdateKeyState(USBH_HID_INST * pInst, unsigned Code, int Value) {
  USBH_HID_KEYBOARD_DATA   KeyData;
  KeyData.Code           = Code;
  KeyData.Value          = Value;
  if (pInst->Device.pfOnKeyStateChange) {
    pInst->Device.pfOnKeyStateChange(&KeyData);
  }
}

/*********************************************************************
*
*       _ParseKeyboardData
*
*  Function description:
*    Checks whether a change of the previously stored keyboard information
*    have been changed. If so, the call back is called in order to inform
*    user application about the change.
*
*/
static void _ParseKeyboardData(USBH_HID_INST * pInst, U8 * pNewState) {
  unsigned i;
  U8       LedState = USBH_HID_Global.LedState;
  for (i = 0; i < 8; i++) {
    if (((pNewState[0] >> i) & 1) != ((pInst->pOldState[0] >> i) & 1)) {
      _UpdateKeyState(pInst, 0xe0 + i, (pNewState[0] >> i) & 1);
    }
  }
  for (i = 2; i < 8; i++) {
    if (pInst->pOldState[i] > 3 && (_IsValueInArray(pNewState + 2, pInst->pOldState[i], 6) == 0)) {
      if (pInst->pOldState[i]) {
        _UpdateKeyState(pInst, pInst->pOldState[i], 0);
      } else {
        USBH_WARN((USBH_MTYPE_HID, "Unknown key (HID scan code %#x) released.\n", pInst->pOldState[i]));
      }
    }
    if (pNewState[i] > 3 && _IsValueInArray(pInst->pOldState + 2, pNewState[i], 6) == 0) {
      if (pNewState[i]) {
        _UpdateKeyState(pInst, pNewState[i], 1);
        //  Update
        if (pNewState[i] == 0x39) {
          LedState ^= (1 << 1);
        }
        if (pNewState[i] == 0x47) {
          LedState ^= (1 << 2);
        }
        if (pNewState[i] == 0x53) {
          LedState ^= (1 << 0);
        }
      } else {
        USBH_WARN((USBH_MTYPE_HID, "Unknown key (HID scancode %#x) released.\n", pNewState[i]));
      }
    }
  }
  if (USBH_HID_Global.LedState != LedState) {
    USBH_HID_Global.LedState = LedState;
    _UpdateLEDState(pInst);
  }
  USBH_MEMCPY(&pInst->pOldState[0], pNewState, OLD_STATE_NUMBYTES);
}

/*********************************************************************
*
*       _ParseMouseData
*/
static void _ParseMouseData(USBH_HID_INST * pInst, U8 * pNewState) {
  USBH_HID_MOUSE_DATA MouseData;
  MouseData.ButtonState = (I8)*pNewState;
  MouseData.xChange     = (I8)*(pNewState + 1);
  MouseData.yChange     = (I8)*(pNewState + 2);
  MouseData.WheelChange = (I8)*(pNewState + 3);
  if (pInst->Device.pfOnMouseStateChange) {
    pInst->Device.pfOnMouseStateChange(&MouseData);
  }
}

/*********************************************************************
*
*       _OnIntInCompletion
*
*  Function description:
*    Is called when an URB is completed.
*/
static void _OnIntInCompletion(USBH_URB * pUrb) {
  USBH_STATUS     Status;
  USBH_BOOL       DoResetEP;
  USBH_HID_INST * pInst;

  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_HID_INST * )pUrb->Header.pContext;
  DEC_HID_REF(); // To get information about pending transfer requests
  if (_CheckStateAndCloseInterface(pInst)) {
    goto End;
  }
  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    USBH_WARN((USBH_MTYPE_HID, "HID Error:  _OnIntInCompletion: App. has an error or is stopped!"));
    goto End;
  }
  DoResetEP = FALSE;
  if (pUrb->Header.Status == USBH_STATUS_SUCCESS) {
    // Dump hid pData
    if (pInst->DeviceType == HID_KEYBOARD) {
      _ParseKeyboardData(pInst, (U8 *)pUrb->Request.BulkIntRequest.pBuffer);
    } else if (pInst->DeviceType == HID_MOUSE) {
      _ParseMouseData(pInst, (U8 *)pUrb->Request.BulkIntRequest.pBuffer);
    }
    pInst->ReadErrorCount = 0; // On success clear error count
  } else {
    USBH_WARN((USBH_MTYPE_HID, "_OnIntInCompletion: URB completed with Status 0x%08X ", pUrb->Header.Status));
    pInst->ReadErrorCount++;
    if (MAX_TRANSFERS_ERRORS <= pInst->ReadErrorCount) {
      USBH_WARN((USBH_MTYPE_HID, "_OnIntInCompletion: Max error count: %d, read stopped", pInst->ReadErrorCount));
      pInst->RunningState = StateError;
    } else {
      // Reset the endpoint and resubmit an new buffer in the completion routine of the request use the same URB
      DoResetEP = TRUE;
      INC_HID_REF();
      Status = USBH_ResetEndpoint(pInst->hInterface, &pInst->InUrb, pInst->IntEp, _OnResetReadEndpointCompletion, (void * )pInst);
      if (Status != USBH_STATUS_PENDING) {
        DEC_HID_REF();
        USBH_WARN((USBH_MTYPE_HID, "_OnIntInCompletion: ResetEndpoint: (%08x), read stopped!", Status));
        pInst->RunningState = StateError;
      }
    }
  }
  if (DoResetEP) {
    return;
  }
  if ((pUrb->Header.pfOnUserCompletion == NULL) && (pInst->pInTransactionEvent) == NULL) {
    if (pInst->RunningState == StateInit || pInst->RunningState == StateRunning) {
      // Resubmit an transfer request
      Status = _SubmitInBuffer(pInst, pInst->pInBuffer, pInst->InMaxPktSize, NULL, NULL);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HID, "_OnIntInCompletion: _SubmitInBuffer: (%08x)", Status));
      }
    }
  }

End:
  if (pInst->pInTransactionEvent) {
    USBH_OS_SetEvent(pInst->pInTransactionEvent);
  }
  if (pUrb->Header.pfOnUserCompletion) {
    (pUrb->Header.pfOnUserCompletion)(pUrb->Header.pUserContext);
  }
}

/*********************************************************************
*
*       _OnOutCompletion
*
*  Function description:
*    Is called when an URB is completed.
*/
static void _OnOutCompletion(USBH_URB * pUrb) {
  USBH_HID_INST * pInst;

  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_HID_INST * )pUrb->Header.pContext;
  DEC_HID_REF(); // To get information about pending transfer requests
  if (pInst->pOutTransactionEvent) {
    USBH_OS_SetEvent(pInst->pOutTransactionEvent);
  }
  if (pUrb->Header.pfOnUserCompletion) {
    (pUrb->Header.pfOnUserCompletion)(pUrb->Header.pUserContext);
  }

}

/*********************************************************************
*
*       _GetReportDescriptor
*
*  Function description:
*    The report descriptor is the essential descriptor that is used to
*    describe the functionality of the HID device.
*    This function submits a control request in order to retrieve this
*    descriptor.
*/
static USBH_STATUS _GetReportDescriptor(USBH_HID_INST * pInst) {
  USBH_STATUS   Status;
  USBH_URB          * pURB;
  USBH_OS_EVENT_OBJ * pEvent;

  pEvent = USBH_OS_AllocEvent();
  pURB                                       = &pInst->ControlUrb;
  pURB->Header.pContext                      = pEvent;
  pURB->Header.pfOnCompletion                = _OnGeneralCompletion;
  pURB->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  pURB->Request.ControlRequest.Setup.Type    = 0x81; // STD, IN, device
  pURB->Request.ControlRequest.Setup.Request = USB_REQ_GET_DESCRIPTOR;
  pURB->Request.ControlRequest.Setup.Value   = 0x2200;
  pURB->Request.ControlRequest.Setup.Index   = pInst->DevInterfaceID;
  pURB->Request.ControlRequest.Setup.Length  = pInst->ReportDescriptorSize;
  pURB->Request.ControlRequest.pBuffer        = pInst->pReportBufferDesc;
  pURB->Request.ControlRequest.Length        = pInst->ReportDescriptorSize;
  Status                                     = USBH_SubmitUrb(pInst->hInterface, pURB);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_GetReportDescriptor: USBH_SubmitUrb (0x%08x)", Status));
    return USBH_STATUS_ERROR;
  }
  USBH_OS_WaitEventTimed(pEvent, 1000);
  USBH_OS_FreeEvent(pEvent);
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _SetDeviceIdle
*
*  Function description:
*    With a SetIdle request the device is set in a state where
*    it only sends a report when a change was recognized or a given timeout passed
*    otherwise it shall answer with NAK when no notification
*
*/
static USBH_STATUS _SetDeviceIdle(USBH_HID_INST * pInst) {
  USBH_STATUS   Status;
  USBH_URB          * pURB;
  USBH_OS_EVENT_OBJ * pEvent;

  pEvent = USBH_OS_AllocEvent();
  pURB                                       = &pInst->ControlUrb;
  pURB->Header.pContext                      = pEvent;
  pURB->Header.pfOnCompletion                = _OnGeneralCompletion;
  pURB->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  pURB->Request.ControlRequest.Setup.Type    = 0x21; // Interface, OUT, Class
  pURB->Request.ControlRequest.Setup.Request = 0x0a;
  pURB->Request.ControlRequest.Setup.Value   = 0x0000;
  pURB->Request.ControlRequest.Setup.Index   = pInst->DevInterfaceID;
  pURB->Request.ControlRequest.Setup.Length  = 0;
  pURB->Request.ControlRequest.pBuffer       = 0;
  pURB->Request.ControlRequest.Length        = 0;
  Status                                     = USBH_SubmitUrb(pInst->hInterface, pURB);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SetDeviceIdle: USBH_SubmitUrb (0x%08x)", Status));
    return USBH_STATUS_ERROR;
  }
  USBH_OS_WaitEventTimed(pEvent, 1000);
  USBH_OS_FreeEvent(pEvent);
  return USBH_STATUS_SUCCESS;
}


/**********************************************************************
*
*       _SubmitInBuffer
*
*  Function description:
*    Submits a request to the HID device.
*
*/
static USBH_STATUS _SubmitInBuffer(USBH_HID_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_HID_USER_FUNC * pfUser, void * pUserContext) {
  USBH_STATUS Status;
  pInst->InUrb.Header.pContext                 = pInst;
  pInst->InUrb.Header.pfOnCompletion           = _OnIntInCompletion;
  pInst->InUrb.Header.Function                 = USBH_FUNCTION_INT_REQUEST;
  pInst->InUrb.Header.pfOnUserCompletion       = pfUser;
  pInst->InUrb.Header.pUserContext             = pUserContext;
  pInst->InUrb.Request.BulkIntRequest.Endpoint = pInst->IntEp;
  pInst->InUrb.Request.BulkIntRequest.pBuffer   = pBuffer;
  pInst->InUrb.Request.BulkIntRequest.Length   = NumBytes;
  INC_HID_REF(); // Only for testing, counts the number of submitted URBs
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->InUrb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitBuffer: USBH_SubmitUrb (0x%08x)", Status));
    DEC_HID_REF();
  }
  return Status;
}

/**********************************************************************
*
*       _SubmitOutBuffer
*
*  Function description:
*    Submits a request to the HID device.
*    The submit operation depends whether there is an OUT-endpoint was specified
*    by the device.
*    If there is no OUT-endpoint, a control-request with the request type SET_REPORT.
*
*/
static USBH_STATUS _SubmitOutBuffer(USBH_HID_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_HID_USER_FUNC * pfUser, void * pUserContext) {
  USBH_STATUS Status;
  if (pInst->OutEp) {
    pInst->OutUrb.Header.pContext                 = pInst;
    pInst->OutUrb.Header.pfOnCompletion           = _OnOutCompletion;
    pInst->OutUrb.Header.Function                 = USBH_FUNCTION_INT_REQUEST;
    pInst->OutUrb.Header.pfOnUserCompletion       = pfUser;
    pInst->OutUrb.Header.pUserContext             = pUserContext;
    pInst->OutUrb.Request.BulkIntRequest.Endpoint = pInst->OutEp;
    pInst->OutUrb.Request.BulkIntRequest.pBuffer  = pBuffer;
    pInst->OutUrb.Request.BulkIntRequest.Length   = NumBytes;
    INC_HID_REF(); // Only for testing, counts the number of submitted URBs
    Status = USBH_SubmitUrb(pInst->hInterface, &pInst->OutUrb);
  } else {
    pInst->ControlUrb.Header.pContext                      = pInst;
    pInst->ControlUrb.Header.pfOnCompletion                = _OnOutCompletion;
    pInst->ControlUrb.Header.pfOnUserCompletion            = pfUser;
    pInst->ControlUrb.Header.pUserContext                  = pUserContext;
    pInst->ControlUrb.Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
    pInst->ControlUrb.Request.ControlRequest.Setup.Type    = 0x21; // STD, IN, device
    pInst->ControlUrb.Request.ControlRequest.Setup.Request = 0x09;
    pInst->ControlUrb.Request.ControlRequest.Setup.Value   = 0x0200 | *pBuffer;
    pInst->ControlUrb.Request.ControlRequest.Setup.Index   = pInst->DevInterfaceID;
    pInst->ControlUrb.Request.ControlRequest.Setup.Length  = (U16)NumBytes;
    pInst->ControlUrb.Request.ControlRequest.pBuffer       = pBuffer;
    pInst->ControlUrb.Request.ControlRequest.Length        = NumBytes;
    INC_HID_REF(); // Only for testing, counts the number of submitted URBs
    Status = USBH_SubmitUrb(pInst->hInterface, &pInst->ControlUrb);
  }
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitOutBuffer: USBH_SubmitUrb returned (0x%08x)", Status));
    DEC_HID_REF();
  }
  return Status;
}


/*********************************************************************
*
*       _OnResetReadEndpointCompletion
*
*  Function description:
*    Endpoint reset is complete. It submits an new URB if possible!
*/
static void _OnResetReadEndpointCompletion(USBH_URB * pUrb) {
  USBH_HID_INST * pInst;
  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_HID_INST * )pUrb->Header.pContext;
  DEC_HID_REF();
  if (_CheckStateAndCloseInterface(pInst)) {
    return;
  }
  if (pInst->RunningState == StateInit || pInst->RunningState == StateRunning) {
    // Resubmit an transfer request
    if (USBH_STATUS_SUCCESS != pUrb->Header.Status) {
      USBH_WARN((USBH_MTYPE_HID, "_OnResetReadEndpointCompletion: URB Status: 0x%08x!", pUrb->Header.Status));
      pInst->RunningState = StateError;
    } else {
      _SubmitInBuffer(pInst, pInst->pInBuffer, pInst->InMaxPktSize, NULL, NULL);
    }
  }
}

/*********************************************************************
*
*       _StopDevice
*
*  Function description:
*    Is used to stop the HID class filter.
*    It is called if the user wants to stop the class filter.
*/
static void _StopDevice(USBH_HID_INST * pInst) {
  USBH_STATUS Status;
  if (StateStop == pInst->RunningState || StateError == pInst->RunningState) {
    USBH_LOG((USBH_MTYPE_HID, "USBH_HID_Stop: app. already stopped state: %d!", pInst->RunningState));
    return;
  }
  // Stops submitting of new URBs from the application
  pInst->RunningState = StateStop;
  if (NULL == pInst->hInterface) {
    USBH_LOG((USBH_MTYPE_HID, "USBH_HID_Stop: interface handle is null, nothing to do!"));
    return;
  }
  if (pInst->RefCnt) {
    //
    // If there are any operation pending, then cancel them in order to return from those routines
    // The return value of those functions shall be USBH_STATUS_CANCELLED
    //
    Status = _CancelIO(pInst);
    if (Status) {
      USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Stop: USBH_AbortEndpoint st:0x%08x!", Status));
    }
  }
}

/*********************************************************************
*
*       _StartDevice
*
*  Function description:
*   Starts the application and is called if a USB device is connected.
*   The function uses the first interface of the device.
*
*  Parameters:
*    InterfaceID    -
*
*  Return value:
*    USBH_STATUS       -
*/
static USBH_STATUS _StartDevice(USBH_HID_INST * pInst) {
  USBH_STATUS  Status;
  USBH_EP_MASK EPMask;
  unsigned int Length;
  U8           aEpDesc[USB_ENDPOINT_DESCRIPTOR_LENGTH];
  U8           aDescriptorBuffer[255];

  // Open the hid interface
  Status = USBH_OpenInterface(pInst->InterfaceID, TRUE, &pInst->hInterface);
  if (USBH_STATUS_SUCCESS != Status) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Start: USBH_OpenInterface failed 0x%08x!", Status));
    return Status;
  }
  //
  //  Get current configuration descriptor
  //
  Length = sizeof(aDescriptorBuffer);
  USBH_GetCurrentConfigurationDescriptor(pInst->hInterface, aDescriptorBuffer, &Length);
  //
  // Get first the EP in descriptor
  //
  USBH_MEMSET(&EPMask,  0, sizeof(USBH_EP_MASK));
  EPMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  EPMask.Direction = USB_IN_DIRECTION;
  EPMask.Type      = USB_EP_TYPE_INT;
  Length = sizeof(aEpDesc);
  Status           = USBH_GetEndpointDescriptor(pInst->hInterface, 0, &EPMask, aEpDesc, &Length);
  if (Status) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Start: USBH_GetEndpointDescriptor failed st: %08x", Status));
    goto Err;
  } else {
    pInst->InMaxPktSize = (int)(aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS] + (aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS + 1] << 8));
    USBH_LOG((USBH_MTYPE_HID, "Address   Attrib.   MaxPacketSize   Interval"));
    USBH_LOG((USBH_MTYPE_HID, "0x%02X      0x%02X      %5d             %d", (int)aEpDesc[USB_EP_DESC_ADDRESS_OFS], (int)aEpDesc[USB_EP_DESC_ATTRIB_OFS], pInst->InMaxPktSize, (int)aEpDesc[USB_EP_DESC_INTERVAL_OFS]));
  }
  //
  // Now try to get the EP Out descriptor
  //
  USBH_MEMSET(&EPMask,  0, sizeof(USBH_EP_MASK));
  EPMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  EPMask.Direction = USB_OUT_DIRECTION;
  EPMask.Type      = USB_EP_TYPE_INT;
  Length = sizeof(aEpDesc);
  Status           = USBH_GetEndpointDescriptor(pInst->hInterface, 0, &EPMask, aEpDesc, &Length);
  if (Status == USBH_STATUS_SUCCESS) {
    pInst->OutMaxPktSize = (int)(aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS] + (aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS + 1] << 8));
  }
  Length = sizeof(aDescriptorBuffer);
  if (USBH_GetInterfaceDescriptor(pInst->hInterface, 0, aDescriptorBuffer, &Length) == USBH_STATUS_SUCCESS) {
    pInst->DevInterfaceID = aDescriptorBuffer[2];
  }
  switch (aDescriptorBuffer[7]) {
  case HID_DEVICE_KEYBOARD_PROTOCOL:
    pInst->DeviceType = HID_KEYBOARD;
    pInst->Device.pfOnKeyStateChange = USBH_HID_Global.pfOnKeyStateChange;
    pInst->pOldState = (U8 *)USBH_MallocZeroed(OLD_STATE_NUMBYTES);
    break;
  case HID_DEVICE_MOUSE_PROTOCOL:
    pInst->DeviceType = HID_MOUSE;
    pInst->Device.pfOnMouseStateChange = USBH_HID_Global.pfOnMouseStateChange;
    break;
  }
  Length = sizeof(aDescriptorBuffer);
  if (USBH_GetDescriptor(pInst->hInterface, 0, USB_HID_DESCRIPTOR_TYPE, &aDescriptorBuffer[0], &Length) == USBH_STATUS_SUCCESS) {
    unsigned NumDesc;
    unsigned i;

    NumDesc = aDescriptorBuffer[5];
    for (i = 0; i < NumDesc; i++) {
      if (aDescriptorBuffer[6 + i * 2] == USB_HID_DESCRIPTOR_TYPE_REPORT) {
        pInst->ReportDescriptorSize = aDescriptorBuffer[7 + i * 2];
        break;
      }
    }
  }
  pInst->pReportBufferDesc =  (U8 *)USBH_MallocZeroed(pInst->ReportDescriptorSize);
  pInst->pInBuffer         =  (U8 *)USBH_MallocZeroed(pInst->InMaxPktSize);
  if (pInst->OutMaxPktSize) {
    pInst->pOutBuffer      =  (U8 *)USBH_MallocZeroed(pInst->OutMaxPktSize);
  }
  // Setup global var.
  pInst->IntEp = aEpDesc[USB_EP_DESC_ADDRESS_OFS];
  _GetReportDescriptor(pInst);
  _SetDeviceIdle(pInst);
  // Submit URB
  if ((pInst->DeviceType == HID_MOUSE) || (pInst->DeviceType == HID_KEYBOARD)) {
    Status = _SubmitInBuffer(pInst, pInst->pInBuffer, pInst->InMaxPktSize, NULL, NULL);
    if (Status != USBH_STATUS_PENDING) { // On error
      USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Start: _SubmitBuffer failed st: %08x", Status));
      goto Err;
    } else {
      Status = USBH_STATUS_SUCCESS;
    }
  } else {
    Status = USBH_STATUS_SUCCESS;
  }
  return Status;
Err: // on error
  USBH_CloseInterface(pInst->hInterface);
  pInst->InterfaceID = 0;
  pInst->hInterface  = NULL;
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       _OnDeviceNotification
*
*  Function description:
*    TBD
*/
static void _OnDeviceNotification(USBH_HID_INST * pInst, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_STATUS Status;
  USBH_DEVICE_EVENT DeviceEvent;
  switch (Event) {
    case USBH_ADD_DEVICE:
      USBH_LOG((USBH_MTYPE_HID, "_OnDeviceNotification: USB HID device detected interface ID: %u !", InterfaceID));
      pInst->RunningState = StateInit;
      if (pInst->hInterface == NULL) {
        // Only one device is handled from the application at the same time
        pInst->InterfaceID = InterfaceID;
        Status             = _StartDevice(pInst);
        if (Status) { // On error
          pInst->RunningState = StateError;
        } else {
          pInst->RunningState = StateRunning;
        }
      }
    DeviceEvent = USBH_DEVICE_EVENT_ADD;
      break;
    case USBH_REMOVE_DEVICE:
      if (pInst->hInterface == NULL || pInst->InterfaceID != InterfaceID) {
        // Only one device is handled from the application at the same time
        return;
      }
      USBH_LOG((USBH_MTYPE_HID, "_OnDeviceNotification: USB HID device removed interface  ID: %u !", InterfaceID));
      _StopDevice(pInst);
      _CheckStateAndCloseInterface(pInst);
    DeviceEvent = USBH_DEVICE_EVENT_REMOVE;
      break;
    default:
      USBH_WARN((USBH_MTYPE_HID, "_OnDeviceNotification: invalid Event: %d !", Event));
    DeviceEvent = USBH_DEVICE_EVENT_REMOVE;
      break;
  }
  if (USBH_HID_Global.pfNotification) {
    USBH_HID_Global.pfNotification(USBH_HID_Global.pNotifyContext, pInst->DevIndex, DeviceEvent);
  }


}

/*********************************************************************
*
*       _OnGeneralDeviceNotification
*/
static void _OnGeneralDeviceNotification(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_HID_INST * pInst;

  USBH_USE_PARA(pContext);
  if (Event == USBH_ADD_DEVICE) {
    pInst = _CreateDevInstance();
    pInst->Device.pfOnMouseStateChange = USBH_HID_Global.pfOnMouseStateChange;
  } else {
    for (pInst = (struct USBH_HID_INST *)(USBH_HID_Global.List.Head.pFirst); pInst; pInst = (struct USBH_HID_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
      if (pInst->InterfaceID == InterfaceID) {
        break;
      }
    }
  }
  _OnDeviceNotification(pInst, Event, InterfaceID);
}

/*********************************************************************
*
*       _CancelOperation
*/
static USBH_STATUS _CancelOperation(USBH_HID_INST * pInst, U8 EPAddress) {
  USBH_STATUS Status;
  pInst->AbortUrb.Header.Function                  = USBH_FUNCTION_ABORT_ENDPOINT;
  pInst->AbortUrb.Header.pfOnCompletion            = NULL;
  pInst->AbortUrb.Header.pContext                  = pInst;
  pInst->AbortUrb.Request.EndpointRequest.Endpoint = EPAddress;
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->AbortUrb);
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_WARN((USBH_MTYPE_HID, "Abort failed: status: %s", USBH_GetStatusStr(Status)));
  }
  return Status;
}

/*********************************************************************
*
*       _CancelIO
*/
static USBH_STATUS _CancelIO(USBH_HID_INST * pInst) {
  USBH_MEMSET(&pInst->AbortUrb, 0, sizeof(pInst->AbortUrb));
  if (pInst->InUrb.Header.Status != USBH_STATUS_SUCCESS) {
    _CancelOperation(pInst, pInst->IntEp);
  }
  if (pInst->OutUrb.Header.Status != USBH_STATUS_SUCCESS) {
    _CancelOperation(pInst, pInst->OutEp);
  }
  if (pInst->ControlUrb.Header.Status != USBH_STATUS_SUCCESS) {
    _CancelOperation(pInst, 0);
  }
  return pInst->AbortUrb.Header.Status;
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_HID_Init
*
*  Function description:
*    Initialize the HID sub class filter.
*
*  Return value:
*    TRUE   - Success
*/
USBH_BOOL USBH_HID_Init(void) {
  USBH_PNP_NOTIFICATION   PnpNotify;
  USBH_INTERFACE_MASK   * pInterfaceMask;

  // Add an plug an play notification routine
  pInterfaceMask            = &PnpNotify.InterfaceMask;
  pInterfaceMask->Mask      = USBH_INFO_MASK_CLASS;
  pInterfaceMask->Class     = USB_DEVICE_CLASS_HUMAN_INTERFACE;
  PnpNotify.pContext         = NULL;
  PnpNotify.pfPnpNotification = _OnGeneralDeviceNotification;
  USBH_HID_Global.ControlWriteTimeout = WRITE_DEFAULT_TIMEOUT;
  USBH_HID_Global.ControlWriteRetries = 1;
  USBH_HID_Global.hDevNotification = USBH_RegisterPnPNotification(&PnpNotify); /* register HID mouse devices */
  if (NULL == USBH_HID_Global.hDevNotification) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Init: USBH_RegisterPnPNotification"));
    return FALSE;
  }
  return TRUE;
}

/*********************************************************************
*
*       USBH_HID_Exit
*
*  Function description:
*    Exit from application. Application has to wait until that all URB requests
*    completed before this function is called!
*/
void USBH_HID_Exit(void) {
  USBH_HID_INST * pInst;

  USBH_LOG((USBH_MTYPE_HID, "USBH_HID_Exit"));
  for (pInst = (struct USBH_HID_INST *)(USBH_HID_Global.List.Head.pFirst); pInst; pInst = (struct USBH_HID_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
    USBH_ASSERT(0 == pInst->RefCnt); // No pending requests
    if (pInst->hInterface != NULL) {
      USBH_CloseInterface(pInst->hInterface);
      pInst->hInterface = NULL;
    }
    USBH_HID_Global.NumDevices--;
  }
  if (USBH_HID_Global.hDevNotification != NULL) {
    USBH_UnregisterPnPNotification(USBH_HID_Global.hDevNotification);
    USBH_HID_Global.hDevNotification = NULL;
  }
  _RemoveAllInstances();
}

/*********************************************************************
*
*       USBH_HID_SetOnMouseStateChange
*
*  Function description:
*     Sets a callback whenever a mouse is attached to the device.
*
*  Parameters:
*    pfOnChange -  Callback that shall be called when a mouse change notification is available
*
*/
void USBH_HID_SetOnMouseStateChange(USBH_HID_ON_MOUSE_FUNC * pfOnChange) {
  USBH_HID_Global.pfOnMouseStateChange = pfOnChange;
}

/*********************************************************************
*
*       USBH_HID_SetOnKeyboardStateChange
*
*  Function description:
*     Sets a callback whenever a keyboard is attached to the device.
*
*  Parameters:
*    pfOnChange    - Callback that shall be called when a keyboard change notification is available
*
*/
void USBH_HID_SetOnKeyboardStateChange(USBH_HID_ON_KEYBOARD_FUNC * pfOnChange) {
  USBH_HID_Global.pfOnKeyStateChange = pfOnChange;
}



/*********************************************************************
*
*       USBH_HID_GetNumDevices
*
*  Function description:
*     Returns the number of available devices. It also retrieves
*     the information about a device.
*
*  Parameters:
*    pDevInfo    -  Pointer to a [array]
*    NumItems    -  Number of items that pDevInfo can hold.
*
*  Return value:
*    Number of devices available
*
*/
int USBH_HID_GetNumDevices(USBH_HID_DEVICE_INFO * pDevInfo, U32 NumItems) {
  USBH_HID_INST * pInst;
  HID_REPORT    * pReport  = NULL;
  unsigned        i;

  if (USBH_HID_Global.NumDevices) {
    if (pDevInfo) {
      NumItems = USBH_MIN(NumItems, USBH_HID_Global.NumDevices);
      pInst = USBH_HID_Global.List.pHidFirst;
      for (i = 0; i < NumItems; i++) {
        USBH_INTERFACE_INFO InterFaceInfo;

        USBH_GetInterfaceInfo(pInst->InterfaceID, &InterFaceInfo);
        if (USBH_DLIST_IsEmpty(&pInst->aReportEnum[USBH_HID_INPUT_REPORT].ReportList) == 0) {
          pReport = (HID_REPORT *)USBH_DLIST_GetNext(&pInst->aReportEnum[USBH_HID_INPUT_REPORT].ReportList);
          pDevInfo->InputReportSize = pReport->Size >> 3;
        } else {
          pDevInfo->InputReportSize = 0;
        }
        if (USBH_DLIST_IsEmpty(&pInst->aReportEnum[USBH_HID_OUTPUT_REPORT].ReportList) == 0) {
          pReport = (HID_REPORT *)USBH_DLIST_GetNext(&pInst->aReportEnum[USBH_HID_OUTPUT_REPORT].ReportList);
          pDevInfo->OutputReportSize = pReport->Size >> 3;
        } else {
          pDevInfo->OutputReportSize = 0;
        }
        pDevInfo->ProductId = InterFaceInfo.ProductId;
        pDevInfo->VendorId  = InterFaceInfo.VendorId;
        pDevInfo->acName[0] = 'h';
        pDevInfo->acName[1] = 'i';
        pDevInfo->acName[2] = 'd';
        pDevInfo->acName[3] = '0' +  pInst->DevIndex / 100;
        pDevInfo->acName[4] = '0' + (pInst->DevIndex % 100) / 10;
        pDevInfo->acName[5] = '0' + (pInst->DevIndex % 10);
        pDevInfo->acName[6] = 0;
        pDevInfo++;
        pInst = pInst->Next.pInst;
      }
    }
  }
  return USBH_HID_Global.NumDevices;
}


/*********************************************************************
*
*       USBH_HID_Open
*
*  Function description:
*
*
*  Parameters:
*    sName    - Pointer to a name of the device eg. hid001 for device 0.
*
*  Return value:
*    != 0     - Handle to a HID device
*    == 0     - Device not available or
*
*/
USBH_HID_HANDLE USBH_HID_Open(const char * sName) {
  USBH_HID_INST * pInst;
  USBH_HID_HANDLE Handle;
  char acName[7];
  Handle = 0;
  pInst = USBH_HID_Global.List.pHidFirst;
  acName[0] = 'h';
  acName[1] = 'i';
  acName[2] = 'd';
  acName[6] = 0;
  do {
    acName[3] = '0' +  pInst->DevIndex / 100;
    acName[4] = '0' + (pInst->DevIndex % 100) / 10;
    acName[5] = '0' + (pInst->DevIndex % 10);
    if (strcmp(sName, acName) == 0) {
      //
      // Device found
      //
      Handle = pInst->Handle;
      pInst->IsOpened = 1;
      break;
    }
    pInst = pInst->Next.pInst;
  } while (pInst);
  return Handle;
}

/*********************************************************************
*
*       USBH_HID_GetDeviceInfo
*
*  Function description:
*     Retrieves information about an opened HID device
*
*  Parameters:
*    hDevice     -  Handle to an opened HID device.
*    pDevInfo    -  Pointer to a USBH_HID_DEVICE_INFO buffer.
*
*  Return value:
*   USBH_STATUS_INVALID_DESCRIPTOR  - The report descriptor could not be parsed
*   USBH_STATUS_MEMORY              - Insufficient memory
*   USBH_STATUS_INVALID_PARAM       - Invalid handle was passed
*   USBH_STATUS_SUCCESS             - Success
*
*/
USBH_STATUS USBH_HID_GetDeviceInfo(USBH_HID_HANDLE hDevice, USBH_HID_DEVICE_INFO * pDevInfo) {
  USBH_HID_INST       * pInst    = NULL;
  HID_REPORT          * pReport  = NULL;
  USBH_INTERFACE_INFO   InterFaceInfo;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->ReportDescriptorParsed == 0) {
      USBH_STATUS Status;

      Status = _ParseReport(pInst, pInst->pReportBufferDesc, pInst->ReportDescriptorSize);
      if (Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HID, "Parsing of report descriptor failed."));
        return Status;
      }
    }
    if (USBH_DLIST_IsEmpty(&pInst->aReportEnum[USBH_HID_INPUT_REPORT].ReportList) == 0) {
      pReport = (HID_REPORT *)USBH_DLIST_GetNext(&pInst->aReportEnum[USBH_HID_INPUT_REPORT].ReportList);
      pDevInfo->InputReportSize = pReport->Size >> 3;
    } else {
      pDevInfo->InputReportSize = 0;
    }
    if (USBH_DLIST_IsEmpty(&pInst->aReportEnum[USBH_HID_OUTPUT_REPORT].ReportList) == 0) {
      pReport = (HID_REPORT *)USBH_DLIST_GetNext(&pInst->aReportEnum[USBH_HID_OUTPUT_REPORT].ReportList);
      pDevInfo->OutputReportSize = pReport->Size >> 3;
    } else {
      pDevInfo->OutputReportSize = 0;
    }
    USBH_GetInterfaceInfo(pInst->InterfaceID, &InterFaceInfo);
    pDevInfo->ProductId = InterFaceInfo.ProductId;
    pDevInfo->VendorId  = InterFaceInfo.VendorId;
    return USBH_STATUS_SUCCESS;

  }
  return USBH_STATUS_INVALID_PARAM;
}

/*********************************************************************
*
*       USBH_HID_GetReportDescriptorParsed
*
*  Function description:
*     Contains a simplified structure that contains the information which
*     reports are available.
*
*  Parameters:
*    hDevice        -  Handle to an open HID device.
*    pReportInfo    -  Pointer to a [array] USBH_HID_REPORT_INFO buffer
*    pNumEntries    -  [IN]  Number of USBH_HID_REPORT_INFO items can store
*                      [OUT] Number of reports that are available
*  Return value:
*   USBH_STATUS_INVALID_DESCRIPTOR  - The report descriptor could not be parsed
*   USBH_STATUS_MEMORY              - Insufficent memory
*   USBH_STATUS_INVALID_PARAM       - Invalid handle was passed
*   USBH_STATUS_SUCCESS             - Success
*
*/
USBH_STATUS USBH_HID_GetReportDescriptorParsed(USBH_HID_HANDLE hDevice, USBH_HID_REPORT_INFO * pReportInfo, unsigned * pNumEntries) {
  USBH_HID_INST       * pInst    = NULL;
  HID_REPORT          * pReport  = NULL;
  unsigned NumEntries;
  unsigned i;

  pInst = _h2p(hDevice);
  NumEntries = *pNumEntries;
  if (pInst) {
    HID_REPORT_ENUM  * pReportEnum;

    if (pInst->ReportDescriptorParsed == 0) {
      USBH_STATUS Status;

      Status = _ParseReport(pInst, pInst->pReportBufferDesc, pInst->ReportDescriptorSize);
      if (Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HID, "Parsing of report descriptor failed."));
        return Status;
      }
    }
    pReportEnum = &pInst->aReportEnum[USBH_HID_INPUT_REPORT];
    for (i = 0; i < USBH_COUNTOF(pReportEnum->apReportIdHash); i++) {
      pReport = pReportEnum->apReportIdHash[i];
      if (pReport) {
        pReportInfo->ReportType = USBH_HID_INPUT_REPORT;
        pReportInfo->ReportId   = pReport->Id;
        pReportInfo->ReportSize = pReport->Size >> 3;
        NumEntries--;
        pReportInfo++;
      }
      if (NumEntries) {
        break;
      }
    }
    if (NumEntries) {
      pReportEnum = &pInst->aReportEnum[USBH_HID_OUTPUT_REPORT];
      for (i = 0; i < USBH_COUNTOF(pReportEnum->apReportIdHash); i++) {
        pReport = pReportEnum->apReportIdHash[i];
        if (pReport) {
          pReportInfo->ReportType = USBH_HID_OUTPUT_REPORT;
          pReportInfo->ReportId   = pReport->Id;
          pReportInfo->ReportSize = pReport->Size >> 3;
          NumEntries--;
          pReportInfo++;
        }
        if (NumEntries) {
          break;
        }
      }
    }
    return USBH_STATUS_SUCCESS;
  }
  return USBH_STATUS_INVALID_PARAM;
}

/*********************************************************************
*
*       USBH_HID_GetReportDescriptorParsed
*
*  Function description:
*     Simply returns the raw report descriptor.
*
*  Parameters:
*    hDevice            -  Handle to an opened device
*    pReportDescriptor  -  Pointer to a buffer that shall contain the report descriptor
*    NumBytes           -  Size of the buffer in bytes.
*
*  Return value:
*    USBH_STATUS_SUCCESS        - Success
*    USBH_STATUS_INVALID_PARAM  - An invalid handle was passed to the function
*
*/
USBH_STATUS USBH_HID_GetReportDescriptor(USBH_HID_HANDLE hDevice, U8 * pReportDescriptor, unsigned NumBytes) {
  USBH_HID_INST * pInst;
  unsigned        NumBytes2Copy;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->pReportBufferDesc) {
      NumBytes2Copy = USBH_MIN(NumBytes, pInst->ReportDescriptorSize);
      USBH_MEMCPY(pReportDescriptor, pInst->pReportBufferDesc, NumBytes2Copy);
      return USBH_STATUS_SUCCESS;
    }
  }
  return USBH_STATUS_INVALID_PARAM;
}


/*********************************************************************
*
*       USBH_HID_GetReport
*
*  Function description:
*    Receives a report from the device.
*
*  Parameters:
*    hDevice       - Handle to an opened HID device
*    pBuffer       - Pointer to a buffer to read
*    BufferSize    - Size of the buffer
*    pfFunc        - [Optional] Pointer to a function to be called when operation finished
*    pContext      - [Optional] Pointer to user-application context
*
*  Return value:
*    USBH_STATUS_SUCCESS        - Success
*    USBH_STATUS_INVALID_PARAM  - An invalid handle was passed to the function
*    USBH_STATUS_PENDING        - Request was submitted and application is informed via callback.
*    Any other value means error
*
*/
USBH_STATUS USBH_HID_GetReport(USBH_HID_HANDLE hDevice, U8 * pBuffer, U32 BufferSize, USBH_HID_USER_FUNC * pfFunc, void * pContext) {
  USBH_HID_INST * pInst;
  USBH_STATUS     Status;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      if (pfFunc) {
        Status = _SubmitInBuffer(pInst, pBuffer, BufferSize, pfFunc, pContext);
      } else {
        pInst->pInTransactionEvent = USBH_OS_AllocEvent();
        _SubmitInBuffer(pInst, pBuffer, BufferSize, NULL, NULL);
        USBH_OS_WaitEvent(pInst->pInTransactionEvent);
        USBH_OS_FreeEvent(pInst->pInTransactionEvent);
        pInst->pInTransactionEvent = NULL;
        Status = pInst->InUrb.Header.Status;
      }
      return Status;
    }
  }
  return USBH_STATUS_INVALID_PARAM;
}

/*********************************************************************
*
*       USBH_HID_SetReport
*
*  Function description:
*    Sends out a report to the device.
*
*  Parameters:
*    hDevice       - Handle to an opened HID device
*    pBuffer       - Pointer to a buffer to send, please make sure
*                    that the first byte of buffer contains the report id to send.
*                    This is elementary important for sending the report via a control request.
*    BufferSize    - Size of the buffer
*    pfFunc        - [Optional] Pointer to a function to be called when operation finished
*    pContext      - [Optional] Pointer to user-application context
*
*  Return value:
*    USBH_STATUS_SUCCESS        - Success
*    USBH_STATUS_INVALID_PARAM  - An invalid handle was passed to the function
*    USBH_STATUS_PENDING        - Request was submitted and application is informed via callback.
*    Any other value means error
*
*/
USBH_STATUS USBH_HID_SetReport(USBH_HID_HANDLE hDevice, const U8 * pBuffer, U32 BufferSize, USBH_HID_USER_FUNC * pfFunc, void * pContext) {
  USBH_HID_INST * pInst;
  USBH_STATUS     Status;
  unsigned        NumRetries;
  pInst = _h2p(hDevice);
  if (pInst) {
    NumRetries = USBH_HID_Global.ControlWriteRetries;
    if (pInst->IsOpened) {
      if (pfFunc) {
        Status = _SubmitOutBuffer(pInst, (U8 *)pBuffer, BufferSize, pfFunc, pContext);
      } else {
        pInst->pOutTransactionEvent = USBH_OS_AllocEvent();
Retry:
        _SubmitOutBuffer(pInst, (U8 *)pBuffer, BufferSize, NULL, NULL);
        if ((pInst->OutEp == 0) && (USBH_HID_Global.ControlWriteTimeout != 0)) {
          if (USBH_OS_WaitEventTimed(pInst->pOutTransactionEvent, USBH_HID_Global.ControlWriteTimeout) != USBH_OS_EVENT_SIGNALED) {
            Status = _CancelOperation(pInst, 0);
            if (Status) {
              USBH_LOG((USBH_MTYPE_HID, "HID: _SubmitUrbAndWait: Cancel operation status: 0x%08x",Status));
            }
            USBH_OS_WaitEvent(pInst->pOutTransactionEvent);
            if (--NumRetries) {
              USBH_OS_ResetEvent(pInst->pOutTransactionEvent);
              goto Retry;
            } else {
              Status = USBH_STATUS_TIMEOUT;
            }
          } else {
            if (pInst->OutEp) {
              Status = pInst->OutUrb.Header.Status;
            } else {
              Status = pInst->ControlUrb.Header.Status;
            }
          }
        } else {
        USBH_OS_WaitEvent(pInst->pOutTransactionEvent);
         Status = pInst->OutUrb.Header.Status;
        }
        USBH_OS_FreeEvent(pInst->pOutTransactionEvent);
        pInst->pOutTransactionEvent = NULL;
      }
      return Status;
    }
  }
  return USBH_STATUS_INVALID_PARAM;
}

/*********************************************************************
*
*       USBH_HID_CancelIo
*
*  Function description:
*    Cancels any pending read/write operation.
*
*  Parameters:
*    hDevice    - Handle to the HID device
*
*  Return value:
*    USBH_STATUS_SUCCESS   - Operation successfully canceled
*    Any other value means error
*
*/
USBH_STATUS USBH_HID_CancelIo(USBH_HID_HANDLE hDevice) {
  USBH_HID_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      return _CancelIO(pInst);
    }
  }
  return USBH_STATUS_INVALID_PARAM;
}

/*********************************************************************
*
*       USBH_HID_Close
*
*  Function description:
*    Closes a handle to opened HID device.
*
*  Parameters:
*    hDevice    -  Handle to the opened device
*
*  Return value:
*    == 0          - Success
*    != 0          - Error
*
*/
USBH_STATUS USBH_HID_Close(USBH_HID_HANDLE hDevice) {
  USBH_HID_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    //
    // Cancel all pending operations.
    // This makes sure that we leave the device
    // in a normal state after a close
    //
    _CancelIO(pInst);
    pInst->IsOpened = 0;
    return USBH_STATUS_SUCCESS;
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_HID_RegisterNotification
*
*  Function description:
*    Registers a notification call back in order to inform user about
*    adding or removing a device.
*
*  Parameters:
*    pfNotification   - Pointer to the call back that shall be called.
*    pContext         - Pointer to an optional context that may be used with
*                       call back.
*  
*/
void USBH_HID_RegisterNotification(USBH_NOTIFICATION_FUNC * pfNotification, void * pContext) {
  USBH_HID_Global.pfNotification = pfNotification;
  USBH_HID_Global.pNotifyContext = pContext;
}

/*********************************************************************
*
*       USBH_HID_ConfigureControlWriteTimeout
*
*  Function description:
*    Setup the time-out that shall be used during a SET_REPORT to the device.
*
*
*  Parameters:
*    Timeout   - Time in ms the SetReport shall wait.
*
*/
void USBH_HID_ConfigureControlWriteTimeout(U32 Timeout) {
  USBH_HID_Global.ControlWriteTimeout = Timeout;
}

/*********************************************************************
*
*       USBH_HID_ConfigureControlRetries
*
*  Function description:
*    Setup the time-out that shall be used during a SET_REPORT to the device.
*
*
*  Parameters:
*    Timeout   - Time in ms the SetReport shall wait.
*
*/
void USBH_HID_ConfigureControlWriteRetries(U8 NumRetries) {
  USBH_HID_Global.ControlWriteRetries = NumRetries;
}

/*************************** EOF ************************************/
