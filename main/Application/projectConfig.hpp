#ifndef _project_Config_
#define _project_Config_

#define RING_BUFFER_SIZE        80000
#define AUDIO_SAMPLING_RATE     16000
#define AUDIO_BITS_PER_SAMPLE   16
#define NEW_READ_MS             100
#define WINDOW_MS               30
#define STRIDE_MS               20
#define FEATURE_SLICE_SIZE      40
#define FEATURE_SLICE_COUNT     49
#define FEATURE_ELEMENT_COUNT   (FEATURE_SLICE_SIZE) * (FEATURE_SLICE_COUNT) 

extern const char* kCategoryLabels[4];

#endif /* _project_Config_ */