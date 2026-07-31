#ifndef _PULSARTOOLS_VERSION_H
#define _PULSARTOOLS_VERSION_H

#include <format>
#include <string>

#include "pulsar/binary.h"
#include "pulsar/version.h"

namespace PulsarTools
{
    constexpr Pulsar::SemVer GetPulsarVersion() { return Pulsar::LANGUAGE_VERSION; }
    constexpr uint32_t GetNeutronVersion() { return Pulsar::Binary::FORMAT_VERSION; }

    constexpr Pulsar::SemVer GetToolsVersion()
    {
        return {
            .Major = 0,
            .Minor = 6,
            .Patch = 0,
            .Pre   = {
                .Kind     = Pulsar::PreReleaseKind::Beta,
                .Revision = 0,
            },
        };
    }
}

#endif // _PULSARTOOLS_VERSION_H
