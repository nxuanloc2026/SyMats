// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
//
// Minimal dependency-free test harness (can be swapped for Catch2 later).
//
//   TEST_CASE("name") { CHECK(cond); CHECK_EQ(a, b); CHECK_THROWS(expr); }
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace symats_test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void report(const char* file, int line, const std::string& msg) {
    ++failures();
    std::cerr << file << ":" << line << ": FAILED: " << msg << "\n";
}

template <class T>
std::string show(const T& v) {
    if constexpr (requires { v.to_string(); }) {
        return v.to_string();
    } else if constexpr (requires { v.str(); }) {
        return v.str();
    } else {
        std::ostringstream os;
        os << v;
        return os.str();
    }
}

}  // namespace symats_test

#define SYMATS_CAT2(a, b) a##b
#define SYMATS_CAT(a, b) SYMATS_CAT2(a, b)

#define TEST_CASE(name)                                                                      \
    void SYMATS_CAT(symats_test_fn_, __LINE__)();                                            \
    static ::symats_test::Registrar SYMATS_CAT(symats_test_reg_, __LINE__)(                  \
        name, &SYMATS_CAT(symats_test_fn_, __LINE__));                                       \
    void SYMATS_CAT(symats_test_fn_, __LINE__)()

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        if (!(cond)) ::symats_test::report(__FILE__, __LINE__, "CHECK(" #cond ")");         \
    } while (0)

#define CHECK_EQ(a, b)                                                                       \
    do {                                                                                     \
        const auto symats_a_ = (a); /* copies: (a) may reference a temporary */             \
        const auto symats_b_ = (b);                                                          \
        if (!(symats_a_ == symats_b_))                                                       \
            ::symats_test::report(__FILE__, __LINE__,                                        \
                                  "CHECK_EQ(" #a ", " #b ")\n    left:  " +                  \
                                      ::symats_test::show(symats_a_) +                      \
                                      "\n    right: " + ::symats_test::show(symats_b_));    \
    } while (0)

#define CHECK_THROWS(expr)                                                                   \
    do {                                                                                     \
        bool symats_threw_ = false;                                                          \
        try {                                                                                \
            (void)(expr);                                                                    \
        } catch (...) {                                                                      \
            symats_threw_ = true;                                                            \
        }                                                                                    \
        if (!symats_threw_) ::symats_test::report(__FILE__, __LINE__, "CHECK_THROWS(" #expr ")"); \
    } while (0)
