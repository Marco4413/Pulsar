#ifndef _PULSAR_ARCHITECTURE_H
#define _PULSAR_ARCHITECTURE_H

// This file exists for the same reason as platform.h
// Here's the copy-pasted quote:
//   Pulsar should always be platform-agnostic, this file
//   exists because at some point PULSAR_PLATFORM_* macros
//   were defined by the build system globally. This is
//   intended to be used by projects which depend on Pulsar
//   not by Pulsar itself, this is why it's not included in
//   core.h

// https://github.com/cpredef/predef/blob/master/Architectures.md
#if defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#  define PULSAR_ARCHITECTURE_AMD64
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define PULSAR_ARCHITECTURE_ARM64
#else
#  define PULSAR_ARCHITECTURE_UNKNOWN
#endif

#endif // _PULSAR_ARCHITECTURE_H
