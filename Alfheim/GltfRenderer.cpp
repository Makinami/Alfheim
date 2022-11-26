#include "pch.h"
#include "GltfRenderer.h"

#include <filesystem>
#include <bitset>

#include <GraphicsCommon.h>
#include <BufferManager.h>
#include <SamplerManager.h>

#include "CompiledShaders/PrimitiveVS.h"
#include "CompiledShaders/PrimitivePS.h"

using namespace Math;

using namespace DirectX;

namespace {
	enum PSOOptions { kNormal, kWireframe, kPSOCount };
	const char* PSONames[] = { "Normal", "Wireframe" };
	EnumVar PSOOption("Model PSO", PSOOptions::kNormal, PSOOptions::kPSOCount, PSONames);
}

void GltfRenderer::Initialize()
{
	m_SimpleLightsBuffer.Create(L"Simple lights", ms_MaximumLights);

	m_RootSig.Reset(6, 0);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSig[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, static_cast<UINT>(5), D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, static_cast<UINT>(5), D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig[4].InitAsDescriptorTable(1);
	m_RootSig[4].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, 1);
	m_RootSig[5].InitAsBufferSRV(11, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig.Finalize(L"StoneRootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	D3D12_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

	m_InstanceBuffer.Create(L"Model instance buffer", ms_MaximumInstances);
}

XMMATRIX InverseTranspose(CXMMATRIX M)
{
	XMMATRIX A = M;
	A.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);

	XMVECTOR det = XMMatrixDeterminant(A);
	return XMMatrixTranspose(XMMatrixInverse(&det, A));
}

void GltfRenderer::Render(GraphicsContext& gfxContext, const Math::Camera& camera, const std::vector<SimpleLight>& m_SimpleLights, Model& model, const std::vector<Math::Matrix4>& instances)
{
	ASSERT(instances.size() < ms_MaximumInstances);
	ASSERT(m_SimpleLights.size() <= ms_MaximumLights);

	gfxContext.SetRootSignature(m_RootSig);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.TransitionResource(model.m_Buffers[0], D3D12_RESOURCE_STATE_GENERIC_READ);

	switch (PSOOption)
	{
	case kNormal: gfxContext.SetPipelineState(m_SurfacePSO); break;
	case kWireframe: gfxContext.SetPipelineState(m_WireframePSO); break;
	}
	gfxContext.SetViewportAndScissor(0, 0, Graphics::g_SceneColorBuffer.GetWidth(), Graphics::g_SceneColorBuffer.GetHeight());

	__declspec(align(16)) struct {
		XMFLOAT4X4 viewProjMatrix;
		XMFLOAT3 cameraPosition;
		int lightsNum;
	} vsConstants;

	XMStoreFloat4x4(&vsConstants.viewProjMatrix, camera.GetViewProjMatrix());
	XMStoreFloat3(&vsConstants.cameraPosition, camera.GetPosition());
	vsConstants.lightsNum = static_cast<int>(m_SimpleLights.size());
	gfxContext.SetDynamicConstantBufferView(1, sizeof(vsConstants), &vsConstants);

	//TODO: Optimize to write directly to the buffer
	auto instanceData = std::vector<InstanceData>();
	instanceData.reserve(instances.size());
	std::ranges::transform(instances, std::back_inserter(instanceData), [](const auto& meshTransformation) {
		auto instance = InstanceData{};
		XMStoreFloat4x4(&instance.WorldTransformation, meshTransformation);
		XMStoreFloat4x4(&instance.NormalTransformation, InverseTranspose(meshTransformation));
		return instance;
	});
	memcpy(&m_InstanceBuffer[0], instanceData.data(), instanceData.size() * sizeof(instanceData[0]));
	gfxContext.SetBufferSRV(5, m_InstanceBuffer);

	auto srvs = std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>();
	srvs.reserve(model.m_Textures.size());
	std::ranges::transform(model.m_Textures, std::back_inserter(srvs), [](const auto& texture) {
		return texture.GetSRV();
		});
	gfxContext.SetDynamicDescriptors(2, 0, static_cast<UINT>(srvs.size()), srvs.data());
	gfxContext.SetDynamicSamplers(3, 0, static_cast<UINT>(model.m_Samplers.size()), model.m_Samplers.data());
	gfxContext.SetDynamicDescriptor(4, 0, model.m_Materials.GetSRV());

	memcpy(&m_SimpleLightsBuffer[0], m_SimpleLights.data(), m_SimpleLights.size() * sizeof(m_SimpleLights[0]));
	gfxContext.SetDynamicDescriptor(4, 1, m_SimpleLightsBuffer.GetSRV());

	const auto& scene = model.scenes[model.defaultScene];
	for (const auto& nodeId : scene.nodes)
	{
		DrawNode(gfxContext, model, nodeId, Matrix4{kIdentity}, static_cast<int>(instanceData.size()));
	}
}

void GltfRenderer::DrawMesh(GraphicsContext& gfxContext, const Model::Mesh& mesh, Matrix4 transformation, int instances)
{
	__declspec(align(16)) struct {
		XMFLOAT4X4 worldTransformation;
		XMFLOAT4X4 normalTransformation;
		int materialId;
	} vsConstants;

	XMStoreFloat4x4(&vsConstants.worldTransformation, transformation);
	XMStoreFloat4x4(&vsConstants.normalTransformation, InverseTranspose(transformation));

	for (const auto& primitive : mesh.m_Primitives)
	{
		vsConstants.materialId = primitive.m_MaterialId;
		gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

		if (const auto it = primitive.m_VertexBufferViews.find("POSITION"); it != primitive.m_VertexBufferViews.end())
			gfxContext.SetVertexBuffer(0, it->second);

		if (const auto it = primitive.m_VertexBufferViews.find("NORMAL"); it != primitive.m_VertexBufferViews.end())
			gfxContext.SetVertexBuffer(1, it->second);

		if (const auto it = primitive.m_VertexBufferViews.find("TEXCOORD_0"); it != primitive.m_VertexBufferViews.end())
			gfxContext.SetVertexBuffer(2, it->second);

		if (const auto it = primitive.m_VertexBufferViews.find("TANGENT"); it != primitive.m_VertexBufferViews.end())
			gfxContext.SetVertexBuffer(3, it->second);

		gfxContext.SetIndexBuffer(primitive.m_IndexBufferView);

		gfxContext.DrawIndexedInstanced(primitive.m_IndexCount, instances, 0, 0, 0);
	}
}

void GltfRenderer::DrawNode(GraphicsContext& gfxContext, const Model& model, int nodeId, Matrix4 transformation, int instances)
{
	const auto& node = model.m_Nodes[nodeId];

	transformation = node.m_Transformation * OrthogonalTransform{ node.m_Rotation, node.m_Translation } * Matrix4::MakeScale(node.m_Scale) * transformation;

	if (node.m_MeshId >= 0 && node.m_MeshId < model.m_Meshes.size())
	{
		DrawMesh(gfxContext, model.m_Meshes[node.m_MeshId], transformation, instances);
	}
	for (const auto& childId : node.m_Children)
	{
		DrawNode(gfxContext, model, childId, transformation, instances);
	}
}
