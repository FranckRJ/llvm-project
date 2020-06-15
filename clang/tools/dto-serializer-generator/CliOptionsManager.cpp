#include "CliOptionsManager.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <fmt/format.h>

#include "Utils.hpp"

namespace dsg
{
    CliOptionsManager::CliOptionsManager(Command command, int argc, const char** argv)
        : _command{std::move(command)}, _optionCategory(_command.name)
    {
        for (const auto& possibleOption : _command.possibleOptions)
        {
            _options[possibleOption] = "";
        }

        for (int i = 0; i < argc; ++i)
        {
            std::string currArg = argv[i];

            if (currArg == "--")
            {
                fillClangOptionsFromCli(argc - i - 1, argv + i + 1);
                break;
            }
            else if (currArg == "--help" || currArg == "-h")
            {
                setErrorMode();
                return;
            }
            else if (!currArg.empty() && currArg[0] == '-')
            {
                if (!applyProcessing(argc, argv, i, &CliOptionsManager::setOptionFromCli))
                {
                    return;
                }
            }
            else
            {
                if (!applyProcessing(argc, argv, i, &CliOptionsManager::processNotOptionArg))
                {
                    return;
                }
            }
        }

        if (_filesToCheck.empty())
        {
            setErrorMode("No file to scan passed as argument.");
            return;
        }

        for (const auto& [option, value] : _options)
        {
            if (option.requirement == OptionReq::Mandatory && value.empty())
            {
                setErrorMode("Missing mandatory option '" + option.name + "'.");
                return;
            }
        }
    }

    bool CliOptionsManager::commandIsValid()
    {
        return _commandIsValid;
    }

    clang::tooling::CommonOptionsParser CliOptionsManager::buildOptionsParser()
    {
        if (!_commandIsValid)
        {
            throw std::runtime_error{"Lol what are you doing ?"};
        }

        _fakeArgv.push_back(_command.name.c_str());

        for (const auto& fileToCheck : _filesToCheck)
        {
            _fakeArgv.push_back(fileToCheck.c_str());
        }

        _fakeArgv.push_back("--");
        _fakeArgv.push_back("-x");
        _fakeArgv.push_back("c++");
        _fakeArgv.push_back("-std=c++14");
        _fakeArgv.push_back("-w");

        for (const auto& clangOption : _clangOptions)
        {
            _fakeArgv.push_back(clangOption.c_str());
        }

        _fakeArgc = _fakeArgv.size();
        _fakeArgv.push_back(nullptr);

        return clang::tooling::CommonOptionsParser{_fakeArgc, _fakeArgv.data(), _optionCategory};
    }

    std::string CliOptionsManager::getOptionValue(const std::string& optionName)
    {
        auto ite = _options.find(optionName);

        if (ite != _options.end())
        {
            return ite->second;
        }
        else
        {
            return "";
        }
    }

    bool CliOptionsManager::applyProcessing(int argc, const char** argv, int& idx,
                                            int (CliOptionsManager::*processFunc)(int, const char**))
    {
        int nbOfProceedArgs = (this->*processFunc)(argc - idx, argv + idx);

        if (nbOfProceedArgs > 0)
        {
            idx += (nbOfProceedArgs - 1);
            return true;
        }

        return false;
    }

    void CliOptionsManager::fillClangOptionsFromCli(int argc, const char** argv)
    {
        for (int i = 0; i < argc; ++i)
        {
            _clangOptions.emplace_back(argv[i]);
        }
    }

    int CliOptionsManager::setOptionFromCli(int argc, const char** argv)
    {
        if (argc > 1)
        {
            auto foundOption = _options.find(argv[0]);

            if (foundOption != _options.end())
            {
                foundOption->second = argv[1];
                return 2;
            }
        }

        setErrorMode("Unknown option '" + std::string{argv[0]} + "'.");
        return -1;
    }

    int CliOptionsManager::processNotOptionArg(int argc, const char** argv)
    {
        if (argc > 0)
        {
            _filesToCheck.emplace_back(argv[0]);
            return 1;
        }

        setErrorMode("No more args to process.");
        return -1;
    }

    void CliOptionsManager::setErrorMode(const std::string& reason)
    {
        _commandIsValid = false;

        if (!reason.empty())
        {
            std::cerr << "ERROR: " << reason << "\n\n";
        }

        printHelp();
    }

    void CliOptionsManager::printHelp()
    {
        static std::string helpTemplate = R"-(USAGE: {0} file [file ...] [{0}-options] [-- [clang-options]]

OPTIONS:
file                                    The files to scan.
{1:<40}The options specific to {0}.
clang-options                           The options to pass to clang.

{0} options:
{2})-";

        static std::string optionTemplate = R"-({0:<40}{1})-";

        std::string optionText;

        for (const Option& option : _command.possibleOptions)
        {
            optionText += fmt::format(optionTemplate, option.name + " " + option.kindOfValue, option.description);
            optionText += "\n";
        }
        Utils::removeTrailingNewline(optionText);

        std::cerr << fmt::format(helpTemplate, _command.name, _command.name + "-options", optionText);
    }
} // namespace dsg
