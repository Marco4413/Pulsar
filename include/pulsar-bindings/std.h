#ifndef _PULSARBINDINGS_STD_H
#define _PULSARBINDINGS_STD_H

#include "pulsar/runtime/module.h"

#include "pulsar-bindings/std/debug.h"
#include "pulsar-bindings/std/error.h"
#include "pulsar-bindings/std/filesystem.h"
#include "pulsar-bindings/std/lexer.h"
#include "pulsar-bindings/std/module.h"
#include "pulsar-bindings/std/print.h"
#include "pulsar-bindings/std/stdio.h"
#include "pulsar-bindings/std/thread.h"
#include "pulsar-bindings/std/time.h"

#define PULSARBINDINGS_STD_X \
    X(Debug)                 \
    X(Error)                 \
    X(FileSystem)            \
    X(Lexer)                 \
    X(Module)                \
    X(Print)                 \
    X(Stdio)                 \
    X(Thread)                \
    X(Time)

namespace PulsarBindings::Std
{
    void BindAll(Pulsar::Module& module, bool declareAndBind=false);
}

#endif // _PULSARBINDINGS_STD_H
