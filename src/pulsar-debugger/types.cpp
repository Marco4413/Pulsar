#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{

ThreadId ComputeThreadId(const Pulsar::ExecutionContext& thread)
{
    return reinterpret_cast<ThreadId>(&thread);
}

}
