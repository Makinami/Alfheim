RWTexture2D<float3> RWTransmittance : register(u0);
RWTexture2D<float3> RWIrradiance : register(u1);
RWTexture2D<float3> RWDeltaIrradiance : register(u2);
RWTexture3D<float4> RWInscatter : register(u3);
RWTexture3D<float4> RWDeltaInscatter : register(u4);
RWTexture3D<float4> RWDeltaJ : register(u5);

Texture2D<float3> ROTransmittance : register(t0);
Texture2D<float4> ROIrradiance : register(t1);
Texture2D<float3> RODeltaIrradiance : register(t2);
Texture3D<float4> ROInscatter : register(t3);
Texture3D<float4> RODeltaInscatter : register(t4);
Texture3D<float4> RODeltaJ : register(t5);

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

cbuffer OrderConstant : register(b1)
{
    int Order;
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
static const float3 GroundAlbedo = 0.1;
static const float MiePhaseFunctionG = 0.2;

/*
    Numerical integration parameters
*/

static const int TRANSMITTANCE_INTEGRAL_SAMPLES = 500;
static const int INSCATTER_INTEGRAL_SAMPLES = 50;
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

void GetIrradianceAltSzAngle(int2 pos, out float alt, out float szAngle)
{
    alt = BottomRadius + pos.y / (SkyRes.y - 1.f) * (TopRadius - BottomRadius);
    szAngle = -0.2f + pos.x / (SkyRes.x - 1.f) * 1.2f;
}

float2 GetIrradianceTextureUvFromAltSzAngle(float alt, float szAngle)
{
    float x_r = (alt - BottomRadius) / (TopRadius - BottomRadius);
    float x_mu_s = szAngle * 0.5 + 0.5;
    return float2(x_mu_s, x_r);
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

float4 GetScatteringTextureUvwzFromRMuMuSNu(float r, float mu, float mu_s, float nu, bool intersects_ground)
{
    // Distance to top atmosphere for a horizontal ray at ground level.
    float H = sqrt(TopRadius * TopRadius - BottomRadius * BottomRadius);
    // Distance to the horizon.
    float rho = sqrt(max(0, r * r - BottomRadius * BottomRadius));
    float u_r = TextureCoordinatesFromUnitRange(rho / H, ScatteringRes.x);

    // Distance of the quadratic equation for the intersections of the ray
    // (r, mu) with the ground.
    float r_mu = r * mu;
    float discriminant = r_mu * r_mu - r * r + BottomRadius * BottomRadius;
    float u_mu;
    if (intersects_ground) {
        // Distance to the ground for the ray (r,mu), and its minimum and maximum
        // values over all mu - obtained for (r,-1) and (r,mu_horizon).
        float d = -r_mu - sqrt(max(0, discriminant));
        float d_min = r - BottomRadius;
        float d_max = rho;
        u_mu = 0.5 - 0.5 * TextureCoordinatesFromUnitRange(d_max == d_min ? 0.0 :
            (d - d_min) / (d_max - d_min), ScatteringRes.y / 2);
    }
    else {
        // Distance to the top atmosphere boundary for the ray (r, mu), and its
        // minimum and maximum values over all mu - obtained for (r,1) and
        // (r,mu_horizon).
        float d = -r_mu + sqrt(max(0, discriminant + H * H));
        float d_min = TopRadius - r;
        float d_max = rho + H;
        u_mu = 0.5 + 0.5 * TextureCoordinatesFromUnitRange((d - d_min) / (d_max - d_min), ScatteringRes.y / 2);
    }

    float d = DistanceToTopAtmosphereBoundary(BottomRadius, mu_s);
    float d_min = TopRadius - BottomRadius;
    float d_max = H;
    float a = (d - d_min) / (d_max - d_min);
    float D = DistanceToTopAtmosphereBoundary(BottomRadius, MinimumMuS);
    float A = (D - d_min) / (d_max - d_min);
    // An ad-hoc function equal to 0 for mu_s = MinimumMuS (because then d = D and
    // thus a = A), euqal to 1 for mu_s = (because then d = d_min and thus a = 0),
    // and with a large slope around mu_s = 0, to get more texture samples near the horizon.
    float u_mu_s = TextureCoordinatesFromUnitRange(max(1.0 - a / A, 0.0) / (1.0 + a), ScatteringRes.z);

    float u_nu = (nu + 1.0) / 2.0;
    return float4(u_nu, u_mu_s, u_mu, u_r);

}

/*
    Utility functions
*/

float RayleighPhaseFunction(float nu) {
    float k = 3.0 / (16.0 * PI);
    return k * (1.0 + nu * nu);
}

float MiePhaseFunction(float nu) {
    float k = 3.0 / (8.0 * PI) * (1.0 - MiePhaseFunctionG * MiePhaseFunctionG) / (2.0 + MiePhaseFunctionG * MiePhaseFunctionG);
    return k * (1.0 + nu * nu) / pow(1.0 + MiePhaseFunctionG * MiePhaseFunctionG - 2.0 * MiePhaseFunctionG * nu, 1.5);
}

float DistanceToNearestAtmosphereBoundary(float r, float mu, bool intersects_ground) {
    if (intersects_ground) {
        return DistanceToBottomAtmosphereBoundary(r, mu);
    }
    else {
        return DistanceToTopAtmosphereBoundary(r, mu);
    }
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

bool RayIntersectsGround(float r, float mu)
{
    return mu < 0.0 && r * r * (mu * mu - 1.0) + BottomRadius * BottomRadius >= 0.0;
}

float3 GetIrradiance(float r, float mu_s) {
    float2 uv = GetIrradianceTextureUvFromAltSzAngle(r, mu_s);
    return ROIrradiance.SampleLevel(LinearClamp, uv, 0).rgb;
}

float4 GetScattering(Texture3D<float4> scattering, float r, float mu, float mu_s, float nu, bool intersects_ground)
{
    float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(r, mu, mu_s, nu, intersects_ground);
    float tex_coord_x = uvwz.x * (ScatteringRes.w - 1);
    float tex_x = floor(tex_coord_x);
    float lerp = tex_coord_x - tex_x;
    float3 uvw0 = float3((tex_x + uvwz.y) / ScatteringRes.w, uvwz.z, uvwz.w);
    float3 uvw1 = float3((tex_x + 1 + uvwz.y) / ScatteringRes.w, uvwz.z, uvwz.w);
    return scattering.SampleLevel(LinearClamp, uvw0, 0) * (1.0 - lerp) + scattering.SampleLevel(LinearClamp, uvw1, 0) * lerp;
}

float4 GetScattering(Texture3D<float4> scaterring, float r, float mu, float mu_s, float nu, bool intersects_ground, int order)
{
    if (order == 1) {
        float4 rayleigh_mie = GetScattering(scaterring, r, mu, mu_s, nu, intersects_ground);
        return float4(rayleigh_mie.xyz * RayleighPhaseFunction(nu) + rayleigh_mie.w * MiePhaseFunction(nu), 0.0);
    }
    else {
        return GetScattering(scaterring, r, mu, mu_s, nu, intersects_ground);
    }
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