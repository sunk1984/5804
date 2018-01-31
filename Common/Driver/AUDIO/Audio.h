
#ifndef _HONEYWELL_FCT_AUDIO_H_
#define _HONEYWELL_FCT_AUDIO_H_

extern BOOL Audio_TestBuzz(U16 buzz_freq, U16 amp_lower, U16 amp_upper);
extern BOOL Audio_LoopTest(U16 tx_amp, U16 amp_lower, U16 amp_upper);
extern BOOL Audio_GenTone(U16 tx_fre, U16 tx_amp);
extern BOOL Audio_StopTone(void);
extern BOOL Audio_CompToneAmp(U16 amp_lower, U16 amp_upper);
extern BOOL Audio_DecToneFreq(U16 freq_lower, U16 freq_upper);
extern BOOL Audio_Open(void);
#endif

