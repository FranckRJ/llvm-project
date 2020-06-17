#include "DtoSerializerGeneratorAstConsumer.hpp"

namespace dsg
{
    DtoSerializerGeneratorAstConsumer::DtoSerializerGeneratorAstConsumer(
        const DtoSerializerGeneratorVisitor::Config& config)
        : _dtoSerializerGeneratorVisitor{config}
    {
    }

    void DtoSerializerGeneratorAstConsumer::HandleTranslationUnit(clang::ASTContext& context)
    {
        _dtoSerializerGeneratorVisitor.TraverseDecl(context.getTranslationUnitDecl());
    }
} // namespace dsg
