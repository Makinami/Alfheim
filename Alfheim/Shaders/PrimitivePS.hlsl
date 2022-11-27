#include "PrimitiveRS.hlsli"

[RootSignature(Primitive_RootSig)]
float4 main(float4 color: COLOR) : SV_TARGET
{
	return color;
}