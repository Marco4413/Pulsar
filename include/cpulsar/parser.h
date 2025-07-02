#ifndef _CPULSAR_PARSER_H
#define _CPULSAR_PARSER_H

#include "cpulsar/core.h"
#include "cpulsar/runtime.h"

// If != 0 an error occurred
typedef int CPulsar_ParseResult;
typedef struct CPulsar_Parser_S* CPulsar_Parser;

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API const char* CPULSAR_CALL CPulsar_ParseResult_ToString(CPulsar_ParseResult parseResult);

CPULSAR_API CPulsar_Parser CPULSAR_CALL CPulsar_Parser_Create();
CPULSAR_API void           CPULSAR_CALL CPulsar_Parser_Delete(CPulsar_Parser self);

CPULSAR_API CPulsar_ParseResult CPULSAR_CALL CPulsar_Parser_AddSourceFile(CPulsar_Parser self, const char* path);
CPULSAR_API CPulsar_ParseResult CPULSAR_CALL CPulsar_Parser_ParseIntoModule(CPulsar_Parser self, CPulsar_Module module);
CPULSAR_API const char*         CPULSAR_CALL CPulsar_Parser_GetErrorMessage(const CPulsar_Parser self);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_PARSER_H
