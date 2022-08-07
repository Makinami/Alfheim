#include "SkyH.hlsli"

void ComputeSingleScatteringIntegrand(float r, float mu, float mu_s, float nu, float d, bool intersects_ground, out float3 rayleigh, out float3 mie)
{
	float r_d = clamp(sqrt(d * d + 2.0 * r * mu * d + r * r), BottomRadius, TopRadius);
	float mu_s_d = clamp((r * mu_s + d * nu) / r_d, -1, 1);
	float3 transmittance = GetTransmittance(r, mu, d, intersects_ground) * GetTransmittanceToSun(r, mu_s);
	rayleigh = transmittance * exp(-(r - BottomRadius) / RayleighDensity);
	mie = transmittance * exp(-(r - BottomRadius) / MieDensity);
}

void ComputeSingleScattering(float r, float mu, float mu_s, float nu, bool intersects_ground, out float3 rayleigh, out float3 mie)
{
	float dx = (intersects_ground ? DistanceToBottomAtmosphereBoundary(r, mu) : DistanceToTopAtmosphereBoundary(r, mu)) / INSCATTER_INTEGRAL_SAMPLES;
	float3 rayleigh_sum = 0;
	float3 mie_sum = 0;

	for (int i = 0; i <= INSCATTER_INTEGRAL_SAMPLES; ++i)
	{
		float d_i = i * dx;
		float3 rayleigh_i;
		float3 mie_i;

		ComputeSingleScatteringIntegrand(r, mu, mu_s, nu, d_i, intersects_ground, rayleigh_i, mie_i);

		float weight_i = (i == 0 || i == INSCATTER_INTEGRAL_SAMPLES) ? 0.5 : 1.0;
		rayleigh_sum += rayleigh_i * weight_i;
		mie_sum += mie_i * weight_i;
	}
	rayleigh = rayleigh_sum * dx * SolarIrradiance * RayleighScattering;
	mie = mie_sum * dx * SolarIrradiance * MieScattering;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float r, mu, mu_s, nu;
	bool intersects_ground;
	GetRMuMuSNuFromScatteringTextureCoordinates(DTid, r, mu, mu_s, nu, intersects_ground);
	float3 rayleigh, mie;
	ComputeSingleScattering(r, mu, mu_s, nu, intersects_ground, rayleigh, mie);
	RWDeltaInscatter[DTid] = float4(rayleigh, mie.r);
}