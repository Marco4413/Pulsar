#ifndef _PULSAR_RUNTIME_INSTRUCTION_H
#define _PULSAR_RUNTIME_INSTRUCTION_H

#include "pulsar/core.h"

namespace Pulsar
{
    enum class InstructionCode {
        // Stack
        PushInt = 0x01,
        PushDbl = 0x02,
        PushFunctionReference       = 0x03,
        PushNativeFunctionReference = 0x04,
        PushEmptyList = 0x05,

        Pop  = 0x0D,
        Swap = 0x0E,
        Dup  = 0x0F,

        // Constants, Locals and Globals
        PushConst = 0x10,

        PushLocal     = 0x11,
        MoveLocal     = 0x12,
        PopIntoLocal  = 0x13,
        CopyIntoLocal = 0x14,

        PushGlobal     = 0x15,
        MoveGlobal     = 0x16,
        PopIntoGlobal  = 0x17,
        CopyIntoGlobal = 0x18,

        // Functions
        Return     = 0x20,
        Call       = 0x21,
        CallNative = 0x22,
        ICall      = 0x2F,

        // Math
        DynSum = 0x30,
        DynSub = 0x31,
        DynMul = 0x32,
        DynDiv = 0x33,
        Mod    = 0x34,
        BitAnd = 0x38,
        BitOr  = 0x39,
        BitNot = 0x3A,
        BitXor = 0x3B,
        BitShiftLeft  = 0x3C,
        BitShiftRight = 0x3D,
        Floor = 0x40,
        Ceil  = 0x41,
        // Comparison
        Compare = 0x50,
        // Jumps
        Jump                           = 0x60,
        JumpIfZero                     = 0x61,
        JumpIfNotZero                  = 0x62,
        JumpIfGreaterThanZero          = 0x63,
        JumpIfGreaterThanOrEqualToZero = 0x64,
        JumpIfLessThanZero             = 0x65,
        JumpIfLessThanOrEqualToZero    = 0x66,
        // Sized Values
        IsEmpty = 0x70,
        Length  = 0x71,
        Prepend = 0x72,
        Append  = 0x73,
        Index   = 0x74,
        //   List-Specific
        Concat = 0x78,
        Head   = 0x79,
        Tail   = 0x7A,
        //   String-Specific
        Prefix = 0x7C,
        Suffix = 0x7D,
        Substr = 0x7E,
        // Type Checking
        IsVoid    = 0x80,
        IsInteger = 0x81,
        IsDouble  = 0x82,
        IsFunctionReference       = 0x83,
        IsNativeFunctionReference = 0x84,
        IsList   = 0x85,
        IsString = 0x86,
        IsCustom = 0x8F,
        // Compound Type Checking
        IsNumber = 0x90,
        IsAnyFunctionReference = 0x91,
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
