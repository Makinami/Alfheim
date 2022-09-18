#pragma once

#include <filesystem>
#include <tiny_gltf.h>

#include "TextureManager.h"
#include <Math/Matrix4.h>


__declspec(align(16)) struct Material
{
	struct TextureAccessor
	{
		uint16_t TextureId = 0xffff;
		uint16_t SamplerId = 0xffff;

		TextureAccessor() = default;
		TextureAccessor(const tinygltf::Texture& texture) : TextureId{ static_cast<uint16_t>(texture.source) }, SamplerId{ static_cast<uint16_t>(texture.sampler) } {}
	};

	int DiffuseTextureId;
	int DiffuseSamplerId;

	int NormalTextureId;
	int NormalSamplerId;

	int OcclusionTextureId;
	int OcclusionSamplerId;

	DirectX::XMFLOAT4 BaseColorFactor;

	uint32_t Flags;
	DirectX::XMFLOAT3 pad;

	struct SpectralGlossinessProperties
	{
		DirectX::XMFLOAT4 DiffuseFactor = DirectX::XMFLOAT4{ 1.f, 1.f, 1.f, 1.f };
		TextureAccessor DiffuseTexture;

		DirectX::XMFLOAT3 SpecularFactor = DirectX::XMFLOAT3{1.f, 1.f, 1.f};
		float GlossinessFactor = 1.f;
		TextureAccessor SpecularGlossinessTexture;
	} SpectralGlossiness;
};

class Model;

class ModelReader
{
public:
	[[nodiscard]] Model Load(const std::filesystem::path filename);

private:
	[[nodiscard]] std::vector<Material> ProcessMaterials(const tinygltf::Model& model) noexcept;

	[[nodiscard]] Material::SpectralGlossinessProperties ProcessSpectralGlossiness(const tinygltf::Value& spectralGlossinessProperties, const std::vector<tinygltf::Texture>& textures) noexcept;
};

class Model : public tinygltf::Model
{
	friend ModelReader;
public:
	struct Primitive {
		std::unordered_map<std::string, D3D12_VERTEX_BUFFER_VIEW> m_VertexBufferViews;
		D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
		size_t m_IndexCount;
		int m_MaterialId;
		// Ignore topology
	};
	struct Mesh {
		std::vector<Primitive> m_Primitives;
		// Missing weights
	};
	struct Node {
		Math::Matrix4 m_Transformation = Math::Matrix4{ Math::kIdentity };
		Math::Vector3 m_Scale = Math::Vector3{ Math::kIdentity };
		Math::Quaternion m_Rotation = Math::Quaternion{ Math::kIdentity };
		Math::Vector3 m_Translation = Math::Vector3{ Math::kZero };
		std::vector<int> m_Children;
		int m_MeshId = -1;
		// Ignore skin,, weights, camera
	};

	std::vector<ByteAddressBuffer> m_Buffers;
	std::vector<Texture> m_Textures;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_Samplers;
	StructuredBuffer m_Materials;
	std::vector<Mesh> m_Meshes;
	std::vector<Node> m_Nodes;
};

