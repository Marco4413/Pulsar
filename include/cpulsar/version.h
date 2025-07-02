#ifndef _CPULSAR_VERSION_H
#define _CPULSAR_VERSION_H

#include "cpulsar/core.h"

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

CPULSAR_API CPulsar_SemVer CPULSAR_CALL CPulsar_GetLanguageVersion(void);
CPULSAR_API uint32_t       CPULSAR_CALL CPulsar_GetNeutronVersion(void);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_VERSION_H
