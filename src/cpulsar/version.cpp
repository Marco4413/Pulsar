#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/version.h"

#include "pulsar/binary.h"
#include "pulsar/version.h"

extern "C"
{

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_SemVer_ToNumber(CPulsar_SemVer self)
{
    uint64_t versionNumber = self.Major;
    versionNumber = (versionNumber << 16) | self.Minor;
    versionNumber = (versionNumber << 16) | self.Patch;
    versionNumber = (versionNumber <<  8) | (uint8_t)self.Pre.Kind;
    versionNumber = (versionNumber <<  8) | self.Pre.Revision;
    return versionNumber;
}

CPULSAR_API CPulsar_SemVer CPULSAR_CALL CPulsar_GetLanguageVersion(void)
{
    return {
        .Major = Pulsar::LANGUAGE_VERSION.Major,
        .Minor = Pulsar::LANGUAGE_VERSION.Minor,
        .Patch = Pulsar::LANGUAGE_VERSION.Patch,
        .Pre   = {
            .Kind     = (CPulsar_PreReleaseKind)Pulsar::LANGUAGE_VERSION.Pre.Kind,
            .Revision = Pulsar::LANGUAGE_VERSION.Pre.Revision,
        },
        .Build = (CPulsar_BuildKind)Pulsar::LANGUAGE_VERSION.Build,
    };
}

CPULSAR_API uint32_t CPULSAR_CALL CPulsar_GetNeutronVersion(void)
{
    return Pulsar::Binary::FORMAT_VERSION;
}

}
