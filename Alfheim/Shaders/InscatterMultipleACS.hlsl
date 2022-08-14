#include "SkyH.hlsli"

float3 ComputeScatteringDensity(float r, float mu, float mu_s, float nu)
{
	// Compute unit direction vectors for the zenith, the view direction omega and
	// and the sun direction omega_s, such that the cosine of the view-zenith
	// angle is mu, the cosine of the sun-zenith angle is mu_s, and the cosine of
	// the view-sun angle is nu. The goal is to simplify computations below.
	float3 zenith_direction = float3(0, 0, 1);
	float3 omega = float3(sqrt(1.0 - mu * mu), 0.0, mu);
	float sun_dir_x = omega.x == 0.0 ? 0.0 : (nu - mu * mu_s) / omega.x;
	float sun_dir_y = sqrt(max(1.0 - sun_dir_x * sun_dir_x - mu_s * mu_s, 0.0));
	float3 omega_s = float3(sun_dir_x, sun_dir_y, mu_s);

	const float dphi = PI / INSCATTER_SPHERICAL_INTEGRAL_SAMPLES;
	const float dtheta = PI / INSCATTER_SPHERICAL_INTEGRAL_SAMPLES;
	float3 rayleigh_mie = float3(0.0, 0.0, 0.0);

	// Nested loops for the integral over all the incident directions omega_i.
	for (int l = 0; l < INSCATTER_SPHERICAL_INTEGRAL_SAMPLES; ++l)
	{
		float theta = (l + 0.5) * dtheta;
		float cos_theta = cos(theta);
		float sin_theta = sin(theta);
		bool intersects_ground = RayIntersectsGround(r, cos_theta);

		// The distance and transmittance to the ground only depend on theta, so we
		// can compute them in the outer loop for efficiency.
		float distance_to_ground = 0.0;
		float3 transmittance_to_ground = float3(0.0, 0.0, 0.0);
		float3 ground_albedo = float3(0.0, 0.0, 0.0);
		if (intersects_ground)
		{
			distance_to_ground = DistanceToBottomAtmosphereBoundary(r, cos_theta);
			transmittance_to_ground = GetTransmittance(r, cos_theta, distance_to_ground, true);
			ground_albedo = GroundAlbedo;
		}

		for (int m = 0; m < 2 * INSCATTER_SPHERICAL_INTEGRAL_SAMPLES; ++m)
		{
			float phi = (m + 0.5) * dphi;
			float3 omega_i = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
			float domega_i = dtheta * dphi * sin(theta);

			// The radiance L_i arriving from direction omega_i after n-1 bounces is
			// the sum of a term given by the precomputed scattering texture for the
			// (n-1)-th order:
			float nu1 = dot(omega_s, omega_i);
			float3 incident_radiance = GetScattering(ROInscatter, r, omega_i.z, mu_s, nu1, intersects_ground, Order - 1).rgb;

			// and of the contribution from the light paths with n-1 bounces and whose
			// last bounce is on the ground. This contribution is the product of the
			// transmittane to the ground, the ground albedo, the ground BRDF, and
			// the irradiance receiver on the ground after n-2 bounces.
			float3 ground_normal = normalize(zenith_direction * r + omega_i * distance_to_ground);
			float3 ground_irradiance = GetIrradiance(BottomRadius, dot(ground_normal, omega_s));
			incident_radiance += transmittance_to_ground * ground_albedo * (1.0 / PI) * ground_irradiance;

			// The radiance finally scattered from direction omega_i towards direction
			// -omega is the product of the incident radiance, the scattering
			// coefficient, and the phase function for directions omega and omega_i
			// (all thi summed over all particle types, i.e. Rayleigh and Mie).
			float nu2 = dot(omega, omega_i);
			float rayleigh_density = GetDensity(RayleighDensity, r);
			float mie_density = GetDensity(MieDensity, r);
			rayleigh_mie += incident_radiance * (RayleighScattering * rayleigh_density * RayleighPhaseFunction(nu2) +
				MieScattering * mie_density * MiePhaseFunction(nu2)) * domega_i;
		}
	}

	return rayleigh_mie;
}

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float r, mu, mu_s, nu;
	bool intersects_ground;
	GetRMuMuSNuFromScatteringTextureCoordinates(DTid, r, mu, mu_s, nu, intersects_ground);
	RWDeltaJ[DTid] = float4(ComputeScatteringDensity(r, mu, mu_s, nu), 0.0);
}