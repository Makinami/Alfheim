#include "pch.h"
#include "SystemTime.h"
#include "GraphRenderer.h"
#include "GameInput.h"
#include "GpuTimeManager.h"
#include "CommandContext.h"

using namespace Graphics;
using namespace GraphRenderer;
using namespace Math;
using namespace std;

#define PERF_GRAPH_ERROR uint32_t(0xFFFFFFFF)
namespace EngineProfiling
{
	bool Paused = false;
}

class StatHistory
{
public:
	void RecordStat(uint32_t FrameIndex, float Value)
	{
		m_RecentHistory[FrameIndex % kHistorySize] = Value;
		m_ExtendedHistory[FrameIndex % kExtendedHistorySize] = Value;
		m_Recent = Value;

		uint32_t ValidCount = 0;
		m_Minimum = FLT_MAX;
		m_Maximum = 0.0f;
		m_Average = 0.0f;

		for (float val : m_RecentHistory)
		{
			if (val > 0.0f)
			{
				++ValidCount;
				m_Average += val;
				m_Minimum = min(val, m_Minimum);
				m_Maximum = max(val, m_Maximum);
			}
		}

		if (ValidCount > 0)
			m_Average /= (float)ValidCount;
		else
			m_Minimum = 0.0f;
	}

	float GetLast(void) const { return m_Recent; }
	float GetMax(void) const { return m_Maximum; }
	float GetMin(void) const { return m_Minimum; }
	float GetAvg(void) const { return m_Average; }

	const float* GetHistory(void) const { return m_ExtendedHistory; }
	uint32_t GetHistoryLength(void) const { return kExtendedHistorySize; }

private:
	static const uint32_t kHistorySize = 64;
	static const uint32_t kExtendedHistorySize = 256;
	float m_RecentHistory[kHistorySize] = { 0.0f };
	float m_ExtendedHistory[kExtendedHistorySize] = { 0.0f };
	float m_Recent = 0.0f;
	float m_Average = 0.0f;
	float m_Minimum = 0.0f;
	float m_Maximum = 0.0f;
};

class GpuTimer
{
public:
	GpuTimer()
	{
		m_TimerIndex = GpuTimeManager::NewTimer();
	}

	void Start(CommandContext& Context)
	{
		GpuTimeManager::StartTimer(Context, m_TimerIndex);
	}

	void Stop(CommandContext& Context)
	{
		GpuTimeManager::StopTimer(Context, m_TimerIndex);
	}

	float GetTime(void)
	{
		return GpuTimeManager::GetTime(m_TimerIndex);
	}

	uint32_t GetTimerIndex(void)
	{
		return m_TimerIndex;
	}
private:
	uint32_t m_TimerIndex;
};

class NestedTimingTree
{
public:
	NestedTimingTree(const wstring& name, NestedTimingTree* parent = nullptr)
		: m_Name(name), m_Parent(parent)
	{}

	NestedTimingTree* GetChild(const wstring& name)
	{
		auto iter = m_LUT.find(name);
		if (iter != m_LUT.end())
			return iter->second;

		NestedTimingTree* node = new NestedTimingTree(name, this);
		m_Children.push_back(node);
		m_LUT[name] = node;
		return node;
	}

	NestedTimingTree* NextScope(void)
	{
		if (m_IsExpanded && m_Children.size() > 0)
			return m_Children[0];

		return m_Parent->NextChild(this);
	}

	NestedTimingTree* PrevScope(void)
	{
		NestedTimingTree* prev = m_Parent->PrevChild(this);
		return prev == m_Parent ? prev : prev->LastChild();
	}

	NestedTimingTree* FirstChild(void)
	{
		return m_Children.size() == 0 ? nullptr : m_Children[0];
	}

	NestedTimingTree* LastChild(void)
	{
		if (!m_IsExpanded || m_Children.size() == 0)
			return this;

		return m_Children.back()->LastChild();
	}

	NestedTimingTree* NextChild(NestedTimingTree* curChild)
	{
		ASSERT(curChild->m_Parent == this);

		if (auto iter = std::find(m_Children.begin(), m_Children.end(), curChild); 
			iter != m_Children.end() && ++iter != m_Children.end())
		{
			return *iter;
		}

		if (m_Parent != nullptr)
			return m_Parent->NextChild(this);
		else
			return &sm_RootScope;
	}

	NestedTimingTree* PrevChild(NestedTimingTree* curChild)
	{
		ASSERT(curChild->m_Parent == this);

		if (*m_Children.begin() == curChild)
		{
			if (this == &sm_RootScope)
				return sm_RootScope.LastChild();
			else
				return this;
		}

		if (auto iter = std::find(m_Children.rbegin(), m_Children.rend(), curChild);
			iter != m_Children.rend() && ++iter != m_Children.rend())
		{
			return *iter;
		}

		ERROR("All attempts to find a previous timing sample failed");

		return nullptr;
	}

	void StartTiming(CommandContext* Context)
	{
		m_StartTick = SystemTime::GetCurrentTick();
		if (Context == nullptr)
			return;

		m_GpuTimer.Start(*Context);

		Context->PIXBeginEvent(m_Name.c_str());
	}

	void StopTiming(CommandContext* Context)
	{
		m_EndTick = SystemTime::GetCurrentTick();
		if (Context == nullptr)
			return;

		m_GpuTimer.Stop(*Context);

		Context->PIXEndEvent();
	}

	void GatherTimes(uint32_t FrameIndex)
	{
		if (sm_SelectedScope == this)
		{
			GraphRenderer::SetSelectedIndex(m_GpuTimer.GetTimerIndex());
		}
		if (EngineProfiling::Paused)
		{
			for (auto node : m_Children)
				node->GatherTimes(FrameIndex);
			return;
		}
		m_CpuTime.RecordStat(FrameIndex, 1000.0f * (float)SystemTime::TimeBetweenTicks(m_StartTick, m_EndTick));
		m_GpuTime.RecordStat(FrameIndex, 1000.0f * m_GpuTimer.GetTime());

		for (auto node : m_Children)
			node->GatherTimes(FrameIndex);

		m_StartTick = 0;
		m_EndTick = 0;
	}

	void SumInclusiveTimes(float& cpuTime, float& gpuTime)
	{
		cpuTime = 0.0f;
		gpuTime = 0.0f;
		for (auto node : m_Children) {
			cpuTime += node->m_CpuTime.GetLast();
			gpuTime += node->m_GpuTime.GetLast();
		}
	}

	static void PushProfilingMarker(const wstring& name, CommandContext* Context);
	static void PopProfilingMarker(CommandContext* Context);
	static void Update(void);
	static void UpdateTimes(void)
	{
		uint32_t FrameIndex = (uint32_t)Graphics::GetFrameCount();

		GpuTimeManager::BeginReadBack();
		sm_RootScope.GatherTimes(FrameIndex);
		s_FrameDelta.RecordStat(FrameIndex, GpuTimeManager::GetTime(0));
		GpuTimeManager::EndReadBack();

		float TotalCpuTime, TotalGpuTime;
		sm_RootScope.SumInclusiveTimes(TotalCpuTime, TotalGpuTime);
		s_TotalCpuTime.RecordStat(FrameIndex, TotalCpuTime);
		s_TotalGpuTime.RecordStat(FrameIndex, TotalGpuTime);

		GraphRenderer::Update(XMFLOAT2(TotalCpuTime, TotalGpuTime), 0, GraphType::Global);
	}

	static float GetTotalCpuTime(void) { return s_TotalCpuTime.GetAvg(); }
	static float GetTotalGpuTime(void) { return s_TotalGpuTime.GetAvg(); }
	static float GetFrameDelta(void) { return s_FrameDelta.GetAvg(); }

	static void Display(TextContext& Text, float x)
	{
		float curX = Text.GetCursorX();
		Text.DrawString("  ");
		float indent = Text.GetCursorX() - curX;
		Text.SetCursorX(curX);
		sm_RootScope.DisplayNode(Text, x - indent, indent);
		sm_RootScope.StoreToGraph();

	}

	void Toogle()
	{
		if (m_GraphHandle == PERF_GRAPH_ERROR)
		    m_GraphHandle = GraphRenderer::InitGraph(GraphType::Profile);
		m_IsGraphed = GraphRenderer::ToggleGraph(m_GraphHandle, GraphType::Profile);
	}
	bool IsGraphed() { return m_IsGraphed; }

private:

	void DisplayNode(TextContext& Text, float x, float indent);
	void StoreToGraph(void);
	void DeleteChildren(void)
	{
		for (auto node : m_Children)
			delete node;
		m_Children.clear();
	}

	wstring m_Name;
	NestedTimingTree* m_Parent;
	vector<NestedTimingTree*> m_Children;
	unordered_map<wstring, NestedTimingTree*> m_LUT;
	int64_t m_StartTick;
	int64_t m_EndTick;
	StatHistory m_CpuTime;
	StatHistory m_GpuTime;
	bool m_IsExpanded = false;
	GpuTimer m_GpuTimer;
	bool m_IsGraphed = false;
	GraphHandle m_GraphHandle = PERF_GRAPH_ERROR;
	static StatHistory s_TotalCpuTime;
	static StatHistory s_TotalGpuTime;
	static StatHistory s_FrameDelta;
	static NestedTimingTree sm_RootScope;
	static NestedTimingTree* sm_CurrentNode;
	static NestedTimingTree* sm_SelectedScope;

	static bool sm_CursorOnGraph;
};

void NestedTimingTree::PushProfilingMarker(const wstring& name, CommandContext* Context)
{
	sm_CurrentNode = sm_CurrentNode->GetChild(name);
	sm_CurrentNode->StartTiming(Context);
}

void NestedTimingTree::PopProfilingMarker(CommandContext* Context)
{
	sm_CurrentNode->StopTiming(Context);
	sm_CurrentNode = sm_CurrentNode->m_Parent;
}

void NestedTimingTree::Update(void)
{
	ASSERT(sm_SelectedScope != nullptr, "Corrupted profiling data structure");

	if (sm_SelectedScope == &sm_RootScope)
	{
		sm_SelectedScope = sm_RootScope.FirstChild();
		if (sm_SelectedScope == &sm_RootScope)
			return;
	}

	if (GameInput::IsFirstPressed(GameInput::DigitalInput::kDPadLeft)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_left))
	{
		// if still on graphs go back to text
		if (sm_CursorOnGraph)
			sm_CursorOnGraph = !sm_CursorOnGraph;
		else
			sm_SelectedScope->m_IsExpanded = false;
	}
	else if (GameInput::IsFirstPressed(GameInput::DigitalInput::kDPadRight)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_right))
	{
		if (sm_SelectedScope->m_IsExpanded == true && !sm_CursorOnGraph)
			sm_CursorOnGraph = true;
		else
			sm_SelectedScope->m_IsExpanded = true;
	}
	else if (GameInput::IsFirstPressed(GameInput::DigitalInput::kDPadDown)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_down))
	{
		sm_SelectedScope = sm_SelectedScope ? sm_SelectedScope->NextScope() : nullptr;
	}
	else if (GameInput::IsFirstPressed(GameInput::DigitalInput::kDPadUp)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_up))
	{
		sm_SelectedScope = sm_SelectedScope ? sm_SelectedScope->PrevScope() : nullptr;
	}
	else if (GameInput::IsFirstPressed(GameInput::DigitalInput::kAButton)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_return))
	{
		sm_SelectedScope->Toogle();
	}
}

void NestedTimingTree::DisplayNode(TextContext& Text, float leftMargin, float indent)
{
	if (this == &sm_RootScope)
	{
		m_IsExpanded = true;
		sm_RootScope.FirstChild()->m_IsExpanded = true;
	}
	else
	{
		if (sm_SelectedScope == this && !sm_CursorOnGraph)
			Text.SetColor(Color(1.0f, 1.0f, 0.5f));
		else
			Text.SetColor(Color(1.0f, 1.0f, 1.0f));

		Text.SetLeftMargin(leftMargin);
		Text.SetCursorX(leftMargin);

		if (m_Children.size() == 0)
			Text.DrawString("  ");
		else if (m_IsExpanded)
			Text.DrawString("- ");
		else
			Text.DrawString("+ ");

		Text.DrawString(m_Name);
		Text.SetCursorX(leftMargin + 300.0f);
		Text.DrawFormattedString("{:6.3f} {:6.3f}   ", m_CpuTime.GetAvg(), m_GpuTime.GetAvg());

		if (IsGraphed())
		{
			Text.SetColor(GraphRenderer::GetGraphColor(m_GraphHandle, GraphType::Profile));
			Text.DrawString("  []\n");
		}
		else
			Text.DrawString("\n");
	}

	if (!m_IsExpanded)
		return;

	for (auto node : m_Children)
		node->DisplayNode(Text, leftMargin + indent, indent);
}

void NestedTimingTree::StoreToGraph(void)
{
	if (m_GraphHandle != PERF_GRAPH_ERROR)
		GraphRenderer::Update(XMFLOAT2(m_CpuTime.GetLast(), m_GpuTime.GetLast()), m_GraphHandle, GraphType::Profile);

	for (auto node : m_Children)
		node->StoreToGraph();
}

StatHistory NestedTimingTree::s_TotalCpuTime;
StatHistory NestedTimingTree::s_TotalGpuTime;
StatHistory NestedTimingTree::s_FrameDelta;
NestedTimingTree NestedTimingTree::sm_RootScope(L"");
NestedTimingTree* NestedTimingTree::sm_CurrentNode = &NestedTimingTree::sm_RootScope;
NestedTimingTree* NestedTimingTree::sm_SelectedScope = &NestedTimingTree::sm_RootScope;
bool NestedTimingTree::sm_CursorOnGraph = false;

namespace EngineProfiling
{
	BoolVar DrawFrameRate("Display Frame Rate", true);
	BoolVar DrawProfiler("Display Profiler", false);
	BoolVar DrawPerfGraph("Display Performace Graph", false);

	void Update()
	{
		if (GameInput::IsFirstPressed(GameInput::DigitalInput::kStartButton)
			|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_space))
		{
			Paused = !Paused;
		}
		NestedTimingTree::UpdateTimes();
	}

	void BeginBlock(const std::wstring& name, CommandContext* Context)
	{
		NestedTimingTree::PushProfilingMarker(name, Context);
	}

	void EndBlock(CommandContext* Context)
	{
		NestedTimingTree::PopProfilingMarker(Context);
	}

	void DisplayFrameRate(TextContext& Text)
	{
		if (!DrawFrameRate)
			return;

		float cpuTime = NestedTimingTree::GetTotalCpuTime();
		float gpuTime = NestedTimingTree::GetTotalGpuTime();
		float frameRate = 1.0f / NestedTimingTree::GetFrameDelta();

		Text.DrawFormattedString("CPU {:7.3f} ms, GPU {:7.3f} ms, {} Hz\n", //? format correctly
			cpuTime, gpuTime, (uint32_t)(frameRate + 0.5f));
	}

	void DisplayPerfGraph(GraphicsContext& Context)
	{
		if (DrawPerfGraph)
			GraphRenderer::RenderGraphs(Context, GraphType::Global);
	}

	void Display(TextContext& Text, float x, float y, [[maybe_unused]] float w, [[maybe_unused]] float h)
	{
		Text.ResetCursor(x, y);

		if (DrawProfiler)
		{
			NestedTimingTree::Update();

			Text.SetColor(Color(0.5f, 1.0f, 1.0f));
			Text.DrawString("Engine Profiling");
			Text.SetColor(Color(0.8f, 0.8f, 0.8f));
			Text.SetTextSize(20.0f);
			Text.DrawString("           CPU    GPU");
			Text.NewLine();
			Text.SetColor(Color(1.0f, 1.0f, 1.0f));

			NestedTimingTree::Display(Text, x);
		}

		Text.GetCommandContext().SetScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
	}
}