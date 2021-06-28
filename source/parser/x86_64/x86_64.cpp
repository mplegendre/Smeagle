// Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
// Spack Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <optional>
#include <regex>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "Function.h"
#include "Symtab.h"
#include "Type.h"
#include "allocators.hpp"
#include "smeagle/parameter.h"
#include "type_checker.hpp"

namespace smeagle::x86_64 {

  namespace st = Dyninst::SymtabAPI;

  // Dereference a pointer or reference
  static st::Type *deref(st::Type *t) {
    if (is_pointer(t->getDataClass())) {
      return deref(t->getPointerType()->getConstituentType());
    }
    if (is_ref(t->getDataClass())) {
      return deref(t->getRefType()->getConstituentType());
    }
    return t;
  }

  // Get a location offset for a variable
  // This function is not used because LocationLists are not reliable
  std::string getParamLocationOffset(st::localVar *param) {
    std::vector<Dyninst::VariableLocation> locs = param->getLocationLists();
    std::stringstream result;

    for (auto i = locs.begin(); i != locs.end(); ++i) {
      Dyninst::VariableLocation current = *i;

      // We only want to know where parameter is at the entrypoint
      result << std::hex << current.lowPC << " to " << current.hiPC << " " << current.mr_reg.name()
             << " " << std::dec << current.frameOffset;

      break;
    }

    return result.str();
  }

  // Unwrap and remove typedef
  static st::Type *remove_typedef(st::Type *t) {
    if (is_typedef(t->getDataClass())) {
      t = t->getTypedefType()->getConstituentType();
    }
    return t;
  };

  // Get directionality from argument type
  std::string getDirectionalityFromType(st::Type *paramType) {
    // Remove any top-level typedef
    paramType = remove_typedef(paramType);
    auto dataClass = paramType->getDataClass();

    // Any type passed by value is imported
    if (!is_indirect(dataClass)) {
      return "import";
    }

    // Remove any reference or pointer indirection
    paramType = deref(paramType);

    // Remove any remaining typedef
    paramType = remove_typedef(paramType);
    dataClass = paramType->getDataClass();

    // A pointer/reference to a primitive is imported
    if (is_primitive(dataClass)) {
      return "import";
    }

    // Passed by pointer or reference and not primitive, value is unknown
    return "unknown";
  }

  // Get register class given the argument type
  std::pair<RegisterClass, RegisterClass> getRegisterClassFromType(st::Type *paramType) {
    // Remove top-level typedef
    paramType = remove_typedef(paramType);

    // If it's a pointer, remove it
    if (is_indirect(paramType->getDataClass())) {
      paramType = deref(paramType);
      paramType = remove_typedef(paramType);
    }

    // Get string to compare to
    std::string paramTypeString = paramType->getName();

    // Is it a constant?
    std::regex checkconstant("(const)");

    // The 64-bit mantissa of arguments of type long double belongs to classX87
    // the 16-bit exponent plus 6 bytes of padding belongs to class X87UP
    std::regex checklongdouble("(long|double)");

    // A variable of type complex long double is classified as type COMPLEX_X87
    std::regex checkcomplexlong("(complex long double)");

    // Does the type string match one of the types?
    bool isconst = (std::regex_search(paramTypeString, checkconstant));
    bool islongdouble = (std::regex_search(paramTypeString, checklongdouble));
    bool iscomplexlong = (std::regex_search(paramTypeString, checkcomplexlong));

    // We will return a vector of classes
    std::pair<RegisterClass, RegisterClass> regClasses;

    // A parameter can have more than one class!
    if (isconst) {
      regClasses = {RegisterClass::MEMORY, RegisterClass::NO_CLASS};
    }

    // __float128 and__m128are split into two halves, least significant SSE,
    // most significant in SSEUP.
    if (paramType->is_complex_float) {
      regClasses = {RegisterClass::SSE, RegisterClass::SSEUP};
    }

    // float,double,_Decimal32,_Decimal64and__m64are in class SSE.
    if (paramType->is_float) {
      regClasses = {RegisterClass::SSE, RegisterClass::NO_CLASS};
    }

    // Signed and unsigned Bool,char,short,int,long,long long, and pointers
    if ((paramType->is_integral) || (paramType->is_boolean) || paramType->is_address) {
      regClasses = {RegisterClass::INTEGER, RegisterClass::NO_CLASS};    
    }

    if (iscomplexlong) {
      regClasses = {RegisterClass::COMPLEX_X87, RegisterClass::NO_CLASS};
    }
    if (islongdouble) {
      regClasses = {RegisterClass::X87, RegisterClass::X87UP};
    }

    // The classification of aggregate (structures and arrays) and union types worksas follows:
    // TODO need to look for struct / arrays?
    // page 18 of abi document
    return regClasses;
  }

  std::vector<parameter> parse_parameters(st::Symbol *symbol) {
    // Get the name and type of the symbol
    std::string sname = symbol->getMangledName();
    st::Function *func = symbol->getFunction();
    std::vector<st::localVar *> params;

    // The function name looks equivalent to the symbol name
    std::string fname = func->getName();

    std::vector<parameter> typelocs;

    // Get parameters with types and names
    if (func->getParams(params)) {
      // Create a RegisterAllocator lookup class
      RegisterAllocator allocator;

      for (auto &param : params) {
        std::string paramName = param->getName();
        st::Type *paramType = param->getType();

        // Get register class based on type
        std::pair<RegisterClass, RegisterClass> regClasses = getRegisterClassFromType(paramType);

        // Get the directionality (export or import) given the type
        std::string direction = getDirectionalityFromType(paramType);

        // This uses location lists, not reliable
        // std::string locoffset = getParamLocationOffset(param);

        // Create a new typelocation to parse later
        parameter p;
        p.name = paramName;
        p.type = paramType->getName();
        p.location = allocator.getRegistersString(regClasses, paramType);
        p.direction = direction;
        typelocs.push_back(p);
      }
    }
    return typelocs;
  }

  parameter parse_return_value(Dyninst::SymtabAPI::Symbol const *) { return {}; }

}  // namespace smeagle::x86_64
