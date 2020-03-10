//===--- InterfaceConformityCheck.cpp - clang-tidy ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InterfaceConformityCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace quantum {

namespace {

bool checkDestructorConformity(const CXXRecordDecl &InterfaceDecl) {
  return (InterfaceDecl.getDestructor() != nullptr &&
          InterfaceDecl.getDestructor()->isVirtual());
}

bool checkMethodConformity(const CXXMethodDecl &MethodDecl) {
  return (!MethodDecl.isUserProvided() || MethodDecl.isPure());
}

} // namespace

void InterfaceConformityCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      cxxRecordDecl(isDefinition(), matchesName("::I[A-Z][^:]*$"))
          .bind("interface_decl"),
      this);
}

void InterfaceConformityCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *InterfaceDecl =
      Result.Nodes.getNodeAs<CXXRecordDecl>("interface_decl");

  if (!checkDestructorConformity(*InterfaceDecl)) {
    diag(InterfaceDecl->getLocation(),
         "interface %0 has a non virtual destructor")
        << InterfaceDecl;
  }

  for (auto MethodDecl : InterfaceDecl->methods()) {
    if (!checkMethodConformity(*MethodDecl)) {
      diag(MethodDecl->getLocation(),
           "method %0 is not virtual pure in interface %1")
          << MethodDecl << InterfaceDecl;
    }
  }

  InterfaceDecl->forallBases(
      [InterfaceDecl, this](const CXXRecordDecl *BaseDecl) {
        for (auto MethodDecl : BaseDecl->methods()) {
          if (!checkMethodConformity(*MethodDecl)) {
            diag(MethodDecl->getLocation(),
                 "method %0 is not virtual pure in base "
                 "class %1 of interface %2")
                << MethodDecl << BaseDecl << InterfaceDecl;
          }
        }
        return true;
      },
      false);
}

} // namespace quantum
} // namespace tidy
} // namespace clang
