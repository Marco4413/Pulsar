#include "pulsar/bytecode.h"

#include "pulsar/binary/bytereader.h"
#include "pulsar/binary/bytewriter.h"

#include "pulsar/utf8.h"

#define RETURN_IF_NOT_OK(expr)     \
    do {                           \
        auto res = (expr);         \
        if (res != ReadResult::OK) \
            return res;            \
    } while (false)

Pulsar::Binary::ReadResult Pulsar::Binary::ReadByteCode(IReader& reader, Module& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ByteCode::ReadHeader(reader, settings));
    return ByteCode::ReadModule(reader, out, settings);
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadLinkedList(IReader& reader, LinkedList<Value>& out, const ReadSettings& settings)
{
    LinkedList<Value> list;
    uint64_t size = 0;
    if (!reader.ReadU64(size))
        return ReadResult::UnexpectedEOF;
    for (uint64_t i = 0; i < size; i++)
        RETURN_IF_NOT_OK(ReadValue(reader, list.Append()->Value(), settings));
    out = std::move(list);
    return ReadResult::OK;
}

#define READ_LIST(list, reader, rFunc, settings) \
    uint64_t size = 0;                           \
    if (!(reader).ReadU64(size))                 \
        return ReadResult::UnexpectedEOF;        \
    (list).Reserve((list).Size()                 \
        +(size_t)size);                          \
    for (uint64_t i = 0; i < size; i++) {        \
        RETURN_IF_NOT_OK(rFunc(reader,           \
            (list).EmplaceBack(), (settings)));  \
    }

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadList(IReader& reader, List<Value>& out, const ReadSettings& settings)
{
    READ_LIST(out, reader, ReadValue, settings);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadList(IReader& reader, List<Instruction>& out, const ReadSettings& settings)
{
    READ_LIST(out, reader, ReadInstruction, settings);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadList(IReader& reader, List<BlockDebugSymbol>& out, const ReadSettings& settings)
{
    READ_LIST(out, reader, ReadBlockDebugSymbol, settings);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadList(IReader& reader, List<FunctionDefinition>& out, const ReadSettings& settings)
{
    READ_LIST(out, reader, ReadFunctionDefinition, settings);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadList(IReader& reader, List<GlobalDefinition>& out, const ReadSettings& settings)
{
    READ_LIST(out, reader, ReadGlobalDefinition, settings);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadList(IReader& reader, List<SourceDebugSymbol>& out, const ReadSettings& settings)
{
    READ_LIST(out, reader, ReadSourceDebugSymbol, settings);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadString(IReader& reader, String& out, bool requireValidUTF8, const ReadSettings& settings)
{
    (void)settings;
    uint64_t length = 0;
    if (!reader.ReadU64(length))
        return ReadResult::UnexpectedEOF;
    String str((size_t)length, '\0');
    if (!reader.ReadData((uint64_t)str.Length(), (uint8_t*)str.Data()))
        return ReadResult::UnexpectedEOF;

    if (requireValidUTF8) {
        UTF8::Decoder decoder(str);
        while (decoder) decoder.Next();
        if (decoder.IsInvalidEncoding())
            return ReadResult::InvalidUTF8Encoding;
    }

    out = std::move(str);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadSized(IReader& reader, std::function<ReadResult(ByteReader&, const ReadSettings&)> func, const ReadSettings& settings)
{
    uint64_t size = 0;
    if (!reader.ReadU64(size))
        return ReadResult::UnexpectedEOF;

    List<uint8_t> data;
    data.Resize((size_t)size);
    if (!reader.ReadData((uint64_t)data.Size(), (uint8_t*)data.Data()))
        return ReadResult::UnexpectedEOF;

    ByteReader byteReader(data.Size(), data.Data());
    RETURN_IF_NOT_OK(func(byteReader, settings));

    return byteReader.IsAtEndOfFile()
        ? ReadResult::OK
        : ReadResult::DataNotConsumed;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadSourcePosition(IReader& reader, SourcePosition& out, const ReadSettings& settings)
{
    (void)settings;
    uint64_t line;
    if (!reader.ReadU64(line))
        return ReadResult::UnexpectedEOF;
    out.Line = (size_t)line;
    uint64_t _char;
    if (!reader.ReadU64(_char))
        return ReadResult::UnexpectedEOF;
    out.Char = (size_t)_char;
    uint64_t index;
    if (!reader.ReadU64(index))
        return ReadResult::UnexpectedEOF;
    out.Index = (size_t)index;
    uint64_t charSpan;
    if (!reader.ReadU64(charSpan))
        return ReadResult::UnexpectedEOF;
    out.CharSpan = (size_t)charSpan;
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadToken(IReader& reader, Token& out, const ReadSettings& settings)
{
    uint16_t tokenType = 0;
    if (!reader.ReadU16(tokenType))
        return ReadResult::UnexpectedEOF;
    out.Type = (TokenType)tokenType;
    return ReadSourcePosition(reader, out.SourcePos, settings);
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadFunctionDebugSymbol(IReader& reader, FunctionDebugSymbol& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ReadToken(reader, out.Token, settings));
    uint64_t sourceIdx = 0;
    if (!reader.ReadU64(sourceIdx))
        return ReadResult::UnexpectedEOF;
    out.SourceIdx = (size_t)sourceIdx;
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadBlockDebugSymbol(IReader& reader, BlockDebugSymbol& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ReadToken(reader, out.Token, settings));
    uint64_t startIdx = 0;
    if (!reader.ReadU64(startIdx))
        return ReadResult::UnexpectedEOF;
    out.StartIdx = (size_t)startIdx;
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadInstruction(IReader& reader, Instruction& out, const ReadSettings& settings)
{
    (void)settings;
    uint8_t instrCode = 0;
    if (!reader.ReadU8(instrCode))
        return ReadResult::UnexpectedEOF;
    out.Code = (InstructionCode)instrCode;
    return reader.ReadI64(out.Arg0)
        ? ReadResult::OK
        : ReadResult::UnexpectedEOF;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadFunctionDefinition(IReader& reader, FunctionDefinition& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ReadString(reader, out.Name, true, settings));
    uint64_t arity = 0;
    if (!reader.ReadU64(arity))
        return ReadResult::UnexpectedEOF;
    out.Arity = (size_t)arity;
    uint64_t returns = 0;
    if (!reader.ReadU64(returns))
        return ReadResult::UnexpectedEOF;
    out.Returns = (size_t)returns;
    uint64_t stackArity = 0;
    if (!reader.ReadU64(stackArity))
        return ReadResult::UnexpectedEOF;
    out.StackArity = (size_t)stackArity;
    uint64_t localsCount = 0;
    if (!reader.ReadU64(localsCount))
        return ReadResult::UnexpectedEOF;
    out.LocalsCount = (size_t)localsCount;

    RETURN_IF_NOT_OK(ReadSized(reader, [&out](IReader& reader, const ReadSettings& settings) mutable {
        return ReadList(reader, out.Code, settings);
    }, settings));

    return ReadSized(reader, [&out](ByteReader& reader, const ReadSettings& settings) mutable {
        if (!settings.LoadDebugSymbols) {
            reader.DiscardBytes();
            return ReadResult::OK;
        }
        if (reader.IsAtEndOfFile())
            return ReadResult::OK;
        RETURN_IF_NOT_OK(ReadFunctionDebugSymbol(reader, out.DebugSymbol, settings));
        return ReadList(reader, out.CodeDebugSymbols, settings);
    }, settings);
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadGlobalDebugSymbol(IReader& reader, GlobalDebugSymbol& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ReadToken(reader, out.Token, settings));
    uint64_t sourceIdx = 0;
    if (!reader.ReadU64(sourceIdx))
        return ReadResult::UnexpectedEOF;
    out.SourceIdx = (size_t)sourceIdx;
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadGlobalDefinition(IReader& reader, GlobalDefinition& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ReadString(reader, out.Name, true, settings));
    uint8_t flags = 0;
    if (!reader.ReadU8(flags))
        return ReadResult::UnexpectedEOF;
    out.IsConstant = (flags & GLOBAL_FLAG_CONSTANT) != 0;
    RETURN_IF_NOT_OK(ReadValue(reader, out.InitialValue, settings));
    return ReadSized(reader, [&out](ByteReader& reader, const ReadSettings& settings) mutable {
        if (!settings.LoadDebugSymbols) {
            reader.DiscardBytes();
            return ReadResult::OK;
        }
        if (reader.IsAtEndOfFile())
            return ReadResult::OK;
        return ReadGlobalDebugSymbol(reader, out.DebugSymbol, settings);
    }, settings);
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadSourceDebugSymbol(IReader& reader, SourceDebugSymbol& out, const ReadSettings& settings)
{
    RETURN_IF_NOT_OK(ReadString(reader, out.Path, true, settings));
    return ReadString(reader, out.Source, true, settings);
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadHeader(IReader& reader, const ReadSettings& settings)
{
    (void)settings;
    char sig[SIGNATURE_LENGTH];
    if (!reader.ReadData(SIGNATURE_LENGTH, (uint8_t*)sig))
        return ReadResult::UnexpectedEOF;
    for (size_t i = 0; i < (size_t)SIGNATURE_LENGTH; i++) {
        if (sig[i] != SIGNATURE[i])
            return ReadResult::InvalidSignature;
    }
    uint32_t version = 0;
    if (!reader.ReadU32(version))
        return ReadResult::UnexpectedEOF;
    return version == FORMAT_VERSION
        ? ReadResult::OK
        : ReadResult::UnsupportedVersion;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadModule(IReader& reader, Module& out, const ReadSettings& settings)
{
    uint64_t moduleSize = 0;
    if (!reader.ReadU64(moduleSize))
        return ReadResult::UnexpectedEOF;
    Module module;
    while (true) {
        uint8_t chunkType = 0;
        if (!reader.ReadU8(chunkType))
            return ReadResult::UnexpectedEOF;
        RETURN_IF_NOT_OK(ReadSized(reader, [&module, chunkType](ByteReader& reader, const ReadSettings& settings) mutable {
            switch (chunkType) {
            case CHUNK_END_OF_MODULE:
                return ReadResult::OK;
            case CHUNK_FUNCTIONS:
                return ReadList(reader, module.Functions, settings);
            case CHUNK_NATIVE_BINDINGS:
                return ReadList(reader, module.NativeBindings, settings);
            case CHUNK_GLOBALS:
                return ReadList(reader, module.Globals, settings);
            case CHUNK_CONSTANTS:
                return ReadList(reader, module.Constants, settings);
            case CHUNK_SOURCE_DEBUG_SYMBOLS:
                if (settings.LoadDebugSymbols)
                    return ReadList(reader, module.SourceDebugSymbols, settings);
                reader.DiscardBytes();
                return ReadResult::OK;
            default:
                if (IsOptionalChunk(chunkType)) {
                    reader.DiscardBytes();
                    return ReadResult::OK;
                }
                return ReadResult::UnsupportedChunkType;
            }
        }, settings));
        if (chunkType == CHUNK_END_OF_MODULE)
            break;
    }
    module.NativeFunctions.Resize(module.NativeBindings.Size());
    out = std::move(module);
    return ReadResult::OK;
}

Pulsar::Binary::ReadResult Pulsar::Binary::ByteCode::ReadValue(IReader& reader, Value& out, const ReadSettings& settings)
{
    uint8_t valueType = 0;
    if (!reader.ReadU8(valueType))
        return ReadResult::UnexpectedEOF;
    return ReadSized(reader, [&out, valueType](IReader& reader, const ReadSettings& settings) mutable {
        switch ((ValueType)valueType) {
        case ValueType::Void:
            return ReadResult::OK;
        case ValueType::Integer: {
            int64_t val;
            if (!reader.ReadI64(val))
                return ReadResult::UnexpectedEOF;
            out.SetInteger(val);
        } break;
        case ValueType::Double: {
            double val;
            if (!reader.ReadF64(val))
                return ReadResult::UnexpectedEOF;
            out.SetDouble(val);
        } break;
        case ValueType::FunctionReference: {
            int64_t idx;
            if (!reader.ReadI64(idx))
                return ReadResult::UnexpectedEOF;
            out.SetFunctionReference(idx);
        } break;
        case ValueType::NativeFunctionReference: {
            int64_t idx;
            if (!reader.ReadI64(idx))
                return ReadResult::UnexpectedEOF;
            out.SetNativeFunctionReference(idx);
        } break;
        case ValueType::List: {
            LinkedList<Value> list;
            RETURN_IF_NOT_OK(ReadLinkedList(reader, list, settings));
            out.SetList(std::move(list));
        } break;
        case ValueType::String: {
            String str;
            RETURN_IF_NOT_OK(ReadString(reader, str, false, settings));
            out.SetString(std::move(str));
        } break;
        case ValueType::Custom:
            return ReadResult::UnsupportedCustomDataType;
        default:
            return ReadResult::UnsupportedValueType;
        }
        return ReadResult::OK;
    }, settings);
}

bool Pulsar::Binary::WriteByteCode(IWriter& writer, const Module& module, const WriteSettings& settings)
{
    return ByteCode::WriteHeader(writer, settings)
        && ByteCode::WriteModule(writer, module, settings);
}

bool Pulsar::Binary::ByteCode::WriteLinkedList(IWriter& writer, const LinkedList<Value>& list, const WriteSettings& settings)
{
    if (!writer.WriteU64((uint64_t)list.Length()))
        return false;
    const LinkedList<Value>::Node* next = list.Front();
    while (next) {
        if (!WriteValue(writer, next->Value(), settings))
            return false;
        next = next->Next();
    }
    return true;
}

#define WRITE_LIST(list, writer, wFunc, settings)    \
    if (!(writer).WriteU64((uint64_t)(list).Size())) \
        return false;                                \
    for (size_t i = 0; i < (list).Size(); i++) {     \
        if (!wFunc((writer), (list)[i], (settings))) \
            return false;                            \
    }

bool Pulsar::Binary::ByteCode::WriteList(IWriter& writer, const List<Value>& list, const WriteSettings& settings)
{
    WRITE_LIST(list, writer, WriteValue, settings);
    return true;
}

bool Pulsar::Binary::ByteCode::WriteList(IWriter& writer, const List<Instruction>& list, const WriteSettings& settings)
{
    WRITE_LIST(list, writer, WriteInstruction, settings);
    return true;
}

bool Pulsar::Binary::ByteCode::WriteList(IWriter& writer, const List<BlockDebugSymbol>& list, const WriteSettings& settings)
{
    WRITE_LIST(list, writer, WriteBlockDebugSymbol, settings);
    return true;
}

bool Pulsar::Binary::ByteCode::WriteList(IWriter& writer, const List<FunctionDefinition>& list, const WriteSettings& settings)
{
    WRITE_LIST(list, writer, WriteFunctionDefinition, settings);
    return true;
}

bool Pulsar::Binary::ByteCode::WriteList(IWriter& writer, const List<GlobalDefinition>& list, const WriteSettings& settings)
{
    WRITE_LIST(list, writer, WriteGlobalDefinition, settings);
    return true;
}

bool Pulsar::Binary::ByteCode::WriteList(IWriter& writer, const List<SourceDebugSymbol>& list, const WriteSettings& settings)
{
    WRITE_LIST(list, writer, WriteSourceDebugSymbol, settings);
    return true;
}

bool Pulsar::Binary::ByteCode::WriteString(IWriter& writer, const String& string, const WriteSettings& settings)
{
    (void)settings;
    return writer.WriteU64((uint64_t)string.Length())
        && writer.WriteData(string.Length(), (const uint8_t*)string.Data());
}

bool Pulsar::Binary::ByteCode::WriteSized(IWriter& writer, std::function<bool(ByteWriter&, const WriteSettings&)> func, const WriteSettings& settings)
{
    ByteWriter bWriter;
    if (!func(bWriter, settings))
        return false;
    return writer.WriteU64((uint64_t)bWriter.Bytes().Size())
        && writer.WriteData(bWriter.Bytes().Size(), bWriter.Bytes().Data());
}

bool Pulsar::Binary::ByteCode::WriteSourcePosition(IWriter& writer, const SourcePosition& sourcePos, const WriteSettings& settings)
{
    (void)settings;
    return writer.WriteU64((uint64_t)sourcePos.Line)
        && writer.WriteU64((uint64_t)sourcePos.Char)
        && writer.WriteU64((uint64_t)sourcePos.Index)
        && writer.WriteU64((uint64_t)sourcePos.CharSpan);
}

bool Pulsar::Binary::ByteCode::WriteToken(IWriter& writer, const Token& token, const WriteSettings& settings)
{
    return writer.WriteU16((uint16_t)token.Type)
        && WriteSourcePosition(writer, token.SourcePos, settings);
}

bool Pulsar::Binary::ByteCode::WriteFunctionDebugSymbol(IWriter& writer, const FunctionDebugSymbol& debugSymbol, const WriteSettings& settings)
{
    return WriteToken(writer, debugSymbol.Token, settings)
        && writer.WriteU64((uint64_t)debugSymbol.SourceIdx);
}

bool Pulsar::Binary::ByteCode::WriteBlockDebugSymbol(IWriter& writer, const BlockDebugSymbol& debugSymbol, const WriteSettings& settings)
{
    return WriteToken(writer, debugSymbol.Token, settings)
        && writer.WriteU64((uint64_t)debugSymbol.StartIdx);
}

bool Pulsar::Binary::ByteCode::WriteInstruction(IWriter& writer, const Instruction& instr, const WriteSettings& settings)
{
    (void)settings;
    return writer.WriteU8((uint8_t)instr.Code)
        && writer.WriteSLEB(instr.Arg0);
}

bool Pulsar::Binary::ByteCode::WriteFunctionDefinition(IWriter& writer, const FunctionDefinition& funcDef, const WriteSettings& settings)
{
    if (!(WriteString(writer, funcDef.Name, settings)
        && writer.WriteU64((uint64_t)funcDef.Arity)
        && writer.WriteU64((uint64_t)funcDef.Returns)
        && writer.WriteU64((uint64_t)funcDef.StackArity)
        && writer.WriteU64((uint64_t)funcDef.LocalsCount)
    )) return false;
    return WriteSized(writer, [&funcDef](IWriter& writer, const WriteSettings& settings) {
        return WriteList(writer, funcDef.Code, settings);
    }, settings) && WriteSized(writer, [&funcDef](IWriter& writer, const WriteSettings& settings) {
        if (settings.StoreDebugSymbols && (funcDef.HasDebugSymbol() || funcDef.HasCodeDebugSymbols())) {
            return WriteFunctionDebugSymbol(writer, funcDef.DebugSymbol, settings)
                && WriteList(writer, funcDef.CodeDebugSymbols, settings);
        }
        return true;
    }, settings);
}

bool Pulsar::Binary::ByteCode::WriteGlobalDebugSymbol(IWriter& writer, const GlobalDebugSymbol& debugSymbol, const WriteSettings& settings)
{
    return WriteToken(writer, debugSymbol.Token, settings)
        && writer.WriteU64((uint64_t)debugSymbol.SourceIdx);
}

bool Pulsar::Binary::ByteCode::WriteGlobalDefinition(IWriter& writer, const GlobalDefinition& globDef, const WriteSettings& settings)
{
    if (!(WriteString(writer, globDef.Name, settings)
        && writer.WriteU8(globDef.IsConstant ? GLOBAL_FLAG_CONSTANT : 0)
        && WriteValue(writer, globDef.InitialValue, settings)
    )) return false;
    return WriteSized(writer, [&globDef](IWriter& writer, const WriteSettings& settings) {
        if (settings.StoreDebugSymbols && globDef.HasDebugSymbol())
            return WriteGlobalDebugSymbol(writer, globDef.DebugSymbol, settings);
        return true;
    }, settings);
}

bool Pulsar::Binary::ByteCode::WriteSourceDebugSymbol(IWriter& writer, const SourceDebugSymbol& debugSymbol, const WriteSettings& settings)
{
    return WriteString(writer, debugSymbol.Path, settings)
        && WriteString(writer, debugSymbol.Source, settings);
}

bool Pulsar::Binary::ByteCode::WriteHeader(IWriter& writer, const WriteSettings& settings)
{
    (void)settings;
    return writer.WriteData(SIGNATURE_LENGTH, (const uint8_t*)SIGNATURE)
        && writer.WriteU32(FORMAT_VERSION);
}

bool Pulsar::Binary::ByteCode::WriteModule(IWriter& writer, const Module& module, const WriteSettings& settings)
{
    return WriteSized(writer, [&module](IWriter& writer, const WriteSettings& settings) {
        // Chunk/Functions
        if (module.Functions.Size() > 0) {
            if (!writer.WriteU8(CHUNK_FUNCTIONS))
                return false;
            if (!WriteSized(writer, [&module](IWriter& writer, const WriteSettings& settings) {
                return WriteList(writer, module.Functions, settings);
            }, settings)) return false;
        }
        // Chunk/NativeBindings
        if (module.NativeBindings.Size() > 0) {
            if (!writer.WriteU8(CHUNK_NATIVE_BINDINGS))
                return false;
            if (!WriteSized(writer, [&module](IWriter& writer, const WriteSettings& settings) {
                return WriteList(writer, module.NativeBindings, settings);
            }, settings)) return false;
        }
        // Chunk/Globals
        if (module.Globals.Size() > 0) {
            if (!writer.WriteU8(CHUNK_GLOBALS))
                return false;
            if (!WriteSized(writer, [&module](IWriter& writer, const WriteSettings& settings) {
                return WriteList(writer, module.Globals, settings);
            }, settings)) return false;
        }
        // Chunk/Constants
        if (module.Constants.Size() > 0) {
            if (!writer.WriteU8(CHUNK_CONSTANTS))
                return false;
            if (!WriteSized(writer, [&module](IWriter& writer, const WriteSettings& settings) {
                return WriteList(writer, module.Constants, settings);
            }, settings)) return false;
        }
        if (settings.StoreDebugSymbols) {
            // Chunk/SourceDebugSymbols
            if (module.SourceDebugSymbols.Size() > 0) {
                if (!writer.WriteU8(CHUNK_SOURCE_DEBUG_SYMBOLS))
                    return false;
                if (!WriteSized(writer, [&module](IWriter& writer, const WriteSettings& settings) {
                    return WriteList(writer, module.SourceDebugSymbols, settings);
                }, settings)) return false;
            }
        }
        
        return writer.WriteU8(CHUNK_END_OF_MODULE)
            && writer.WriteU64(0);
    }, settings);
}

bool Pulsar::Binary::ByteCode::WriteValue(IWriter& writer, const Value& value, const WriteSettings& settings)
{
    if (!writer.WriteU8((uint8_t)value.Type()))
        return false;
    return WriteSized(writer, [&value](IWriter& writer, const WriteSettings& settings) {
        switch (value.Type()) {
        case ValueType::Void:
            return true;
        case ValueType::Integer:
        case ValueType::FunctionReference:
        case ValueType::NativeFunctionReference:
            return writer.WriteI64(value.AsInteger());
        case ValueType::Double:
            return writer.WriteF64(value.AsDouble());
        case ValueType::List:
            return WriteLinkedList(writer, value.AsList(), settings);
        case ValueType::String:
            return WriteString(writer, value.AsString(), settings);
        case ValueType::Custom:
        default:
            return false;
        }
    }, settings);
}

const char* Pulsar::Binary::ReadResultToString(ReadResult rr)
{
    switch (rr) {
    case ReadResult::OK:
        return "OK";
    case ReadResult::Error:
        return "Error";
    case ReadResult::UnexpectedEOF:
        return "UnexpectedEOF";
    case ReadResult::DataNotConsumed:
        return "DataNotConsumed";
    case ReadResult::InvalidSignature:
        return "InvalidSignature";
    case ReadResult::UnsupportedVersion:
        return "UnsupportedVersion";
    case ReadResult::UnsupportedChunkType:
        return "UnsupportedChunkType";
    case ReadResult::UnsupportedCustomDataType:
        return "UnsupportedCustomDataType";
    case ReadResult::UnsupportedValueType:
        return "UnsupportedValueType";
    case ReadResult::InvalidUTF8Encoding:
        return "InvalidUTF8Encoding";
    }
    return "Unknown";
}
