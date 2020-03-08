set(LLVM_LINK_COMPONENTS support)

add_clang_library(clangTidyQuantumModule
  InterfaceConformityCheck.cpp
  QuantumTidyModule.cpp

  LINK_LIBS
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  clangTidy
  clangTidyUtils
  clangTooling
  )
