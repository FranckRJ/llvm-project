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
          InterfaceDecl.getDestructor()->isVirtual() &&
          InterfaceDecl.getDestructor()->isDefaulted());
}

bool checkMethodConformity(const CXXMethodDecl &MethDecl) {
  return (!MethDecl.isUserProvided() || MethDecl.isPure() ||
          isa<CXXDestructorDecl>(MethDecl));
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
         "interface %0 has a non defaulted virtual destructor")
        << InterfaceDecl;
  }

  for (auto MethDecl : InterfaceDecl->methods()) {
    if (!checkMethodConformity(*MethDecl)) {
      diag(MethDecl->getLocation(),
           "method %0 is not virtual pure in interface %1")
          << MethDecl << InterfaceDecl;
    }
  }

  InterfaceDecl->forallBases(
      [InterfaceDecl, this](const CXXRecordDecl *BaseDecl) {
        for (auto MethDecl : BaseDecl->methods()) {
          if (!checkMethodConformity(*MethDecl)) {
            diag(MethDecl->getLocation(),
                 "method %0 is not virtual pure in base "
                 "class %1 of interface %2")
                << MethDecl << BaseDecl << InterfaceDecl;
          }
        }
        return true;
      },
      false);
}

} // namespace quantum
} // namespace tidy
} // namespace clang
