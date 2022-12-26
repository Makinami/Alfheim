#include "pch.h"

#include "Model.h"
#include "Math/BoundingSphere.h"

#include <tiny_gltf.h>
#include <bitset>
#include <numeric>

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

Math::BoundingSphere BuildNodeBoundingSphere(const Model& model, int nodeId)
{
	const auto& node = model.m_Nodes[nodeId];
	if (node.m_MeshId != -1)
	{
		return model.m_Meshes[node.m_MeshId].m_BoundingSphere;
	}
	else
	{
		return std::reduce(node.m_Children.begin(), node.m_Children.end(), Math::BoundingSphere(), [&](const Math::BoundingSphere& sphere, int childId) {
			return sphere.Union(BuildNodeBoundingSphere(model, childId));
			});
	}
}

Model ModelReader::Load(const std::filesystem::path filename)
{
	auto model = Model{};
	std::string warning, error;
	if (!tinygltf::TinyGLTF().LoadBinaryFromFile(&model, &error, &warning, filename.string())) throw new std::runtime_error(error);
	if (!warning.empty()) Utility::Print(warning);

	model.m_Buffers.reserve(model.buffers.size());
	std::ranges::transform(model.buffers, std::back_inserter(model.m_Buffers), [](const tinygltf::Buffer& buffer) {
		auto dxBuffer = ByteAddressBuffer{};
		dxBuffer.Create(L"", buffer.data.size(), sizeof(buffer.data[0]), buffer.data.data());
		return dxBuffer;
	});

	model.m_Textures.reserve(model.images.size());
	std::ranges::transform(model.images, std::back_inserter(model.m_Textures), [](const tinygltf::Image& image) {
		auto texture = Texture{};
		texture.Create(image.width, image.height, GetDxgiFormat(image.component, image.pixel_type), image.image.data());
		return texture;
	});

	model.m_Samplers.reserve(model.samplers.size());
	std::ranges::transform(model.samplers, std::back_inserter(model.m_Samplers), [](const tinygltf::Sampler& sampler) {
		auto desc = SamplerDesc{};
		desc.AddressU = GetAddressMode(sampler.wrapS);
		desc.AddressV = GetAddressMode(sampler.wrapT);
		desc.AddressW = GetAddressMode(sampler.wrapR);
		desc.Filter = GetFilter(sampler.minFilter, sampler.magFilter);
		return desc.CreateDescriptor();
	});

	const auto materials = ProcessMaterials(model);
	model.m_Materials.Create(fmt::format(L"{} - materials", filename.c_str()), materials.size(), sizeof(materials[0]), materials.data());

	model.m_Meshes.reserve(model.meshes.size());
	std::ranges::transform(model.meshes, std::back_inserter(model.m_Meshes), [&](const tinygltf::Mesh& mesh) {
		auto newMesh = Model::Mesh{};
		std::ranges::transform(mesh.primitives, std::back_inserter(newMesh.m_Primitives), [&](const tinygltf::Primitive& gltfPrimitive) {
			auto primitive = Model::Primitive{};
			for (const auto& [name, accessorId] : gltfPrimitive.attributes) {
				const auto& accessor = model.accessors[accessorId];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				const auto stride = accessor.ByteStride(bufferView);
				primitive.m_VertexBufferViews.insert({ name, model.m_Buffers[bufferView.buffer].VertexBufferView(bufferView.byteOffset, accessor.count * stride, stride) });
			}
			{
				const auto& accessor = model.accessors[gltfPrimitive.indices];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				primitive.m_IndexBufferView = model.m_Buffers[bufferView.buffer].IndexBufferView(bufferView.byteOffset);
				primitive.m_IndexCount = accessor.count;
			}
			primitive.m_MaterialId = gltfPrimitive.material;

			auto position = gltfPrimitive.attributes.find("POSITION");
			if (position != gltfPrimitive.attributes.end()) {
				auto accessor = model.accessors[position->second];
				assert(accessor.minValues.size() == 3 && accessor.maxValues.size() == 3);
				auto min = Math::Vector3(static_cast<float>(accessor.minValues[0]), static_cast<float>(accessor.minValues[1]), static_cast<float>(accessor.minValues[2]));
				auto max = Math::Vector3(static_cast<float>(accessor.maxValues[0]), static_cast<float>(accessor.maxValues[1]), static_cast<float>(accessor.maxValues[2]));
				auto primitiveSphere = Math::BoundingSphere((min + max) * 0.5f, (max - min).Length() * 0.5f);
				newMesh.m_BoundingSphere = newMesh.m_BoundingSphere.Union(primitiveSphere);
			}

			return primitive;
		});
		return newMesh;
	});

	model.m_Nodes.reserve(model.nodes.size());
	std::ranges::transform(model.nodes, std::back_inserter(model.m_Nodes), [&](const auto& gltfNode) {
		auto node = Model::Node{};
		if (gltfNode.matrix.size() == 16)
		{
			node.m_Transformation = Math::Matrix4{
				Math::Vector4{static_cast<float>(gltfNode.matrix[0]), static_cast<float>(gltfNode.matrix[1]), static_cast<float>(gltfNode.matrix[2]), static_cast<float>(gltfNode.matrix[3])},
				Math::Vector4{static_cast<float>(gltfNode.matrix[4]), static_cast<float>(gltfNode.matrix[5]), static_cast<float>(gltfNode.matrix[6]), static_cast<float>(gltfNode.matrix[7])},
				Math::Vector4{static_cast<float>(gltfNode.matrix[8]), static_cast<float>(gltfNode.matrix[9]), static_cast<float>(gltfNode.matrix[10]), static_cast<float>(gltfNode.matrix[11])},
				Math::Vector4{static_cast<float>(gltfNode.matrix[12]), static_cast<float>(gltfNode.matrix[13]), static_cast<float>(gltfNode.matrix[14]), static_cast<float>(gltfNode.matrix[15])},
			};
		}
		else
		{
			//TODO: rewrite into m_Transformation
			if (gltfNode.scale.size() == 3)
				node.m_Scale = Math::Vector3{ static_cast<float>(gltfNode.scale[0]), static_cast<float>(gltfNode.scale[1]), static_cast<float>(gltfNode.scale[2]) };

			if (gltfNode.rotation.size() == 4)
				node.m_Rotation = Math::Quaternion{ Math::Vector3{static_cast<float>(gltfNode.rotation[0]), static_cast<float>(gltfNode.rotation[1]), static_cast<float>(gltfNode.rotation[2])}, static_cast<float>(gltfNode.rotation[3]) };

			if (gltfNode.translation.size() == 3)
				node.m_Translation = Math::Vector3{static_cast<float>(gltfNode.translation[0]), static_cast<float>(gltfNode.translation[1]), static_cast<float>(gltfNode.translation[2]) };
		}
		node.m_MeshId = gltfNode.mesh;
		node.m_Children = std::move(gltfNode.children);
		return node;
	});

	model.m_Scenes.reserve(model.scenes.size());
	std::ranges::transform(model.scenes, std::back_inserter(model.m_Scenes), [&](const auto& gltfScene) {
		auto scene = Model::Scene{};
		scene.m_Name = gltfScene.name;
		scene.m_Nodes = gltfScene.nodes;
		
		scene.m_BoundingSphere = std::reduce(scene.m_Nodes.begin(), scene.m_Nodes.end(), Math::BoundingSphere(), [&](const Math::BoundingSphere& sphere, int childId) {
			return sphere.Union(BuildNodeBoundingSphere(model, childId));
		});

		return scene;
	});

	return model;
}

std::vector<Material> ModelReader::ProcessMaterials(const tinygltf::Model& model) noexcept
{
	auto materials = std::vector<Material>();
	materials.reserve(model.materials.size());
	std::ranges::transform(model.materials, std::back_inserter(materials), [this, &model](const tinygltf::Material& material) {
		std::bitset<32> flags;

		auto localMaterial = Material{};

		// Specular Glossiness
		if (const auto it = material.extensions.find("KHR_materials_pbrSpecularGlossiness"); it != material.extensions.end())
		{
			localMaterial.SpectralGlossiness = ProcessSpectralGlossiness(it->second, model.textures);
			flags.set(0);
		}
		{
			//const auto it = material.extensions.find("KHR_materials_pbrSpecularGlossiness");
			//const auto& pbrSpecularGlossiness = it->second;
			//const auto& diffuseTexture = pbrSpecularGlossiness.Get("specularGlossinessTexture");
			//const auto& glTexture = m_Model.textures[diffuseTexture.Get("index").GetNumberAsInt()];
			////
		}
		{
			const auto& normal = material.normalTexture;
			const auto& glTexture = model.textures[normal.index];
			localMaterial.NormalTextureId = glTexture.source;
			localMaterial.NormalSamplerId = glTexture.sampler;
		}
		{
			const auto& oclusion = material.occlusionTexture;
			const auto& glTexture = model.textures[oclusion.index];
			localMaterial.OcclusionTextureId = glTexture.source;
			localMaterial.OcclusionSamplerId = glTexture.sampler;
		}

		localMaterial.Flags = flags.to_ulong();
		return localMaterial;
	});

	return materials;
}

// https://kcoley.github.io/glTF/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness/schema/glTF.KHR_materials_pbrSpecularGlossiness.schema.json
Material::SpectralGlossinessProperties ModelReader::ProcessSpectralGlossiness(const tinygltf::Value& properties, const std::vector<tinygltf::Texture>& textures) noexcept
{
	Material::SpectralGlossinessProperties result;

	if (const auto diffuseFactor = properties.Get("diffuseFactor"); diffuseFactor.IsArray())
		result.DiffuseFactor = DirectX::XMFLOAT4{ static_cast<float>(diffuseFactor.Get(0).GetNumberAsDouble()), static_cast<float>(diffuseFactor.Get(1).GetNumberAsDouble()), static_cast<float>(diffuseFactor.Get(2).GetNumberAsDouble()), static_cast<float>(diffuseFactor.Get(3).GetNumberAsDouble()) };

	if (const auto& diffuseTexture = properties.Get("diffuseTexture"); diffuseTexture.IsObject())
		result.DiffuseTexture = textures[diffuseTexture.Get("index").GetNumberAsInt()];

	if (const auto specularFactor = properties.Get("specularFactor"); specularFactor.IsArray())
		result.SpecularFactor = DirectX::XMFLOAT3{ static_cast<float>(specularFactor.Get(0).GetNumberAsDouble()), static_cast<float>(specularFactor.Get(1).GetNumberAsDouble()), static_cast<float>(specularFactor.Get(2).GetNumberAsDouble()) };

	if (const auto glossinessFactor = properties.Get("glossinessFactor"); glossinessFactor.IsNumber())
		result.GlossinessFactor = static_cast<float>(glossinessFactor.Get(0).GetNumberAsDouble());

	if (const auto& specularGlossinessTexture = properties.Get("specularGlossinessTexture"); specularGlossinessTexture.IsObject())
		result.SpecularGlossinessTexture = textures[specularGlossinessTexture.Get("index").GetNumberAsInt()];

	return result;
}
