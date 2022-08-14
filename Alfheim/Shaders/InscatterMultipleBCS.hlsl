#include "SkyH.hlsli"

float3 ComputeMultipleScattering(float r, float mu, float mu_s, float nu, bool intersects_ground)
{
	float dx = DistanceToNearestAtmosphereBoundary(r, mu, intersects_ground) / INSCATTER_INTEGRAL_SAMPLES;
	float3 rayleigh_mie_sum = float3(0.0, 0.0, 0.0);
	for (int i = 0; i <= INSCATTER_INTEGRAL_SAMPLES; ++i)
	{
		float d_i = i * dx;

		// The r, mu and mu_s parameters at the current integration point.
		float r_i = clamp(sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r), BottomRadius, TopRadius);
		float mu_i = clamp((r * mu + d_i) / r_i, -1, 1);
		float mu_s_i = clamp((r * mu_s + d_i * nu) / r_i, -1, 1);

		// The Rayleigh and Mie multiple scattering at the current sample point.
		float3 rayleigh_mie_i = GetScattering(RODeltaJ, r_i, mu_i, mu_s_i, nu, intersects_ground).rgb * GetTransmittance(r, mu, d_i, intersects_ground) * dx;

		float weight_i = (i == 0 || i == INSCATTER_INTEGRAL_SAMPLES) ? 0.5 : 1.0;
		rayleigh_mie_sum += rayleigh_mie_i * weight_i;
	}
	return rayleigh_mie_sum;
}

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float r, mu, mu_s, nu;
	bool intersects_ground;
	GetRMuMuSNuFromScatteringTextureCoordinates(DTid, r, mu, mu_s, nu, intersects_ground);
	float3 result = ComputeMultipleScattering(r, mu, mu_s, nu, intersects_ground) / RayleighPhaseFunction(nu);

	RWDeltaInscatter[DTid] = float4(result, 0.0);
	RWInscatter[DTid] += float4(result, 0.0);
}