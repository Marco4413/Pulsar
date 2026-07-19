#ifndef _PULSARTOOLS_BINDINGS_H
#define _PULSARTOOLS_BINDINGS_H

#include "pulsar/runtime.h"

#include "pulsar-tools/cli.h"
#include "pulsar-bindings/std.h"

namespace PulsarTools
{
    inline void BindNatives(Pulsar::Module& module, const CLI::RuntimeOptions& runtimeOptions, bool declareNatives)
    {
        #define X(name)                             \
            if (*runtimeOptions.Bind##name) {       \
                PulsarBindings::Std::name __##name; \
                __##name.BindAll(                   \
                        module, declareNatives);    \
            }

        PULSARBINDINGS_STD_X

        #undef X
    }
}

#endif // _PULSARTOOLS_BINDINGS_H
