#pragma once

#include "RootSignature.h"
#include "ColorBuffer.h"

class Sky
{
public:
	struct Parameters
	{
		// Rayleight
		DirectX::XMFLOAT3 RayleighScattering;
		float RayleighDensity;
		// Mie
		DirectX::XMFLOAT3 MieExtinction;
		float MieDensity;
		// TODO: Absorption/Ozone (more complicated than regular fall off)
		//DirectX::XMFLOAT3 AbsorptionScattering;
		//float AbsorptionDensity;
		// Planet
		float PlanetRadius;
		float AtmosphereHeight;
	};

	struct TextureResolutions
	{
		unsigned int TransmitanceAngleResolutionX;
		unsigned int TransmitanceAltitudeResolutionY;
		unsigned int SkyWidth; //?
		unsigned int SkyHeight; //?
		unsigned int AltiduteRes;
		unsigned int VzRes; //?
		unsigned int SzRes; //?
		unsigned int VsRes; //?
	};
public:
	void Initialize();

	void Precompute();

private:
	RootSignature m_RootSig;

	ComputePSO m_TransmittancePSO;
	ComputePSO m_IrradianceSinglePSO;
	ComputePSO m_InscatterSinglePSO;

	ColorBuffer m_Transmittance;
	//ColorBuffer m_Irradiance;

	TextureResolutions m_TexRes = {
		.TransmitanceAngleResolutionX = 256,
		.TransmitanceAltitudeResolutionY = 64,
		.SkyWidth = 64,
		.SkyHeight = 16,
		.AltiduteRes = 32,
		.VzRes = 128,
		.SzRes = 32,
		.VsRes = 8
	};

	Parameters m_AtmosphericParams = {
		.RayleighScattering = {5.8e-3f, 1.35e-2f, 3.31e-2f},
		.RayleighDensity = 8.f,
		.MieExtinction = {4e-3f / 0.9f, 4e-3f / 0.9f, 4e-3f / 0.9f},
		.MieDensity = 1.2f,
		.PlanetRadius = 6371.f,
		.AtmosphereHeight = 60.f
	};
};

