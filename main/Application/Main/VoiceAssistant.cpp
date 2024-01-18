#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "esp_peripherals.h"
#include "i2s_stream.h"

#include "VoiceAssistant.hpp"
#include "AudioProvider.hpp"
#include "LyratAudio.cpp"

static const char *TAG = "VoiceAssistant";

void Application()
{
    ESP_LOGI(TAG, "Starting the application.");
    AudioProvider* provider = new LyratAudio();
    provider->provide();
    while(1)
    {
        ESP_LOGI(TAG, "Running.");
        vTaskDelay(100);
    }
}