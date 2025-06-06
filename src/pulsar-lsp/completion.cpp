#include "pulsar-lsp/completion.h"

#include <atomic>
#include <mutex>

static std::mutex g_InitializingInstructions;
static std::atomic_bool g_InstructionsInitialized;
static PulsarLSP::Completion::InstructionMap g_Instructions;

#define KEYWORD(kw) \
    (lsp::CompletionItem{ .label = (kw), .kind = lsp::CompletionItemKind::Keyword })

static PulsarLSP::Completion::KeywordList g_Keywords{
    KEYWORD("not"),
    KEYWORD("if"),
    KEYWORD("else"),
    KEYWORD("end"),
    KEYWORD("global"),
    KEYWORD("const"),
    KEYWORD("do"),
    KEYWORD("while"),
    KEYWORD("break"),
    KEYWORD("continue"),
    KEYWORD("local"),
};

#define DEFINE_INSTRUCTION(name, arg, signature, doc)        \
    do {                                                     \
        item.label = "(!" name ")";                          \
        item.kind = lsp::CompletionItemKind::Function;       \
        if constexpr (*(arg) == '\0') {                      \
            item.detail = item.label + (": " signature);     \
        } else {                                             \
            item.detail = "(!" name " " arg "): " signature; \
        }                                                    \
        item.documentation = lsp::MarkupContent{             \
            .kind  = lsp::MarkupKind::Markdown,              \
            .value = (doc),                                  \
        };                                                   \
        g_Instructions.Emplace((name), std::move(item));     \
    } while (0)

#define DEFINE_TYPE_CHECK_INSTRUCTION(name, checksFor) \
    DEFINE_INSTRUCTION(name, "", "T -> T, Integer", "Checks if T is of type " checksFor " and returns a non-zero Integer if it is.")

#define DEFINE_CONDITIONAL_JUMP_INSTRUCTION(name, condition) \
    DEFINE_INSTRUCTION(name, "@label", "Number ->", "Jumps to @label if the last Number on the stack is " condition ".")

const PulsarLSP::Completion::InstructionMap& PulsarLSP::Completion::GetInstructions()
{
    if (g_InstructionsInitialized)
        return g_Instructions;
    
    std::lock_guard lock(g_InitializingInstructions);
    if (g_InstructionsInitialized)
        return g_Instructions;

    // Initialize g_Instructions
    lsp::CompletionItem item{};
    DEFINE_INSTRUCTION(
        "pop", "count", "...Any ->",
        "Pops the last value on the stack if `count` is <= 1. "
        "Pops the last `count` values on the stack if `count` > 1."
    );

    DEFINE_INSTRUCTION(
        "swap", "", "T, U -> U, T",
        "Swaps the last two values on the stack."
    );

    DEFINE_INSTRUCTION(
        "dup", "count", "T -> T, ...T",
        "Duplicates the last value on the stack if `count` is <= 1. "
        "Creates `count` copies of the last value on the stack if `count` is > 1."
    );

    DEFINE_INSTRUCTION(
        "floor", "", "Number -> Integer",
        "Replaces the last Number on the stack with the biggest Integer lesser or equal to it."
    );

    DEFINE_INSTRUCTION(
        "ceil", "", "Number -> Integer",
        "Replaces the last Number on the stack with the smallest Integer greater or equal to it."
    );

    DEFINE_INSTRUCTION(
        "compare", "", "T, T -> Number",
        "Compares the last two elements on the stack (`a` and `b`) and pushes a Number:\n"
        "- \\> 0 if a \\> b\n"
        "- \\< 0 if a \\< b\n"
        "- = 0 if a = b\n\n"
        "Both elements must be either String or Number."
    );

    DEFINE_INSTRUCTION(
        "equals?", "", "Any, Any -> Integer",
        "Checks if the last two elements on the stack are equal, "
        "pushing a non-zero Integer if both are the same value."
    );

    DEFINE_INSTRUCTION(
        "icall", "", "...Args, AnyFunctionReference -> ...RetVals",
        "Calls the referenced function like a normal (function) call. "
        "Consuming all necessary arguments and pushing all returned values on the stack."
    );

    DEFINE_INSTRUCTION(
        "length", "", "T -> T, Integer",
        "Calculates the length of a List or String and pushes it onto the stack."
    );

    DEFINE_INSTRUCTION(
        "empty?", "", "T -> T, Integer",
        "Checks if a List or String is empty and pushes a non-zero value if it is."
    );

    DEFINE_INSTRUCTION(
        "prepend", "", "T, U -> T",
        "Where T is either a List or String. And U:\n"
        "- If T is a List: Any\n"
        "- If T is a String: Integer or String\n\n"
        "The result is of type T and is the top-most element on the stack prepended to the one after it. "
        "If T is a String and U an Integer, U is clamped to a single byte."
    );

    DEFINE_INSTRUCTION(
        "append", "", "T, U -> T",
        "Where T is either a List or String. And U:\n"
        "- If T is a List: Any\n"
        "- If T is a String: Integer or String\n\n"
        "The result is of type T and is the top-most element on the stack appended to the one after it. "
        "If T is a String and U an Integer, U is clamped to a single byte."
    );

    DEFINE_INSTRUCTION(
        "concat", "", "List, List -> List",
        "Concatenates the top-most value on the stack with the one after it.\n\n"
        "`[1, 2] [3, 4] (!concat)` produces `[1, 2, 3, 4]`"
    );

    DEFINE_INSTRUCTION(
        "head", "", "List -> List, Any",
        "Removes the first element of a List and pushes it onto the stack."
    );

    DEFINE_INSTRUCTION(
        "tail", "", "List -> List, Any",
        "Removes the last element of a List and pushes it onto the stack."
    );

    DEFINE_INSTRUCTION(
        "pack", "count", "...T -> [ ...T ]",
        "Creates an empty list if `count` <= 0.\n"
        "Packs the last `count` values on the stack into a list if `count` > 0."
    );

    DEFINE_INSTRUCTION(
        "unpack", "count", "[ ...T ] -> ...T",
        "Pops the last list on the stack if `count` <= 0.\n"
        "Unpacks the last `count` values of the list on the stack if `count` > 0."
    );

    DEFINE_INSTRUCTION(
        "index", "", "T, Integer -> T, U",
        "Where T is either a List or String. And U:\n"
        "- If T is a List: Any\n"
        "- If T is a String: Integer\n\n"
        "Copies the value at a certain index in T. "
        "If T is a String, U is an Integer and is the byte stored at the specified index."
    );

    DEFINE_INSTRUCTION(
        "prefix", "", "String, Integer -> String, String",
        "Removes the first bytes of a String, returning those bytes as a String."
    );

    DEFINE_INSTRUCTION(
        "suffix", "", "String, Integer -> String, String",
        "Removes the last bytes of a String, returning those bytes as a String."
    );

    DEFINE_INSTRUCTION(
        "substr", "", "String, Integer, Integer -> String, String",
        "Given: str, start-idx, end-idx\n\n"
        "Returns the substring of `str` starting from `start-idx`, ending at `end-idx` (exclusive)."
    );

    DEFINE_TYPE_CHECK_INSTRUCTION("void?", "Void");
    DEFINE_TYPE_CHECK_INSTRUCTION("integer?", "Integer");
    DEFINE_TYPE_CHECK_INSTRUCTION("double?", "Double");
    DEFINE_TYPE_CHECK_INSTRUCTION("number?", "Integer or Double");
    DEFINE_TYPE_CHECK_INSTRUCTION("fn-ref?", "FunctionReference");
    DEFINE_TYPE_CHECK_INSTRUCTION("native-fn-ref?", "NativeFunctionReference");
    DEFINE_TYPE_CHECK_INSTRUCTION("any-fn-ref?", "FunctionReference or NativeFunctionReference");
    DEFINE_TYPE_CHECK_INSTRUCTION("list?", "List");
    DEFINE_TYPE_CHECK_INSTRUCTION("string?", "String");
    DEFINE_TYPE_CHECK_INSTRUCTION("custom?", "Custom");

    DEFINE_INSTRUCTION(
        "j!", "@label", "->",
        "Unconditionally performs a jump to @label."
    );

    DEFINE_CONDITIONAL_JUMP_INSTRUCTION("jz!", "= 0");
    DEFINE_CONDITIONAL_JUMP_INSTRUCTION("jnz!", "!= 0");
    DEFINE_CONDITIONAL_JUMP_INSTRUCTION("jgz!", "\\> 0");
    DEFINE_CONDITIONAL_JUMP_INSTRUCTION("jgez!", "\\>= 0");
    DEFINE_CONDITIONAL_JUMP_INSTRUCTION("jlz!", "\\< 0");
    DEFINE_CONDITIONAL_JUMP_INSTRUCTION("jlez!", "\\<= 0");

    g_InstructionsInitialized = true;
    return g_Instructions;
}

const PulsarLSP::Completion::KeywordList& PulsarLSP::Completion::GetKeywords()
{
    return g_Keywords;
}
