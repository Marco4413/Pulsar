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

Pulsar::RuntimeState PulsarTools::Bindings::Module::FFromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& modulePath = frame.Locals[0];
    if (modulePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Parser parser;
    auto result = parser.AddSourceFile(modulePath.AsString());
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceBack()
            .SetCustom({ type });
        return Pulsar::RuntimeState::OK;
    }
    
    ModuleType::Ref module = ModuleType::Ref::New();
    result = parser.ParseIntoModule(*module, Pulsar::ParseSettings_Default);
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceBack()
            .SetCustom({ type });
        return Pulsar::RuntimeState::OK;
    }

    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Data=module });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Module::FRun(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& moduleRef = frame.Locals[0];
    if (moduleRef.Type() != Pulsar::ValueType::Custom
        || moduleRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ModuleType::Ref module = moduleRef.AsCustom().As<ModuleType>();

    Pulsar::ExecutionContext context(*module);
    context.CallFunction("main");
    auto runtimeState = context.Run();
    if (runtimeState != Pulsar::RuntimeState::OK)
        return runtimeState;

    Pulsar::ValueList retValues(std::move(context.GetStack()));
    frame.Stack.EmplaceBack()
        .SetList(std::move(retValues));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Module::FIsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& moduleRef = frame.Locals[0];
    if (moduleRef.Type() != Pulsar::ValueType::Custom
        || moduleRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ModuleType::Ref module = moduleRef.AsCustom().As<ModuleType>();
    frame.Stack.EmplaceBack()
        .SetInteger(module ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
