#ifndef _PULSARTOOLS_CLI_H
#define _PULSARTOOLS_CLI_H

#include "pulsar/parser.h"
#include "pulsar/runtime.h"

#include "pulsar-tools/argue.h"
#include "pulsar-tools/logger.h"
#include "pulsar-tools/utils.h"

namespace PulsarTools::CLI
{
    Logger& GetLogger();

    struct GeneralOptions
    {
    public:
        GeneralOptions(Argue::IArgParser& cmd) :
            LogLevel(cmd, "log-level", "L", "LOGLEVEL",
                "Sets the log level for the CLI. (default: all)",
                {"all", "error"}, 0)
        {}

        Argue::ChoiceOption LogLevel;
    };

    struct ParserOptions
    {
        ParserOptions(Argue::IArgParser& cmd) :
            IncludeFolders(cmd, "include", "I", "FOLDER",
                "Adds FOLDER to the include paths.\n"
                "#include directives will search these folders if no relative file can be found.",
                false),
            DeclareBoundNatives(cmd, "declare-natives", "N",
                "Automatically declare bound natives so that they can be used in global producers. (default: false)",
                false),
            Debug(cmd, "debug", "g",
                "Generate debug symbols for better runtime errors. (default: true)",
                true),
            StackTrace(cmd, "stack-trace", "",
                "Print stack trace on global evaluation error. (default: true)",
                true),
            StackTraceDepth(cmd, "stack-trace-depth", "", "DEPTH",
                "Sets the max depth of the printed stack-trace. (default: 10)",
                10),
            ErrorNotes(cmd, "error-notes", "",
                "Print error notes on parsing errors. (default: true)",
                true),
            AllowInclude(cmd, "allow-include", "",
                "Allow the usage of the #include directive. (default: true)",
                true),
            AllowLabels(cmd, "allow-labels", "L",
                "Allow the usage of labels. (default: false)",
                false)
        {}

        Argue::CollectionOption IncludeFolders;

        Argue::FlagOption DeclareBoundNatives;
        Argue::FlagOption Debug;
        Argue::FlagOption StackTrace;
        Argue::IntOption  StackTraceDepth;
        Argue::FlagOption ErrorNotes;
        Argue::FlagOption AllowInclude;
        Argue::FlagOption AllowLabels;
    };

    struct RuntimeOptions
    {
        RuntimeOptions(Argue::IArgParser& cmd) :
            EntryPoint(cmd, "entry-point", "e", "FUNC", "Set entry point. (default: main)", "main"),
            BindDebug(cmd, "bind-debug", "", "Debugging utilities."),
            BindError(cmd, "bind-error", "", "Error handling bindings."),
            BindFilesystem(cmd, "bind-filesystem", "", "Bind File System natives."),
            BindLexer(cmd, "bind-lexer", "", "Bindings to the Pulsar Lexer."),
            BindModule(cmd, "bind-module", "", "Bind Module natives."),
            BindPrint(cmd, "bind-print", "", "Printing functions."),
            BindStdio(cmd, "bind-stdio", "", "Direct access to stdio for String IO."),
            BindThread(cmd, "bind-thread", "", "Bind Thread natives."),
            BindTime(cmd, "bind-time", "", "Bindings to the system clock."),
            BindAll(cmd, "bind-all", "", "Bind all available natives. (default: true)", true,
                BindDebug, BindError, BindFilesystem, BindLexer, BindModule, BindPrint, BindStdio, BindThread, BindTime)
        {
            BindAll.SetValue(true);
        }

        Argue::StrOption EntryPoint;

        Argue::FlagOption BindDebug;
        Argue::FlagOption BindError;
        Argue::FlagOption BindFilesystem;
        Argue::FlagOption BindLexer;
        Argue::FlagOption BindModule;
        Argue::FlagOption BindPrint;
        Argue::FlagOption BindStdio;
        Argue::FlagOption BindThread;
        Argue::FlagOption BindTime;
        Argue::FlagGroupOption BindAll;
    };

    struct CompilerOptions
    {
        CompilerOptions(Argue::IArgParser& cmd) :
            OutputFile(cmd, "out", "o", "OUTPUT",
                "Tells the compiler where to save the compiled Neutron file. (default: <INFILENAME>.ntr)",
                "")
        {}

        Argue::StrOption OutputFile;
    };

    struct InputFileArgs
    {
        InputFileArgs(Argue::IArgParser& cmd) :
            FilePath(cmd, "The file to be ran.", "FILEPATH")
        {}

        Argue::StrArgument FilePath;
    };

    struct InputProgramArgs :
        public InputFileArgs
    {
        InputProgramArgs(Argue::IArgParser& cmd) :
            InputFileArgs(cmd),
            Args(cmd, "Arguments to be passed to the program.", "ARG")
        {}

        Argue::StrVarArgument Args;
    };

    namespace Action
    {
        int Check(const ParserOptions& parserOptions, const InputFileArgs& input);
        int Read(Pulsar::Module& module, const ParserOptions& parserOptions, const InputFileArgs& input);
        int Write(const Pulsar::Module& module, const CompilerOptions& compilerOptions, const InputFileArgs& input);
        int Parse(Pulsar::Module& module, const ParserOptions& parserOptions, const RuntimeOptions& runtimeOptions, const InputFileArgs& input);
        int Run(const Pulsar::Module& module, const RuntimeOptions& runtimeOptions, const InputProgramArgs& input);
    }

    class CheckCommand
    {
    public:
        CheckCommand(Argue::IArgParser& parser) :
            m_Command(parser, "check", "Checks the specified Pulsar file for syntax errors."),
            m_ParserOptions(m_Command),
            m_Input(m_Command)
        {}

        operator bool() const { return m_Command; }
        int operator()() const
        {
            return Action::Check(m_ParserOptions, m_Input);
        }

    private:
        Argue::CommandParser m_Command;
        ParserOptions m_ParserOptions;
        InputFileArgs m_Input;
    };

    class CompileCommand
    {
    public:
        CompileCommand(Argue::IArgParser& parser) :
            m_Command(parser, "compile", "Compiles a Pulsar file into a Neutron file."),
            m_ParserOptions(m_Command),
            m_RuntimeOptions(m_Command),
            m_CompilerOptions(m_Command),
            m_Input(m_Command)
        {}

        operator bool() const { return m_Command; }
        int operator()() const
        {
            Pulsar::Module module;
            int exitCode = IsNeutronFile(*m_Input.FilePath)
                ? Action::Read(module, m_ParserOptions, m_Input)
                : Action::Parse(module, m_ParserOptions, m_RuntimeOptions, m_Input);
            if (exitCode) return exitCode;
            return Action::Write(module, m_CompilerOptions, m_Input);
        }

    private:
        Argue::CommandParser m_Command;
        ParserOptions m_ParserOptions;
        RuntimeOptions m_RuntimeOptions;
        CompilerOptions m_CompilerOptions;
        InputFileArgs m_Input;
    };

    class RunCommand
    {
    public:
        RunCommand(Argue::IArgParser& parser) :
            m_Command(parser, "run", "Runs a Pulsar or Neutron file."),
            m_ParserOptions(m_Command),
            m_RuntimeOptions(m_Command),
            m_Input(m_Command)
        {}

        operator bool() const { return m_Command; }
        int operator()() const
        {
            Pulsar::Module module;
            int exitCode = IsNeutronFile(*m_Input.FilePath)
                ? Action::Read(module, m_ParserOptions, m_Input)
                : Action::Parse(module, m_ParserOptions, m_RuntimeOptions, m_Input);
            if (exitCode) return exitCode;
            return Action::Run(module, m_RuntimeOptions, m_Input);
        }

    private:
        Argue::CommandParser m_Command;
        ParserOptions m_ParserOptions;
        RuntimeOptions m_RuntimeOptions;
        InputProgramArgs m_Input;
    };

    struct Program
    {
        Program(const char* path) :
            Parser(path, "*(*PulsarTools file) -> 1."),
            Options(Parser),
            CmdHelp(Parser),
            CmdRun(Parser),
            CmdCheck(Parser),
            CmdCompile(Parser)
        {}
        
        Argue::ArgParser Parser;
        GeneralOptions Options;
        Argue::HelpCommand CmdHelp;
        PulsarTools::CLI::RunCommand CmdRun;
        PulsarTools::CLI::CheckCommand CmdCheck;
        PulsarTools::CLI::CompileCommand CmdCompile;
    };
}

#endif // _PULSARTOOLS_CLI_H
