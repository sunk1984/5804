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
*       UbdHcStateStr
*
*  Function description
*/
const char * UbdHcStateStr(HOST_CONTROLLER_STATE x) {
  return (
    TB_ENUMSTR(HC_UNKNOWN) :
    TB_ENUMSTR(HC_WORKING) :
    TB_ENUMSTR(HC_REMOVED) :
    TB_ENUMSTR(HC_SUSPEND) :
    "unknown HC state"
  );
}

/*********************************************************************
*
*       UbdRhPortResetStateStr
*
*  Function description
*/
const char * UbdRhPortResetStateStr(RH_PORTRESET_STATE x) {
  return (
    TB_ENUMSTR(RH_PORTRESET_IDLE) :
    TB_ENUMSTR(RH_PORTRESET_START) :
    TB_ENUMSTR(RH_PORTRESET_RES) :
    TB_ENUMSTR(RH_PORTRESET_INIT) :
    TB_ENUMSTR(RH_PORTRESET_WAIT_RESET) :
    TB_ENUMSTR(RH_PORTRESET_SET_ADDRESS) :
    TB_ENUMSTR(RH_PORTRESET_WAIT_ADDRESS) :
    TB_ENUMSTR(RH_PORTRESET_RESTART) :
    TB_ENUMSTR(RH_PORTRESET_WAIT_RESTART) :
    TB_ENUMSTR(RH_PORTRESET_REMOVED) :
      "unknown enum state"
    );
}

/*********************************************************************
*
*       UbdEnumStateStr
*
*  Function description
*/
const char * UbdEnumStateStr(DEV_ENUM_STATE x) {
  return (
    TB_ENUMSTR(DEV_ENUM_IDLE):
    TB_ENUMSTR(DEV_ENUM_START):
    TB_ENUMSTR(DEV_ENUM_GET_DEVICE_DESC_PART):
    TB_ENUMSTR(DEV_ENUM_GET_DEVICE_DESC):
    TB_ENUMSTR(DEV_ENUM_GET_CONFIG_DESC_PART):
    TB_ENUMSTR(DEV_ENUM_GET_CONFIG_DESC):
    TB_ENUMSTR(DEV_ENUM_GET_LANG_ID):
    TB_ENUMSTR(DEV_ENUM_GET_SERIAL_DESC):
    TB_ENUMSTR(DEV_ENUM_SET_CONFIGURATION):
    TB_ENUMSTR(DEV_ENUM_INIT_HUB):
    TB_ENUMSTR(DEV_ENUM_RESTART):
    TB_ENUMSTR(DEV_ENUM_REMOVED):
      "unknown enum state"
    );
}

#if USBH_EXTHUB_SUPPORT

/*********************************************************************
*
*       UbdHubEnumStateStr
*
*  Function description
*/
const char * UbdHubEnumStateStr(HUB_ENUM_STATE x) {
  return (
    TB_ENUMSTR(HUB_ENUM_IDLE):
    TB_ENUMSTR(HUB_ENUM_START):
    TB_ENUMSTR(HUB_ENUM_GET_STATUS):
    TB_ENUMSTR(HUB_ENUM_HUB_DESC):
    TB_ENUMSTR(HUB_ENUM_SET_POWER):
    TB_ENUMSTR(HUB_ENUM_POWER_GOOD):
    TB_ENUMSTR(HUB_ENUM_PORT_STATE):
    TB_ENUMSTR(HUB_ENUM_ADD_DEVICE):
      "unknown hub init state"
    );
}

/*********************************************************************
*
*       UbdHubNotificationStateStr
*
*  Function description
*/
const char * UbdHubNotificationStateStr(HUB_NOTIFY_STATE x) {
  return (
    TB_ENUMSTR(HUB_NOTIFY_IDLE):
    TB_ENUMSTR(HUB_NOTIFY_START):
    TB_ENUMSTR(HUB_NOTIFY_GET_HUB_STATUS):
    TB_ENUMSTR(HUB_NOTIFY_CLEAR_HUB_STATUS):
    TB_ENUMSTR(HUB_NOTIFY_GET_PORT_STATUS):
    TB_ENUMSTR(HUB_NOTIFY_CLR_PORT_STATUS):
    TB_ENUMSTR(HUB_NOTIFY_CHECK_OVER_CURRENT):
    TB_ENUMSTR(HUB_NOTIFY_CHECK_CONNECT):
    TB_ENUMSTR(HUB_NOTIFY_CHECK_REMOVE):
    TB_ENUMSTR(HUB_NOTIFY_DISABLE_PORT):
    TB_ENUMSTR(HUB_NOTIFY_REMOVED):
    TB_ENUMSTR(HUB_NOTIFY_ERROR):
      "unknown hub notify state"
    );
}

/*********************************************************************
*
*       UbdHubPortResetStateStr
*
*  Function description
*/
const char * UbdHubPortResetStateStr(HUB_PORTRESET_STATE x) {
  return (
    TB_ENUMSTR(HUB_PORTRESET_IDLE):
    TB_ENUMSTR(HUB_PORTRESET_START):
    TB_ENUMSTR(HUB_PORTRESET_RESTART):
    TB_ENUMSTR(HUB_PORTRESET_WAIT_RESTART):
    TB_ENUMSTR(HUB_PORTRESET_RES):
    TB_ENUMSTR(HUB_PORTRESET_IS_ENABLED):
    TB_ENUMSTR(HUB_PORTRESET_WAIT_RESET):
    TB_ENUMSTR(HUB_PORTRESET_SET_ADDRESS):
    TB_ENUMSTR(HUB_PORTRESET_WAIT_SET_ADDRESS):
    TB_ENUMSTR(HUB_PORTRESET_START_DEVICE_ENUM):
    TB_ENUMSTR(HUB_PORTRESET_DISABLE_PORT):
    TB_ENUMSTR(HUB_PORTRESET_REMOVED):
      "unknown hub enum state"
    );
}

#endif // USBH_EXTHUB_SUPPORT

/*********************************************************************
*
*       UbdUrbFunctionStr
*
*  Function description
*/
const char * UbdUrbFunctionStr(USBH_FUNCTION x) {
  return (
    TB_ENUMSTR(USBH_FUNCTION_CONTROL_REQUEST) :
    TB_ENUMSTR(USBH_FUNCTION_BULK_REQUEST) :
    TB_ENUMSTR(USBH_FUNCTION_INT_REQUEST) :
    TB_ENUMSTR(USBH_FUNCTION_ISO_REQUEST) :
    TB_ENUMSTR(USBH_FUNCTION_RESET_DEVICE) :
    TB_ENUMSTR(USBH_FUNCTION_RESET_ENDPOINT) :
    TB_ENUMSTR(USBH_FUNCTION_ABORT_ENDPOINT) :
    TB_ENUMSTR(USBH_FUNCTION_SET_CONFIGURATION) :
    TB_ENUMSTR(USBH_FUNCTION_SET_INTERFACE) :
    TB_ENUMSTR(USBH_FUNCTION_SET_POWER_STATE) :
      "unknown USBH function code"
    );
}

/*********************************************************************
*
*       UbdPortStateStr
*
*  Function description
*/
const char * UbdPortStateStr(PORT_STATE x) {
  return (
    TB_ENUMSTR(PORT_UNKNOWN):
    TB_ENUMSTR(PORT_REMOVED):
    TB_ENUMSTR(PORT_CONNECTED):
    TB_ENUMSTR(PORT_RESTART):
    TB_ENUMSTR(PORT_SUSPEND):
    TB_ENUMSTR(PORT_RESET):
    TB_ENUMSTR(PORT_ENABLED):
    TB_ENUMSTR(PORT_ERROR):
      "unknown port state state"
    );
}

/*********************************************************************
*
*       UbdPortSpeedStr
*
*  Function description
*/
const char * UbdPortSpeedStr(USBH_SPEED x) {
  return (
    TB_ENUMSTR(USBH_SPEED_UNKNOWN):
    TB_ENUMSTR(USBH_LOW_SPEED):
    TB_ENUMSTR(USBH_FULL_SPEED):
    TB_ENUMSTR(USBH_HIGH_SPEED):
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
TB_ENUMSTR(USBH_STATUS_SUCCESS            ) :
TB_ENUMSTR(USBH_STATUS_CRC                ) :
TB_ENUMSTR(USBH_STATUS_BITSTUFFING        ) :
TB_ENUMSTR(USBH_STATUS_DATATOGGLE         ) :
TB_ENUMSTR(USBH_STATUS_STALL              ) :
TB_ENUMSTR(USBH_STATUS_NOTRESPONDING      ) :
TB_ENUMSTR(USBH_STATUS_PID_CHECK          ) :
TB_ENUMSTR(USBH_STATUS_UNEXPECTED_PID     ) :
TB_ENUMSTR(USBH_STATUS_DATA_OVERRUN       ) :
TB_ENUMSTR(USBH_STATUS_DATA_UNDERRUN      ) :
TB_ENUMSTR(USBH_STATUS_BUFFER_OVERRUN     ) :
TB_ENUMSTR(USBH_STATUS_BUFFER_UNDERRUN    ) :
TB_ENUMSTR(USBH_STATUS_NOT_ACCESSED       ) :
TB_ENUMSTR(USBH_STATUS_MAX_HARDWARE_ERROR ) :
TB_ENUMSTR(USBH_STATUS_ERROR              ) :
TB_ENUMSTR(USBH_STATUS_BUFFER_OVERFLOW    ) :
TB_ENUMSTR(USBH_STATUS_INVALID_PARAM      ) :
TB_ENUMSTR(USBH_STATUS_PENDING            ) :
TB_ENUMSTR(USBH_STATUS_DEVICE_REMOVED     ) :
TB_ENUMSTR(USBH_STATUS_CANCELED           ) :
TB_ENUMSTR(USBH_STATUS_BUSY               ) :
TB_ENUMSTR(USBH_STATUS_INVALID_DESCRIPTOR ) :
TB_ENUMSTR(USBH_STATUS_ENDPOINT_HALTED    ) :
TB_ENUMSTR(USBH_STATUS_TIMEOUT            ) :
TB_ENUMSTR(USBH_STATUS_PORT               ) :
TB_ENUMSTR(USBH_STATUS_LENGTH             ) :
TB_ENUMSTR(USBH_STATUS_COMMAND_FAILED     ) :
TB_ENUMSTR(USBH_STATUS_INTERFACE_PROTOCOL ) :
TB_ENUMSTR(USBH_STATUS_INTERFACE_SUB_CLASS) :
TB_ENUMSTR(USBH_STATUS_SENSE_STOP         ) :
TB_ENUMSTR(USBH_STATUS_SENSE_REPEAT       ) :
TB_ENUMSTR(USBH_STATUS_WRITE_PROTECT      ) :
TB_ENUMSTR(USBH_STATUS_INVALID_ALIGNMENT  ) :
TB_ENUMSTR(USBH_STATUS_MEMORY             ) :
TB_ENUMSTR(USBH_STATUS_RESOURCES          ) :
  "unknown status"
  );
}

/******************************* EOF ********************************/
