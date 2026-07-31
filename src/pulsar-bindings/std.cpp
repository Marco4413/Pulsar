#include "pulsar-bindings/std.h"

void PulsarBindings::Std::BindAll(Pulsar::Module& module)
{
    #define X(name) \
        PulsarBindings::Std::name std##name; \
        std##name.BindAll(module);

    PULSARBINDINGS_STD_X

    #undef X
}
