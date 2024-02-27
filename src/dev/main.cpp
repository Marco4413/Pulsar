#include <chrono>

// Pulsar-Dev is a single translation unit.
#include "./natives.hpp"
#include "./prettyprint.hpp"

int main(int argc, const char** argv)
{
    const char* executable = *argv++;
    --argc;

    if (argc == 0) {
        fmt::println("No input file provided to {}.", executable);
        return 1;
    }

    const Pulsar::String program(*argv);
    if (!std::filesystem::exists(program.Data())) {
        fmt::println("{} not found.", program);
        return 1;
    }

    Pulsar::Module module;
    module.DeclareAndBindNativeFunction({ "stack-dump", 0, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame& frame = eContext.CallStack.CallingFrame();
            fmt::print("Stack Dump: [");
            for (size_t i = 0; i < frame.Stack.Size(); i++) {
                if (i > 0) fmt::print(",");
                fmt::print(" {}", frame.Stack[i]);
            }
            fmt::println(" ]");
            return Pulsar::RuntimeState::OK;
        });

    { // Parse Module
        Pulsar::Parser parser;
        parser.AddSourceFile(program);
        auto result = parser.ParseIntoModule(module, Pulsar::ParseSettings_Default);
        if (result != Pulsar::ParseResult::OK) {
            PrintPrettyError(
                parser.GetLastErrorSource(), parser.GetLastErrorPath(),
                parser.GetLastErrorToken(), parser.GetLastErrorMessage(),
                DEFAULT_VIEW_RANGE);
            fmt::println("\nParse Error: {}", (int)result);
            return 1;
        }
    }

    ModuleNativeBindings moduleBindings;
    moduleBindings.BindToModule(module);

    LexerNativeBindings lexerBindings;
    lexerBindings.BindToModule(module);

    module.BindNativeFunction({ "print!", 1, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
            Pulsar::Value& val = frame.Locals[0];
            if (val.Type() == Pulsar::ValueType::String)
                fmt::print("{}", val.AsString());
            else fmt::print("{}", val);
            return Pulsar::RuntimeState::OK;
        });

    module.BindNativeFunction({ "println!", 1, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
            Pulsar::Value& val = frame.Locals[0];
            if (val.Type() == Pulsar::ValueType::String)
                fmt::println("{}", val.AsString());
            else fmt::println("{}", val);
            return Pulsar::RuntimeState::OK;
        });

    module.BindNativeFunction({ "hello-from-cpp", 0, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            (void) eContext;
            fmt::println("Hello from C++!");
            return Pulsar::RuntimeState::OK;
        });

    auto startTime = std::chrono::high_resolution_clock::now();
    Pulsar::ValueStack stack;
    { // Push argv into the Stack.
        Pulsar::ValueList argList;
        for (int i = 0; i < argc; i++)
            argList.Append()->Value().SetString(argv[i]);
        stack.EmplaceBack().SetList(std::move(argList));
    }
    Pulsar::ExecutionContext context = module.CreateExecutionContext();
    auto runtimeState = module.CallFunctionByName("main", stack, context);
    auto stopTime = std::chrono::high_resolution_clock::now();

    auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(stopTime-startTime);
    fmt::println("Execution took: {}us", execTime.count());

    fmt::println("Runtime State: {}", Pulsar::RuntimeStateToString(runtimeState));
    if (runtimeState != Pulsar::RuntimeState::OK) {
        PrintPrettyRuntimeError(context, DEFAULT_VIEW_RANGE);
        fmt::println("");
        return 1;
    }

    fmt::println("Stack Dump:");
    for (size_t i = 0; i < stack.Size(); i++)
        fmt::println("{}. {}", i+1, stack[i]);

    return 0;
}
