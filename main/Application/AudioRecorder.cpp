#include "AudioRecorder.hpp"
#include "projectConfig.hpp"

static const char *AUDIO_TAG = "AudioRecorder";

constexpr static uint32_t bytesToRead = NEW_READ_MS * AUDIO_SAMPLING_RATE / 1000 * sizeof(int16_t);
constexpr static uint32_t numberOfNewStrideSamples = STRIDE_MS * AUDIO_SAMPLING_RATE / 1000;
constexpr static uint32_t numberOfOldSamples = (WINDOW_MS - STRIDE_MS) * AUDIO_SAMPLING_RATE / 1000;
volatile int32_t latest_audio_timestamp = 0;
int16_t g_audio_output_buffer[512];
int16_t oldDataBuffer[160];
ringbuff_t* ringBuffer;

static void i2s_read_task(void* parameters)
{
    ESP_LOGI(AUDIO_TAG, "Audio recording task created");
    Buffers* buffs = (Buffers*)parameters;
    size_t bytes_read;
    while(1)
    {
        i2s_read(I2S_NUM_0, (void*) buffs->newDataBuffer, bytesToRead, &bytes_read, 10);

        if(bytes_read <= 0)
        {
            ESP_LOGE(AUDIO_TAG, "I2S read error: %d", bytes_read);
        }
        if (bytes_read < bytesToRead) 
        {
            ESP_LOGW(AUDIO_TAG, "Partial I2S read: %d/%ld", bytes_read, bytesToRead);
        }

        int bytesWrittenToRingBuff = rbuff_write(ringBuffer,(uint8_t*) buffs->newDataBuffer, bytes_read, portMAX_DELAY);
        latest_audio_timestamp = latest_audio_timestamp + ((1000 * (bytesWrittenToRingBuff / 2)) / AUDIO_SAMPLING_RATE);
        if (bytesWrittenToRingBuff <= 0) 
        {
            ESP_LOGE(AUDIO_TAG, "Could Not Write to Ring Buffer: %d ", bytesWrittenToRingBuff);
        }
        if (bytesWrittenToRingBuff < bytes_read) 
        {
            ESP_LOGW(AUDIO_TAG, "Partial Write");
        }
        
        /*
        for(int i = 0; i < bytes_read/2; i+=2) {
            int16_t d;
            memcpy((void*)&d, &buffs->newDataBuffer[i], 2);
        //`if(i%500 == 0)
            printf("%d\n", d);
        //
        }
        */
        
    
        //vTaskDelay(pdMS_TO_TICKS(80));
    }
}

AudioRecorder::AudioRecorder() 
        {
            this->buffers.newDataBuffer = (uint8_t*)malloc(bytesToRead);
            //this->buffers.oldDataBuffer = (int16_t*)malloc(numberOfOldSamples * sizeof(int16_t));
            this->buffers.newStrideDataBuffer = (int16_t*)malloc(numberOfNewStrideSamples * sizeof(int16_t));

            ringBuffer = rbuff_init("ring_buffer", RING_BUFFER_SIZE);
            if(!ringBuffer)
            {
                ESP_LOGE(AUDIO_TAG, "Error creating ring buffer");
            }
            
            ESP_LOGI(AUDIO_TAG, "Starting codec chip");
            audio_board_handle_t board_handle = audio_board_init();
            audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
           
            ESP_LOGI(AUDIO_TAG, "Creating i2s stream to read audio data from codec chip");
            i2s_config_t i2s = {
                .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
                .sample_rate = (uint32_t)AUDIO_SAMPLING_RATE,
                .bits_per_sample = (i2s_bits_per_sample_t)AUDIO_BITS_PER_SAMPLE, // change to 16 bits per sample
                .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Although the microphone is mono, the I2S data is 2-channel
                .communication_format = I2S_COMM_FORMAT_I2S,
                .intr_alloc_flags = 0, // Default interrupt priority
                .dma_buf_count = 3,
                .dma_buf_len = 300,
                .use_apll = false,
                .tx_desc_auto_clear = false, // Auto clear tx descriptor on underflow
                .fixed_mclk = -1
            };
             
            audio_element_handle_t i2s_stream_reader;
            i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
            i2s_cfg.type = AUDIO_STREAM_READER;
            i2s_cfg.i2s_port = I2S_NUM_0;
            i2s_cfg.i2s_config = i2s;
            i2s_stream_reader = i2s_stream_init(&i2s_cfg);   

            ESP_LOGI(AUDIO_TAG, "Creating audio recording task");
            xTaskCreate(
                i2s_read_task,           // Task function
                "i2s_read_task",         // Name of the task
                1024 * 4,                // Stack size of the task
                (void*)&(this->buffers), // Parameter of the task
                1,                       // Priority of the task
                &(this->i2s_handle)      // Task handle to keep track of the created task
            );
        }

        AudioRecorder::~AudioRecorder()
        {
            if(i2s_handle != NULL) {
                vTaskDelete(this->i2s_handle);
            }
            i2s_driver_uninstall(I2S_NUM_0);
            free(buffers.newDataBuffer);
            rbuff_cleanup(ringBuffer);
        }

        uint32_t AudioRecorder::getSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples)
        {
                memcpy((void*)(g_audio_output_buffer),(void*)oldDataBuffer,2*numberOfOldSamples);
                int readBytes = rbuff_read(ringBuffer,(uint8_t*)(g_audio_output_buffer + numberOfOldSamples),2*numberOfNewStrideSamples, 10);
                if (readBytes < 0) {
                    ESP_LOGE(AUDIO_TAG, " Model Could not read data from Ring Buffer");
                } else if (readBytes < numberOfNewStrideSamples * sizeof(int16_t)) {
                    ESP_LOGD(AUDIO_TAG, "RB FILLED RIGHT NOW IS %d", rbuff_filled(ringBuffer));
                    ESP_LOGD(AUDIO_TAG, " Partial Read of Data by Model ");
                    ESP_LOGV(AUDIO_TAG, " Could only read %d bytes when required %d bytes ",
                    readBytes, (int) (numberOfNewStrideSamples * sizeof(int16_t)));
                }
                
                memcpy((void*)oldDataBuffer, (void*)(g_audio_output_buffer+numberOfNewStrideSamples), 2*numberOfOldSamples);
            
            *audio_samples_size = 512;
            *audio_samples = g_audio_output_buffer;
            return numberOfOldSamples + numberOfNewStrideSamples;
        }    
        int32_t AudioRecorder::LatestAudioTimestamp() { return latest_audio_timestamp; }

uint32_t GetSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples)
        {
            memcpy((void*)(g_audio_output_buffer),(void*)oldDataBuffer,2*numberOfOldSamples);
                int readBytes = rbuff_read(ringBuffer,(uint8_t*)(g_audio_output_buffer + numberOfOldSamples),2*numberOfNewStrideSamples, 10);
                if (readBytes < 0) {
                    ESP_LOGE(AUDIO_TAG, " Model Could not read data from Ring Buffer");
                } else if (readBytes < numberOfNewStrideSamples * sizeof(int16_t)) {
                    ESP_LOGD(AUDIO_TAG, "RB FILLED RIGHT NOW IS %d", rbuff_filled(ringBuffer));
                    ESP_LOGD(AUDIO_TAG, " Partial Read of Data by Model ");
                    ESP_LOGV(AUDIO_TAG, " Could only read %d bytes when required %d bytes ",
                    readBytes, (int) (numberOfNewStrideSamples * sizeof(int16_t)));
                }
                
                memcpy((void*)oldDataBuffer, (void*)(g_audio_output_buffer+numberOfNewStrideSamples), 2*numberOfOldSamples);
            
            *audio_samples_size = 512;
            *audio_samples = g_audio_output_buffer;
            return numberOfOldSamples + numberOfNewStrideSamples;
        }