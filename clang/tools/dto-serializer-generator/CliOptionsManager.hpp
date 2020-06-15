#pragma once

#include <map>
#include <string>
#include <vector>

#include "clang/Tooling/CommonOptionsParser.h"

namespace dsg
{
    class CliOptionsManager
    {
    public:
        CliOptionsManager(std::vector<std::string> possibleOptions, int argc, const char** argv);

        bool commandIsValid();

        clang::tooling::CommonOptionsParser buildOptionsParser();

        std::string getOptionValue(const std::string& optionName);

    private:
        bool applyProcessing(int argc, const char** argv, int& idx,
                             int (CliOptionsManager::*processFunc)(int, const char**));

        void fillClangOptionsFromCli(int argc, const char** argv);

        int setOptionFromCli(int argc, const char** argv);

        int processNotOptionArg(int argc, const char** argv);

        void printHelp();

    private:
        bool _commandIsValid = true;
        std::map<std::string, std::string> _options;
        std::vector<std::string> _clangOptions;
        std::vector<std::string> _filesToCheck;
        int _fakeArgc;
        std::vector<const char*> _fakeArgv;
    };
} // namespace dsg
