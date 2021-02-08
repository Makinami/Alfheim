#pragma once

class SamplerDesc;
class CommandSignature;

namespace Graphics
{
	extern SamplerDesc SamplerLinearClampDesc;
	extern SamplerDesc SamplerPointClampDesc;

	extern D3D12_RASTERIZER_DESC RasterizerDefault;;
	extern D3D12_RASTERIZER_DESC RasterizerTwoSided;
	extern D3D12_RASTERIZER_DESC RasterizerWireframe;

	extern D3D12_BLEND_DESC BlendDisable;
	extern D3D12_BLEND_DESC BlendPreMultiplied;
	extern D3D12_BLEND_DESC BlendTraditional;

	extern D3D12_DEPTH_STENCIL_DESC DepthStateDisabled;
	extern D3D12_DEPTH_STENCIL_DESC DepthStateReadWrite;
	extern D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;
	extern D3D12_DEPTH_STENCIL_DESC DepthStateReadOnlyReversed;
	extern D3D12_DEPTH_STENCIL_DESC DepthStateTestEqual;

	void InitializeCommonState(void);
	void DestroyCommonState(void);
}