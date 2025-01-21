/*
Copyright (c) 2025 [Marco4413](https://github.com/Marco4413)

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "pulsar-tools/argue.h"

#include <charconv> // int64_t std::from_chars

void Argue::TextBuilder::PutText(std::string_view text)
{
    std::string_view::size_type newLineIdx;
    while ((newLineIdx = text.find("\n")) != std::string_view::npos) {
        PutLine(text.substr(0, newLineIdx));
        text.remove_prefix(newLineIdx+1);
        NewLine();
    }
    PutLine(text);
}

void Argue::TextBuilder::NewLine()
{
    if (m_CurrentLine.length() > 0) {
        m_Text += '\n';
        m_Text += m_CurrentLine;

        m_CurrentLineIndentLength = 0;
        m_CurrentLine.clear();
    }
}

void Argue::TextBuilder::Spacer()
{
    NewLine();
    if (!m_Text.ends_with('\n'))
        m_Text += '\n';
}

void Argue::TextBuilder::Indent()
{
    ++m_IndentationLevel;
}

void Argue::TextBuilder::DeIndent()
{
    if (m_IndentationLevel > 0)
        --m_IndentationLevel;
}

std::string Argue::TextBuilder::Build()
{
    std::string result = m_Text;
    result += '\n';
    result += m_CurrentLine;
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n'))
        result.pop_back();
    result += '\n';
    return result;
}

void Argue::TextBuilder::PutLine(std::string_view text)
{
    while (true) {
        size_t currentLineWidth = m_CurrentLine.length() - m_CurrentLineIndentLength;
        bool wrap = currentLineWidth >= m_MaxParagraphWidth;
        if (wrap) NewLine();

        if (m_CurrentLine.empty()) {
            m_CurrentLineIndentLength = m_IndentationLevel * m_Indent.length();
            if (wrap && m_IndentOnWrap) {
                m_CurrentLineIndentLength += m_Indent.length();
                m_CurrentLine += m_Indent;
            }

            for (size_t i = 0; i < m_IndentationLevel; ++i)
                m_CurrentLine += m_Indent;
        }

        std::string_view::size_type nextWordIdx = text.find(' ');
        if (nextWordIdx == std::string_view::npos) {
            m_CurrentLine += text;
            break;
        }

        m_CurrentLine += text.substr(0, nextWordIdx);
        m_CurrentLine += ' ';
        text.remove_prefix(nextWordIdx+1);
    }
}

Argue::IOption::IOption(
        IArgParser& parser,
        std::string_view name,
        std::string_view shortName,
        std::string_view metaVar,
        std::string_view description) :
    m_Parser(parser),
    m_Name(name),
    m_ShortName(shortName),
    m_MetaVar(metaVar),
    m_Description(description)
{
    m_Parser.AddOption(*this);
}

void Argue::IOption::WriteHint(ITextBuilder& hint) const
{
    if (HasMetaVar()) {
        const char VAR_OPEN  = IsVarOptional() ? '[' : '<';
        const char VAR_CLOSE = IsVarOptional() ? ']' : '>';

        if (m_Parser.HasShortPrefix() && HasShortName()) {
            hint.PutText(s(
                m_Parser.GetPrefix(), GetName(), '=', VAR_OPEN, GetMetaVar(), VAR_CLOSE, ", ",
                m_Parser.GetShortPrefix(), GetShortName(), VAR_OPEN, GetMetaVar(), VAR_CLOSE
            ));
        } else {
            hint.PutText(s(
                m_Parser.GetPrefix(), GetName(), '=', VAR_OPEN, GetMetaVar(), VAR_CLOSE
            ));
        }
    } else {
        if (m_Parser.HasShortPrefix() && HasShortName()) {
            hint.PutText(s(
                m_Parser.GetPrefix(), GetName(), ", ",
                m_Parser.GetShortPrefix(), GetShortName()
            ));
        } else {
            hint.PutText(s(
                m_Parser.GetPrefix(), GetName()
            ));
        }
    }
}

void Argue::IOption::WriteHelp(ITextBuilder& help) const
{
    WriteHint(help);
    if (HasDescription()) {
        help.NewLine();
        help.Indent();
        help.PutText(GetDescription());
        help.DeIndent();
    }
}

bool Argue::IOption::Parse(std::string_view arg, bool isShort)
{
    if (!ConsumeName(arg, isShort))
        return false;

    if (!isShort && HasMetaVar()) {
        const char VAR_OPEN  = IsVarOptional() ? '[' : '<';
        const char VAR_CLOSE = IsVarOptional() ? ']' : '>';

        if (!arg.starts_with('='))
            return SetError(s("Expected '=", VAR_OPEN, GetMetaVar(), VAR_CLOSE, "' for '", m_Parser.GetPrefix(), GetName(), "', got '", arg, "'."));
        arg.remove_prefix(1);
    }

    return ParseValue(arg);
}

bool Argue::IOption::SetError(std::string&& errorMessage)
{
    return m_Parser.SetError(std::forward<std::string>(errorMessage));
}

bool Argue::IOption::ConsumeName(std::string_view& arg, bool isShort)
{
    if (isShort) {
        if (!HasShortName() || !arg.starts_with(GetShortName()))
            return false;
        arg.remove_prefix(GetShortName().length());
        return true;
    }

    if (!arg.starts_with(GetName()))
        return false;
    arg.remove_prefix(GetName().length());
    return true;
}

Argue::IPositionalArgument::IPositionalArgument(IArgParser& parser, std::string_view description, std::string_view metaVar) :
    m_Parser(parser),
    m_MetaVar(metaVar),
    m_Description(description)
{
    m_Parser.AddArgument(*this);
}

void Argue::IPositionalArgument::WriteHint(ITextBuilder& hint) const
{
    if (HasDefault()) {
        if (IsVariadic()) {
            hint.PutText(s("[...", GetMetaVar(), ']'));
        } else{
            hint.PutText(s('[', GetMetaVar(), ']'));
        }
    } else {
        hint.PutText(s('<', GetMetaVar(), '>'));
    }
}

void Argue::IPositionalArgument::WriteHelp(ITextBuilder& help) const
{
    if (IsVariadic()) {
        help.PutText(s("...", GetMetaVar(), ':'));
    } else {
        help.PutText(s(GetMetaVar(), ':'));
    }
    if (HasDescription()) {
        help.NewLine();
        help.Indent();
        help.PutText(GetDescription());
        help.DeIndent();
    }
}

bool Argue::IPositionalArgument::SetError(std::string&& errorMessage)
{
    return m_Parser.SetError(std::forward<std::string>(errorMessage));
}

// std::string operator+(const char* cStr, std::string_view strView)
// {
//     std::string s = cStr;
//     return s += strView;
// }

void Argue::IArgParser::WriteHint(ITextBuilder& hint) const
{
    if (m_Commands.size() > 0) {
        std::string subcommands = m_Commands[0]->GetName();
        for (size_t i = 1; i < m_Commands.size(); ++i) {
            subcommands += '|';
            subcommands += m_Commands[i]->GetName();
        }

        hint.PutText(GetName());
        if (m_Options.size() > 0) {
            hint.PutText(" [...OPTIONS]");
        }
        hint.PutText(s(" [", subcommands, " ...]"));
    } else {
        hint.PutText(GetName());
        if (m_Options.size() > 0) {
            hint.PutText(" [...OPTIONS]");
        }
    }
    if (m_Arguments.size() > 0) {
        hint.PutText(" [--]");
        for (const IPositionalArgument* arg : m_Arguments) {
            hint.PutText(" ");
            arg->WriteHint(hint);
        }
    }
}

void Argue::IArgParser::WriteHelp(ITextBuilder& help, bool briefOptions, bool briefSubcommands) const
{
    WriteHint(help);
    help.Spacer();

    if (HasDescription()) {
        help.Indent();
        help.PutText(GetDescription());
        help.DeIndent();
        help.Spacer();
    }

    for (const IPositionalArgument* arg : m_Arguments) {
        if (arg->HasDescription()) {
            arg->WriteHelp(help);
            help.Spacer();
        }
    }

    if (m_Options.size() > 0) {
        help.PutText("OPTIONS:");
        help.NewLine();
        if (briefOptions) {
            for (const IOption* opt : m_Options) {
                help.Indent();
                opt->WriteHint(help);
                help.DeIndent();
                help.NewLine();
            }
            help.Spacer();
        } else {
            for (const IOption* opt : m_Options) {
                help.Indent();
                opt->WriteHelp(help);
                help.DeIndent();
                help.Spacer();
            }
        }
    }

    if (m_Commands.size() > 0) {
        help.PutText("SUBCOMMANDS:");
        help.NewLine();
        if (briefSubcommands) {
            for (const IArgParser* cmd : m_Commands) {
                help.Indent();
                cmd->WriteHint(help);
                help.DeIndent();
                help.NewLine();
            }
            help.Spacer();
        } else {
            for (const IArgParser* cmd : m_Commands) {
                help.Indent();
                cmd->WriteHelp(help, briefOptions, briefSubcommands);
                help.DeIndent();
                help.Spacer();
            }
        }
    }
}

bool Argue::IArgParser::Parse(std::stack<std::string_view> args)
{
    if (args.top() != GetName())
        return false;
    args.pop();

    const bool arePrefixesTheSame = GetShortPrefix() == GetPrefix();

    m_WasUsed = true;
    bool isParsingPositionals = false;
    size_t positionalIdx = 0;
    for (; !args.empty(); args.pop()) {
        std::string_view fullArg = args.top();
        std::string_view arg = fullArg;

        bool isShortPrefix = false;
        if (isParsingPositionals) {
            if (positionalIdx >= m_Arguments.size())
                return SetError(s("Unexpected positional argument '", fullArg, "'."));
            IPositionalArgument* positional = m_Arguments[positionalIdx];
            if (!positional->Parse(arg))
                return false;
            if (!positional->IsVariadic())
                ++positionalIdx;
            continue;
        } else if (arg == "--") {
            isParsingPositionals = true;
            continue;
        } else if (arg.starts_with(GetPrefix())) {
            arg.remove_prefix(GetPrefix().length());
            isShortPrefix = false;
        } else if (HasShortPrefix() && arg.starts_with(GetShortPrefix())) {
            arg.remove_prefix(GetShortPrefix().length());
            isShortPrefix = true;
        } else {
            // Try Parse Commands
            for (IArgParser* cmd : m_Commands) {
                if (cmd->Parse(args))
                    return true;
                if (cmd->HasError())
                    return false;
            }

            isParsingPositionals = true;
            if (positionalIdx >= m_Arguments.size())
                return SetError(s("Unexpected positional argument '", fullArg, "'."));
            IPositionalArgument* positional = m_Arguments[positionalIdx];
            if (!positional->Parse(arg))
                return false;
            if (!positional->IsVariadic())
                ++positionalIdx;
            continue;
        }

        bool hasParsedOption = false;
        for (IOption* opt : m_Options) {
            if (opt->Parse(arg, isShortPrefix)) {
                hasParsedOption = true;
                break;
            }

            if (HasError())
                return false;

            if (arePrefixesTheSame) {
                if (opt->Parse(arg, !isShortPrefix)) {
                    hasParsedOption = true;
                    break;
                }

                if (HasError())
                    return false;
            }
        }

        if (!hasParsedOption) {
            return SetError(s("Unknown option '", fullArg, "'."));
        }
    }

    for (const IOption* opt : m_Options) {
        if (!opt->HasValue())
            return SetError(s("Missing option '", GetPrefix(), opt->GetName(), "'."));
    }

    for (const IPositionalArgument* arg : m_Arguments) {
        if (!arg->HasValue())
            return SetError(s("Missing argument '", arg->GetMetaVar(), "'."));
    }

    return !HasError();
}

void Argue::FlagOption::WriteHint(ITextBuilder& hint) const
{
    const IArgParser& parser = GetParser();
    if (parser.HasShortPrefix() && HasShortName()) {
        hint.PutText(s(parser.GetPrefix(), GetName(), ", ", parser.GetShortPrefix(), GetShortName()));
    } else {
        hint.PutText(s(parser.GetPrefix(), GetName()));
    }
}

void Argue::FlagOption::WriteHelp(ITextBuilder& help) const
{
    const IArgParser& parser = GetParser();

    WriteHint(help);
    help.PutText(", ");
    if (parser.HasShortPrefix() && HasShortName())
        help.NewLine();
    help.PutText(s(GetParser().GetPrefix(), "no-", GetName()));

    if (HasDescription()) {
        help.NewLine();
        help.Indent();
        help.PutText(GetDescription());
        help.DeIndent();
    }
}

bool Argue::FlagOption::Parse(std::string_view arg, bool isShort)
{
    if (ConsumeName(arg, isShort)) {
        if (!arg.empty())
            return false;
        SetValue(true);
        return true;
    }
    
    if (isShort)
        return false;

    if (arg.starts_with("no-")) {
        arg.remove_prefix(3);
        if (arg != GetName())
            return false;
        SetValue(false);
        return true;
    }

    return false;
}

bool Argue::IntOption::ParseValue(std::string_view val)
{
    int64_t value;
    auto result = std::from_chars(&val.front(), &val.back()+1, value, 10);
    if (result.ptr != &val.back()+1)
        return SetError(s("Expected integer for '", GetParser().GetPrefix(), GetName(), "', got '", val, "'."));

    m_Parsed = true;
    m_Value = value;

    return true;
}

bool Argue::StrOption::ParseValue(std::string_view val)
{
    m_Parsed = true;
    m_Value = val;
    return true;
}

void Argue::ChoiceOption::WriteHint(ITextBuilder& hint) const
{
    const IArgParser& parser = GetParser();
    if (parser.HasShortPrefix() && HasShortName()) {
        hint.PutText(s(
            parser.GetPrefix(), GetName(), '=', GetChoiceString(), ", ",
            parser.GetShortPrefix(), GetShortName(), GetChoiceString()
        ));
    } else {
        hint.PutText(s(
            parser.GetPrefix(), GetName(), '=', GetChoiceString()
        ));
    }
}

bool Argue::ChoiceOption::ParseValue(std::string_view val)
{
    for (size_t i = 0; i < m_Choices.size(); ++i) {
        if (m_Choices[i] == val) {
            m_Parsed = true;
            m_ValueIdx = i;
            return true;
        }
    }

    return SetError(s("Expected one of ", GetChoiceString(), " for '", GetParser().GetPrefix(), GetName(), "', got '", val, "'."));
}

std::string Argue::ChoiceOption::GetChoiceString() const
{
    std::string result = "{";
    for (const auto& choice : m_Choices) {
        result += choice;
        result += ',';
    }
    if (result.back() == ',')
        result.back() = '}';
    else result += '}';
    return result;
}

bool Argue::CollectionOption::ParseValue(std::string_view val)
{
    if (!m_AcceptEmptyValues && val.empty()) {
        return SetError(s(
            "Empty values are not allowed for '", GetParser().GetPrefix(), GetName(), "'."
        ));
    }
    m_Value.emplace_back(val);
    return true;
}

bool Argue::StrArgument::Parse(std::string_view arg)
{
    m_Parsed = true;
    m_Value = arg;
    return true;
}

bool Argue::StrVarArgument::Parse(std::string_view arg)
{
    m_Value.emplace_back(arg);
    return true;
}

void Argue::HelpCommand::operator()(ITextBuilder& help) const
{
    std::string pathUntilLast;
    std::stack<std::string_view> helpPath;
    {
        const auto& rawPath = *m_HelpFor;
        for (size_t i = rawPath.size(); i > 0; --i)
            helpPath.emplace(rawPath[i-1]);
    }

    const IArgParser* currentSubcommand = &m_Parser;
    while (!helpPath.empty()) {
        std::string_view cmdToMatch = helpPath.top();
        bool hasFoundCommand = false;
        for (const IArgParser* sub : currentSubcommand->GetSubCommands()) {
            if (sub->GetName() == cmdToMatch) {
                hasFoundCommand = true;
                currentSubcommand = sub;
                break;
            }
        }

        if (!hasFoundCommand) {
            help.PutText(s("Could not find help for '", pathUntilLast, cmdToMatch, "'."));
            help.NewLine();
            return;
        }

        helpPath.pop();
        if (!helpPath.empty()) {
            pathUntilLast += cmdToMatch;
            pathUntilLast += " ";
        }
    }

    bool briefOptions = false;
    bool briefSubcommands = *m_PrintType == "brief";

    help.PutText(pathUntilLast);
    currentSubcommand->WriteHelp(help, briefOptions, briefSubcommands);
}
