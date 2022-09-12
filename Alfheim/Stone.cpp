#include "pch.h"
#include "Stone.h"

#include <filesystem>

#include <GraphicsCommon.h>
#include <BufferManager.h>
#include <SamplerManager.h>

#include "CompiledShaders/PrimitiveVS.h"
#include "CompiledShaders/PrimitivePS.h"
using namespace Math;

using namespace DirectX;

[[nodiscard]] D3D12_TEXTURE_ADDRESS_MODE GetAddressMode(int wrap) noexcept
{
	switch (wrap) {
	case TINYGLTF_TEXTURE_WRAP_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

[[nodiscard]] D3D12_FILTER GetFilter(int min, int mag) noexcept
{
	switch (mag) {
	case TINYGLTF_TEXTURE_FILTER_NEAREST:
		switch (min) {
		case TINYGLTF_TEXTURE_FILTER_NEAREST: return D3D12_FILTER_MIN_MAG_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_LINEAR: return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: return D3D12_FILTER_MIN_MAG_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
	case TINYGLTF_TEXTURE_FILTER_LINEAR:
		switch (min) {
		case TINYGLTF_TEXTURE_FILTER_NEAREST: return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_LINEAR: return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}
	return D3D12_FILTER_ANISOTROPIC;
}

[[nodiscard]] DXGI_FORMAT GetDxgiFormat(int components, int type) noexcept {


	switch (components) {
	case 1:
		switch (type) {
		case TINYGLTF_COMPONENT_TYPE_BYTE: return DXGI_FORMAT_R8_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return DXGI_FORMAT_R8_UNORM;
		case TINYGLTF_COMPONENT_TYPE_SHORT: return DXGI_FORMAT_R16_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return DXGI_FORMAT_R16_UNORM;
		case TINYGLTF_COMPONENT_TYPE_INT: return DXGI_FORMAT_R32_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return DXGI_FORMAT_R32_UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		}
	case 2:
		switch (type) {
		case TINYGLTF_COMPONENT_TYPE_BYTE: return DXGI_FORMAT_R8G8_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return DXGI_FORMAT_R8G8_UNORM;
		case TINYGLTF_COMPONENT_TYPE_SHORT: return DXGI_FORMAT_R16G16_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return DXGI_FORMAT_R16G16_UNORM;
		case TINYGLTF_COMPONENT_TYPE_INT: return DXGI_FORMAT_R32G32_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return DXGI_FORMAT_R32G32_UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
		}
	case 3:
		switch (type) {
		case TINYGLTF_COMPONENT_TYPE_INT: return DXGI_FORMAT_R32G32B32_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return DXGI_FORMAT_R32G32B32_UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
		}
	case 4:
		switch (type) {
		case TINYGLTF_COMPONENT_TYPE_BYTE: return DXGI_FORMAT_R8G8B8A8_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TINYGLTF_COMPONENT_TYPE_SHORT: return DXGI_FORMAT_R16G16B16A16_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return DXGI_FORMAT_R16G16B16A16_UNORM;
		case TINYGLTF_COMPONENT_TYPE_INT: return DXGI_FORMAT_R32G32B32A32_SINT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return DXGI_FORMAT_R32G32B32A32_UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}

void Stone::Initialize()
{
	auto filename = "./Stone.glb";
	auto path = std::filesystem::current_path();

	std::string err, warn;
	tinygltf::TinyGLTF loader;
	loader.LoadBinaryFromFile(&m_Model, &err, &warn, filename);

	m_Buffers.reserve(m_Model.buffers.size());
	std::ranges::transform(m_Model.buffers, std::back_inserter(m_Buffers), [](const tinygltf::Buffer& buffer) {
		auto dxBuffer = ByteAddressBuffer{};
		dxBuffer.Create(L"", buffer.data.size(), sizeof(buffer.data[0]), buffer.data.data());
		return dxBuffer;
	});

	m_Textures.reserve(m_Model.images.size());
	std::ranges::transform(m_Model.images, std::back_inserter(m_Textures), [](const tinygltf::Image& image) {
		auto texture = Texture{};
		texture.Create(image.width, image.height, GetDxgiFormat(image.component, image.pixel_type), image.image.data());
		return texture;
	});

	m_Samplers.reserve(m_Model.samplers.size());
	std::ranges::transform(m_Model.samplers, std::back_inserter(m_Samplers), [](const tinygltf::Sampler& sampler) {
		auto desc = SamplerDesc{};
		desc.AddressU = GetAddressMode(sampler.wrapS);
		desc.AddressV = GetAddressMode(sampler.wrapT);
		desc.AddressW = GetAddressMode(sampler.wrapR);
		desc.Filter = GetFilter(sampler.minFilter, sampler.magFilter);
		return desc.CreateDescriptor();
	});

	m_RootSig.Reset(4, 0);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
	m_RootSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 5);
	m_RootSig.Finalize(L"StoneRootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	D3D12_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	m_WireframePSO.SetRasterizerState(Graphics::RasterizerDefault);
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
	gfxContext.SetDynamicConstantBufferView(1, sizeof(vsConstants), &vsConstants);

	//gfxContext.SetVertexBuffer(2, m_InstanceBuffer.VertexBufferView(1, sizeof(InstanceData)));

	const auto& scene = m_Model.scenes[m_Model.defaultScene];
	const auto& transformation = Matrix4::MakeScale(50) * Matrix4{kIdentity};
	for (const auto& nodeId : scene.nodes)
	{
		DrawNode(gfxContext, m_Model.nodes[nodeId], transformation);
	}

	//gfxContext.DrawIndexedInstanced(m_Foos[type].IndexCount, 1, 0, 0, 0);
}

void Stone::DrawMesh(GraphicsContext& gfxContext, const tinygltf::Mesh& mesh, Matrix4 transformation)
{
	__declspec(align(16)) struct {
		XMFLOAT4X4 transformation;
	} vsConstants;

	XMStoreFloat4x4(&vsConstants.transformation, transformation);
	gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

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

		if (const auto it = primitive.attributes.find("TEXCOORD_0"); it != primitive.attributes.end())
		{
			const auto& textureCoordinatesAccessor = m_Model.accessors[it->second];
			const auto& textureCoordinatesBufferView = m_Model.bufferViews[textureCoordinatesAccessor.bufferView];
			const auto stride = textureCoordinatesAccessor.ByteStride(textureCoordinatesBufferView);
			gfxContext.SetVertexBuffer(2, m_Buffers[textureCoordinatesBufferView.buffer].VertexBufferView(textureCoordinatesBufferView.byteOffset, textureCoordinatesAccessor.count * stride, stride));
		}

		const auto& indicesAccessor = m_Model.accessors[primitive.indices];
		const auto& indicesBufferView = m_Model.bufferViews[indicesAccessor.bufferView];
		gfxContext.SetIndexBuffer(m_Buffers[indicesBufferView.buffer].IndexBufferView(indicesBufferView.byteOffset));

		const auto& material = m_Model.materials[primitive.material];
		{
			const auto it = material.extensions.find("KHR_materials_pbrSpecularGlossiness");
			const auto& pbrSpecularGlossiness = it->second;
			const auto& diffuseTexture = pbrSpecularGlossiness.Get("diffuseTexture");
			const auto& glTexture = m_Model.textures[diffuseTexture.Get("index").GetNumberAsInt()];
			const auto& dxTexture = m_Textures[glTexture.source];

			gfxContext.SetDynamicDescriptor(2, 0, dxTexture.GetSRV());
			gfxContext.SetDynamicSampler(3, 0, m_Samplers[glTexture.sampler]);
		}
		{
			const auto it = material.extensions.find("KHR_materials_pbrSpecularGlossiness");
			const auto& pbrSpecularGlossiness = it->second;
			const auto& diffuseTexture = pbrSpecularGlossiness.Get("specularGlossinessTexture");
			const auto& glTexture = m_Model.textures[diffuseTexture.Get("index").GetNumberAsInt()];
			const auto& dxTexture = m_Textures[glTexture.source];

			gfxContext.SetDynamicDescriptor(2, 1, dxTexture.GetSRV());
			gfxContext.SetDynamicSampler(3, 1, m_Samplers[glTexture.sampler]);
		}
		{
			const auto& normal = material.normalTexture;
			const auto& glTexture = m_Model.textures[normal.index];
			const auto& dxTexture = m_Textures[glTexture.source];

			gfxContext.SetDynamicDescriptor(2, 2, dxTexture.GetSRV());
			gfxContext.SetDynamicSampler(3, 2, m_Samplers[glTexture.sampler]);
		}
		{
			const auto& oclusion = material.occlusionTexture;
			const auto& glTexture = m_Model.textures[oclusion.index];
			const auto& dxTexture = m_Textures[glTexture.source];

			gfxContext.SetDynamicDescriptor(2, 3, dxTexture.GetSRV());
			gfxContext.SetDynamicSampler(3, 3, m_Samplers[glTexture.sampler]);
		}

		gfxContext.DrawIndexed(indicesAccessor.count);
	}
}

void Stone::DrawNode(GraphicsContext& gfxContext, const tinygltf::Node& node, Matrix4 transformation)
{
	if (node.matrix.size() == 16)
	{
		transformation = Matrix4{
			Vector4{static_cast<float>(node.matrix[0]), static_cast<float>(node.matrix[1]), static_cast<float>(node.matrix[2]), static_cast<float>(node.matrix[3])},
			Vector4{static_cast<float>(node.matrix[4]), static_cast<float>(node.matrix[5]), static_cast<float>(node.matrix[6]), static_cast<float>(node.matrix[7])},
			Vector4{static_cast<float>(node.matrix[8]), static_cast<float>(node.matrix[9]), static_cast<float>(node.matrix[10]), static_cast<float>(node.matrix[11])},
			Vector4{static_cast<float>(node.matrix[12]), static_cast<float>(node.matrix[13]), static_cast<float>(node.matrix[14]), static_cast<float>(node.matrix[15])},
		} * transformation;
	}
	else
	{
		if (node.scale.size() == 3)
		{
			transformation = Matrix4::MakeScale(Vector3{ static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2]) }) * transformation;
		}
		if (node.rotation.size() == 4 && (node.rotation[0] != 0 || node.rotation[1] != 0 || node.rotation[1] != 0))
		{
			transformation = Matrix4{ OrthogonalTransform{ Quaternion{Vector3{static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2])}, static_cast<float>(node.rotation[3])}} } *transformation;
		}
		if (node.translation.size() == 3)
		{
			transformation = Matrix4{ OrthogonalTransform{Vector3{static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]), static_cast<float>(node.translation[2])}} } * transformation;
		}
	}

	if (node.mesh >= 0 && node.mesh < m_Model.meshes.size())
	{
		DrawMesh(gfxContext, m_Model.meshes[node.mesh], transformation);
	}
	for (const auto& childId : node.children)
	{
		DrawNode(gfxContext, m_Model.nodes[childId], transformation);
	}
}
