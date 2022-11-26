#include "pch.h"
#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

namespace Graphics
{
	DepthBuffer g_SceneDepthBuffer;
	ColorBuffer g_SceneColorBuffer;
	ColorBuffer g_OverlayBuffer;
	ColorBuffer g_HorizontalBuffer;

	DXGI_FORMAT DefaultHdrColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
}

#define DSV_FORMAT DXGI_FORMAT_D32_FLOAT

void Graphics::InitializeRenderingBuffers(uint32_t bufferWidth, uint32_t bufferHeight)
{
	GraphicsContext& InitContext = GraphicsContext::Begin();

	/*const uint32_t bufferWidth1 = (bufferWidth + 1) / 2;
	const uint32_t bufferWidth2 = (bufferWidth + 3) / 4;
	const uint32_t bufferWidth3 = (bufferWidth + 7) / 8;
	const uint32_t bufferWidth4 = (bufferWidth + 15) / 16;
	const uint32_t bufferWidth5 = (bufferWidth + 31) / 32;
	const uint32_t bufferWidth6 = (bufferWidth + 63) / 64;
	const uint32_t bufferHeight1 = (bufferHeight + 1) / 2;
	const uint32_t bufferHeight2 = (bufferHeight + 3) / 4;
	const uint32_t bufferHeight3 = (bufferHeight + 7) / 8;
	const uint32_t bufferHeight4 = (bufferHeight + 15) / 16;
	const uint32_t bufferHeight5 = (bufferHeight + 31) / 32;
	const uint32_t bufferHeight6 = (bufferHeight + 63) / 64;*/

	g_SceneColorBuffer.Create(L"Main Color Buffer", bufferWidth, bufferHeight, 1, DefaultHdrColorFormat);
	g_OverlayBuffer.Create(L"UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	g_HorizontalBuffer.Create(L"Bicubic Intermediate", g_DisplayWidth, bufferHeight, 1, DefaultHdrColorFormat);

	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", bufferWidth, bufferHeight, DSV_FORMAT);

	InitContext.Finish();
}

void Graphics::ResizeDisplayDependentBuffers([[maybe_unused]] uint32_t NativeWidth, [[maybe_unused]] uint32_t NativeHeight)
{
	g_OverlayBuffer.Create(L"UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	g_HorizontalBuffer.Create(L"Bicubic Intermediate", g_DisplayWidth, NativeHeight, 1, DefaultHdrColorFormat);
}

void Graphics::DestroyRenderingBuffers()
{
	g_SceneColorBuffer.Destroy();
	g_OverlayBuffer.Destroy();
}
