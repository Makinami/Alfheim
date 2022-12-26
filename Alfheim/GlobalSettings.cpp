#include "pch.h"

#include "GlobalSettings.h"

namespace Settings
{
	const char* RasterizationNames[] = { "Normal", "Wireframe" };
	EnumVar RasterizationMethod("Rasterization method", RasterizationOptions::kNormal, RasterizationOptions::kPSOCount, RasterizationNames);
}