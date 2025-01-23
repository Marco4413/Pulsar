#include "pulsar-tools/utils.h"

#include <filesystem>
#include <fstream>

#include "pulsar/binary.h"

bool PulsarTools::IsFile(std::string_view filepath)
{
    std::filesystem::path path(filepath);
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool PulsarTools::IsNeutronFile(std::string_view filepath)
{
    if (!IsFile(filepath))
        return false;
    std::filesystem::path path(filepath);
    std::ifstream file(path, std::ios::binary);
    char sig[Pulsar::Binary::SIGNATURE_LENGTH];
    if (!file.read(sig, (std::streamsize)Pulsar::Binary::SIGNATURE_LENGTH))
        return false;
    return std::memcmp((void*)sig, (void*)Pulsar::Binary::SIGNATURE, (size_t)Pulsar::Binary::SIGNATURE_LENGTH) == 0;
}
