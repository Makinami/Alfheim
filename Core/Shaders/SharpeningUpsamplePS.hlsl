#include "ShaderUtility.hlsli"
#include "PresentRS.hlsli"

Texture2D<float3> ColorTex : register(t0);
SamplerState BilinearClamp : register(s0);

cbuffer Constants : register(b0)
{
	float2 UVOffset0;
	float2 UVOffset1;
	float WA, WB;
}

float3 GetColor(float2 UV)
{
	float3 Color = ColorTex.SampleLevel(BilinearClamp, UV, 0);
#ifdef GAMMA_SPACE
	return ApplyDisplayProfile(Color, DISPLAY_PLANE_FORMAT);
#else
	return Color;
#endif
}

[RootSignature(Present_RootSig)]
float3 main(float4 position : SV_Position, float2 uv : TexCoord0) : SV_Target0
{
	float3 Color = WB * GetColor(uv) - WA * (
		GetColor(uv + UVOffset0) + GetColor(uv - UVOffset0) +
		GetColor(uv + UVOffset1) + GetColor(uv - UVOffset1));

#ifdef GAMMA_SPACE
	return Color;
#else
	return ApplyDisplayProfile(Color, DISPLAY_PLANE_FORMAT);
#endif
}
