cbuffer constBuffer : register(b0)
{
	float4 INVERSE_GRID_SIZE;
	float4 GRID_SIZE;
	float4 MIN_K;
	int2 RESOLUTION;
	float2 WIND;
	float G;
	float DENSITY;
	float SURFACE_TENSION;
	float TIME;
	float LAMBDA_J;
};

Texture2DArray<float> spectrumTex : register(t0);
Texture2DArray<float> phaseTex : register(t1);

RWTexture2DArray<float4> fftWaves : register(u0);

static const float KM = sqrt(DENSITY * G / SURFACE_TENSION); // 370.0

float2 getSpectrum(float k, float2 s0, float2 s0c)
{
	float w = sqrt(G * k * (1.0 + k * k / (KM * KM)));
	float c = cos(w * TIME);
	float s = sin(w * TIME);
	return float2((s0.x + s0c.x) * c - (s0.y + s0c.y) * s, (s0.x - s0c.x) * s + (s0.y - s0c.y) * c);
}

float2 i(float2 z) // i * z (complex)
{
	return float2(-z.y, z.x);
}

float2 sampleSpectrum(uint3 coordinates)
{
	float phase = phaseTex[coordinates];
	float2 phaseVector;
	sincos(phase, phaseVector.x, phaseVector.y);

	float spec = spectrumTex[coordinates];

	return spec * phaseVector;
}

[numthreads(8, 8, 1)]
void main(int3 DTid : SV_DispatchThreadID)
{
	float x = DTid.x >= RESOLUTION.x >> 1 ? DTid.x - RESOLUTION.x : DTid.x;
	float y = DTid.y >= RESOLUTION.y >> 1 ? DTid.y - RESOLUTION.y : DTid.y;

	float4 spec1 = float4(sampleSpectrum(uint3(DTid.xy, 0)), sampleSpectrum(uint3(RESOLUTION - DTid.xy, 0)));
	float4 spec2 = float4(sampleSpectrum(uint3(DTid.xy, 1)), sampleSpectrum(uint3(RESOLUTION - DTid.xy, 1)));
	float4 spec3 = float4(sampleSpectrum(uint3(DTid.xy, 2)), sampleSpectrum(uint3(RESOLUTION - DTid.xy, 2)));
	float4 spec4 = float4(sampleSpectrum(uint3(DTid.xy, 3)), sampleSpectrum(uint3(RESOLUTION - DTid.xy, 3)));

	float2 k1 = float2(x, y) * INVERSE_GRID_SIZE.x;
	float2 k2 = float2(x, y) * INVERSE_GRID_SIZE.y;
	float2 k3 = float2(x, y) * INVERSE_GRID_SIZE.z;
	float2 k4 = float2(x, y) * INVERSE_GRID_SIZE.w;

	float K1 = length(k1);
	float K2 = length(k2);
	float K3 = length(k3);
	float K4 = length(k4);

	float IK1 = K1 == 0.0 ? 0.0 : 1.0 / K1;
	float IK2 = K2 == 0.0 ? 0.0 : 1.0 / K2;
	float IK3 = K3 == 0.0 ? 0.0 : 1.0 / K3;
	float IK4 = K4 == 0.0 ? 0.0 : 1.0 / K4;

	float2 h1 = getSpectrum(K1, spec1.xy, spec1.zw);
	float2 h2 = getSpectrum(K2, spec2.xy, spec2.zw);
	float2 h3 = getSpectrum(K3, spec3.xy, spec3.zw);
	float2 h4 = getSpectrum(K4, spec4.xy, spec4.zw);

	float4 slope = float4(i(k1.x * h1) - k1.y * h1, i(k2.x * h2) - k2.y * h2);
	fftWaves[uint3(DTid.xy, 0)] = float4(slope.xy * IK1 * LAMBDA_J, h1); // grid size 1 displacement
	fftWaves[uint3(DTid.xy, 1)] = float4(slope.zw * IK2 * LAMBDA_J, h2); // grid size 2 displacement
	fftWaves[uint3(DTid.xy, 4)] = slope; // grid size 1 & 2 slope

	slope = float4(i(k3.x * h3) - k3.y * h3, i(k4.x * h4) - k4.y * h4);
	fftWaves[uint3(DTid.xy, 2)] = float4(slope.xy * IK3 * LAMBDA_J, h3); // grid size 3 displacement
	fftWaves[uint3(DTid.xy, 3)] = float4(slope.zw * IK4 * LAMBDA_J, h4); // grid size 4 displacement
	fftWaves[uint3(DTid.xy, 5)] = slope; // grid size 3 & 4 slope
}