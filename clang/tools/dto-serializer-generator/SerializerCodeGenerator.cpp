#include "SerializerCodeGenerator.hpp"

#include <fstream>
#include <stdexcept>
#include <string_view>

#include <fmt/format.h>

#include "Utils.hpp"

namespace fs = std::filesystem;

namespace dsg
{
    namespace
    {
        std::string lowerAllLetters(std::string word)
        {
            for (auto& letter : word)
            {
                letter = ::tolower(letter);
            }

            return word;
        }

        std::string camelCaseWord(std::string word)
        {
            if (word.empty())
            {
                return word;
            }

            word[0] = ::tolower(word[0]);
            return Utils::replaceAll(word, "DTO", "Dto");
        }

        std::string pascalCaseWord(std::string word)
        {
            if (word.empty())
            {
                return word;
            }

            word[0] = ::toupper(word[0]);
            return Utils::replaceAll(word, "DTO", "Dto");
        }

        bool isANameForDto(std::string_view name)
        {
            if (name.size() <= 3)
            {
                return false;
            }

            return (name.substr(name.size() - 3) == "Dto" || name.substr(name.size() - 3) == "DTO");
        }

        std::string removeDtoSuffix(std::string_view word)
        {
            if (!isANameForDto(word))
            {
                return std::string{word};
            }

            return std::string{word.substr(0, word.size() - 3)};
        }

        // 0 = DTO include.
        // 1 = constant definitions.
        // 2 = DTO type.
        // 3 = serialization code.
        // 4 = deserialization code.
        const std::string implementationSourceTemplate =
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

        const std::string interfaceHeaderTemplate =
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

        const std::string implementationHeaderTemplate =
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

        const std::string constantDefinitionTemplate = R"-(        constexpr auto const& g_{4}{10}{{ "{9}" }};)-";

        const std::string serializePrimitiveTemplate = R"-(        obj.set(g_{4}{10}, dto.{8});)-";

        const std::string deserializePrimitiveTemplate = R"-(        dto.{8} = obj.getValue<{7}>(g_{4}{10});)-";

        const std::string serializeUuidTemplate = R"-(        obj.set(g_{4}{10}, dto.{8}.toString());)-";

        const std::string deserializeUuidTemplate =
            R"-(        dto.{8} = Uuid{{ obj.getValue<std::string>(g_{4}{10}) }};)-";

        const std::string serializeDtoTemplate =
            R"-(        Poco::JSON::Object {9}SubObj = JsonDtoBuilder<{7}>::serializeToObject(dto.{8});
        obj.set(g_{4}{10}, std::move({9}SubObj));)-";

        const std::string deserializeDtoTemplate =
            R"-(        Poco::JSON::Object::Ptr {9}SubObjPtr = obj.getObject(g_{4}{10});
        dto.{8} = JsonDtoBuilder<{7}>::deserializeFromObject(*{9}SubObjPtr);)-";

        const std::string serializeDtoVectorTemplate =
            R"-(        Poco::JSON::Array {9}SubArr = JsonDtoBuilder<{7}>::serializeToArray(dto.{8});
        obj.set(g_{4}{10}, std::move({9}SubArr));)-";

        const std::string deserializeDtoVectorTemplate =
            R"-(        Poco::JSON::Array::Ptr {9}SubArrPtr = obj.getArray(g_{4}{10});
        dto.{8} = JsonDtoBuilder<{7}>::deserializeFromArray(*{9}SubArrPtr);)-";
    } // namespace

    SerializerCodeGenerator::SerializerCodeGenerator(fs::path rootOfGeneration, const std::string& dtoDefFileName,
                                                     const std::string& dtoTypeName)
        : _rootOfGeneration{std::move(rootOfGeneration)}
        , _dtoStrReplacements{createDtoStringReplacements(dtoDefFileName, dtoTypeName)}
    {
    }

    void SerializerCodeGenerator::generateInterfaceHeader()
    {
        std::ofstream file{_rootOfGeneration / ("i" + _dtoStrReplacements.typeAllLowerCase + "serializer.h")};
        file << formatTemplateWithDtoReplacements(interfaceHeaderTemplate);
    }

    void SerializerCodeGenerator::generateImplementationHeader()
    {
        std::ofstream file{_rootOfGeneration / ("json" + _dtoStrReplacements.typeAllLowerCase + "serializer.h")};
        file << formatTemplateWithDtoReplacements(implementationHeaderTemplate);
    }

    void SerializerCodeGenerator::generateImplementationSource()
    {
        std::string trimmedConstantDefsCode = Utils::removeTrailingNewline(_constantDefsCode);
        std::string trimmedSerializeCode = Utils::removeTrailingNewline(_serializeCode);
        std::string trimmedDeserializeCode = Utils::removeTrailingNewline(_deserializeCode);

        std::ofstream file{_rootOfGeneration / ("json" + _dtoStrReplacements.typeAllLowerCase + "serializer.cpp")};
        file << fmt::format(implementationSourceTemplate, _dtoStrReplacements.fileNameOfDef, trimmedConstantDefsCode,
                            _dtoStrReplacements.type, trimmedSerializeCode, trimmedDeserializeCode);
    }

    void SerializerCodeGenerator::addMemberInCodeGeneration(MemberType memberType, const std::string& memberTypeName,
                                                            const std::string& memberName)
    {
        MemberStringReplacements memberStrReplacements = createMemberStringReplacements(memberTypeName, memberName);

        _constantDefsCode +=
            formatTemplateWithDtoAndMemberReplacements(constantDefinitionTemplate, memberStrReplacements);
        _constantDefsCode += "\n";

        const std::string* serializeTemplatePtr;
        const std::string* deserializeTemplatePtr;

        switch (memberType)
        {
            case MemberType::Primitive:
            {
                serializeTemplatePtr = &serializePrimitiveTemplate;
                deserializeTemplatePtr = &deserializePrimitiveTemplate;
                break;
            }
            case MemberType::Uuid:
            {
                serializeTemplatePtr = &serializeUuidTemplate;
                deserializeTemplatePtr = &deserializeUuidTemplate;
                break;
            }
            case MemberType::Dto:
            {
                serializeTemplatePtr = &serializeDtoTemplate;
                deserializeTemplatePtr = &deserializeDtoTemplate;
                break;
            }
            case MemberType::DtoVector:
            {
                serializeTemplatePtr = &serializeDtoVectorTemplate;
                deserializeTemplatePtr = &deserializeDtoVectorTemplate;
                break;
            }
            default:
            {
                throw std::runtime_error{"Wtf man ?"};
            }
        }

        _serializeCode += formatTemplateWithDtoAndMemberReplacements(*serializeTemplatePtr, memberStrReplacements);
        _serializeCode += "\n";

        _deserializeCode += formatTemplateWithDtoAndMemberReplacements(*deserializeTemplatePtr, memberStrReplacements);
        _deserializeCode += "\n";
    }

    SerializerCodeGenerator::DtoStringReplacements
    SerializerCodeGenerator::createDtoStringReplacements(const std::string& dtoDefFileName,
                                                         const std::string& dtoTypeName)
    {
        SerializerCodeGenerator::DtoStringReplacements strReplacements{};

        strReplacements.fileNameOfDef = dtoDefFileName;
        strReplacements.type = dtoTypeName;
        strReplacements.typeCamelCase = camelCaseWord(strReplacements.type);
        strReplacements.typePascalCase = pascalCaseWord(strReplacements.type);
        strReplacements.typeCamelCaseWithoutDtoSuffix = removeDtoSuffix(strReplacements.typeCamelCase);
        strReplacements.typePascalCaseWithoutDtoSuffix = removeDtoSuffix(strReplacements.typePascalCase);
        strReplacements.typeAllLowerCase = lowerAllLetters(strReplacements.type);

        return strReplacements;
    }

    SerializerCodeGenerator::MemberStringReplacements
    SerializerCodeGenerator::createMemberStringReplacements(const std::string& memberTypeName,
                                                            const std::string& memberName)
    {
        MemberStringReplacements strReplacements;

        strReplacements.type = memberTypeName;
        strReplacements.name = memberName;
        strReplacements.nameCamelCase = camelCaseWord(strReplacements.name);
        strReplacements.namePascalCase = pascalCaseWord(strReplacements.name);

        return strReplacements;
    }

    std::string SerializerCodeGenerator::formatTemplateWithDtoReplacements(const std::string& strTemplate)
    {
        return fmt::format(strTemplate, _dtoStrReplacements.fileNameOfDef, _dtoStrReplacements.type,
                           _dtoStrReplacements.typeCamelCase, _dtoStrReplacements.typePascalCase,
                           _dtoStrReplacements.typeCamelCaseWithoutDtoSuffix,
                           _dtoStrReplacements.typePascalCaseWithoutDtoSuffix, _dtoStrReplacements.typeAllLowerCase);
    }

    std::string SerializerCodeGenerator::formatTemplateWithDtoAndMemberReplacements(
        const std::string& strTemplate, const MemberStringReplacements& memberStrReplacements)
    {
        return fmt::format(strTemplate, _dtoStrReplacements.fileNameOfDef, _dtoStrReplacements.type,
                           _dtoStrReplacements.typeCamelCase, _dtoStrReplacements.typePascalCase,
                           _dtoStrReplacements.typeCamelCaseWithoutDtoSuffix,
                           _dtoStrReplacements.typePascalCaseWithoutDtoSuffix, _dtoStrReplacements.typeAllLowerCase,
                           memberStrReplacements.type, memberStrReplacements.name, memberStrReplacements.nameCamelCase,
                           memberStrReplacements.namePascalCase);
    }
} // namespace dsg
