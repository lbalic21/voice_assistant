#ifndef _Feature_Generator_
#define _Feature_Generator_



#include "tensorflow/lite/c/common.h"

// Sets up any resources needed for the feature generation pipeline.
TfLiteStatus InitializeMicroFeatures();

// Converts audio sample data into a more compact form that's appropriate for
// feeding into a neural network.
TfLiteStatus GenerateMicroFeatures(const int16_t* input, int input_size,
                                   int output_size, int8_t* output,
                                   size_t* num_samples_read);



#endif  /* _Feature_Generator_ */