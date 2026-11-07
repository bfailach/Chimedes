#pragma once

// Minimal, dependency-free test framework, so the core can be tested
// without building JUCE. Usage:
//
//   TEST ("test name") { CHECK (a == b); CHECK_NEAR (x, 1.5); }

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace chimedes::test
{

struct TestCase
{
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount()
{
    static int failures = 0;
    return failures;
}

struct Registrar
{
    Registrar (std::string name, std::function<void()> body)
    {
        registry().push_back ({ std::move (name), std::move (body) });
    }
};

inline void reportFailure (const char* file, int line, const std::string& message)
{
    ++failureCount();
    std::cerr << "    FAILED " << file << ":" << line << "  " << message << "\n";
}

} // namespace chimedes::test

#define CHIMEDES_CONCAT_INNER(a, b) a##b
#define CHIMEDES_CONCAT(a, b) CHIMEDES_CONCAT_INNER (a, b)

#define TEST(name)                                                                         \
    static void CHIMEDES_CONCAT (testBody_, __LINE__)();                                   \
    static const chimedes::test::Registrar CHIMEDES_CONCAT (testRegistrar_, __LINE__) {    \
        name, CHIMEDES_CONCAT (testBody_, __LINE__) };                                     \
    static void CHIMEDES_CONCAT (testBody_, __LINE__)()

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (! (condition))                                                                 \
            chimedes::test::reportFailure (__FILE__, __LINE__, #condition);                \
    } while (false)

#define CHECK_NEAR(actual, expected)                                                       \
    do {                                                                                   \
        const double chimedesActual = (actual);                                            \
        const double chimedesExpected = (expected);                                        \
        if (std::abs (chimedesActual - chimedesExpected) > 1e-9)                           \
            chimedes::test::reportFailure (__FILE__, __LINE__,                             \
                std::string (#actual) + " = " + std::to_string (chimedesActual)            \
                + ", expected " + std::to_string (chimedesExpected));                      \
    } while (false)

// Stops the current test if the condition fails (so it does not go on with invalid data).
#define REQUIRE(condition)                                                                 \
    do {                                                                                   \
        if (! (condition)) {                                                               \
            chimedes::test::reportFailure (__FILE__, __LINE__, #condition);                \
            return;                                                                        \
        }                                                                                  \
    } while (false)
