//===--- QuantumTidyModule.cpp - clang-tidy ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "InterfaceConformityCheck.h"
#include "NamingAcronymsCheck.h"

namespace clang {
namespace tidy {
namespace quantum {

class QuantumModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<InterfaceConformityCheck>(
        "quantum-interface-conformity");
    CheckFactories.registerCheck<NamingAcronymsCheck>(
        "quantum-naming-acronyms");
  }
};

// Register the QuantumModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<QuantumModule>
    X("quantum-module", "Adds quantum coding guidelines checks.");

} // namespace quantum

// This anchor is used to force the linker to link in the generated object file
// and thus register the QuantumModule.
volatile int QuantumModuleAnchorSource = 0;

} // namespace tidy
} // namespace clang
