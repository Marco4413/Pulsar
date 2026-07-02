#include "pulsar-tools/bindings/module.h"

#include "pulsar/parser.h"

PulsarTools::Bindings::Module::Module() :
    IBinding()
{
    BindCustomType("Pulsar-Tools/Module");
    BindNativeFunction({ "module/from-file", 1, 1 }, CreateTypeBoundFactory(FFromFile, "Pulsar-Tools/Module"));
    BindNativeFunction({ "module/run",       1, 1 }, CreateTypeBoundFactory(FRun,      "Pulsar-Tools/Module"));
    BindNativeFunction({ "module/valid?",    1, 1 }, CreateTypeBoundFactory(FIsValid,  "Pulsar-Tools/Module"));
}

Pulsar::RuntimeState PulsarTools::Bindings::Module::FFromFile(Pulsar::ExecutionContext& eContext, uint64_t moduleTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& modulePath = frame.Locals[0];
    if (modulePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Parser parser;
    auto result = parser.AddSourceFile(modulePath.AsString());
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceCustom({ moduleTypeId, nullptr });
        return Pulsar::RuntimeState::OK;
    }

    ModuleType::Ref module = ModuleType::Ref::New();
    result = parser.ParseIntoModule(*module, Pulsar::ParseSettings_Default);
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceCustom({ moduleTypeId, nullptr });
        return Pulsar::RuntimeState::OK;
    }

    frame.Stack.EmplaceCustom({ moduleTypeId, module });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Module::FRun(Pulsar::ExecutionContext& eContext, uint64_t moduleTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& moduleReference = frame.Locals[0];

    ModuleType::Ref module;
    if (!moduleReference.GetCustomAs(moduleTypeId, module))
        return Pulsar::RuntimeState::TypeError;
    if (!module) return Pulsar::RuntimeState::InvalidCustomTypeReference;

    Pulsar::ExecutionContext context(*module);
    context.CallFunction("main");
    auto runtimeState = context.Run();
    if (runtimeState != Pulsar::RuntimeState::OK)
        return runtimeState;

    Pulsar::Value::List returnValues;
    for (Pulsar::Value& value : context.GetStack()) returnValues.Append(std::move(value));
    frame.Stack.EmplaceList(std::move(returnValues));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Module::FIsValid(Pulsar::ExecutionContext& eContext, uint64_t moduleTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& moduleReference = frame.Locals[0];

    ModuleType::Ref module;
    if (!moduleReference.GetCustomAs(moduleTypeId, module))
        return Pulsar::RuntimeState::TypeError;

    frame.Stack.EmplaceInteger(module ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
