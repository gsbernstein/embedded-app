#ifndef _EPD_WAVEFORM_H_
#define _EPD_WAVEFORM_H_

#include <stdint.h>

#define MAX_WAVEFORM_VERSION 5

typedef struct
{
    uint32_t SourceData;
    uint16_t FrameCount;
    uint16_t Repeat;
} WaveformDataInit;

typedef struct
{
    int16_t FrameCount;
    uint8_t Reserved[2];
    uint8_t Data;
} WaveformDataBlock;

extern uint32_t EPD_WF_Init(const uint8_t *data);
extern const WaveformDataBlock *EPD_WF_GetData(int16_t temperature, uint8_t mode);
extern uint8_t EPD_WF_GetBpp(void);
extern uint16_t EPD_WF_Version(void);
extern uint8_t EPD_WF_SrcDataMode(void);

#endif
