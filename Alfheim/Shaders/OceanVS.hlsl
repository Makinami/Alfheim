cbuffer constBuffer : register(b0)
{
	float4 INVERSE_GRID_SIZE;
	float4 GRID_SIZE;
	float4x4 VIEW_PROJ;
};

Texture2DArray<float4> Waves : register(t0);
SamplerState AnisotropicSampler: register(s0);

float4 main(float2 pos : POSITION) : SV_POSITION
{
	float3 wPos = float3(pos.x, 0, pos.y);
	wPos += Waves.SampleLevel(AnisotropicSampler, float3(pos * INVERSE_GRID_SIZE.x, 0), 0);
	wPos += Waves.SampleLevel(AnisotropicSampler, float3(pos * INVERSE_GRID_SIZE.y, 1), 0);
	wPos += Waves.SampleLevel(AnisotropicSampler, float3(pos * INVERSE_GRID_SIZE.z, 2), 0);
	wPos += Waves.SampleLevel(AnisotropicSampler, float3(pos * INVERSE_GRID_SIZE.w, 3), 0);
	return mul(VIEW_PROJ, float4(wPos, 1));
}