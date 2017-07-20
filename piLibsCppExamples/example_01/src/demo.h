#pragma once

#include "libLog/piLog.h"
#include "libRender/piRenderer.h"
#include "libMath/piVecTypes.h"

#include "global.h"

class Demo
{
public:
    Demo();
    ~Demo();

    bool Init(piRenderer *renderer, piLog *log, const ivec2 & res );
    void Deinit(piRenderer *renderer, piLog *log);
    void GlobalRender(piRenderer *renderer, piLog *log, double time );
    void EyeRender(piRenderer *renderer, piLog *log, piRTarget renderTarget );

private:
    double mTime;
};
