#ifndef _PULSAR_RUNTIME_INSTRUCTION_H
#define _PULSAR_RUNTIME_INSTRUCTION_H

#include <cinttypes>

namespace Pulsar
{
    enum class InstructionCode {
        PushInt, PushDbl,
        PushFunctionReference,
        PushNativeFunctionReference,
        PushLocal, PopIntoLocal,
        Call, CallNative, Return,
        ICall,
        DynSum, DynSub, DynMul, DynDiv,
        Mod,
        Compare,
        Jump,
        JumpIfZero,
        JumpIfNotZero,
        JumpIfGreaterThanZero,
        JumpIfGreaterThanOrEqualToZero,
        JumpIfLessThanZero,
        JumpIfLessThanOrEqualToZero,
    };

    struct Instruction
    {
        InstructionCode Code;
        int64_t Arg0 = 0;
    };

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
