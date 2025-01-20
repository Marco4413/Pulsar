#include "pulsar-tools/cli.h"

#include <chrono>

#include "pulsar-tools/views.h"

static PulsarTools::Logger g_Logger(stdout, stderr);

PulsarTools::Logger& PulsarTools::CLI::GetLogger()
{
    return g_Logger;
}

int PulsarTools::CLI::Action::Check(const ParserOptions& parserOptions, const InputFileArgs& input)
{
    ARGUE_UNUSED(parserOptions);
    ARGUE_UNUSED(input);
    return 1;
}

int PulsarTools::CLI::Action::Write(const Pulsar::Module& module, const CompilerOptions& compilerOptions)
{
    ARGUE_UNUSED(module);
    ARGUE_UNUSED(compilerOptions);
    return 1;
}

int PulsarTools::CLI::Action::Parse(Pulsar::Module& module, const ParserOptions& parserOptions, const RuntimeOptions& runtimeOptions, const InputFileArgs& input)
{
    Logger& logger = GetLogger();

    // TODO: Bind natives
    ARGUE_UNUSED(runtimeOptions);

    // TODO: Add Include folders
    Pulsar::ParseSettings parserSettings{
        .StoreDebugSymbols              = *parserOptions.Debug,
        .AppendStackTraceToErrorMessage = *parserOptions.StackTrace,
        .StackTraceMaxDepth             = static_cast<size_t>(*parserOptions.StackTraceDepth),
        .AppendNotesToErrorMessage      = *parserOptions.ErrorNotes,
        .AllowIncludeDirective          = *parserOptions.AllowInclude,
        .AllowLabels                    = *parserOptions.AllowLabels,
    };

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
        logger.Error(CreateRuntimeErrorMessage(context));
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
