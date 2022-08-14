#include "SkyH.hlsli"

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float r, mu_s;
	GetIrradianceAltSzAngle(DTid.xy, r, mu_s);

	const float dphi = PI / IRRADIANCE_INTEGRAL_SAMPLES;
	const float dtheta = PI / IRRADIANCE_INTEGRAL_SAMPLES;

	float3 result = float3(0.0, 0.0, 0.0);
	float3 omega_s = float3(sqrt(1 - mu_s * mu_s), 0.0, mu_s);
	for (int j = 0; j < IRRADIANCE_INTEGRAL_SAMPLES / 2; ++j) {
		float theta = (j + 0.5) * dtheta;
		for (int i = 0; i < 2 * IRRADIANCE_INTEGRAL_SAMPLES; ++i) {
			float phi = (i + 0.5) * dtheta;
			float3 omega = float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
			float domega = dtheta * dphi * sin(theta);

			float nu = dot(omega, omega_s);
			result += GetScattering(ROInscatter, r, omega.z, mu_s, nu, false, Order).rgb * omega.z * domega;
		}
	}

	float3 foo = RWIrradiance[DTid.xy];

	RWDeltaIrradiance[DTid.xy] = result;
	RWIrradiance[DTid.xy] += result;
}