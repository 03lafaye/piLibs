#include "libLog/piLog.h"
#include "libSystem/piWindow.h"
#include "libSystem/piTimer.h"
#include "libSystem/piStr.h"
#include "libRender/piRenderer.h"
#include "libDataUtils/piString.h"
#include "libMath/piVecTypes.h"
#include "demo.h"

#include "global.h"


//----------------------------------------------------------------------------------

class MainRenderReporter : public piRenderReporter
{
private:
    piLog *mLog;

public:
    MainRenderReporter(piLog *log) { mLog = log; }
    virtual ~MainRenderReporter() {}
    void Info( const char *str )
    {
        piString wstr; wstr.InitCopyS(str);
        mLog->Printf(LT_EXTRA_PARAMS, LT_MESSAGE, L"%s", wstr.GetS());
        wstr.End();
    }
    void Error( const char *str, int level )
    {
        piString wstr; wstr.InitCopyS(str);
        mLog->Printf(LT_EXTRA_PARAMS, LT_ERROR, L"%s", wstr.GetS());
        wstr.End();
    }
    void Begin(uint64 memCurrent, uint64 memPeak, int texCurrent, int texPeak)
    {
        mLog->Printf(LT_EXTRA_PARAMS, LT_MESSAGE, L"---- Renderer Report ---- ");
        mLog->Printf(LT_EXTRA_PARAMS, LT_MESSAGE, L"Peak: %dI64 MB in %d textures", memPeak >> 20, texPeak);
        mLog->Printf(LT_EXTRA_PARAMS, LT_MESSAGE, L"Curr: %dI64 MB in %d textures", memCurrent >> 20, texCurrent);
    }
    void End( void )
    {
        mLog->Printf(LT_EXTRA_PARAMS, LT_MESSAGE, L"---- Renderer Report ---- ");
    }
    void Texture(const wchar_t *key, uint64 kb, piTextureFormat format, bool compressed, int xres, int yres, int zres)
    {
        mLog->Printf(LT_EXTRA_PARAMS, LT_MESSAGE, L"* Texture: %5dI64 MB, %4d x %4d x %4d %2d (%s)", kb>>10, xres, yres, zres, format, key);
    }
};

void * piMainCrashHandlerFunc(void)
{
    return nullptr;
}

int piMainFunc( const wchar_t *path, const wchar_t **args, int numargs, void *instance )
{
    piLog  mLog;

    if (!mLog.Init(L"debug.txt", 1 + 2))
        return 0;

    ivec2 wres = ivec2(1600, 900);

    piWindowMgr mWinMgr = piWindowMgr_Init();
    if (!mWinMgr)
        return 0;

    ivec2 wo = ivec2(0, 0);
    piWindow mWindow = piWindow_init(mWinMgr, L"rendering", wo.x, wo.y, wres.x, wres.y, false, true, false, false);
    if (!mWindow)
        return 0;

    piWindow_show( mWindow );

    // renderer
    piRenderReporter *mRenderReporter = new MainRenderReporter(&mLog);

    piRenderer *mRenderer = piRenderer::Create(GL);
    if (!mRenderer)
        return 0;

    // renderer
    const void *hwnds[1] = { piWindow_getHandle(mWindow) };
    if (!mRenderer->Initialize(0, hwnds, 1, true, mRenderReporter))
    {
        mLog.Printf(LT_EXTRA_PARAMS, LT_ERROR, L"Can't create an OpenGL 4.4 context");
        return 0;
    }
    mRenderer->SetActiveWindow(0);

    Demo *mDemo = new Demo();

    if (!mDemo->Init(mRenderer, &mLog, wres))
    {
        mLog.Printf(LT_EXTRA_PARAMS, LT_ERROR, L"Can't init demo");
        return 0;
    }


    double to = piTimer_GetTime();
    double renderFpsTo = 0.0;
    int    renderFrame = 0;
    float  renderFps = 0.0;
    int    totalFrames = 0;
    int    done = 0;

    while( !done )
    {
        const double time = piTimer_GetTime() - to;

        // events
        piWindowEvents_Erase(mWindow);
        piWindowMgr_MessageLoop(mWinMgr);
        done |= piWindow_getExitReq(mWindow);
        piWindowEvents *evt = piWindow_getEvents(mWindow);
        piWindowEvents_GetMouse_D(&evt->mouse);

        if (evt->keyb.key['a']) { evt->keyb.key['a'] = 0;  }
        if (evt->keyb.key['s']) { evt->keyb.key['s'] = 0;  }

        mDemo->GlobalRender(mRenderer, &mLog, time);
        mDemo->EyeRender(mRenderer, &mLog, nullptr);
        mRenderer->SwapBuffers();

        totalFrames++;

        // update fps counter
        renderFrame++;
        const double dt = time - renderFpsTo;
        if (dt > 1.0)
        {
            renderFps = (float)renderFrame / (float)dt;
            renderFrame = 0;
            renderFpsTo = time;
        }
        if ((totalFrames & 15) == 0)
        {
            wchar_t str[64];
            piwsprintf(str, 64, L"%.1f fps   ::   %.2f", renderFps, time);
            piWindow_setText(mWindow, str);
        }
    }

    mDemo->Deinit(mRenderer, &mLog);

    if (mRenderer != nullptr )
    {
        mRenderer->Report();

        if (mRenderReporter != nullptr) delete mRenderReporter;
        mRenderer->Deinitialize();
        delete mRenderer;
    }

    if (mWindow != nullptr ) piWindow_end(mWindow);
    if (mWinMgr != nullptr ) piWindowMgr_End(mWinMgr);

    mLog.End();

    return 1;
}
