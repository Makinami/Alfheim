#ifndef __COLOR_SPACE_UTILITY_HLSLI__
#define __COLOR_SPACE_UTILITY_HLSLI__

//#pragma warning(disable: 3571)

//
// Gamma ramps and encoding transfer functions
//
// Orthogonal to color space though usually tightly coupled.  For instance, sRGB is both a
// color space (defined by three basis vectors and a white point) and a gamma ramp.  Gamma
// ramps are designed to reduce perceptual error when quantizing floats to integers with a
// limited number of bits.  More variation is needed in darker colors because our eyes are
// more sensitive in the dark.  The way the curve helps is that it spreads out dark values
// across more code words allowing for more variation.  Likewise, bright values are merged
// together into fewer code words allowing for less variation.
//
// The sRGB curve is not a true gamma ramp but rather a piecewise function comprising a linear
// section and a power function.  When sRGB-encoded colors are passed to an LCD monitor, they
// look correct on screen because the monitor expects the colors to be encoded with sRGB, and it
// removes the sRGB curve to linearize the values.  When textures are encoded with sRGB--as many
// are--the sRGB curve needs to be removed before involving the colors in linear mathematics such
// as physically based lighting.

float3 ApplySRGBCurve(float3 x)
{
	// Approximately pow(x, 1.0 / 2.2)
	return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float3 RemoveSRGBCurve(float3 x)
{
	// Approximately pow(x, 2.2)
	return x < 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

#endif