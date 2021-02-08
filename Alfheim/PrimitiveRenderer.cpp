#include "pch.h"
#include "PrimitiveRenderer.h"

#include "GraphicsCommon.h"
#include "BufferManager.h"
#include "CommandContext.h"

#include "CompiledShaders/PrimitiveVS.h"
#include "CompiledShaders/PrimitivePS.h"

#include "Camera.h"

#include "MeshBufferPacker.h"

using namespace DirectX;

namespace {
	constexpr int max_instances = 1024;
}

struct VertexData
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
};

namespace DirectX
{
	auto operator ==(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) noexcept
	{
		return a.x == b.x && a.y == b.y && a.z == b.z;
		//return a.x != b.x ? a.x <=> b.x : (a.y != b.y ? a.y <=> b.y : a.z <=> b.z); // three-way comparison not detected by std::find
	}
}

template <typename Container, typename Value>
auto append_unique(Container& container, const Value& value) -> typename Container::iterator
{
	if (auto it = std::find(container.begin(), container.end(), value); it == container.end())
		return container.insert(it, value);
	else
		return it;
}

auto GenerateSphereMesh(int lod = 0) -> std::pair<std::vector<VertexData>, std::vector<int>>
{
	using namespace Math;

	const float X = 0.525731f;
	const float Z = 0.850651f;

	auto vertices = std::vector{
		XMFLOAT3(-X, 0.0f, Z),  XMFLOAT3(X, 0.0f, Z),
		XMFLOAT3(-X, 0.0f, -Z), XMFLOAT3(X, 0.0f, -Z),
		XMFLOAT3(0.0f, Z, X),   XMFLOAT3(0.0f, Z, -X),
		XMFLOAT3(0.0f, -Z, X),  XMFLOAT3(0.0f, -Z, -X),
		XMFLOAT3(Z, X, 0.0f),   XMFLOAT3(-Z, X, 0.0f),
		XMFLOAT3(Z, -X, 0.0f),  XMFLOAT3(-Z, -X, 0.0f)
	};

	auto indices = std::vector{
		1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
		1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
		3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
		10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
	};

	auto normalize = [](const XMFLOAT3& V) {
		auto length = sqrt(V.x * V.x + V.y * V.y + V.z * V.z);
		return XMFLOAT3{ V.x / length, V.y / length, V.z / length };
	};

	auto midpoint = [&](const XMFLOAT3& A, const XMFLOAT3& B) {
		auto mid = XMFLOAT3{ A.x + B.x, A.y + B.y, A.z + B.z };
		return normalize(mid);
	};

	for (int i = 0; i < lod; ++i)
	{
		const auto triangleCount = indices.size() / 3;
		indices.reserve(indices.size() * 4);

		for (int j = 0; j < triangleCount; ++j)
		{
			auto iA = indices[3 * j + 0];
			auto iB = indices[3 * j + 1];
			auto iC = indices[3 * j + 2];

			auto iAB = static_cast<int>(std::distance(
				vertices.begin(),
				append_unique(vertices, midpoint(vertices[iA], vertices[iB]))
			));
			auto iBC = static_cast<int>(std::distance(
				vertices.begin(),
				append_unique(vertices, midpoint(vertices[iB], vertices[iC]))
			));
			auto iAC = static_cast<int>(std::distance(
				vertices.begin(),
				append_unique(vertices, midpoint(vertices[iA], vertices[iC]))
			));

			indices[3 * j + 0] = iA;
			indices[3 * j + 1] = iAB;
			indices[3 * j + 2] = iAC;

			indices.push_back(iAB);
			indices.push_back(iB);
			indices.push_back(iBC);

			indices.push_back(iAC);
			indices.push_back(iBC);
			indices.push_back(iC);

			indices.push_back(iAB);
			indices.push_back(iBC);
			indices.push_back(iAC);
		}
	}

	auto vertexData = std::vector<VertexData>{};
	vertexData.reserve(vertices.size());

	std::transform(vertices.begin(), vertices.end(), std::back_inserter(vertexData), [&](const XMFLOAT3& vertex) {
		return VertexData{ vertex, normalize(vertex) };
	});

	return { vertexData, indices };
}

[[nodiscard]] auto GenerateCubeMesh() -> std::pair<std::vector<VertexData>, std::vector<int>>
{
	auto vertices = std::vector{
		VertexData{ XMFLOAT3(0, 0, 0), XMFLOAT3(-1, 0, 0) },
		VertexData{ XMFLOAT3(0, 1, 0), XMFLOAT3(-1, 0, 0) },
		VertexData{ XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0) },
		VertexData{ XMFLOAT3(0, 1, 1), XMFLOAT3(-1, 0, 0) },

		VertexData{ XMFLOAT3(1, 0, 0), XMFLOAT3(1, 0, 0) },
		VertexData{ XMFLOAT3(1, 1, 0), XMFLOAT3(1, 0, 0) },
		VertexData{ XMFLOAT3(1, 0, 1), XMFLOAT3(1, 0, 0) },
		VertexData{ XMFLOAT3(1, 1, 1), XMFLOAT3(1, 0, 0) },

		VertexData{ XMFLOAT3(0, 0, 0), XMFLOAT3(0, -1, 0) },
		VertexData{ XMFLOAT3(1, 0, 0), XMFLOAT3(0, -1, 0) },
		VertexData{ XMFLOAT3(0, 0, 1), XMFLOAT3(0, -1, 0) },
		VertexData{ XMFLOAT3(1, 0, 1), XMFLOAT3(0, -1, 0) },

		VertexData{ XMFLOAT3(0, 1, 0), XMFLOAT3(0, 1, 0) },
		VertexData{ XMFLOAT3(1, 1, 0), XMFLOAT3(0, 1, 0) },
		VertexData{ XMFLOAT3(0, 1, 1), XMFLOAT3(0, 1, 0) },
		VertexData{ XMFLOAT3(1, 1, 1), XMFLOAT3(0, 1, 0) },

		VertexData{ XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, -1) },
		VertexData{ XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1) },
		VertexData{ XMFLOAT3(0, 1, 0), XMFLOAT3(0, 0, -1) },
		VertexData{ XMFLOAT3(1, 1, 0), XMFLOAT3(0, 0, -1) },

		VertexData{ XMFLOAT3(0, 0, 1), XMFLOAT3(0, 0, 1) },
		VertexData{ XMFLOAT3(1, 0, 1), XMFLOAT3(0, 0, 1) },
		VertexData{ XMFLOAT3(0, 1, 1), XMFLOAT3(0, 0, 1) },
		VertexData{ XMFLOAT3(1, 1, 1), XMFLOAT3(0, 0, 1) }
	};

	auto indices = std::vector{
		0, 1, 2,  1, 2, 3,
		4, 5, 6,  5, 6, 7,
		8, 9, 10,  9, 10, 11,
		12, 13, 14,  13, 14, 15,
		16, 17, 18,  17, 18, 19,
		20, 21, 22,  21, 22, 23
	};

	return { vertices, indices };
}

auto GeneratePlaneMesh() -> std::pair<std::vector<VertexData>, std::vector<int>>
{
	return {
		{
			VertexData{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
			VertexData{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
			VertexData{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
			VertexData{{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
		},
		{ 0, 2, 1,  1, 2, 3 }
	};
}

void PrimitiveRenderer::Initialize()
{
	m_RootSig.Reset(3, 0);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSig[1].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig[2].InitAsBufferSRV(1, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig.Finalize(L"PrimitivesRootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	D3D12_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	m_SurfacePSO.SetRootSignature(m_RootSig);
	m_SurfacePSO.SetBlendState(Graphics::BlendDisable);
	m_SurfacePSO.SetRasterizerState(Graphics::RasterizerDefault);
	m_SurfacePSO.SetDepthStencilState(Graphics::DepthStateReadWrite);
	m_SurfacePSO.SetRenderTargetFormats(1, &Graphics::g_SceneColorBuffer.GetFormat(), Graphics::g_SceneDepthBuffer.GetFormat());
	m_SurfacePSO.SetInputLayout(_countof(InputLayout), InputLayout);
	m_SurfacePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_SurfacePSO.SetVertexShader(g_pPrimitiveVS, sizeof(g_pPrimitiveVS));
	m_SurfacePSO.SetPixelShader(g_pPrimitivePS, sizeof(g_pPrimitivePS));
	m_SurfacePSO.Finalize();

	m_WireframePSO = m_SurfacePSO;
	m_WireframePSO.SetRasterizerState(Graphics::RasterizerWireframe);
	m_WireframePSO.Finalize();

	MeshBufferPacker<VertexData> packer;
	m_Foos[0] = packer.Push(GenerateSphereMesh(1));
	m_Foos[1] = packer.Push(GenerateCubeMesh());
	m_Foos[2] = packer.Push(GeneratePlaneMesh());
	packer.Finalize(L"Primitives", m_VertexBuffer, m_IndexBuffer);

	m_InstanceBuffer.Create(L"Sphere instance buffer", max_instances);

	QueueSphere({ 0, 2, 0 }, 2.f);
	QueuePlane({ -2, 0, -2 }, { 4, 0, 0 }, { 0, 0, 4 });

	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;
	m_Viewport.Width = static_cast<float>(Graphics::g_SceneColorBuffer.GetWidth());
	m_Viewport.Height = static_cast<float>(Graphics::g_SceneColorBuffer.GetHeight());
	m_Viewport.MinDepth = 0.f;
	m_Viewport.MaxDepth = 1.f;

	m_Scissor.left = 0;
	m_Scissor.top = 0;
	m_Scissor.right = static_cast<LONG>(Graphics::g_SceneColorBuffer.GetWidth());
	m_Scissor.bottom = static_cast<LONG>(Graphics::g_SceneColorBuffer.GetHeight());
}

void PrimitiveRenderer::Render(GraphicsContext& gfxContext, const Math::Camera& camera)
{
	// skip completely if there is nothing queued
	if (std::none_of(m_PrimitiveQueues.begin(), m_PrimitiveQueues.end(), std::size<decltype(m_PrimitiveQueues)::value_type>)) return;

	ScopedTimer _prof(L"Primitives", gfxContext);

	gfxContext.SetRootSignature(m_RootSig);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());
	gfxContext.TransitionResource(m_VertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());

	gfxContext.SetPipelineState(m_SurfacePSO);
	gfxContext.SetViewportAndScissor(m_Viewport, m_Scissor);

	__declspec(align(16)) struct {
		XMFLOAT4X4 viewProjMatrix;
	} vsConstants;

	XMStoreFloat4x4(&vsConstants.viewProjMatrix, camera.GetViewProjMatrix());
	gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

	gfxContext.SetVertexBuffer(1, m_InstanceBuffer.VertexBufferView(max_instances, sizeof(InstanceData)));

	size_t instance = 0;
	for (int type = 0; type < m_PrimitiveQueues.size(); ++type)
	{
		auto& queue = m_PrimitiveQueues[type];
		// don't draw empty queues
		if (queue.size() == 0) continue;

		ASSERT(instance + queue.size() < max_instances, "Run out of primitives' instance buffer");
		memcpy(&m_InstanceBuffer[instance], queue.data(), queue.size() * sizeof(InstanceData));
		gfxContext.DrawIndexedInstanced(m_Foos[type].IndexCount, queue.size(), m_Foos[type].StartIndexLocation, m_Foos[type].BaseVertexLocation, instance);
		instance += queue.size();
	}
}
