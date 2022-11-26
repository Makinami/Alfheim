struct VSOutput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
};

static const float3 SUN_DIR = normalize(float3(0, 1, 1));

float4 main(VSOutput vsOutput) : SV_TARGET
{
	float light = dot(vsOutput.normal, SUN_DIR);
	return float4(light.xxx, 1.0f);
}