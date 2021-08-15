#pragma once

#include "pch.h"

class CommandContext;
class RootSignature;
class VertexShader;
class GeometryShader;
class HullShader;
class DomainShader;
class PixelShader;
class ComputeShader;

class PSO
{
public:
	static void DestroyAll(void);

	void SetRootSignature(const RootSignature& BindMappings)
	{
		m_RootSignature = &BindMappings;
	}

	ID3D12PipelineState* GetPipelineStateObject(void) const { return m_PSO; }

protected:
	const RootSignature* m_RootSignature = nullptr;

	ID3D12PipelineState* m_PSO;
};

class GraphicsPSO : public PSO
{
	friend class CommandContext;\

public:
	void SetBlendState(const D3D12_BLEND_DESC& BlendDesc);
	void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc);
	void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);
	void SetSampleMask(UINT SampleMask);
	void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType);
	void SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MssaCount = 1, UINT MsaaQuality = 0);
	void SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
	void SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);

	void SetVertexShader(const void* Binary, size_t Size) { m_PSODesc.VS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
	void SetPixelShader(const void* Binary, size_t Size) { m_PSODesc.PS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
	void SetGeometryShader(const void* Binary, size_t Size) { m_PSODesc.GS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
	void SetHullShader(const void* Binary, size_t Size) { m_PSODesc.HS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
	void SetDomainShader(const void* Binary, size_t Size) { m_PSODesc.DS = CD3DX12_SHADER_BYTECODE(Binary, Size); }

	void Finalize();

private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_PSODesc = {
		.SampleMask = 0xFFFFFFFFu,
		.InputLayout = {.NumElements = 0},
		.SampleDesc = {1, 0},
		.NodeMask = 1
	};
	std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC> m_InputLayouts;
};

class ComputePSO : public PSO
{
	friend class CommandContext;

public:
	ComputePSO();

	void SetComputeShader(const void* Binary, size_t Size) { m_PSODesc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
	void SetComputeShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.CS = Binary; }

	void Finalize();

private:
	D3D12_COMPUTE_PIPELINE_STATE_DESC m_PSODesc;
};