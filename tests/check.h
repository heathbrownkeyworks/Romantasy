#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

struct TestCase
{
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& Tests()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int g_failures = 0;

struct TestRegistrar
{
    TestRegistrar(const char* name, std::function<void()> fn)
    {
        Tests().push_back({ name, std::move(fn) });
    }
};

#define TEST(name)                                        \
    static void name();                                   \
    static TestRegistrar name##_registrar(#name, name);   \
    static void name()

// A function, not an inline `if`, so MSVC never emits C4127 (constant
// condition) when a constexpr expression is checked.
inline void CheckImpl(bool ok, const char* file, int line, const char* expr)
{
    if (!ok) {
        std::printf("  FAIL %s:%d: %s\n", file, line, expr);
        ++g_failures;
    }
}

#define CHECK(expr) CheckImpl(static_cast<bool>(expr), __FILE__, __LINE__, #expr)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(static_cast<double>((a) - (b))) <= (eps))
