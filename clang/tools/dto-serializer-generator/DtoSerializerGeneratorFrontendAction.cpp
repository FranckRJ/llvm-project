#include "DtoSerializerGeneratorFrontendAction.hpp"

#include "DtoSerializerGeneratorAstConsumer.hpp"

namespace fs = std::filesystem;

namespace dsg
{
    DtoSerializerGeneratorFrontendAction::Factory::Factory(std::filesystem::path rootForGeneration,
                                                           std::string nameOfDtoToGenerate)
        : _rootForGeneration{std::move(rootForGeneration)}, _nameOfDtoToGenerate{std::move(nameOfDtoToGenerate)}
    {
    }

    std::unique_ptr<clang::FrontendAction> DtoSerializerGeneratorFrontendAction::Factory::create()
    {
        return std::make_unique<DtoSerializerGeneratorFrontendAction>(_rootForGeneration, _nameOfDtoToGenerate);
    }

    DtoSerializerGeneratorFrontendAction::DtoSerializerGeneratorFrontendAction(std::filesystem::path rootForGeneration,
                                                                               std::string nameOfDtoToGenerate)
        : _rootForGeneration{std::move(rootForGeneration)}, _nameOfDtoToGenerate{std::move(nameOfDtoToGenerate)}
    {
    }

    std::unique_ptr<clang::ASTConsumer>
    DtoSerializerGeneratorFrontendAction::CreateASTConsumer(clang::CompilerInstance& compilerInstance,
                                                            llvm::StringRef file)
    {
        DtoSerializerGeneratorVisitor::Config config{&compilerInstance,
                                                     clang::PrintingPolicy{compilerInstance.getLangOpts()}, file.str(),
                                                     _rootForGeneration, _nameOfDtoToGenerate};
        return std::make_unique<DtoSerializerGeneratorAstConsumer>(config);
    }
} // namespace dsg
