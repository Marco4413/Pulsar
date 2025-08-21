#include "pulsar-debugger/types.h"

#include <filesystem>

namespace PulsarDebugger
{

ThreadId ComputeThreadId(const Pulsar::ExecutionContext& thread)
{
    return static_cast<ThreadId>(reinterpret_cast<size_t>(&thread)) % MAX_SAFE_INTEGER;
}

Pulsar::ParserNotifications DebuggableModule::GetParserNotificationsListener()
{
    Pulsar::ParserNotifications notifications;
    notifications.OnBlockNotification = [this](Pulsar::ParserNotifications::OnBlockNotificationParams&& params) -> bool
    {
        const auto* thisFunctionIdx = params.LocalScope.Global.Functions.Find(params.FnDefinition.Name);
        if (!thisFunctionIdx) return false;
        if (thisFunctionIdx->Value() >= this->m_FunctionInfos.Size())
            this->m_FunctionInfos.Resize(thisFunctionIdx->Value()+1);

        auto& thisFunctionInfo = this->m_FunctionInfos[thisFunctionIdx->Value()];
        if (!thisFunctionInfo.HasData) {
            thisFunctionInfo.HasData     = true;
            thisFunctionInfo.Index       = thisFunctionIdx->Value();
            thisFunctionInfo.Name        = params.FnDefinition.Name;
            thisFunctionInfo.DebugSymbol = params.FnDefinition.DebugSymbol;
            thisFunctionInfo.StartPos    = params.Position;
            thisFunctionInfo.EndPos      = params.Position;
        } else if (params.Position.Line > thisFunctionInfo.EndPos.Line ||
                   (   params.Position.Line == thisFunctionInfo.EndPos.Line
                    && params.Position.Char >= thisFunctionInfo.EndPos.Char)) {
            thisFunctionInfo.EndPos = params.Position;
        }

        switch (params.Type) {
        case Pulsar::ParserNotifications::BlockNotificationType::BlockStart: {
            auto& newScope = thisFunctionInfo.OpenLocalScopes.EmplaceBack();
            newScope.StartPos = params.Position;
            newScope.EndPos   = params.Position;
            newScope.Locals   = params.LocalScope.Locals;
        } break;
        case Pulsar::ParserNotifications::BlockNotificationType::BlockEnd: {
            if (thisFunctionInfo.OpenLocalScopes.IsEmpty()) break;
            auto& localScope  = thisFunctionInfo.OpenLocalScopes.Back();
            localScope.EndPos = params.Position;
            thisFunctionInfo.LocalScopes.EmplaceBack(std::move(localScope));
            thisFunctionInfo.OpenLocalScopes.PopBack();
        } break;
        case Pulsar::ParserNotifications::BlockNotificationType::LocalScopeChanged: {
            if (!thisFunctionInfo.LocalScopes.IsEmpty()) {
                auto& localScope  = thisFunctionInfo.OpenLocalScopes.Back();
                localScope.EndPos = params.Position;
                thisFunctionInfo.LocalScopes.EmplaceBack(std::move(localScope));
                thisFunctionInfo.OpenLocalScopes.PopBack();
            }
            auto& newScope = thisFunctionInfo.OpenLocalScopes.EmplaceBack();
            newScope.StartPos = params.Position;
            newScope.EndPos   = params.Position;
            newScope.Locals   = params.LocalScope.Locals;
        }
        }

        return false;
    };

    return notifications;
}

Pulsar::Module& DebuggableModule::GetModule()
{
    return m_Module;
}

const Pulsar::Module& DebuggableModule::GetModule() const
{
    return m_Module;
}

const DebuggableModule::LocalScopeInfo* DebuggableModule::GetLocalScopeInfo(SourceReference sourceReference, size_t line) const
{
    for (size_t i = 0; i < m_FunctionInfos.Size(); ++i) {
        const auto& functionInfo = m_FunctionInfos[i];
        if (functionInfo.DebugSymbol.SourceIdx == static_cast<size_t>(sourceReference)
            && line >= functionInfo.StartPos.Line
            && line <= functionInfo.EndPos.Line
        ) {
            for (size_t j = 0; j < functionInfo.LocalScopes.Size(); ++j) {
                const auto& localScope = functionInfo.LocalScopes[j];
                if (   line >= localScope.StartPos.Line
                    && line <= localScope.EndPos.Line
                ) return &localScope;
            }

            break; // fail, didn't find a local scope
        }
    }
    return nullptr;
}

bool DebuggableModule::IsLineReachable(SourceReference sourceReference, size_t line) const
{
    for (size_t i = 0; i < m_FunctionInfos.Size(); ++i) {
        const auto& functionInfo = m_FunctionInfos[i];
        if (functionInfo.DebugSymbol.SourceIdx != static_cast<size_t>(sourceReference))
            continue;
        if (line < functionInfo.StartPos.Line || line > functionInfo.EndPos.Line)
            continue;

        const auto& codeDebugSymbols = m_Module.Functions[functionInfo.Index].CodeDebugSymbols;
        for (size_t j = 0; j < codeDebugSymbols.Size(); ++j) {
            if (line == codeDebugSymbols[j].Token.SourcePos.Line)
                return true;
        }
    }

    return false;
}

const Pulsar::SourceDebugSymbol* DebuggableModule::GetSource(SourceReference sourceReference) const
{
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Module.SourceDebugSymbols.Size())
        return nullptr;
    return &m_Module.SourceDebugSymbols[sourceReference];
}

const Pulsar::String* DebuggableModule::GetSourcePath(SourceReference sourceReference) const
{
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Module.SourceDebugSymbols.Size())
        return nullptr;
    return &m_Module.SourceDebugSymbols[sourceReference].Path;
}

const Pulsar::String* DebuggableModule::GetSourceContent(SourceReference sourceReference) const
{
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Module.SourceDebugSymbols.Size())
        return nullptr;
    return &m_Module.SourceDebugSymbols[sourceReference].Source;
}

SourceReference DebuggableModule::FindSourceReferenceForPath(const char* path) const
{
    std::filesystem::path pathToSearch(path);
    for (size_t uSourceReference = 0; uSourceReference < m_Module.SourceDebugSymbols.Size(); ++uSourceReference) {
        std::filesystem::path candidatePath(m_Module.SourceDebugSymbols[uSourceReference].Path.CString());

        std::error_code ec;
        if (std::filesystem::equivalent(pathToSearch, candidatePath, ec) && !ec)
            return static_cast<SourceReference>(uSourceReference);
    }
    return INVALID_SOURCE_REFERENCE;
}

}
