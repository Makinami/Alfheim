cbuffer constBuffer : register(b0)
{
	float4 INVERSE_GRID_SIZE;
	float4 GRID_SIZE;
	float4x4 VIEW_PROJ;
};

Texture2D<float4> Terrain: register(t0);
SamplerState AnisotropicSampler: register(s0);

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
};

VSOutput main(float2 pos : POSITION)
{
	VSOutput vsOutput;

	float3 wPos = float3(pos.x, 0, pos.y);
	float3 terrain = Terrain.SampleLevel(AnisotropicSampler, pos * INVERSE_GRID_SIZE.x, 0).xyz;

	wPos.y += terrain.x;
	vsOutput.position = mul(VIEW_PROJ, float4(wPos, 1));
	vsOutput.normal = normalize(-float3(terrain.y, -1, terrain.z));

	return vsOutput;
}