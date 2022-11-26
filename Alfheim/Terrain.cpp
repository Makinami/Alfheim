#include "pch.h"

#include "GraphicsCommon.h"
#include "SamplerManager.h"
#include "BufferManager.h"

#include "Terrain.h"
#include "CompiledShaders/TerrainVS.h"
#include "CompiledShaders/TerrainPS.h"

using namespace DirectX;

auto GeneratePlaneGrid(int x, int y) -> std::pair<std::vector<XMFLOAT2>, std::vector<int>>
{
	++x; ++y;

	auto vertices = std::vector<XMFLOAT2>(x * y);
	for (int j = 0; j < y; ++j)
		for (int i = 0; i < x; ++i)
			vertices[j * x + i] = XMFLOAT2{ static_cast<float>(i), static_cast<float>(j) };

	auto indices = std::vector<int>();
	indices.reserve((x - 1) * (y - 1) * 6);
	for (int j = 0; j < y - 1; ++j)
		for (int i = 0; i < x - 1; ++i)
		{
			indices.emplace_back(i + (j + 1) * x);
			indices.emplace_back((i + 1) + (j + 1) * x);
			indices.emplace_back((i + 1) + j * x);
			indices.emplace_back((i + 1) + j * x);
			indices.emplace_back(i + (j + 1) * x);
			indices.emplace_back(i + j * x);
		}

	return { vertices, indices };
}

void Terrain::Initialize()
{
	// When using RGB, actual RowPitch got in d3dx12.h:1978 pDevice->GetCopyableFootprints
	// is different from the one calculated by the abstraction. Could be worked around,
	// but for now easier to switch to RGBA which always works.
	// (probably because of the row memory alligment requirements)
	std::vector<XMFLOAT4> heightMapData(101 * 101);
	for (int x = 0; x < 101; ++x)
		for (int y = 0; y < 101; ++y)
			heightMapData[x * 101 + y] = XMFLOAT4{ 0, static_cast<float>(sin(x) + cos(y)), 0, 0 };
	m_HeightMap.Create(101, 101, DXGI_FORMAT_R32G32B32A32_FLOAT, heightMapData.data());

	D3D12_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_RootSig.Reset(2, 1);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSig[1].InitAsDescriptorTable(1);
	m_RootSig[1].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSig.InitStaticSampler(0, Graphics::SamplerAnisoWrapDesc);
	m_RootSig.Finalize(L"Terrain RootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_RenderPSO.SetRootSignature(m_RootSig);
	m_RenderPSO.SetInputLayout(_countof(InputLayout), InputLayout);
	m_RenderPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_RenderPSO.SetVertexShader(g_pTerrainVS, sizeof(g_pTerrainVS));
	m_RenderPSO.SetRasterizerState(Graphics::RasterizerWireframe);
	m_RenderPSO.SetDepthStencilState(Graphics::DepthStateReadWrite);
	m_RenderPSO.SetPixelShader(g_pTerrainPS, sizeof(g_pTerrainPS));
	m_RenderPSO.SetBlendState(Graphics::BlendDisable);
	m_RenderPSO.SetRenderTargetFormat(Graphics::g_SceneColorBuffer.GetFormat(), Graphics::g_SceneDepthBuffer.GetFormat());
	m_RenderPSO.Finalize();

	auto [vertices, indices] = GeneratePlaneGrid(101, 101);
	m_VertexBuffer.Create(L"Terrain vertex buffer", vertices.size(), sizeof(vertices[0]), vertices.data());
	m_IndexBuffer.Create(L"Terrain index buffer", indices.size(), sizeof(indices[0]), indices.data());
}

void Terrain::Render(GraphicsContext& gfxContext, const Math::Camera& camera)
{
	ScopedTimer _prof(L"Terrain", gfxContext);

	gfxContext.SetRootSignature(m_RootSig);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());
	gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());

	gfxContext.SetPipelineState(m_RenderPSO);
	gfxContext.SetViewportAndScissor(0, 0, Graphics::g_SceneColorBuffer.GetWidth(), Graphics::g_SceneColorBuffer.GetHeight());

	gfxContext.SetDynamicDescriptor(1, 0, m_HeightMap.GetSRV());

	__declspec(align(16)) struct {
		XMFLOAT4 inverseGridSize;
		XMFLOAT4 gridSize;
		XMFLOAT4X4 viewProjectionMatrix;
	} renderConstants;

	renderConstants.inverseGridSize = XMFLOAT4{ 1.f / 100, 0, 0, 0 };
	XMStoreFloat4x4(&renderConstants.viewProjectionMatrix, camera.GetViewProjMatrix());

	gfxContext.SetDynamicConstantBufferView(0, sizeof(renderConstants), &renderConstants);

	gfxContext.DrawIndexed(m_IndexBuffer.GetElementCount());
}
