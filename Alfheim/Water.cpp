#include "pch.h"

#include <random>
#include <ranges>

#include "GraphicsCommon.h"
#include "BufferManager.h"
#include "SamplerManager.h"


#include "Water.h"
#include "CompiledShaders/OceanSpectrumCS.h"
#include "CompiledShaders/OceanInitFFTCS.h"
#include "CompiledShaders/OceanFFTCS.h"
#include "CompiledShaders/OceanVS.h"
#include "CompiledShaders/OceanPS.h"

using namespace DirectX;

auto GeneratePlaneGrid(int x, int y) -> std::pair<std::vector<XMFLOAT2>, std::vector<int>>
{
	++x; ++y;

	auto vertices = std::vector<XMFLOAT2>(x*y);
	for (int j = 0; j < y; ++j)
		for (int i = 0; i < x; ++i)
			vertices[j * x + i] = XMFLOAT2{ static_cast<float>(i), static_cast<float>(j) };

	auto indices = std::vector<int>();
	indices.reserve((x-1) * (y-1) * 6);
	for (int j = 0; j < y-1; ++j)
		for (int i = 0; i < x-1; ++i)
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

void Water::Initialize()
{
	m_Spectrum.CreateArray(L"Water spectrum", m_FFTSize, m_FFTSize, 4, DXGI_FORMAT_R32_FLOAT);
	m_Phase.CreateArray(L"Water phase", m_FFTSize, m_FFTSize, 4, DXGI_FORMAT_R32_FLOAT);
	m_Waves.CreateArray(L"Water waves", m_FFTSize, m_FFTSize, 6, DXGI_FORMAT_R32G32B32A32_FLOAT);
	//m_Slopes.CreateArray(L"Water waves' slopes", m_FFTSize, m_FFTSize, 2, DXGI_FORMAT_R32G32B32A32_FLOAT);

	std::default_random_engine rd;
	std::uniform_real_distribution dis(0.0f, DirectX::XM_2PI);
	
	std::vector<float> phase(4 * m_FFTSize * m_FFTSize);
	for (auto&& i : phase)
		i = dis(rd);

	const auto rowPitch = __int64{ m_FFTSize * sizeof(phase[0]) };
	const auto slicePitch = __int64{ m_FFTSize * rowPitch };
	D3D12_SUBRESOURCE_DATA texResources[] = {
		{&phase[0], rowPitch, slicePitch},
		{&phase[m_FFTSize*m_FFTSize], rowPitch, slicePitch},
		{&phase[2*m_FFTSize * m_FFTSize], rowPitch, slicePitch},
		{&phase[3*m_FFTSize * m_FFTSize], rowPitch, slicePitch}
	};
	GraphicsContext::InitializeTexture(m_Phase, 4, texResources);

	m_RootSig.Reset(3, 1);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSig[1].InitAsDescriptorTable(2);
	m_RootSig[1].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_RootSig[1].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
	m_RootSig[2].InitAsConstants(1, 1);
	m_RootSig.InitStaticSampler(0, Graphics::SamplerAnisoWrapDesc);
	m_RootSig.Finalize(L"Water RootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(m_RootSig); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(m_GenerateSpectrumPSO, g_pOceanSpectrumCS);
	CreatePSO(m_PrepareFFTPSO, g_pOceanInitFFTCS);
	CreatePSO(m_FFTPSO, g_pOceanFFTCS);

#undef CreatePSO

	D3D12_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		//{ "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		//{ "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		//{ "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		//{ "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	m_ForwardPSO.SetRootSignature(m_RootSig);
	m_ForwardPSO.SetBlendState(Graphics::BlendDisable);
	m_ForwardPSO.SetRasterizerState(Graphics::RasterizerWireframe);
	m_ForwardPSO.SetDepthStencilState(Graphics::DepthStateReadWrite);
	m_ForwardPSO.SetRenderTargetFormats(1, &Graphics::g_SceneColorBuffer.GetFormat(), Graphics::g_SceneDepthBuffer.GetFormat());
	m_ForwardPSO.SetInputLayout(_countof(InputLayout), InputLayout);
	m_ForwardPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_ForwardPSO.SetVertexShader(g_pOceanVS, sizeof(g_pOceanVS));
	m_ForwardPSO.SetPixelShader(g_pOceanPS, sizeof(g_pOceanPS));
	m_ForwardPSO.Finalize();


	auto [vertices, indices] = GeneratePlaneGrid(101, 101);
	m_VertexBuffer.Create(L"Water grid vertex buffer", vertices.size(), sizeof(vertices[0]), vertices.data());
	m_IndexBuffer.Create(L"Water grid index buffer", indices.size(), sizeof(indices[0]), indices.data());
}

void Water::Update(float deltaT)
{
	m_Time += deltaT;

	auto& cmpContext = ComputeContext::Begin(L"Water init", false);

	cmpContext.SetRootSignature(m_RootSig);

	__declspec(align(16)) struct {
		XMFLOAT4 inversetGridSize;
		XMFLOAT4 gridSize;
		XMFLOAT4 minK;
		XMINT2 gridResolution;
		XMFLOAT2 wind;
		float g;
		float density;
		float surfaceTension;
		float time;
		float labdaJ;
	} genSpectrumConstants;

	genSpectrumConstants.gridSize = { m_GridSize[0], m_GridSize[1], m_GridSize[2], m_GridSize[3] };
	genSpectrumConstants.inversetGridSize = { XM_2PI / m_GridSize[0], XM_2PI / m_GridSize[1], XM_2PI / m_GridSize[2], XM_2PI / m_GridSize[3] };
	genSpectrumConstants.minK = { XM_PI / m_GridSize[0], XM_PI * m_FFTSize / m_GridSize[0], XM_PI * m_FFTSize / m_GridSize[1], XM_PI * m_FFTSize / m_GridSize[2] };
	genSpectrumConstants.gridResolution = { m_FFTSize, m_FFTSize };
	genSpectrumConstants.wind = { 15, 15 };
	genSpectrumConstants.g = 9.81f;
	genSpectrumConstants.density = 997.f;
	genSpectrumConstants.surfaceTension = 0.0714f;
	genSpectrumConstants.time = m_Time;

	cmpContext.SetDynamicConstantBufferView(0, sizeof(genSpectrumConstants), &genSpectrumConstants);

	ResetParameterDependentResources(cmpContext);


	cmpContext.SetPipelineState(m_PrepareFFTPSO);

	cmpContext.TransitionResource(m_Spectrum, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cmpContext.SetDynamicDescriptor(1, 1, m_Spectrum.GetSRV());
	cmpContext.SetDynamicDescriptor(1, 2, m_Phase.GetSRV());
	cmpContext.TransitionResource(m_Waves, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmpContext.SetDynamicDescriptor(1, 0, m_Waves.GetUAV());
		
	cmpContext.Dispatch2D(m_FFTSize, m_FFTSize);

	cmpContext.SetPipelineState(m_FFTPSO);
	
	// ROW
	cmpContext.SetConstant(2, 0);
	cmpContext.TransitionResource(m_Waves, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmpContext.Dispatch3D(m_FFTSize, m_FFTSize, 6, m_FFTSize, 1, 1);
	// COLUMN
	cmpContext.SetConstant(2, 1);
	cmpContext.TransitionResource(m_Waves, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmpContext.Dispatch3D(m_FFTSize, m_FFTSize, 6, m_FFTSize, 1, 1);

	cmpContext.Finish();
}

void Water::Render(GraphicsContext& gfxContext, const Math::Camera& camera)
{
	ScopedTimer _prof(L"Water", gfxContext);

	gfxContext.SetRootSignature(m_RootSig);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());
	gfxContext.TransitionResource(m_VertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());

	gfxContext.SetPipelineState(m_ForwardPSO);
	gfxContext.SetViewportAndScissor(0, 0, Graphics::g_SceneColorBuffer.GetWidth(), Graphics::g_SceneColorBuffer.GetHeight());

	gfxContext.TransitionResource(m_Waves, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	gfxContext.SetDynamicDescriptor(1, 1, m_Waves.GetSRV());

	__declspec(align(16)) struct {
		XMFLOAT4 inverseGridSize;
		XMFLOAT4 gridSize;
		XMFLOAT4X4 viewProjMatrix;
	} renderConstants;

	renderConstants.inverseGridSize = XMFLOAT4{ 1.f / m_GridSize[0], 1.f / m_GridSize[1], 1.f / m_GridSize[2], 1.f / m_GridSize[3] };
	renderConstants.gridSize = XMFLOAT4{ m_GridSize[0], m_GridSize[1], m_GridSize[2], m_GridSize[3] };
	XMStoreFloat4x4(&renderConstants.viewProjMatrix, camera.GetViewProjMatrix());

	gfxContext.SetDynamicConstantBufferView(0, sizeof(renderConstants), &renderConstants);

	gfxContext.DrawIndexed(m_IndexBuffer.GetElementCount());
}

void Water::ResetParameterDependentResources(ComputeContext& cmpContext)
{
	cmpContext.SetPipelineState(m_GenerateSpectrumPSO);

	cmpContext.SetDynamicDescriptor(1, 0, m_Spectrum.GetUAV());

	cmpContext.Dispatch3D(m_FFTSize, m_FFTSize, 4, 8, 8, 1);
}
