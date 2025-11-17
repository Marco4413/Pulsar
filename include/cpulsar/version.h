#ifndef _CPULSAR_VERSION_H
#define _CPULSAR_VERSION_H

#include "cpulsar/core.h"

#ifdef PULSAR_DEBUG
#  define CPULSAR_VERSION_CURRENT_BUILD_KIND() (CPulsar_BuildKind_Debug)
#else // PULSAR_DEBUG
#  define CPULSAR_VERSION_CURRENT_BUILD_KIND() (CPulsar_BuildKind_Release)
#endif // PULSAR_DEBUG

// Returns the version of CPulsar referenced by this header file.
#define CPULSAR_VERSION_CURRENT() CPULSAR_LITERAL_S( \
        CPulsar_SemVer,                              \
        .Major = 0, .Minor = 1, .Patch = 0,          \
        .Pre   = {CPulsar_PreReleaseKind_Alpha, 0},  \
        .Build = CPULSAR_VERSION_CURRENT_BUILD_KIND())

// Returns the version number of CPulsar referenced by this header file.
#define CPULSAR_VERSION_NUMBER_CURRENT() (CPulsar_SemVer_ToNumber(CPULSAR_VERSION_CURRENT()))

typedef enum {
    CPulsar_PreReleaseKind_Alpha = 0,
    CPulsar_PreReleaseKind_Beta  = 1,
    CPulsar_PreReleaseKind_RC    = 2,
    CPulsar_PreReleaseKind_None  = 0xFF,
} CPulsar_PreReleaseKind;

typedef struct {
    CPulsar_PreReleaseKind Kind;
    uint8_t                Revision;
} CPulsar_PreRelease;

typedef enum {
    CPulsar_BuildKind_Debug   = 0,
    CPulsar_BuildKind_Release = 1,
} CPulsar_BuildKind;

typedef struct {
    uint16_t Major;
    uint16_t Minor;
    uint16_t Patch;
    CPulsar_PreRelease  Pre;
    CPulsar_BuildKind Build;
} CPulsar_SemVer;

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_SemVer_ToNumber(CPulsar_SemVer self);

// Returns the version of CPulsar referenced by the linked library.
CPULSAR_API CPulsar_SemVer CPULSAR_CALL CPulsar_GetVersion(void);
// Returns the version number of CPulsar referenced by the linked library.
CPULSAR_API uint64_t       CPULSAR_CALL CPulsar_GetVersionNumber(void);

CPULSAR_API CPulsar_SemVer CPULSAR_CALL CPulsar_GetLanguageVersion(void);
CPULSAR_API uint32_t       CPULSAR_CALL CPulsar_GetNeutronVersion(void);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_VERSION_H
