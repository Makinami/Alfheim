#include "ShaderUtility.hlsli"
#include "PresentRS.hlsli"

Texture2D<float3> ColorTex : register(t0);

cbuffer Constants : register(b0)
{
    uint2 TextureSize;
    float A;
}

float W1(float x)
{
    return x * x * ((A + 2) * x - (A + 3)) + 1.0;
}

float W2(float x)
{
    return A * (x * (x * (x - 5) + 8) - 4);
}

float4 GetWeights(float d1)
{
    return float4(W2(1.0 + d1), W1(d1), W1(1.0 - d1), W2(2.0 - d1));
}

float3 GetColor(uint s, uint t)
{
    return ColorTex[uint2(s, t)];
}

[RootSignature(Present_RootSig)]
float3 main(float4 position : SV_Position, float2 uv : TexCoord0) : SV_Target0
{
    float2 t = uv * TextureSize + 0.5;
    float2 f = frac(t);
    int2 st = int2(position.x, t.y);

    uint MaxHeight = TextureSize.y - 1;

    uint t0 = max(st.y - 2, 0);
    uint t1 = max(st.y - 1, 0);
    uint t2 = min(st.y + 0, MaxHeight);
    uint t3 = min(st.y + 1, MaxHeight);

    float4 W = GetWeights(f.y);
    float3 Color =
        W.x * GetColor(st.x, t0) +
        W.y * GetColor(st.x, t1) +
        W.z * GetColor(st.x, t2) +
        W.w * GetColor(st.x, t3);

#ifdef GAMMA_SPACE
        return Color;
#else
        return ApplyDisplayProfile(Color, DISPLAY_PLANE_FORMAT);
#endif
}
