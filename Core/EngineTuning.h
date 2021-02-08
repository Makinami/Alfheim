#pragma once

#include <string>

class VariableGroup;
class TextContext;

class EngineVar
{
public:
	virtual ~EngineVar() {}

	virtual void Increment(void) {}
	virtual void Decrement(void) {}
	virtual void Bang(void) {}

	virtual void DisplayValue(TextContext&) const {}
	virtual std::string ToString(void) const { return ""; }
	virtual void SetValue(FILE* file, const std::string& setting) = 0; // set value read from file

	EngineVar* NextVar(void);
	EngineVar* PrevVar(void);

protected:
	EngineVar(void) = default;
	EngineVar(const std::string& path);

private:
	friend class VariableGroup;
	VariableGroup* m_GroupPtr = nullptr;
};

class BoolVar : public EngineVar
{
public:
	BoolVar(const std::string& path, bool val);
	BoolVar& operator=(bool val) { m_Flag = val; return *this; }
	operator bool() const { return m_Flag; }

	virtual void Increment(void) override { m_Flag = true; }
	virtual void Decrement(void) override { m_Flag = false; }
	virtual void Bang(void) override { m_Flag = !m_Flag; }

	virtual void DisplayValue(TextContext& Text) const override;
	virtual std::string ToString(void) const override;
	virtual void SetValue(FILE* file, const std::string& setting) override;

private:
	bool m_Flag;
};

class NumVar : public EngineVar
{
public:
	NumVar(const std::string& path, float val, float minValue = -FLT_MAX, float maxValue = FLT_MAX, float stepSize = 1.0f);
	NumVar& operator=(float val) { m_Value = Clamp(val); return *this; }
	operator float() const { return m_Value; }

	virtual void Increment(void) override { m_Value = Clamp(m_Value + m_StepSize); }
	virtual void Decrement(void) override { m_Value = Clamp(m_Value - m_StepSize); }

	virtual void DisplayValue(TextContext& Text) const override;
	virtual std::string ToString(void) const override;
	virtual void SetValue(FILE* file, const std::string& setting) override;

protected:
	float Clamp(float val) { return val > m_MaxValue ? m_MaxValue : val < m_MinValue ? m_MinValue : val; }

	float m_Value;
	float m_MinValue;
	float m_MaxValue;
	float m_StepSize;
};

class EnumVar : public EngineVar
{
public:
	EnumVar(const std::string& path, int32_t initialVal, int32_t listLength, const char** listLabels);
	EnumVar& operator=(int32_t val) { m_Value = Clamp(val); return *this; }
	operator int32_t() const { return m_Value; }

	virtual void Increment(void) override { m_Value = (m_Value + 1) % m_EnumLength; }
	virtual void Decrement(void) override { m_Value = (m_Value + m_EnumLength - 1) % m_EnumLength; }

	virtual void DisplayValue(TextContext& Text) const override;
	virtual std::string ToString(void) const override;
	virtual void SetValue(FILE* file, const std::string& setting) override;

private:
	int32_t Clamp(int32_t val) { return val < 0 ? 0 : val >= m_EnumLength ? m_EnumLength - 1 : val; }

	int32_t m_Value;
	int32_t m_EnumLength;
	const char** m_EnumLabels;
};

class CallbackTrigger : public EngineVar
{
public:
	CallbackTrigger(const std::string& path, std::function<void(void*)> callback, void* args = nullptr);

	virtual void Bang(void) override { m_Callback(m_Arguments); m_BangDisplay = 64; }

	virtual void DisplayValue(TextContext& Text) const override;
	virtual void SetValue(FILE* file, const std::string& setting) override;

private:
	std::function<void(void*)> m_Callback;
	void* m_Arguments;
	mutable uint32_t m_BangDisplay;
};

class GraphicsContext;

namespace EngineTuning
{
	void Initialize(void);
	void Update(float frameTime);
	void Display(GraphicsContext& Context, float x, float y, float w, float h);
}