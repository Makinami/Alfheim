#include "PrimitiveRS.hlsli"

struct InstanceData
{
	float4x4 world;
};

struct VertexData
{
	float3 position;
	float3 normal;
};

StructuredBuffer<VertexData> VertexBuffer : register(t0);
StructuredBuffer<InstanceData> InstanceBuffer : register(t1);

cbuffer VSConstantsVP : register(b1)
{
	float4x4 viewProj;
	float3 cameraPosition;
}

cbuffer VSConstantsW : register(b0)
{
	float4x4 worldMatrix;
	float4x4 normalMatrix;
	int materialId;
}

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord: TEXCOORD0;
	float4 tangent :TANGENT;
	//float4x4 world : WORLD;
	//uint vertexId : SV_VertexID;
	//uint instanceId : SV_InstanceID;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 worldPos : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 tangent :TANGENT;
};

VSOutput main(VSInput vin)
{
	VSOutput vout;

	/*float4x4 lWorld = InstanceBuffer[vin.instanceId].world;
	float4 lPosition = float4(VertexBuffer[vin.vertexId].position, 1.0f);
	float4 lNormal = float4(VertexBuffer[vin.vertexId].normal, 0.0f);*/

	vout.worldPos = mul(worldMatrix, float4(vin.position, 1.0f));
	vout.position = mul(viewProj, float4(vout.worldPos, 1.0f));
	vout.normal = mul(normalMatrix, float4(vin.normal, 0.0f));
	vout.tangent = mul(normalMatrix, vin.tangent);
	vout.texCoord = vin.texCoord;

	return vout;
}