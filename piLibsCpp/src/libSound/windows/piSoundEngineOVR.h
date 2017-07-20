#pragma once


#undef min
#undef max


#include "../../libDataUtils/piArray.h"
#include "../piSound.h"

#include <stdint.h>
#include <dsound.h>

struct IDirectSound8;
struct IDirectSoundBuffer;

namespace piLibs
{

struct Sound;
struct SampleFormat;

static const int32_t kInvalidSound = -1;

class piSoundEngineOVR : public piSoundEngine
{
public:
    static const uint32_t kMaxSoundsDefault = 128;

    piSoundEngineOVR();
    virtual ~piSoundEngineOVR();

    virtual bool Init(void* hwnd, int deviceID, int maxSoundsOverride);
    virtual void Deinit();
    virtual int  GetNumDevices(void);
    virtual const wchar_t *GetDeviceName(int id) const;
    virtual int GetDeviceFromGUID(void *deviceGUID);
    virtual bool ResizeMixBuffers(int32_t const& mixSamples, int32_t const& spatialSamples) { return SetupOVRContextAndMixBuffer( mixSamples, spatialSamples ); }

    virtual void Tick(double dt);

    virtual int32_t AddSound(const wchar_t* filename);
    virtual int32_t AddSound(const SampleFormat*, void*) { return -1; } // Init creates mix buffer which is sufficient
    virtual void DelSound(int32_t soundIndex);

    virtual bool Play(int32_t soundIndex);
    virtual bool Play(int32_t soundIndex, bool loop, float volume);
    virtual bool Stop(int32_t soundIndex);
    virtual bool IsValid(int32_t soundIndex) const { return soundIndex >= 0 && soundIndex < m_maxSounds && m_sounds.GetPtr(soundIndex); }
    virtual bool GetIsPlaying(int32_t soundIndex);

    virtual bool SetPosition(int32_t soundIndex, const float * position);
    virtual bool SetOrientation(int32_t soundIndex, const float * dir, const float * up);
    virtual bool GetSpatialize( int32_t soundIndex ) const;
    virtual bool SetSpatialize( int32_t soundIndex, bool spatialize );
    virtual bool CanChangeSpatialize(int32_t soundIndex) const;
    virtual bool GetLooping( int32_t soundIndex ) const;
    virtual bool SetLooping( int32_t soundIndex, bool looping );
    virtual bool GetPaused(int32_t soundIndex) const;
    virtual bool SetPaused(int32_t soundIndex, bool paused);
    virtual float GetVolume( int32_t soundIndex ) const;
    virtual bool SetVolume( int32_t soundIndex, float volume );
    virtual int32_t GetAttenMode(int32_t id) const;
    virtual bool SetAttenMode(int32_t id, int32_t attenMode);
    virtual float GetAttenRadius(int32_t id) const;
    virtual bool SetAttenRadius(int32_t id, float attenRadius);
    virtual float GetAttenMin(int32_t id) const;
    virtual bool SetAttenMin(int32_t id, float attenMin);
    virtual float GetAttenMax(int32_t id) const;
    virtual bool SetAttenMax(int32_t id, float attenMax);

    virtual void PauseAllSounds();
    virtual void ResumeAllSounds();
    //virtual void SetListener(const mat4x4d& lst2world);

private:
    static BOOL CALLBACK myDSEnumCallback(LPGUID lpGuid, const wchar_t * lpcstrDescription, const wchar_t * lpcstrModule, void * lpContext);
    int mNumDevices;
    struct DeviceInfo
    {
        wchar_t *mName;
        GUID mGUID;
    }mDevice[16];

    bool SetupOVRAudio();
    bool SetupOVRContextAndMixBuffer(int32_t const& mixBufferSampleCount, int32_t const& spatialProcessSampleCount);

    IDirectSound8* m_directSound;
    IDirectSoundBuffer* m_primaryBuffer;
    IDirectSoundBuffer* m_spatialMixingBuffer;
    piArray m_sounds;
    void * m_context;
    int32_t m_mixBufferSampleCount;
    int32_t m_spatialProcessSampleCount;
    float* m_frameMixSamplesL;
    float* m_frameMixSamplesR;
    int16_t* m_frameStereoSamples;
    float* m_soundMonoSamplesIn;
    float* m_soundStereoSamplesOut;
    //mat4x4d m_world2view;
    //mat4x4d m_view2world;
    int32_t m_lastSegmentWritten;
    int32_t m_maxSounds;
    bool m_paused;
};

} // namespace piLibs

