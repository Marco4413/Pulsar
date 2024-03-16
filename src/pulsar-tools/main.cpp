#include "pulsar-tools/core.h"
#include "pulsar-tools/print.h"

#include "pulsar-tools/bindings/debug.h"
#include "pulsar-tools/bindings/lexer.h"
#include "pulsar-tools/bindings/module.h"
#include "pulsar-tools/bindings/print.h"
#include "pulsar-tools/bindings/thread.h"

#include "pulsar/runtime.h"
#include "pulsar/parser.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/string.h"

#include <chrono>
#include <cstring>
#include <filesystem>

inline const char* ShiftArgs(int& argc, const char**& argv)
{
    if (argc <= 0)
        return nullptr;
    --argc;
    return *argv++;
}

size_t StrIndexOf(const char* str, char ch)
{
    size_t strlen = std::strlen(str);
    size_t idx = 0;
    for (; idx < strlen && str[idx] != ch; idx++);
    return idx;
}

struct FlagOption
{
    uint32_t Flag;
    bool IsInverse = false;
    const char* Description = nullptr;
};

constexpr uint32_t P_DEBUG_SYMBOLS = 1;
constexpr uint32_t P_STACK_TRACE   = 2;
constexpr uint32_t P_ERROR_NOTES   = 4;
constexpr uint32_t P_ALLOW_INCLUDE = 8;
constexpr uint32_t P_DEFAULT       =
      P_DEBUG_SYMBOLS
    | P_STACK_TRACE
    | P_ERROR_NOTES
    | P_ALLOW_INCLUDE;

constexpr uint32_t R_BIND_DEBUG  =  1 << 16;
constexpr uint32_t R_BIND_LEXER  =  2 << 16;
constexpr uint32_t R_BIND_MODULE =  4 << 16;
constexpr uint32_t R_BIND_PRINT  =  8 << 16;
constexpr uint32_t R_BIND_THREAD = 16 << 16;
constexpr uint32_t R_BIND_ALL    =
      R_BIND_DEBUG
    | R_BIND_LEXER
    | R_BIND_MODULE
    | R_BIND_PRINT
    | R_BIND_THREAD;

struct NamedFlagOption
{
    Pulsar::String Name;
    FlagOption Option;
};

#define PULSARTOOLS_FLAG_OPTIONS(optName, ...)                                 \
    const Pulsar::List<NamedFlagOption> optName##_Ordered { __VA_ARGS__ };     \
    const Pulsar::HashMap<Pulsar::String, FlagOption> optName { __VA_ARGS__ };

#define PULSARTOOLS_FLAG_OPTION_LONG(cLong, cShort, sLong, flag, desc) \
    { cShort "/"    sLong, { (flag) } },                               \
    { cShort "/no-" sLong, { (flag), true } },                         \
    { cLong  "/"    sLong, { (flag) } },                               \
    { cLong  "/no-" sLong, { (flag), true, desc } }

#define PULSARTOOLS_FLAG_OPTION(cLong, cShort, sLong, sShort, flag, desc) \
    { cShort "/"    sShort, { (flag) } },                                 \
    { cShort "/no-" sShort, { (flag), true } },                           \
    { cLong  "/"    sLong,  { (flag) } },                                 \
    { cLong  "/no-" sLong,  { (flag), true, desc } }

PULSARTOOLS_FLAG_OPTIONS(ParserOptions,
    PULSARTOOLS_FLAG_OPTION("--parser", "-p", "debug-symbols", "debug", P_DEBUG_SYMBOLS, "Store debug symbols for better runtime error information."),
    PULSARTOOLS_FLAG_OPTION("--parser", "-p", "stack-trace",   "st",    P_STACK_TRACE,   "Enable stack trace for compile-time evaluation errors."),
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "error-notes",   P_ERROR_NOTES,   "Enable notes about errors."),
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "allow-include", P_ALLOW_INCLUDE, "Allow the usage of the include directive."),
)

PULSARTOOLS_FLAG_OPTIONS(RuntimeOptions,
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-debug",  "b-debug",  R_BIND_DEBUG,  "Bind Debug natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-lexer",  "b-lexer",  R_BIND_LEXER,  "Bind Lexer natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-module", "b-module", R_BIND_MODULE, "Bind Module natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-print",  "b-print",  R_BIND_PRINT,  "Bind Print natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-thread", "b-thread", R_BIND_THREAD, "Bind Thread natives.\n(passing handles to threads is not supported)"),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-all",    "b-all",    R_BIND_ALL,    "Bind all available natives. (default: true)"),
)

void PrintFlagOptions(const Pulsar::List<NamedFlagOption>& opts)
{
    for (size_t i = 0; i < opts.Size(); i++) {
        const NamedFlagOption& namedOption = opts[i];
        PULSARTOOLS_INFOF("    {}", namedOption.Name);
        if (namedOption.Option.Description) {
            // Split description lines
            Pulsar::StringView view(namedOption.Option.Description, std::strlen(namedOption.Option.Description));
            do {
                size_t idx = StrIndexOf(view.CStringFrom(0), '\n');
                Pulsar::String line = view.GetPrefix(idx);
                PULSARTOOLS_INFOF("        {0}", line);
                view.RemovePrefix(idx+1);
            } while (!view.Empty());
            PULSARTOOLS_INFO("");
        }
    }
}

void PrintCheckCommandUsage(const char* executable)
{
    PULSARTOOLS_INFOF("{} check [...PARSER_OPTIONS] <filepath>", executable);
    PULSARTOOLS_INFO("PARSER_OPTIONS:");
    PrintFlagOptions(ParserOptions_Ordered);
}

bool Command_Check(const char* executable, int argc, const char** argv)
{
    if (argc <= 0) {
        PrintCheckCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} check: No file provided.", executable);
        return false;
    }

    Pulsar::String filepath;
    uint32_t flagOptions = P_DEFAULT;

    while (argc > 0) {
        filepath = ShiftArgs(argc, argv);
        if (filepath.Length() > 0 && filepath[0] != '-')
            break;

        const FlagOption* opt = nullptr;
        auto nameOptPair = ParserOptions.Find(filepath);
        
        if (nameOptPair) {
            opt = nameOptPair.Value;
        } else {
            PrintCheckCommandUsage(executable);
            PULSARTOOLS_ERRORF("{} check: Invalid option '{}'.", executable, filepath);
            return false;
        }

        PULSARTOOLS_ASSERT(opt, "Invalid FlagOption* state.");
        if (opt->IsInverse)
            flagOptions &= ~opt->Flag;
        else flagOptions |= opt->Flag;
    }

    if (filepath.Length() > 0 && filepath[0] == '-') {
        PrintCheckCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} check: No file provided.", executable);
        return false;
    } else if (!std::filesystem::exists(filepath.Data())) {
        PULSARTOOLS_ERRORF("{} check: '{}' does not exist.", executable, filepath);
        return false;
    }

    Pulsar::ParseSettings parserSettings;
    parserSettings.StoreDebugSymbols              = (flagOptions & P_DEBUG_SYMBOLS) != 0;
    parserSettings.AppendStackTraceToErrorMessage = (flagOptions & P_STACK_TRACE) != 0;
    parserSettings.AppendNotesToErrorMessage      = (flagOptions & P_ERROR_NOTES) != 0;
    parserSettings.AllowIncludeDirective          = (flagOptions & P_ALLOW_INCLUDE) != 0;

    Pulsar::Module module;
    // Add debug bindings
    PulsarTools::DebugNativeBindings::BindToModule(module, true);

    { // Parse Module
        PULSARTOOLS_INFOF("Parsing '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        Pulsar::Parser parser;
        auto parseResult = parser.AddSourceFile(filepath);
        if (parseResult == Pulsar::ParseResult::OK)
            parseResult = parser.ParseIntoModule(module, parserSettings);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Parsing took: {}us", parseTime.count());

        if (parseResult != Pulsar::ParseResult::OK) {
            PULSARTOOLS_ERRORF("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
            PulsarTools::PrintPrettyError(
                parser.GetErrorSource(), parser.GetErrorPath(),
                parser.GetErrorToken(), parser.GetErrorMessage());
            PULSARTOOLS_PRINTF("\n");
            return false;
        }
    }

    return true;
}

void PrintRunCommandUsage(const char* executable)
{
    PULSARTOOLS_INFOF("{} run [...PARSER_OPTIONS|RUNTIME_OPTIONS] <filepath> [...args]", executable);
    PULSARTOOLS_INFO("PARSER_OPTIONS:");
    PrintFlagOptions(ParserOptions_Ordered);
    PULSARTOOLS_INFO("RUNTIME_OPTIONS:");
    PULSARTOOLS_INFO("    -r/entry <func_name>");
    PULSARTOOLS_INFO("    --runtime/entry <func_name>");
    PULSARTOOLS_INFO("        Sets the entry point (default: 'main').");
    PULSARTOOLS_INFO("        The given function must accept only one argument which is a list of strings.");
    PULSARTOOLS_INFO("");
    PrintFlagOptions(RuntimeOptions_Ordered);
}

bool Command_Run(const char* executable, int argc, const char** argv)
{
    if (argc <= 0) {
        PrintRunCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} run: No file provided.", executable);
        return false;
    }

    Pulsar::String filepath;
    uint32_t flagOptions = P_DEFAULT | R_BIND_ALL;
    Pulsar::String entryPoint = "main";

    while (argc > 0) {
        filepath = ShiftArgs(argc, argv);
        if (filepath.Length() > 0 && filepath[0] != '-')
            break;

        const FlagOption* opt = nullptr;
        auto parseNameOptPair = ParserOptions.Find(filepath);
        auto runNameOptPair = RuntimeOptions.Find(filepath);
        
        if (parseNameOptPair) {
            opt = parseNameOptPair.Value;
        } else if (runNameOptPair) {
            opt = runNameOptPair.Value;
        } else if (filepath == "-r/entry" || filepath == "--runtime/entry") {
            if (argc <= 0) {
                PrintRunCommandUsage(executable);
                PULSARTOOLS_ERRORF("{} run: Option '{}' expects a value.", executable, filepath);
                return false;
            }
            entryPoint = ShiftArgs(argc, argv);
            continue;
        } else {
            PrintRunCommandUsage(executable);
            PULSARTOOLS_ERRORF("{} run: Invalid option '{}'.", executable, filepath);
            return false;
        }

        PULSARTOOLS_ASSERT(opt, "Invalid FlagOption* state.");
        if (opt->IsInverse)
            flagOptions &= ~opt->Flag;
        else flagOptions |= opt->Flag;
    }

    if (filepath.Length() > 0 && filepath[0] == '-') {
        PrintRunCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} run: No file provided.", executable);
        return false;
    } else if (!std::filesystem::exists(filepath.Data())) {
        PULSARTOOLS_ERRORF("{} run: '{}' does not exist.", executable, filepath);
        return false;
    }

    Pulsar::ParseSettings parserSettings;
    parserSettings.StoreDebugSymbols              = (flagOptions & P_DEBUG_SYMBOLS) != 0;
    parserSettings.AppendStackTraceToErrorMessage = (flagOptions & P_STACK_TRACE) != 0;
    parserSettings.AppendNotesToErrorMessage      = (flagOptions & P_ERROR_NOTES) != 0;
    parserSettings.AllowIncludeDirective          = (flagOptions & P_ALLOW_INCLUDE) != 0;

    Pulsar::Module module;
    // Debug bindings are declared before parsing so they can be used in compile-time evaluations
    if (flagOptions & R_BIND_DEBUG)
        PulsarTools::DebugNativeBindings::BindToModule(module, true);

    { // Parse Module
        PULSARTOOLS_INFOF("Parsing '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        Pulsar::Parser parser;
        auto parseResult = parser.AddSourceFile(filepath);
        if (parseResult == Pulsar::ParseResult::OK)
            parseResult = parser.ParseIntoModule(module, parserSettings);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Parsing took: {}us", parseTime.count());

        if (parseResult != Pulsar::ParseResult::OK) {
            PULSARTOOLS_ERRORF("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
            PulsarTools::PrintPrettyError(
                parser.GetErrorSource(), parser.GetErrorPath(),
                parser.GetErrorToken(), parser.GetErrorMessage());
            PULSARTOOLS_PRINTF("\n");
            return false;
        }
    }

    // Add bindings
    if (flagOptions & R_BIND_LEXER)
        PulsarTools::LexerNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_MODULE)
        PulsarTools::ModuleNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_PRINT)
        PulsarTools::PrintNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_THREAD)
        PulsarTools::ThreadNativeBindings::BindToModule(module);

    { // Run Module
        PULSARTOOLS_INFOF("Running '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        Pulsar::ValueStack stack;
        { // Push argv into the Stack.
            Pulsar::ValueList argList;
            argList.Append()->Value().SetString(filepath);
            for (int i = 0; i < argc; i++)
                argList.Append()->Value().SetString(argv[i]);
            stack.EmplaceBack().SetList(std::move(argList));
        }
        Pulsar::ExecutionContext context = module.CreateExecutionContext();
        auto runtimeState = module.CallFunctionByName(entryPoint, stack, context);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_PRINTF("\n"); // Add new line after script output
        PULSARTOOLS_INFOF("Execution took: {}us", execTime.count());

        if (runtimeState != Pulsar::RuntimeState::OK) {
            PULSARTOOLS_ERRORF("Runtime Error: {}", Pulsar::RuntimeStateToString(runtimeState));
            PulsarTools::PrintPrettyRuntimeError(context);
            PULSARTOOLS_PRINTF("\n");
            return false;
        }

        PULSARTOOLS_INFOF("Runtime State: {}", Pulsar::RuntimeStateToString(runtimeState));
        if (stack.Size() > 0) {
            PULSARTOOLS_INFOF("Stack after ({}) call:", entryPoint);
            for (size_t i = 0; i < stack.Size(); i++)
                PULSARTOOLS_INFOF("{}. {}", i+1, stack[i]);
        } else {
            PULSARTOOLS_INFOF("Stack after ({}) call: []", entryPoint);
        }
    }

    return true;
}

void PrintProgramUsage(const char* executable)
{
    PULSARTOOLS_INFOF("{} <COMMAND> ...", executable);
    PULSARTOOLS_INFO("COMMAND:");
    PULSARTOOLS_INFO("    run");
    PULSARTOOLS_INFO("        Runs the specified Pulsar file.");
    PULSARTOOLS_INFO("");
    PULSARTOOLS_INFO("    check");
    PULSARTOOLS_INFO("        Checks the specified Pulsar file for parse errors.");
    PULSARTOOLS_INFO("");
}

int main(int argc, const char** argv)
{
    PULSARTOOLS_ASSERT(argc >= 1, "pulsar-tools executable not in argv.");
    const char* executable = ShiftArgs(argc, argv);
    if (argc <= 0) {
        PrintProgramUsage(executable);
        PULSARTOOLS_ERRORF("{}: No command provided.", executable);
        return 1;
    }

    Pulsar::String command = ShiftArgs(argc, argv);
    if (command == "check") {
        if (!Command_Check(executable, argc, argv))
            return 1;
    } else if (command == "run") {
        if (!Command_Run(executable, argc, argv))
            return 1;
    } else {
        PrintProgramUsage(executable);
        PULSARTOOLS_ERRORF("{}: '{}' is not a valid command.", executable, command);
        return 1;
    }

    return 0;
}
