#include "PrimitiveRS.hlsli"

Texture2D g_Textures[] : register(t0);
SamplerState g_Samplers[] : register(s0);

//Texture2D<float4> DiffuseTexture : register(t0);
//SamplerState DiffuseSampler : register(s0);

//Texture2D<float4> foo1 : register(t1);

//Texture2D<float4> NormalTexture : register(t2);
//SamplerState NormalSampler : register(s2);

//Texture2D<float4> OcclusionTexture : register(t3);
//SamplerState OcclusionSampler : register(s3);

struct Material
{
	int DiffuseTextureId;
	int DiffuseSamplerId;

	int NormalTextureId;
	int NormalSamplerId;

	int OcclusionTextureId;
	int OcclusionSamplerId;
};

StructuredBuffer<Material> g_Materials : register(t0, space1);

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
	int lightsNum;
}

cbuffer VSConstantsVP : register(b1)
{
	float4x4 viewProj;
	float3 cameraPosition;
}

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 worldPos : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 tangent :TANGENT;
};

float4 getBaseColor(VSOutput pin)
{
	return g_Textures[g_Materials[materialId].DiffuseTextureId].Sample(g_Samplers[g_Materials[materialId].DiffuseSamplerId], pin.texCoord);
}

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 ComputePointLight(SimpleLight light, float3 baseColor, float3 position, float3 normal, float3 toEye)
{
	float3 lightVec = light.Position - position;

	float d = length(lightVec);

	if (d > light.FalloffEnd)
		return 0.f;

	lightVec /= d;

	float ndotl = max(dot(lightVec, normal), 0.f);
	float3 lightStrength = light.Strength * ndotl;

	float att = CalcAttenuation(d, light.FalloffStart, light.FalloffEnd);
	lightStrength *= att;

	return baseColor * lightStrength;
	//return BlinnPhong(lightStrength, lightVec, normal, toEye);
}

float4 main(VSOutput pin) : SV_TARGET
{
	float4 baseColor = getBaseColor(pin);

	// Normal
	float3 normalT = 2.0f * g_Textures[g_Materials[materialId].NormalTextureId].Sample(g_Samplers[g_Materials[materialId].NormalSamplerId], pin.texCoord) - 1.0f;

	float3 normalW = normalize(pin.normal);
	float3 tangent = normalize(pin.tangent - dot(pin.tangent, normalW) * normalW);
	float3 bitangent = cross(normalW, tangent);

	float3x3 TBN = float3x3(tangent, bitangent, normalW);

	float3 normal = mul(normalT, TBN);

	float3 result = 0.f;

	float3 toEye = normalize(cameraPosition - pin.worldPos);

	for (int i = 0; i < lightsNum; ++i) {
		SimpleLight light = g_Lights[i];

		result += ComputePointLight(light, baseColor, pin.worldPos, normal, toEye);
	}

	//float3 base = DiffuseTexture.Sample(DiffuseSampler, pin.texCoord).rgb;
	float occlusion = g_Textures[g_Materials[materialId].OcclusionTextureId].Sample(g_Samplers[g_Materials[materialId].OcclusionSamplerId], pin.texCoord).r;
	
	return float4(result, 1.0f);
}