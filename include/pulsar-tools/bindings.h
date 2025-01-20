#ifndef _PULSARTOOLS_BINDINGS_H
#define _PULSARTOOLS_BINDINGS_H

#include "pulsar/runtime.h"

#include "pulsar-tools/cli.h"
#include "pulsar-tools/bindings/debug.h"
#include "pulsar-tools/bindings/print.h"

#define __PULSARTOOLS_BINDINGS \
    X(Debug)                   \
    X(Print)

namespace PulsarTools
{
    inline void BindNatives(Pulsar::Module& module, const CLI::RuntimeOptions& runtimeOptions, bool declareNatives)
    {
        #define X(name)                          \
            if (*runtimeOptions.Bind##name) {    \
                Bindings::name __##name;         \
                __##name.BindAll(                \
                        module, declareNatives); \
            }

        __PULSARTOOLS_BINDINGS

        #undef X
    }
}

#endif // _PULSARTOOLS_BINDINGS_H
