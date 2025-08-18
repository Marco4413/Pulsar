#ifndef _PULSARDEBUGGER_TYPES_H
#define _PULSARDEBUGGER_TYPES_H

#include <memory>
#include <optional>

#include <pulsar/parser.h>
#include <pulsar/runtime.h>

namespace PulsarDebugger
{
    constexpr int64_t MAX_SAFE_INTEGER = 0x1FFFFFFFFFFFFF;

    using Id                = int64_t;
    using ThreadId          = Id;
    using FrameId           = Id;
    using ScopeId           = Id;

    using Reference          = int64_t;
    using VariablesReference = Reference;
    using SourceReference    = Reference;

    constexpr Reference NULL_REFERENCE = 0;

    // Thread Ids are based on the address of their ExecutionContext.
    // So make sure not to move it in memory to keep the ThreadId valid.
    ThreadId ComputeThreadId(const Pulsar::ExecutionContext& thread);

    using SharedDebuggableModule = std::shared_ptr<const class DebuggableModule>;

    class DebuggableModule
    {
    public:
        struct LocalScopeInfo
        {
            using Local = Pulsar::LocalScope::LocalVar;

            Pulsar::SourcePosition StartPos;
            Pulsar::SourcePosition EndPos;
            Pulsar::List<Local> Locals;
        };

        struct FunctionInfo
        {
            bool HasData = false;
            size_t Index;
            Pulsar::String Name;
            Pulsar::FunctionDebugSymbol DebugSymbol;
            Pulsar::SourcePosition StartPos;
            Pulsar::SourcePosition EndPos;
            Pulsar::List<LocalScopeInfo> LocalScopes;
            Pulsar::List<LocalScopeInfo> OpenLocalScopes;
        };

    public:
        DebuggableModule();
        ~DebuggableModule() = default;

        DebuggableModule(const DebuggableModule&) = default;
        DebuggableModule(DebuggableModule&&)      = default;

        DebuggableModule& operator=(const DebuggableModule&) = default;
        DebuggableModule& operator=(DebuggableModule&&)      = default;

        Pulsar::ParserNotifications GetParserNotificationsListener();

        Pulsar::Module& GetModule();
        const Pulsar::Module& GetModule() const;

        std::optional<DebuggableModule::LocalScopeInfo> GetLocalScopeInfo(SourceReference sourceReference, size_t line) const;
        bool IsLineReachable(SourceReference sourceReference, size_t line) const;

        std::optional<Pulsar::SourceDebugSymbol> GetSource(SourceReference sourceReference) const;
        std::optional<Pulsar::String> GetSourcePath(SourceReference sourceReference) const;
        std::optional<Pulsar::String> GetSourceContent(SourceReference sourceReference) const;

        // TODO: This may be optimized by batching
        // void FilterReachableLines(SourceReference sourceReference, Pulsar::List<size_t>& lines) const;

    private:
        Pulsar::Module m_Module;
        Pulsar::List<FunctionInfo> m_FunctionInfos;
    };
}

#endif // _PULSARDEBUGGER_TYPES_H
