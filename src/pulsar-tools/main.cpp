#include "pulsar-tools/core.h"
#include "pulsar-tools/print.h"

#include "pulsar-tools/bindings/debug.h"
#include "pulsar-tools/bindings/error.h"
#include "pulsar-tools/bindings/filesystem.h"
#include "pulsar-tools/bindings/lexer.h"
#include "pulsar-tools/bindings/module.h"
#include "pulsar-tools/bindings/print.h"
#include "pulsar-tools/bindings/stdio.h"
#include "pulsar-tools/bindings/thread.h"
#include "pulsar-tools/bindings/time.h"

#include "pulsar/bytecode.h"
#include "pulsar/binary/filereader.h"
#include "pulsar/binary/filewriter.h"

#include "pulsar/runtime.h"
#include "pulsar/parser.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/string.h"

#include <chrono>
#include <cstring>
#include <filesystem>

inline const char* ShiftArgs(int& argc, const char**& argv)
{
    PULSARTOOLS_ASSERT(argc > 0, "ShiftArgs called with an invalid argc.");
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

constexpr uint32_t P_DEBUG         = (1     );
constexpr uint32_t P_STACK_TRACE   = (1 << 1);
constexpr uint32_t P_ERROR_NOTES   = (1 << 2);
constexpr uint32_t P_ALLOW_INCLUDE = (1 << 3);
constexpr uint32_t P_ALLOW_LABELS  = (1 << 4);
constexpr uint32_t P_DEFAULT       =
      P_DEBUG
    | P_STACK_TRACE
    | P_ERROR_NOTES
    | P_ALLOW_INCLUDE;

constexpr uint32_t R_BIND_DEBUG  = (1     ) << 16;
constexpr uint32_t R_BIND_ERROR  = (1 << 1) << 16;
constexpr uint32_t R_BIND_FS     = (1 << 2) << 16;
constexpr uint32_t R_BIND_LEXER  = (1 << 3) << 16;
constexpr uint32_t R_BIND_MODULE = (1 << 4) << 16;
constexpr uint32_t R_BIND_PRINT  = (1 << 5) << 16;
constexpr uint32_t R_BIND_STDIO  = (1 << 6) << 16;
constexpr uint32_t R_BIND_THREAD = (1 << 7) << 16;
constexpr uint32_t R_BIND_TIME   = (1 << 8) << 16;
constexpr uint32_t R_BIND_ALL    =
      R_BIND_DEBUG
    | R_BIND_ERROR
    | R_BIND_FS
    | R_BIND_LEXER
    | R_BIND_MODULE
    | R_BIND_PRINT
    | R_BIND_STDIO
    | R_BIND_THREAD
    | R_BIND_TIME;

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
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "debug", P_DEBUG,
        "Store debug symbols for better runtime error information. (default: true)"
        "\nAutomatically binds and declares debug functions to be used during parse-time evaluation."),
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "stack-trace",   P_STACK_TRACE,   "Enable stack trace for compile-time evaluation errors. (default: true)"),
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "error-notes",   P_ERROR_NOTES,   "Enable notes about errors. (default: true)"),
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "allow-include", P_ALLOW_INCLUDE, "Allow the usage of the include directive. (default: true)"),
    PULSARTOOLS_FLAG_OPTION_LONG("--parser", "-p", "allow-labels",  P_ALLOW_LABELS,  "Allow the usage of labels within functions. (default: false)"),
)

PULSARTOOLS_FLAG_OPTIONS(RuntimeOptions,
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-debug",      "b-debug",  R_BIND_DEBUG,  "Debugging utilities."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-error",      "b-error",  R_BIND_ERROR,  "Error handling bindings."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-filesystem", "b-fs",     R_BIND_FS,     "Bind File System natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-lexer",      "b-lexer",  R_BIND_LEXER,  "Bindings to the Pulsar Lexer."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-module",     "b-module", R_BIND_MODULE, "Bind Module natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-print",      "b-print",  R_BIND_PRINT,  "Printing functions."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-stdio",      "b-stdio",  R_BIND_STDIO,  "Direct access to stdio for String IO."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-thread",     "b-thread", R_BIND_THREAD, "Bind Thread natives."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-time",       "b-time",   R_BIND_TIME,   "Bindings to the system clock."),
    PULSARTOOLS_FLAG_OPTION("--runtime", "-r", "bind-all",        "b-all",    R_BIND_ALL,    "Bind all available natives. (default: true)"),
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
                size_t idx = StrIndexOf(view.DataFromStart(), '\n');
                Pulsar::String line = view.GetPrefix(idx);
                PULSARTOOLS_INFOF("        {0}", line);
                view.RemovePrefix(idx);
                // Remove new line
                if (!view.Empty())
                    view.RemovePrefix(1);
            } while (!view.Empty());
            PULSARTOOLS_INFO("");
        }
    }
}

bool IsFile(const Pulsar::String& filepath)
{
    std::filesystem::path path(filepath.Data());
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool IsNeutronFile(const Pulsar::String& filepath)
{
    if (!IsFile(filepath))
        return false;
    std::ifstream file(filepath.Data(), std::ios::binary);
    char sig[Pulsar::Binary::SIGNATURE_LENGTH];
    if (!file.read(sig, (std::streamsize)Pulsar::Binary::SIGNATURE_LENGTH))
        return false;
    return std::memcmp((void*)sig, (void*)Pulsar::Binary::SIGNATURE, (size_t)Pulsar::Binary::SIGNATURE_LENGTH) == 0;
}

bool ParseOptions(
    const char* executable, const char* command,
    bool parserFlags, bool runtimeFlags, uint32_t& flagsOut,
    int& argc, const char**& argv, Pulsar::String& option,
    std::function<void(const char*)> usagePrinter,
    std::function<bool(const Pulsar::String&, int&, const char**&)> customParser)
{
    while (argc > 0) {
        option = ShiftArgs(argc, argv);
        if (option.Length() > 0 && option[0] != '-')
            break;

        const FlagOption* opt = nullptr;
        auto parseNameOptPair = ParserOptions.Find(option);
        auto runNameOptPair = RuntimeOptions.Find(option);
        
        if (parserFlags && parseNameOptPair) {
            opt = parseNameOptPair.Value;
        } else if (runtimeFlags && runNameOptPair) {
            opt = runNameOptPair.Value;
        } else if (customParser && customParser(option, argc, argv)) {
            continue;
        } else {
            if (usagePrinter)
                usagePrinter(executable);
            PULSARTOOLS_ERRORF("{} {}: Invalid option '{}'.", executable, command, option);
            return false;
        }

        PULSARTOOLS_ASSERT(opt, "Invalid FlagOption* state.");
        if (opt->IsInverse)
            flagsOut &= ~opt->Flag;
        else flagsOut |= opt->Flag;
    }
    return true;
}

void ParserSettingsFromFlagOptions(uint32_t flagOptions, Pulsar::ParseSettings& parserSettings)
{
    parserSettings.StoreDebugSymbols              = (flagOptions & P_DEBUG) != 0;
    parserSettings.AppendStackTraceToErrorMessage = (flagOptions & P_STACK_TRACE)   != 0;
    parserSettings.AppendNotesToErrorMessage      = (flagOptions & P_ERROR_NOTES)   != 0;
    parserSettings.AllowIncludeDirective          = (flagOptions & P_ALLOW_INCLUDE) != 0;
    parserSettings.AllowLabels                    = (flagOptions & P_ALLOW_LABELS)  != 0;
}

void BindNativesFromFlagOptions(uint32_t flagOptions, Pulsar::Module& module)
{
    // Add bindings
    if (flagOptions & R_BIND_DEBUG)
        PulsarTools::DebugNativeBindings::BindToModule(module, false);
    if (flagOptions & R_BIND_ERROR)
        PulsarTools::ErrorNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_FS)
        PulsarTools::FileSystemNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_LEXER)
        PulsarTools::LexerNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_MODULE)
        PulsarTools::ModuleNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_PRINT)
        PulsarTools::PrintNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_STDIO)
        PulsarTools::STDIONativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_THREAD)
        PulsarTools::ThreadNativeBindings::BindToModule(module);
    if (flagOptions & R_BIND_TIME)
        PulsarTools::TimeNativeBindings::BindToModule(module);
}

bool ParseModuleFromFile(const Pulsar::String& filepath, Pulsar::Module& out, uint32_t flagOptions)
{
    Pulsar::Module module;

    Pulsar::ParseSettings parserSettings;
    ParserSettingsFromFlagOptions(flagOptions, parserSettings);

    // Debug bindings are declared before parsing so they can be used in compile-time evaluations
    if (flagOptions & P_DEBUG)
        PulsarTools::DebugNativeBindings::BindToModule(module, true);

    PULSARTOOLS_INFOF("Parsing '{}'.", filepath);
    auto startTime = std::chrono::high_resolution_clock::now();
    Pulsar::Parser parser;
    auto parseResult = parser.AddSourceFile(filepath);
    if (parseResult == Pulsar::ParseResult::OK)
        parseResult = parser.ParseIntoModule(module, parserSettings);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    PULSARTOOLS_INFOF("Parsing took: {}us", parseTime.count());

    out = std::move(module);
    if (parseResult != Pulsar::ParseResult::OK) {
        PULSARTOOLS_ERRORF("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
        PulsarTools::PrintPrettyError(
            parser.GetErrorSource(), parser.GetErrorPath(),
            parser.GetErrorToken(), parser.GetErrorMessage());
        PULSARTOOLS_PRINTF("\n");
        return false;
    }

    return true;
}

bool RunModule(const Pulsar::String& filepath, const Pulsar::String& entryPoint, const Pulsar::Module& module, uint32_t flagOptions, int argc, const char** argv)
{
    (void)flagOptions;
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
    return true;
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
    uint32_t flagOptions = P_DEFAULT | R_BIND_DEBUG;

    if (!ParseOptions(
        executable, "check",
        true, false, flagOptions,
        argc, argv, filepath,
        PrintCheckCommandUsage, nullptr
    )) return false;

    if (filepath.Length() == 0 || filepath[0] == '-') {
        PrintCheckCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} check: No file provided.", executable);
        return false;
    } else if (!IsFile(filepath)) {
        PULSARTOOLS_ERRORF("{} check: '{}' does not exist.", executable, filepath);
        return false;
    }

    Pulsar::Module module;
    return ParseModuleFromFile(filepath, module, flagOptions);
}

void PrintCompileCommandUsage(const char* executable)
{
    PULSARTOOLS_INFOF("{} compile [...PARSER_OPTIONS|COMPILER_OPTIONS] <filepath>", executable);
    PULSARTOOLS_INFO("PARSER_OPTIONS:");
    PrintFlagOptions(ParserOptions_Ordered);
    PULSARTOOLS_INFO("COMPILER_OPTIONS:");
    PULSARTOOLS_INFO("    -c/out <path>");
    PULSARTOOLS_INFO("    --compiler/output <path>");
    PULSARTOOLS_INFO("        Tells the compiler where to save the compiled Neutron file.");
    PULSARTOOLS_INFO("        Defaults to '<filepath>.ntr'. (the extension is swapped)");
    PULSARTOOLS_INFO("");
}

bool Command_Compile(const char* executable, int argc, const char** argv)
{
    if (argc <= 0) {
        PrintCompileCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} compile: No file provided.", executable);
        return false;
    }

    Pulsar::String filepath;
    uint32_t flagOptions = P_DEFAULT | R_BIND_ALL;
    Pulsar::String outputPath = "";

    if (!ParseOptions(
        executable, "compile",
        true, false, flagOptions,
        argc, argv, filepath,
        PrintCompileCommandUsage,
        [executable, &outputPath](Pulsar::String opt, int& argc, const char**& argv) mutable {
            if (opt == "-c/out" || opt == "--compiler/output") {
                if (argc <= 0) {
                    PrintCompileCommandUsage(executable);
                    PULSARTOOLS_ERRORF("{} compile: Option '{}' expects a value.", executable, opt);
                    return false;
                }
                outputPath = ShiftArgs(argc, argv);
                return true;
            }
            return false;
        }
    )) return false;

    if (filepath.Length() == 0 || filepath[0] == '-') {
        PrintCompileCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} compile: No file provided.", executable);
        return false;
    } else if (!IsFile(filepath)) {
        PULSARTOOLS_ERRORF("{} compile: '{}' does not exist.", executable, filepath);
        return false;
    }

    if (outputPath.Length() == 0) {
        // Swap extension
        std::filesystem::path path(filepath.Data());
        path.replace_extension(".ntr");
        outputPath = path.generic_string().c_str();
    }

    Pulsar::Module module;
    if (!ParseModuleFromFile(filepath, module, flagOptions))
        return false;

    { // Write to File
        PULSARTOOLS_INFOF("Writing to '{}'.", outputPath);
        auto startTime = std::chrono::high_resolution_clock::now();

        Pulsar::Binary::FileWriter moduleFile(outputPath);
        if (!Pulsar::Binary::WriteByteCode(moduleFile, module)) {
            PULSARTOOLS_ERRORF("Could not write to '{}'.", outputPath);
            return false;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Writing took: {}us", execTime.count());
    }

    return true;
}

void PrintRunCommandUsage(const char* executable)
{
    PULSARTOOLS_INFOF("{} run [...PARSER_OPTIONS|RUNTIME_OPTIONS] <filepath> [...args]", executable);
    PULSARTOOLS_INFO("PARSER_OPTIONS:");
    PULSARTOOLS_INFO("    **These options are ignored when <filepath> is a Neutron file.");
    PULSARTOOLS_INFO("");
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

    if (!ParseOptions(
        executable, "run",
        true, true, flagOptions,
        argc, argv, filepath,
        PrintRunCommandUsage,
        [executable, &entryPoint](const Pulsar::String& opt, int& argc, const char**& argv) mutable {
            if (opt == "-r/entry" || opt == "--runtime/entry") {
                if (argc <= 0) {
                    PrintRunCommandUsage(executable);
                    PULSARTOOLS_ERRORF("{} run: Option '{}' expects a value.", executable, opt);
                    return false;
                }
                entryPoint = ShiftArgs(argc, argv);
                return true;
            }
            return false;
        }
    )) return false;

    if (filepath.Length() == 0 || filepath[0] == '-') {
        PrintRunCommandUsage(executable);
        PULSARTOOLS_ERRORF("{} run: No file provided.", executable);
        return false;
    } else if (!IsFile(filepath)) {
        PULSARTOOLS_ERRORF("{} run: '{}' does not exist.", executable, filepath);
        return false;
    }

    Pulsar::Module module;
    if (IsNeutronFile(filepath)) {
        // Read Module
        PULSARTOOLS_INFOF("Reading '{}'.", filepath);
        auto startTime = std::chrono::high_resolution_clock::now();
        
        Pulsar::Binary::FileReader fileReader(filepath);
        auto readResult = Pulsar::Binary::ReadByteCode(fileReader, module);
        if (readResult != Pulsar::Binary::ReadResult::OK) {
            PULSARTOOLS_ERRORF("Read Error: {}", Pulsar::Binary::ReadResultToString(readResult));
            return false;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto readTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        PULSARTOOLS_INFOF("Reading took: {}us", readTime.count());
    } else if (!ParseModuleFromFile(filepath, module, flagOptions))
        return false;

    // Add bindings
    BindNativesFromFlagOptions(flagOptions, module);
    return RunModule(filepath, entryPoint, module, flagOptions, argc, argv);
}

void PrintProgramUsage(const char* executable)
{
    PULSARTOOLS_INFOF("{} [COMMAND] ...", executable);
    PULSARTOOLS_INFO("COMMAND:");
    PULSARTOOLS_INFO("    check");
    PULSARTOOLS_INFO("        Checks the specified Pulsar file for parse errors.");
    PULSARTOOLS_INFO("");
    PULSARTOOLS_INFO("    compile");
    PULSARTOOLS_INFO("        Parses and outputs a Neutron file from a Pulsar source file.");
    PULSARTOOLS_INFO("");
    PULSARTOOLS_INFO("    run (default)");
    PULSARTOOLS_INFO("        If no other command is invoked, this is the default one.");
    PULSARTOOLS_INFO("        Runs the specified Pulsar or Neutron file.");
    PULSARTOOLS_INFO("        When running a Neutron file, Parser settings are ignored.");
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
    } else if (command == "compile") {
        if (!Command_Compile(executable, argc, argv))
            return 1;
    } else if (command == "run") {
        if (!Command_Run(executable, argc, argv))
            return 1;
    } else {
        // Fallback to the run command
        ++argc; // "command" is probably the filepath, go back 1 argument
        --argv;
        if (!Command_Run(executable, argc, argv))
            return 1;
        // PrintProgramUsage(executable);
        // PULSARTOOLS_ERRORF("{}: '{}' is not a valid command.", executable, command);
        // return 1;
    }

    return 0;
}
