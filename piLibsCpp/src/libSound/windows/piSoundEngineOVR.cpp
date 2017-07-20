#include <dsound.h>
#include <math.h>

#define DO_NOT_COMPILE

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

#include "piSoundEngineOVR.h"

#include "../../libMisc/formats/piWav.h"
#include "piDecompressMP3.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

#ifndef DO_NOT_COMPILE
#include "OVR_Audio.h"
#endif

#define AMBISONIC_CHANNELS 4
#define BITS_PER_SAMPLE 16
#define NATIVE_SAMPLE_RATE 48000
#define MIX_BUFFER_SAMPLES 4096
#define SPATIALIZE_SAMPLES 1024

//////////////////////////////////////////////////////
/// BEGIN BIG OVR HACK
//////////////////////////////////////////////////////

#define OVR_ALIGNAS(n) __declspec(align(n))
#define OVR_UNUSED_STRUCT_PAD(padName, size) char padName[size];

/// A quaternion rotation.
typedef struct OVR_ALIGNAS(4) ovrQuatf_
{
    float x, y, z, w;
} ovrQuatf;

/// A 2D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrVector2f_
{
    float x, y;
} ovrVector2f;

/// A 3D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrVector3f_
{
    float x, y, z;
} ovrVector3f;

/// A 4x4 matrix with float elements.
typedef struct OVR_ALIGNAS(4) ovrMatrix4f_
{
    float M[4][4];
} ovrMatrix4f;


/// Position and orientation together.
typedef struct OVR_ALIGNAS(4) ovrPosef_
{
    ovrQuatf     Orientation;
    ovrVector3f  Position;
} ovrPosef;

/// A full pose (rigid body) configuration with first and second derivatives.
///
/// Body refers to any object for which ovrPoseStatef is providing data.
/// It can be the HMD, Touch controller, sensor or something else. The context
/// depends on the usage of the struct.
typedef struct OVR_ALIGNAS(8) ovrPoseStatef_
{
    ovrPosef     ThePose;               ///< Position and orientation.
    ovrVector3f  AngularVelocity;       ///< Angular velocity in radians per second.
    ovrVector3f  LinearVelocity;        ///< Velocity in meters per second.
    ovrVector3f  AngularAcceleration;   ///< Angular acceleration in radians per second per second.
    ovrVector3f  LinearAcceleration;    ///< Acceleration in meters per second per second.
    OVR_UNUSED_STRUCT_PAD(pad0, 4)    ///< \internal struct pad.
    double       TimeInSeconds;         ///< Absolute time that this pose refers to. \see ovr_GetTimeInSeconds
} ovrPoseStatef;

//////////////////////////////////////////////////////
/// END BIG OVR HACK
//////////////////////////////////////////////////////

namespace piLibs
{

struct Sound
{
    IDirectSoundBuffer* dsBuffer = nullptr;
    float dsBufferBytesPerNativeMonoSample = 1;
    int32_t dsBufferByteSize = 0;
    float* monoFloats = nullptr;
    int32_t monoSampleCount = 0;
    #ifndef DO_NOT_COMPILE
    ovrAudioAmbisonicStream ambiStream = nullptr;
    #endif
    float* ambiFloats = nullptr;
    int32_t readOffset = 0;
    int32_t nativeRate = 0;
    int32_t nativeChannels = 0;
    float position[3];
    float dir[3];
    float up[3];
    float volume = 1;
    int32_t attenMode = 0; 
    float attenRadius = 0;
    float attenRange[2] = { 0, 0 };
    bool restart = false;
    bool looping = false;
    bool paused = true;
    enum { Unspatialized, Spatialized, Ambisonic } playMode = Unspatialized;
};


template<typename T>
static const T Min(const T a, const T b)
{
    return a < b ? a : b;
}

template<typename T>
static const T Max(const T a, const T b)
{
    return a > b ? a : b;
}

template<typename T>
static const T Clamp(const T tVal, const T tMin, const T tMax)
{
    return Max(Min(tVal, tMax), tMin);
}

static const float AdjustSampleVolume(const float sampleIn, const float volumeIn)
{
    float const clampedVolume = Clamp<const float>(volumeIn, 0, 1);
    float const factor = ( expf(clampedVolume)-1.0f )/( 2.71828183f-1.0f );
    return sampleIn * factor;
}

static void GetMixBufferCursors(IDirectSoundBuffer* buffer, int32_t const& mixBufferSampleCount, int32_t& playOffset, int32_t& writeOffset, int32_t& writeCount)
{
    if(buffer)
    {
        int32_t playCursor = 0;
        int32_t writeCursor = 0;
        buffer->GetCurrentPosition((DWORD*)&playCursor, (DWORD*)&writeCursor);
        int32_t const bytesPerStereoSample = ( sizeof(int16_t)*2 );
        if(writeCursor>playCursor)
        {
            const int32_t mixBufferSize = bytesPerStereoSample*mixBufferSampleCount; // stereo 16bit
            writeCount = mixBufferSize-writeCursor+playCursor;
        }
        else
        {
            writeCount = playCursor-writeCursor;
        }

        playOffset = playCursor/bytesPerStereoSample;
        writeOffset = writeCursor/bytesPerStereoSample;
        writeCount /= bytesPerStereoSample;
    }
}

static void CopyToBuffer(IDirectSoundBuffer* buffer, int32_t const writeOffset, int32_t const writeCount, int16_t const* output)
{
    if(buffer)
    {
        int32_t const bytesPerStereoSample = ( sizeof(uint16_t)*2 );
        HRESULT result;
        unsigned char *writeToEnd;
        unsigned long writeToEndSize;
        unsigned char *beginToPlay;
        unsigned long beginToPlaySize;

        // Lock the secondary buffer to write wave data into it.
        result = buffer->Lock(
            writeOffset * bytesPerStereoSample,
            writeCount * bytesPerStereoSample,
            (void**)&writeToEnd,
            (DWORD*)&writeToEndSize,
            (void**)&beginToPlay,
            (DWORD*)&beginToPlaySize, 0);
        if(!FAILED(result))
        {
            // Copy the wave data into the buffer.
            memcpy(writeToEnd, output, writeToEndSize);

            if((int32_t)writeToEndSize!=writeCount * bytesPerStereoSample)
            {
                memcpy(beginToPlay, ( (char*)output )+writeToEndSize, beginToPlaySize);
            }

            // Unlock the secondary buffer after the data has been written to it.
            result = buffer->Unlock(writeToEnd, writeToEndSize, beginToPlay, beginToPlaySize);
        }
        else
        {
            //Log("Could not lock buffer for update.\n");
        }
    }
}

static IDirectSoundBuffer* CreateBuffer(IDirectSound8* device, int32_t sampleRate, int32_t bitsPerSample, int32_t numChannels, int32_t dataSizeBytes)
{
    WAVEFORMATEX waveFormat;
    DSBUFFERDESC bufferDesc;
    HRESULT result;
    IDirectSoundBuffer* tempBuffer;

    // Set the wave format of secondary buffer that this wave file will be loaded onto.
    const int32_t bytesPerSample = bitsPerSample/8;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nSamplesPerSec = sampleRate;
    waveFormat.wBitsPerSample = bitsPerSample;
    waveFormat.nChannels = numChannels;
    waveFormat.nBlockAlign = bytesPerSample * waveFormat.nChannels;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    // verify: data size in bytes must be integral multiples of sample/channel
    if(dataSizeBytes%( ( waveFormat.nChannels )*bytesPerSample )!=0)
    {
        return nullptr;
    }

    // Set the buffer description of the secondary sound buffer that the wave file will be loaded onto.
    bufferDesc.dwSize = sizeof(DSBUFFERDESC);
    bufferDesc.dwFlags = DSBCAPS_CTRLVOLUME|DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS;
    bufferDesc.dwBufferBytes = dataSizeBytes;
    bufferDesc.dwReserved = 0;
    bufferDesc.lpwfxFormat = &waveFormat;
    bufferDesc.guid3DAlgorithm = GUID_NULL;

    // Create a temporary sound buffer with the specific buffer settings.
    result = device->CreateSoundBuffer(&bufferDesc, &tempBuffer, NULL);
    if(FAILED(result))
    {
        return nullptr;
    }

    // Test the buffer format against the direct sound 8 interface and create the secondary buffer.
    IDirectSoundBuffer* resultBuffer;
    result = tempBuffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&resultBuffer);
    if(FAILED(result))
    {
        return nullptr;
    }

    // Release the temporary buffer.
    tempBuffer->Release();
    tempBuffer = 0;

    return resultBuffer;
}

static bool UpdateBuffer(IDirectSoundBuffer* buffer, void* data, int32_t dataSizeBytes)
{
    HRESULT result;
    unsigned char *bufferPtr;
    unsigned long bufferSize;

    // Lock the secondary buffer to write wave data into it.
    result = buffer->Lock(0, dataSizeBytes, (void**)&bufferPtr, (DWORD*)&bufferSize, NULL, 0, 0);
    if(FAILED(result))
    {
        return false;
    }

    // Copy the wave data into the buffer.
    memcpy(bufferPtr, data, dataSizeBytes);

    // Unlock the secondary buffer after the data has been written to it.
    result = buffer->Unlock((void*)bufferPtr, bufferSize, NULL, 0);
    if(FAILED(result))
    {
        return false;
    }

    return true;
}

static void ReleaseBuffer(IDirectSoundBuffer*& buffer)
{
    // Release the secondary sound buffer.
    if(buffer)
    {
        buffer->Stop();
        buffer->Release();
        buffer = nullptr;
    }
}

static bool SetBufferVolume(IDirectSoundBuffer* buffer, float volume)
{
    if(buffer)
    {
        // Set the volume using an exponential ramp (direct sound takes decibels)
        int logv = (int)( 10.0f*100.0f * log2f(fmaxf(volume, 0.00001f)) );
        if(logv>DSBVOLUME_MAX) logv = DSBVOLUME_MAX;
        else if(logv<DSBVOLUME_MIN) logv = DSBVOLUME_MIN;
        HRESULT result = buffer->SetVolume(logv);
        if(FAILED(result))
        {
            return false;
        }
        return true;
    }
    return false;
}

static bool PlayBuffer(IDirectSoundBuffer* buffer, int32_t byteOffset, float volume, bool looping)
{
    HRESULT result;

    // Set position at the beginning of the sound buffer.
    result = buffer->SetCurrentPosition(byteOffset);
    if(FAILED(result))
    {
        return false;
    }

    // Play the contents of the secondary sound buffer.
    result = buffer->Play(0, 0, looping ? DSBPLAY_LOOPING : 0);
    if(FAILED(result))
    {
        return false;
    }

    if(!SetBufferVolume(buffer, volume))
    {
        return false;
    }

    return true;
}

static int32_t GetBufferPlayOffsetBytes(IDirectSoundBuffer* buffer)
{
    if(buffer)
    {
        int32_t playCursor = 0;
        int32_t writeCursor = 0;
        buffer->GetCurrentPosition((DWORD*)&playCursor, (DWORD*)&writeCursor);
        return playCursor;
    }
    return 0;
}

static bool GetBufferIsPlaying(IDirectSoundBuffer* buffer)
{
    if ( buffer )
    {
        DWORD status;
        buffer->GetStatus(&status);
        return ( status&DSBSTATUS_PLAYING )!=0;
    }
    return false;
}

static bool StopBuffer(IDirectSoundBuffer* buffer)
{
    if ( buffer )
    {
        buffer->Stop();
        return true;
    }
    return false;
}

void ReleaseSampleData(Sound& sound)
{
    ReleaseBuffer(sound.dsBuffer);
    sound.monoSampleCount = 0;
    delete[] sound.monoFloats;
    sound.monoFloats = nullptr;
    #ifndef DO_NOT_COMPILE
    ovrAudio_DestroyAmbisonicStream(sound.ambiStream);
    sound.ambiStream = nullptr;
    #endif
    delete[] sound.ambiFloats;
    sound.ambiFloats = nullptr;
}

static void GenerateTestPattern(piWav& wav)
{
    wav.mBits = 24;
    wav.mNumChannels = 2;
    wav.mRate = 48000;
    wav.mNumSamples = 24000;
    wav.mDataSize = ( wav.mBits/8 ) * ( wav.mNumChannels ) * wav.mNumSamples;
    wav.mData = new uint8_t[wav.mDataSize];
    for(int i = 0; i<wav.mNumSamples; i++)
    {
        static double vol = 0.85;
        static double freq = 52;
        const double kPi = 3.1415926535897932384626433832795;
        const double v = vol * ::sin(freq * 2.0 * kPi * double(i)/wav.mNumSamples);

        if(wav.mBits==8)
        {
            for(int j = 0; j<wav.mNumChannels; j++)
            {
                ( (uint8_t*)wav.mData )[i*wav.mNumChannels+j] = Clamp< uint8_t >((uint8_t)(128*( v+1.0 )), 0, 255);
            }
        }
        else if(wav.mBits==16)
        {
            for(int j = 0; j<wav.mNumChannels; j++)
            {
                ( (uint16_t*)wav.mData )[i*wav.mNumChannels+j] = Clamp< int16_t >((int16_t)( v*32768 ), -32768, 32767);
            }
        }
        else if(wav.mBits==24)
        {
            int32_t vi = Clamp< int32_t >((int32_t)( v*8388608 ), -8388608, 8388607);
            for(int j = 0; j<wav.mNumChannels; j++)
            {
                int sampleOffset = ( i * wav.mNumChannels+j )*3;
                ( (uint8_t*)wav.mData )[sampleOffset+0] = ( vi>>0 ) & 0xff;
                ( (uint8_t*)wav.mData )[sampleOffset+1] = ( vi>>8 ) & 0xff;
                ( (uint8_t*)wav.mData )[sampleOffset+2] = ( vi>>16 )& 0xff;
            }
        }
    }
}

static bool LoadSampleData(IDirectSound8* directSound, const wchar_t* filename, Sound& sound, int32_t const& spatialProcessSampleCount)
{
    sound.dsBuffer = nullptr;
    sound.dsBufferBytesPerNativeMonoSample = 1;
    sound.dsBufferByteSize = 0;
    sound.monoSampleCount = 0;
    sound.monoFloats = nullptr;
    #ifndef DO_NOT_COMPILE
    sound.ambiStream = nullptr;
    #endif
    sound.ambiFloats = nullptr;

    piWav wav; 
    struct RAII { RAII(piWav& w) :raii(w) {} ~RAII() { raii.Deinit(); } piWav& raii; } wavRAII( wav );
    //static bool sGenerateTestPattern = false;
    //if(sGenerateTestPattern) GenerateTestPattern(wav); else
    if(!wav.Open(filename))
    {
        if(!OpenMP3FromFile(&wav, filename))
        {
            return false;
        }
    }

    // we will need this later if we try to change the latency
    sound.nativeRate = wav.mRate;
    sound.nativeChannels = wav.mNumChannels;

    // we treat 4-channel wav's as ambisonics. don't create a directsound buffer and don't convert to mono.
    if (wav.mNumChannels == AMBISONIC_CHANNELS)
    {
        // only support a single bit rate
        if (!(wav.mBits == 8 || wav.mBits == 16 || wav.mBits == 24))
        {
            return false;
        }

        const int32_t ambisonicOrder = wav.mNumChannels == 4 ? 1 : ( wav.mNumChannels == 9 ? 2 : ( wav.mNumChannels == 16 ? 3 : 0 ) );
        #ifndef DO_NOT_COMPILE
        if( ovrAudio_CreateAmbisonicStream( wav.mRate, spatialProcessSampleCount, ovrAudioAmbisonicFormat_AmbiX,
            ambisonicOrder, &sound.ambiStream ) != ovrSuccess )
        {
            return false;
        }
        #endif

        const int32_t bytesPerChannelSample = wav.mBits/8;
        const int32_t ambiSampleCount = wav.mDataSize/bytesPerChannelSample;
        sound.ambiFloats = new float[ambiSampleCount];
        sound.playMode = Sound::Ambisonic;
        sound.monoSampleCount = ambiSampleCount/4;

        if (wav.mBits == 8)
        {
            for (int i = 0; i < ambiSampleCount; i++)
            {
                uint8_t m = ((uint8_t*)wav.mData)[i];
                float floatValue = Clamp<float>(2.0f * ((float)m) / 255.0f - 1.0f, -1, 1);
                sound.ambiFloats[i] = floatValue;
            }
        }
        else if (wav.mBits == 16)
        {
            for (int i = 0; i < ambiSampleCount; i++)
            {
                int16_t m = ((int16_t*)wav.mData)[i];
                float floatValue = Clamp<float>(((float)m) / 32768.0f, -1, 1);
                sound.ambiFloats[i] = floatValue;
            }
        }
        else if (wav.mBits == 24)
        {
            for (int i = 0; i < ambiSampleCount; i++)
            {
                uint8_t* b = ((uint8_t*)wav.mData) + 3 * i;
                int32_t m = (int32_t)(b[0] << 8 | b[1] << 16 | b[2] << 24) >> 8;
                float dither = (float(rand()) / float(RAND_MAX)) * (1.0f / 8388608.0f);
                float floatValue = Clamp<float>(((float)m + dither) / 8388608.0f, -1, 1);
                sound.ambiFloats[i] = floatValue;
            }
        }
        return true;
    }

    // if we can at least create a direct sound buffer, we've succeeded.
    sound.dsBuffer = CreateBuffer(directSound, wav.mRate, wav.mBits, wav.mNumChannels, wav.mDataSize);
    if (!sound.dsBuffer)
    {
        return false;
    }

    // we use these to convert positions from dsound stereo to mono floats and back.
    sound.dsBufferByteSize = wav.mDataSize;
    sound.dsBufferBytesPerNativeMonoSample = ( wav.mBits / 8 ) * ( wav.mNumChannels ) * ( wav.mRate / (float)NATIVE_SAMPLE_RATE );

    if(!UpdateBuffer(sound.dsBuffer, wav.mData, wav.mDataSize))
    {
        ReleaseSampleData(sound);
        return false;
    }

    // the rest of the code below attempts to resample the input into 16bit mono 48k, for spatialization.
    // it's not an error if it fails, spatialization just won't be available.

    // Check that the wave file was recorded in stereo format.
    if(!( wav.mNumChannels==1||wav.mNumChannels==2 ))
    {
        //Log("Mono resample: sound file '%s' must have only 1 or 2 channels. Aborting.\n", filename);
        return true;
    }

    // Check that the wave file was recorded at the native sample rate
    if(wav.mRate!=NATIVE_SAMPLE_RATE )
    {
        //Log("Mono resample: sound file '%s' had invalid sample rate, must be %d. Aborting.\n", filename, NATIVE_SAMPLE_RATE);
        return true;
    }

    // We'll try to transcode from 8 or 24 into 16 bit
    if(!( wav.mBits==8||wav.mBits==16||wav.mBits==24 ))
    {
        //Log("Mono resample: sound file '%s' had invalid bits per sample, must be 8 or 16. Aborting.\n", filename);
        return true;
    }
    
    // Figure out our conversion parameters
    int32_t const inputNumSamples = wav.mDataSize/( wav.mNumChannels * ( wav.mBits/8 ) );
    sound.monoSampleCount = inputNumSamples;
    sound.monoFloats = new float[sound.monoSampleCount];
    if(!sound.monoFloats)
    {
        sound.monoSampleCount = 0;
        return true;
    }

    // Convert from whatever input is to 48k 16 bit mono
    bool const isStereoInput = wav.mNumChannels==2;
    for(int32_t i = 0; i<inputNumSamples; i++)
    {
        float monoSample = 0;

        if(isStereoInput)
        {
            if(wav.mBits==8)
            {
                uint8_t l = ( (uint8_t*)wav.mData )[i*2+0];
                uint8_t r = ( (uint8_t*)wav.mData )[i*2+1];
                monoSample = Clamp<float>( 2.0f * ( ( (float)l +(float)r ) / 510.0f ) - 1.0f, -1, 1 );
            }
            else if(wav.mBits==16)
            {
                int16_t l = ( (int16_t*)wav.mData )[i*2+0];
                int16_t r = ( (int16_t*)wav.mData )[i*2+1];
                monoSample = Clamp<float>( ( (float)l + (float)r ) / 65536.0f, -1, 1 );
            }
            else if (wav.mBits == 24)
            {
                uint8_t* b = ( (uint8_t*)wav.mData )+3*i*2;
                int32_t l = (int32_t)(b[0]<<8|b[1]<<16|b[2]<<24) >> 8;
                int32_t r = (int32_t)(b[3]<<8|b[4]<<16|b[5]<<24) >> 8;
                float dither = (float(rand()) / float(RAND_MAX)) * (1.0f / 16777216.0f);
                monoSample = Clamp<float>( ( (float)l +(float)r + dither) / 16777216.0f, -1, 1 );
            }
        }
        else
        {
            if(wav.mBits==8)
            {
                uint8_t m = ( (uint8_t*)wav.mData )[i];
                monoSample = Clamp<float>(2.0f * ( (float)m )/255.0f-1.0f, -1, 1);
            }
            else if (wav.mBits == 16)
            {
                int16_t m = ( (int16_t*)wav.mData )[i];
                monoSample = Clamp<float>(( (float)m )/32768.0f, -1, 1);
            }
            else if(wav.mBits==24)
            {
                uint8_t* b = ( (uint8_t*)wav.mData )+3*i;
                int32_t m = (int32_t)( b[0]<<8|b[1]<<16|b[2]<<24 ) >> 8;
                float dither = (float(rand()) / float(RAND_MAX)) * (1.0f / 8388608.0f);
                monoSample = Clamp<float>(( (float)m + dither )/8388608.0f, -1, 1);
            }
        }

        sound.monoFloats[i] = monoSample;
    }

    return true;
}

piSoundEngineOVR::piSoundEngineOVR()
    : m_directSound(nullptr)
    , m_primaryBuffer(nullptr)
    , m_spatialMixingBuffer(nullptr)
    , m_context{ nullptr }
    , m_mixBufferSampleCount( 0 )
    , m_spatialProcessSampleCount( 0 )
    , m_frameMixSamplesL( nullptr )
    , m_frameMixSamplesR(nullptr)
    , m_soundMonoSamplesIn(nullptr)
    , m_soundStereoSamplesOut(nullptr)
    , m_lastSegmentWritten(-1)
    , m_maxSounds(kMaxSoundsDefault)
    , m_paused(false)
{
}

piSoundEngineOVR::~piSoundEngineOVR()
{
    Deinit();
}

bool piSoundEngineOVR::SetupOVRAudio()
{
    // Version checking is not strictly necessary but it's a good idea!

    #ifndef DO_NOT_COMPILE
    int major, minor, patch;
    const char *VERSION_STRING;
    VERSION_STRING = ovrAudio_GetVersion(&major, &minor, &patch);
    //Log("Using OVRAudio: %s\n", VERSION_STRING);

    if(major!=OVR_AUDIO_MAJOR_VERSION||
       minor!=OVR_AUDIO_MINOR_VERSION)
    {
        //Log("Mismatched Audio SDK version!\n");
    }

    // Initialize the library.  This finds the DLL, loads it, and then
    // maps the various entry points.
    if(ovrAudio_Initialize()!=ovrSuccess)
    {
        //Log("Could not initialize library!\n");
        return false;
    }
    #endif
    return true;
}

bool piSoundEngineOVR::SetupOVRContextAndMixBuffer( int32_t const& mixBufferSampleCount, int32_t const& spatialProcessSampleCount )
{
    // just return if the values are already correct.
    if (mixBufferSampleCount == m_mixBufferSampleCount && spatialProcessSampleCount == m_spatialProcessSampleCount)
    {
        return true;
    }

    // only allow integral numbers of spatialize_samples
    if(mixBufferSampleCount%spatialProcessSampleCount!=0)
    {
        //Log("Spatialization buffer size (%d) must be multiple of mix buffer size (%d). Aborting.\n", spatialProcessSampleCount, mixBufferSampleCount);
        return false;
    }
    m_mixBufferSampleCount = mixBufferSampleCount;
    m_spatialProcessSampleCount = spatialProcessSampleCount;

    #ifndef DO_NOT_COMPILE
    ovrAudioContextConfiguration config = {};
    config.acc_Size = sizeof(config);
    config.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
    config.acc_SampleRate = NATIVE_SAMPLE_RATE;
    config.acc_BufferLength = m_spatialProcessSampleCount;
    config.acc_MaxNumSources = m_maxSounds; // TODO: pool/recycle a smaller number
    
    // destroy state that was dependent on the old buffer sizes
    if ( m_context )
    {
        ovrAudio_FreeSamples(m_soundMonoSamplesIn);
        ovrAudio_FreeSamples(m_soundStereoSamplesOut);
        delete[]m_frameMixSamplesL;
        delete[]m_frameMixSamplesR;
        delete[]m_frameStereoSamples;
        m_soundMonoSamplesIn = nullptr;
        m_soundStereoSamplesOut = nullptr;
        m_frameMixSamplesL = nullptr;
        m_frameMixSamplesR = nullptr;
        m_frameStereoSamples = nullptr;

        for(int i = 0; i < m_maxSounds; i++)
        {
            Sound* pSound = (Sound*)m_sounds.GetPtr(i);
            if( pSound && pSound->ambiStream )
            {
                Sound& sound = *pSound;
                ovrAudio_DestroyAmbisonicStream(sound.ambiStream);
                const int32_t ambisonicOrder = sound.nativeChannels == 4 ? 1 : 
                                             ( sound.nativeChannels == 9 ? 2 : 
                                             ( sound.nativeChannels == 16 ? 3 : 0 ) );
                ovrAudio_CreateAmbisonicStream(sound.nativeRate, spatialProcessSampleCount, 
                    ovrAudioAmbisonicFormat_AmbiX, ambisonicOrder, &sound.ambiStream);
            }
        }
        ovrAudio_DestroyContext( (ovrAudioContext)m_context );
    }

    if(ovrAudio_CreateContext( (ovrAudioContext*)&m_context, &config)!=ovrSuccess)
    {
        //Log("WARNING: Could not create context!\n");
        return false;
    }

    m_frameMixSamplesL = new float[m_spatialProcessSampleCount];
    m_frameMixSamplesR = new float[m_spatialProcessSampleCount];
    m_frameStereoSamples = new int16_t[2*m_spatialProcessSampleCount];
    m_soundMonoSamplesIn = ovrAudio_AllocSamples(m_spatialProcessSampleCount);
    m_soundStereoSamplesOut = ovrAudio_AllocSamples(2*m_spatialProcessSampleCount);

    // turn off everything for now
    ovrResult rx;
    rx = ovrAudio_Enable(m_context, ovrAudioEnable_Doppler_RESERVED, 0);   //Log("Doppler_RESERVED=0: %d\n", rx);
    rx = ovrAudio_Enable(m_context, ovrAudioEnable_SimpleRoomModeling, 0); //Log("SimpleRoomModeling=0: %d\n", rx);
    rx = ovrAudio_Enable(m_context, ovrAudioEnable_LateReverberation, 0);  //Log("LateReverberation=0: %d\n", rx);
    rx = ovrAudio_Enable(m_context, ovrAudioEnable_RandomizeReverb, 0);    //Log("RandomizeReverb=0: %d\n", rx);
    rx = ovrAudio_Enable(m_context, ovrAudioEnable_PerSourceReverb, 0);    //Log("PerSourceReverb=0: %d\n", rx);
    
    // prepare the mixing buffer -- always a 16 bit, stereo buffer at the native sample rate
    const int32_t numChannels = 2;
    const int32_t bytesPerSample = BITS_PER_SAMPLE/8;
    const int32_t mixBufferSizeBytes = m_mixBufferSampleCount * numChannels * bytesPerSample;
    if (m_spatialMixingBuffer)
    {
        ReleaseBuffer(m_spatialMixingBuffer);
    }
    m_spatialMixingBuffer = CreateBuffer(m_directSound, NATIVE_SAMPLE_RATE, BITS_PER_SAMPLE, numChannels, mixBufferSizeBytes );
    if(!m_spatialMixingBuffer)
    {
        //Log("Could not create spatial mixing buffer. Aborting.\n");
        return false;
    }

    int16_t* temp = new int16_t[m_mixBufferSampleCount*2];
    memset(temp, 0, m_mixBufferSampleCount*2*2);
    UpdateBuffer(m_spatialMixingBuffer, temp, m_mixBufferSampleCount*2*2);
    delete[]temp;
    PlayBuffer(m_spatialMixingBuffer, 0, 1.0f, true);

    m_lastSegmentWritten = -1;
#endif
    return true;
}

bool piSoundEngineOVR::Init(void* hwnd, int deviceID, int maxSoundsOverride)
{
    // don't let the array grow. we're currently bound to a fixed number of spatializable samples.
    // we need to separate spatialization to make it a pooled resource which can be run out of separately.
    // we also don't let the array shrink, since that would alter the index, which is maintained for ID
    // by client code.
    m_maxSounds = maxSoundsOverride > 0 ? maxSoundsOverride : kMaxSoundsDefault;
    if(!m_sounds.Init(m_maxSounds, sizeof(Sound*)))
        return false;

    for (int i=  0; i < m_maxSounds; i++)
    {
        m_sounds.AppendPtr(nullptr, false);
    }

    // Initialize the direct sound interface pointer for the default sound device.
    LPCGUID dv = NULL;
    if(deviceID>0) dv = &mDevice[deviceID].mGUID;
    HRESULT result;
    result = DirectSoundCreate8(dv, &m_directSound, NULL);
    if(FAILED(result))
    {
        return false;
    }

    if(!SetupOVRAudio())
    {
        //Log("Could not initialize OVR Audio SDK. Aborting.\n");
        return false;
    }

    // Set the cooperative level to priority so the format of the primary sound buffer can be modified.
    result = m_directSound->SetCooperativeLevel((HWND)hwnd, DSSCL_PRIORITY);
    if(FAILED(result))
    {
        return false;
    }

    // Setup the primary buffer description.
    DSBUFFERDESC bufferDesc;
    bufferDesc.dwSize = sizeof(DSBUFFERDESC);
    bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER|DSBCAPS_CTRLVOLUME;
    bufferDesc.dwBufferBytes = 0;
    bufferDesc.dwReserved = 0;
    bufferDesc.lpwfxFormat = NULL;
    bufferDesc.guid3DAlgorithm = GUID_NULL;

    // Get control of the primary sound buffer on the default sound device.
    result = m_directSound->CreateSoundBuffer(&bufferDesc, &m_primaryBuffer, NULL);
    if(FAILED(result))
    {
        return false;
    }

    // Setup the format of the primary sound buffer.
    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nSamplesPerSec = NATIVE_SAMPLE_RATE;
    waveFormat.wBitsPerSample = BITS_PER_SAMPLE;
    waveFormat.nChannels = 2;
    waveFormat.nBlockAlign = ( waveFormat.wBitsPerSample/8 ) * waveFormat.nChannels;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    // Set the primary buffer to be the wave format specified.
    result = m_primaryBuffer->SetFormat(&waveFormat);
    if(FAILED(result))
    {
        return false;
    }

    if(!SetupOVRContextAndMixBuffer(MIX_BUFFER_SAMPLES, SPATIALIZE_SAMPLES))
    {
        //Log("Could not initialize OVR Context. Aborting.\n");
        return false;
    }

    return true;
}

void piSoundEngineOVR::Deinit()
{
    #ifndef DO_NOT_COMPILE
    ovrAudio_FreeSamples(m_soundMonoSamplesIn);
    ovrAudio_FreeSamples(m_soundStereoSamplesOut);
#endif
    delete[]m_frameMixSamplesL;
    delete[]m_frameMixSamplesR;
    delete[]m_frameStereoSamples;
    m_soundMonoSamplesIn = nullptr;
    m_soundStereoSamplesOut = nullptr;
    m_frameMixSamplesL = nullptr;
    m_frameMixSamplesR = nullptr;
    m_frameStereoSamples = nullptr;

    for(int i = 0; i < m_maxSounds; i++)
    {
        DelSound(i);
    }

    // Release the spatial sound buffer pointer.
    if(m_spatialMixingBuffer)
    {
        m_spatialMixingBuffer->Release();
        m_spatialMixingBuffer = nullptr;
    }

    // Release the primary sound buffer pointer.
    if(m_primaryBuffer)
    {
        m_primaryBuffer->Release();
        m_primaryBuffer = nullptr;
    }

    // Release the direct sound interface pointer.
    if(m_directSound)
    {
        m_directSound->Release();
        m_directSound = nullptr;
    }

    #ifndef DO_NOT_COMPILE
    ovrAudio_DestroyContext(m_context);
    ovrAudio_Shutdown();
    #endif
}

BOOL CALLBACK piSoundEngineOVR::myDSEnumCallback(LPGUID lpGuid, const wchar_t * lpcstrDescription, const wchar_t * lpcstrModule, void * lpContext)
{
    (void)lpcstrModule;
    piSoundEngineOVR *me = (piSoundEngineOVR*)lpContext;
    if(lpGuid==nullptr)
        memset(&me->mDevice[me->mNumDevices].mGUID, 0, sizeof(GUID));
    else
        memcpy(&me->mDevice[me->mNumDevices].mGUID, lpGuid, sizeof(GUID));
    const size_t len = wcslen(lpcstrDescription);
    me->mDevice[me->mNumDevices].mName = (wchar_t *)malloc(( len+2 )*sizeof(wchar_t));
    if(!me->mDevice[me->mNumDevices].mName)
        return FALSE;

    wcscpy_s(me->mDevice[me->mNumDevices].mName, len+2, lpcstrDescription);

    //wcscpy_s( me->mDevice[me->mNumDevices].mName, 63, lpcstrDescription );
    me->mNumDevices++;
    return TRUE;
}

int piSoundEngineOVR::GetNumDevices(void)
{
    mNumDevices = 0;
    DirectSoundEnumerateW(myDSEnumCallback, this);
    return mNumDevices;
}

const wchar_t* piSoundEngineOVR::GetDeviceName(int id) const
{
    return mDevice[id].mName;
}

int piSoundEngineOVR::GetDeviceFromGUID(void *deviceGUID)
{
    GUID *guid = (GUID*)deviceGUID;
    for(int i = 0; i<mNumDevices; i++)
    {
        if(mDevice[i].mGUID.Data1==guid->Data1 &&
           mDevice[i].mGUID.Data2==guid->Data2 &&
           mDevice[i].mGUID.Data3==guid->Data3 &&
           memcmp(mDevice[i].mGUID.Data4, guid->Data4, 8)==0)
        {
            return i;
        }
    }
    return -1;
}

int32_t piSoundEngineOVR::AddSound( const wchar_t* filename )
{
    Sound sound;
    if ( LoadSampleData( m_directSound, filename, sound, m_spatialProcessSampleCount ) )
    {
        // create a sound in an empty slot, return the slot index
        for(int i = 0; i < m_maxSounds; i++)
        {
            if(!m_sounds.GetPtr(i))
            {
                m_sounds.SetPtr(i, new Sound{ sound });
                return i;
            }
        }

        // if we couldn't find a slot, release the data
        ReleaseSampleData( sound );
    }

    return kInvalidSound;
}

void piSoundEngineOVR::DelSound(int32_t soundIndex)
{
    if(IsValid(soundIndex))
    {
        Sound* pSound = (Sound*)m_sounds.GetPtr(soundIndex);
        ReleaseSampleData( *pSound );
        delete pSound;
        m_sounds.SetPtr(soundIndex, nullptr);
    }
}

bool piSoundEngineOVR::Play(int32_t soundIndex)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.readOffset = 0;
        sound.restart = true;
        sound.paused = false;

        if (sound.playMode == Sound::Unspatialized)
        {
            return PlayBuffer(sound.dsBuffer, sound.readOffset, sound.volume, sound.looping);
        }
        return true;
    }
    return false;
}

bool piSoundEngineOVR::Play(int32_t soundIndex, bool loop, float volume)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.looping = loop;
        sound.volume = volume;
        sound.readOffset = 0;
        sound.restart = true;
        sound.paused = false;

        if(sound.playMode == Sound::Unspatialized)
        {
            return PlayBuffer(sound.dsBuffer, sound.readOffset, sound.volume, sound.looping);
        }
        return true;
    }
    return false;
}

bool piSoundEngineOVR::Stop(int32_t soundIndex)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.readOffset = 0;
        sound.restart = true;
        sound.paused = true;

        if(sound.playMode == Sound::Unspatialized)
        {
            return StopBuffer(sound.dsBuffer);
        }
        return true;
    }
    return false;
}

bool piSoundEngineOVR::GetIsPlaying(int32_t soundIndex)
{
    // TODO? incorporate global m_paused
    if(IsValid(soundIndex) && !((Sound*)m_sounds.GetPtr(soundIndex))->paused)
    {
        return true;
    }
    return false;
}

bool piSoundEngineOVR::SetPosition(int32_t soundIndex, const float * position)
{
    if(IsValid(soundIndex))
    {
        memcpy( ((Sound*)m_sounds.GetPtr(soundIndex))->position, position, 3*sizeof(float) );
        return true;
    }
    return false;
}

bool piSoundEngineOVR::SetOrientation(int32_t soundIndex, const float * dir, const float * up)
{
    if(IsValid(soundIndex))
    {
        memcpy( ((Sound*)m_sounds.GetPtr(soundIndex))->dir, dir, 3*sizeof(float) );
        memcpy( ((Sound*)m_sounds.GetPtr(soundIndex))->up, up, 3*sizeof(float) );
        return true;
    }
    return false;
}

bool piSoundEngineOVR::GetSpatialize( int32_t soundIndex ) const
{
    if (IsValid(soundIndex))
    {
        return ( ((Sound*)m_sounds.GetPtr( soundIndex ))->playMode == Sound::Spatialized ) ||
               ( ((Sound*)m_sounds.GetPtr( soundIndex ))->playMode == Sound::Ambisonic );
    }

    return false;
}

bool piSoundEngineOVR::SetSpatialize( int32_t soundIndex, bool spatialize )
{
    if (IsValid(soundIndex))
    {
        Sound& sound = *((Sound*)(m_sounds.GetPtr(soundIndex)));

        // currently ambisonic sounds are created ambisonic and never change.
        if( sound.playMode == Sound::Ambisonic ) return false;

        bool spatializeChanged = (sound.playMode == Sound::Spatialized) != spatialize;
        sound.playMode = spatialize ? Sound::Spatialized : Sound::Unspatialized;
        if ( spatializeChanged && !sound.paused )
        {
            if ( spatialize)
            {
                // convert the directsound buffer byte offset (stereo 16) into mono sample count
                int32_t playOffsetBytes = sound.paused ? 0 : GetBufferPlayOffsetBytes(sound.dsBuffer);
                sound.readOffset = Max(Min(int32_t(playOffsetBytes/sound.dsBufferBytesPerNativeMonoSample), sound.monoSampleCount-1), 0);
                return StopBuffer(sound.dsBuffer);
            }
            else
            {
                // convert the mono sample count into directsound buffer byte offset (stereo 16)
                sound.readOffset = Max(Min(int32_t(sound.readOffset * sound.dsBufferBytesPerNativeMonoSample), sound.dsBufferByteSize - int32_t(sound.dsBufferBytesPerNativeMonoSample)), 0);
                return PlayBuffer(sound.dsBuffer, sound.readOffset, sound.volume, sound.looping);
            }
        }
        return true;
    }
    return false;
}

bool piSoundEngineOVR::CanChangeSpatialize(int32_t soundIndex) const
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        return sound.playMode != Sound::Ambisonic &&
            sound.monoFloats && sound.dsBuffer;
    }

    return false;
}

bool piSoundEngineOVR::GetLooping( int32_t soundIndex ) const
{
    if (IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr( soundIndex ))->looping;
    }

    return false;
}

bool piSoundEngineOVR::SetLooping(int32_t soundIndex, bool looping)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        bool const loopingChanged = sound.looping!=looping;
        sound.looping = looping;
        if(sound.playMode == Sound::Unspatialized && loopingChanged && !sound.paused)
        {
            sound.readOffset = GetBufferPlayOffsetBytes(sound.dsBuffer);
            return PlayBuffer(sound.dsBuffer, sound.readOffset, sound.volume, sound.looping);
        }
        return true;
    }
    return false;
}

bool piSoundEngineOVR::GetPaused( int32_t soundIndex ) const
{
    if (IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr( soundIndex ))->paused;
    }

    return false;
}

bool piSoundEngineOVR::SetPaused(int32_t soundIndex, bool paused)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        bool const pauseChanged = sound.paused != paused;
        sound.paused = paused;

        if (sound.playMode == Sound::Unspatialized && pauseChanged)
        {
            if ( paused )
            {
                sound.readOffset = GetBufferPlayOffsetBytes(sound.dsBuffer);
                return StopBuffer(sound.dsBuffer);
            }
            else
            {
                return PlayBuffer(sound.dsBuffer, sound.readOffset, sound.volume, sound.looping);
            }
        }

        return true;
    }
    return false;
}

float piSoundEngineOVR::GetVolume( int32_t soundIndex ) const
{
    if (IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr( soundIndex ))->volume;
    }

    return 0;
}

bool piSoundEngineOVR::SetVolume(int32_t soundIndex, float volume)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.volume = volume;
        if(sound.playMode==Sound::Unspatialized)
        {
            return SetBufferVolume(sound.dsBuffer, sound.volume);
        }
        return true;
    }
    return false;
}

int32_t piSoundEngineOVR::GetAttenMode(int32_t soundIndex) const
{
    if(IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr(soundIndex))->attenMode;
    }
    return 0;
}

bool piSoundEngineOVR::SetAttenMode(int32_t soundIndex, int32_t attenMode)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.attenMode = attenMode;
        return true;
    }
    return false;
}

float piSoundEngineOVR::GetAttenRadius(int32_t soundIndex) const
{
    if(IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr(soundIndex))->attenRadius;
    }
    return 0;
}

bool piSoundEngineOVR::SetAttenRadius(int32_t soundIndex, float attenRadius)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.attenRadius = attenRadius;
        return true;
    }
    return false;
}

float piSoundEngineOVR::GetAttenMin(int32_t soundIndex) const
{
    if(IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr(soundIndex))->attenRange[0];
    }
    return 0;
}

bool piSoundEngineOVR::SetAttenMin(int32_t soundIndex, float attenMin)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.attenRange[0] = attenMin;
        return true;
    }
    return false;
}

float piSoundEngineOVR::GetAttenMax(int32_t soundIndex) const
{
    if(IsValid(soundIndex))
    {
        return ((Sound*)m_sounds.GetPtr(soundIndex))->attenRange[1];
    }
    return 0;
}

bool piSoundEngineOVR::SetAttenMax(int32_t soundIndex, float attenMax)
{
    if(IsValid(soundIndex))
    {
        Sound& sound = *(Sound*)m_sounds.GetPtr(soundIndex);
        sound.attenRange[1] = attenMax;
        return true;
    }
    return false;
}

void piSoundEngineOVR::PauseAllSounds()
{
    if(m_paused) return;

    for ( int i = 0; i < m_maxSounds; i++)
    {
        // stop all currently playing non-spatialized sounds
        if( IsValid(i) )
        {
            Sound& sound = *((Sound*)m_sounds.GetPtr(i));
            if( sound.playMode == Sound::Unspatialized && !sound.paused )
            {
                sound.readOffset = GetBufferPlayOffsetBytes(sound.dsBuffer);
                StopBuffer(sound.dsBuffer);
            }
        }
    }

    m_spatialMixingBuffer->Stop();
    int16_t* nothing = new int16_t[2*m_mixBufferSampleCount];
    memset(nothing, 0, 2*2*m_mixBufferSampleCount);
    CopyToBuffer(m_spatialMixingBuffer, 0, m_mixBufferSampleCount, nothing);
    delete[]nothing;

    m_lastSegmentWritten = -1;
    m_paused = true;
}

void piSoundEngineOVR::ResumeAllSounds()
{
    if(!m_paused) return;

    for(int i = 0; i < m_maxSounds; i++)
    {
        // resume all currently playing non-spatialized sounds
        if( IsValid(i))
        {
            Sound& sound = *((Sound*)m_sounds.GetPtr(i));
            if( sound.playMode == Sound::Unspatialized && !sound.paused )
            {
                PlayBuffer(sound.dsBuffer, sound.readOffset, sound.volume, sound.looping);
            }
        }
    }

    m_paused = false;
    PlayBuffer( m_spatialMixingBuffer, 0, 1.0f, true );
}


void piSoundEngineOVR::Tick(double dt)
{
#ifndef DO_NOT_COMPILE

    (void)dt;
    if(m_paused) return;

    // the ovr api takes a listener position, but the whole api is float based
    // we'll use the default listener transform (at origin, facing down -z, y-up),
    // but do all the math in double format.
    //ovrPoseStatef poseState = { 0 };
    //poseState.ThePose.Orientation = *( (ovrQuatf*)( &m_listenerQuat[0] ) );
    //poseState.ThePose.Position = *( (ovrVector3f*)( &m_listenerPos[0] ) );
    //ovrAudio_SetListenerPoseStatef(m_context, &poseState);

    int32_t playOffset = 0;
    int32_t writeOffset = 0;
    int32_t writeCount = 0;
    GetMixBufferCursors(m_spatialMixingBuffer, m_mixBufferSampleCount, playOffset, writeOffset, writeCount);

    int32_t nextWriteSegment = ( writeOffset % m_spatialProcessSampleCount )==0 ? ( writeOffset/m_spatialProcessSampleCount ) : ( ( writeOffset/m_spatialProcessSampleCount )+1 );
    int32_t numSegments = m_mixBufferSampleCount/m_spatialProcessSampleCount;
    nextWriteSegment %= numSegments;
    if (nextWriteSegment==m_lastSegmentWritten)
    {
        return;
    }
    m_lastSegmentWritten = nextWriteSegment;
    writeOffset = nextWriteSegment * m_spatialProcessSampleCount;

    // always write in m_spatialProcessSampleCount increments
    //if(writeCount<m_spatialProcessSampleCount) Log("low write count (%d/%d)\n", writeCount, m_spatialProcessSampleCount);
    writeCount = Min(writeCount, m_spatialProcessSampleCount);

    memset(m_frameMixSamplesL, 0, m_spatialProcessSampleCount*sizeof(float));
    memset(m_frameMixSamplesR, 0, m_spatialProcessSampleCount*sizeof(float));
    for(int i = 0; i < m_maxSounds; i++)
    {
        if(Sound* sound = (Sound*)m_sounds.GetPtr(i))
        {
            if (sound->playMode == Sound::Unspatialized && !sound->paused)
            {
                sound->paused = !GetBufferIsPlaying(sound->dsBuffer);
                continue;
            }

            // we only manually process spatialized sounds that have valid data and are currently playing 
            if(!( ( sound->playMode == Sound::Ambisonic && sound->ambiFloats ) || 
                  ( sound->playMode == Sound::Spatialized && sound->monoFloats ) ) || sound->paused) continue;

            int32_t delta = m_spatialProcessSampleCount;
            if(sound->restart)
            {
                sound->restart = false;
                delta = 0;
            }

            // update the read offset
            sound->readOffset += delta;
            if(sound->looping)
            {
                sound->readOffset %= sound->monoSampleCount;
            }
            else
            {
                // if the read offset is beyond the end of the buffer, then this sound
                // should no longer be mixed in.
                if(sound->readOffset>=sound->monoSampleCount)
                {
                    sound->readOffset = sound->monoSampleCount;
                    sound->paused = true;
                }
            }
            if (sound->ambiFloats)
            {

                // we'd like to process these just like normal positional sound, but the
                // ovr api only supports specifying listener orientation. here we re-create
                // object-to-view by creating a transposed matrix of the object orientation 
                // (so the inverse) and concatenate with the view-to-world.
                vec3d side = normalize(cross(sound->up, sound->dir));
                mat4x4d world2obj(side.x, side.y, side.z, 0,
                                  sound->up.x, sound->up.y, sound->up.z, 0,
                                  sound->dir.x, sound->dir.y, sound->dir.z, 0,
                                  0, 0, 0, 1);
                mat4x4d view2obj = world2obj * m_view2world;
                vec3 dirf = d2f((view2obj * vec4d(0, 0, 1, 0)).xyz());
                vec3 upf = d2f((view2obj * vec4d(0, 1, 0, 0) ).xyz());

                ovrAudio_SetAmbisonicListenerOrientation( sound->ambiStream,
                   dirf.x, dirf.y, dirf.z, upf.x, upf.y, upf.z );

                // process current chunk of ambisonic stream, accounting for looping
                if (sound->readOffset + m_spatialProcessSampleCount < sound->monoSampleCount)
                {
                    // can do it in 1 chunk
                    ovrAudio_ProcessAmbisonicStreamInterleaved( sound->ambiStream,
                        sound->ambiFloats + AMBISONIC_CHANNELS * sound->readOffset, m_soundStereoSamplesOut, m_spatialProcessSampleCount);
                }
                else
                {
                    // need 2 operations to handle buffer end
                    int32_t samplesAtEnd = sound->monoSampleCount - sound->readOffset;
                    int32_t remainderSamples = m_spatialProcessSampleCount - samplesAtEnd;
                    ovrAudio_ProcessAmbisonicStreamInterleaved( sound->ambiStream,
                        sound->ambiFloats + AMBISONIC_CHANNELS * sound->readOffset, m_soundStereoSamplesOut, samplesAtEnd);
                    if (sound->looping)
                    {
                        ovrAudio_ProcessAmbisonicStreamInterleaved( sound->ambiStream,
                            sound->ambiFloats, m_soundStereoSamplesOut + 2*samplesAtEnd, remainderSamples);
                    }
                    else
                    {
                        memset(m_soundStereoSamplesOut + 2*samplesAtEnd, 0, sizeof(float)*2*remainderSamples);
                    }
                }

                for(int j = 0; j<writeCount; j++)
                {
                    m_frameMixSamplesL[j] += m_soundStereoSamplesOut[2*j+0];
                    m_frameMixSamplesR[j] += m_soundStereoSamplesOut[2*j+1];
                }

                continue;
            }
            // update sound attenuation mode and range
            if(sound->attenMode ==ovrAudioSourceAttenuationMode_InverseSquare)
            {
                const float fixedScaleUnused = -1.0f;
                ovrAudio_SetAudioSourceAttenuationMode(m_context, i,
                    ovrAudioSourceAttenuationMode_InverseSquare, fixedScaleUnused);
                ovrAudio_SetAudioSourceRange(m_context, i, sound->attenRange[0], sound->attenRange[1]);
            }
            else
            {
                const float fixedScaleUnused = -1.0f;
                ovrAudio_SetAudioSourceAttenuationMode(m_context, i,
                    ovrAudioSourceAttenuationMode_None, fixedScaleUnused);
            }

            ovrAudio_SetAudioSourcePropertyf(m_context, i, ovrAudioSourceProperty_Diameter, 2*sound->attenRadius);

            // update sound position
            vec3 posf = d2f(( m_world2view * vec4d(sound->position, 1) ).xyz());
            ovrAudio_SetAudioSourcePos(m_context, i, posf.x, posf.y, posf.z);

            // Grabs the next chunk of data from the sound, looping etc. 
            // as necessary.  This is application specific code.
            memset(m_soundMonoSamplesIn, 0, m_spatialProcessSampleCount*sizeof(float));
            for(int j = 0; j<writeCount; j++)
            {
                int32_t curReadSample = sound->readOffset+j;
                if(curReadSample>=sound->monoSampleCount)
                {
                    if(sound->looping)
                    {
                        curReadSample -= sound->monoSampleCount;
                    }
                    else
                    {
                        break;
                    }
                }

                m_soundMonoSamplesIn[j] = AdjustSampleVolume(sound->monoFloats[curReadSample], sound->volume);
            }

            // Spatialize the sound into the output buffer.  Note that there
            // are two APIs, one for interleaved sample data and another for
            // separate left/right sample data
            uint32_t flagsIn = ovrAudioSpatializationFlag_None;
            uint32_t statusOut = 0;
            memset(m_soundStereoSamplesOut, 0, 2*m_spatialProcessSampleCount*sizeof(float));

            ovrAudio_SpatializeMonoSourceInterleaved(m_context,
                                                        i,
                                                        flagsIn, &statusOut,
                                                        m_soundStereoSamplesOut, m_soundMonoSamplesIn);

            for(int j = 0; j<writeCount; j++)
            {
                m_frameMixSamplesL[j] += m_soundStereoSamplesOut[2*j+0];
                m_frameMixSamplesR[j] += m_soundStereoSamplesOut[2*j+1];
            }
        }
    }

    // finally, copy mixdata into output buffer, from write position to play position
    for(int i = 0; i<writeCount; i++)
    {
        m_frameStereoSamples[i*2+0] = Clamp< int16_t >((int16_t)( m_frameMixSamplesL[i]*32768.0f ), -32768, 32767);
        m_frameStereoSamples[i*2+1] = Clamp< int16_t >((int16_t)( m_frameMixSamplesR[i]*32768.0f ), -32768, 32767);
    }

    CopyToBuffer(m_spatialMixingBuffer, writeOffset, writeCount, m_frameStereoSamples);
#endif
}


} // namespace piLibs


