#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "GameInput.h"
#include "BufferManager.h"
#include "CommandContext.h"

#pragma comment(lib, "runtimeobject.lib")

namespace GameCore
{
    using namespace Graphics;

    void InitializeApplication(IGameApp& game)
    {
        Graphics::Initialize();
        SystemTime::Initialize();
        GameInput::Initialize();
        EngineTuning::Initialize();

        game.Startup();
    }

    void TerminateApplication(IGameApp& game)
    {
        game.Cleanup();

        GameInput::Shutdown();
    }

    bool UpdateApplication(IGameApp& game)
    {
        EngineProfiling::Update();

        float DeltaTime = Graphics::GetFrameTime();

        GameInput::Update(DeltaTime);
        EngineTuning::Update(DeltaTime);

        game.Update(DeltaTime);
        game.RenderScene();

        //!

        GraphicsContext& UiContext = GraphicsContext::Begin(L"Render UI");
        UiContext.TransitionResource(g_OverlayBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        UiContext.ClearColor(g_OverlayBuffer);
        UiContext.SetRenderTarget(g_OverlayBuffer.GetRTV());
        UiContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
        game.RenderUI(UiContext);

        EngineTuning::Display(UiContext, 10.0f, 40.0f, 1900.0f, 1040.0f);

        UiContext.Finish();

        Graphics::Present();

        return !game.IsDone();
    }

    bool IGameApp::IsDone(void)
    {
        return GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_escape);
    }

    HWND g_hWnd = nullptr;

    LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void RunApplication([[maybe_unused]] IGameApp& app, const wchar_t* className)
    {
        Microsoft::WRL::Wrappers::RoInitializeWrapper InitializeWinRT(RO_INIT_MULTITHREADED);
        ASSERT_SUCCEEDED(InitializeWinRT);

        HINSTANCE hInst = GetModuleHandle(0);

        // Register class
        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInst;
        wcex.hIcon = LoadIcon(hInst, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = className;
        wcex.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
        ASSERT(0 != RegisterClassEx(&wcex), "Unable to register a window");

        RECT rc = { 0, 0, static_cast<LONG>(g_DisplayWidth), static_cast<LONG>(g_DisplayHeight) };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

        ASSERT(g_hWnd != 0);

        InitializeApplication(app);

        ShowWindow(g_hWnd, SW_SHOWDEFAULT);

        do
        {
            MSG msg = {};
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT)
                goto exit;
        }
        while (UpdateApplication(app));

        exit:
        Graphics::Terminate();
        TerminateApplication(app);
        Graphics::Shutdown();
    }

    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch(message)
        {
            case WM_SIZE:
                Graphics::Resize((UINT)(UINT64)lParam&0xFFFF, (UINT)(UINT64)lParam >> 16);
                break;

            case WM_DESTROY:
                PostQuitMessage(0);
                break;

            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
        }

        return 0;
    }
}