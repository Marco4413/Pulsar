#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/parser.h"

#include "pulsar/parser.h"

static_assert(sizeof(CPulsar_ParseResult) >= sizeof(Pulsar::ParseResult));

using Module = Pulsar::Module;
using Parser = Pulsar::Parser;

extern "C"
{

CPULSAR_API const char* CPulsar_ParseResult_ToString(CPulsar_ParseResult parseResult)
{
    return ParseResultToString((Pulsar::ParseResult)parseResult);
}

CPULSAR_API CPulsar_Parser CPulsar_Parser_Create()
{
    return CPULSAR_REF(CPulsar_Parser_S, *PULSAR_NEW(Parser));
}

CPULSAR_API void CPulsar_Parser_Delete(CPulsar_Parser _self)
{
    PULSAR_DELETE(Parser, &CPULSAR_DEREF(Parser, _self));
}

CPULSAR_API CPulsar_ParseResult CPulsar_Parser_AddSourceFile(CPulsar_Parser _self, const char* path)
{
    return (CPulsar_ParseResult)CPULSAR_DEREF(Parser, _self).AddSourceFile(path);
}

CPULSAR_API CPulsar_ParseResult CPulsar_Parser_ParseIntoModule(CPulsar_Parser _self, CPulsar_Module _module)
{
    return (CPulsar_ParseResult)CPULSAR_DEREF(Parser, _self)
        .ParseIntoModule(CPULSAR_DEREF(Module, _module));
}

CPULSAR_API CPulsar_ParseResult CPulsar_Parser_GetError(CPulsar_Parser _self)
{
    return (CPulsar_ParseResult)CPULSAR_DEREF(Parser, _self).GetError();
}

CPULSAR_API const char* CPulsar_Parser_GetErrorMessage(const CPulsar_Parser _self)
{
    return CPULSAR_DEREF(const Parser, _self).GetErrorMessage().CString();
}

}
