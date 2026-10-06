// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "giac.h"
#include "static_extern.h"

int main() {
  static_assert(sizeof(giac::gen) == sizeof(giac::alias_gen));
  const giac::gen sum = giac::gen(2) + giac::gen(3);
  giac::context context;
  const giac::gen x(giac::identificateur("x"));
  const giac::gen factored = giac::_factor(x*x - giac::gen(1), &context);
  if (sum != giac::gen(5) || giac::is_undef(factored)) return 1;
  const giac::gen roots = giac::_solve(giac::makesequence(x - giac::gen(2), x), &context);
  if (roots.type != giac::_VECT || roots.ref_VECTptr()->size() != 1 ||
      roots.ref_VECTptr()->front() != giac::gen(2)) return 2;
  const giac::gen matrix(giac::makevecteur(
      giac::gen(giac::makevecteur(1, 2)), giac::gen(giac::makevecteur(3, 4))));
  return giac::_det(matrix, &context) == giac::gen(-2) ? 0 : 3;
}
