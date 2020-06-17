#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include "CliOptionsManager.hpp"
#include "DtoSerializerGeneratorFrontendAction.hpp"

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
        clang::tooling::CommonOptionsParser op = optManager.buildOptionsParser();
        std::string directoryForGeneration = optManager.getOptionValue("-o");
        std::string dtoToSerialize = optManager.getOptionValue("-c");

        clang::tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList());
        dsg::DtoSerializerGeneratorFrontendAction::Factory dtoSerializerGeneratorFrontendActionFactory{
            directoryForGeneration, dtoToSerialize};
        return tool.run(&dtoSerializerGeneratorFrontendActionFactory);
    }
    else
    {
        return 0;
    }
}
