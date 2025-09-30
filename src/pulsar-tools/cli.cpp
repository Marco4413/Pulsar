#include "pulsar-tools/cli.h"

#include <chrono>
#include <optional>

#include <pulsar/platform.h>

#if defined(PULSAR_PLATFORM_MACOSX)
#  include <mach-o/dyld.h>
#  include <string>
#elif defined(PULSAR_PLATFORM_UNIX)
#  include <array>
#elif defined(PULSAR_PLATFORM_WINDOWS)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif // PULSAR_PLATFORM_*

#include "pulsar/bytecode.h"
#include "pulsar/binary/filereader.h"
#include "pulsar/binary/filewriter.h"

#include "pulsar-tools/bindings.h"
#include "pulsar-tools/views.h"

static PulsarTools::Logger g_Logger(stdout, stderr);

PulsarTools::Logger& PulsarTools::CLI::GetLogger()
{
    return g_Logger;
}

const std::filesystem::path& PulsarTools::CLI::GetThisProcessExecutable()
{
    static thread_local std::optional<std::filesystem::path> s_ThisProcessExecutable = std::nullopt;
    if (s_ThisProcessExecutable) return *s_ThisProcessExecutable;
#if defined(PULSAR_PLATFORM_MACOSX)
    // TODO: This is not tested since I don't have a MACOS device
    std::string pathBuffer;
    uint32_t pathLength = pathBuffer.size();

    // Get full pathLength
    _NSGetExecutablePath(pathBuffer.data(), &pathLength);
    pathBuffer.resize(pathLength);

    std::error_code ec;
    if (_NSGetExecutablePath(pathBuffer.data(), &pathLength) == 0) {
        if (std::filesystem::exists(pathBuffer)) {
            s_ThisProcessExecutable = std::filesystem::canonical(pathBuffer, ec);
        }
    }
    
    if (ec) s_ThisProcessExecutable = std::filesystem::path();
#elif defined(PULSAR_PLATFORM_UNIX)
    // TODO: Maybe add implementations specific to BSD and Solaris (I've read that BSD may not have the proc fs)
    // See: https://stackoverflow.com/a/1024937
    // Solaris may not work, see: https://serverfault.com/a/862189
    constexpr std::array<const char*, 4> SELF_PROC_LINKS{
        "/proc/self/exe",
        "/proc/curproc/exe",
        "/proc/curproc/file",
        "/proc/self/path/a.out"
    };

    std::error_code ec;
    for (auto selfProcLink : SELF_PROC_LINKS) {
        if (std::filesystem::exists(selfProcLink)) {
            s_ThisProcessExecutable = std::filesystem::canonical(selfProcLink, ec);
            break;
        }
    }

    if (ec) s_ThisProcessExecutable = std::filesystem::path();
#elif defined(PULSAR_PLATFORM_WINDOWS)
    constexpr size_t EXE_PATH_LENGTH = MAX_PATH+1;
    TCHAR exePath[EXE_PATH_LENGTH]{0};
    DWORD status = GetModuleFileName(NULL, exePath, EXE_PATH_LENGTH);
    if (status < EXE_PATH_LENGTH) {
        s_ThisProcessExecutable = std::filesystem::path(exePath);
    }
#else // PULSAR_PLATFORM_*
    // Unsupported platform
#endif // PULSAR_PLATFORM_*
    if (!s_ThisProcessExecutable)
        s_ThisProcessExecutable = std::filesystem::path();
    return *s_ThisProcessExecutable;
}

const std::filesystem::path& PulsarTools::CLI::GetInterpreterIncludeFolder()
{
    static thread_local std::optional<std::filesystem::path> s_InterpreterIncludeFolder = std::nullopt;
    if (s_InterpreterIncludeFolder) return *s_InterpreterIncludeFolder;

    s_InterpreterIncludeFolder = GetThisProcessExecutable();
    if (s_InterpreterIncludeFolder->empty()) return *s_InterpreterIncludeFolder;
    return s_InterpreterIncludeFolder->replace_filename("include");
}

// TODO: Move into Pulsar so that it can be used by other projects
using IncludePaths = std::vector<std::string>;

static Pulsar::ParseSettings::IncludeResolverFn CreateIncludeResolver(const IncludePaths& includePaths)
{
    return [includePaths](Pulsar::Parser& parser, Pulsar::String cwf, Pulsar::Token token) {
        std::filesystem::path targetPath(token.StringVal.CString());

        Pulsar::ParseResult result;
        { // Try relative path first
            std::filesystem::path workingPath(cwf.CString());
            std::filesystem::path filePath = workingPath.parent_path() / targetPath;
            result = parser.AddSourceFile(filePath.generic_string().c_str());
            if (result == Pulsar::ParseResult::OK) return result;
        }

        for (size_t i = includePaths.size(); i > 0; --i) {
            std::filesystem::path workingPath(includePaths[i-1]);
            std::filesystem::path filePath = workingPath / targetPath;
            if (std::filesystem::exists(filePath) &&
                std::filesystem::is_regular_file(filePath)
            ) {
                parser.ClearError();
                return parser.AddSourceFile(filePath.generic_string().c_str());
            }
        }

        return result;
    };
}

Pulsar::ParseSettings PulsarTools::CLI::ParserOptions::ToParseSettings() const
{
    Pulsar::ParseSettings settings = Pulsar::ParseSettings_Default;
    settings.StoreDebugSymbols         = *this->Debug;
    settings.AppendNotesToErrorMessage = *this->ErrorNotes;
    settings.AllowIncludeDirective     = *this->AllowInclude;
    settings.AllowLabels               = *this->AllowLabels;

    auto includeFolders = *this->IncludeFolders;
    if (*this->InterpreterIncludeFolder) {
        const auto& interpreterIncludeFolderPath = PulsarTools::CLI::GetInterpreterIncludeFolder();
        if (!interpreterIncludeFolderPath.empty() && std::filesystem::exists(interpreterIncludeFolderPath)) {
            includeFolders.push_back(interpreterIncludeFolderPath.generic_string());
        }
    }

    if (!includeFolders.empty()) {
        settings.IncludeResolver = CreateIncludeResolver(includeFolders);
    }
    return settings;
}

int PulsarTools::CLI::Action::Check(const ParserOptions& parserOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    Pulsar::ParseSettings parserSettings = parserOptions.ToParseSettings();
    parserSettings.MapGlobalProducersToVoid = true;

    logger.Info("Parsing '{}'.", *input.FilePath);
    Pulsar::String filepath = (*input.FilePath).c_str();
    Pulsar::Module module;

    auto startTime = std::chrono::steady_clock::now();

    Pulsar::Parser parser;
    auto parseResult = parser.AddSourceFile(filepath);
    if (parseResult == Pulsar::ParseResult::OK)
        parseResult = parser.ParseIntoModule(module, parserSettings);

    auto endTime = std::chrono::steady_clock::now();
    auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    logger.Info("Parsing took: {}us", parseTime.count());

    if (parseResult != Pulsar::ParseResult::OK) {
        logger.Error("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
        logger.Error(CreateParserErrorMessage(parser, logger.GetColor()));
        return 1;
    }

    return 0;
}

int PulsarTools::CLI::Action::Read(Pulsar::Module& module, const ParserOptions& parserOptions, const RuntimeOptions& runtimeOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    Pulsar::Binary::ReadSettings readerSettings = Pulsar::Binary::ReadSettings_Default;
    readerSettings.LoadDebugSymbols = *parserOptions.Debug;

    std::string inputPath = *input.FilePath;
    logger.Info("Reading '{}'.", inputPath);
    auto startTime = std::chrono::steady_clock::now();

    Pulsar::Binary::FileReader fileReader(inputPath.c_str());
    auto readResult = Pulsar::Binary::ReadByteCode(fileReader, module, readerSettings);
    if (readResult != Pulsar::Binary::ReadResult::OK) {
        logger.Error("Read Error: {}", Pulsar::Binary::ReadResultToString(readResult));
        return 1;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto readTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    logger.Info("Reading took: {}us", readTime.count());

    BindNatives(module, runtimeOptions, false);
    return 0;
}

int PulsarTools::CLI::Action::Write(const Pulsar::Module& module, const CompilerOptions& compilerOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    std::string outputPath = *compilerOptions.OutputFile;
    if (outputPath.empty()) {
        std::filesystem::path path(*input.FilePath);
        path.replace_extension(".ntx");
        outputPath = path.generic_string();
    }

    logger.Info("Writing to '{}'.", outputPath);
    auto startTime = std::chrono::steady_clock::now();

    Pulsar::Binary::FileWriter moduleFile(outputPath.c_str());
    if (!Pulsar::Binary::WriteByteCode(moduleFile, module)) {
        logger.Error("Could not write to '{}'.", outputPath);
        return 1;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto writeTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    logger.Info("Writing took: {}us", writeTime.count());

    return 0;
}

int PulsarTools::CLI::Action::Parse(Pulsar::Module& module, const ParserOptions& parserOptions, const RuntimeOptions& runtimeOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    if (*parserOptions.DeclareBoundNatives) {
        BindNatives(module, runtimeOptions, true);
    } else if (*parserOptions.Debug) {
        Bindings::Debug debug;
        debug.BindAll(module, true);
    }

    Pulsar::ParseSettings parserSettings = parserOptions.ToParseSettings();
    parserSettings.AppendStackTraceToErrorMessage = *runtimeOptions.StackTrace;
    parserSettings.StackTraceMaxDepth             = static_cast<size_t>(*runtimeOptions.StackTraceDepth);

    logger.Info("Parsing '{}'.", *input.FilePath);
    Pulsar::String filepath = (*input.FilePath).c_str();

    auto startTime = std::chrono::steady_clock::now();

    Pulsar::Parser parser;
    auto parseResult = parser.AddSourceFile(filepath);
    if (parseResult == Pulsar::ParseResult::OK)
        parseResult = parser.ParseIntoModule(module, parserSettings);

    auto endTime = std::chrono::steady_clock::now();
    auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    logger.Info("Parsing took: {}us", parseTime.count());

    if (parseResult != Pulsar::ParseResult::OK) {
        logger.Error("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
        logger.Error(CreateParserErrorMessage(parser, logger.GetColor()));
        return 1;
    }

    if (!*parserOptions.DeclareBoundNatives) {
        BindNatives(module, runtimeOptions, false);
    }

    return 0;
}

int PulsarTools::CLI::Action::Run(const Pulsar::Module& module, const RuntimeOptions& runtimeOptions, const InputProgramArgs& input)
{
    Logger& logger = GetLogger();

    size_t stackTraceDepth = static_cast<size_t>(
        (*runtimeOptions.StackTraceDepth) *
        (*runtimeOptions.StackTrace));

    logger.Info("Running '{}'.", *input.FilePath);

    auto startTime = std::chrono::steady_clock::now();

    Pulsar::ExecutionContext context(module);
    Pulsar::ValueStack& stack = context.GetStack();
    { // Push argv into the Stack.
        Pulsar::ValueList argList;
        argList.Append()->Value().SetString((*input.FilePath).c_str());
        for (const std::string& arg : *input.Args)
            argList.Append()->Value().SetString(arg.c_str());
        stack.EmplaceBack().SetList(std::move(argList));
    }

    auto functionCallState = context.CallFunction((*runtimeOptions.EntryPoint).c_str());
    auto runtimeState = context.Run();

    auto endTime = std::chrono::steady_clock::now();
    auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    logger.PutNewLine();
    logger.Info("Execution took: {}us", execTime.count());

    if (functionCallState == Pulsar::RuntimeState::FunctionNotFound) {
        logger.Error("Runtime Error: {}", Pulsar::RuntimeStateToString(functionCallState));
        logger.Error("Entry point function ({}) not found.", *runtimeOptions.EntryPoint);
        return 1;
    } else if (runtimeState != Pulsar::RuntimeState::OK) {
        logger.Error("Runtime Error: {}", Pulsar::RuntimeStateToString(runtimeState));
        logger.Error(CreateRuntimeErrorMessage(context, stackTraceDepth, logger.GetColor()));
        return 1;
    }

    logger.Info("Runtime State: {}", Pulsar::RuntimeStateToString(runtimeState));
    if (stack.Size() > 0) {
        logger.Info("Stack after ({}) call:", *runtimeOptions.EntryPoint);
        for (size_t i = 0; i < stack.Size(); i++)
            logger.Info("{}. {}", i+1, stack[i]);
    } else {
        logger.Info("Stack after ({}) call: []", *runtimeOptions.EntryPoint);
    }

    return 0;
}
