#include "PrimitiveRS.hlsli"

struct InstanceData
{
	float4 color;
	float4x4 world;
};

struct VertexData
{
	float3 position;
};

StructuredBuffer<VertexData> VertexBuffer : register(t0);
StructuredBuffer<InstanceData> InstanceBuffer : register(t1);

cbuffer VSConstants : register(b0)
{
	float4x4 viewProj;
}

struct VSInput
{
	float3 position : POSITION;
	float4 color : COLOR;
	float4x4 world : WORLD;
	//uint vertexId : SV_VertexID;
	//uint instanceId : SV_InstanceID;
};

struct VSOutput
{
	float4 color : COLOR;
	float4 position : SV_POSITION;
	float3 worldPos : POSITION;
};

[RootSignature(Primitive_RootSig)]
VSOutput main(VSInput vin)
{
	VSOutput vout;

	/*float4x4 lWorld = InstanceBuffer[vin.instanceId].world;
	float4 lPosition = float4(VertexBuffer[vin.vertexId].position, 1.0f);
	float4 lNormal = float4(VertexBuffer[vin.vertexId].normal, 0.0f);*/

	vout.worldPos = mul(vin.world, float4(vin.position, 1.0f));
	vout.position = mul(viewProj, float4(vout.worldPos, 1.0f));
	vout.color = vin.color;

	return vout;
}