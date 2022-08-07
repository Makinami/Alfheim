#include "pch.h"
#include "Sky.h"

#include "CommandContext.h"
#include "GraphicsCommon.h"
#include "SamplerManager.h"

#include "CompiledShaders/TransmittanceCS.h"
#include "CompiledShaders/IrradianceSingleCS.h"
#include "CompiledShaders/InscatterSingleCS.h"

using namespace DirectX;

void Sky::Initialize()
{
	m_RootSig.Reset(3, 1);
	m_RootSig[0].InitAsConstantBuffer(0);
	m_RootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 3);
	m_RootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSig.InitStaticSampler(0, Graphics::SamplerLinearClampDesc);
	m_RootSig.Finalize(L"Sky precompute root signature");

	m_Transmittance.Init(L"Transmittance texture", m_TexRes.TransmitanceAngleResolutionX, m_TexRes.TransmitanceAltitudeResolutionY, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(m_RootSig); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(m_TransmittancePSO, g_pTransmittanceCS);
	CreatePSO(m_IrradianceSinglePSO, g_pIrradianceSingleCS);
	CreatePSO(m_InscatterSinglePSO, g_pInscatterSingleCS);

#undef CreatePSO
}

void Sky::Precompute()
{
	auto DeltaIrradiance = ColorBuffer{};
	DeltaIrradiance.Init(L"Delta irradiance - ΔE", m_TexRes.SkyWidth, m_TexRes.SkyHeight, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	auto DeltaInscatter = ColorBuffer();
	DeltaInscatter.Init3D(L"Delta inscatter - ΔS", m_TexRes.SzRes * m_TexRes.VsRes, m_TexRes.VzRes, m_TexRes.AltiduteRes, DXGI_FORMAT_R32G32B32A32_FLOAT);

	auto& cmpContext = ComputeContext::Begin(L"Sky precomputation", false);

	cmpContext.SetRootSignature(m_RootSig);

	__declspec(align(16)) struct {
		XMFLOAT2 TrasnmittanceRes;
		XMFLOAT2 SkyRes;
		XMFLOAT4 InscatterRes;
		XMFLOAT3 RayleighScattering;
		float RayleighDensity;
		XMFLOAT3 MieExtinction;
		float MieDensity;
		float TopRadius;
		float BottomRadius;
	} constants;
	constants.TrasnmittanceRes = { static_cast<float>(m_TexRes.TransmitanceAngleResolutionX), static_cast<float>(m_TexRes.TransmitanceAltitudeResolutionY) };
	constants.SkyRes = { static_cast<float>(m_TexRes.SkyWidth), static_cast<float>(m_TexRes.SkyHeight) };
	constants.InscatterRes = { static_cast<float>(m_TexRes.AltiduteRes), static_cast<float>(m_TexRes.VzRes), static_cast<float>(m_TexRes.SzRes), static_cast<float>(m_TexRes.VsRes) };
	constants.TopRadius = m_AtmosphericParams.PlanetRadius + m_AtmosphericParams.AtmosphereHeight;
	constants.BottomRadius = m_AtmosphericParams.PlanetRadius;
	constants.RayleighScattering = m_AtmosphericParams.RayleighScattering;
	constants.RayleighDensity = m_AtmosphericParams.RayleighDensity;
	constants.MieExtinction = m_AtmosphericParams.MieExtinction;
	constants.MieDensity = m_AtmosphericParams.MieDensity;
	cmpContext.SetDynamicConstantBufferView(0, sizeof(constants), &constants);

	cmpContext.SetDynamicDescriptor(1, 0, m_Transmittance.GetUAV());
	cmpContext.SetDynamicDescriptor(1, 1, DeltaIrradiance.GetUAV());
	cmpContext.SetDynamicDescriptor(1, 2, DeltaInscatter.GetUAV());

	cmpContext.SetDynamicDescriptor(2, 0, m_Transmittance.GetSRV());

	// line 1
	// T(x,v) = T(x,x0(x,v))
	cmpContext.SetPipelineState(m_TransmittancePSO);
	cmpContext.TransitionResource(m_Transmittance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmpContext.Dispatch2D(m_TexRes.TransmitanceAngleResolutionX, m_TexRes.TransmitanceAltitudeResolutionY);

	// line 2
	// deltaE = eps[L](x,s)
	cmpContext.SetPipelineState(m_IrradianceSinglePSO);
	cmpContext.TransitionResource(m_Transmittance, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cmpContext.TransitionResource(DeltaIrradiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmpContext.Dispatch2D(m_TexRes.SkyWidth, m_TexRes.SkyHeight);

	// line 3
	// deltaS = S[L](x,v,s)
	cmpContext.SetPipelineState(m_InscatterSinglePSO);
	cmpContext.TransitionResource(DeltaInscatter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmpContext.Dispatch3D(DeltaInscatter.GetWidth(), DeltaInscatter.GetHeight(), DeltaInscatter.GetWidth(), 8, 8, 1);

	cmpContext.Finish(true);
}
