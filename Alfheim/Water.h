#pragma once

#include "ColorBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "CommandContext.h"
#include "Camera.h"

class Water
{
public:
	void Initialize();

	void Update(float deltaT);

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera);

private:
	void ResetParameterDependentResources(ComputeContext& cmpContext);

private:
	ColorBuffer m_Spectrum;
	ColorBuffer m_Phase;
	ColorBuffer m_Waves;
	//ColorBuffer m_Slopes;

	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;

	const int m_FFTSize = 256;
	std::array<float, 4> m_GridSize = { 5488.0f, 392.0f, 28.0f, 2.0f };

	RootSignature m_RootSig;

	float m_Time = 0.f;

	ComputePSO m_GenerateSpectrumPSO;
	ComputePSO m_PrepareFFTPSO;
	ComputePSO m_FFTPSO;

	GraphicsPSO m_ForwardPSO;
};

