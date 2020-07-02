#include "DtoSerializerGeneratorVisitor.hpp"

#include <iostream>
#include <optional>

#include "SerializerCodeGenerator.hpp"

namespace fs = std::filesystem;

namespace dsg
{
    namespace
    {
        std::optional<std::string> unqualifyDtoType(clang::QualType type)
        {
            std::string name = type.getAsString();

            if (name.size() <= 3 || (name.substr(name.size() - 3) != "Dto" && name.substr(name.size() - 3) != "DTO"))
            {
                return std::nullopt;
            }

            auto* tagDecl = type->getUnqualifiedDesugaredType()->getAsTagDecl();

            if (tagDecl)
            {
                return tagDecl->getNameAsString();
            }
            else
            {
                std::cerr << "Error : impossible to get unqualified type of '" << type.getAsString() << "'\n";
                return std::nullopt;
            }
        }

        std::optional<std::string> uncontainDtoType(clang::QualType type)
        {
            auto* record = type->getAsRecordDecl();

            if (!record || record->getQualifiedNameAsString() != "std::vector")
            {
                return std::nullopt;
            }

            const auto* templateSpec = type->getAs<clang::TemplateSpecializationType>();
            if (templateSpec && templateSpec->getNumArgs() > 0)
            {
                return unqualifyDtoType(templateSpec->getArg(0).getAsType());
            }
            else
            {
                std::cerr << "Error : impossible to get type contained inside '" << type.getAsString() << "'\n";
                return std::nullopt;
            }
        }
    } // namespace

    DtoSerializerGeneratorVisitor::DtoSerializerGeneratorVisitor(Config config)
        : _config{std::move(config)}, _astContext{&(_config.compilerInstance->getASTContext())}
    {
    }

    bool DtoSerializerGeneratorVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl* dto)
    {
        if (!(dto->isCompleteDefinition()) || dto->getNameAsString() != _config.nameOfDtoToGenerate)
        {
            return true;
        }

        fs::path root{_config.rootForGeneration};
        root.make_preferred();

        SerializerCodeGenerator serializerCodeGenerator{root, _config.currentFile.filename().string(),
                                                        _config.nameOfDtoToGenerate};

        serializerCodeGenerator.generateInterfaceHeader();
        serializerCodeGenerator.generateImplementationHeader();

        for (const clang::FieldDecl* field : dto->fields())
        {
            const std::string fieldType = field->getType().getAsString(_config.printingPolicy);
            const std::optional<std::string> unqualifiedDtoType = unqualifyDtoType(field->getType());
            const std::optional<std::string> containedDtoType = uncontainDtoType(field->getType());

            const auto [memberType, memberTypeName] = [&]() {
                if (unqualifiedDtoType)
                {
                    return std::make_pair(SerializerCodeGenerator::MemberType::Dto, *unqualifiedDtoType);
                }
                else if (containedDtoType)
                {
                    return std::make_pair(SerializerCodeGenerator::MemberType::DtoVector, *containedDtoType);
                }
                else
                {
                    if (fieldType == "qs::Uuid")
                    {
                        return std::make_pair(SerializerCodeGenerator::MemberType::Uuid, fieldType);
                    }
                    else
                    {
                        return std::make_pair(SerializerCodeGenerator::MemberType::Primitive, fieldType);
                    }
                }
            }();

            serializerCodeGenerator.addMemberInCodeGeneration(memberType, memberTypeName, field->getNameAsString());
        }

        serializerCodeGenerator.generateImplementationSource();

        return true;
    }
} // namespace dsg
