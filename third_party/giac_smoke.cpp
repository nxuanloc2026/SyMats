// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

#include "gen.h"

int main() {
  const giac::gen sum = giac::gen(2) + giac::gen(3);
  return sum == giac::gen(5) ? 0 : 1;
}
