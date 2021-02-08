#pragma once

#include "pch.h"

namespace GameCore
{
    class IGameApp
    {
    public:
        virtual ~IGameApp() {}

        virtual void Startup(void) = 0;
        virtual void Cleanup(void) = 0;

        virtual bool IsDone(void);

        virtual void Update(float deltaT) = 0;

        virtual void RenderScene(void) = 0;

        virtual void RenderUI(class GraphicsContext&) {};
    };

    void RunApplication(IGameApp& app, const wchar_t* className);
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    #define MAIN_FUNCTION() int wmain(/* argc, wchar_t** argv*/)
#else
    #define MAIN_FUNCTION()  [Platform::MTAThread] int main(Platform::Array<Platform::String^>^)
#endif

#define CREATE_APPLICATION( app_class ) \
    MAIN_FUNCTION() \
    { \
        IGameApp* app = new app_class(); \
        GameCore::RunApplication(*app, L#app_class); \
        delete app; \
        return 0; \
    }
