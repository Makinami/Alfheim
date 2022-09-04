RWTexture2DArray<float> Spectrum : register(u0);

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
};

static const float2 gbWind = float2(5, 5);

static const float PI = 3.14159265359;
//static const float G = 9.81;
//static const float DENSITY = 997; // water density kg/m3
//static const float SURFACE_TENSION = 0.0714; // water surface tension N/m

static const float KM = sqrt(DENSITY * G / SURFACE_TENSION); // 370.0
static const float CM = sqrt(2 * G / KM); // 0.23

float square(float x) {
	return x * x;
}

float omega(float k) {
	return sqrt(G * k * (1.0 + square(k / KM)));
}

float tanh(float x) {
	return (1.0 - exp(-2.0 * x)) / (1.0 + exp(-2.0 * x));
}

[numthreads(8, 8, 1)]
void main(int3 DTid : SV_DispatchThreadID)
{
	int2 coordinates = DTid.xy;
	float n = (coordinates.x < RESOLUTION.x * 0.5) ? coordinates.x : coordinates.x - RESOLUTION.x;
	float m = (coordinates.y < RESOLUTION.y * 0.5) ? coordinates.y : coordinates.y - RESOLUTION.y;
	float2 waveVector = float2(n, m) * INVERSE_GRID_SIZE[DTid.z];


	// david
	float k = length(waveVector);

	float U10 = length(WIND);

	float Omega = 0.84;
	float kp = G * square(Omega / U10);

	float c = omega(k) / k;
	float cp = omega(kp) / kp;

	float Lpm = exp(-1.25 * square(kp / k));
	float gamma = 1.7;
	float sigma = 0.08 * (1.0 + 4.0 * pow(Omega, -3.0));
	float Gamma = exp(-square(sqrt(k / kp) - 1.0) / 2.0 * square(sigma));
	float Jp = pow(gamma, Gamma);
	float Fp = Lpm * Jp * exp(-Omega / sqrt(10.0) * (sqrt(k / kp) - 1.0));
	float alphap = 0.006 * sqrt(Omega);
	float Bl = 0.5 * alphap * cp / c * Fp;

	float z0 = 0.000037 * square(U10) / G * pow(U10 / cp, 0.9);
	float uStar = 0.41 * U10 / log(10.0 / z0);
	float alpham = 0.01 * ((uStar < CM) ? (1.0 + log(uStar / CM)) : (1.0 + 3.0 * log(uStar / CM)));
	float Fm = exp(-0.25 * square(k / KM - 1.0));
	float Bh = 0.5 * alpham * CM / c * Fm * Lpm;

	float a0 = log(2.0) / 4.0;
	float am = 0.13 * uStar / CM;
	float Delta = tanh(a0 + 4.0 * pow(c / cp, 2.5) + am * pow(CM / c, 2.5));

	float cosPhi = dot(WIND / U10, normalize(waveVector));

	float S = (1.0 / (2.0 * PI)) * pow(k, -4.0) * (Bl + Bh) * (1.0 + Delta * (2.0 * cosPhi * cosPhi - 1.0));
	//

	float dk = INVERSE_GRID_SIZE[DTid.z];
	float h = sqrt(S / 2.0) * dk;

	if (all(abs(waveVector) < MIN_K[DTid.z]) || isnan(h)) {
		h = 0.0;
	}

	Spectrum[DTid.xyz] = h;
}