#ifndef _PULSAR_BYTECODE_H
#define _PULSAR_BYTECODE_H

#include "pulsar/core.h"

#include "pulsar/binary.h"
#include "pulsar/binary/reader.h"
#include "pulsar/binary/writer.h"
#include "pulsar/binary/bytereader.h"
#include "pulsar/binary/bytewriter.h"

#include "pulsar/lexer.h"
#include "pulsar/parser.h"
#include "pulsar/runtime.h"
#include "pulsar/runtime/debug.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/string.h"

namespace Pulsar::Binary
{
    enum class ReadResult
    {
        OK = 0,
        Error = 1,
        UnexpectedEOF,
        DataNotConsumed,
        InvalidSignature,
        UnsupportedVersion,
        UnsupportedChunkType,
        UnsupportedCustomDataType,
        UnsupportedValueType,
    };

    const char* ReadResultToString(ReadResult rr);

    struct ReadSettings
    {
        bool LoadDebugSymbols = true;
    };

    inline const ReadSettings ReadSettings_Default{};

    struct WriteSettings
    {
        bool StoreDebugSymbols = true;
    };

    inline const WriteSettings WriteSettings_Default{};

    ReadResult ReadByteCode(IReader& reader, Module& out, const ReadSettings& settings=ReadSettings_Default);
    bool WriteByteCode(IWriter& writer, const Module& module, const WriteSettings& settings=WriteSettings_Default);
    
    namespace ByteCode {
        // LinkedLists are stored like normal Lists
        ReadResult ReadLinkedList(IReader& reader, LinkedList<Value>& out, const ReadSettings& settings);
        ReadResult ReadList(IReader& reader, List<Value>& out, const ReadSettings& settings);
        ReadResult ReadList(IReader& reader, List<Instruction>& out, const ReadSettings& settings);
        ReadResult ReadList(IReader& reader, List<BlockDebugSymbol>& out, const ReadSettings& settings);
        ReadResult ReadList(IReader& reader, List<FunctionDefinition>& out, const ReadSettings& settings);
        ReadResult ReadList(IReader& reader, List<GlobalDefinition>& out, const ReadSettings& settings);
        ReadResult ReadList(IReader& reader, List<SourceDebugSymbol>& out, const ReadSettings& settings);
        ReadResult ReadString(IReader& reader, String& out, const ReadSettings& settings);
        ReadResult ReadSized(IReader& reader, std::function<ReadResult(ByteReader&, const ReadSettings&)> func, const ReadSettings& settings);

        ReadResult ReadSourcePosition(IReader& reader, SourcePosition& out, const ReadSettings& settings);
        ReadResult ReadToken(IReader& reader, Token& out, const ReadSettings& settings);

        ReadResult ReadFunctionDebugSymbol(IReader& reader, FunctionDebugSymbol& out, const ReadSettings& settings);
        ReadResult ReadBlockDebugSymbol(IReader& reader, BlockDebugSymbol& out, const ReadSettings& settings);
        
        ReadResult ReadInstruction(IReader& reader, Instruction& out, const ReadSettings& settings);
        ReadResult ReadFunctionDefinition(IReader& reader, FunctionDefinition& out, const ReadSettings& settings);
        
        ReadResult ReadGlobalDebugSymbol(IReader& reader, GlobalDebugSymbol& out, const ReadSettings& settings);

        ReadResult ReadGlobalDefinition(IReader& reader, GlobalDefinition& out, const ReadSettings& settings);

        ReadResult ReadSourceDebugSymbol(IReader& reader, SourceDebugSymbol& out, const ReadSettings& settings);

        ReadResult ReadHeader(IReader& reader, const ReadSettings& settings);
        ReadResult ReadModule(IReader& reader, Module& out, const ReadSettings& settings);

        ReadResult ReadValue(IReader& reader, Value& out, const ReadSettings& settings);

        // LinkedLists are stored like normal Lists
        bool WriteLinkedList(IWriter& writer, const LinkedList<Value>& list, const WriteSettings& settings);
        bool WriteList(IWriter& writer, const List<Value>& list, const WriteSettings& settings);
        bool WriteList(IWriter& writer, const List<Instruction>& list, const WriteSettings& settings);
        bool WriteList(IWriter& writer, const List<BlockDebugSymbol>& list, const WriteSettings& settings);
        bool WriteList(IWriter& writer, const List<FunctionDefinition>& list, const WriteSettings& settings);
        bool WriteList(IWriter& writer, const List<GlobalDefinition>& list, const WriteSettings& settings);
        bool WriteList(IWriter& writer, const List<SourceDebugSymbol>& list, const WriteSettings& settings);
        bool WriteString(IWriter& writer, const String& string, const WriteSettings& settings);
        bool WriteSized(IWriter& writer, std::function<bool(ByteWriter&, const WriteSettings&)> func, const WriteSettings& settings);

        bool WriteSourcePosition(IWriter& writer, const SourcePosition& sourcePos, const WriteSettings& settings);
        bool WriteToken(IWriter& writer, const Token& token, const WriteSettings& settings);

        bool WriteFunctionDebugSymbol(IWriter& writer, const FunctionDebugSymbol& debugSymbol, const WriteSettings& settings);
        bool WriteBlockDebugSymbol(IWriter& writer, const BlockDebugSymbol& debugSymbol, const WriteSettings& settings);
        
        bool WriteInstruction(IWriter& writer, const Instruction& instr, const WriteSettings& settings);
        bool WriteFunctionDefinition(IWriter& writer, const FunctionDefinition& funcDef, const WriteSettings& settings);
        
        bool WriteGlobalDebugSymbol(IWriter& writer, const GlobalDebugSymbol& debugSymbol, const WriteSettings& settings);

        bool WriteGlobalDefinition(IWriter& writer, const GlobalDefinition& globDef, const WriteSettings& settings);

        bool WriteSourceDebugSymbol(IWriter& writer, const SourceDebugSymbol& debugSymbol, const WriteSettings& settings);

        bool WriteHeader(IWriter& writer, const WriteSettings& settings);
        bool WriteModule(IWriter& writer, const Module& module, const WriteSettings& settings);

        bool WriteValue(IWriter& writer, const Value& value, const WriteSettings& settings);
    }
}

#endif // _PULSAR_BINARY_BYTECODE_H
