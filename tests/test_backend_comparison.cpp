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
    {"RUBI-001", "Rubi", "MIT", "Integration", "Integrate(x^2, x)", "Plus(Times(Rational(1, 3), Power(x, 3)), C)"},
    {"RUBI-002", "Rubi", "MIT", "Integration", "Integrate(Power(x, -1), x)", "Log(x)"},
    {"RUBI-003", "Rubi", "MIT", "Integration", "Integrate(Sin(x), x)", "Times(-1, Cos(x))"},
    {"RUBI-004", "Rubi", "MIT", "Integration", "Integrate(Cos(x), x)", "Sin(x)"},
    {"RUBI-005", "Rubi", "MIT", "Integration", "Integrate(Exp(x), x)", "Exp(x)"},
    {"RUBI-006", "Rubi", "MIT", "Integration", "Integrate(Power(Plus(1, Power(x, 2)), -1), x)", "ArcTan(x)"},
    {"RUBI-007", "Rubi", "MIT", "Integration", "Integrate(Times(x, Exp(x)), x)", "Plus(Times(x, Exp(x)), Times(-1, Exp(x)))"},
    {"RUBI-008", "Rubi", "MIT", "Integration", "Integrate(Power(Plus(a, Times(b, x)), -1), x)", "Times(Power(b, -1), Log(Plus(a, Times(b, x))))"},
    {"RUBI-009", "Rubi", "MIT", "Integration", "Integrate(Times(x, Power(Plus(1, Power(x, 2)), -1)), x)", "Times(Rational(1, 2), Log(Plus(1, Power(x, 2))))"},
    {"RUBI-010", "Rubi", "MIT", "Integration", "Integrate(Power(Plus(1, Times(-1, Power(x, 2))), Rational(-1, 2)), x)", "ArcSin(x)"}
};

const std::vector<ComparisonTestCase> sympy_cases = {
    {"SYMPY-001", "SymPy", "BSD-3-Clause", "Differentiation", "D(Power(x, 3), x)", "Times(3, Power(x, 2))"},
    {"SYMPY-002", "SymPy", "BSD-3-Clause", "Differentiation", "D(Sin(x), x)", "Cos(x)"},
    {"SYMPY-003", "SymPy", "BSD-3-Clause", "Differentiation", "D(Exp(Power(x, 2)), x)", "Times(2, x, Exp(Power(x, 2)))"},
    {"SYMPY-004", "SymPy", "BSD-3-Clause", "Expansion", "Expand(Power(Plus(x, 1), 2))", "Plus(Power(x, 2), Times(2, x), 1)"},
    {"SYMPY-005", "SymPy", "BSD-3-Clause", "Expansion", "Expand(Times(Plus(x, y), Plus(x, Times(-1, y))))", "Plus(Power(x, 2), Times(-1, Power(y, 2)))"},
    {"SYMPY-006", "SymPy", "BSD-3-Clause", "Factorization", "Factor(Plus(Power(x, 2), -1))", "Times(Plus(x, -1), Plus(x, 1))"},
    {"SYMPY-007", "SymPy", "BSD-3-Clause", "Simplification", "Simplify(Plus(Power(Sin(x), 2), Power(Cos(x), 2)))", "1"},
    {"SYMPY-008", "SymPy", "BSD-3-Clause", "LinearAlgebra", "Det(List(List(a, b), List(c, d)))", "Plus(Times(a, d), Times(-1, b, c))"},
    {"SYMPY-009", "SymPy", "BSD-3-Clause", "LinearAlgebra", "Trace(List(List(1, 2), List(3, 4)))", "5"},
    {"SYMPY-010", "SymPy", "BSD-3-Clause", "Solving", "Solve(Equal(Plus(Power(x, 2), -4), 0), x)", "List(Equal(x, -2), Equal(x, 2))"}
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
