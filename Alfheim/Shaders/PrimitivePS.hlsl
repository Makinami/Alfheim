#include "PrimitiveRS.hlsli"

[RootSignature(Primitive_RootSig)]
float4 main() : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}