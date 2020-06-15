#include "Utils.hpp"

namespace dsg::Utils
{
    void removeTrailingNewline(std::string& str)
    {
        while (str.size() > 0 && str[str.size() - 1] == '\n')
        {
            str.pop_back();
        }
    }
} // namespace dsg::Utils
