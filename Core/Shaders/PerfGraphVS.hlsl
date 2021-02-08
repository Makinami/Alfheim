#include "PerfGraphRS.hlsli"

cbuffer CBGraphColor : register(b0)
{
	float3 Color;
	float RcpXScale;
	uint NodeCount;
	uint FrameID;
}

cbuffer constants : register(b1)
{
	uint Instance;
	float RcpYScale;
}

struct VSOutput
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
};

StructuredBuffer<float> PerfTimes : register(t0);

[RootSignature(PerfGraph_RootSig)]
VSOutput main( uint VertexID : SV_VertexID)
{
	// Assume NodeCount is a power of 2
	uint offset = (FrameID + VertexID) & (NodeCount - 1);

	//? TODO: Stop interleacing data
	float perfTime = saturate(PerfTimes[offset] * RcpYScale) * 2.0 - 1.0;
	float frame = VertexID * RcpXScale - 1.0;

	VSOutput output;
	output.pos = float4(frame, perfTime, 1, 1);
	output.col = Color;

	return output;
}