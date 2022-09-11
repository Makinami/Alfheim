#include "pch.h"
#include "Stone.h"

#include <filesystem>

#include <GraphicsCommon.h>
#include <BufferManager.h>

#include "CompiledShaders/PrimitiveVS.h"
#include "CompiledShaders/PrimitivePS.h"

using namespace DirectX;

void Stone::Initialize()
{
	auto filename = "./Stone.glb";
	auto path = std::filesystem::current_path();

	std::string err, warn;
	tinygltf::TinyGLTF loader;
	loader.LoadBinaryFromFile(&m_Model, &err, &warn, filename);

	m_Buffers.reserve(m_Model.buffers.size());
	std::ranges::transform(m_Model.buffers, std::back_inserter(m_Buffers), [](const auto& buffer) {
		auto dxBuffer = ByteAddressBuffer{};
		dxBuffer.Create(L"", buffer.data.size(), sizeof(buffer.data[0]), buffer.data.data());
		return dxBuffer;
	});

	m_RootSig.Reset(1, 0);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig.Finalize(L"StoneRootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	D3D12_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
}

void Stone::Render([[maybe_unused]] GraphicsContext& gfxContext, [[maybe_unused]] const Math::Camera& camera)
{
	gfxContext.SetRootSignature(m_RootSig);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());
	gfxContext.TransitionResource(m_Buffers[0], D3D12_RESOURCE_STATE_GENERIC_READ);
	//gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());

	gfxContext.SetPipelineState(m_WireframePSO);
	gfxContext.SetViewportAndScissor(0, 0, Graphics::g_SceneColorBuffer.GetWidth(), Graphics::g_SceneColorBuffer.GetHeight());

	__declspec(align(16)) struct {
		XMFLOAT4X4 viewProjMatrix;
	} vsConstants;

	XMStoreFloat4x4(&vsConstants.viewProjMatrix, camera.GetViewProjMatrix());
	gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

	//gfxContext.SetVertexBuffer(2, m_InstanceBuffer.VertexBufferView(1, sizeof(InstanceData)));

	const auto& scene = m_Model.scenes[m_Model.defaultScene];
	for (const auto& nodeId : scene.nodes)
	{
		DrawNode(gfxContext, m_Model.nodes[nodeId]);
	}

	//gfxContext.DrawIndexedInstanced(m_Foos[type].IndexCount, 1, 0, 0, 0);
}

void Stone::DrawMesh([[maybe_unused]] GraphicsContext& gfxContext, [[maybe_unused]] const tinygltf::Mesh& mesh)
{
	for (const auto& primitive : mesh.primitives)
	{
		if (const auto it = primitive.attributes.find("POSITION"); it != primitive.attributes.end())
		{
			const auto& positionAccessor = m_Model.accessors[it->second];
			const auto& positionBufferView = m_Model.bufferViews[positionAccessor.bufferView];
			const auto stride = positionAccessor.ByteStride(positionBufferView);
			gfxContext.SetVertexBuffer(0, m_Buffers[positionBufferView.buffer].VertexBufferView(positionBufferView.byteOffset, positionAccessor.count * stride, stride));
		}

		if (const auto it = primitive.attributes.find("NORMAL"); it != primitive.attributes.end())
		{
			const auto& normalAccessor = m_Model.accessors[it->second];
			const auto& normalBufferView = m_Model.bufferViews[normalAccessor.bufferView];
			const auto stride = normalAccessor.ByteStride(normalBufferView);
			gfxContext.SetVertexBuffer(1, m_Buffers[normalBufferView.buffer].VertexBufferView(normalBufferView.byteOffset, normalAccessor.count * stride, stride));
		}

		const auto& indicesAccessor = m_Model.accessors[primitive.indices];
		const auto& indicesBufferView = m_Model.bufferViews[indicesAccessor.bufferView];
		gfxContext.SetIndexBuffer(m_Buffers[indicesBufferView.buffer].IndexBufferView(indicesBufferView.byteOffset));

		gfxContext.DrawIndexed(indicesAccessor.count);
	}
}

void Stone::DrawNode(GraphicsContext& gfxContext, const tinygltf::Node& node)
{
	if (node.mesh >= 0 && node.mesh < m_Model.meshes.size())
	{
		DrawMesh(gfxContext, m_Model.meshes[node.mesh]);
	}
	for (const auto& childId : node.children)
	{
		DrawNode(gfxContext, m_Model.nodes[childId]);
	}
}
