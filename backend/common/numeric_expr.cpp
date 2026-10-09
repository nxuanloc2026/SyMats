// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "numeric_expr.h"

#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/zeta.hpp>

namespace symats::numeric {
namespace {
// Poles, domain errors and overflow give NaN/inf (like <cmath>) instead of throwing, so a
// plot or sample point outside the domain is just a gap.
using namespace boost::math::policies;
using Quiet = policy<domain_error<ignore_error>, pole_error<ignore_error>, overflow_error<ignore_error>,
                     evaluation_error<ignore_error>, promote_double<false>>;
const Quiet quiet{};
}  // namespace

const Functions& special_functions() {
    static const Functions table = [] {
        Functions t = elementary_functions();
        t.unary.insert({
            {"Gamma", [](double x) { return boost::math::tgamma(x, quiet); }},
            {"LogGamma", [](double x) { return boost::math::lgamma(x, quiet); }},
            {"Erf", [](double x) { return boost::math::erf(x, quiet); }},
            {"Erfc", [](double x) { return boost::math::erfc(x, quiet); }},
            {"Zeta", [](double x) { return boost::math::zeta(x, quiet); }},
        });
        t.binary.insert({
            {"Gamma", [](double a, double x) { return boost::math::tgamma(a, x, quiet); }},
            {"Beta", [](double a, double b) { return boost::math::beta(a, b, quiet); }},
            {"BesselJ", [](double n, double x) { return boost::math::cyl_bessel_j(n, x, quiet); }},
            {"BesselY", [](double n, double x) { return boost::math::cyl_neumann(n, x, quiet); }},
            {"BesselI", [](double n, double x) { return boost::math::cyl_bessel_i(n, x, quiet); }},
            {"BesselK", [](double n, double x) { return boost::math::cyl_bessel_k(n, x, quiet); }},
        });
        return t;
    }();
    return table;
}

}  // namespace symats::numeric
