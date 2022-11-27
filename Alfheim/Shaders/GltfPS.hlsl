#include "GltfRS.hlsli"

Texture2D g_Textures[] : register(t0);
SamplerState g_Samplers[] : register(s0);

//Texture2D<float4> DiffuseTexture : register(t0);
//SamplerState DiffuseSampler : register(s0);

//Texture2D<float4> foo1 : register(t1);

//Texture2D<float4> NormalTexture : register(t2);
//SamplerState NormalSampler : register(s2);

//Texture2D<float4> OcclusionTexture : register(t3);
//SamplerState OcclusionSampler : register(s3);

#define SPECULARGLOSSINESS_FLAG (1 << 0)
#define METALLICROUGHNESS_FLAG  (1 << 1)

static const float PI = 3.14159265f;

struct SpectralGlossinessProperties
{
	float4 DiffuseFactor;
	uint DiffuseTexture;

	float3 SpecularFactor;
	float GlossinessFactor;
	uint SpecularGlossinessTexture;
};

struct Material
{
	int _DiffuseTextureId;
	int _DiffuseSamplerId;

	int NormalTextureId;
	int NormalSamplerId;

	int OcclusionTextureId;
	int OcclusionSamplerId;
	
	float4 BaseColorFactor;
	
	uint Flags;
	float3 pad;
	
	SpectralGlossinessProperties SpectralGlossiness;
};

StructuredBuffer<Material> g_Materials : register(t0, space1);

float4 SampleTexture(int texture, float2 pos)
{
	return g_Textures[texture & 0xff].Sample(g_Samplers[texture >> 16], pos);
}

struct MaterialInfo
{
	float3 baseColor;
	float3 f0;
	float3 f90;
	float perceptualRoughness;
	float3 c_diff;
	float specularWeight;
	float alphaRoughness;
};

struct SimpleLight
{
	float3 Strength;
	float FalloffStart;
	float3 Direction;
	float FalloffEnd;
	float3 Position;
	float SpotPower;
};

StructuredBuffer<SimpleLight> g_Lights : register(t1, space1);

cbuffer VSConstantsW : register(b0)
{
	float4x4 worldMatrix;
	float4x4 normalMatrix;
	int materialId;
}

cbuffer VSConstantsVP : register(b1)
{
	float4x4 viewProj;
	float3 cameraPosition;
	int lightsNum;
}

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 worldPos : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 tangent : TANGENT;
};

float4 getBaseColor(VSOutput pin, Material material)
{
	if (material.Flags & SPECULARGLOSSINESS_FLAG && material.SpectralGlossiness.DiffuseTexture != -1)
	{
		return material.SpectralGlossiness.DiffuseFactor * SampleTexture(material.SpectralGlossiness.DiffuseTexture, pin.texCoord);
	}
	else if (material.Flags & METALLICROUGHNESS_FLAG)
	{
		
	}
	else
	{
		return 1.f;
	}

	return 1.f / 0.f;
}

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 ComputePointLightIntensity(SimpleLight light, float3 position, float3 normal)
{
	float3 lightVec = light.Position - position;

	const float d = length(lightVec);

	if (d > light.FalloffEnd)
		return 0.f;

	lightVec /= d;

	const float att = CalcAttenuation(d, light.FalloffStart, light.FalloffEnd);
	const float ndotl = max(dot(lightVec, normal), 0.f);

	return light.Strength * ndotl * att;
}

MaterialInfo getSpecularGlossinessInfo(Material material, MaterialInfo info, VSOutput pin)
{
	info.f0 = material.SpectralGlossiness.SpecularFactor;
	info.perceptualRoughness = material.SpectralGlossiness.GlossinessFactor;

	if (material.SpectralGlossiness.SpecularGlossinessTexture != -1)
	{
		float4 val = SampleTexture(material.SpectralGlossiness.SpecularGlossinessTexture, pin.texCoord);
		info.perceptualRoughness *= val.a;
		info.f0 *= val.rgb;
	}

	info.perceptualRoughness = 1.0 - info.perceptualRoughness;
	info.c_diff = info.baseColor * (1.0 - max(max(info.f0.r, info.f0.g), info.f0.b));
	return info;
}

float3 F_Schlick(float3 f0, float f90, float VdotH)
{
	float x = saturate(1.f - VdotH);
	float x2 = x * x;
	float x5 = x * x2 * x2;
	return f0 + (f90 - f0) * x5;
}

float3 BRDF_lambertian(float3 f0, float3 f90, float3 diffuseColor, float specularWeight, float VdotH)
{
	// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
	return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / PI);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GXX(float NdotL, float NdotV, float alphaRoughness)
{
	const float alphaRoughnessSq = alphaRoughness * alphaRoughness;

	const float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
	const float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

	const float GGX = GGXL + GGXV;
	if (GGX > 0.f)
	{
		return 0.5f / GGX;
	}
	return 0.f;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
	float alphaRoughnessSq = alphaRoughness * alphaRoughness;
	float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
	// breaks when f == 0 (viewing and light vectors equal to normal and perfectly smooth
	return alphaRoughnessSq / (PI * f * f);
}

float3 BRDF_specularGGX(float3 f0, float3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
	const float3 F = F_Schlick(f0, f90, VdotH);
	const float Vis = V_GXX(NdotL, NdotV, alphaRoughness);
	const float D = D_GGX(NdotH, alphaRoughness);

	return specularWeight * F * Vis * D;
}

float4 main(VSOutput pin) : SV_TARGET
{
	float3 v = normalize(cameraPosition - pin.worldPos);

	Material material = g_Materials[materialId];

	float4 baseColor = getBaseColor(pin, material);

	// Normal
	float3 normalT = 2.0f * g_Textures[material.NormalTextureId].Sample(g_Samplers[material.NormalSamplerId], pin.texCoord) - 1.0f;

	float3 normalW = normalize(pin.normal);
	float3 tangent = normalize(pin.tangent - dot(pin.tangent, normalW) * normalW);
	float3 bitangent = cross(normalW, tangent);

	float3x3 TBN = float3x3(tangent, bitangent, normalW);

	float3 normal = mul(normalT, TBN);

	MaterialInfo materialInfo;
	materialInfo.baseColor = baseColor.rgb;
	materialInfo.specularWeight = 1.f;

	materialInfo = getSpecularGlossinessInfo(material, materialInfo, pin);

	materialInfo.perceptualRoughness = saturate(materialInfo.perceptualRoughness);

	// Roughness is authored as perceptual roughness; as is convention,
	// convert to material roughness by squaring the perceptual roughness.
	materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

	materialInfo.f90 = 1.f;

	float3 f_specular = 0.f;
	float3 f_diffuse = 0.f;

	float ao = 1.0;

	if (material.OcclusionTextureId != -1)
	{
		// completely unnecessary if doesn't use image based lightning?
		ao = g_Textures[material.OcclusionTextureId].Sample(g_Samplers[material.OcclusionSamplerId], pin.texCoord);
		float OcclusionStrength = 1.0;
		f_diffuse = lerp(f_diffuse, f_diffuse * ao, OcclusionStrength);
		f_specular = lerp(f_specular, f_specular * ao, OcclusionStrength);
	}


	float3 result = 0.f;

	float3 toEye = normalize(cameraPosition - pin.worldPos);

	for (int i = 0; i < lightsNum; ++i)
	{
		const SimpleLight light = g_Lights[i];

		float3 pointToLight;
		if (any(light.Position))
		{
			pointToLight = light.Position - pin.worldPos;
		}
		else
		{
			pointToLight = -light.Direction;
		}

		const float3 l = normalize(pointToLight);
		const float3 h = normalize(v + l);
		const float NdotL = saturate(dot(normal, l));
		const float VdotH = saturate(dot(v, h));
		const float NdotV = saturate(dot(normal, v));
		const float NdotH = saturate(dot(normal, h));

		if (VdotH > 0 || NdotL > 0)
		{
			float3 intensity = ComputePointLightIntensity(light, pin.worldPos, normal);

			f_diffuse += intensity * NdotL * BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.c_diff, materialInfo.specularWeight, VdotH);
			f_specular += intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, materialInfo.specularWeight, VdotH, NdotL, NdotV, NdotH);
		}

	}

	float3 color = 0.f;

	color = f_diffuse + f_specular;

	//float3 base = DiffuseTexture.Sample(DiffuseSampler, pin.texCoord).rgb;
	float occlusion = g_Textures[g_Materials[materialId].OcclusionTextureId].Sample(g_Samplers[g_Materials[materialId].OcclusionSamplerId], pin.texCoord).r;
	
	return float4(color, 1.0f);
}
