#ifndef _PULSAR_BINARY_FILEREADER_H
#define _PULSAR_BINARY_FILEREADER_H

#ifndef PULSAR_NO_FILESYSTEM
#  include <fstream>
#endif // PULSAR_NO_FILESYSTEM

#include "pulsar/core.h"

#include "pulsar/binary/reader.h"
#include "pulsar/structures/string.h"

namespace Pulsar::Binary
{
#ifndef PULSAR_NO_FILESYSTEM
    class FileReader : public IReader
    {
    public:
        FileReader(const String& path)
            : m_File(path.CString(), std::ios::binary) { }

        bool ReadData(uint64_t size, uint8_t* data) override
        {
            m_File.read((char*)data, (std::streamsize)size);
            return (bool)m_File;
        }
    
    private:
        std::ifstream m_File;
    };
#else // PULSAR_NO_FILESYSTEM
    class FileReader : public IReader
    {
    public:
        FileReader(const String& path) { PULSAR_UNUSED(path); }

        bool ReadData(uint64_t size, uint8_t* data) override
        {
            PULSAR_UNUSED(size, data);
            return false;
        }
    };
#endif // PULSAR_NO_FILESYSTEM
}

#endif // _PULSAR_BINARY_FILEREADER_H
