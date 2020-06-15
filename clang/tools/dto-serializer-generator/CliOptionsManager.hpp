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
        enum class OptionReq
        {
            Mandatory,
            Optional
        };

        struct Option
        {
            std::string name;
            std::string kindOfValue;
            std::string description;
            OptionReq requirement;
        };

        struct Command
        {
            std::string name;
            std::vector<Option> possibleOptions;
        };

        CliOptionsManager(Command command, int argc, const char** argv);

        bool commandIsValid();

        clang::tooling::CommonOptionsParser buildOptionsParser();

        std::string getOptionValue(const std::string& optionName);

    private:
        bool applyProcessing(int argc, const char** argv, int& idx,
                             int (CliOptionsManager::*processFunc)(int, const char**));

        void fillClangOptionsFromCli(int argc, const char** argv);

        int setOptionFromCli(int argc, const char** argv);

        int processNotOptionArg(int argc, const char** argv);

        void setErrorMode(const std::string& reason = "");

        void printHelp();

    private:
        struct OptionComparator
        {
            using is_transparent = void;

            bool operator()(const Option& lhs, const Option& rhs) const
            {
                return lhs.name < rhs.name;
            }
            bool operator()(const std::string& lhs, const Option& rhs) const
            {
                return lhs < rhs.name;
            }
            bool operator()(const Option& lhs, const std::string& rhs) const
            {
                return lhs.name < rhs;
            }
        };

    private:
        const Command _command;
        llvm::cl::OptionCategory _optionCategory; // What is its purpose ?
        bool _commandIsValid = true;
        std::map<Option, std::string, OptionComparator> _options;
        std::vector<std::string> _clangOptions;
        std::vector<std::string> _filesToCheck;
        int _fakeArgc;
        std::vector<const char*> _fakeArgv;
    };
} // namespace dsg
