#pragma once

#include "EngineTuning.h"

namespace Settings
{
	enum RasterizationOptions { kNormal, kWireframe, kPSOCount };
	extern EnumVar RasterizationMethod;
}

