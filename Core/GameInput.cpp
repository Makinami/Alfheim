#include "pch.h"
#include "GameCore.h"
#include "GameInput.h"

#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;

namespace GameCore
{
    extern HWND g_hWnd;
}

namespace
{
	bool s_Buttons[2][static_cast<uint32_t>(GameInput::DigitalInput::kNumDigitalInputs)];
    float s_HoldDuration[static_cast<int>(GameInput::DigitalInput::kNumDigitalInputs)] = { 0.0f };
	float s_Analogs[static_cast<uint32_t>(GameInput::AnalogInput::kNumAnalogInputs)];
    float s_AnalogsTC[static_cast<int>(GameInput::AnalogInput::kNumAnalogInputs)];

    ComPtr<IDirectInput8A> s_DI;
    ComPtr<IDirectInputDevice8A> s_Keyboard;
    ComPtr<IDirectInputDevice8A> s_Mouse;

    DIMOUSESTATE2 s_MouseState;
    unsigned char s_Keybuffer[256];
	unsigned char s_DXKeyMapping[static_cast<uint32_t>(GameInput::DigitalInput::kNumKeys)];

    float FilterAnalogInput(int val, int deadZone)
    {
        if (val < 0)
        {
            if (val > -deadZone)
                return 0.0f;
            else
                return (val + deadZone) / (32768.f - deadZone);
        }
        else
        {
            if (val < deadZone)
                return 0.0f;
            else
                return (val - deadZone) / (32767.0f - deadZone);
        }
    }

	void KbmBuildKeyMapping()
	{
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_escape)] = 1;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_1)] = 2;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_2)] = 3;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_3)] = 4;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_4)] = 5;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_5)] = 6;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_6)] = 7;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_7)] = 8;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_8)] = 9;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_9)] = 10;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_0)] = 11;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_minus)] = 12;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_equals)] = 13;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_back)] = 14;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_tab)] = 15;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_q)] = 16;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_w)] = 17;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_e)] = 18;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_r)] = 19;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_t)] = 20;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_y)] = 21;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_u)] = 22;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_i)] = 23;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_o)] = 24;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_p)] = 25;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_lbracket)] = 26;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_rbracket)] = 27;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_return)] = 28;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_lcontrol)] = 29;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_a)] = 30;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_s)] = 31;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_d)] = 32;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f)] = 33;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_g)] = 34;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_h)] = 35;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_j)] = 36;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_k)] = 37;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_l)] = 38;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_semicolon)] = 39;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_apostrophe)] = 40;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_grave)] = 41;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_lshift)] = 42;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_backslash)] = 43;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_z)] = 44;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_x)] = 45;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_c)] = 46;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_v)] = 47;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_b)] = 48;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_n)] = 49;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_m)] = 50;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_comma)] = 51;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_period)] = 52;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_slash)] = 53;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_rshift)] = 54;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_multiply)] = 55;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_lalt)] = 56;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_space)] = 57;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_capital)] = 58;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f1)] = 59;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f2)] = 60;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f3)] = 61;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f4)] = 62;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f5)] = 63;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f6)] = 64;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f7)] = 65;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f8)] = 66;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f9)] = 67;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f10)] = 68;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numlock)] = 69;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_scroll)] = 70;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad7)] = 71;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad8)] = 72;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad9)] = 73;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_subtract)] = 74;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad4)] = 75;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad5)] = 76;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad6)] = 77;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_add)] = 78;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad1)] = 79;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad2)] = 80;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad3)] = 81;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpad0)] = 82;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_decimal)] = 83;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f11)] = 87;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_f12)] = 88;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_numpadenter)] = 156;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_rcontrol)] = 157;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_divide)] = 181;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_sysrq)] = 183;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_ralt)] = 184;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_pause)] = 197;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_home)] = 199;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_up)] = 200;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_pgup)] = 201;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_left)] = 203;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_right)] = 205;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_end)] = 207;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_down)] = 208;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_pgdn)] = 209;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_insert)] = 210;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_delete)] = 211;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_lwin)] = 219;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_rwin)] = 220;
        s_DXKeyMapping[static_cast<int>(GameInput::DigitalInput::kKey_apps)] = 221;
	}

    void KbmZeroInputs()
    {
        memset(&s_MouseState, 0, sizeof(DIMOUSESTATE2));
        memset(s_Keybuffer, 0, sizeof(s_Keybuffer));
    }

	void KbmInitialize()
	{
		KbmBuildKeyMapping();

        if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&s_DI, nullptr)))
            ASSERT(false, "DirectInput8 initialization failed.");

        if (FAILED(s_DI->CreateDevice(GUID_SysKeyboard, &s_Keyboard, nullptr)))
            ASSERT(false, "Keyboard CreateDevice failed");
        if (FAILED(s_Keyboard->SetDataFormat(&c_dfDIKeyboard)))
            ASSERT(false, "Keyboard SetDataFormat failed.");
        if (FAILED(s_Keyboard->SetCooperativeLevel(GameCore::g_hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
            ASSERT(false, "Keyboard SetCooperativeLevel failed.");

        DIPROPDWORD dipdw;
        dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj = 0;
        dipdw.diph.dwHow = DIPH_DEVICE;
        dipdw.dwData = 10;
        if (FAILED(s_Keyboard->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
            ASSERT(false, "Keyboard set buffer size failed");

        if (FAILED(s_DI->CreateDevice(GUID_SysMouse, &s_Mouse, nullptr)))
            ASSERT(false, "Mouse CreateDevice failed.");
        if (FAILED(s_Mouse->SetDataFormat(&c_dfDIMouse2)))
            ASSERT(false, "Mouse SetDataFormat failed.");
        if (FAILED(s_Mouse->SetCooperativeLevel(GameCore::g_hWnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE)))
            ASSERT(false, "Mouse SetCooperativeLevel failed.");

        KbmZeroInputs();
	}

    void KbmShutdown()
    {
        if (s_Keyboard)
        {
            s_Keyboard->Unacquire();
            s_Keyboard = nullptr;
        }

        if (s_Mouse)
        {
            s_Mouse->Unacquire();
            s_Mouse = nullptr;
        }

        s_DI = nullptr;
    }

    void KbmUpdate()
    {
        HWND foreground = GetForegroundWindow();
        bool visible = IsWindowVisible(foreground);

        if (foreground != GameCore::g_hWnd // wouldn't be able to acquire
            || !visible)
        {
            KbmZeroInputs();
        }
        else
        {
            s_Mouse->Acquire();
            s_Mouse->GetDeviceState(sizeof(DIMOUSESTATE2), &s_MouseState);
            s_Keyboard->Acquire();
            s_Keyboard->GetDeviceState(sizeof(s_Keybuffer), s_Keybuffer);
        }
    }
}

void GameInput::Initialize()
{
	ZeroMemory(s_Buttons, sizeof(s_Buttons));
	ZeroMemory(s_Analogs, sizeof(s_Analogs));

	KbmInitialize();
}

void GameInput::Shutdown()
{
    KbmShutdown();
}

void GameInput::Update(float frameDelta)
{
    memcpy(s_Buttons[1], s_Buttons[0], sizeof(s_Buttons[0]));
    memset(s_Buttons[0], 0, sizeof(s_Buttons[0]));
    memset(s_Analogs, 0, sizeof(s_Analogs));

    XINPUT_STATE newInputState;
    if (ERROR_SUCCESS == XInputGetState(0, &newInputState))
    {
        if (newInputState.Gamepad.wButtons & (1 << 0)) s_Buttons[0][static_cast<size_t>(DigitalInput::kDPadUp)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 1)) s_Buttons[0][static_cast<size_t>(DigitalInput::kDPadDown)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 2)) s_Buttons[0][static_cast<size_t>(DigitalInput::kDPadLeft)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 3)) s_Buttons[0][static_cast<size_t>(DigitalInput::kDPadRight)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 4)) s_Buttons[0][static_cast<size_t>(DigitalInput::kStartButton)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 5)) s_Buttons[0][static_cast<size_t>(DigitalInput::kBackButton)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 6)) s_Buttons[0][static_cast<size_t>(DigitalInput::kLThumbClick)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 7)) s_Buttons[0][static_cast<size_t>(DigitalInput::kRThumbClick)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 8)) s_Buttons[0][static_cast<size_t>(DigitalInput::kLShoulder)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 9)) s_Buttons[0][static_cast<size_t>(DigitalInput::kRShoulder)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 12)) s_Buttons[0][static_cast<size_t>(DigitalInput::kAButton)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 13)) s_Buttons[0][static_cast<size_t>(DigitalInput::kBButton)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 14)) s_Buttons[0][static_cast<size_t>(DigitalInput::kYButton)] = true;
        if (newInputState.Gamepad.wButtons & (1 << 15)) s_Buttons[0][static_cast<size_t>(DigitalInput::kYButton)] = true;

        s_Analogs[static_cast<size_t>(AnalogInput::kAnalogLeftTrigger)] = newInputState.Gamepad.bLeftTrigger / 255.0f;
        s_Analogs[static_cast<size_t>(AnalogInput::kAnalogRightTrigger)] = newInputState.Gamepad.bRightTrigger / 255.0f;
        s_Analogs[static_cast<size_t>(AnalogInput::kAnalogLeftStickX)] = FilterAnalogInput(newInputState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        s_Analogs[static_cast<size_t>(AnalogInput::kAnalogLeftStickY)] = FilterAnalogInput(newInputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        s_Analogs[static_cast<size_t>(AnalogInput::kAnalogRightStickX)] = FilterAnalogInput(newInputState.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        s_Analogs[static_cast<size_t>(AnalogInput::kAnalogRightStickY)] = FilterAnalogInput(newInputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    }

    KbmUpdate();

    for (uint32_t i = 0; i < static_cast<int>(DigitalInput::kNumKeys); ++i)
    {
        s_Buttons[0][i] = (s_Keybuffer[s_DXKeyMapping[i]] & 0x80) != 0;
    }

    for (uint32_t i = 0; i < 8; ++i)
    {
        if (s_MouseState.rgbButtons[i] > 0) s_Buttons[0][static_cast<int>(DigitalInput::kMouse0) + i] = true;
    }

    s_Analogs[static_cast<int>(AnalogInput::kAnalogMouseX)] = (float)s_MouseState.lX * .0018f;
    s_Analogs[static_cast<int>(AnalogInput::kAnalogMouseY)] = (float)s_MouseState.lY * -.0018f;

    if (s_MouseState.lZ > 0)
        s_Analogs[static_cast<int>(AnalogInput::kAnalogMouseScroll)] = 1.0;
    else if (s_MouseState.lZ < 0)
        s_Analogs[static_cast<int>(AnalogInput::kAnalogMouseScroll)] = -1.0;

    for (uint32_t i = 0; i < static_cast<int>(DigitalInput::kNumDigitalInputs); ++i)
    {
        if (s_Buttons[0][i])
        {
            if (!s_Buttons[1][i])
                s_HoldDuration[i] = 0.0f;
            else
                s_HoldDuration[i] += frameDelta;
        }
    }

    for (uint32_t i = 0; i < static_cast<int>(AnalogInput::kNumAnalogInputs); ++i)
    {
        s_AnalogsTC[i] = s_Analogs[i] * frameDelta;
    }
}

bool GameInput::IsPressed(DigitalInput di)
{
    return s_Buttons[0][static_cast<int>(di)];
}

bool GameInput::IsFirstPressed(DigitalInput di)
{
    return s_Buttons[0][static_cast<int>(di)] && !s_Buttons[1][static_cast<int>(di)];
}

bool GameInput::IsRelease(DigitalInput di)
{
    return !s_Buttons[0][static_cast<int>(di)];
}

bool GameInput::IsFirstReleased(DigitalInput di)
{
    return !s_Buttons[0][static_cast<int>(di)] && s_Buttons[1][static_cast<int>(di)];
}

float GameInput::GetDurationPressed(DigitalInput di)
{
    return s_HoldDuration[static_cast<int>(di)];
}

float GameInput::GetAnalogInput(AnalogInput ai)
{
    return s_Analogs[static_cast<int>(ai)];
}

float GameInput::GetTimeCorrectedAnalogInput(AnalogInput ai)
{
    return s_AnalogsTC[static_cast<int>(ai)];
}
