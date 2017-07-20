#pragma once

#include <Dsound.h>

#include "../../libDataUtils/piTArray.h"

#include "../piSound.h"

namespace piLibs {

//#define USE3D

class piSoundEngineDS : public piSoundEngine
{
public:
    piSoundEngineDS();
    ~piSoundEngineDS();

    bool Init(void *hwnd, int deviceID, int maxSoundsOverride);
    void Deinit(void);
    int  GetNumDevices(void);
    const wchar_t *GetDeviceName(int id) const;
	int GetDeviceFromGUID(void *deviceGUID);
    bool ResizeMixBuffers(int32_t const& mixSamples, int32_t const& spatialSamples) { return true; }

    int   AddSound( const wchar_t *filename ); // wav file
    int   AddSound( const SampleFormat *fmt, void *buffer );
    bool  Play(int id, bool loop, float volume);
    bool  Stop(int id);
    bool  SetVolume(int id, float volume);
    bool  GetIsPlaying(int id);
    void  DelSound(int id);

	void PauseAllSounds(void);
	void ResumeAllSounds(void);

    bool  SetPosition(int id, const float * pos);
    //bool  SetOrientation(int id, const float * dir, const float * up);

    // stub spatializer engine interface
    void Tick(double dt) {};
    bool SetOrientation(int id, const float * dir, const float * up) { return false; }
    bool GetSpatialize(int32_t id) const { return false; }
    bool SetSpatialize(int32_t id, bool spatialize) { return false; }
    bool CanChangeSpatialize(int32_t id) const { return false; }
    bool GetLooping(int32_t id) const { return false; }
    bool SetLooping(int32_t id, bool looping) { return false; }
    bool GetPaused(int32_t id) const { return false; }
    bool SetPaused(int32_t id, bool paused) { return false; }
    float GetVolume(int32_t id) const { return false; }
    int32_t GetAttenMode(int32_t id) const { return 0; }
    bool SetAttenMode(int32_t id, int32_t attenMode) { return false; }
    float GetAttenRadius(int32_t id) const  { return false; }
    bool SetAttenRadius(int32_t id, float attenRadius) { return false; }
    float GetAttenMin(int32_t id) const { return false; }
    bool SetAttenMin(int32_t id, float attenMin) { return false; }
    float GetAttenMax(int32_t id) const { return false; }
    bool SetAttenMax(int32_t id, float attenMax) { return false; }

private:
    static BOOL CALLBACK myDSEnumCallback(LPGUID lpGuid, const wchar_t * lpcstrDescription, const wchar_t * lpcstrModule, void * lpContext);

    int mNumDevices;
    struct DeviceInfo
    {
		wchar_t  *mName;
		GUID     mGUID;
    }mDevice[16];

    typedef struct
    {
        IDirectSoundBuffer8* mBuffer;
        #ifdef USE3D
        IDirectSound3DBuffer8* mBuffer3D;
        #endif
        int  mID;
        // for pause/resume
        bool mWasPlaying;
        bool mWasLooping;
    }iSound;

    IDirectSound8* m_DirectSound;
    IDirectSoundBuffer* m_primaryBuffer;
    piTArray<iSound> mSounds;

#ifdef USE3D
    IDirectSound3DListener8 *m_listener;
#endif
};

} // namespace piLibs
