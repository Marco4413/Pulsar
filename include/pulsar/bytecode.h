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

    ReadResult ReadByteCode(IReader& reader, Module& out);
    bool WriteByteCode(IWriter& writer, const Module& module);
    
    namespace ByteCode {
        // LinkedLists are stored like normal Lists
        ReadResult ReadLinkedList(IReader& reader, LinkedList<Value>& out);
        ReadResult ReadList(IReader& reader, List<Value>& out);
        ReadResult ReadList(IReader& reader, List<Instruction>& out);
        ReadResult ReadList(IReader& reader, List<BlockDebugSymbol>& out);
        ReadResult ReadList(IReader& reader, List<FunctionDefinition>& out);
        ReadResult ReadList(IReader& reader, List<GlobalDefinition>& out);
        ReadResult ReadList(IReader& reader, List<SourceDebugSymbol>& out);
        ReadResult ReadString(IReader& reader, String& out);
        ReadResult ReadSized(IReader& reader, std::function<ReadResult(ByteReader&)> func);

        ReadResult ReadSourcePosition(IReader& reader, SourcePosition& out);
        ReadResult ReadToken(IReader& reader, Token& out);

        ReadResult ReadFunctionDebugSymbol(IReader& reader, FunctionDebugSymbol& out);
        ReadResult ReadBlockDebugSymbol(IReader& reader, BlockDebugSymbol& out);
        
        ReadResult ReadInstruction(IReader& reader, Instruction& out);
        ReadResult ReadFunctionDefinition(IReader& reader, FunctionDefinition& out);
        
        ReadResult ReadGlobalDebugSymbol(IReader& reader, GlobalDebugSymbol& out);

        ReadResult ReadGlobalDefinition(IReader& reader, GlobalDefinition& out);

        ReadResult ReadSourceDebugSymbol(IReader& reader, SourceDebugSymbol& out);

        ReadResult ReadHeader(IReader& reader);
        ReadResult ReadModule(IReader& reader, Module& out);

        ReadResult ReadValue(IReader& reader, Value& out);

        // LinkedLists are stored like normal Lists
        bool WriteLinkedList(IWriter& writer, const LinkedList<Value>& list);
        bool WriteList(IWriter& writer, const List<Value>& list);
        bool WriteList(IWriter& writer, const List<Instruction>& list);
        bool WriteList(IWriter& writer, const List<BlockDebugSymbol>& list);
        bool WriteList(IWriter& writer, const List<FunctionDefinition>& list);
        bool WriteList(IWriter& writer, const List<GlobalDefinition>& list);
        bool WriteList(IWriter& writer, const List<SourceDebugSymbol>& list);
        bool WriteString(IWriter& writer, const String& string);
        bool WriteSized(IWriter& writer, std::function<bool(ByteWriter&)> func);

        bool WriteSourcePosition(IWriter& writer, const SourcePosition& sourcePos);
        bool WriteToken(IWriter& writer, const Token& token);

        bool WriteFunctionDebugSymbol(IWriter& writer, const FunctionDebugSymbol& debugSymbol);
        bool WriteBlockDebugSymbol(IWriter& writer, const BlockDebugSymbol& debugSymbol);
        
        bool WriteInstruction(IWriter& writer, const Instruction& instr);
        bool WriteFunctionDefinition(IWriter& writer, const FunctionDefinition& funcDef);
        
        bool WriteGlobalDebugSymbol(IWriter& writer, const GlobalDebugSymbol& debugSymbol);

        bool WriteGlobalDefinition(IWriter& writer, const GlobalDefinition& globDef);

        bool WriteSourceDebugSymbol(IWriter& writer, const SourceDebugSymbol& debugSymbol);

        bool WriteHeader(IWriter& writer);
        bool WriteModule(IWriter& writer, const Module& module);

        bool WriteValue(IWriter& writer, const Value& value);
    }
}

#endif // _PULSAR_BINARY_BYTECODE_H
