#include "CliOptionsManager.hpp"

#include <iostream>
#include <stdexcept>

namespace dsg
{
    namespace
    {
        // What is its purpose ?
        static llvm::cl::OptionCategory DtoSerializerGeneratorCategory("dto-serializer-generator");
    } // namespace

    CliOptionsManager::CliOptionsManager(std::vector<std::string> possibleOptions, int argc, const char** argv)
    {
        for (const auto& possibleOption : possibleOptions)
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
            else if (!currArg.empty() && currArg[0] == '-')
            {
                if (applyProcessing(argc, argv, i, &CliOptionsManager::setOptionFromCli))
                {
                    continue;
                }
            }
            else
            {
                if (applyProcessing(argc, argv, i, &CliOptionsManager::processNotOptionArg))
                {
                    continue;
                }
            }

            printHelp();
            _commandIsValid = false;
            return;
        }

        if (_filesToCheck.empty())
        {
            printHelp();
            _commandIsValid = false;
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

        _fakeArgv.push_back("dto-serializer-generator");

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

        return clang::tooling::CommonOptionsParser{_fakeArgc, _fakeArgv.data(), DtoSerializerGeneratorCategory};
    }

    std::string CliOptionsManager::getOptionValue(const std::string& optionName)
    {
        return _options[optionName];
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

        return -1;
    }

    int CliOptionsManager::processNotOptionArg(int argc, const char** argv)
    {
        if (argc > 0)
        {
            _filesToCheck.emplace_back(argv[0]);
            return 1;
        }

        return -1;
    }

    void CliOptionsManager::printHelp()
    {
        std::cerr << "Useful help.\n";
    }
} // namespace dsg
