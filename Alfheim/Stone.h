#pragma once

#include <CommandContext.h>
#include <Camera.h>
#include <GpuBuffer.h>
#include <tiny_gltf.h>

class Stone
{
	struct InstanceData
	{
		DirectX::XMFLOAT4X4 WorldTransformation;
	};

public:
	void Initialize();
	void Shutdown() {}

	void Update([[maybe_unused]] float deltaT) {}

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera);

private:
	void DrawNode(GraphicsContext& gfxContext, const tinygltf::Node& node);
	void DrawMesh(GraphicsContext& gfxContext, const tinygltf::Mesh& mesh);

	RootSignature m_RootSig;

	GraphicsPSO m_SurfacePSO;
	GraphicsPSO m_WireframePSO;

	StructuredBuffer m_PositionBuffer;
	StructuredBuffer m_NormalBuffer;
	StructuredUploadBuffer<InstanceData> m_InstanceBuffer;

	std::vector<ByteAddressBuffer> m_Buffers;

	tinygltf::Model m_Model;
};