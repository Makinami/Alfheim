#pragma once

#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable:4238) // nonstandard extension used : class rvalue used as lvalue
#pragma warning(disable:4324) // structure was padded due to __declspec(align())

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#undef pascal

#include <d3d12.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define MY_IID_PPV_ARGS IID_PPV_ARGS
#define D3D12_GPU_VIRTUAL_ADDRESS_NULL     ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN  ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

#include "d3dx12.h"

#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <memory>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <wrl.h>
#include <ppltasks.h>

#include "Utility.h"
#include "VectorMath.h"
#include "EngineTuning.h"
#include "EngineProfiling.h"