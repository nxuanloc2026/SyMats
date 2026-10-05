// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "giac.h"
#include "static_extern.h"

int main() {
  const giac::gen sum = giac::gen(2) + giac::gen(3);
  giac::context context;
  const giac::gen x(giac::identificateur("x"));
  const giac::gen factored = giac::_factor(x*x - giac::gen(1), &context);
  return sum == giac::gen(5) && !giac::is_undef(factored) ? 0 : 1;
}
