#pragma once

#include <string>

namespace dsg
{
    namespace Utils
    {
        [[nodiscard]] std::string replaceAll(std::string str, const std::string& from, const std::string& to);
        [[nodiscard]] std::string removeTrailingNewline(std::string str);
    } // namespace Utils
} // namespace dsg
