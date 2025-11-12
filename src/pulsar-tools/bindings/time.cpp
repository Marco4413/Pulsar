#include "pulsar-tools/bindings/time.h"

#include <chrono>
#include <ctime>

PulsarTools::Bindings::Time::Time() :
    IBinding()
{
    BindNativeFunction({ "time", 0, 1 }, FTime);
    BindNativeFunction({ "time/steady", 0, 1 }, FSteady);
    BindNativeFunction({ "time/micros", 0, 1 }, FMicros);
}

Pulsar::RuntimeState PulsarTools::Bindings::Time::FTime(Pulsar::ExecutionContext& eContext)
{
    auto time = std::chrono::system_clock::now();
    auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    eContext.CurrentFrame()
        .Stack.EmplaceInteger((int64_t)timeSinceEpoch.count());
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Time::FSteady(Pulsar::ExecutionContext& eContext)
{
    auto time = std::chrono::steady_clock::now();
    auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    eContext.CurrentFrame()
        .Stack.EmplaceInteger((int64_t)timeSinceEpoch.count());
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Time::FMicros(Pulsar::ExecutionContext& eContext)
{
    // Use the high resolution clock only if it's monotonic
    if constexpr (std::chrono::high_resolution_clock::is_steady) {
        auto time = std::chrono::high_resolution_clock::now();
        auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch());
        eContext.CurrentFrame()
            .Stack.EmplaceInteger((int64_t)timeSinceEpoch.count());
    } else {
        auto time = std::chrono::steady_clock::now();
        auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch());
        eContext.CurrentFrame()
            .Stack.EmplaceInteger((int64_t)timeSinceEpoch.count());
    }
    return Pulsar::RuntimeState::OK;
}
