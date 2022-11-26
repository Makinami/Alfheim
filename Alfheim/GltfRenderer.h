#pragma once

#include <CommandContext.h>
#include <Camera.h>
#include <GpuBuffer.h>
#include <tiny_gltf.h>
#include <TextureManager.h>
#include "Model.h"

struct SimpleLight
{
	DirectX::XMFLOAT3 Strength;
	float FalloffStart;
	DirectX::XMFLOAT3 Direction;
	float FalloffEnd;
	DirectX::XMFLOAT3 Position;
	float SpotPower;
};

class GltfRenderer
{
	struct InstanceData
	{
		DirectX::XMFLOAT4X4 WorldTransformation;
		DirectX::XMFLOAT4X4 NormalTransformation;
	};

public:
	void Initialize();
	void Shutdown() {}

	void Update([[maybe_unused]] float deltaT) {}

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera, const std::vector<SimpleLight>& m_SimpleLights, Model& model, const std::vector<Math::Matrix4>& instances);

private:
	void DrawNode(GraphicsContext& gfxContext, const Model& model, int nodeId, Math::Matrix4 transformation, int instances);
	void DrawMesh(GraphicsContext& gfxContext, const Model::Mesh& mesh, Math::Matrix4 transformation, int instances);

	RootSignature m_RootSig;

	GraphicsPSO m_SurfacePSO;
	GraphicsPSO m_WireframePSO;

	StructuredUploadBuffer<InstanceData> m_InstanceBuffer;

	StructuredUploadBuffer<SimpleLight> m_SimpleLightsBuffer;

	static const int ms_MaximumLights = 16;
	static const int ms_MaximumInstances = 128;
};