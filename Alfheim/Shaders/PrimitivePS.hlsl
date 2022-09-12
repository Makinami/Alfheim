#include "PrimitiveRS.hlsli"

Texture2D<float4> foo0 : register(t0);
Texture2D<float4> foo1 : register(t1);
Texture2D<float4> foo2 : register(t2);

Texture2D<float4> OcclusionTexture : register(t3);
SamplerState OcclusionSampler : register(s3);

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 worldPos : POSITION;
	float2 texCoord : TEXCOORD0;
};

float4 main(VSOutput pin) : SV_TARGET
{
	float occlusion = OcclusionTexture.Sample(OcclusionSampler, pin.texCoord).r;
	occlusion += foo0.Sample(OcclusionSampler, pin.texCoord).r;
	occlusion += foo1.Sample(OcclusionSampler, pin.texCoord).r;
	occlusion += foo2.Sample(OcclusionSampler, pin.texCoord).r;
	return float4(occlusion.rrr, 1.0f);
}