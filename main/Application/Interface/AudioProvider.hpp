#ifndef _Audio_Provider_
#define _Audio_Provider_

#ifdef __cplusplus
extern "C" {
#endif

class AudioProvider
{
    public:
        virtual void provide() = 0;
};

#ifdef __cplusplus
}
#endif

#endif /* _Audio_Provider_ */