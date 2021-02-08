#include "PerfGraphRS.hlsli"

struct VSOutput
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
};

cbuffer CB1 : register(b1)
{
	float RecSize; //? is it necessary?
}

[RootSignature(PerfGraph_RootSig)]
VSOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	//VSOutput Output;
	//float2 uv = float2( (vertexID >> 1) & 1, vertexID & 1 );
	//float2 Corner = lerp( float2(-1.0f, 1.0f), float2(1.0f, RecSize), uv );
	//Corner.y -= 0.45f * instanceID;
	//Output.pos = float4(Corner.xy, 1.0,1);
	//Output.col = float3(0.0, 0.0, 0.0);
	//return Output;

	VSOutput Output;
	float2 uv = float2((vertexID >> 1) & 1, vertexID & 1);
	float2 Corner = lerp(float2(-1.0f, 1.0f), float2(1.0f, -1.0f), uv);
	Output.pos = float4(Corner.xy, 1.0f, 1.0f);
	Output.col = float3(0.0f, 0.0f, 0.0f);
	return Output;
}