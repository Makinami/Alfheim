#include "pch.h"
#include "GraphicsCore.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "Hash.h"

#include <map>

using Math::IsAligned;
using namespace Graphics;
using Microsoft::WRL::ComPtr;
using namespace std;

static map<size_t, ComPtr<ID3D12PipelineState>> s_GraphicsPSOHashMap;
static map<size_t, ComPtr<ID3D12PipelineState>> s_ComputePSOHashMap;

void PSO::DestroyAll(void)
{
	s_GraphicsPSOHashMap.clear();
	s_ComputePSOHashMap.clear();
}

void GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
{
	m_PSODesc.BlendState = BlendDesc;
}

void GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
{
	m_PSODesc.RasterizerState = RasterizerDesc;
}

void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
{
	m_PSODesc.DepthStencilState = DepthStencilDesc;
}

void GraphicsPSO::SetSampleMask(UINT SampleMask)
{
	m_PSODesc.SampleMask = SampleMask;
}

void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
{
	ASSERT(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefinded topology");
	m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MssaCount, UINT MsaaQuality)
{
	SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MssaCount, MsaaQuality);
}

void GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	ASSERT(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
	for (UINT i = 0; i < NumRTVs; ++i)
		m_PSODesc.RTVFormats[i] = RTVFormats[i];
	for (UINT i = NumRTVs; i < m_PSODesc.NumRenderTargets; ++i)
		m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	m_PSODesc.NumRenderTargets = NumRTVs;
	m_PSODesc.DSVFormat = DSVFormat;
	m_PSODesc.SampleDesc.Count = MsaaCount;
	m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPSO::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
{
	m_PSODesc.InputLayout.NumElements = NumElements;

	if (NumElements > 0)
	{
		D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
		memcpy(NewElements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
		m_InputLayouts.reset((const D3D12_INPUT_ELEMENT_DESC*)NewElements);
	}
	else
		m_InputLayouts = nullptr;
}

void GraphicsPSO::Finalize()
{
	m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
	ASSERT(m_PSODesc.pRootSignature != nullptr);

	m_PSODesc.InputLayout.pInputElementDescs = nullptr;
	size_t HashCode = Utility::HashState(&m_PSODesc);
	HashCode = Utility::HashState(m_InputLayouts.get(), m_PSODesc.InputLayout.NumElements, HashCode);
	m_PSODesc.InputLayout.pInputElementDescs = m_InputLayouts.get();

	ID3D12PipelineState** PSORef = nullptr;
	bool firstCompile = false;
	{
		static mutex s_HashMapMutex;
		lock_guard<mutex> CS(s_HashMapMutex);
		auto iter = s_GraphicsPSOHashMap.find(HashCode);

		if (iter == s_GraphicsPSOHashMap.end())
		{
			firstCompile = true;
			PSORef = s_GraphicsPSOHashMap[HashCode].GetAddressOf();
		}
		else
			PSORef = iter->second.GetAddressOf();
	}

	if (firstCompile)
	{
		ASSERT_SUCCEEDED(g_Device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
		s_GraphicsPSOHashMap[HashCode].Attach(m_PSO);
	}
	else
	{
		while (*PSORef == nullptr)
			this_thread::yield();
		m_PSO = *PSORef;
	}
}
