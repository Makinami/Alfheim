#pragma once

#include "RootSignature.h"
#include "PipelineState.h"
#include "CommandContext.h"
#include "Camera.h"
#include "TextureManager.h"

class Terrain
{
public:
	void Initialize();

	void Update([[maybe_unused]] float deltaT) {  };

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera);

private:
	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;

	RootSignature m_RootSig;

	GraphicsPSO m_RenderPSO;

	Texture m_HeightMap;

	int m_Size = 1024;
};