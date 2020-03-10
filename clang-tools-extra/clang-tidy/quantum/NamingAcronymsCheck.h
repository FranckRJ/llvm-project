//===--- NamingAcronymsCheck.h - clang-tidy ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_QUANTUM_NAMINGACRONYMSCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_QUANTUM_NAMINGACRONYMSCHECK_H

#include "../ClangTidyCheck.h"

namespace clang {
namespace tidy {
namespace quantum {

/// Check that acronyms aren't writed in uppercase.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/quantum-naming-acronyms.html
class NamingAcronymsCheck : public ClangTidyCheck {
public:
  NamingAcronymsCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace quantum
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_QUANTUM_NAMINGACRONYMSCHECK_H
