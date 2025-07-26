#ifndef _PULSARDEBUGGER_HELPERS_H
#define _PULSARDEBUGGER_HELPERS_H

// FIXME: This is required by value.h because LinkedList defines conversion methods.
#include <pulsar/structures/list.h>
#include <pulsar/runtime/value.h>

namespace PulsarDebugger
{
    // TODO: Add this to Pulsar (it's also used by the LSP)
    const char* ValueTypeToString(Pulsar::ValueType type);
    Pulsar::String ValueToString(const Pulsar::Value& value, bool recursive=false);
}

#endif // _PULSARDEBUGGER_HELPERS_H
