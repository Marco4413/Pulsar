#ifndef _PULSAR_VERSION_H
#define _PULSAR_VERSION_H

#include "pulsar/core.h"

namespace Pulsar
{
    namespace Version
    {
        enum class BuildKind
        {
            Debug   = 0,
            Release = 1,
        };

        enum class PreReleaseKind : uint16_t
        {
            Alpha = 0,
            Beta  = 1,
            RC    = 2,
            None  = 0xFFFF,
        };

        struct PreRelease
        {
            PreReleaseKind Kind;
            // If .Kind == ::None, this field must be ignored
            uint16_t Version;
        };

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

            constexpr uint64_t GetCore() const
            {
                uint64_t versionCore = Major;
                versionCore = (versionCore << (8*sizeof(Minor))) | Minor;
                versionCore = (versionCore << (8*sizeof(Patch))) | Patch;
                return versionCore;
            }

            constexpr uint64_t GetPre() const
            {
                uint64_t pre = (uint16_t)Pre.Kind;
                pre = (pre << (8*sizeof(Pre.Version))) | Pre.Version;
                return pre;
            }

            /**
             * Returns:
             * -  > 0 if this > b
             * -  < 0 if this < b
             * - == 0 if this == b
             */
            constexpr int64_t Compare(SemVer b) const
            {
                uint64_t coreA = GetCore();
                uint64_t coreB = b.GetCore();
                int64_t coreDelta = coreA - coreB;
                if (coreDelta != 0) return coreDelta;
                if (Pre.Kind == PreReleaseKind::None && Pre.Kind == b.Pre.Kind)
                    return 0;

                uint64_t preA = GetPre();
                uint64_t preB = b.GetPre();
                int64_t preDelta = preA - preB;
                return preDelta;
            }
        };

        constexpr const char* BuildKindToString(BuildKind kind)
        {
            switch (kind) {
            case BuildKind::Debug:   return "debug";
            case BuildKind::Release: return "release";
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
            .Kind    = Version::PreReleaseKind::Beta,
            .Version = 0,
        },
    };
}

#endif // _PULSAR_VERSION_H
