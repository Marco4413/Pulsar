#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/parser.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/parser.h"

static_assert(sizeof(CPulsar_ParseResult) >= sizeof(Pulsar::ParseResult));

extern "C"
{

CPULSAR_API const char* CPULSAR_CALL CPulsar_ParseResult_ToString(CPulsar_ParseResult parseResult)
{
    return ParseResultToString((Pulsar::ParseResult)parseResult);
}

CPULSAR_API CPulsar_Parser* CPULSAR_CALL CPulsar_Parser_Create()
{
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::Parser));
}

CPULSAR_API void CPULSAR_CALL CPulsar_Parser_Delete(CPulsar_Parser* _self)
{
    PULSAR_DELETE(Pulsar::Parser, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API CPulsar_ParseResult CPULSAR_CALL CPulsar_Parser_AddSourceFile(CPulsar_Parser* _self, const char* path)
{
    return (CPulsar_ParseResult)CPULSAR_UNWRAP(_self).AddSourceFile(path);
}

CPULSAR_API CPulsar_ParseResult CPULSAR_CALL CPulsar_Parser_ParseIntoModule(CPulsar_Parser* _self, CPulsar_Module* _module)
{
    return (CPulsar_ParseResult)CPULSAR_UNWRAP(_self)
        .ParseIntoModule(CPULSAR_UNWRAP(_module));
}

CPULSAR_API CPulsar_ParseResult CPULSAR_CALL CPulsar_Parser_GetErrorMessage_Reason(const CPulsar_Parser* _self)
{
    return (CPulsar_ParseResult)CPULSAR_UNWRAP(_self).GetErrorMessage().Reason;
}

CPULSAR_API const char* CPULSAR_CALL CPulsar_Parser_GetErrorMessage_Message(const CPulsar_Parser* _self)
{
    return CPULSAR_UNWRAP(_self).GetErrorMessage().Message.CString();
}

}
