#include "PerfGraphRS.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

[RootSignature(PerfGraph_RootSig)]
float4 main(VSOutput input) : SV_TARGET
{
	return float4(input.col, 0.75);
}