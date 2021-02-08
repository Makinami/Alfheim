#pragma once

#include "GameCore.h"

class CommandContext;

namespace GpuTimeManager
{
	void Initialize(uint32_t MaxNumTimers = 4096);
	void Shutdown();

	int32_t NewTimer(void);

	void StartTimer(CommandContext& Context, uint32_t TimerIdx);
	void StopTimer(CommandContext& Context, uint32_t TimerIdx);

	void BeginReadBack(void);
	void EndReadBack(void);

	float GetTime(uint32_t TimerIdx);
};

