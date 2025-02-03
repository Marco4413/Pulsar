#ifndef _PULSARTOOLS_VERSION_H
#define _PULSARTOOLS_VERSION_H

#include <string>

#include "fmt/format.h"

#include "pulsar/binary.h"
#include "pulsar/version.h"

namespace PulsarTools
{
    namespace Version
    {
        using namespace Pulsar::Version;

        std::string ToString(SemVer v)
        {
            std::string vstr = fmt::format("{}.{}.{}", v.Major, v.Minor, v.Patch);
            if (v.Pre.Kind != PreReleaseKind::None) {
                vstr += fmt::format("-{}.{}", PreReleaseKindToString(v.Pre.Kind), v.Pre.Version);
            }
            if (v.Build != BuildKind::Release) {
                vstr += fmt::format("+{}", BuildKindToString(v.Build));
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
                .Kind    = Version::PreReleaseKind::Beta,
                .Version = 0,
            },
        };
    }
}

#endif // _PULSARTOOLS_VERSION_H
