#ifndef PULSAR_NO_FILESYSTEM
#ifndef _PULSAR_BINARY_FILEWRITER_H
#define _PULSAR_BINARY_FILEWRITER_H

#include <fstream>

#include "pulsar/core.h"

#include "pulsar/binary/writer.h"
#include "pulsar/structures/string.h"

namespace Pulsar::Binary
{
    class FileWriter : public IWriter
    {
    public:
        FileWriter(const String& path)
            : m_File(path.Data(), std::ios::binary) { }

        bool WriteData(uint64_t size, const uint8_t* data) override
        {
            m_File.write((const char*)data, (std::streamsize)size);
            return (bool)m_File;
        }
    
    private:
        std::ofstream m_File;
    };
}

#endif // _PULSAR_BINARY_FILEWRITER_H
#endif // PULSAR_NO_FILESYSTEM
