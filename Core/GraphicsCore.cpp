#include "pch.h"
#include "GraphicsCore.h"
#include "GameCore.h"
#include "BufferManager.h"
#include "GpuTimeManager.h"
#include "TextRenderer.h"
#include "ColorBuffer.h"
#include "SystemTime.h"
#include "SamplerManager.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "RootSignature.h"
#include "GraphRenderer.h"

#include <dxgi1_6.h>

#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/BufferCopyPS.h"
#include "CompiledShaders/PresentSDRPS.h"
#include "CompiledShaders/BilinearUpsamplePS.h"
#include "CompiledShaders/BicubicHorizontalUpsamplePS.h"
#include "CompiledShaders/BicubicVerticalUpsamplePS.h"
#include "CompiledShaders/SharpeningUpsamplePS.h"

#define SWAP_CHAIN_BUFFER_COUNT 3

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif

using namespace Math;

namespace GameCore
{
	extern HWND g_hWnd;
}

namespace
{
	float s_FrameTime = 0.0f;
	uint64_t s_FrameIndex = 0;
	int64_t s_FrameStartTick = 0;

	BoolVar s_LimitTo30Hz("Timing/Limit To 30Hz", false);
	BoolVar s_DropRandomFrames("Timing/Drop Random Frames", false);
}

namespace Graphics
{
	void PreparePresentLDR();
	void CompositeOverlays(GraphicsContext& Context);

	const uint32_t kMaxNativeWidth = 3840;
	const uint32_t kMaxNativeHeight = 2160;
	const uint32_t kNumPredefinedResolutions = 6;

	const char* ResolutionLabels[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3200x1800", "3840x2160" };
	EnumVar TargetResolution("Graphics/Display/Native Resolution", k1080p, kNumPredefinedResolutions, ResolutionLabels);

	BoolVar s_EnableVSync("Timing/VSync", true);

	bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT = false;
	bool g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = false;
	bool g_bEnableHDROutput = false;

	uint32_t g_NativeWidth = 0;
	uint32_t g_NativeHeight = 0;
	uint32_t g_DisplayWidth = 1920;
	uint32_t g_DisplayHeight = 1080;
	ColorBuffer g_PreDisplayBuffer;

	void SetNativeResolution(void)
	{
		uint32_t NativeWidth, NativeHeight;
		switch (eResolution((int)TargetResolution))
		{
			default:
			case k720p:
				NativeWidth = 1280;
				NativeHeight = 720;
				break;
			case k900p:
				NativeWidth = 1600;
				NativeHeight = 900;
				break;
			case k1080p:
				NativeWidth = 1920;
				NativeHeight = 1080;
				break;
			case k1440p:
				NativeWidth = 2560;
				NativeHeight = 1440;
				break;
			case k1800p:
				NativeWidth = 3200;
				NativeHeight = 1800;
				break;
			case k2160p:
				NativeWidth = 2160;
				NativeHeight = 2160;
				break;
		}

		if (g_NativeWidth == NativeWidth && g_NativeHeight == NativeHeight)
			return;

		DEBUGPRINT("Changing native resolution to {}x{}", NativeWidth, NativeHeight);

		g_NativeWidth = NativeWidth;
		g_NativeHeight = NativeHeight;

		g_CommandManager.IdleGPU();

		InitializeRenderingBuffers(NativeWidth, NativeHeight);
	}

	ID3D12Device* g_Device;

	CommandListManager g_CommandManager;
	ContextManager g_ContextManager;

	ComPtr<IDXGISwapChain1> s_SwapChain1 = nullptr;

	DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
	};

	RootSignature s_PresentRS;
	GraphicsPSO s_BlendUIPSO;
	GraphicsPSO PresentSDRPS;
	GraphicsPSO SharpeningUpsamplePS;
	GraphicsPSO BicubicHorizontalUpsamplePS;
	GraphicsPSO BicubicVerticalUpsamplePS;
	GraphicsPSO BilinearUpsamplePS;

	ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
	UINT g_CurrentBuffer = 0;

	enum { kBilinear, kBicubic, kSharpening, kFilterCount };
	const char* FilterLabels[] = { "Bilinear", "Bicubic", "Sharpening" };
	EnumVar UpsampleFilter("Graphics/Display/Upsample Filter", kFilterCount - 1, kFilterCount, FilterLabels);
	NumVar BicubicUpsampleWeight("Graphics/Display/Bicubic Filter Weight", -0.75f, -1.0f, -0.25f, 0.25f);
	NumVar SharpeningSpread("Graphics/Display/Sharpness Sample Spread", 1.0f, 0.7f, 2.0f, 0.1f);
	NumVar SharpeningRotation("Graphics/Display/Sharpness Sample Rotation", 45.0f, 0.0f, 90.0f, 15.0f);
	NumVar SharpeningStrength("Graphics/Display/Sharpness Strength", 0.10f, 0.0f, 1.0f, 0.01f);

	enum DebugZoomLevel { kDebugZoomOff, kDebugZoom2x, kDebugZoom4x, kDebugZoom8x, kDebugZoom16x, kDebugZoomCount };
	const char* DebugZoomLabels[] = { "Off", "2x Zoom", "4x Zoom", "8x Zoom", "16x Zoom" };
	EnumVar DebugZoom("Graphics/Display/Magnify Pixels", kDebugZoomOff, kDebugZoomCount, DebugZoomLabels);
}

void Graphics::Initialize(void)
{
	ASSERT(s_SwapChain1 == nullptr, "Graphics has already been initialized");

	ComPtr<ID3D12Device> pDevice;

#if _DEBUG
	ComPtr<ID3D12Debug> debugInterface;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
		debugInterface->EnableDebugLayer();
	else
		Utility::Print("WARNING: Unable to enable D3D12 debug validation layer\n");
#endif // _DEBUG

	//! EnableExperimentalShaderModels();

	ComPtr<IDXGIFactory4> dxgiFactory;
	ASSERT_SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> pAdapter;

	static const bool bUseWarpDriver = false;

	if (!bUseWarpDriver)
	{
		SIZE_T MaxSize = 0;

		for (auto Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
		{
			DXGI_ADAPTER_DESC1 desc;
			pAdapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			if (desc.DedicatedVideoMemory > MaxSize && SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice))))
			{
				pAdapter->GetDesc1(&desc);
				Utility::Printf(L"D3D12-capable hardware found:  {} ({} MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
				MaxSize = desc.DedicatedVideoMemory;
			}
		}

		if (MaxSize > 0)
			g_Device = pDevice.Detach();
	}

	if (g_Device == nullptr)
	{
		if (bUseWarpDriver)
			Utility::Print("WARP software adapter requested. Initializing...\n");
		else
			Utility::Print("Failed to find a hardware adapter. Falling back to WARP.\n");
		ASSERT_SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter)));
		ASSERT_SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));
		g_Device = pDevice.Detach();
	}
#ifndef RELEASE
	else
	{
		bool DeveloperModeEnabled = false;

		HKEY hKey;
		LSTATUS result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hKey);
		if (result == ERROR_SUCCESS)
		{
			DWORD keyValue, keySize = sizeof(DWORD);
			result = RegQueryValueEx(hKey, L"AllowDevelopmentWithoutDevLicense", 0, NULL, (byte*)&keyValue, &keySize);
			if (result == ERROR_SUCCESS && keyValue == 1)
				DeveloperModeEnabled = true;
			RegCloseKey(hKey);
		}

		WARN_ONCE_IF_NOT(DeveloperModeEnabled, "Enabled Developer Mode on Windows 10 to get consistent profiling results");

		/*if (DeveloperModeEnabled)
			g_Device->SetStablePowerState(TRUE);*/
	}
#endif // !RELEASE

#if _DEBUG
	ID3D12InfoQueue* pInfoQueue = nullptr;
	if (SUCCEEDED(g_Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
	{
		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] =
		{
			// This occurs when there are uninitialized descriptors in a descriptor table, even when a
			// shader does not access the missing descriptors.  I find this is common when switching
			// shader permutations and not wanting to change much code to reorder resources.
			D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,

			// Triggered when a shader does not export all color components of a render target, such as
			// when only writing RGB to an R10G10B10A2 buffer, ignoring alpha.
			D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,

			// This occurs when a descriptor table is unbound even when a shader does not access the missing
			// descriptors.  This is common with a root signature shared between disparate shaders that
			// don't all need the same types of resources.
			D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,

			// RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS
			(D3D12_MESSAGE_ID)1008,
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		pInfoQueue->PushStorageFilter(&NewFilter);
		pInfoQueue->Release();
	}
#endif

	// We like to do read-modify-write operations on UAVs during post processing.  To support that, we
	// need to either have the hardware do typed UAV loads of R11G11B10_FLOAT or we need to manually
	// decode an R32_UINT representation of the same buffer.  This code determines if we get the hardware
	// load support.
	D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData = {};
	if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &FeatureData, sizeof(FeatureData))))
	{
		if (FeatureData.TypedUAVLoadAdditionalFormats)
		{
			D3D12_FEATURE_DATA_FORMAT_SUPPORT Support =
			{
				DXGI_FORMAT_R11G11B10_FLOAT, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE
			};

			if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
				(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
			{
				g_bTypedUAVLoadSupport_R11G11B10_FLOAT = true;
			}

			Support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

			if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
				(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
			{
				g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = true;
			}
		}
	}

	g_CommandManager.Create(g_Device);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = g_DisplayWidth;
	swapChainDesc.Height = g_DisplayHeight;
	swapChainDesc.Format = SwapChainFormat;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(g_CommandManager.GetCommandQueue(), GameCore::g_hWnd, &swapChainDesc, nullptr, nullptr, &s_SwapChain1));

	for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> DisplayPlane;
		ASSERT_SUCCEEDED(s_SwapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayPlane)));
		g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
	}

	InitializeCommonState();

	s_PresentRS.Reset(4, 2);
	s_PresentRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
	s_PresentRS[1].InitAsConstants(0, 6, D3D12_SHADER_VISIBILITY_ALL);
	s_PresentRS[2].InitAsBufferSRV(2, D3D12_SHADER_VISIBILITY_PIXEL);
	s_PresentRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	s_PresentRS.InitStaticSampler(0, SamplerLinearClampDesc);
	s_PresentRS.InitStaticSampler(1, SamplerPointClampDesc);
	s_PresentRS.Finalize(L"Present");

	// Initialize PSOs
	s_BlendUIPSO.SetRootSignature(s_PresentRS);
	s_BlendUIPSO.SetRasterizerState(RasterizerTwoSided);
	s_BlendUIPSO.SetBlendState(BlendPreMultiplied);
	s_BlendUIPSO.SetDepthStencilState(DepthStateDisabled);
	s_BlendUIPSO.SetSampleMask(0xFFFFFFFF);
	s_BlendUIPSO.SetInputLayout(0, nullptr);
	s_BlendUIPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	s_BlendUIPSO.SetVertexShader(g_pScreenQuadVS, sizeof(g_pScreenQuadVS));
	s_BlendUIPSO.SetPixelShader(g_pBufferCopyPS, sizeof(g_pBufferCopyPS));
	s_BlendUIPSO.SetRenderTargetFormat(SwapChainFormat, DXGI_FORMAT_UNKNOWN);
	s_BlendUIPSO.Finalize();

#define CreatePSO(ObjName, ShaderByteCode) \
	ObjName = s_BlendUIPSO; \
	ObjName.SetBlendState(BlendDisable); \
	ObjName.SetPixelShader(ShaderByteCode, sizeof(ShaderByteCode)); \
	ObjName.Finalize();

	CreatePSO(PresentSDRPS, g_pPresentSDRPS);
	CreatePSO(BilinearUpsamplePS, g_pBilinearUpsamplePS);
	CreatePSO(BicubicVerticalUpsamplePS, g_pBicubicVerticalUpsamplePS);
	CreatePSO(SharpeningUpsamplePS, g_pSharpeningUpsamplePS);

#undef CreatePSO

	BicubicHorizontalUpsamplePS = s_BlendUIPSO;
	BicubicHorizontalUpsamplePS.SetBlendState(BlendDisable);
	BicubicHorizontalUpsamplePS.SetPixelShader(g_pBicubicHorizontalUpsamplePS, sizeof(g_pBicubicHorizontalUpsamplePS));
	BicubicHorizontalUpsamplePS.SetRenderTargetFormat(DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_UNKNOWN);
	BicubicHorizontalUpsamplePS.Finalize();
	

	//ASSERT(false, "Present root sig and others not initialized");

	//!

	g_PreDisplayBuffer.Create(L"PreDisplay Buffer", g_DisplayWidth, g_DisplayHeight, 1, SwapChainFormat);

	GpuTimeManager::Initialize(4096);
	SetNativeResolution();
	//!
	TextRenderer::Initialize();
	GraphRenderer::Initialize();
	//!
}

void Graphics::Resize(uint32_t width, uint32_t height)
{
	ASSERT(s_SwapChain1 != nullptr);

	// Check for invalid windows dimensions
	if (width == 0 || height == 0)
		return;

	// CHeck for an unneeded resize
	if (width == g_DisplayWidth && height == g_DisplayHeight)
		return;

	g_CommandManager.IdleGPU();

	g_DisplayWidth = width;
	g_DisplayHeight = height;

	DEBUGPRINT("Changing display resolution to {}x{}", width, height);

	g_PreDisplayBuffer.Create(L"PreDisplay Buffer", width, height, 1, SwapChainFormat);

	for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		g_DisplayPlane[i].Destroy();

	ASSERT_SUCCEEDED(s_SwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, SwapChainFormat, 0));

	for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> DisplayPlane;
		ASSERT_SUCCEEDED(s_SwapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayPlane)));
		g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
	}

	g_CurrentBuffer = 0;
	g_CommandManager.IdleGPU();

	ResizeDisplayDependentBuffers(g_NativeWidth, g_NativeHeight);
}

void Graphics::Terminate(void)
{
	g_CommandManager.IdleGPU();
	s_SwapChain1->SetFullscreenState(FALSE, nullptr);
}

void Graphics::Shutdown(void)
{
	CommandContext::DestroyAllContexts();
	g_CommandManager.Shutdown();
	GpuTimeManager::Shutdown();
	s_SwapChain1.Reset();
	PSO::DestroyAll();
	RootSignature::DestroyAll();
	DescriptorAllocator::DestroyAll();

	DestroyCommonState();
	DestroyRenderingBuffers();
	//!
	TextRenderer::Shutdown();
	GraphRenderer::Shutdown();
	//!

	for (auto& plane : g_DisplayPlane)
		plane.Destroy();

	g_PreDisplayBuffer.Destroy();

#ifdef _DEBUG
	auto debugInterface = ComPtr<ID3D12DebugDevice>{};
	if (SUCCEEDED(g_Device->QueryInterface(debugInterface.GetAddressOf())))
		debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif // _DEBUG

	SAFE_RELEASE(g_Device);
}

void Graphics::PreparePresentLDR(void)
{
	GraphicsContext& Context = GraphicsContext::Begin(L"Present");

	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	
	Context.SetRootSignature(s_PresentRS);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Context.SetDynamicDescriptor(0, 0, g_SceneColorBuffer.GetSRV());

	ColorBuffer& UpsampleDest = (DebugZoom == kDebugZoomOff ? g_DisplayPlane[g_CurrentBuffer] : g_PreDisplayBuffer);

	if (g_NativeWidth == g_DisplayWidth && g_NativeHeight == g_DisplayHeight)
	{
		Context.SetPipelineState(PresentSDRPS);
		Context.TransitionResource(UpsampleDest, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(UpsampleDest.GetRTV());
		Context.SetViewportAndScissor(0, 0, g_NativeWidth, g_NativeHeight);
		Context.Draw(3);
	}
	else if (UpsampleFilter == kBicubic)
	{
		Context.TransitionResource(g_HorizontalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(g_HorizontalBuffer.GetRTV());
		Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_NativeHeight);
		Context.SetPipelineState(BicubicHorizontalUpsamplePS);
		Context.SetConstants(1, g_NativeWidth, g_NativeHeight, (float)BicubicUpsampleWeight);
		Context.Draw(3);

		Context.TransitionResource(g_HorizontalBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(UpsampleDest, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(UpsampleDest.GetRTV());
		Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
		Context.SetPipelineState(BicubicVerticalUpsamplePS);
		Context.SetConstants(1, g_DisplayWidth, g_NativeHeight, (float)BicubicUpsampleWeight);
		Context.SetDynamicDescriptor(0, 0, g_HorizontalBuffer.GetSRV());
		Context.Draw(3);
	}
	else if (UpsampleFilter == kSharpening)
	{
		Context.SetPipelineState(SharpeningUpsamplePS);
		Context.TransitionResource(UpsampleDest, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(UpsampleDest.GetRTV());
		Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
		float TexelWidth = 1.0f / g_NativeWidth;
		float TexelHeight = 1.0f / g_NativeHeight;
		float X = Math::Cos((float)SharpeningRotation / 180.0f * 3.14159f) * (float)SharpeningSpread;
		float Y = Math::Sin((float)SharpeningRotation / 180.0f * 3.14159f) * (float)SharpeningSpread;
		const float WA = SharpeningStrength;
		const float WB = 1.0f + 4.0f * WA;
		float Constants[] = { X * TexelWidth, Y * TexelHeight,Y * TexelWidth, -X * TexelHeight, WA, WB };
		Context.SetConstantArray(1, _countof(Constants), Constants);
		Context.Draw(3);
	}
	else if (UpsampleFilter == kBilinear)
	{
		Context.SetPipelineState(BilinearUpsamplePS);
		Context.TransitionResource(UpsampleDest, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(UpsampleDest.GetRTV());
		Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
		Context.Draw(3);
	}

	CompositeOverlays(Context);

	Context.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	// Close the final context to be executed before frame present.
	Context.Finish();
}

void Graphics::CompositeOverlays(GraphicsContext& Context)
{
	// Bland (or write) to UI overlay
	Context.TransitionResource(g_OverlayBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.SetDynamicDescriptor(0, 0, g_OverlayBuffer.GetSRV());
	Context.SetPipelineState(s_BlendUIPSO);
	Context.SetConstants(1, 1.0f / g_NativeWidth, 1.0f / g_NativeHeight);
	Context.Draw(3);
}

void Graphics::Present(void)
{
	//! if (g_bEnableHDROutput)
		//! PreparePresentHDR();
	//! else
	PreparePresentLDR();

	g_CurrentBuffer = (g_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

	UINT PresentInterval = s_EnableVSync ? std::min(4, (int)Round(s_FrameTime * 60.0f)) : 0;

	s_SwapChain1->Present(PresentInterval, 0);

	int64_t CurrentTick = SystemTime::GetCurrentTick();

	if (s_EnableVSync)
	{
		// With VSync enabled, the time step between frames becomes a multiple of 16.666 ms.  We need
		// to add logic to vary between 1 and 2 (or 3 fields).  This delta time also determines how
		// long the previous frame should be displayed (i.e. the present interval.)
		s_FrameTime = (s_LimitTo30Hz ? 2.0f : 1.0f) / 60.0f;
		if (s_DropRandomFrames)
		{
			if (std::rand() % 50 == 0)
				s_FrameTime += (1.0f / 60.0f);
		}
	}
	else
	{
		s_FrameTime = (float)SystemTime::TimeBetweenTicks(s_FrameStartTick, CurrentTick);
	}

	s_FrameStartTick = CurrentTick;

	++s_FrameIndex;
	//! TemporalEffects::Update((uint32_t)s_FrameIndex);

	SetNativeResolution();
}

uint64_t Graphics::GetFrameCount(void)
{
	return s_FrameIndex;
}

float Graphics::GetFrameTime(void)
{
	return s_FrameTime;
}