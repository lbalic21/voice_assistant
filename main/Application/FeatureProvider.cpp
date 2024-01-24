#include "FeatureProvider.hpp"
#include "AudioRecorder.hpp"
#include "FeatureGenerator.hpp"
#include "projectConfig.hpp"
#include "tensorflow/lite/micro/micro_log.h"

FeatureProvider::FeatureProvider(int feature_size, int8_t* feature_data)
    : feature_size_(feature_size),
      feature_data_(feature_data),
      is_first_run_(true) {
  // Initialize the feature data to default values.
  for (int n = 0; n < feature_size_; ++n) {
    feature_data_[n] = 0;
  }
}

FeatureProvider::~FeatureProvider() {}

TfLiteStatus FeatureProvider::PopulateFeatureData(
    int32_t last_time_in_ms, int32_t time_in_ms, int* how_many_new_slices) {
  if (feature_size_ != FEATURE_ELEMENT_COUNT) {
    MicroPrintf("Requested feature_data_ size %d doesn't match %d",
                feature_size_, FEATURE_ELEMENT_COUNT);
    return kTfLiteError;
  }

  // Quantize the time into steps as long as each window stride, so we can
  // figure out which audio data we need to fetch.
  const int last_step = (last_time_in_ms / STRIDE_MS);
  const int current_step = (time_in_ms / STRIDE_MS);

  int slices_needed = current_step - last_step;
  // If this is the first call, make sure we don't use any cached information.
  if (is_first_run_) {
    TfLiteStatus init_status = InitializeMicroFeatures();
    if (init_status != kTfLiteOk) {
      return init_status;
    }
    is_first_run_ = false;
    slices_needed = FEATURE_SLICE_COUNT;
  }
  if (slices_needed > FEATURE_SLICE_COUNT) {
    slices_needed = FEATURE_SLICE_COUNT;
  }
  *how_many_new_slices = slices_needed;

  const int slices_to_keep = FEATURE_SLICE_COUNT  - slices_needed;
  const int slices_to_drop = FEATURE_SLICE_COUNT  - slices_to_keep;
  // If we can avoid recalculating some slices, just move the existing data
  // up in the spectrogram, to perform something like this:
  // last time = 80ms          current time = 120ms
  // +-----------+             +-----------+
  // | data@20ms |         --> | data@60ms |
  // +-----------+       --    +-----------+
  // | data@40ms |     --  --> | data@80ms |
  // +-----------+   --  --    +-----------+
  // | data@60ms | --  --      |  <empty>  |
  // +-----------+   --        +-----------+
  // | data@80ms | --          |  <empty>  |
  // +-----------+             +-----------+
  if (slices_to_keep > 0) {
    for (int dest_slice = 0; dest_slice < slices_to_keep; ++dest_slice) {
      int8_t* dest_slice_data =
          feature_data_ + (dest_slice * FEATURE_SLICE_SIZE);
      const int src_slice = dest_slice + slices_to_drop;
      const int8_t* src_slice_data =
          feature_data_ + (src_slice * FEATURE_SLICE_SIZE);
      for (int i = 0; i < FEATURE_SLICE_SIZE; ++i) {
        dest_slice_data[i] = src_slice_data[i];
      }
    }
  }
  // Any slices that need to be filled in with feature data have their
  // appropriate audio data pulled, and features calculated for that slice.
  if (slices_needed > 0) {
    for (int new_slice = slices_to_keep; new_slice < FEATURE_SLICE_COUNT;
         ++new_slice) {
      const int new_step = (current_step - FEATURE_SLICE_COUNT + 1) + new_slice;
      const int32_t slice_start_ms = (new_step * STRIDE_MS);
      int16_t* audio_samples = nullptr;
      int audio_samples_size = 0;
      // TODO(petewarden): Fix bug that leads to non-zero slice_start_ms
      GetSamples((slice_start_ms > 0 ? slice_start_ms : 0),
                      WINDOW_MS, &audio_samples_size,
                      &audio_samples);
      if (audio_samples_size < 512) {
        MicroPrintf("Audio data size %d too small, want %d",
                    audio_samples_size, 512);
        return kTfLiteError;
      }
      int8_t* new_slice_data = feature_data_ + (new_slice * FEATURE_SLICE_SIZE);
      size_t num_samples_read;
      TfLiteStatus generate_status = GenerateMicroFeatures(
          audio_samples, audio_samples_size, FEATURE_SLICE_SIZE,
          new_slice_data, &num_samples_read);
      if (generate_status != kTfLiteOk) {
        return generate_status;
      }
    }
  }
  return kTfLiteOk;
}