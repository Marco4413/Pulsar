#ifndef _PULSARTOOLS_VERSION_H
#define _PULSARTOOLS_VERSION_H

#include <format>
#include <string>

#include "pulsar/binary.h"
#include "pulsar/version.h"

namespace PulsarTools
{
    namespace Version
    {
        using namespace Pulsar::Version;

        inline std::string ToString(SemVer v)
        {
            std::string vstr = std::format("{}.{}.{}", v.Major, v.Minor, v.Patch);
            if (v.Pre.Kind != PreReleaseKind::None) {
                vstr += std::format("-{}.{}", PreReleaseKindToString(v.Pre.Kind), v.Pre.Revision);
            }
            if (v.Build != BuildKind::Release) {
                vstr += std::format("+{}", BuildKindToString(v.Build));
            }
            return vstr;
        }
    }

    constexpr Version::SemVer GetPulsarVersion() { return Pulsar::LANGUAGE_VERSION; }
    constexpr uint32_t GetNeutronVersion() { return Pulsar::Binary::FORMAT_VERSION; }

    constexpr Version::SemVer GetToolsVersion()
    {
        return {
            .Major = 0,
            .Minor = 6,
            .Patch = 0,
            .Pre   = {
                .Kind     = Version::PreReleaseKind::Beta,
                .Revision = 0,
            },
        };
    }
}

#endif // _PULSARTOOLS_VERSION_H
