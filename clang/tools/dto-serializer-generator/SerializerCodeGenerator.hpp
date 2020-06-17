#pragma once

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace dsg
{
    class SerializerCodeGenerator
    {
    public:
        enum class MemberType
        {
            Primitive,
            Uuid,
            Dto,
            DtoVector
        };

    public:
        explicit SerializerCodeGenerator(std::filesystem::path rootOfGeneration, const std::string& dtoDefFileName,
                                         const std::string& dtoTypeName);

        void generateInterfaceHeader();

        void generateImplementationHeader();

        void generateImplementationSource();

        void addMemberInCodeGeneration(MemberType memberType, const std::string& memberTypeName,
                                       const std::string& memberName);

    private:
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

    private:
        DtoStringReplacements createDtoStringReplacements(const std::string& dtoDefFileName,
                                                          const std::string& dtoTypeName);

        MemberStringReplacements createMemberStringReplacements(const std::string& memberTypeName,
                                                                const std::string& memberName);

        std::string formatTemplateWithDtoReplacements(const std::string& strTemplate);

        std::string formatTemplateWithDtoAndMemberReplacements(const std::string& strTemplate,
                                                               const MemberStringReplacements& memberStrReplacements);

    private:
        const std::filesystem::path _rootOfGeneration;
        const DtoStringReplacements _dtoStrReplacements;
        std::string _constantDefsCode;
        std::string _serializeCode;
        std::string _deserializeCode;
    };
} // namespace dsg
