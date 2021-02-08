#pragma once

#include "pch.h"

namespace Utility
{
    inline void Print(const char* msg) { fmt::print("{}", msg); }
    inline void Print(const wchar_t* msg) { fmt::print(L"{}", msg); }

    template <typename ...Args>
    inline void Printf(const char* format, Args... args)
    {
        fmt::print(format, args...);
    }

    template <typename ...Args>
    inline void Printf(const wchar_t* format, Args... args)
    {
        fmt::print(format, args...);
    }

#ifndef RELEASE
    template <typename ...Args>
    inline void PrintSubMessage(const char* format, Args... args)
    {
        fmt::print("--> ");
        fmt::print(format, args...);
        fmt::print("\n");
    }

    template <typename ...Args>
    inline void PrintSubMessage(const wchar_t* format, Args... args)
    {
        fmt::print("--> ");
        fmt::print(format, args...);
        fmt::print("\n");
    }

    inline void PrintSubMessage(void) {}
#endif
} // namespace Utility

#ifdef ERROR
#undef ERROR
#endif
#ifdef ASSERT
#undef ASSERT
#endif
#ifdef HALT
#undef HALT
#endif

#define HALT(...) ERROR(__VA_ARGS__) __debugbreak();

#ifdef RELEASE

    #define ASSERT(isTrue, ...) (void)(isTrue)
    #define WARN_ONCE_IF(isTrue, ...) (void)(isTrue)
    #define WARN_ONCE_IF_NOT(isTrue, ...) (void)(isTrue)
    #define ERROR(msg, ...)
    #define DEBUGPRINT(msg, ...) do {} while (0)
    #define ASSERT_SUCCCEEDED(hr, ...) (void)(hr)

#else // !RELEASE

    #define STRINGIFY(x) #x
    #define STRINGIFY_BUILTIN(x) STRINGIFY(x)
    #define ASSERT(isFalse, ...) \
        if (!(bool)(isFalse)) { \
            Utility::Print("\nAssertion failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("\'" #isFalse "\' is false"); \
            Utility::PrintSubMessage(__VA_ARGS__); \
            Utility::Print("\n"); \
            __debugbreak(); \
        }

    #define ASSERT_SUCCEEDED( hr, ... ) \
        if (FAILED(hr)) { \
            Utility::Print("\nHRESULT failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("hr = {:#x}", static_cast<HRESULT>(hr)); \
            Utility::PrintSubMessage(__VA_ARGS__); \
            Utility::PrintSubMessage("\n"); \
            __debugbreak(); \
        }

    #define WARN_ONCE_IF(isTrue, ...) \
    { \
        static bool s_TriggeredWarning = false; \
        if ((bool)(isTrue) && !s_TriggeredWarning) { \
            s_TriggeredWarning = true; \
            Utility::Print("\nWarning issued in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("\'" #isTrue "\' is true"); \
            Utility::PrintSubMessage(__VA_ARGS__); \
            Utility::Print("\n"); \
        } \
    }

    #define WARN_ONCE_IF_NOT(isTrue, ...) WARN_ONCE_IF(!(isTrue), __VA_ARGS__)

    #define ERROR(...) \
        Utility::Print("\nError reported in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
        Utility::PrintSubMessage(__VA_ARGS__); \
        Utility::Print("\n");

    #define DEBUGPRINT(msg, ...) \
    Utility::Printf(msg "\n", ##__VA_ARGS__);

#endif

void SIMDMemCopy(void* __restrict Dest, const void* __restrict Source, size_t NumQuadwords);
void SIMDMemFill(void* __restrict Dest, __m128 FillVector, size_t NumQuadwords);
