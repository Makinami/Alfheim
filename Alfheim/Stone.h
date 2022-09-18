#pragma once

#include <CommandContext.h>
#include <Camera.h>
#include <GpuBuffer.h>
#include <tiny_gltf.h>
#include <TextureManager.h>
#include "Model.h"

struct SimpleLight;

class Stone
{
	struct SimpleLight
	{
		DirectX::XMFLOAT3 Strength;
		float FalloffStart;
		DirectX::XMFLOAT3 Direction;
		float FalloffEnd;
		DirectX::XMFLOAT3 Position;
		float SpotPower;
	};

public:
	void Initialize();
	void Shutdown() {}

	void Update([[maybe_unused]] float deltaT) {}

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera, Model& model, Math::Matrix4 transformation);

private:
	void DrawNode(GraphicsContext& gfxContext, const Model& model, int nodeId, Math::Matrix4 transformation);
	void DrawMesh(GraphicsContext& gfxContext, const Model::Mesh& mesh, Math::Matrix4 transformation);

	RootSignature m_RootSig;

	GraphicsPSO m_SurfacePSO;
	GraphicsPSO m_WireframePSO;

	std::vector<SimpleLight> m_SimpleLights;
	StructuredUploadBuffer<SimpleLight> m_SimpleLightsBuffer;
};