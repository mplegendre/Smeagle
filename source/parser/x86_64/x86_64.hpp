// Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
// Spack Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#pragma once

#include <vector>

#include "Symtab.h"
#include "smeagle/abi_description.h"
#include "smeagle/parameter.h"

namespace smeagle::x86_64 {

  std::vector<parameter> parse_inline(Dyninst::SymtabAPI::Function* func);
  std::vector<parameter> parse_callsites(Dyninst::SymtabAPI::Symbol* symbol);
  std::vector<parameter> parse_parameters(Dyninst::SymtabAPI::Symbol* symbol);
  parameter parse_return_value(Dyninst::SymtabAPI::Symbol const* symbol);
  smeagle::abi_variable_description parse_variable(Dyninst::SymtabAPI::Symbol* symbol);
}  // namespace smeagle::x86_64
