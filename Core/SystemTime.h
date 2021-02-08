#pragma once

class SystemTime
{
public:
	static void Initialize(void);

	static int64_t GetCurrentTick(void);

	static inline double TicksToSeconds(int64_t TickCount)
	{
		return TickCount * sm_CpuTickDelta;
	}

	static inline double TickToMilisecs(int64_t TickCount)
	{
		return TickCount * sm_CpuTickDelta * 1000.0f;
	}

	static inline double TimeBetweenTicks(int64_t tick1, int64_t tick2)
	{
		return TicksToSeconds(tick2 - tick1);
	}

private:
	static double sm_CpuTickDelta;
};