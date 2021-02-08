#include "TextRS.hlsli"

cbuffer cbFontParams : register(b0)
{
    float4 Color;
    float2 ShadowOffset;
    float ShadowHardness;
    float ShadowOpacity;
    float HeightRange;    // The range of the signed distance field.
}

Texture2D<float> SignedDistanceFieldTex : register(t0);
SamplerState LinearSampler : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float GetAlpha(float2 uv, float range)
{
    return saturate(SignedDistanceFieldTex.Sample(LinearSampler, uv) * range + 0.5);
}

[RootSignature(Text_RootSig)]
float4 main(PS_INPUT Input) : SV_TARGET
{
    float alpha1 = GetAlpha(Input.uv, HeightRange) * Color.a;
    float alpha2 = GetAlpha(Input.uv - ShadowOffset, HeightRange * ShadowHardness) * ShadowOpacity * Color.a;
	return float4(Color.rgb * alpha1, lerp(alpha2, 1, alpha1));
}