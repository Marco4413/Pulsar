#ifndef _PULSAR_VERSION_H
#define _PULSAR_VERSION_H

#include "pulsar/core.h"
#include "pulsar/structures/string.h"

namespace Pulsar
{
    enum class BuildKind
    {
        Debug   = 0,
        Release = 1,
        Unknown = 0xFF,
    };

    constexpr const char* BuildKindToString(BuildKind kind);

    enum class PreReleaseKind : uint8_t
    {
        Alpha = 0,
        Beta  = 1,
        RC    = 2,
        None  = 0xFF,
    };

    constexpr const char* PreReleaseKindToString(PreReleaseKind kind);

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

        constexpr uint64_t ToNumber() const;
        inline String ToString() const;

        /**
         * Returns:
         * -  > 0 if this > b
         * -  < 0 if this < b
         * - == 0 if this == b
         */
        constexpr int64_t Compare(SemVer b) const;

        static constexpr SemVer FromNumber(uint64_t versionNumber);
    };

    constexpr SemVer LANGUAGE_VERSION = {
        .Major = 0,
        .Minor = 7,
        .Patch = 0,
        .Pre   = {
            .Kind     = PreReleaseKind::Beta,
            .Revision = 0,
        },
    };
}

constexpr uint64_t Pulsar::SemVer::ToNumber() const
{
    uint64_t versionNumber = Major;
    versionNumber = (versionNumber << 16) | Minor;
    versionNumber = (versionNumber << 16) | Patch;
    versionNumber = (versionNumber <<  8) | (uint8_t)Pre.Kind;
    versionNumber = (versionNumber <<  8) | Pre.Revision;
    return versionNumber;
}

inline Pulsar::String Pulsar::SemVer::ToString() const
{
    String s;
    s.Reserve(32);
    s += UIntToString(Major);
    s += '.';
    s += UIntToString(Minor);
    s += '.';
    s += UIntToString(Patch);
    if (Pre.Kind != PreReleaseKind::None) {
        s += '-';
        s += PreReleaseKindToString(Pre.Kind);
        s += '.';
        s += UIntToString(Pre.Revision);
    }
    if (Build != BuildKind::Release) {
        s += '+';
        s += BuildKindToString(Build);
    }
    return s;
}

constexpr int64_t Pulsar::SemVer::Compare(SemVer b) const
{
    uint64_t va = ToNumber();
    uint64_t vb = b.ToNumber();
    return va - vb;
}

constexpr Pulsar::SemVer Pulsar::SemVer::FromNumber(uint64_t versionNumber)
{
    return {
        .Major = (uint16_t)((versionNumber >> 48) & 0xFFFF),
        .Minor = (uint16_t)((versionNumber >> 32) & 0xFFFF),
        .Patch = (uint16_t)((versionNumber >> 16) & 0xFFFF),
        .Pre   = {
            .Kind     = (PreReleaseKind)((versionNumber >> 8) & 0xFF),
            .Revision = (uint8_t)(versionNumber & 0xFF),
        },
        .Build = BuildKind::Unknown,
    };
}

constexpr const char* Pulsar::BuildKindToString(BuildKind kind)
{
    switch (kind) {
    case BuildKind::Debug:   return "debug";
    case BuildKind::Release: return "release";
    case BuildKind::Unknown: return "unknown";
    }
    return "unknown";
}

constexpr const char* Pulsar::PreReleaseKindToString(PreReleaseKind kind)
{
    switch (kind) {
    case PreReleaseKind::Alpha: return "alpha";
    case PreReleaseKind::Beta:  return "beta";
    case PreReleaseKind::RC:    return "rc";
    case PreReleaseKind::None:  return "none";
    }
    return "unknown";
}

#endif // _PULSAR_VERSION_H
