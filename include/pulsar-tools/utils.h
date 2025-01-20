#ifndef _PULSARTOOLS_UTILS_H
#define _PULSARTOOLS_UTILS_H

#include <string_view>

namespace PulsarTools
{
    bool IsFile(std::string_view filepath);
    bool IsNeutronFile(std::string_view filepath);
}

#endif // _PULSARTOOLS_UTILS_H
