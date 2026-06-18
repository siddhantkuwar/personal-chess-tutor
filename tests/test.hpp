#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pct::test {

struct Case {
    std::string name;
    std::function<void()> run;
};

std::vector<Case>& registry();

struct Register {
    Register(std::string name, std::function<void()> run);
};

inline void fail(const char* expression, const char* file, int line, std::string detail = {}) {
    std::ostringstream message;
    message << file << ':' << line << ": check failed: " << expression;
    if (!detail.empty())
        message << " (" << detail << ')';
    throw std::runtime_error(message.str());
}

} // namespace pct::test

#define PCT_JOIN_DETAIL(a, b) a##b
#define PCT_JOIN(a, b) PCT_JOIN_DETAIL(a, b)
#define TEST_CASE(name)                                                                            \
    static void PCT_JOIN(test_, __LINE__)();                                                       \
    static ::pct::test::Register PCT_JOIN(reg_, __LINE__)(name, PCT_JOIN(test_, __LINE__));        \
    static void PCT_JOIN(test_, __LINE__)()

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression))                                                                         \
            ::pct::test::fail(#expression, __FILE__, __LINE__);                                    \
    } while (false)

#define CHECK_EQ(actual, expected)                                                                 \
    do {                                                                                           \
        const auto pct_actual = (actual);                                                          \
        const auto pct_expected = (expected);                                                      \
        if (!(pct_actual == pct_expected)) {                                                       \
            std::ostringstream pct_detail;                                                         \
            pct_detail << "actual=" << pct_actual << ", expected=" << pct_expected;                \
            ::pct::test::fail(#actual " == " #expected, __FILE__, __LINE__, pct_detail.str());     \
        }                                                                                          \
    } while (false)

#define CHECK_THROWS(expression)                                                                   \
    do {                                                                                           \
        bool pct_threw = false;                                                                    \
        try {                                                                                      \
            static_cast<void>(expression);                                                         \
        } catch (...) {                                                                            \
            pct_threw = true;                                                                      \
        }                                                                                          \
        if (!pct_threw)                                                                            \
            ::pct::test::fail("throws: " #expression, __FILE__, __LINE__);                         \
    } while (false)
