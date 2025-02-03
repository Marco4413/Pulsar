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

        inline std::string ToString(SemVer v)
        {
            std::string vstr = fmt::format("{}.{}.{}", v.Major, v.Minor, v.Patch);
            if (v.Pre.Kind != PreReleaseKind::None) {
                vstr += fmt::format("-{}.{}", PreReleaseKindToString(v.Pre.Kind), v.Pre.Revision);
            }
            if (v.Build != BuildKind::Release) {
                vstr += fmt::format("+{}", BuildKindToString(v.Build));
            }
            return vstr;
        }

        constexpr SemVer FromNumber(uint64_t versionNumber)
        {
            return {
                .Major = (uint16_t)((versionNumber >> 48) & 0xFFFF),
                .Minor = (uint16_t)((versionNumber >> 32) & 0xFFFF),
                .Patch = (uint16_t)((versionNumber >> 16) & 0xFFFF),
                .Pre   = {
                    .Kind     = (PreReleaseKind)((versionNumber >> 8) & 0xFF),
                    .Revision = (uint8_t)(versionNumber & 0xFF),
                },
                .Build = (BuildKind)0xFF,
            };
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
