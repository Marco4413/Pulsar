#include "pulsar-tools/bindings/filesystem.h"

#include <cinttypes>
#include <filesystem>
#include <fstream>

PulsarTools::Bindings::FileSystem::FileSystem() :
    IBinding()
{
    BindNativeFunction({ "fs/exists?", 1, 2 }, FExists);
    BindNativeFunction({ "fs/read-all", 1, 1 }, FReadAll);
}

Pulsar::RuntimeState PulsarTools::Bindings::FileSystem::FExists(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& filePath = frame.Locals[0];
    if (filePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    bool fileExists = std::filesystem::exists(filePath.AsString().CString());
    frame.Stack.EmplaceBack(std::move(filePath));
    frame.Stack.EmplaceBack().SetInteger(fileExists ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::FileSystem::FReadAll(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& filePath = frame.Locals[0];
    if (filePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    
    std::filesystem::path fsPath(filePath.AsString().CString());
    if (!std::filesystem::is_regular_file(fsPath))
        return Pulsar::RuntimeState::Error;

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    Pulsar::String contents;
    contents.Resize(fileSize);
    if (!file.read(contents.Data(), fileSize))
        contents.Resize(0);

    frame.Stack.EmplaceBack()
        .SetString(std::move(contents));
    return Pulsar::RuntimeState::OK;
}
