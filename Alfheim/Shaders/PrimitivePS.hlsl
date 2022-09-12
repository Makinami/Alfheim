#include "PrimitiveRS.hlsli"

Texture2D<float4> DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);

Texture2D<float4> foo1 : register(t1);

Texture2D<float4> NormalTexture : register(t2);
SamplerState NormalSampler : register(s2);

Texture2D<float4> OcclusionTexture : register(t3);
SamplerState OcclusionSampler : register(s3);

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 worldPos : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 tangent :TANGENT;
};

float4 main(VSOutput pin) : SV_TARGET
{
	float3 normalT = 2.0f * NormalTexture.Sample(NormalSampler, pin.texCoord) - 1.0f;

	float3 normalW = normalize(pin.normal);
	float3 tangent = normalize(pin.tangent - dot(pin.tangent, normalW) * normalW);
	float3 bitangent = cross(normalW, tangent);

	float3x3 TBN = float3x3(tangent, bitangent, normalW);

	float3 normal = mul(normalT, TBN);

	float3 base = DiffuseTexture.Sample(DiffuseSampler, pin.texCoord).rgb;
	float occlusion = OcclusionTexture.Sample(OcclusionSampler, pin.texCoord).r;
	
	return float4(normal, 1.0f);
}