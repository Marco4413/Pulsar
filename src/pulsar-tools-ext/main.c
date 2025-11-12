#include "cpulsar/runtime.h"
#include "cpulsar/version.h"

#include <stdio.h>
#include <string.h> // memcpy

CPulsar_RuntimeState CPULSAR_CALL NativePrintln(CPulsar_ExecutionContext context, void* args)
{
    (void)args;

    CPulsar_Frame  frame  = CPulsar_ExecutionContext_CurrentFrame(context);
    CPulsar_Locals locals = CPulsar_Frame_GetLocals(frame);
    CPulsar_Value  msg    = CPulsar_Locals_Get(locals, 0);
    if (!CPulsar_Value_IsString(msg)) {
        return CPulsar_RuntimeState_Error;
    }

    printf("%s\n", CPulsar_Value_AsString(msg));
    return CPulsar_RuntimeState_OK;
}

void CPULSAR_CALL ExtIntegerData_Free(void* data);
CPulsar_CustomTypeGlobalData_Ref CPULSAR_CALL ExtIntegerData_Factory(void);

void CPULSAR_CALL ExtInteger_Free(void* data);
CPulsar_CBuffer CPULSAR_CALL ExtInteger_Create(int64_t initValue);

typedef struct {
    uint64_t ExtIntegerId;
} CustomTypeIds;

CPulsar_CBuffer CPULSAR_CALL CustomTypeIdsToBuffer(CustomTypeIds data);

CPulsar_RuntimeState CPULSAR_CALL NativeExtIntegerCreate(CPulsar_ExecutionContext context, void* args);
CPulsar_RuntimeState CPULSAR_CALL NativeExtIntegerSet(CPulsar_ExecutionContext context, void* args);
CPulsar_RuntimeState CPULSAR_CALL NativeExtIntegerGet(CPulsar_ExecutionContext context, void* args);

// Returns the version of Pulsar this binding was made for
CPULSAR_EXPORT uint64_t CPULSAR_CALL GetPulsarVersion(void)
{
    // SemVer::Build can be ignored
    return CPulsar_SemVer_ToNumber((CPulsar_SemVer){
        .Major = 0,
        .Minor = 7,
        .Patch = 0,
        .Pre   = {
            .Kind     = CPulsar_PreReleaseKind_Beta,
            .Revision = 0,
        },
    });
}

CPULSAR_EXPORT void CPULSAR_CALL BindTypes(CPulsar_Module module)
{
    CPulsar_Module_BindCustomType(module, "Ext/Integer", ExtIntegerData_Factory);
}

CPULSAR_EXPORT void CPULSAR_CALL BindFunctions(CPulsar_Module module, int declareAndBind)
{
    CPulsar_Module_BindNativeFunctionEx(
        module,
        (CPulsar_FunctionSignature){
            .Name = "println!",
            .Arity = 1,
            .Returns = 0,
        },
        NativePrintln,
        CPULSAR_CBUFFER_NULL,
        declareAndBind
    );

    CustomTypeIds typeIds = {
        .ExtIntegerId = CPulsar_Module_FindCustomType(module, "Ext/Integer"),
    };
    CPulsar_Module_BindNativeFunctionEx(
        module,
        (CPulsar_FunctionSignature){
            .Name = "ext/integer/create",
            .Arity = 0,
            .Returns = 1,
        },
        NativeExtIntegerCreate,
        CustomTypeIdsToBuffer(typeIds),
        declareAndBind
    );
    CPulsar_Module_BindNativeFunctionEx(
        module,
        (CPulsar_FunctionSignature){
            .Name = "ext/integer/set",
            .Arity = 2,
            .Returns = 1,
        },
        NativeExtIntegerSet,
        CustomTypeIdsToBuffer(typeIds),
        declareAndBind
    );
    CPulsar_Module_BindNativeFunctionEx(
        module,
        (CPulsar_FunctionSignature){
            .Name = "ext/integer/get",
            .Arity = 1,
            .Returns = 2,
        },
        NativeExtIntegerGet,
        CustomTypeIdsToBuffer(typeIds),
        declareAndBind
    );
}

void CPULSAR_CALL ExtIntegerData_Free(void* data)
{
    printf("ExtIntegerData_Free\n");
    CPulsar_Free(data);
}

void* CPULSAR_CALL ExtIntegerData_Copy(void* data)
{
    printf("ExtIntegerData_Copy\n");
    int64_t* dataCopy = CPulsar_Malloc(sizeof(int64_t));
    // Increase the number stored on each copy.
    // Obviously, this is for demonstration purposes.
    *dataCopy = (*(int64_t*)data) + 1;
    return dataCopy;
}

CPulsar_CustomTypeGlobalData_Ref CPULSAR_CALL ExtIntegerData_Factory(void)
{
    printf("ExtIntegerData_Factory\n");
    int64_t* data = CPulsar_Malloc(sizeof(int64_t));
    *data = 1;
    return CPulsar_CustomTypeGlobalData_Ref_FromBuffer(
        (CPulsar_CBuffer){
            .Data = data,
            .Free = ExtIntegerData_Free,
            .Copy = ExtIntegerData_Copy,
        }
    );
}

void CPULSAR_CALL ExtInteger_Free(void* data)
{
    printf("ExtInteger_Free\n");
    CPulsar_Free(data);
}

CPulsar_CBuffer CPULSAR_CALL ExtInteger_Create(int64_t initValue)
{
    printf("ExtInteger_Create\n");
    int64_t* data = CPulsar_Malloc(sizeof(int64_t));
    *data = initValue;
    return (CPulsar_CBuffer){
        .Data = data,
        .Free = ExtInteger_Free,
    };
}

CPulsar_CBuffer CPULSAR_CALL CustomTypeIdsToBuffer(CustomTypeIds data)
{
    CPulsar_CBuffer buffer = {
        .Data = CPulsar_Malloc(sizeof(data)),
        .Free = CPulsar_Free,
    };
    memcpy(buffer.Data, &data, sizeof(data));
    return buffer;
}

CPulsar_RuntimeState CPULSAR_CALL NativeExtIntegerCreate(CPulsar_ExecutionContext context, void* args)
{
    CustomTypeIds* typeIds = args;

    CPulsar_Frame frame = CPulsar_ExecutionContext_CurrentFrame(context);
    CPulsar_Stack stack = CPulsar_Frame_GetStack(frame);

    CPulsar_CBuffer* typeData = CPulsar_ExecutionContext_GetCustomTypeGlobalDataBuffer(context, typeIds->ExtIntegerId);
    int64_t initValue = *(int64_t*)(typeData->Data);

    CPulsar_Value extIntegerVal = CPulsar_Stack_Emplace(stack);
    CPulsar_Value_SetCustomBuffer(extIntegerVal, typeIds->ExtIntegerId, ExtInteger_Create(initValue));

    return CPulsar_RuntimeState_OK;
}

CPulsar_RuntimeState CPULSAR_CALL NativeExtIntegerSet(CPulsar_ExecutionContext context, void* args)
{
    CustomTypeIds* typeIds = args;

    CPulsar_Frame  frame  = CPulsar_ExecutionContext_CurrentFrame(context);
    CPulsar_Locals locals = CPulsar_Frame_GetLocals(frame);
    CPulsar_Stack  stack  = CPulsar_Frame_GetStack(frame);

    CPulsar_Value extIntegerVal = CPulsar_Locals_Get(locals, 0);
    CPulsar_Value newVal = CPulsar_Locals_Get(locals, 1);

    if (!CPulsar_Value_IsCustom(extIntegerVal) ||
        !CPulsar_Value_IsInteger(newVal))
        return CPulsar_RuntimeState_Error;

    CPulsar_CBuffer* extInteger = CPulsar_Value_AsCustomBuffer(extIntegerVal, typeIds->ExtIntegerId);
    if (!extInteger) return CPulsar_RuntimeState_Error;

    *(int64_t*)extInteger->Data = CPulsar_Value_AsInteger(newVal);

    CPulsar_Stack_Push(stack, extIntegerVal);
    return CPulsar_RuntimeState_OK;
}

CPulsar_RuntimeState CPULSAR_CALL NativeExtIntegerGet(CPulsar_ExecutionContext context, void* args)
{
    CustomTypeIds* typeIds = args;

    CPulsar_Frame  frame  = CPulsar_ExecutionContext_CurrentFrame(context);
    CPulsar_Locals locals = CPulsar_Frame_GetLocals(frame);
    CPulsar_Stack  stack  = CPulsar_Frame_GetStack(frame);

    CPulsar_Value extIntegerVal = CPulsar_Locals_Get(locals, 0);

    if (!CPulsar_Value_IsCustom(extIntegerVal))
        return CPulsar_RuntimeState_Error;

    CPulsar_CBuffer* extInteger = CPulsar_Value_AsCustomBuffer(extIntegerVal, typeIds->ExtIntegerId);
    if (!extInteger) return CPulsar_RuntimeState_Error;

    CPulsar_Value val = CPulsar_Value_Create();
    CPulsar_Value_SetInteger(val, *(int64_t*)extInteger->Data);

    CPulsar_Stack_Push(stack, extIntegerVal);
    CPulsar_Stack_Push(stack, val);

    CPulsar_Value_Delete(val);

    return CPulsar_RuntimeState_OK;
}
