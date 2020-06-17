#pragma once

#include <filesystem>

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>

namespace dsg
{
    class DtoSerializerGeneratorFrontendAction : public clang::ASTFrontendAction
    {
    public:
        class Factory : public clang::tooling::FrontendActionFactory
        {
        public:
            explicit Factory(std::filesystem::path rootForGeneration, std::string nameOfDtoToGenerate);

            std::unique_ptr<clang::FrontendAction> create() override;

        private:
            std::filesystem::path _rootForGeneration;
            std::string _nameOfDtoToGenerate;
        };

        explicit DtoSerializerGeneratorFrontendAction(std::filesystem::path rootForGeneration,
                                                      std::string nameOfDtoToGenerate);

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compilerInstance,
                                                              llvm::StringRef file) override;

    private:
        std::filesystem::path _rootForGeneration;
        std::string _nameOfDtoToGenerate;
    };
} // namespace dsg
