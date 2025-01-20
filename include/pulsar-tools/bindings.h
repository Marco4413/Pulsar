#ifndef _PULSARTOOLS_BINDINGS_H
#define _PULSARTOOLS_BINDINGS_H

#include "pulsar/runtime.h"

#include "pulsar-tools/cli.h"
#include "pulsar-tools/bindings/debug.h"
#include "pulsar-tools/bindings/error.h"
#include "pulsar-tools/bindings/filesystem.h"
#include "pulsar-tools/bindings/lexer.h"
#include "pulsar-tools/bindings/module.h"
#include "pulsar-tools/bindings/print.h"
#include "pulsar-tools/bindings/stdio.h"
#include "pulsar-tools/bindings/thread.h"
#include "pulsar-tools/bindings/time.h"

#define __PULSARTOOLS_BINDINGS \
    X(Debug)                   \
    X(Error)                   \
    X(FileSystem)              \
    X(Lexer)                   \
    X(Module)                  \
    X(Print)                   \
    X(Stdio)                   \
    X(Thread)                  \
    X(Time)

namespace PulsarTools
{
    inline void BindNatives(Pulsar::Module& module, const CLI::RuntimeOptions& runtimeOptions, bool declareNatives)
    {
        if (*runtimeOptions.BindThread) {
            Bindings::Channel __Channel;
            __Channel.BindAll(module, declareNatives);
        }

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
