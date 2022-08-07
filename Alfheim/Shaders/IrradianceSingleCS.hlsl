#include "SkyH.hlsli"

[numthreads(8, 8, 1)]
void main(int2 DTid : SV_DispatchThreadID)
{
    float alt, szAngle;
    GetIrradianceAltSzAngle(DTid, alt, szAngle);
    RWDeltaIrradiance[DTid] = GetTransmittanceToTopAtmosphereBoundary(alt, szAngle) * saturate(szAngle);
}