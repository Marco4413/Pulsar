#ifndef _PULSAR_BINARY_FILEWRITER_H
#define _PULSAR_BINARY_FILEWRITER_H

#ifndef PULSAR_NO_FILESYSTEM
#  include <fstream>
#endif // PULSAR_NO_FILESYSTEM

#include "pulsar/core.h"

#include "pulsar/binary/writer.h"
#include "pulsar/structures/string.h"

namespace Pulsar::Binary
{
#ifndef PULSAR_NO_FILESYSTEM
    class FileWriter : public IWriter
    {
    public:
        FileWriter(const String& path)
            : m_File(path.CString(), std::ios::binary) { }

        bool WriteData(uint64_t size, const uint8_t* data) override
        {
            m_File.write((const char*)data, (std::streamsize)size);
            return (bool)m_File;
        }
    
    private:
        std::ofstream m_File;
    };
#else // PULSAR_NO_FILESYSTEM
    class FileWriter : public IWriter
    {
    public:
        FileWriter(const String& path) { PULSAR_UNUSED(path); }

        bool WriteData(uint64_t size, const uint8_t* data) override
        {
            PULSAR_UNUSED(size, data);
            return false;
        }
    };
#endif // PULSAR_NO_FILESYSTEM
}

#endif // _PULSAR_BINARY_FILEWRITER_H
