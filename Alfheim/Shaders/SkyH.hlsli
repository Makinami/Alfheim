RWTexture2D<float3> RWTransmittance : register(u0);
RWTexture2D<float3> RWDeltaIrradiance : register(u1);
RWTexture3D<float4> RWDeltaInscatter : register(u2);

Texture2D<float3> ROTransmittance : register(t0);

SamplerState LinearClamp : register(s0);

cbuffer SkyParameters : register(b0)
{
    float2 TransmittanceRes;
    float2 SkyRes;
    float4 ScatteringRes; // R, MU, MU_S, NU

    float3 RayleighScattering;
    float RayleighDensity;

    float3 MieExtinction;
    float MieDensity;

    //float3 AbsorptionScattering;
    //float AbsorptionDensity;

    float TopRadius;
    float BottomRadius;
}

/*
    To be moved to the program
*/

// The cosine of the maximum Sun zenith angle for which atmospheric scattering
// must be precomputed (for maximum precision, use the smallest Sun zenith
// angle yielding negligible sky light radiance values. For instance, for the
// Earth case, 102 degrees is a good choice - yielding mu_s_min = -0.2).
static const float MinimumMuS = -0.2;

static const float3 SolarIrradiance = float3(1.5, 1.5, 1.5); // for now (see the reference)
static const float SunAngularRadius = 0.2678;
static const float3 MieScattering = float3(4e-3, 4e-3, 4e-3);

/*
    Numerical integration parameters
*/

static const int TRANSMITTANCE_INTEGRAL_SAMPLES = 500;
static const int INSCATTER_INTEGRAL_SAMPLES = 5;
static const int IRRADIANCE_INTEGRAL_SAMPLES = 32;
static const int INSCATTER_SPHERICAL_INTEGRAL_SAMPLES = 16;

static const float PI = 3.141592657f;

/*
    Parametrization options
*/

#define TRANSMITTANCE_NON_LINEAR
#define INSCATTER_NON_LINEAR

/*
    Parametrization functions
*/

float DistanceToTopAtmosphereBoundary(float r, float mu)
{
    float discriminant = max(0, r * r * (mu * mu - 1) + TopRadius * TopRadius);
    return max(0, -r * mu + sqrt(discriminant));
}

float DistanceToBottomAtmosphereBoundary(float r, float mu)
{
    float discriminant = max(0, r * r * (mu * mu - 1.0) + BottomRadius * BottomRadius);
    return max(0, -r * mu - sqrt(discriminant));
}

float TextureCoordinatesFromUnitRange(float x, int textureSize)
{
    return 0.5 / textureSize + x * (1.0 - 1.0 / textureSize);
}

float UnitRangeFromTextureCoordinates(float u, int textureSize)
{
    return (u - 0.5 / textureSize) / (1.0 - 1.0 / textureSize);
}

void GetRMuFromTransmittanceTextureCoordinates(int2 pos, out float r, out float mu)
{
    r = pos.y / (TransmittanceRes.y - 1);
    mu = pos.x / (TransmittanceRes.x - 1);
#ifdef TRANSMITTANCE_NON_LINEAR
    r = BottomRadius + (r * r) * (TopRadius - BottomRadius);
    mu = -0.15 + tan(1.5 * mu) / tan(1.5) * 1.15;
#else
    r = BottomRadius + r * (TopRadius - BottomRadius);
    mu = -0.15 + mu * (1.0 + 0.15);
#endif
}

float2 GetTransmittanceUV(float alt, float vzAngle)
{
    float2 foo = float2(
#ifdef TRANSMITTANCE_NON_LINEAR                                
        atan((vzAngle + 0.15) / 1.15 * tan(1.5)) / 1.5,
        sqrt((alt - BottomRadius) / (TopRadius - BottomRadius))
#else   
        (vzAngle + 0.15) / 1.15,
        (alt - BottomRadius) / (TopRadius - BottomRadius)
#endif
        );
    return foo;
}

void GetIrradianceAltSzAngle(float2 pos, out float alt, out float szAngle)
{
    alt = BottomRadius + pos.y / (SkyRes.y - 1.f) * (TopRadius - BottomRadius);
    szAngle = -0.2f + pos.x / (SkyRes.x - 1.f) * 1.2f;
}

void GetRMuMuSNuFromScatteringTextureUvwz(float4 pos, out float r, out float mu, out float mu_s, out float nu, out bool intersects_ground)
{
    // Distance to top atmosphere boundary for a horizontal ray at ground level.
    float H = sqrt(TopRadius * TopRadius - BottomRadius * BottomRadius);
    // Distance to the horizon
    float rho = H * pos.x;
    r = sqrt(rho * rho + BottomRadius * BottomRadius);

    if (pos.y < 0.5)
    {
        // Distance to the ground for the ray (r, mu), and its minimum and maximum
        // values over all mu - obtained for (r, -1) and (r, mu_horizon) - from which
        // we can recover mu:
        float d_min = r - BottomRadius;
        float d_max = rho;
        float d = d_min + (d_max - d_min) * (1.0 - 2.0 * pos.y);
        mu = d == 0.0 ? -1.0 : clamp(-(rho * rho + d * d) / (2.0 * r * d), -1.0, 1.0);
        intersects_ground = true;
    }
    else
    {
        // Distance to the top atmosphere boundry for the ray (r, mu), and its
        // minimum and maximum values over all mu - obtained for (r, 1) and
        // (r, mu_horizon) - from which we can recover mu:
        float d_min = TopRadius - r;
        float d_max = rho + H;
        float d = d_min + (d_max - d_min) * (2.0 * pos.y - 1.0);
        mu = d == 0.0 ? 1.0 : clamp((H * H - rho * rho - d * d) / (2.0 * r * d), -1, 1);
        intersects_ground = false;
    }

    float x_mu_s = pos.z;
    float d_min = TopRadius - BottomRadius;
    float d_max = H;
    float D = DistanceToTopAtmosphereBoundary(BottomRadius, MinimumMuS);
    float A = (D - d_min) / (d_max - d_min);
    float a = (A - x_mu_s * A) / (1.0 + x_mu_s * A);
    float d = d_min + min(a, A) * (d_max - d_min);
    mu_s = d == 0.0 ? 1.0 : clamp((H * H - d * d) / (2.0 * BottomRadius * d), -1.0, 1.0);
    nu = clamp(pos.w * 2.0 - 1.0, -1.0, 1.0);
}

void GetRMuMuSNuFromScatteringTextureCoordinates(int3 pos, out float r, out float mu, out float mu_s, out float nu, out bool intersets_ground)
{
    float pos_nu = floor(pos.x / ScatteringRes.z);
    float pos_mu_s = fmod(pos.x, ScatteringRes.z);
    float4 uvwz = float4(pos.z, pos.y, pos_mu_s, pos_nu) / (ScatteringRes - 1);
    GetRMuMuSNuFromScatteringTextureUvwz(uvwz, r, mu, mu_s, nu, intersets_ground);
    float dSqrt = sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s));
    nu = clamp(nu, mu * mu_s - dSqrt, mu * mu_s + dSqrt);
}

/*
    Utility functions
*/

// Ground or top
// mu = cos(ray zenith angle at ray origin)
float DistanceToAtmosphereBoundary(float r, float mu)
{
    float dout = -r * mu + sqrt(max(0, r * r * (mu * mu - 1) + TopRadius * TopRadius));
    float delta2 = r * r * (mu * mu - 1.0) + BottomRadius * BottomRadius;
    if (delta2 >= 0.0)
    {
        float din = -r * mu - sqrt(delta2);
        if (din >= 0.0)
        {
            dout = min(dout, din);
        }
    }
    return dout;
}

float3 GetTransmittanceToTopAtmosphereBoundary(float alt, float vzAngle)
{
    return ROTransmittance.SampleLevel(LinearClamp, GetTransmittanceUV(alt, vzAngle), 0);
}

float3 GetTransmittanceToSun(float r, float mu_s)
{
    float sin_theta_h = BottomRadius / r;
    float cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
    return GetTransmittanceToTopAtmosphereBoundary(r, mu_s) *
        smoothstep(-sin_theta_h * SunAngularRadius, sin_theta_h * SunAngularRadius, mu_s - cos_theta_h);
}

float3 GetTransmittance(float r, float mu, float d, bool intersects_ground)
{
    float r_d = max(0, sqrt(d * d + 2.0 * r * mu * d + r * r));
    float mu_d = clamp((r * mu + d) / r_d, -1, 1);

    if (intersects_ground)
        return min(GetTransmittanceToTopAtmosphereBoundary(r_d, -mu_d) / GetTransmittanceToTopAtmosphereBoundary(r, -mu), 1);
    else
        return min(GetTransmittanceToTopAtmosphereBoundary(r, mu) / GetTransmittanceToTopAtmosphereBoundary(r_d, mu_d), 1);
}

/*
    Computation functions
*/

// Get actual density on the height r from ground
float GetDensity(float falloff, float r)
{
    return exp(-(r - BottomRadius) / falloff);
}

float ComputeOpticalLengthToTopAtmosphereBoundry(float density, float alt, float vzAngle)
{
    // leave early if below the horizon. TODO: check is it faster
    //if (vzAngle < -sqrt(1.0f - ((BottomRadius * BottomRadius) / (alt * alt))))
        //return 1e9;

    float result = 0.f;
    // Integration step
    float dx = DistanceToTopAtmosphereBoundary(alt, vzAngle) / TRANSMITTANCE_INTEGRAL_SAMPLES;

    float prev = GetDensity(density, alt);
    for (int i = 1; i <= TRANSMITTANCE_INTEGRAL_SAMPLES; ++i)
    {
        float xj = i * dx;
        float curr = GetDensity(density, sqrt(alt * alt + xj * xj + 2.f * xj * alt * vzAngle));
        result += (prev + curr) * 0.5f * dx;
        prev = curr;
    }
    return result;
}

float3 ComputeTransmittanceToTopAtmosphereBoundry(float r, float mu)
{
    return exp(-(
        RayleighScattering * ComputeOpticalLengthToTopAtmosphereBoundry(RayleighDensity, r, mu) +
        MieExtinction * ComputeOpticalLengthToTopAtmosphereBoundry(MieDensity, r, mu)
        //AbsorptionScattering * ComputeOpticalLengthToTopAtmosphereBoundry(AbsorptionDensity, r, mu)
        ));
}