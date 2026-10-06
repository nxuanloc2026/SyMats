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
  if (giac::_det(matrix, &context) != giac::gen(-2)) return 3;
  if (giac::normal(giac::_charpoly(giac::makesequence(matrix,x), &context)-(x*x-5*x-2),
                   &context) != giac::gen(0)) return 4;
  const giac::gen nilpotent(giac::makevecteur(
      giac::gen(giac::makevecteur(0,1)),giac::gen(giac::makevecteur(0,0))));
  const giac::gen exponential(giac::analytic_apply(giac::at_exp,*nilpotent.ref_VECTptr(),&context));
  const giac::gen expected(giac::makevecteur(
      giac::gen(giac::makevecteur(1,1)),giac::gen(giac::makevecteur(0,1))));
  if (exponential != expected) return 5;
  const giac::gen diagonal(giac::makevecteur(
      giac::gen(giac::makevecteur(1,0)),giac::gen(giac::makevecteur(0,2))));
  const auto eigenvalues = (*giac::at_eigenvalues)(diagonal,&context);
  if (eigenvalues.type != giac::_VECT || eigenvalues.ref_VECTptr()->size()!=2 ||
      (*eigenvalues.ref_VECTptr())[0]+(*eigenvalues.ref_VECTptr())[1]!=giac::gen(3)) return 6;
  const giac::gen y(giac::identificateur("y"));
  const giac::gen yx(giac::symbolic(*giac::at_of,giac::makesequence(y,x)));
  const giac::gen derivative(giac::symbolic(*giac::at_derive,giac::makesequence(yx,x,2)));
  const auto solution = giac::_desolve(giac::makesequence(derivative+yx,x,y),&context);
  if (giac::is_undef(solution)) return 7;
  const giac::gen t(giac::identificateur("t"));
  const auto sine = giac::_ilaplace(giac::makesequence(giac::gen(1)/(1+x*x),x,t),&context);
  const auto cosine = giac::_ilaplace(giac::makesequence(x/(1+x*x),x,t),&context);
  return sine==giac::sin(t,&context) && cosine==giac::cos(t,&context) ? 0 : 8;
}
