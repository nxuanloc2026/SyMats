// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "giac_backend.h"

#include "test.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
namespace {
int __cdecl trace_assert(int, char* message, int* response) {
    std::cerr << "CRT ASSERT: " << message << '\n';
    void* frames[32]{};
    const auto count = CaptureStackBackTrace(0, 32, frames, nullptr);
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(process, nullptr, TRUE);
    char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    for (USHORT i = 0; i < count; ++i) {
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymFromAddr(process, reinterpret_cast<DWORD64>(frames[i]), &displacement, symbol)) {
            std::cerr << "  " << symbol->Name;
            if (SymGetLineFromAddr64(process, reinterpret_cast<DWORD64>(frames[i]),
                                     &lineDisplacement, &line))
                std::cerr << " " << line.FileName << ':' << line.LineNumber;
            std::cerr << '\n';
        }
    }
    *response = 0;
    return TRUE;
}
}
#endif

using namespace symats;

TEST_CASE("Giac bridge preserves exact atoms and lists") {
    const ExprList cases = {
        make_integer(Integer::from_string("123456789012345678901234567890")),
        make_rational(7, 13), make_symbol("x"), make_symbol("Pi"),
        make_normal("List", {make_integer(1), make_symbol("y")}),
    };
    for (const auto& expr : cases) {
        auto result = giac_roundtrip(expr);
        CHECK(result.has_value());
        if (result) CHECK(equal(*result, expr));
    }
}

TEST_CASE("Giac bridge preserves mapped and generic heads") {
    const auto x = make_symbol("x");
    const ExprList cases = {
        plus({make_integer(2), x}),
        make_normal("Sin", {x}),
        make_normal("f", {x, make_integer(3)}),
        make_normal(make_normal("Derivative", {make_integer(1)}), {x}),
    };
    for (const auto& expr : cases) {
        auto result = giac_roundtrip(expr);
        CHECK(result.has_value());
        if (result) CHECK(equal(*result, expr));
    }
}

TEST_CASE("Giac backend supports symbolic factorization") {
    GiacBackend backend;
    CHECK(backend.supports("Factor"));
    CHECK(!backend.supports("Plot"));
    const auto x = make_symbol("x");
    const auto polynomial = subtract(power(x, make_integer(2)), make_integer(1));
    auto result = backend.evaluate(make_normal("Factor", {polynomial}));
    CHECK(result.has_value());
    if (result) {
        CHECK(result->value != nullptr);
        CHECK(result->status == ResultStatus::Unverified);
    }
}

TEST_CASE("Giac backend evaluates first symbolic operations") {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportHook(trace_assert);
#endif
    GiacBackend backend;
    const auto x = make_symbol("x");
    const ExprList cases = {
        make_normal("Integrate", {x, x}),
        make_normal("Limit", {x, make_normal("Rule", {x, make_integer(2)})}),
        make_normal("Series", {make_normal("Exp", {x}),
            make_normal("List", {x, make_integer(0), make_integer(3)})}),
        make_normal("Solve", {make_normal("Equal", {x, make_integer(2)}), x}),
        make_normal("Det", {make_normal("List", {
            make_normal("List", {make_integer(1), make_integer(2)}),
            make_normal("List", {make_integer(3), make_integer(4)})})}),
    };
    for (const auto& expr : cases) {
        auto result = backend.evaluate(expr);
        std::cerr << to_full_form(expr) << " -> "
                  << (result ? to_full_form(result->value) : "declined") << '\n';
        CHECK(result.has_value());
    }
}
