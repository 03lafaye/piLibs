#include "libLog/piLog.h"
#include "libSystem/piStr.h"
#include "libDataUtils/piString.h"
#include "libDataUtils/piArray.h"
#include "libRender/piRenderer.h"
#include "libMisc/piRenderMesh/piRenderMesh.h"

#include "demo.h"
#include "global.h"

Demo::Demo()
{
}
Demo::~Demo()
{
}

bool Demo::Init(piRenderer *renderer, piLog *log, const ivec2 & res )
{
    return true;
}

void Demo::Deinit(piRenderer *renderer, piLog *log)
{
}


void Demo::GlobalRender(piRenderer *renderer, piLog *log, double time )
{
    mTime = time;
}

void Demo::EyeRender(piRenderer *renderer, piLog * log, piRTarget target )
{
    const vec3 col = 0.5f + 0.5f*cos( float(mTime) + vec3(0.0f, 2.0f, 4.0f ) );
    const float color0[4] = { col.x, col.y, col.z, 1.0f };
    renderer->Clear( color0, nullptr, nullptr, nullptr, false );
}

