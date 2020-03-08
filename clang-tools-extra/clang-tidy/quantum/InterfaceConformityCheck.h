//===--- InterfaceConformityCheck.h - clang-tidy ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_QUANTUM_INTERFACECONFORMITYCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_QUANTUM_INTERFACECONFORMITYCHECK_H

#include "../ClangTidyCheck.h"

namespace clang {
namespace tidy {
namespace quantum {

/// Finds interfaces that break Quantum code guidelines.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/quantum-interface-conformity.html
class InterfaceConformityCheck : public ClangTidyCheck {
public:
  InterfaceConformityCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace quantum
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_QUANTUM_INTERFACECONFORMITYCHECK_H
