/*
This file will teach you how to read a Pulsar source file and run it!
By reading this demo you should be able to get Pulsar up and running within your project.
*/

// fmt is just used for logging, use whatever logging library you have
//   (or don't log at all, I won't judge you).
#include "fmt/format.h"

// Both the Parser and all runtime structures are needed.
// Pulsar is split into 4 parts: Lexer, Parser, Runtime and Binary.
// Where Binary is split into more parts as seen later.
// You should always include what you need and don't assume each of those parts includes another one.
// i.e. Runtime includes the Lexer to save debug information, that may change in the future (it may just include Token). 
#include "pulsar/parser.h"
#include "pulsar/runtime.h"

// Uncomment the following line if you want to read a Neutron file
// #define READ_AS_NEUTRON_FILE

#ifdef READ_AS_NEUTRON_FILE
// The Parser is not needed when reading a Neutron file.
// For simplicity, I've left the include outside of this if.
#include "pulsar/binary/filereader.h"
#include "pulsar/bytecode.h"
#endif // READ_AS_NEUTRON_FILE

int main(int argc, const char** argv)
{
    // This demo uses hard-coded paths.
    // argv is ignored.
    (void)argc; (void)argv;

#ifdef READ_AS_NEUTRON_FILE

    // readResult will contain the result of the last read.
    Pulsar::Binary::ReadResult readResult = Pulsar::Binary::ReadResult::OK;
    // Creating a FileReader (which is derived from Pulsar::Binary::IReader).
    // Note: FileReader does not check for the existance of the file.
    //       If the file does not exist, ReadByteCode will return UnexpectedEOF.
    Pulsar::Binary::FileReader fileReader("examples/neutron/00-smallest-neutron.ntr");
    Pulsar::Module module;
    Pulsar::Binary::ReadSettings readSettings{
        // Tell the reader to ignore any debug symbol.
        // If there aren't any in the file, the value of this setting does not matter.
        .LoadDebugSymbols = false,
    };
    // Read the byte code referenced by fileReader.
    readResult = Pulsar::Binary::ReadByteCode(fileReader, module, readSettings);
    if (readResult != Pulsar::Binary::ReadResult::OK) {
        // Read failed, module may contain an
        //   "half-baked" representation of the file.
        // So its data is to be treated as invalid.

        fmt::println("[READ ERROR]: {}",
            Pulsar::Binary::ReadResultToString(readResult));
        return 1;
    }

#else // READ_AS_NEUTRON_FILE

    // parseResult will contain the result of the last method ran on the Parser.
    Pulsar::ParseResult parseResult = Pulsar::ParseResult::OK;
    // The Parser constructor does not take any argument.
    Pulsar::Parser parser;
    // Adding a source file to the Parser.
    // This method is also used internally by the #include directive.
    // You can basically see the Parser as an empty file in which you add
    //   #include "examples/logo.pls"
    // where the path is relative to the current working directory.
    parseResult = parser.AddSourceFile("examples/logo.pls");
    if (parseResult != Pulsar::ParseResult::OK) {
        // Pulsar::String is null-terminated, there's no need to worry about reading invalid memory.
        fmt::println("[PARSE ERROR]: {}: {}",
            Pulsar::ParseResultToString(parseResult),
            parser.GetErrorMessage().CString());
        return 1;
    }

    // A Module contains a representation of the file which can be
    //   interpreted by the Pulsar VM. In this case we default-initialize it.
    Pulsar::Module module;
    // This struct will hold the settings to be used by the Parser.
    Pulsar::ParseSettings parseSettings{
        // We don't want #include to be used!
        .AllowIncludeDirective = false,
        // You can override the behaviour of the #include directive!
        // You can extend Pulsar to accept files over HTTP or any other protocol.
        // Or more commonly to resolve for common include directories.
        // The default behaviour is to resolve the path relative to the current file,
        //   then call parser.AddSourceFile with the resolved path.
        // parser.AddSourceFile also makes sure to standardize the format so that the file may not be included twice.
        // If using parser.AddSource you should be careful to do that yourself.
        .IncludeResolver = nullptr,
    };
    // Now it's time to tell the Parser to do its magic and translate the source code.
    parseResult = parser.ParseIntoModule(module, parseSettings);
    if (parseResult != Pulsar::ParseResult::OK) {
        // Here the Parser failed, module may contain
        //   an "half-baked" representation of the file.
        // So its data is to be treated as invalid.

        // Getting the Token where the error happened.
        const Pulsar::Token& errorToken = parser.GetErrorToken();
        fmt::println("[PARSE ERROR]: {}:{}:{}: {}: {}",
            // The path of the file which caused the error.
            parser.GetErrorPath()->Data(),
            // Line and Char within the line were the error occurred.
            errorToken.SourcePos.Line+1,
            errorToken.SourcePos.Char,
            Pulsar::ParseResultToString(parseResult),
            parser.GetErrorMessage().CString());
        return 1;
    }

#endif // READ_AS_NEUTRON_FILE

    // The logo example needs a native function called "println!" to be ran.
    // Module::BindNativeFunction returns how many definitions matched and, therefore, were bound.
    module.BindNativeFunction({
        .Name = "println!",
        .Arity = 1,
        .Returns = 0,
        // This is not necessary as by default it's the same as Arity.
        .LocalsCount = 1,
    }, [](Pulsar::ExecutionContext& eContext) {
        // We get the frame of this function call.
        Pulsar::Frame& frame = eContext.CurrentFrame();
        // Since Arity and LocalsCount were set to 1,
        //   frame.Locals[0] exists and is some Value.
        const Pulsar::Value& str = frame.Locals[0];
        // Make sure that it's a String and print it.
        // (this is done to simplify this implementation from the one
        //   pulsar-tools uses. pulsar-tools allows any value to be printed)
        if (str.Type() != Pulsar::ValueType::String)
            return Pulsar::RuntimeState::TypeError;
        fmt::println("{}", str.AsString().CString());
        return Pulsar::RuntimeState::OK;
    });

    // A Module only contains the "code", it does not run it.
    // So an ExecutionContext is needed, and we can create it from a module.
    // ExecutionContext holds a reference to the specified module, so make sure
    //   the module outlives the context.
    Pulsar::ExecutionContext context(module);
    // We prepare the initial stack which must contain all arguments
    //   needed by the function to be called (main only needs a List).
    Pulsar::ValueStack& stack = context.GetStack();
    stack.EmplaceBack()
        .SetList(Pulsar::ValueList());
    // Call the last function named "main". We can assume it accepts 1 argument (args) and returns anything.
    context.CallFunction("main");
    Pulsar::RuntimeState runtimeState = context.Run();
    if (runtimeState != Pulsar::RuntimeState::OK) {
        // Fortunately ExecutionContext::GetStackTrace exists
        //   and printing the stack trace is very simple.
        // It accepts a single argument which is the depth of the trace (how many calls to show).
        // It works because ExecutionContext is not modified on error, so it is the last state the VM was in.
        Pulsar::String stackTrace = context.GetStackTrace(10);
        fmt::println("[RUNTIME ERROR]: {}\n{}",
            Pulsar::RuntimeStateToString(runtimeState),
            stackTrace.CString());
        return 1;
    }

    // Now stack contains the values returned by the call.
    // In case "main" does not take any argument, whatever
    //   was put on top of the stack is not consumed.
    fmt::println("Items on the Stack: {}", stack.Size());
    return 0;
}
