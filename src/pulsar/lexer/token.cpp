#include "pulsar/lexer/token.h"

const char* Pulsar::TokenTypeToString(TokenType ttype)
{
    switch (ttype) {
    case TokenType::None:              return "None";
    case TokenType::Identifier:        return "Identifier";
    case TokenType::OpenParenth:       return "OpenParenth";
    case TokenType::CloseParenth:      return "CloseParenth";
    case TokenType::OpenBracket:       return "OpenBracket";
    case TokenType::CloseBracket:      return "CloseBracket";
    case TokenType::IntegerLiteral:    return "IntegerLiteral";
    case TokenType::DoubleLiteral:     return "DoubleLiteral";
    case TokenType::StringLiteral:     return "StringLiteral";
    case TokenType::Plus:              return "Plus";
    case TokenType::Minus:             return "Minus";
    case TokenType::Star:              return "Star";
    case TokenType::Slash:             return "Slash";
    case TokenType::Modulus:           return "Modulus";
    case TokenType::BitAnd:            return "BitAnd";
    case TokenType::BitOr:             return "BitOr";
    case TokenType::BitNot:            return "BitNot";
    case TokenType::BitXor:            return "BitXor";
    case TokenType::BitShiftLeft:      return "BitShiftLeft";
    case TokenType::BitShiftRight:     return "BitShiftRight";
    case TokenType::FullStop:          return "FullStop";
    case TokenType::Negate:            return "Negate";
    case TokenType::Colon:             return "Colon";
    case TokenType::Comma:             return "Comma";
    case TokenType::RightArrow:        return "RightArrow";
    case TokenType::LeftArrow:         return "LeftArrow";
    case TokenType::BothArrows:        return "BothArrows";
    case TokenType::Equals:            return "Equals";
    case TokenType::NotEquals:         return "NotEquals";
    case TokenType::Less:              return "Less";
    case TokenType::LessOrEqual:       return "LessOrEqual";
    case TokenType::More:              return "More";
    case TokenType::MoreOrEqual:       return "MoreOrEqual";
    case TokenType::PushReference:     return "PushReference";
    case TokenType::KW_Not:            return "KW_Not";
    case TokenType::KW_If:             return "KW_If";
    case TokenType::KW_Else:           return "KW_Else";
    case TokenType::KW_End:            return "KW_End";
    case TokenType::KW_Global:         return "KW_Global";
    case TokenType::KW_Const:          return "KW_Const";
    case TokenType::KW_Do:             return "KW_Do";
    case TokenType::KW_While:          return "KW_While";
    case TokenType::KW_Break:          return "KW_Break";
    case TokenType::KW_Continue:       return "KW_Continue";
    case TokenType::KW_Local:          return "KW_Local";
    case TokenType::CompilerDirective: return "CompilerDirective";
    case TokenType::Label:             return "Label";
    case TokenType::EndOfFile:         return "EndOfFile";
    }
    return "Unknown";
}
