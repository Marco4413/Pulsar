#ifndef _PULSARDEBUGGER_TYPES_H
#define _PULSARDEBUGGER_TYPES_H

#include <memory>
#include <optional>

#include <pulsar/parser.h>
#include <pulsar/runtime.h>

#include <pulsar/structures/ref.h>

#include <pulsar-bindings/bindingsregister.h>

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

    // This value is not DAP-compliant, it's just used
    //  internally to represent an invalid source reference.
    constexpr SourceReference INVALID_SOURCE_REFERENCE = -1;
    constexpr VariablesReference NULL_VARIABLES_REFERENCE = 0;

    // Thread Ids are based on the address of their ExecutionContext.
    // So make sure not to move it in memory to keep the ThreadId valid.
    ThreadId ComputeThreadId(const Pulsar::ExecutionContext& thread);

    template <typename T>
    using Ref = Pulsar::SharedRef<T>;

    class DebuggableModule
    {
    public:
        using CRef = PulsarDebugger::Ref<const DebuggableModule>;

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
        DebuggableModule() = default;
        ~DebuggableModule() = default;

        DebuggableModule(const DebuggableModule&) = default;
        DebuggableModule(DebuggableModule&&)      = default;

        DebuggableModule& operator=(const DebuggableModule&) = default;
        DebuggableModule& operator=(DebuggableModule&&)      = default;

        Pulsar::Module& Get();
        const Pulsar::Module& Get() const;

        Pulsar::ParserNotifications GetParserNotificationsListener();
        PulsarBindings::BindingsRegister& GetBindings();

        // Any function which returns a pointer, the returned pointer, if not nullptr, is valid while this instance exists.
        const DebuggableModule::LocalScopeInfo* GetLocalScopeInfo(SourceReference sourceReference, size_t line) const;
        bool IsLineReachable(SourceReference sourceReference, size_t line) const;

        const Pulsar::SourceDebugSymbol* GetSource(SourceReference sourceReference) const;
        const Pulsar::String* GetSourcePath(SourceReference sourceReference) const;
        const Pulsar::String* GetSourceContent(SourceReference sourceReference) const;

        SourceReference FindSourceReferenceForPath(Pulsar::StringView path) const;

        // TODO: This may be optimized by batching
        // void FilterReachableLines(SourceReference sourceReference, Pulsar::List<size_t>& lines) const;

    private:
        PulsarBindings::BindingsRegister m_Bindings;
        Pulsar::Module m_Module;
        Pulsar::List<FunctionInfo> m_FunctionInfos;
    };
}

#endif // _PULSARDEBUGGER_TYPES_H
