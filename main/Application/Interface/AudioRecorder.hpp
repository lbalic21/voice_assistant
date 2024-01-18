#ifndef _Audio_Recorder_
#define _Audio_Recorder_

#ifdef __cplusplus
extern "C" {
#endif

class AudioRecorder
{
    public:
        virtual void provide() = 0;
};

#ifdef __cplusplus
}
#endif

#endif /* _Audio_Recorder_ */