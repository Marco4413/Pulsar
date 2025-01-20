#include "pulsar-tools/cli.h"

#include <chrono>
#include <filesystem>

#include "pulsar/bytecode.h"
#include "pulsar/binary/filereader.h"
#include "pulsar/binary/filewriter.h"

#include "pulsar-tools/views.h"

static PulsarTools::Logger g_Logger(stdout, stderr);

PulsarTools::Logger& PulsarTools::CLI::GetLogger()
{
    return g_Logger;
}

Pulsar::ParseSettings ToParseSettings(const PulsarTools::CLI::ParserOptions& parserOptions)
{
    // TODO: Add Include folders
    Pulsar::ParseSettings settings = Pulsar::ParseSettings_Default;
    settings.StoreDebugSymbols         = *parserOptions.Debug;
    settings.AppendNotesToErrorMessage = *parserOptions.ErrorNotes;
    settings.AllowIncludeDirective     = *parserOptions.AllowInclude;
    settings.AllowLabels               = *parserOptions.AllowLabels;
    return settings;
}

int PulsarTools::CLI::Action::Check(const ParserOptions& parserOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    Pulsar::ParseSettings parserSettings = ToParseSettings(parserOptions);
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
        logger.Error(CreateParserErrorMessage(parser));
        return 1;
    }

    return 0;
}

int PulsarTools::CLI::Action::Read(Pulsar::Module& module, const ParserOptions& parserOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    Pulsar::Binary::ReadSettings readerSettings = Pulsar::Binary::ReadSettings_Default;
    readerSettings.LoadDebugSymbols = *parserOptions.Debug;

    std::string inputPath = *input.FilePath;
    logger.Info("Reading '{}'.", inputPath);
    auto startTime = std::chrono::steady_clock::now();

    Pulsar::Binary::FileReader fileReader(inputPath.c_str());
    auto readResult = Pulsar::Binary::ReadByteCode(fileReader, module);
    if (readResult != Pulsar::Binary::ReadResult::OK) {
        logger.Error("Read Error: {}", Pulsar::Binary::ReadResultToString(readResult));
        return 1;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto readTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime-startTime);
    logger.Info("Reading took: {}us", readTime.count());

    return 0;
}

int PulsarTools::CLI::Action::Write(const Pulsar::Module& module, const CompilerOptions& compilerOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    std::string outputPath = *compilerOptions.OutputFile;
    if (outputPath.empty()) {
        std::filesystem::path path(*input.FilePath);
        path.replace_extension(".ntr");
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

    // TODO: Bind natives

    Pulsar::ParseSettings parserSettings = ToParseSettings(parserOptions);
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
        logger.Error(CreateParserErrorMessage(parser));
        return 1;
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
        logger.Error(CreateRuntimeErrorMessage(context, stackTraceDepth));
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
