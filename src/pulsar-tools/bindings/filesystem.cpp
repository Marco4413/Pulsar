#include "pulsar-tools/bindings/filesystem.h"

#include <filesystem>
#include <fstream>

void PulsarTools::FileSystemNativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "fs/exists?", 1, 2 }, FS_Exists);
    module.BindNativeFunction({ "fs/read-all", 1, 1 }, FS_ReadAll);
}

Pulsar::RuntimeState PulsarTools::FileSystemNativeBindings::FS_Exists(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& filePath = frame.Locals[0];
    if (filePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    bool fileExists = std::filesystem::exists(filePath.AsString().Data());
    frame.Stack.EmplaceBack(std::move(filePath));
    frame.Stack.EmplaceBack().SetInteger(fileExists ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::FileSystemNativeBindings::FS_ReadAll(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& filePath = frame.Locals[0];
    if (filePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    
    std::filesystem::path fsPath(filePath.AsString().Data());
    if (!std::filesystem::is_regular_file(fsPath))
        return Pulsar::RuntimeState::Error;

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    Pulsar::String contents;
    contents.Resize(fileSize);
    if (!file.read((char*)contents.Data(), fileSize))
        contents.Resize(0);

    frame.Stack.EmplaceBack()
        .SetString(std::move(contents));
    return Pulsar::RuntimeState::OK;
}
