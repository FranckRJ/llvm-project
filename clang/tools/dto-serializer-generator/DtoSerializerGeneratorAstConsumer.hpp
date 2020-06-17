#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/ADT/StringRef.h>

#include "DtoSerializerGeneratorVisitor.hpp"

namespace dsg
{
    class DtoSerializerGeneratorAstConsumer : public clang::ASTConsumer
    {
    public:
        explicit DtoSerializerGeneratorAstConsumer(const DtoSerializerGeneratorVisitor::Config& config);

        void HandleTranslationUnit(clang::ASTContext& context) override;

    private:
        DtoSerializerGeneratorVisitor _dtoSerializerGeneratorVisitor;
    };
} // namespace dsg
