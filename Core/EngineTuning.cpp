#include "pch.h"
#include "TextRenderer.h"
#include "GameInput.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "GraphRenderer.h"

using namespace std;
using namespace Math;
using namespace Graphics;

namespace EngineTuning
{
	enum { kMaxUnregisteredTweaks = 1024 };
	char s_UnregisteredPath[kMaxUnregisteredTweaks][128];
	EngineVar* s_UnregisteredVariable[kMaxUnregisteredTweaks] = { nullptr };
	auto s_UnregisteredCount = int32_t{ 0 };

	float s_ScrollOffset = 0.0f;
	float s_ScrollTopTrigger = 1080.0f * 0.2f;
	float s_ScrollBottomTrigger = 1080.f * 0.8f;

	void AddToVariableGraph(const string& path, EngineVar& var);
	void RegisterVariable(const string& path, EngineVar& var);

	EngineVar* sm_SelectedVariable = nullptr;
	bool sm_IsVisible = false;
}

class VariableGroup : public EngineVar
{
public:
	VariableGroup() = default;

	EngineVar* FindChild(const string& name)
	{
		auto iter = m_Children.find(name);
		return iter == m_Children.end() ? nullptr : iter->second;
	}

	void AddChild(const string& name, EngineVar& child)
	{
		m_Children[name] = &child;
		child.m_GroupPtr = this;
	}

	void Display(TextContext& Text, float leftMargin, EngineVar* highlightedTweak);

	void SaveToFile(FILE* file, int fileMargin);
	void LoadSettingsFromFile(FILE* file);

	EngineVar* NextVariable(EngineVar* currentVariable);
	EngineVar* PrevVariable(EngineVar* currentVariable);
	EngineVar* FirstVariable(void);
	EngineVar* LastVariable(void);

	bool IsExpanded(void) const { return m_IsExpanded; }

	virtual void Increment(void) override { m_IsExpanded = true; }
	virtual void Decrement(void) override { m_IsExpanded = false; }
	virtual void Bang(void) override { m_IsExpanded = !m_IsExpanded; }

	virtual void SetValue(FILE*, const std::string&) override {}

	static VariableGroup sm_RootGroup;

private:
	bool m_IsExpanded = false;
	std::map<string, EngineVar*> m_Children;
};

VariableGroup VariableGroup::sm_RootGroup;

EngineVar* EngineVar::NextVar(void)
{
	EngineVar* next = nullptr;
	VariableGroup* isGroup = dynamic_cast<VariableGroup*>(this);
	if (isGroup != nullptr && isGroup->IsExpanded())
		next = isGroup->FirstVariable();

	if (next == nullptr)
		next = m_GroupPtr->NextVariable(this);

	return next != nullptr ? next : this;
}

EngineVar* EngineVar::PrevVar(void)
{
	EngineVar* prev = m_GroupPtr->PrevVariable(this);
	if (prev != nullptr && prev != m_GroupPtr)
	{
		VariableGroup* isGroup = dynamic_cast<VariableGroup*>(prev);
		if (isGroup != nullptr && isGroup->IsExpanded())
			prev = isGroup->FirstVariable();
	}
	return prev != nullptr ? prev : this;
}

EngineVar::EngineVar(const std::string& path)
{
	EngineTuning::RegisterVariable(path, *this);
}

void EngineTuning::Initialize(void)
{
	for (auto i = 0; i < s_UnregisteredCount; ++i)
	{
		ASSERT(strlen(s_UnregisteredPath[i]) > 0, "Registered = {}\n", i);
		ASSERT(s_UnregisteredVariable[i] != nullptr);
		AddToVariableGraph(s_UnregisteredPath[i], *s_UnregisteredVariable[i]);
	}
	s_UnregisteredCount = -1;
}

void HandleDigitalButtonPress(GameInput::DigitalInput button, float timeDelta, std::function<void()> action)
{
	if (!GameInput::IsPressed(button))
		return;

	float durationHeld = GameInput::GetDurationPressed(button);

	if (durationHeld == 0.0f)
	{
		action();
		return;
	}

	// After ward, tick at fixed intervals
	float oldDuration = durationHeld - timeDelta;

	// Before 2 seconds, use slow scale (200ms/tick), afterward use fast scale (50ms/tick).
	float timeStretch = durationHeld < 2.0f ? 5.0f : 20.0f;

	if (Floor(durationHeld * timeStretch) > Floor(oldDuration * timeStretch))
		action();
}

void EngineTuning::Update(float frameTime)
{
	if (GameInput::IsFirstPressed(GameInput::DigitalInput::kBackButton)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_back))
		sm_IsVisible = !sm_IsVisible;

	if (!sm_IsVisible)
		return;

	if (sm_SelectedVariable == nullptr || sm_SelectedVariable == &VariableGroup::sm_RootGroup)
		sm_SelectedVariable = VariableGroup::sm_RootGroup.FirstVariable();

	if (sm_SelectedVariable == nullptr)
		return;

	HandleDigitalButtonPress(GameInput::DigitalInput::kDPadRight, frameTime, [] { sm_SelectedVariable->Increment(); });
	HandleDigitalButtonPress(GameInput::DigitalInput::kDPadLeft, frameTime, [] { sm_SelectedVariable->Decrement(); });
	HandleDigitalButtonPress(GameInput::DigitalInput::kDPadDown, frameTime, [] { sm_SelectedVariable = sm_SelectedVariable->NextVar(); });
	HandleDigitalButtonPress(GameInput::DigitalInput::kDPadUp, frameTime, [] { sm_SelectedVariable = sm_SelectedVariable->PrevVar(); });

	HandleDigitalButtonPress(GameInput::DigitalInput::kKey_right, frameTime, [] { sm_SelectedVariable->Increment(); });
	HandleDigitalButtonPress(GameInput::DigitalInput::kKey_left, frameTime, [] { sm_SelectedVariable->Decrement(); });
	HandleDigitalButtonPress(GameInput::DigitalInput::kKey_down, frameTime, [] { sm_SelectedVariable = sm_SelectedVariable->NextVar(); });
	HandleDigitalButtonPress(GameInput::DigitalInput::kKey_up, frameTime, [] { sm_SelectedVariable = sm_SelectedVariable->PrevVar(); });

	if (GameInput::IsFirstPressed(GameInput::DigitalInput::kAButton)
		|| GameInput::IsFirstPressed(GameInput::DigitalInput::kKey_return))
	{
		sm_SelectedVariable->Bang();
	}
}

void StartSave(void*)
{
	FILE* settingsFile;
	fopen_s(&settingsFile, "engineTuning.txt", "wb");
	if (settingsFile != nullptr)
	{
		VariableGroup::sm_RootGroup.SaveToFile(settingsFile, 2);
		fclose(settingsFile);
	}
}
static CallbackTrigger Save("Save Settings", StartSave, nullptr);

void StartLoad(void*)
{
	FILE* settingsFile;
	fopen_s(&settingsFile, "engineTuning.txt", "rb");
	if (settingsFile != nullptr)
	{
		VariableGroup::sm_RootGroup.LoadSettingsFromFile(settingsFile);
		fclose(settingsFile);
	}
}
static CallbackTrigger Load("Load Settings", StartLoad, nullptr);

void EngineTuning::Display(GraphicsContext& Context, float x, float y, float w, float h)
{
	GraphRenderer::RenderGraphs(Context, GraphRenderer::GraphType::Profile);

	TextContext Text(Context);
	Text.Begin();

	EngineProfiling::DisplayFrameRate(Text);

	Text.ResetCursor(x, y);

	if (!sm_IsVisible)
	{
		EngineProfiling::Display(Text, x, y, w, h);
		return;
	}

	s_ScrollTopTrigger = y + h * 0.2f;
	s_ScrollBottomTrigger = y + h * 0.8f;

	float hScale = g_DisplayWidth / 1920.0f;
	float vScale = g_DisplayHeight / 1080.0f;

	Context.SetScissor((uint32_t)Floor(x * hScale), (uint32_t)Floor(y * vScale),
		(uint32_t)Ceiling((x + w) * hScale), (uint32_t)Ceiling((y + h) * vScale));

	Text.ResetCursor(x, y - s_ScrollOffset);
	Text.SetColor(Color(0.5f, 1.0f, 1.0f));
	Text.DrawString("Engine Tuning\n");
	Text.SetTextSize(20.0f);

	VariableGroup::sm_RootGroup.Display(Text, x, sm_SelectedVariable);

	EngineProfiling::DisplayPerfGraph(Context);

	Text.End();
	Context.SetScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
}

void EngineTuning::AddToVariableGraph(const string& path, EngineVar& var)
{
	vector<string> separatedPath;
	string leafName;
	size_t start = 0, end = 0;

	while (true)
	{
		end = path.find('/', start);
		if (end == string::npos)
		{
			leafName = path.substr(start);
			break;
		}
		else
		{
			separatedPath.push_back(path.substr(start, end - start));
			start = end + 1;
		}
	}

	VariableGroup* group = &VariableGroup::sm_RootGroup;

	for (auto segment : separatedPath)
	{
		VariableGroup* nextGroup;
		EngineVar* node = group->FindChild(segment);
		if (node == nullptr)
		{
			nextGroup = new VariableGroup();
			group->AddChild(segment, *nextGroup);
			group = nextGroup;
		}
		else
		{
			nextGroup = dynamic_cast<VariableGroup*>(node);
			ASSERT(nextGroup != nullptr, "Attempted to trash the tweak graph");
			group = nextGroup;
		}
	}

	group->AddChild(leafName, var);
}

EnumVar::EnumVar(const std::string& path, int32_t initialVal, int32_t listLength, const char** listLabels)
	: EngineVar(path)
{
	ASSERT(listLength > 0);
	m_EnumLength = listLength;
	m_EnumLabels = listLabels;
	m_Value = Clamp(initialVal);
}

void EnumVar::DisplayValue(TextContext& Text) const
{
	Text.DrawString(m_EnumLabels[m_Value]);
}

std::string EnumVar::ToString(void) const
{
	return m_EnumLabels[m_Value];
}

void EnumVar::SetValue(FILE* file, const std::string& setting)
{
	std::string scanString = "\n" + setting + ": %[^\n]";
	char valueRead[14];

	if (fscanf_s(file, scanString.c_str(), valueRead, _countof(valueRead)) == 1)
	{
		std::string valueReadStr = valueRead;
		valueReadStr = valueReadStr.substr(0, valueReadStr.length() - 1);

		// if we don't find the string, then leave m_EnumLabels[m_Value] as default
		for (int32_t i = 0; i < m_EnumLength; ++i)
		{
			if (m_EnumLabels[i] == valueReadStr)
			{
				m_Value = i;
				break;
			}
		}
	}
}

void EngineTuning::RegisterVariable(const string& path, EngineVar& var)
{
	if (s_UnregisteredCount >= 0)
	{
		int32_t Idx = s_UnregisteredCount++;
		strcpy_s(s_UnregisteredPath[Idx], path.c_str());
		s_UnregisteredVariable[Idx] = &var;
	}
	else
	{
		AddToVariableGraph(path, var);
	}
}

BoolVar::BoolVar(const std::string& path, bool val)
	: EngineVar(path), m_Flag(val)
{}

void BoolVar::DisplayValue(TextContext & Text) const
{
	Text.DrawFormattedString("[{}]", m_Flag ? 'X' : '-');
}

std::string BoolVar::ToString(void) const
{
	return m_Flag ? "on" : "off";
}

void BoolVar::SetValue(FILE* file, const std::string& setting)
{
	std::string pattern = "\n " + setting + ": %s";
	char valstr[6];

	// Search through the file for an entry that matches this setting's name
	fscanf_s(file, pattern.c_str(), valstr, _countof(valstr));

	// Look for one of the many affirmations
	m_Flag = (
		0 == _stricmp(valstr, "1") ||
		0 == _stricmp(valstr, "on") ||
		0 == _stricmp(valstr, "yes") ||
		0 == _stricmp(valstr, "true"));
}

void VariableGroup::Display(TextContext& Text, float leftMargin, EngineVar* highlightedTweak)
{
	Text.SetLeftMargin(leftMargin);
	Text.SetCursorX(leftMargin);

	for (auto [name, var]: m_Children)
	{
		if (var == highlightedTweak)
		{
			Text.SetColor(Color(1.0f, 1.0f, 0.25f));
			float temp1 = Text.GetCursorY() - EngineTuning::s_ScrollBottomTrigger;
			float temp2 = Text.GetCursorY() - EngineTuning::s_ScrollTopTrigger;
			if (temp1 > 0.0f)
			{
				EngineTuning::s_ScrollOffset += 0.2f * temp1;
			}
			else if (temp2 < 0.0f)
			{
				EngineTuning::s_ScrollOffset = max(0.0f, EngineTuning::s_ScrollOffset + 0.2f * temp2);
			}
		}
		else
			Text.SetColor(Color(1.0f, 1.0f, 1.0f));

		VariableGroup* subGroup = dynamic_cast<VariableGroup*>(var);
		if (subGroup != nullptr)
		{
			if (subGroup->IsExpanded())
			{
				Text.DrawString("- ");
			}
			else
			{
				Text.DrawString("+ ");
			}
			Text.DrawString(name);
			Text.DrawString("/...\n");

			if (subGroup->IsExpanded())
			{
				subGroup->Display(Text, leftMargin + 30.0f, highlightedTweak);
				Text.SetLeftMargin(leftMargin);
				Text.SetCursorX(leftMargin);
			}
		}
		else
		{
			var->DisplayValue(Text);
			Text.SetCursorX(leftMargin + 200.0f);
			Text.DrawString(name);
			Text.NewLine();
		}
	}
}

void VariableGroup::SaveToFile(FILE* file, int fileMargin)
{
	for (auto& [name, variable] : m_Children)
	{
		const char* buffer = name.c_str();

		VariableGroup* subGroup = dynamic_cast<VariableGroup*>(variable);
		if (subGroup != nullptr)
		{
			fmt::print(file, "{:{}} + {} ...\n", ' ', fileMargin, buffer);
			subGroup->SaveToFile(file, fileMargin + 5);
		}
		else if (dynamic_cast<CallbackTrigger*>(variable) == nullptr)
		{
			fmt::print(file, "{:{}} {}:  {}\n", ' ', fileMargin, buffer, variable->ToString().c_str());
		}
	}
}

void VariableGroup::LoadSettingsFromFile(FILE* file)
{
	for (auto& [name, variable] : m_Children)
	{
		VariableGroup* subGroup = dynamic_cast<VariableGroup*>(variable);
		if (subGroup != nullptr)
		{
			char skippedLines[100];
			fscanf_s(file, "%*s %[^\n]", skippedLines, (int)_countof(skippedLines));
			subGroup->LoadSettingsFromFile(file);
		}
		else
		{
			variable->SetValue(file, name);
		}
	}
}

EngineVar* VariableGroup::NextVariable(EngineVar* currentVariable)
{
	auto iter = m_Children.begin();
	for (; iter != m_Children.end(); ++iter)
	{
		if (currentVariable == iter->second)
			break;
	}

	ASSERT(iter != m_Children.end(), "Did not find engine variable in its designated group");

	auto nextIter = iter;
	++nextIter;

	if (nextIter == m_Children.end())
		return m_GroupPtr ? m_GroupPtr->NextVariable(this) : nullptr;
	else
		return nextIter->second;
}

EngineVar* VariableGroup::PrevVariable(EngineVar* currentVariable)
{
	auto iter = m_Children.begin();
	for (; iter != m_Children.end(); ++iter)
	{
		if (currentVariable == iter->second)
			break;
	}

	ASSERT(iter != m_Children.end(), "Did not find engine variable in its designated group");

	if (iter == m_Children.begin())
		return this;

	auto prevIter = iter;
	--prevIter;

	VariableGroup* isGroup = dynamic_cast<VariableGroup*>(prevIter->second);
	if (isGroup && isGroup->IsExpanded())
		return isGroup->LastVariable();

	return prevIter->second;
}

EngineVar* VariableGroup::FirstVariable(void)
{
	return m_Children.size() == 0 ? nullptr : m_Children.begin()->second;
}

EngineVar* VariableGroup::LastVariable(void)
{
	if (m_Children.size() == 0)
		return this;

	auto LastVariable = m_Children.rbegin();

	VariableGroup* isGroup = dynamic_cast<VariableGroup*>(LastVariable->second);
	if (isGroup && isGroup->IsExpanded())
		return isGroup->LastVariable();

	return LastVariable->second;
}

NumVar::NumVar(const std::string& path, float val, float minVal, float maxVal, float stepSize)
	: EngineVar(path)
{
	ASSERT(minVal <= maxVal);
	m_MinValue = minVal;
	m_MaxValue = maxVal;
	m_Value = Clamp(val);
	m_StepSize = stepSize;
}

void NumVar::DisplayValue(TextContext& Text) const
{
	Text.DrawFormattedString("{:<11.6}", m_Value);
}

std::string NumVar::ToString(void) const
{
	return std::to_string(m_Value);
}

void NumVar::SetValue(FILE* file, const std::string& setting)
{
	std::string scanString = "\n" + setting + ": %f";
	float valueRead;

	// If we haven't read correctly, just keep m_Value at default value
	if (fscanf_s(file, scanString.c_str(), &valueRead))
		*this = valueRead;
}

CallbackTrigger::CallbackTrigger(const std::string& path, std::function<void(void*)> callback, void* args)
	: EngineVar(path), m_Callback(callback), m_Arguments(args), m_BangDisplay(0)
{
}

void CallbackTrigger::DisplayValue(TextContext& Text) const
{
	static const char s_animation[] = { '-','\\','|','/' };
	Text.DrawFormattedString("[{}]", s_animation[(m_BangDisplay >> 3) & 3]);

	if (m_BangDisplay > 0)
		--m_BangDisplay;
}

void CallbackTrigger::SetValue(FILE* file, const std::string& setting)
{
	// Skip over setting without reading anything
	std::string scanString = "\n" + setting + ": %[^\n]";
	char skippedLines[100];
	fscanf_s(file, scanString.c_str(), skippedLines, _countof(skippedLines));
}
