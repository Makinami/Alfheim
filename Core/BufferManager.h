#pragma once

#include "ColorBuffer.h"
#include "DepthBuffer.h"

namespace Graphics
{
	extern DepthBuffer g_SceneDepthBuffer;
	extern ColorBuffer g_SceneColorBuffer;
	extern ColorBuffer g_OverlayBuffer;
	extern ColorBuffer g_HorizontalBuffer;

	void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void DestroyRenderingBuffers();
}