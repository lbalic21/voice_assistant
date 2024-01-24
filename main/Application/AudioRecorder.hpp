#ifndef _Audio_Recorder_
#define _Audio_Recorder_

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_common.h"
#include "board.h"
#include "esp_peripherals.h"
#include "i2s_stream.h"
#include "audio_idf_version.h"
#include <cstdlib>
#include <cstring>
#include "ringBuff.h"

typedef struct Buffers
{
    uint8_t* newDataBuffer;
    int16_t* newStrideDataBuffer;
} Buffers;

class AudioRecorder
{
    private:
        Buffers buffers;
        TaskHandle_t i2s_handle;

    public:
        AudioRecorder();
        ~AudioRecorder();
        uint32_t getSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples);
        int32_t LatestAudioTimestamp();
};

uint32_t GetSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples);

#ifdef __cplusplus
}
#endif

#endif /* _Audio_Recorder_ */