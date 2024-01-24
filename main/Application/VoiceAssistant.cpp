#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "VoiceAssistant.hpp"
#include "projectConfig.hpp"
#include "AudioRecorder.hpp"
#include "FeatureProvider.hpp"
#include "CommandRecognizer.hpp"
#include "CommandResponder.hpp"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model.h"

#include "../managed_components/espressif__esp-tflite-micro/tensorflow/lite/micro/micro_op_resolver.h"

static const char *TAG = "VoiceAssistant";

AudioRecorder* recorder = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* model_input = nullptr;
FeatureProvider* feature_provider = nullptr;
CommandRecognizer* recognizer = nullptr;
int32_t previous_time = 0;

constexpr int kTensorArenaSize = 30 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
int8_t feature_buffer[FEATURE_ELEMENT_COUNT];
int8_t* model_input_buffer = nullptr;

void Application(void)
{
    ESP_LOGI(TAG, "Starting the application.");
    recorder = new AudioRecorder();
    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
      MicroPrintf("Model provided is schema version %d not equal to supported "
                  "version %d.", model->version(), TFLITE_SCHEMA_VERSION);
      return;
    }
    static tflite::MicroMutableOpResolver<4> micro_op_resolver;
    if (micro_op_resolver.AddDepthwiseConv2D() != kTfLiteOk) {
      return;
    }
    if (micro_op_resolver.AddFullyConnected() != kTfLiteOk) {
      return;
    }
    if (micro_op_resolver.AddSoftmax() != kTfLiteOk) {
      return;
    }
    if (micro_op_resolver.AddReshape() != kTfLiteOk) {
      return;
    }
    // Build an interpreter to run the model with.
    static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) 
    {
      MicroPrintf("AllocateTensors() failed");
      return;
    }
    // Get information about the memory area to use for the model's input.
    model_input = interpreter->input(0);
    if ((model_input->dims->size != 2) || (model_input->dims->data[0] != 1) || (model_input->dims->data[1] != (FEATURE_SLICE_COUNT * FEATURE_SLICE_SIZE)) ||
        (model_input->type != kTfLiteInt8)) {
      MicroPrintf("Bad input tensor parameters in model");
      return;
    }
    model_input_buffer = model_input->data.int8;

    static FeatureProvider static_feature_provider(FEATURE_ELEMENT_COUNT, feature_buffer);
    feature_provider = &static_feature_provider;

    static CommandRecognizer static_recognizer;
    recognizer = &static_recognizer;


  previous_time = 0;


    while(1)
    {
        ESP_LOGI(TAG, "Running");
        // Fetch the spectrogram for the current time.
        const int32_t current_time = recorder->LatestAudioTimestamp();
        int how_many_new_slices = 0;
        TfLiteStatus feature_status = feature_provider->PopulateFeatureData(previous_time, current_time, &how_many_new_slices);
        if (feature_status != kTfLiteOk) 
        {
            MicroPrintf( "Feature generation failed");
            return;
        }
        previous_time = current_time;
        // If no new audio samples have been received since last time, don't bother
        // running the network model.
        if (how_many_new_slices == 0) {
          return;
        }
        // Copy feature buffer to input tensor
        for (int i = 0; i < 4; i++) 
        {
          model_input_buffer[i] = feature_buffer[i];
        }
      
        // Run the model on the spectrogram input and make sure it succeeds.
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk) 
        {
          MicroPrintf( "Invoke failed");
          return;
        }

        // Obtain a pointer to the output tensor
        TfLiteTensor* output = interpreter->output(0);
        // Determine whether a command was recognized based on the output of inference
        const char* found_command = nullptr;
        uint8_t score = 0;
        bool is_new_command = false;
        TfLiteStatus process_status = recognizer->ProcessLatestResults(
            output, current_time, &found_command, &score, &is_new_command);
        if (process_status != kTfLiteOk) {
          MicroPrintf("RecognizeCommands::ProcessLatestResults() failed");
          return;
        }
        // Do something based on the recognized command. The default implementation
        // just prints to the error console, but you should replace this with your
        // own function for a real application.
        RespondToCommand(current_time, found_command, score, is_new_command);

    }
}