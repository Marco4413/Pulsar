#ifndef _PULSAR_RUNTIME_INSTRUCTION_H
#define _PULSAR_RUNTIME_INSTRUCTION_H

#include "pulsar/core.h"

namespace Pulsar
{
    enum class InstructionCode {
        // Stack and Locals
        PushInt, PushDbl,
        PushFunctionReference,
        PushNativeFunctionReference,
        PushConst,
        PushLocal, MoveLocal,
        PopIntoLocal, CopyIntoLocal,
        PushGlobal, MoveGlobal,
        PopIntoGlobal, CopyIntoGlobal,
        Pop, Swap, Dup,
        // Functions
        Call, CallNative, Return,
        ICall,
        // Math
        DynSum, DynSub, DynMul, DynDiv,
        Mod,
        BitAnd, BitOr, BitNot, BitXor,
        BitShiftLeft, BitShiftRight,
        Floor, Ceil,
        // Conditional Jumps
        Compare,
        Jump,
        JumpIfZero,
        JumpIfNotZero,
        JumpIfGreaterThanZero,
        JumpIfGreaterThanOrEqualToZero,
        JumpIfLessThanZero,
        JumpIfLessThanOrEqualToZero,
        // Lists and Strings
        Length, IsEmpty,
        PushEmptyList,
        Prepend, Append, Concat,
        Head, Tail,
        Index,
        Prefix, Suffix, Substr,
        // Type Checking
        IsVoid,
        IsInteger,
        IsDouble,
        IsNumber,
        IsFunctionReference,
        IsNativeFunctionReference,
        IsAnyFunctionReference,
        IsList,
        IsString,
        IsCustom,
    };

    struct Instruction
    {
        InstructionCode Code;
        int64_t Arg0 = 0;
    };

    constexpr InstructionCode InvertJump(InstructionCode jmpInstr)
    {
        switch (jmpInstr) {
        case InstructionCode::JumpIfZero:
            return InstructionCode::JumpIfNotZero;
        case InstructionCode::JumpIfNotZero:
            return InstructionCode::JumpIfZero;
        case InstructionCode::JumpIfGreaterThanZero:
            return InstructionCode::JumpIfLessThanOrEqualToZero;
        case InstructionCode::JumpIfGreaterThanOrEqualToZero:
            return InstructionCode::JumpIfLessThanZero;
        case InstructionCode::JumpIfLessThanZero:
            return InstructionCode::JumpIfGreaterThanOrEqualToZero;
        case InstructionCode::JumpIfLessThanOrEqualToZero:
            return InstructionCode::JumpIfGreaterThanZero;
        case InstructionCode::Jump:
        default:
            return InstructionCode::Jump;
        }
    }

    constexpr bool ShouldJump(InstructionCode jmpInstr, double val)
    {
        switch (jmpInstr) {
        case InstructionCode::Jump:
            return true;
        case InstructionCode::JumpIfZero:
            return val == 0.0;
        case InstructionCode::JumpIfNotZero:
            return val != 0.0;
        case InstructionCode::JumpIfGreaterThanZero:
            return val > 0.0;
        case InstructionCode::JumpIfGreaterThanOrEqualToZero:
            return val >= 0.0;
        case InstructionCode::JumpIfLessThanZero:
            return val < 0.0;
        case InstructionCode::JumpIfLessThanOrEqualToZero:
            return val <= 0.0;
        default:
            return false;
        }
    }

    constexpr bool ShouldJump(InstructionCode jmpInstr, int64_t val)
    {
        switch (jmpInstr) {
        case InstructionCode::Jump:
            return true;
        case InstructionCode::JumpIfZero:
            return val == 0;
        case InstructionCode::JumpIfNotZero:
            return val != 0;
        case InstructionCode::JumpIfGreaterThanZero:
            return val > 0;
        case InstructionCode::JumpIfGreaterThanOrEqualToZero:
            return val >= 0;
        case InstructionCode::JumpIfLessThanZero:
            return val < 0;
        case InstructionCode::JumpIfLessThanOrEqualToZero:
            return val <= 0;
        default:
            return false;
        }
    }
}

#endif // _PULSAR_RUNTIME_INSTRUCTION_H
