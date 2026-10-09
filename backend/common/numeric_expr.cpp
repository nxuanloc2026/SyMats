// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "numeric_expr.h"

#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/zeta.hpp>

namespace symats::numeric {

const Functions& special_functions() {
    static const Functions table = [] {
        Functions t = elementary_functions();
        t.unary.insert({
            {"Gamma", [](double x) { return boost::math::tgamma(x); }},
            {"LogGamma", [](double x) { return boost::math::lgamma(x); }},
            {"Erf", [](double x) { return boost::math::erf(x); }},
            {"Erfc", [](double x) { return boost::math::erfc(x); }},
            {"Zeta", [](double x) { return boost::math::zeta(x); }},
        });
        t.binary.insert({
            {"Gamma", [](double a, double x) { return boost::math::tgamma(a, x); }},
            {"Beta", [](double a, double b) { return boost::math::beta(a, b); }},
            {"BesselJ", [](double n, double x) { return boost::math::cyl_bessel_j(n, x); }},
            {"BesselY", [](double n, double x) { return boost::math::cyl_neumann(n, x); }},
            {"BesselI", [](double n, double x) { return boost::math::cyl_bessel_i(n, x); }},
            {"BesselK", [](double n, double x) { return boost::math::cyl_bessel_k(n, x); }},
        });
        return t;
    }();
    return table;
}

}  // namespace symats::numeric
