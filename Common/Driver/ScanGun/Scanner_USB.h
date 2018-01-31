#ifndef USBH_HID__
#define USBH_HID__

extern void USBH_HID_SetOnScanGunStateChange(USBH_HID_ON_SCANGUN_FUNC * pfOnChange);
extern void SCANGUN_GetBarCode(U8 * barCode, U8 len);
extern U8 SCANGUN_BarCode_PutDUT(U8 * barCode, U8 len);
extern void USBH_HID_Task(void);

#endif
