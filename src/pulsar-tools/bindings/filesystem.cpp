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
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    if (frame.Locals[0].Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    bool exists = std::filesystem::exists(frame.Locals[0].AsString().Data());
    frame.Stack.EmplaceBack(std::move(frame.Locals[0]));
    frame.Stack.EmplaceBack()
        .SetInteger(exists ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::FileSystemNativeBindings::FS_ReadAll(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    if (frame.Locals[0].Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    
    std::filesystem::path filePath(frame.Locals[0].AsString().Data());
    if (!std::filesystem::is_regular_file(filePath))
        return Pulsar::RuntimeState::Error;
    std::ifstream file(filePath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(filePath);

    Pulsar::String contents;
    contents.Resize(fileSize);
    if (!file.read((char*)contents.Data(), fileSize))
        contents.Resize(0);

    frame.Stack.EmplaceBack()
        .SetString(std::move(contents));
    return Pulsar::RuntimeState::OK;
}
