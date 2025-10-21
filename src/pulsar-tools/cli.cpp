#include "pulsar-tools/cli.h"

#include <chrono>
#include <optional>

#include "pulsar/platform.h"

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
#include "pulsar/optimizer.h"

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

Pulsar::ParseSettings PulsarTools::CLI::ParserOptions::ToParseSettings() const
{
    Pulsar::ParseSettings settings = Pulsar::ParseSettings_Default;
    settings.StoreDebugSymbols         = *this->Debug;
    settings.AppendNotesToErrorMessage = *this->ErrorNotes;
    settings.AllowIncludeDirective     = *this->AllowInclude;
    settings.AllowLabels               = *this->AllowLabels;

    Pulsar::ParseSettings::IncludePaths includePaths;
    includePaths.Reserve((*this->IncludeFolders).size()+1);

    for (const auto& includeFolder : *this->IncludeFolders) {
        includePaths.EmplaceBack(includeFolder.c_str());
    }

    if (*this->InterpreterIncludeFolder) {
        const auto& interpreterIncludeFolder = PulsarTools::CLI::GetInterpreterIncludeFolder();
        if (!interpreterIncludeFolder.empty() && std::filesystem::exists(interpreterIncludeFolder)) {
            includePaths.EmplaceBack(interpreterIncludeFolder.generic_string().c_str());
        }
    }

    if (!includePaths.IsEmpty()) {
        settings.IncludeResolver = Pulsar::ParseSettings::CreateFileSystemIncludeResolver(
                std::move(includePaths), true);
    }

    settings.Warnings.DuplicateFunctionNames = *this->WarnDuplicateFunctionNames;

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

    const auto& warningMessages = parser.GetWarningMessages();
    for (size_t i = 0; i < warningMessages.Size(); ++i) {
        logger.Info(CreateParserMessageReport(
                parser, MessageReportKind_Warning, warningMessages[i],
                logger.GetColor()));
    }

    if (parseResult != Pulsar::ParseResult::OK) {
        logger.Error("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
        logger.Error(CreateParserMessageReport(
                parser, MessageReportKind_Error, parser.GetErrorMessage(),
                logger.GetColor()));
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

    const auto& warningMessages = parser.GetWarningMessages();
    for (size_t i = 0; i < warningMessages.Size(); ++i) {
        logger.Warn(CreateParserMessageReport(
                parser, MessageReportKind_Warning, warningMessages[i],
                logger.GetColor()));
    }

    if (parseResult != Pulsar::ParseResult::OK) {
        logger.Error("Parse Error: {}", Pulsar::ParseResultToString(parseResult));
        logger.Error(CreateParserMessageReport(
                parser, MessageReportKind_Error, parser.GetErrorMessage(),
                logger.GetColor()));
        return 1;
    }

    if (!*parserOptions.DeclareBoundNatives) {
        BindNatives(module, runtimeOptions, false);
    }

    return 0;
}

int PulsarTools::CLI::Action::Optimize(Pulsar::Module& module, const OptimizerOptions& optimizerOptions, const Argue::StrOption* entryPoint)
{
    if (!optimizerOptions.HasOptimizationsActive()) return 0;

    Logger& logger = GetLogger();

    std::chrono::microseconds totalOptimizeTime(0);

    Pulsar::BaseOptimizerSettings optimizerSettings;
    {
        bool exportAllFunctions = false;
        Pulsar::List<Pulsar::StringView> exportedFunctions;

        if (entryPoint && *entryPoint) {
            exportedFunctions.PushBack((**entryPoint).c_str());
        }

        if (optimizerOptions.Exports) {
            const auto& exports = *optimizerOptions.Exports;
            exportAllFunctions = exports.ExportAllFunctions;
            if (!exportAllFunctions) {
                for (size_t i = 0; i < exports.ExportedFunctions.Size(); ++i)
                    exportedFunctions.PushBack(exports.ExportedFunctions[i]);
            }

            optimizerSettings.IsExportedNative = exports.ExportAllNatives
                ? [](size_t, const auto&) { return true; }
                : Pulsar::UnusedOptimizer::Settings::CreateReachableNativesFilter(module, exports.ExportedNatives);

            optimizerSettings.IsExportedGlobal = exports.ExportAllGlobals
                ? [](size_t, const auto&) { return true; }
                : Pulsar::UnusedOptimizer::Settings::CreateReachableGlobalsFilter(module, exports.ExportedGlobals);
        }

        optimizerSettings.IsExportedFunction = exportAllFunctions
            ? [](size_t, const auto&) { return true; }
            : Pulsar::UnusedOptimizer::Settings::CreateReachableFunctionsFilter(module, exportedFunctions);
    }

    if (*optimizerOptions.OptimizeUnused) {
        size_t initFunctions = module.Functions.Size()
            ,  initNatives   = module.NativeBindings.Size()
            ,  initGlobals   = module.Globals.Size()
            ,  initConstants = module.Constants.Size();

        auto startTime = std::chrono::steady_clock::now();

        Pulsar::UnusedOptimizer optimizer;
        optimizer.Optimize(module, optimizerSettings);

        auto endTime = std::chrono::steady_clock::now();
        auto optimizeUnusedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
        totalOptimizeTime += optimizeUnusedTime;

        logger.Info("Optimize Unused:");
        logger.Info("- Removed {}/{} functions.", initFunctions-module.Functions.Size(),    initFunctions);
        logger.Info("- Removed {}/{} natives.",   initNatives-module.NativeBindings.Size(), initNatives);
        logger.Info("- Removed {}/{} globals.",   initGlobals-module.Globals.Size(),        initGlobals);
        logger.Info("- Removed {}/{} constants.", initConstants-module.Constants.Size(),    initConstants);
        logger.Info("- Time: {}us", optimizeUnusedTime.count());
    }

    logger.Info("Optimizations took: {}us", totalOptimizeTime.count());
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
        logger.Error(CreateRuntimeErrorMessageReport(context, stackTraceDepth, logger.GetColor()));
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

void PulsarTools::CLI::ExportOption::WriteHint(Argue::ITextBuilder& hint) const
{
    const char* ARG_FORMAT = "f:<FUNC>|n:<NATIVE>|g:<GLOBAL>";

    const auto& parser = GetParser();
    if (parser.HasShortPrefix() && HasShortName()) {
        hint.PutText(Argue::s(
            parser.GetPrefix(), GetName(), '=', ARG_FORMAT, ", ",
            parser.GetShortPrefix(), GetShortName(), ARG_FORMAT
        ));
    } else {
        hint.PutText(Argue::s(
            parser.GetPrefix(), GetName(), '=', ARG_FORMAT
        ));
    }
}

bool PulsarTools::CLI::ExportOption::ParseValue(std::string_view val)
{
    size_t idSeparatorIdx = val.find_first_of(':');
    if (idSeparatorIdx == std::string_view::npos)
        return SetError("Could not find ':' to separate export type from identifier.");

    bool* exportAllRef = nullptr;
    Pulsar::List<Pulsar::String>* exportedRef = nullptr;

    std::string_view exportType = val.substr(0, idSeparatorIdx);
    if (exportType == "f" || exportType == "function") {
        exportAllRef = &m_Exports.ExportAllFunctions;
        exportedRef  = &m_Exports.ExportedFunctions;
    } else if (exportType == "n" || exportType == "native") {
        exportAllRef = &m_Exports.ExportAllNatives;
        exportedRef  = &m_Exports.ExportedNatives;
    } else if (exportType == "g" || exportType == "global") {
        exportAllRef = &m_Exports.ExportAllGlobals;
        exportedRef  = &m_Exports.ExportedGlobals;
    } else {
        return SetError("Invalid export type, valid export types are 'f' (function), 'n' (native), or 'g' (global).");
    }

    std::string_view identifier = val.substr(idSeparatorIdx+1);
    if (identifier.empty()) {
        return SetError("Invalid identifier, identifier may not be empty. You may specify either a name or '*' to export everything.");
    }

    if (!*exportAllRef) {
        if (identifier == "*") {
            *exportAllRef = true;
            if (exportedRef->Size() > 0)
                exportedRef->Clear();
        } else {
            exportedRef->EmplaceBack(Pulsar::String(identifier.data(), identifier.length()));
        }
    }

    return true;
}
