//===-- CGBuilder.h - Choose IRBuilder implementation  ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_CGBUILDER_H
#define CLANG_CODEGEN_CGBUILDER_H

#include "llvm/IRBuilder.h"
#include "SeeCBuilder.h"

namespace clang {
namespace CodeGen {

// Don't preserve names on values in an optimized build.
#ifdef NDEBUG
typedef seec::SeeCIRBuilder<false> CGBuilderTy;
#else
typedef seec::SeeCIRBuilder<> CGBuilderTy;
#endif

}  // end namespace CodeGen
}  // end namespace clang

#endif
