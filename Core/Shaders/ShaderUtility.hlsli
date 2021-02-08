#ifndef __SHADER_UTILITY_HLSLI__
#define __SHADER_UTILITY_HSLSI__

//#pragma warning(disable: 3571)

#include "ColorSpaceUtility.hlsli"

#define COLOR_FORMAT_LINEAR           0
#define COLOR_FORMAT_sRGB_FULL        1
#define COLOR_FORMAT_sRGB_LIMITED     2
#define COLOR_FORMAT_Rec709_FULL      3
#define COLOR_FORMAT_Rec709_LIMITED   4
#define COLOR_FORMAT_HDR10            5
#define COLOR_FORMAT_TV_DEFAULT       COLOR_FORMAT_Rec709_LIMITED
#define COLOR_FORMAT_PC_DEFAULT       COLOR_FORMAT_sRGB_FULL

#define HDR_COLOR_FORMAT       COLOR_FORMAT_LINEAR
#define LDR_COLOR_FORMAT       COLOR_FORMAT_LINEAR
#define DISPLAY_PLANE_FORMAT   COLOR_FORMAT_PC_DEFAULT

float3 ApplyDisplayProfile(float3 x, int DisplayFormat)
{
    switch (DisplayFormat)
    {
        default:
        case COLOR_FORMAT_LINEAR:
            return x;
        case COLOR_FORMAT_sRGB_FULL:
            return ApplySRGBCurve(x);
    }
}
float3 RemoveDisplayProfile(float3 x, int DisplayFormat)
{
    switch (DisplayFormat)
    {
        default:
        case COLOR_FORMAT_LINEAR:
            return x;
        case COLOR_FORMAT_sRGB_FULL:
            return RemoveSRGBCurve(x);
        //!
    }
}

#endif // __SHADER_UTILITY_HLSLI__