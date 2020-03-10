//===--- NamingAcronymsCheck.cpp - clang-tidy -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "NamingAcronymsCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace quantum {

void NamingAcronymsCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      cxxRecordDecl(isDefinition(), matchesName("(JSON|HTTP|DTO)[^:]*$"))
          .bind("record_decl"),
      this);
  Finder->addMatcher(
      fieldDecl(matchesName("(JSON|HTTP|DTO)[^:]*$")).bind("field_decl"), this);
  Finder->addMatcher(
      cxxMethodDecl(isDefinition(), matchesName("(JSON|HTTP|DTO)[^:]*$"))
          .bind("method_decl"),
      this);
  Finder->addMatcher(
      functionDecl(isDefinition(), matchesName("(JSON|HTTP|DTO)[^:]*$"))
          .bind("function_decl"),
      this);
  Finder->addMatcher(
      varDecl(isDefinition(), matchesName("(JSON|HTTP|DTO)[^:]*$"))
          .bind("var_decl"),
      this);
}

void NamingAcronymsCheck::check(const MatchFinder::MatchResult &Result) {
  if (const auto *Decl = Result.Nodes.getNodeAs<CXXRecordDecl>("record_decl")) {
    diag(Decl->getLocation(),
         "class %0 has a name with a SCREAMING CASE acronym")
        << Decl;
    return;
  }

  if (const auto *Decl = Result.Nodes.getNodeAs<FieldDecl>("field_decl")) {
    diag(Decl->getLocation(),
         "member %0 has a name with a SCREAMING CASE acronym")
        << Decl;
    return;
  }

  const auto *MethDecl = Result.Nodes.getNodeAs<CXXMethodDecl>("method_decl");
  if (MethDecl && !isa<CXXConstructorDecl>(MethDecl) &&
      !isa<CXXDestructorDecl>(MethDecl)) {
    diag(MethDecl->getLocation(),
         "method %0 has a name with a SCREAMING CASE acronym")
        << MethDecl;
    return;
  }

  const auto *FuncDecl = Result.Nodes.getNodeAs<FunctionDecl>("function_decl");
  if (FuncDecl && !isa<CXXMethodDecl>(FuncDecl)) {
    diag(FuncDecl->getLocation(),
         "function %0 has a name with a SCREAMING CASE acronym")
        << FuncDecl;
    return;
  }

  if (const auto *Decl = Result.Nodes.getNodeAs<VarDecl>("var_decl")) {
    diag(Decl->getLocation(),
         "variable %0 has a name with a SCREAMING CASE acronym")
        << Decl;
    return;
  }
}

} // namespace quantum
} // namespace tidy
} // namespace clang
