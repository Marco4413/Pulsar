#include "pulsar-tools/bindings/time.h"

#include <chrono>
#include <ctime>

void PulsarTools::TimeNativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "time", 0, 1 }, Time_Time);
    module.BindNativeFunction({ "time/steady", 0, 1 }, Time_Steady);
    module.BindNativeFunction({ "time/micros", 0, 1 }, Time_Micros);
}

Pulsar::RuntimeState PulsarTools::TimeNativeBindings::Time_Time(Pulsar::ExecutionContext& eContext)
{
    auto time = std::chrono::system_clock::now();
    auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    eContext.CurrentFrame().Stack
        .EmplaceBack().SetInteger((int64_t)timeSinceEpoch.count());
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::TimeNativeBindings::Time_Steady(Pulsar::ExecutionContext& eContext)
{
    auto time = std::chrono::steady_clock::now();
    auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
    eContext.CurrentFrame().Stack
        .EmplaceBack().SetInteger((int64_t)timeSinceEpoch.count());
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::TimeNativeBindings::Time_Micros(Pulsar::ExecutionContext& eContext)
{
    // Use the high resolution clock only if it's monotonic
    if constexpr (std::chrono::high_resolution_clock::is_steady) {
        auto time = std::chrono::high_resolution_clock::now();
        auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch());
        eContext.CurrentFrame().Stack
            .EmplaceBack().SetInteger((int64_t)timeSinceEpoch.count());
    } else {
        auto time = std::chrono::steady_clock::now();
        auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch());
        eContext.CurrentFrame().Stack
            .EmplaceBack().SetInteger((int64_t)timeSinceEpoch.count());
    }
    return Pulsar::RuntimeState::OK;
}
