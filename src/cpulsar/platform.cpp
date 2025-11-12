#include "cpulsar/platform.h"

#include "pulsar/platform.h"

#if    defined(CPULSAR_PLATFORM_WINDOWS) != defined(PULSAR_PLATFORM_WINDOWS) \
    || defined(CPULSAR_PLATFORM_MACOSX)  != defined(PULSAR_PLATFORM_MACOSX)  \
    || defined(CPULSAR_PLATFORM_BSD)     != defined(PULSAR_PLATFORM_BSD)     \
    || defined(CPULSAR_PLATFORM_SOLARIS) != defined(PULSAR_PLATFORM_SOLARIS) \
    || defined(CPULSAR_PLATFORM_LINUX)   != defined(PULSAR_PLATFORM_LINUX)   \
    || defined(CPULSAR_PLATFORM_UNIX)    != defined(PULSAR_PLATFORM_UNIX)    \
    || defined(CPULSAR_PLATFORM_UNKNOWN) != defined(PULSAR_PLATFORM_UNKNOWN)
#  error Platform mismatch between CPulsar and Pulsar.
#endif // *PULSAR_PLATFORM_UNKNOWN
