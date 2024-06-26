// ------------------------------------------------------------------------
//
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2019 - 2020 by the deal.II authors
//
// This file is part of the deal.II library.
//
// Part of the source code is dual licensed under Apache-2.0 WITH
// LLVM-exception OR LGPL-2.1-or-later. Detailed license information
// governing the source code and code contributions can be found in
// LICENSE.md and CONTRIBUTING.md at the top level directory of deal.II.
//
// ------------------------------------------------------------------------


// Check that functions to add scalar variables to symbol maps, and subsequently
// set their associated values, work correctly.

#include <deal.II/differentiation/sd.h>

#include <complex>
#include <iostream>
#include <string>

#include "../tests.h"

namespace SD = Differentiation::SD;
namespace SE = ::SymEngine;

int
main()
{
  initlog();

  deallog << "Construct symbol map" << std::endl;
  SD::types::substitution_map symbol_map;
  SD::add_to_symbol_map(symbol_map, SD::Expression("x1"));
  SD::add_to_symbol_map(symbol_map,
                        SD::types::symbol_vector{SD::Expression("x2"),
                                                 SD::Expression("x3")});
  // Add via map, but check that associated values are ignored.
  SD::add_to_symbol_map(
    symbol_map,
    SD::types::substitution_map{{SD::Expression("x4"), SD::Expression(4)},
                                {SD::Expression("x5"), SD::Expression(5)},
                                {SD::Expression("x6"), SD::Expression(6)}});
  SD::add_to_symbol_map(
    symbol_map,
    SD::Expression("x7"),
    SD::types::symbol_vector{SD::Expression("x8"), SD::Expression("x9")},
    SD::types::substitution_map{{SD::Expression("x10"), SD::Expression(10)},
                                {SD::Expression("x11"), SD::Expression(11)},
                                {SD::Expression("x12"), SD::Expression(12)}});

  SD::Utilities::print_substitution_map(deallog, symbol_map);


  deallog << "Set values in symbol map" << std::endl;
  SD::set_value_in_symbol_map(symbol_map,
                              SD::Expression("x1"),
                              SD::Expression(1));
  SD::set_value_in_symbol_map(symbol_map,
                              SD::Expression("x2"),
                              SD::Expression(2.0));
  SD::set_value_in_symbol_map(symbol_map,
                              SD::Expression("x3"),
                              std::complex<double>(3.0));
  SD::set_value_in_symbol_map(
    symbol_map,
    SD::types::symbol_vector{SD::Expression("x4"), SD::Expression("x5")},
    SD::types::symbol_vector{SD::Expression(4), SD::Expression(5.0)});
  SD::set_value_in_symbol_map(symbol_map,
                              SD::types::symbol_vector{SD::Expression("x6"),
                                                       SD::Expression("x7")},
                              std::vector<double>{6.0, 7.0});
  SD::set_value_in_symbol_map(symbol_map,
                              std::make_pair(SD::Expression("x8"), 8.0));
  SD::set_value_in_symbol_map(symbol_map,
                              std::vector<std::pair<SD::Expression, double>>{
                                std::make_pair(SD::Expression("x9"), 9.0),
                                std::make_pair(SD::Expression("x10"), 10.0)});
  SD::set_value_in_symbol_map(
    symbol_map,
    SD::types::substitution_map{{SD::Expression("x10"), SD::Expression(10)},
                                {SD::Expression("x11"), SD::Expression(11)},
                                {SD::Expression("x12"), SD::Expression(12)}});

  SD::Utilities::print_substitution_map(deallog, symbol_map);

  deallog << "OK" << std::endl;
}
