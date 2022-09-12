#pragma once

#include <CommandContext.h>
#include <Camera.h>
#include <GpuBuffer.h>
#include <tiny_gltf.h>
#include <TextureManager.h>

class Stone
{
	struct InstanceData
	{
		DirectX::XMFLOAT4X4 WorldTransformation;
	};

public:
	void Initialize(std::string_view filename);
	void Shutdown() {}

	void Update([[maybe_unused]] float deltaT) {}

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera);

private:
	void DrawNode(GraphicsContext& gfxContext, const tinygltf::Node& node, Math::Matrix4 transformation);
	void DrawMesh(GraphicsContext& gfxContext, const tinygltf::Mesh& mesh, Math::Matrix4 transformation);

	RootSignature m_RootSig;

	GraphicsPSO m_SurfacePSO;
	GraphicsPSO m_WireframePSO;

	StructuredBuffer m_PositionBuffer;
	StructuredBuffer m_NormalBuffer;
	StructuredUploadBuffer<InstanceData> m_InstanceBuffer;

	std::vector<ByteAddressBuffer> m_Buffers;
	std::vector<Texture> m_Textures;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_Samplers;

	tinygltf::Model m_Model;
};