/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_DebugStrings.c
Purpose     : USB host  debug strings
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stdlib.h>
#include "USBH_Int.h"

/*********************************************************************
*
*       USBH_HcState2Str
*
*  Function description
*/
const char * USBH_HcState2Str(HOST_CONTROLLER_STATE x) {
  return (
    USBH_ENUM_TO_STR(HC_UNKNOWN) :
    USBH_ENUM_TO_STR(HC_WORKING) :
    USBH_ENUM_TO_STR(HC_REMOVED) :
    USBH_ENUM_TO_STR(HC_SUSPEND) :
    "unknown HC state"
  );
}

/*********************************************************************
*
*       USBH_RhPortResetState2Str
*
*  Function description
*/
const char * USBH_RhPortResetState2Str(USBH_ROOT_HUB_PORTRESET_STATE x) {
  return (
    USBH_ENUM_TO_STR(RH_PORTRESET_IDLE) :
    USBH_ENUM_TO_STR(RH_PORTRESET_START) :
    USBH_ENUM_TO_STR(RH_PORTRESET_RES) :
    USBH_ENUM_TO_STR(RH_PORTRESET_INIT) :
    USBH_ENUM_TO_STR(RH_PORTRESET_WAIT_RESET) :
    USBH_ENUM_TO_STR(RH_PORTRESET_SET_ADDRESS) :
    USBH_ENUM_TO_STR(RH_PORTRESET_WAIT_ADDRESS) :
    USBH_ENUM_TO_STR(RH_PORTRESET_RESTART) :
    USBH_ENUM_TO_STR(RH_PORTRESET_WAIT_RESTART) :
    USBH_ENUM_TO_STR(RH_PORTRESET_REMOVED) :
      "unknown root hub enum state"
    );
}

/*********************************************************************
*
*       USBH_EnumState2Str
*
*  Function description
*/
const char * USBH_EnumState2Str(DEV_ENUM_STATE x) {
  return (
    USBH_ENUM_TO_STR(DEV_ENUM_IDLE):
    USBH_ENUM_TO_STR(DEV_ENUM_START):
    USBH_ENUM_TO_STR(DEV_ENUM_GET_DEVICE_DESC_PART):
    USBH_ENUM_TO_STR(DEV_ENUM_GET_DEVICE_DESC):
    USBH_ENUM_TO_STR(DEV_ENUM_GET_CONFIG_DESC_PART):
    USBH_ENUM_TO_STR(DEV_ENUM_GET_CONFIG_DESC):
    USBH_ENUM_TO_STR(DEV_ENUM_GET_LANG_ID):
    USBH_ENUM_TO_STR(DEV_ENUM_GET_SERIAL_DESC):
    USBH_ENUM_TO_STR(DEV_ENUM_SET_CONFIGURATION):
    USBH_ENUM_TO_STR(DEV_ENUM_INIT_HUB):
    USBH_ENUM_TO_STR(DEV_ENUM_RESTART):
    USBH_ENUM_TO_STR(DEV_ENUM_REMOVED):
      "unknown enum state"
    );
}

/*********************************************************************
*
*       USBH_HubEnumState2Str
*
*  Function description
*/
const char * USBH_HubEnumState2Str(USBH_HUB_ENUM_STATE x) {
  return (
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_IDLE):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_START):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_GET_STATUS):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_HUB_DESC):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_SET_POWER):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_POWER_GOOD):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_PORT_STATE):
    USBH_ENUM_TO_STR(USBH_HUB_ENUM_ADD_DEVICE):
      "unknown hub init state"
    );
}

/*********************************************************************
*
*       USBH_HubNotificationState2Str
*
*  Function description
*/
const char * USBH_HubNotificationState2Str(USBH_HUB_NOTIFY_STATE x) {
  return (
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_IDLE):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_START):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_GET_HUB_STATUS):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_CLEAR_HUB_STATUS):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_GET_PORT_STATUS):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_CLR_PORT_STATUS):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_CHECK_OVER_CURRENT):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_CHECK_CONNECT):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_CHECK_REMOVE):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_DISABLE_PORT):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_REMOVED):
    USBH_ENUM_TO_STR(USBH_HUB_NOTIFY_ERROR):
      "unknown hub notify state"
    );
}

/*********************************************************************
*
*       USBH_HubPortResetState2Str
*
*  Function description
*/
const char * USBH_HubPortResetState2Str(USBH_HUB_PORTRESET_STATE x) {
  return (
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_IDLE):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_START):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_RESTART):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_WAIT_RESTART):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_RES):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_IS_ENABLED):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_WAIT_RESET):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_SET_ADDRESS):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_WAIT_SET_ADDRESS):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_START_DEVICE_ENUM):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_DISABLE_PORT):
    USBH_ENUM_TO_STR(USBH_HUB_PORTRESET_REMOVED):
    "unknown hub port state"
    );
}

/*********************************************************************
*
*       USBH_UrbFunction2Str
*
*  Function description
*/
const char * USBH_UrbFunction2Str(USBH_FUNCTION x) {
  return (
    USBH_ENUM_TO_STR(USBH_FUNCTION_CONTROL_REQUEST) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_BULK_REQUEST) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_INT_REQUEST) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_ISO_REQUEST) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_RESET_DEVICE) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_RESET_ENDPOINT) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_ABORT_ENDPOINT) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_SET_CONFIGURATION) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_SET_INTERFACE) :
    USBH_ENUM_TO_STR(USBH_FUNCTION_SET_POWER_STATE) :
      "unknown USBH function code"
    );
}

/*********************************************************************
*
*       USBH_PortState2Str
*
*  Function description
*/
const char * USBH_PortState2Str(PORT_STATE x) {
  return (
    USBH_ENUM_TO_STR(PORT_UNKNOWN):
    USBH_ENUM_TO_STR(PORT_REMOVED):
    USBH_ENUM_TO_STR(PORT_CONNECTED):
    USBH_ENUM_TO_STR(PORT_RESTART):
    USBH_ENUM_TO_STR(PORT_SUSPEND):
    USBH_ENUM_TO_STR(PORT_RESET):
    USBH_ENUM_TO_STR(PORT_ENABLED):
    USBH_ENUM_TO_STR(PORT_ERROR):
    "unknown port state state"
    );
}

/*********************************************************************
*
*       USBH_PortSpeed2Str
*
*  Function description
*/
const char * USBH_PortSpeed2Str(USBH_SPEED x) {
  return (
    USBH_ENUM_TO_STR(USBH_SPEED_UNKNOWN):
    USBH_ENUM_TO_STR(USBH_LOW_SPEED):
    USBH_ENUM_TO_STR(USBH_FULL_SPEED):
    USBH_ENUM_TO_STR(USBH_HIGH_SPEED):
      "unknown port speed"
    );
}

/*********************************************************************
*
*       USBH_GetStatusStr
*
*  Function description
*    Returns the status as a string constant
*/
const char * USBH_GetStatusStr(USBH_STATUS x) {
  return (
    USBH_ENUM_TO_STR(USBH_STATUS_SUCCESS            ) :
    USBH_ENUM_TO_STR(USBH_STATUS_CRC                ) :
    USBH_ENUM_TO_STR(USBH_STATUS_BITSTUFFING        ) :
    USBH_ENUM_TO_STR(USBH_STATUS_DATATOGGLE         ) :
    USBH_ENUM_TO_STR(USBH_STATUS_STALL              ) :
    USBH_ENUM_TO_STR(USBH_STATUS_NOTRESPONDING      ) :
    USBH_ENUM_TO_STR(USBH_STATUS_PID_CHECK          ) :
    USBH_ENUM_TO_STR(USBH_STATUS_UNEXPECTED_PID     ) :
    USBH_ENUM_TO_STR(USBH_STATUS_DATA_OVERRUN       ) :
    USBH_ENUM_TO_STR(USBH_STATUS_DATA_UNDERRUN      ) :
    USBH_ENUM_TO_STR(USBH_STATUS_BUFFER_OVERRUN     ) :
    USBH_ENUM_TO_STR(USBH_STATUS_BUFFER_UNDERRUN    ) :
    USBH_ENUM_TO_STR(USBH_STATUS_NOT_ACCESSED       ) :
    USBH_ENUM_TO_STR(USBH_STATUS_MAX_HARDWARE_ERROR ) :
    USBH_ENUM_TO_STR(USBH_STATUS_ERROR              ) :
    USBH_ENUM_TO_STR(USBH_STATUS_BUFFER_OVERFLOW    ) :
    USBH_ENUM_TO_STR(USBH_STATUS_INVALID_PARAM      ) :
    USBH_ENUM_TO_STR(USBH_STATUS_PENDING            ) :
    USBH_ENUM_TO_STR(USBH_STATUS_DEVICE_REMOVED     ) :
    USBH_ENUM_TO_STR(USBH_STATUS_CANCELED           ) :
    USBH_ENUM_TO_STR(USBH_STATUS_BUSY               ) :
    USBH_ENUM_TO_STR(USBH_STATUS_INVALID_DESCRIPTOR ) :
    USBH_ENUM_TO_STR(USBH_STATUS_ENDPOINT_HALTED    ) :
    USBH_ENUM_TO_STR(USBH_STATUS_TIMEOUT            ) :
    USBH_ENUM_TO_STR(USBH_STATUS_PORT               ) :
    USBH_ENUM_TO_STR(USBH_STATUS_LENGTH             ) :
    USBH_ENUM_TO_STR(USBH_STATUS_COMMAND_FAILED     ) :
    USBH_ENUM_TO_STR(USBH_STATUS_INTERFACE_PROTOCOL ) :
    USBH_ENUM_TO_STR(USBH_STATUS_INTERFACE_SUB_CLASS) :
    USBH_ENUM_TO_STR(USBH_STATUS_SENSE_STOP         ) :
    USBH_ENUM_TO_STR(USBH_STATUS_SENSE_REPEAT       ) :
    USBH_ENUM_TO_STR(USBH_STATUS_WRITE_PROTECT      ) :
    USBH_ENUM_TO_STR(USBH_STATUS_INVALID_ALIGNMENT  ) :
    USBH_ENUM_TO_STR(USBH_STATUS_MEMORY             ) :
    USBH_ENUM_TO_STR(USBH_STATUS_RESOURCES          ) :
    "unknown status"
  );
}


/*********************************************************************
*
*       USBH_GetStatusStr
*
*  Function description
*    Returns the status as a string constant
*/
const char * USBH_Ep0State2Str(USBH_EP0_PHASE x) {
  return (
    USBH_ENUM_TO_STR(ES_IDLE) :
    USBH_ENUM_TO_STR(ES_SETUP) :
    USBH_ENUM_TO_STR(ES_COPY_DATA) :
    USBH_ENUM_TO_STR(ES_DATA) :
    USBH_ENUM_TO_STR(ES_PROVIDE_HANDSHAKE) :
    USBH_ENUM_TO_STR(ES_HANDSHAKE) :
    USBH_ENUM_TO_STR(ES_ERROR) :
      "unknown enum state!"
    );
}


/******************************* EOF ********************************/
