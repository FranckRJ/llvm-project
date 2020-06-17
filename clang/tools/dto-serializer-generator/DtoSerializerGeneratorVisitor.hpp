#pragma once

#include <filesystem>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/ADT/StringRef.h>

namespace dsg
{
    class DtoSerializerGeneratorVisitor : public clang::RecursiveASTVisitor<DtoSerializerGeneratorVisitor>
    {
    public:
        struct Config
        {
            clang::CompilerInstance* compilerInstance;
            clang::PrintingPolicy printingPolicy;
            std::filesystem::path currentFile;
            std::filesystem::path rootForGeneration;
            std::string nameOfDtoToGenerate;
        };

        explicit DtoSerializerGeneratorVisitor(Config config);

        bool VisitCXXRecordDecl(clang::CXXRecordDecl* dto);

    private:
        Config _config;
        clang::ASTContext* _astContext;
    };
} // namespace dsg
