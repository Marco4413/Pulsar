#include "pulsar-bindings/std.h"

void PulsarBindings::Std::BindAll(Pulsar::Module& module, bool declareAndBind)
{
    #define X(name) \
        PulsarBindings::Std::name std##name;      \
        std##name.BindAll(module, declareAndBind);

    PULSARBINDINGS_STD_X

    #undef X
}
