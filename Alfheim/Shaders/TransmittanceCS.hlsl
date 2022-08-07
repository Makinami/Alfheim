#include "SkyH.hlsli"

[numthreads(8, 8, 1)]
void main( int2 DTid : SV_DispatchThreadID )
{
	float r, mu;
	GetRMuFromTransmittanceTextureCoordinates(DTid, r, mu);
	RWTransmittance[DTid] = ComputeTransmittanceToTopAtmosphereBoundry(r, mu);
}