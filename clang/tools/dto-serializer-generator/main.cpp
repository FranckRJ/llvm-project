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
#include "Utils.hpp"

using namespace clang::tooling;
using namespace clang;
using namespace llvm;

namespace fs = std::filesystem;

std::string directoryForGeneration;
std::string dtoToSerialize;
PrintingPolicy pp{LangOptions{}};

struct DtoStringReplacements
{
    std::string fileNameOfDef;                  // {0}
    std::string type;                           // {1}
    std::string typeCamelCase;                  // {2}
    std::string typePascalCase;                 // {3}
    std::string typeCamelCaseWithoutDtoSuffix;  // {4}
    std::string typePascalCaseWithoutDtoSuffix; // {5}
    std::string typeAllLowerCase;               // {6}
};

struct MemberStringReplacements
{
    std::string type;           // {7}
    std::string name;           // {8}
    std::string nameCamelCase;  // {9}
    std::string namePascalCase; // {10}
};

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
             * \class I{3}Serializer
             * \brief Interface serializing and deserializing between {1} and string.
             */
            class I{3}Serializer : public ISpecificDtoSerializer<{1}>
            {{
            }};
        }}
    }}
}}
)-";

static const std::string implementationHeaderTemplate =
    R"-(#pragma once

#include "i{6}serializer.h"
#include "basejsondtoserializer.h"

namespace qs
{{
    namespace userinteraction
    {{
        namespace business
        {{
            /*!
             * \class Json{3}Serializer
             * \brief Class serializing and deserializing between {1} and JSON string.
             */
            class Json{3}Serializer : public BaseJsonDtoSerializer<I{3}Serializer>
            {{
            }};
        }}
    }}
}}
)-";

static const std::string constantDefinitionTemplate = R"-(        constexpr auto const& g_{4}{10}{{ "{9}" }};)-";

static const std::string serializePrimitiveTemplate = R"-(        obj.set(g_{4}{10}, dto.{8});)-";

static const std::string deserializePrimitiveTemplate = R"-(        dto.{8} = obj.getValue<{7}>(g_{4}{10});)-";

static const std::string serializeDtoTemplate =
    R"-(        Poco::JSON::Object subObj{10} = JsonDtoBuilder<{7}>::serializeToObject(dto.{8});
        obj.set(g_{4}{10}, subObj{10});)-";

static const std::string deserializeDtoTemplate =
    R"-(        Poco::JSON::Object::Ptr subObjPtr{10} = obj.getObject(g_{4}{10});
        dto.{8} = JsonDtoBuilder<{7}>::deserializeFromObject(*subObjPtr{10});)-";

static const std::string serializeDtoListTemplate =
    R"-(        Poco::JSON::Array subArr{10} = JsonDtoBuilder<{7}>::serializeToArray(dto.{8});
        obj.set(g_{4}{10}, subArr{10});)-";

static const std::string deserializeDtoListTemplate =
    R"-(        Poco::JSON::Array::Ptr subArrPtr{10} = obj.getArray(g_{4}{10});
        dto.{8} = JsonDtoBuilder<{7}>::deserializeFromArray(*subArrPtr{10});)-";

std::string formatTemplateWithDtoReplacements(const std::string& strTemplate, const DtoStringReplacements& replacements)
{
    return fmt::format(strTemplate, replacements.fileNameOfDef, replacements.type, replacements.typeCamelCase,
                       replacements.typePascalCase, replacements.typeCamelCaseWithoutDtoSuffix,
                       replacements.typePascalCaseWithoutDtoSuffix, replacements.typeAllLowerCase);
}

std::string formatTemplateWithDtoAndMemberReplacements(const std::string& strTemplate,
                                                       const DtoStringReplacements& dtoReplacements,
                                                       const MemberStringReplacements& memberReplacements)
{
    return fmt::format(strTemplate, dtoReplacements.fileNameOfDef, dtoReplacements.type, dtoReplacements.typeCamelCase,
                       dtoReplacements.typePascalCase, dtoReplacements.typeCamelCaseWithoutDtoSuffix,
                       dtoReplacements.typePascalCaseWithoutDtoSuffix, dtoReplacements.typeAllLowerCase,
                       memberReplacements.type, memberReplacements.name, memberReplacements.nameCamelCase,
                       memberReplacements.namePascalCase);
}

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

        const DtoStringReplacements dtoStrReplacements = [&]() {
            DtoStringReplacements replacements;

            replacements.fileNameOfDef = currentFile.filename().string();
            replacements.type = dtoToSerialize;
            replacements.typeCamelCase = camelCaseWord(replacements.type);
            replacements.typePascalCase = pascalCaseWord(replacements.type);
            replacements.typeCamelCaseWithoutDtoSuffix = removeDtoSuffix(replacements.typeCamelCase);
            replacements.typePascalCaseWithoutDtoSuffix = removeDtoSuffix(replacements.typePascalCase);
            replacements.typeAllLowerCase = lowerAllLetters(replacements.type);

            return replacements;
        }();

        {
            std::ofstream interfaceHeaderFile{root / ("i" + dtoStrReplacements.typeAllLowerCase + "serializer.h")};
            interfaceHeaderFile << formatTemplateWithDtoReplacements(interfaceHeaderTemplate, dtoStrReplacements);
        }
        {
            std::ofstream implementationHeaderFile{root /
                                                   ("json" + dtoStrReplacements.typeAllLowerCase + "serializer.h")};
            implementationHeaderFile << formatTemplateWithDtoReplacements(implementationHeaderTemplate,
                                                                          dtoStrReplacements);
        }

        std::ofstream implementationSourceFile{root /
                                               ("json" + dtoStrReplacements.typeAllLowerCase + "serializer.cpp")};

        std::string constantDefs;
        std::string serializeCode;
        std::string deserializeCode;

        for (const FieldDecl* field : dto->fields())
        {
            const std::string fieldType = field->getType().getAsString(pp);
            const std::string unqualifiedDtoType = unqualifyDtoType(field->getType());
            const std::string containedDtoType = uncontainDtoType(field->getType());

            const MemberStringReplacements memberStrReplacements = [&]() {
                MemberStringReplacements replacements;

                std::string typeUsed;
                if (!unqualifiedDtoType.empty())
                {
                    typeUsed = unqualifiedDtoType;
                }
                else if (!containedDtoType.empty())
                {
                    typeUsed = containedDtoType;
                }
                else
                {
                    typeUsed = fieldType;
                }

                replacements.type = typeUsed;
                replacements.name = field->getNameAsString();
                replacements.nameCamelCase = camelCaseWord(replacements.name);
                replacements.namePascalCase = pascalCaseWord(replacements.name);

                return replacements;
            }();

            constantDefs += formatTemplateWithDtoAndMemberReplacements(constantDefinitionTemplate, dtoStrReplacements,
                                                                       memberStrReplacements);
            constantDefs += "\n";

            if (!unqualifiedDtoType.empty())
            {
                serializeCode += formatTemplateWithDtoAndMemberReplacements(serializeDtoTemplate, dtoStrReplacements,
                                                                            memberStrReplacements);
                serializeCode += "\n";

                deserializeCode += formatTemplateWithDtoAndMemberReplacements(
                    deserializeDtoTemplate, dtoStrReplacements, memberStrReplacements);
                deserializeCode += "\n";
            }
            else if (!containedDtoType.empty())
            {
                serializeCode += formatTemplateWithDtoAndMemberReplacements(serializeDtoListTemplate,
                                                                            dtoStrReplacements, memberStrReplacements);
                serializeCode += "\n";

                deserializeCode += formatTemplateWithDtoAndMemberReplacements(
                    deserializeDtoListTemplate, dtoStrReplacements, memberStrReplacements);
                deserializeCode += "\n";
            }
            else
            {
                serializeCode += formatTemplateWithDtoAndMemberReplacements(serializePrimitiveTemplate,
                                                                            dtoStrReplacements, memberStrReplacements);
                serializeCode += "\n";

                deserializeCode += formatTemplateWithDtoAndMemberReplacements(
                    deserializePrimitiveTemplate, dtoStrReplacements, memberStrReplacements);
                deserializeCode += "\n";
            }
        }

        dsg::Utils::removeTrailingNewline(constantDefs);
        dsg::Utils::removeTrailingNewline(serializeCode);
        dsg::Utils::removeTrailingNewline(deserializeCode);

        implementationSourceFile << fmt::format(implementationSourceTemplate, currentFile.filename().string(),
                                                constantDefs, dtoStrReplacements.type, serializeCode, deserializeCode);

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
