#include "../piSound.h"
#include "piSoundEngineDS.h"
#include "piSoundEngineOVR.h"

namespace piLibs {

piSoundEngine::piSoundEngine()
{
}

piSoundEngine::~piSoundEngine()
{
}

piSoundEngine * piSoundEngine::Create(bool forVR)
{
    if( forVR )
        return new piSoundEngineOVR();

    return new piSoundEngineDS();
}

}