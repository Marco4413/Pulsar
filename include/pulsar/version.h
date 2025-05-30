#ifndef _PULSAR_VERSION_H
#define _PULSAR_VERSION_H

#include "pulsar/core.h"

namespace Pulsar
{
    namespace Version
    {
        enum class BuildKind : uint8_t
        {
            Debug   = 0,
            Release = 1,
            Unknown = 0xFF,
        };

        enum class PreReleaseKind : uint8_t
        {
            Alpha = 0,
            Beta  = 1,
            RC    = 2,
            None  = 0xFF,
        };

        struct PreRelease
        {
            PreReleaseKind Kind;
            // If .Kind == ::None, this field must be 0
            uint8_t Revision;
        };

        // This struct (except for ::Build) must fit within a 64 bit integer.
        // See ::ToNumber()
        struct SemVer
        {
            uint16_t Major;
            uint16_t Minor;
            uint16_t Patch;
            PreRelease Pre;
#ifdef PULSAR_DEBUG
            BuildKind Build = BuildKind::Debug;
#else // PULSAR_DEBUG
            BuildKind Build = BuildKind::Release;
#endif // PULSAR_DEBUG

            constexpr uint64_t ToNumber() const
            {
                uint64_t versionNumber = Major;
                versionNumber = (versionNumber << 16) | Minor;
                versionNumber = (versionNumber << 16) | Patch;
                versionNumber = (versionNumber <<  8) | (uint8_t)Pre.Kind;
                versionNumber = (versionNumber <<  8) | Pre.Revision;
                return versionNumber;
            }

            /**
             * Returns:
             * -  > 0 if this > b
             * -  < 0 if this < b
             * - == 0 if this == b
             */
            constexpr int64_t Compare(SemVer b) const
            {
                uint64_t va = ToNumber();
                uint64_t vb = b.ToNumber();
                return va - vb;
            }
        };

        constexpr const char* BuildKindToString(BuildKind kind)
        {
            switch (kind) {
            case BuildKind::Debug:   return "debug";
            case BuildKind::Release: return "release";
            case BuildKind::Unknown:
            default: return "unknown";
            }
        }

        constexpr const char* PreReleaseKindToString(PreReleaseKind kind)
        {
            switch (kind) {
            case PreReleaseKind::Alpha: return "alpha";
            case PreReleaseKind::Beta:  return "beta";
            case PreReleaseKind::RC:    return "rc";
            case PreReleaseKind::None:  return "none";
            default: return "unknown";
            }
        }
    }

    constexpr Version::SemVer LANGUAGE_VERSION = {
        .Major = 0,
        .Minor = 7,
        .Patch = 0,
        .Pre   = {
            .Kind     = Version::PreReleaseKind::Beta,
            .Revision = 0,
        },
    };
}

#endif // _PULSAR_VERSION_H
