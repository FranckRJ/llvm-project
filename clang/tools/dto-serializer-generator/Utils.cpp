#include "Utils.hpp"

namespace dsg::Utils
{
    std::string replaceAll(std::string str, const std::string& from, const std::string& to)
    {
        if (from.empty())
        {
            return str;
        }

        std::string::size_type start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos)
        {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }

        return str;
    }

    std::string removeTrailingNewline(std::string str)
    {
        while (str.size() > 0 && str[str.size() - 1] == '\n')
        {
            str.pop_back();
        }

        return str;
    }
} // namespace dsg::Utils
