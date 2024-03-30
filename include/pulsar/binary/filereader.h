#ifndef PULSAR_NO_FILESYSTEM
#ifndef _PULSAR_BINARY_FILEREADER_H
#define _PULSAR_BINARY_FILEREADER_H

#include <fstream>

#include "pulsar/core.h"

#include "pulsar/binary/reader.h"
#include "pulsar/structures/string.h"

namespace Pulsar::Binary
{
    class FileReader : public IReader
    {
    public:
        FileReader(const String& path)
            : m_File(path.Data()) { }

        bool ReadData(uint64_t size, uint8_t* data) override
        {
            m_File.read((char*)data, (std::streamsize)size);
            return (bool)m_File;
        }
    
    private:
        std::ifstream m_File;
    };
}

#endif // _PULSAR_BINARY_FILEREADER_H
#endif // PULSAR_NO_FILESYSTEM
