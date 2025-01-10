#ifndef _PULSARLSP_COMPLETION_H
#define _PULSARLSP_COMPLETION_H

#include <lsp/types.h>

#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/string.h"

namespace PulsarLSP::Completion
{
    // An instruction name to completion item map.
    using InstructionMap = Pulsar::HashMap<std::string, lsp::CompletionItem>;
    const InstructionMap& GetInstructions();
}

#endif // _PULSARLSP_COMPLETION_H
