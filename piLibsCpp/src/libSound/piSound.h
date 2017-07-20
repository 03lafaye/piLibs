#pragma once

#include "../libSystem/piTypes.h"

namespace piLibs {

//====================================================
// new class, to replace all of the above (slowly)

class piSoundEngine
{
public:
    piSoundEngine();
    virtual ~piSoundEngine();

    static piSoundEngine * Create( bool forVR );

    virtual bool Init(void *hwnd, int deviceID, int maxSoundsOverride) = 0;
    virtual void Deinit(void) = 0;
    virtual int  GetNumDevices( void ) = 0;
    virtual const wchar_t *GetDeviceName(int id) const = 0;
	virtual int GetDeviceFromGUID(void *deviceGUID) = 0;
    virtual bool ResizeMixBuffers(int32_t const& mixSamples, int32_t const& spatialSamples) = 0;
    virtual void Tick(double dt) = 0;

    typedef struct       // Has to be mono. Then engine will spacialize
    {
        int mFrequency;  // samples per second (eg, 44100)
        int mPrecision;  // 8, 16 or 24 bits per sample
        int mLength;     // in samples
    }SampleFormat;

    virtual int  AddSound( const wchar_t *filename ) = 0; // wav file
    virtual int  AddSound( const SampleFormat *fmt, void *buffer ) = 0;
    virtual void DelSound(int id) = 0;
    virtual bool Play(int id, bool loop, float volume) = 0;
    virtual bool Stop(int id) = 0;
    virtual bool GetIsPlaying(int id) = 0;
    virtual bool SetVolume(int id, float volume) = 0;

    virtual bool  SetPosition(int id, const float * pos) = 0;
    virtual bool  SetOrientation(int id, const float * dir, const float * up) = 0;

    virtual bool GetSpatialize(int32_t id) const = 0;
    virtual bool SetSpatialize(int32_t id, bool spatialize) = 0;
    virtual bool CanChangeSpatialize(int32_t id) const = 0;
    virtual bool GetLooping(int32_t id) const = 0;
    virtual bool SetLooping(int32_t id, bool looping) = 0;
    virtual bool GetPaused(int32_t id) const = 0;
    virtual bool SetPaused(int32_t id, bool paused) = 0;
    virtual float GetVolume(int32_t id) const = 0;
    virtual int32_t GetAttenMode(int32_t id) const = 0;
    virtual bool SetAttenMode(int32_t id, int32_t attenMode) = 0;
    virtual float GetAttenRadius(int32_t id) const = 0;
    virtual bool SetAttenRadius(int32_t id, float attenRadius) = 0;
    virtual float GetAttenMin(int32_t id) const = 0;
    virtual bool SetAttenMin(int32_t id, float attenMin) = 0;
    virtual float GetAttenMax(int32_t id) const = 0;
    virtual bool SetAttenMax(int32_t id, float attenMax) = 0;

	virtual void PauseAllSounds( void )=0;
	virtual void ResumeAllSounds( void ) = 0;
};


} // namespace piLibs
