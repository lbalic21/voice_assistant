#ifndef _Feature_Provider_
#define _Feature_Provider_


#include "stdint.h"
#include "tensorflow/lite/c/common.h"

class FeatureProvider
{
    private:
        int feature_size_;
        int8_t* feature_data_;
        // Make sure we don't try to use cached information if this is the first call
        // into the provider.
        bool is_first_run_;

    public:
        FeatureProvider(int feature_size, int8_t* feature_data);
        ~FeatureProvider();
        TfLiteStatus PopulateFeatureData(int32_t last_time_in_ms, int32_t time_in_ms, int* how_many_new_slices);
};



#endif /* _Feature_Provider_ */