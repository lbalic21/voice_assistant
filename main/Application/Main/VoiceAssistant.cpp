#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "VoiceAssistant.hpp"
#include "AudioRecorder.hpp"
#include "AudioESP.cpp"

#include "../managed_components/espressif__esp-tflite-micro/tensorflow/lite/micro/micro_op_resolver.h"

static const char *TAG = "VoiceAssistant";

void Application()
{
    TfLiteStatus s;
    ESP_LOGI(TAG, "Starting the application.");
    AudioRecorder* recorder = new AudioESP();
    recorder->provide();
    while(1)
    {
        ESP_LOGI(TAG, "Running.");
        vTaskDelay(100);
    }
}