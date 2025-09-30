#ifndef _CPULSAR_PLATFORM_H
#define _CPULSAR_PLATFORM_H

// This a copy-paste of pulsar/platform.h

// https://github.com/cpredef/predef/blob/master/OperatingSystems.md
#if defined(_WIN32)
#  define CPULSAR_PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#  define CPULSAR_PLATFORM_MACOSX
#  define CPULSAR_PLATFORM_UNIX
#elif  defined(__FreeBSD__)   \
    || defined(__NetBSD__)    \
    || defined(__OpenBSD__)   \
    || defined(__bsdi__)      \
    || defined(__DragonFly__) \
    || defined(__MidnightBSD__)
#  define CPULSAR_PLATFORM_BSD
#  define CPULSAR_PLATFORM_UNIX
#elif (defined(sun) || defined(__sun)) && (defined(__SVR4) || defined(__svr4__))
#  define CPULSAR_PLATFORM_SOLARIS
#  define CPULSAR_PLATFORM_UNIX
#elif defined(__linux__)
#  define CPULSAR_PLATFORM_LINUX
#  define CPULSAR_PLATFORM_UNIX
#elif defined(__unix__)
#  define CPULSAR_PLATFORM_UNIX
#else
#  define CPULSAR_PLATFORM_UNKNOWN
#endif

#endif // _CPULSAR_PLATFORM_H
