#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include <fmt/format.h>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "CliOptionsManager.hpp"

using namespace clang::tooling;
using namespace clang;
using namespace llvm;

namespace fs = std::filesystem;

std::string directoryForGeneration;
std::string dtoToSerialize;
PrintingPolicy pp{LangOptions{}};

// 0 = DTO include.
// 1 = constant definitions.
// 2 = DTO type.
// 3 = serialization code.
// 4 = deserialization code.
static const std::string implementationSourceTemplate =
    R"-(#include "private/jsondtobuilder.h"
#include "{0}"

namespace qs {{ namespace userinteraction {{ namespace business
{{
    namespace
    {{
{1}
    }}

    template <>
    Poco::JSON::Object JsonDtoBuilder<{2}>::serializeToObject(const Dto& dto)
    {{
        Poco::JSON::Object obj;

{3}

        return obj;
    }}

    template <>
    auto JsonDtoBuilder<{2}>::deserializeFromObject(const Poco::JSON::Object& obj) -> Dto
    {{
        Dto dto;

{4}

        return dto;
    }}
}}}}}}
)-";

// 0 = DTO include.
// 1 = DTO Type in PascaleCase.
// 2 = DTO Type.
static const std::string interfaceHeaderTemplate =
    R"-(#pragma once

#include "ispecificdtoserializer.h"
#include "{0}"

namespace qs
{{
    namespace userinteraction
    {{
        namespace business
        {{
            /*!
             * \class I{1}Serializer
             * \brief Interface serializing and deserializing between {2} and string.
             */
            class I{1}Serializer : public ISpecificDtoSerializer<{2}>
            {{
            }};
        }}
    }}
}}
)-";

// 0 = DTO Type in all lower case.
// 1 = DTO Type in PascalCase.
// 2 = DTO Type.
static const std::string implementationHeaderTemplate =
    R"-(#pragma once

#include "i{0}serializer.h"
#include "basejsondtoserializer.h"

namespace qs
{{
    namespace userinteraction
    {{
        namespace business
        {{
            /*!
             * \class Json{1}Serializer
             * \brief Class serializing and deserializing between {2} and JSON string.
             */
            class Json{1}Serializer : public BaseJsonDtoSerializer<I{1}Serializer>
            {{
            }};
        }}
    }}
}}
)-";

// 0 = DTO type in camelCase, without DTO at the end.
// 1 = member name in PascalCase.
// 2 = member name in camelCase.
static const std::string constantDefinitionTemplate = R"-(        constexpr auto const& g_{0}{1}{{ "{2}" }};)-";

// 0 = DTO type in camelCase, without DTO at the end.
// 1 = member name in PascalCase.
// 2 = member name.
static const std::string serializePrimitiveTemplate = R"-(        obj.set(g_{0}{1}, dto.{2});)-";

// 0 = member name.
// 1 = Type of member.
// 2 = DTO type in camelCase, without DTO at the end.
// 3 = member name in PascalCase.
static const std::string deserializePrimitiveTemplate = R"-(        dto.{0} = obj.getValue<{1}>(g_{2}{3});)-";

// 0 = DTO type in camelCase, without DTO at the end.
// 1 = member name in PascalCase.
// 2 = member unqualified type.
// 3 = member name.
static const std::string serializeDtoTemplate =
    R"-(        Poco::JSON::Object subObj{1} = JsonDtoBuilder<{2}>::serializeToObject(dto.{3});
        obj.set(g_{0}{1}, subObj{1});)-";

// 0 = member name.
// 1 = member unqualified type.
// 2 = DTO type in camelCase, without DTO at the end.
// 3 = member name in PascalCase.
static const std::string deserializeDtoTemplate =
    R"-(        Poco::JSON::Object::Ptr subObjPtr{3} = obj.getObject(g_{2}{3});
        dto.{0} = JsonDtoBuilder<{1}>::deserializeFromObject(*subObjPtr{3});)-";

// 0 = DTO type in camelCase, without DTO at the end.
// 1 = member name in PascalCase.
// 2 = member unqualified type.
// 3 = member name.
static const std::string serializeDtoListTemplate =
    R"-(        Poco::JSON::Array subArr{1} = JsonDtoBuilder<{2}>::serializeToArray(dto.{3});
        obj.set(g_{0}{1}, subArr{1});)-";

// 0 = member name.
// 1 = member unqualified type.
// 2 = DTO type in camelCase, without DTO at the end.
// 3 = member name in PascalCase.
static const std::string deserializeDtoListTemplate =
    R"-(        Poco::JSON::Array::Ptr subArrPtr{3} = obj.getArray(g_{2}{3});
        dto.{0} = JsonDtoBuilder<{1}>::deserializeFromArray(*subArrPtr{3});)-";

class DtoSerializerGeneratorVisitor : public RecursiveASTVisitor<DtoSerializerGeneratorVisitor>
{
private:
    ASTContext* astContext;
    fs::path currentFile;

    static void replaceAll(std::string& str, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return;
        std::size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos)
        {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    static std::string lowerAllLetters(std::string word)
    {
        for (auto& letter : word)
        {
            letter = ::tolower(letter);
        }

        return word;
    }

    static std::string camelCaseWord(std::string word)
    {
        if (word.empty())
        {
            return word;
        }

        word[0] = ::tolower(word[0]);
        replaceAll(word, "DTO", "Dto");
        return word;
    }

    static std::string pascalCaseWord(std::string word)
    {
        if (word.empty())
        {
            return word;
        }

        word[0] = ::toupper(word[0]);
        replaceAll(word, "DTO", "Dto");
        return word;
    }

    static std::string removeDtoSuffix(std::string word)
    {
        if (word.size() <= 3 || (word.substr(word.size() - 3) != "Dto" && word.substr(word.size() - 3) != "DTO"))
        {
            return word;
        }

        return word.substr(0, word.size() - 3);
    }

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

        const std::string dtoType = dtoToSerialize;
        const std::string allLowerDtoType = lowerAllLetters(dtoType);
        const std::string camelCaseDtoType = camelCaseWord(dtoType);
        const std::string camelCaseDtoTypeWithoutDtoSuffix = removeDtoSuffix(camelCaseDtoType);
        const std::string pascalCaseDtoType = pascalCaseWord(dtoType);

        {
            std::ofstream interfaceHeaderFile{root / ("i" + allLowerDtoType + "serializer.h")};
            interfaceHeaderFile << fmt::format(interfaceHeaderTemplate, currentFile.filename().string(),
                                               pascalCaseDtoType, dtoType);
        }
        {
            std::ofstream implementationHeaderFile{root / ("json" + allLowerDtoType + "serializer.h")};
            implementationHeaderFile << fmt::format(implementationHeaderTemplate, allLowerDtoType, pascalCaseDtoType,
                                                    dtoType);
        }

        std::ofstream implementationSourceFile{root / ("json" + allLowerDtoType + "serializer.cpp")};

        std::string constantDefs;
        std::string serializeCode;
        std::string deserializeCode;

        for (const FieldDecl* field : dto->fields())
        {
            //field->getType()->getAs<TemplateSpecializationType>()->getArg(0).getAsType().getAsString();
            const std::string fieldType = field->getType().getAsString(pp);
            const std::string unqualifiedDtoType = unqualifyDtoType(field->getType());
            const std::string containedDtoType = uncontainDtoType(field->getType());

            const std::string fieldName = field->getNameAsString();
            const std::string camelCaseFieldName = camelCaseWord(fieldName);
            const std::string pascalCaseFieldName = pascalCaseWord(fieldName);

            constantDefs += fmt::format(constantDefinitionTemplate, camelCaseDtoTypeWithoutDtoSuffix,
                                        pascalCaseFieldName, camelCaseFieldName);
            constantDefs += "\n";

            if (!unqualifiedDtoType.empty())
            {
                serializeCode += fmt::format(serializeDtoTemplate, camelCaseDtoTypeWithoutDtoSuffix,
                                             pascalCaseFieldName, unqualifiedDtoType, fieldName);
                serializeCode += "\n";

                deserializeCode += fmt::format(deserializeDtoTemplate, fieldName, unqualifiedDtoType,
                                               camelCaseDtoTypeWithoutDtoSuffix, pascalCaseFieldName);
                deserializeCode += "\n";
            }
            else if (!containedDtoType.empty())
            {
                serializeCode += fmt::format(serializeDtoListTemplate, camelCaseDtoTypeWithoutDtoSuffix,
                                             pascalCaseFieldName, containedDtoType, fieldName);
                serializeCode += "\n";

                deserializeCode += fmt::format(deserializeDtoListTemplate, fieldName, containedDtoType,
                                               camelCaseDtoTypeWithoutDtoSuffix, pascalCaseFieldName);
                deserializeCode += "\n";
            }
            else
            {
                serializeCode += fmt::format(serializePrimitiveTemplate, camelCaseDtoTypeWithoutDtoSuffix,
                                             pascalCaseFieldName, fieldName);
                serializeCode += "\n";

                deserializeCode += fmt::format(deserializePrimitiveTemplate, fieldName, fieldType,
                                               camelCaseDtoTypeWithoutDtoSuffix, pascalCaseFieldName);
                deserializeCode += "\n";
            }
        }

        if (!constantDefs.empty())
        {
            constantDefs.pop_back();
        }
        if (!serializeCode.empty())
        {
            serializeCode.pop_back();
        }
        if (!deserializeCode.empty())
        {
            deserializeCode.pop_back();
        }

        implementationSourceFile << fmt::format(implementationSourceTemplate, currentFile.filename().string(),
                                                constantDefs, dtoType, serializeCode, deserializeCode);

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
