#pragma once

#include <string>
#include "TextRenderer.h"

class CommandContext;

namespace EngineProfiling
{
	void Update();

	void BeginBlock(const std::wstring& name, CommandContext* Context = nullptr);
	void EndBlock(CommandContext* Context = nullptr);

	void DisplayFrameRate(TextContext& Text);
	void DisplayPerfGraph(GraphicsContext& Text);
	void Display(TextContext& Text, float x, float y, float w, float h);
}

#ifdef RELEASE
class ScopedTimer
{
public:
	ScopedTimer([[maybe_unused]] const std::wstring&) {}
	ScopedTimer([[maybe_unused]] const std::wstring&, [[maybe_unused]] CommandContext&) {}
};
#else
class ScopedTimer
{
public:
	ScopedTimer(const std::wstring& name)
	{
		EngineProfiling::BeginBlock(name);
	}
	ScopedTimer(const std::wstring& name, CommandContext& Context) : m_Context(&Context)
	{
		EngineProfiling::BeginBlock(name, m_Context);
	}
	~ScopedTimer()
	{
		EngineProfiling::EndBlock(m_Context);
	}

private:
	CommandContext* m_Context;
};
#endif