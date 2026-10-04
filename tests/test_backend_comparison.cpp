// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include <iostream>
#include <string>
#include <vector>

#include "symats/expr.h"
#include "symats/text.h"
#include "test.h"

namespace {

struct ComparisonTestCase {
    std::string id;
    std::string suite;       // "Rubi" or "SymPy"
    std::string license;     // "MIT" or "BSD-3-Clause"
    std::string category;    // "Integration", "Differentiation", "Expansion", "LinearAlgebra", etc.
    std::string input;       // Input expression in plain text
    std::string expected;    // Expected result in plain text
};

struct ComparisonReport {
    int total = 0;
    int passed = 0;
    int mismatched = 0;
    std::vector<std::string> mismatch_details;
};

// Evaluate an expression string (currently parses text into Expr; when backend/evaluator is present, evaluates).
symats::ExprPtr evaluate_expression(const std::string& input_text) {
    symats::ExprPtr expr = symats::parse_text(input_text);
    // Future expansion: pass `expr` to evaluator / MathBackend once T-001/T-012 are completed.
    return expr;
}

ComparisonReport run_comparison_suite(const std::vector<ComparisonTestCase>& cases, bool strict_eval = false) {
    ComparisonReport report;
    for (const auto& tc : cases) {
        report.total++;
        try {
            symats::ExprPtr input_expr = symats::parse_text(tc.input);
            symats::ExprPtr expected_expr = symats::parse_text(tc.expected);

            symats::ExprPtr result_expr = evaluate_expression(tc.input);

            if (symats::equal(result_expr, expected_expr)) {
                report.passed++;
            } else {
                report.mismatched++;
                std::string detail = "[" + tc.suite + "/" + tc.category + " - " + tc.id + "]\n" +
                                     "  Input:    " + tc.input + "\n" +
                                     "  Expected: " + tc.expected + " (" + symats::to_full_form(expected_expr) + ")\n" +
                                     "  Actual:   " + symats::to_text(result_expr) + " (" + symats::to_full_form(result_expr) + ")";
                report.mismatch_details.push_back(detail);
            }
        } catch (const std::exception& e) {
            report.mismatched++;
            std::string detail = "[" + tc.suite + "/" + tc.category + " - " + tc.id + "]\n" +
                                 "  Input:     " + tc.input + "\n" +
                                 "  Exception: " + std::string(e.what());
            report.mismatch_details.push_back(detail);
        }
    }

    if (report.mismatched > 0) {
        std::cout << "\n=== Backend Comparison Suite Mismatch Report (" << cases.front().suite << ") ===\n";
        std::cout << "Total: " << report.total << " | Passed: " << report.passed
                  << " | Mismatched: " << report.mismatched << "\n";
        for (const auto& detail : report.mismatch_details) {
            std::cout << "----------------------------------------\n" << detail << "\n";
        }
        std::cout << "====================================================\n\n";
    }

    if (strict_eval) {
        CHECK_EQ(report.mismatched, 0);
    }

    return report;
}

const std::vector<ComparisonTestCase> rubi_integral_cases = {
    {"RUBI-001", "Rubi", "MIT", "Integration", "Integrate(x^2, x)", "1/3 * x^3"},
    {"RUBI-002", "Rubi", "MIT", "Integration", "Integrate(1/x, x)", "Log(x)"},
    {"RUBI-003", "Rubi", "MIT", "Integration", "Integrate(Sin(x), x)", "-Cos(x)"},
    {"RUBI-004", "Rubi", "MIT", "Integration", "Integrate(Cos(x), x)", "Sin(x)"},
    {"RUBI-005", "Rubi", "MIT", "Integration", "Integrate(Exp(x), x)", "Exp(x)"},
    {"RUBI-006", "Rubi", "MIT", "Integration", "Integrate(1/(1 + x^2), x)", "ArcTan(x)"},
    {"RUBI-007", "Rubi", "MIT", "Integration", "Integrate(x * Exp(x), x)", "x * Exp(x) - Exp(x)"},
    {"RUBI-008", "Rubi", "MIT", "Integration", "Integrate(1/(a + b*x), x)", "1/b * Log(a + b*x)"},
    {"RUBI-009", "Rubi", "MIT", "Integration", "Integrate(x/(1 + x^2), x)", "1/2 * Log(1 + x^2)"},
    {"RUBI-010", "Rubi", "MIT", "Integration", "Integrate((1 - x^2)^(-1/2), x)", "ArcSin(x)"}
};

const std::vector<ComparisonTestCase> sympy_cases = {
    {"SYMPY-001", "SymPy", "BSD-3-Clause", "Differentiation", "D(x^3, x)", "3 * x^2"},
    {"SYMPY-002", "SymPy", "BSD-3-Clause", "Differentiation", "D(Sin(x), x)", "Cos(x)"},
    {"SYMPY-003", "SymPy", "BSD-3-Clause", "Differentiation", "D(Exp(x^2), x)", "2 * x * Exp(x^2)"},
    {"SYMPY-004", "SymPy", "BSD-3-Clause", "Expansion", "Expand((x + 1)^2)", "x^2 + 2*x + 1"},
    {"SYMPY-005", "SymPy", "BSD-3-Clause", "Expansion", "Expand((x + y)*(x - y))", "x^2 - y^2"},
    {"SYMPY-006", "SymPy", "BSD-3-Clause", "Factorization", "Factor(x^2 - 1)", "(x - 1)*(x + 1)"},
    {"SYMPY-007", "SymPy", "BSD-3-Clause", "Simplification", "Simplify(Sin(x)^2 + Cos(x)^2)", "1"},
    {"SYMPY-008", "SymPy", "BSD-3-Clause", "LinearAlgebra", "Det([[a, b], [c, d]])", "a*d - b*c"},
    {"SYMPY-009", "SymPy", "BSD-3-Clause", "LinearAlgebra", "Trace([[1, 2], [3, 4]])", "5"},
    {"SYMPY-010", "SymPy", "BSD-3-Clause", "Solving", "Solve(x^2 - 4 == 0, x)", "{{x -> -2}, {x -> 2}}"}
};

// Maxima oracle cases (Source: Maxima 5.47.0 - GPL reference system)
const std::vector<ComparisonTestCase> maxima_cases = {
    {"MAXIMA-001", "Maxima", "GPL-2.0-or-later", "Integration", "Integrate(x * Sin(x), x)", "Sin(x) - x * Cos(x)"},
    {"MAXIMA-002", "Maxima", "GPL-2.0-or-later", "Differentiation", "D(x^4 + 3*x^2, x)", "4*x^3 + 6*x"},
    {"MAXIMA-003", "Maxima", "GPL-2.0-or-later", "Expansion", "Expand((a + b)^3)", "a^3 + 3*a^2*b + 3*a*b^2 + b^3"},
    {"MAXIMA-004", "Maxima", "GPL-2.0-or-later", "Factorization", "Factor(x^3 - 8)", "(x - 2)*(x^2 + 2*x + 4)"},
    {"MAXIMA-005", "Maxima", "GPL-2.0-or-later", "Limit", "Limit(Sin(x)/x, x, 0)", "1"},
    {"MAXIMA-006", "Maxima", "GPL-2.0-or-later", "Solving", "Solve(x^2 - 9 == 0, x)", "{{x -> -3}, {x -> 3}}"},
    {"MAXIMA-007", "Maxima", "GPL-2.0-or-later", "LinearAlgebra", "Det([[1, 2], [3, 4]])", "-2"},
    {"MAXIMA-008", "Maxima", "GPL-2.0-or-later", "DSolve", "DSolve(D(y(x), x) == y(x), y(x), x)", "{{y(x) -> C_1 * Exp(x)}}"}
};

}  // namespace

TEST_CASE("rubi_integral_comparison_suite") {
    // Parse verification for all Rubi test cases.
    for (const auto& tc : rubi_integral_cases) {
        symats::ExprPtr input_expr = symats::parse_text(tc.input);
        symats::ExprPtr expected_expr = symats::parse_text(tc.expected);
        CHECK(input_expr != nullptr);
        CHECK(expected_expr != nullptr);
    }

    // Run suite and report mismatches.
    // Currentlyunevaluated heads (like Integrate) will be reported as mismatches until backends (T-001/T-012) are implemented.
    ComparisonReport report = run_comparison_suite(rubi_integral_cases, /*strict_eval=*/false);
    CHECK_EQ(report.total, static_cast<int>(rubi_integral_cases.size()));
}

TEST_CASE("sympy_comparison_suite") {
    // Parse verification for all SymPy test cases.
    for (const auto& tc : sympy_cases) {
        symats::ExprPtr input_expr = symats::parse_text(tc.input);
        symats::ExprPtr expected_expr = symats::parse_text(tc.expected);
        CHECK(input_expr != nullptr);
        CHECK(expected_expr != nullptr);
    }

    // Run suite and report mismatches.
    ComparisonReport report = run_comparison_suite(sympy_cases, /*strict_eval=*/false);
    CHECK_EQ(report.total, static_cast<int>(sympy_cases.size()));
}

TEST_CASE("maxima_comparison_suite") {
    // Parse verification for all Maxima oracle test cases.
    for (const auto& tc : maxima_cases) {
        symats::ExprPtr input_expr = symats::parse_text(tc.input);
        symats::ExprPtr expected_expr = symats::parse_text(tc.expected);
        CHECK(input_expr != nullptr);
        CHECK(expected_expr != nullptr);
    }

    // Run suite and report mismatches.
    ComparisonReport report = run_comparison_suite(maxima_cases, /*strict_eval=*/false);
    CHECK_EQ(report.total, static_cast<int>(maxima_cases.size()));
}
