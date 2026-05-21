#ifndef _PULSAR_PLATFORM_H
#define _PULSAR_PLATFORM_H

// Pulsar should always be platform-agnostic, this file
// exists because at some point PULSAR_PLATFORM_* macros
// were defined by the build system globally. This is
// intended to be used by projects which depend on Pulsar
// not by Pulsar itself, this is why it's not included in
// core.h

// https://github.com/cpredef/predef/blob/master/OperatingSystems.md
#if defined(_WIN32)
#  define PULSAR_PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#  define PULSAR_PLATFORM_MACOSX
#  define PULSAR_PLATFORM_UNIX
#elif  defined(__FreeBSD__)   \
    || defined(__NetBSD__)    \
    || defined(__OpenBSD__)   \
    || defined(__bsdi__)      \
    || defined(__DragonFly__) \
    || defined(__MidnightBSD__)
#  define PULSAR_PLATFORM_BSD
#  define PULSAR_PLATFORM_UNIX
#elif (defined(sun) || defined(__sun)) && (defined(__SVR4) || defined(__svr4__))
#  define PULSAR_PLATFORM_SOLARIS
#  define PULSAR_PLATFORM_UNIX
#elif defined(__linux__)
#  define PULSAR_PLATFORM_LINUX
#  define PULSAR_PLATFORM_UNIX
#elif defined(__unix__)
#  define PULSAR_PLATFORM_UNIX
#else
#  define PULSAR_PLATFORM_UNKNOWN
#endif

#endif // _PULSAR_PLATFORM_H
