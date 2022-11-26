#include "PrimitiveRS.hlsli"

struct InstanceData
{
	float4x4 worldMatrix;
	float4x4 normalMatrix;
};

struct VertexData
{
	float3 position;
	float3 normal;
};

StructuredBuffer<VertexData> VertexBuffer : register(t0);
StructuredBuffer<InstanceData> InstanceBuffer : register(t11);

cbuffer VSConstantsVP : register(b1)
{
	float4x4 viewProj;
	float3 cameraPosition;
}

cbuffer VSConstantsW : register(b0)
{
	float4x4 meshWorldMatrix;
	float4x4 meshNormalMatrix;
	int materialId;
}

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord: TEXCOORD0;
	float4 tangent :TANGENT;
	uint instanceId : SV_InstanceID;
	//float4x4 world : WORLD;
	//uint vertexId : SV_VertexID;
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

	float4x4 instanceWorldMatrix = InstanceBuffer[vin.instanceId].worldMatrix;
	float4x4 instanceNormalMatrix = InstanceBuffer[vin.instanceId].normalMatrix;
	/*float4 lPosition = float4(VertexBuffer[vin.vertexId].position, 1.0f);
	float4 lNormal = float4(VertexBuffer[vin.vertexId].normal, 0.0f);*/

	vout.worldPos = mul(meshWorldMatrix, float4(vin.position, 1.0f));
	vout.worldPos = mul(instanceWorldMatrix, float4(vout.worldPos, 1.f));
	
	vout.position = mul(viewProj, float4(vout.worldPos, 1.0f));
	
	vout.normal = mul(meshNormalMatrix, vin.normal);
	vout.normal = mul(instanceNormalMatrix, vout.normal);
	
	vout.tangent = mul(meshNormalMatrix, vin.tangent);
	vout.tangent = mul(instanceNormalMatrix, vout.tangent);
	
	vout.texCoord = vin.texCoord;

	return vout;
}