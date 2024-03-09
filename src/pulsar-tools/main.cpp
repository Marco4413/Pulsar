#include "pulsar-tools/core.h"
#include "pulsar-tools/print.h"

#include "pulsar-tools/bindings/debug.h"
#include "pulsar-tools/bindings/lexer.h"
#include "pulsar-tools/bindings/module.h"
#include "pulsar-tools/bindings/print.h"

#include "pulsar/runtime.h"
#include "pulsar/parser.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

inline const char* ShiftArgs(int& argc, const char**& argv)
{
    if (argc <= 0)
        return nullptr;
    --argc;
    return *argv++;
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

constexpr uint32_t R_BIND_DEBUG  = 1 << 16;
constexpr uint32_t R_BIND_LEXER  = 2 << 16;
constexpr uint32_t R_BIND_MODULE = 4 << 16;
constexpr uint32_t R_BIND_PRINT  = 8 << 16;
constexpr uint32_t R_BIND_ALL    =
      R_BIND_DEBUG
    | R_BIND_LEXER
    | R_BIND_MODULE
    | R_BIND_PRINT;

#define PULSARTOOLS_FLAG_OPTIONS(optName, ...)                                                  \
    const std::vector<std::pair<Pulsar::String, FlagOption>> optName##_Ordered { __VA_ARGS__ }; \
    const std::unordered_map<Pulsar::String, FlagOption> optName { __VA_ARGS__ };

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
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-all",    "b-all",    R_BIND_ALL,    "Bind all available natives."),
)

void PrintFlagOptions(const std::vector<std::pair<Pulsar::String, FlagOption>>& opts)
{
    for (const auto& kv : opts) {
        PULSARTOOLS_INFOF("    {}", kv.first);
        if (kv.second.Description) {
            PULSARTOOLS_INFOF("        {}", kv.second.Description);
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
        const auto& parseOptIt = ParserOptions.find(filepath);
        
        if (parseOptIt != ParserOptions.end()) {
            opt = &parseOptIt->second;
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
    PulsarTools::DebugNativeBindings debugBindings;
    debugBindings.BindToModule(module, true);

    { // Parse Module
        PULSARTOOLS_INFOF("Parsing '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        Pulsar::Parser parser;
        parser.AddSourceFile(filepath);
        auto parseResult = parser.ParseIntoModule(module, parserSettings);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Parsing took: {}us", parseTime.count());

        if (parseResult != Pulsar::ParseResult::OK) {
            PULSARTOOLS_ERRORF("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
            PulsarTools::PrintPrettyError(
                parser.GetLastErrorSource(), parser.GetLastErrorPath(),
                parser.GetLastErrorToken(), parser.GetLastErrorMessage());
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
        const auto& parseOptIt = ParserOptions.find(filepath);
        const auto& runOptIt = RuntimeOptions.find(filepath);
        
        if (parseOptIt != ParserOptions.end()) {
            opt = &parseOptIt->second;
        } else if (runOptIt != RuntimeOptions.end()) {
            opt = &runOptIt->second;
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
    PulsarTools::DebugNativeBindings debugBindings;
    if (flagOptions & R_BIND_DEBUG)
        debugBindings.BindToModule(module, true);

    { // Parse Module
        PULSARTOOLS_INFOF("Parsing '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        Pulsar::Parser parser;
        parser.AddSourceFile(filepath);
        auto parseResult = parser.ParseIntoModule(module, parserSettings);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Parsing took: {}us", parseTime.count());

        if (parseResult != Pulsar::ParseResult::OK) {
            PULSARTOOLS_ERRORF("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
            PulsarTools::PrintPrettyError(
                parser.GetLastErrorSource(), parser.GetLastErrorPath(),
                parser.GetLastErrorToken(), parser.GetLastErrorMessage());
            PULSARTOOLS_PRINTF("\n");
            return false;
        }
    }

    // Add bindings
    PulsarTools::LexerNativeBindings lexerBindings;
    PulsarTools::ModuleNativeBindings moduleBindings;
    PulsarTools::PrintNativeBindings printBindings;
    if (flagOptions & R_BIND_LEXER)
        lexerBindings.BindToModule(module);
    if (flagOptions & R_BIND_MODULE)
        moduleBindings.BindToModule(module);
    if (flagOptions & R_BIND_PRINT)
        printBindings.BindToModule(module);

    { // Run Module
        PULSARTOOLS_INFOF("Running '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        Pulsar::ValueStack stack;
        { // Push argv into the Stack.
            Pulsar::ValueList argList;
            for (int i = 0; i < argc; i++)
                argList.Append()->Value().SetString(argv[i]);
            stack.EmplaceBack().SetList(std::move(argList));
        }
        Pulsar::ExecutionContext context = module.CreateExecutionContext();
        auto runtimeState = module.CallFunctionByName(entryPoint, stack, context);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Execution took: {}us", execTime.count());

        if (runtimeState != Pulsar::RuntimeState::OK) {
            PULSARTOOLS_ERRORF("Runtime Error: {}", Pulsar::RuntimeStateToString(runtimeState));
            PulsarTools::PrintPrettyRuntimeError(context);
            PULSARTOOLS_PRINTF("\n");
            return false;
        }

        PULSARTOOLS_INFOF("Runtime State: {}", Pulsar::RuntimeStateToString(runtimeState));
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
        PULSARTOOLS_ERRORF("{}: No command provided.", executable);
        PrintProgramUsage(executable);
        return 1;
    }

    std::string command = ShiftArgs(argc, argv);
    if (command == "check") {
        if (!Command_Check(executable, argc, argv))
            return 1;
    } else if (command == "run") {
        if (!Command_Run(executable, argc, argv))
            return 1;
    } else {
        PULSARTOOLS_ERRORF("{}: '{}' is not a valid command.", executable, command);
        PrintProgramUsage(executable);
        return 1;
    }

    return 0;
}
