#include <filesystem>
#include <iostream>
#include <utility>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "CliOptionsManager.hpp"
#include "SerializerCodeGenerator.hpp"
#include "Utils.hpp"

using namespace clang::tooling;
using namespace clang;
using namespace llvm;

namespace fs = std::filesystem;

std::string directoryForGeneration;
std::string dtoToSerialize;
PrintingPolicy pp{LangOptions{}};

class DtoSerializerGeneratorVisitor : public RecursiveASTVisitor<DtoSerializerGeneratorVisitor>
{
private:
    ASTContext* astContext;
    fs::path currentFile;

    static std::string unqualifyDtoType(QualType type)
    {
        std::string name = type.getAsString();

        if (name.size() <= 3 || (name.substr(name.size() - 3) != "Dto" && name.substr(name.size() - 3) != "DTO"))
        {
            return "";
        }

        auto* tagDecl = type->getUnqualifiedDesugaredType()->getAsTagDecl();

        if (tagDecl)
        {
            return tagDecl->getNameAsString();
        }
        else
        {
            std::cerr << "Error : impossible to get unqualified type of '" << type.getAsString() << "'\n";
            return "";
        }
    }

    static std::string uncontainDtoType(QualType type)
    {
        auto* record = type->getAsRecordDecl();

        if (!record || record->getQualifiedNameAsString() != "std::vector")
        {
            return "";
        }

        const auto* templateSpec = type->getAs<TemplateSpecializationType>();
        if (templateSpec && templateSpec->getNumArgs() > 0)
        {
            return unqualifyDtoType(templateSpec->getArg(0).getAsType());
        }
        else
        {
            std::cerr << "Error : impossible to get type contained inside '" << type.getAsString() << "'\n";
            return "";
        }
    }

public:
    explicit DtoSerializerGeneratorVisitor(CompilerInstance* CI, StringRef file)
        : astContext{&(CI->getASTContext())}, currentFile{file.str()}
    {
    }

    bool VisitCXXRecordDecl(CXXRecordDecl* dto)
    {
        if (!(dto->isCompleteDefinition()) || dto->getNameAsString() != dtoToSerialize)
        {
            return true;
        }

        fs::path root{directoryForGeneration};
        root.make_preferred();

        dsg::SerializerCodeGenerator serializerCodeGenerator{root, currentFile.filename().string(), dtoToSerialize};

        serializerCodeGenerator.generateInterfaceHeader();
        serializerCodeGenerator.generateImplementationHeader();

        for (const FieldDecl* field : dto->fields())
        {
            const std::string fieldType = field->getType().getAsString(pp);
            const std::string unqualifiedDtoType = unqualifyDtoType(field->getType());
            const std::string containedDtoType = uncontainDtoType(field->getType());

            const auto [memberType, memberTypeName] = [&]() {
                if (!unqualifiedDtoType.empty())
                {
                    return std::make_pair(dsg::SerializerCodeGenerator::MemberType::Dto, unqualifiedDtoType);
                }
                else if (!containedDtoType.empty())
                {
                    return std::make_pair(dsg::SerializerCodeGenerator::MemberType::DtoVector, containedDtoType);
                }
                else
                {
                    return std::make_pair(dsg::SerializerCodeGenerator::MemberType::Primitive, fieldType);
                }
            }();

            serializerCodeGenerator.addMemberInCodeGeneration(memberType, memberTypeName, field->getNameAsString());
        }

        serializerCodeGenerator.generateImplementationSource();

        return true;
    }
};

class DtoSerializerGeneratorASTConsumer : public ASTConsumer
{
private:
    DtoSerializerGeneratorVisitor* visitor;

public:
    explicit DtoSerializerGeneratorASTConsumer(CompilerInstance* CI, StringRef file)
        : visitor(new DtoSerializerGeneratorVisitor(CI, file))
    {
    }

    void HandleTranslationUnit(ASTContext& Context) override
    {
        visitor->TraverseDecl(Context.getTranslationUnitDecl());
    }
};

class DtoSerializerGeneratorFrontendAction : public ASTFrontendAction
{
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override
    {
        pp = PrintingPolicy{CI.getLangOpts()};
        return std::make_unique<DtoSerializerGeneratorASTConsumer>(&CI, file);
    }
};

int main(int argc, const char** argv)
{
    using OptionReq = dsg::CliOptionsManager::OptionReq;

    dsg::CliOptionsManager::Command command{
        "dto-serializer-generator",
        {{"-o", "directory", "Output folder where to write files.", OptionReq::Mandatory},
         {"-c", "class_name", "The name of the class for which the serializer will be generated.",
          OptionReq::Mandatory}}};

    dsg::CliOptionsManager optManager{command, argc - 1, argv + 1};

    if (optManager.commandIsValid())
    {
        CommonOptionsParser op = optManager.buildOptionsParser();
        directoryForGeneration = optManager.getOptionValue("-o");
        dtoToSerialize = optManager.getOptionValue("-c");

        ClangTool tool(op.getCompilations(), op.getSourcePathList());
        return tool.run(newFrontendActionFactory<DtoSerializerGeneratorFrontendAction>().get());
    }
    else
    {
        return 0;
    }
}
