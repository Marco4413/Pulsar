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

// TODO: Create a separate repo for this library

#ifndef _ARGUE_H
#define _ARGUE_H

#include <stack>
#include <string>
#include <string_view>
#include <utility> // std::forward
#include <vector>

#define ARGUE_DELETE_MOVE_COPY(className)            \
    className(const className&) = delete;            \
    className(className&&) = delete;                 \
    className& operator=(const className&) = delete; \
    className& operator=(className&&) = delete;      \

#define ARGUE_UNUSED(var) \
    ((void)var)

namespace Argue
{
    template<typename ...Args>
    std::string s(Args&& ...args)
    {
        std::string result;
        // See https://en.cppreference.com/w/cpp/language/fold
        (result += ... += std::forward<Args>(args));
        return result;
    }

    class ITextBuilder
    {
    public:
        virtual ~ITextBuilder() = default;

        virtual void PutText(std::string_view text) = 0;

        virtual void NewLine() = 0;
        virtual void Spacer() = 0;

        virtual void Indent() = 0;
        virtual void DeIndent() = 0;

        // Builds the final text resetting this builder instance
        virtual std::string Build() = 0;
    };

    class TextBuilder final :
        public ITextBuilder
    {
    public:
        TextBuilder(
                std::string_view indent="  ",
                bool indentOnWrap=true,
                size_t maxParagraphWidth=80) :
            m_Indent(indent),
            m_IndentOnWrap(indentOnWrap),
            m_MaxParagraphWidth(maxParagraphWidth)
        {}

        virtual ~TextBuilder() = default;

        void PutText(std::string_view text) override;

        void NewLine() override;
        void Spacer() override;

        void Indent() override;
        void DeIndent() override;

        std::string Build() override;
    
    protected:
        // Call this if text does not contain new lines
        void PutLine(std::string_view text);

    private:
        std::string m_Text = "";

        size_t m_IndentationLevel = 0;
        std::string m_CurrentLine = "";
        size_t m_CurrentLineIndentLength = 0;

        std::string m_Indent;
        bool m_IndentOnWrap;
        size_t m_MaxParagraphWidth;
    };

    class IArgParser; // Forward declaration

    // An option cannot be moved/copied and must live as long as its parser
    class IOption
    {
    public:
        IOption(IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view metaVar,
                std::string_view description);

        virtual ~IOption() = default;

        ARGUE_DELETE_MOVE_COPY(IOption)

        const std::string& GetName() const { return m_Name; }
        const std::string& GetShortName() const { return m_ShortName; }

        bool HasMetaVar() const { return !m_MetaVar.empty(); }
        const std::string& GetMetaVar() const { return m_MetaVar; }

        bool HasDescription() const { return !m_Description.empty(); }
        const std::string& GetDescription() const { return m_Description; }

        const IArgParser& GetParser() const { return m_Parser; }

        bool HasShortName() const { return !m_ShortName.empty(); }

        // See ::HasValue()
        operator bool() const { return HasValue(); };

    public:
        virtual bool IsVarOptional() const { return false; }
        virtual void WriteHint(ITextBuilder& hint) const;
        virtual void WriteHelp(ITextBuilder& help) const;

        // Returns true if parsing was successful.
        // Returning false does not mean that ::SetError was called.
        // If the parser has no error, the option was not recognized.
        virtual bool Parse(std::string_view arg, bool isShort);
        // true if GetValue() will return a valid value
        // i.e. ParseValue was successful or a default exists
        virtual bool HasValue() const = 0;

        // GetValue() method specific to the implementation
        // And a dereference alias to that method

    protected:
        // Implementing this method will allow to use the default behaviour of ::Parse()
        // i.e. --longName=VALUE, -shortVALUE
        virtual bool ParseValue(std::string_view val)
        {
            ARGUE_UNUSED(val);
            return SetError("Value parsing was not implemented.");
        }

        // Always returns false, this allows `return SetError(...)` in ::Parse functions.
        bool SetError(std::string&& errorMessage);

        // Default name parsing behaviour, use this within ::Parse()
        bool ConsumeName(std::string_view& arg, bool isShort);

    private:
        IArgParser& m_Parser;
        std::string m_Name;
        std::string m_ShortName;
        std::string m_MetaVar;
        std::string m_Description;
    };

    class IPositionalArgument
    {
    public:
        // `metaVar` must have a length > 0 not counting spaces
        IPositionalArgument(
                IArgParser& parser,
                std::string_view description,
                std::string_view metaVar);
        virtual ~IPositionalArgument() = default;

        ARGUE_DELETE_MOVE_COPY(IPositionalArgument)

        size_t HasDescription() const { return !m_Description.empty(); }
        const std::string& GetDescription() const { return m_Description; }

        bool HasMetaVar() const { return !m_MetaVar.empty(); }
        const std::string& GetMetaVar() const { return m_MetaVar; }

        const IArgParser& GetParser() const { return m_Parser; }

        // See ::HasValue()
        operator bool() const { return HasValue(); };

    public:
        virtual bool IsVariadic() const = 0;
        virtual bool HasDefault() const { return IsVariadic(); }

        virtual void WriteHint(ITextBuilder& hint) const;
        virtual void WriteHelp(ITextBuilder& help) const;

        // Returns true if parsing was successful.
        // Returns false if ::SetError() was called.
        // A positional argument must be present at its position
        virtual bool Parse(std::string_view arg) = 0;
        // true if GetValue() will return a valid value
        virtual bool HasValue() const = 0;

        // GetValue() method specific to the implementation
        // And a dereference alias to that method

    protected:
        // Always returns false, this allows `return SetError(...)` in ::Parse functions.
        bool SetError(std::string&& errorMessage);

    private:
        IArgParser& m_Parser;
        std::string m_MetaVar;
        std::string m_Description;
    };

    // A parser cannot be moved/copied and must live as long as its options/subparsers
    class IArgParser
    {
    public:
        // `command` for a root parser is the program itself
        IArgParser(std::string_view command, std::string_view description) :
            m_Name(command),
            m_Description(description)
        {}

        virtual ~IArgParser() = default;

        ARGUE_DELETE_MOVE_COPY(IArgParser)

        const std::string& GetName() const { return m_Name; }
        size_t HasDescription() const { return !m_Description.empty(); }
        const std::string& GetDescription() const { return m_Description; }

        // The returned pointers are not null unless something terrible happened
        const std::vector<IOption*>& GetOptions() const { return m_Options; }
        const std::vector<IArgParser*>& GetSubCommands() const { return m_Commands; }
        const std::vector<IPositionalArgument*>& GetArguments() const { return m_Arguments; }

        // Returns true if this command was used and there was no error.
        // Moreover, all direct children options to this command have a value.
        operator bool() const { return !HasError() && m_WasUsed; }

        // const std::vector<IPositionalArgument>& GetPositionalArguments() const { return m_PositionalArguments; }
        // const std::vector<IPositionalArgument>& operator*() const { return GetPositionalArguments(); }

        bool Parse(int argc, const char** argv)
        {
            std::stack<std::string_view> args;
            for (int i = argc-1; i >= 0; --i)
                args.emplace(argv[i]);
            return Parse(args);
        }

    public:
        virtual void WriteHint(ITextBuilder& hint) const;
        virtual void WriteHelp(ITextBuilder& help, bool briefOptions=false, bool briefSubcommands=true) const;

        // If this returns false, either there was an error or the command did not match.
        virtual bool Parse(std::stack<std::string_view> args);

        virtual const std::string& GetError() const = 0;
        // Always returns false, this allows `return SetError(...)` in ::Parse functions.
        virtual bool SetError(std::string&& errorMessage) = 0;
        virtual bool HasError() const = 0;

        virtual const std::string& GetPrefix() const = 0;
        virtual const std::string& GetShortPrefix() const = 0;
        virtual bool HasShortPrefix() const { return GetShortPrefix().length() > 0; }

    public: // The following methods are called by constructors
        void AddOption(IOption& opt)
        {
            m_Options.emplace_back(&opt);
        }

        void AddCommand(IArgParser& cmd)
        {
            m_Commands.emplace_back(&cmd);
        }

        void AddArgument(IPositionalArgument& arg)
        {
            m_Arguments.emplace_back(&arg);
        }

    private:
        std::string m_Name;
        std::string m_Description;

        std::vector<IOption*> m_Options;
        std::vector<IArgParser*> m_Commands;
        std::vector<IPositionalArgument*> m_Arguments;

        bool m_WasUsed = false;
    };

    class ArgParser final :
        public IArgParser
    {
    public:
        ArgParser(std::string_view program, std::string_view description) :
            ArgParser(program, description, "--", "-")
        {}

        ArgParser(
                std::string_view program,
                std::string_view description,
                std::string_view prefix,
                std::string_view shortPrefix) :
            IArgParser(program, description),
            m_Prefix(prefix),
            m_ShortPrefix(shortPrefix)
        {}

        ~ArgParser() = default;

        ARGUE_DELETE_MOVE_COPY(ArgParser)

    public:
        const std::string& GetError() const override { return m_ErrorMessage; }
        bool SetError(std::string&& errorMessage) override
        {
            m_ErrorMessage = std::forward<std::string>(errorMessage);
            return false;
        }
        bool HasError() const override { return !m_ErrorMessage.empty(); }

        const std::string& GetPrefix() const override { return m_Prefix; }
        const std::string& GetShortPrefix() const override { return m_ShortPrefix; }

    private:
        std::string m_Prefix;
        std::string m_ShortPrefix;
        std::string m_ErrorMessage;
    };

    class CommandParser final :
        public IArgParser
    {
    public:
        CommandParser(
                IArgParser& parent,
                std::string_view command,
                std::string_view description) :
            IArgParser(command, description),
            m_Parent(parent)
        {
            m_Parent.AddCommand(*this);
        }

        ~CommandParser() = default;

        ARGUE_DELETE_MOVE_COPY(CommandParser)

    public:
        const std::string& GetError() const override { return m_Parent.GetError(); }
        bool SetError(std::string&& errorMessage) override
        {
            m_Parent.SetError(std::forward<std::string>(errorMessage));
            return false;
        }
        bool HasError() const override { return m_Parent.HasError(); }

        const std::string& GetPrefix() const override { return m_Parent.GetPrefix(); }
        const std::string& GetShortPrefix() const override { return m_Parent.GetShortPrefix(); }

    private:
        IArgParser& m_Parent;
    };

    class FlagOption :
        public IOption
    {
    public:
        FlagOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view description,
                bool initValue=false) :
            IOption(parser, name, shortName, "", description)
        {
            SetValue(initValue);
        }

        virtual ~FlagOption() = default;

        ARGUE_DELETE_MOVE_COPY(FlagOption)
    
    public:
        bool IsVarOptional() const override { return true; }
        void WriteHelp(ITextBuilder& help) const override;

        bool Parse(std::string_view arg, bool isShort) override;
        bool HasValue() const override { return true; }

        bool operator*() const { return GetValue(); }
        bool GetValue() const  { return m_Value; }

    public:
        virtual void SetValue(bool flag) { m_Value = flag; }

    private:
        bool m_Value = false;
    };

    class FlagGroupOption final :
        public FlagOption
    {
    public:
        template<typename ...FlagGroup>
        FlagGroupOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view description,
                bool initValue=false,
                FlagGroup& ...flagGroup) :
            FlagOption(parser, name, shortName, description, initValue)
        {
            (m_Group.push_back(&static_cast<FlagOption&>(flagGroup)), ...);
            SetValue(m_Value);
        }

        virtual ~FlagGroupOption() = default;

        ARGUE_DELETE_MOVE_COPY(FlagGroupOption)

        const std::vector<FlagOption*>& GetGroup() const { return m_Group; }

    public:
        void SetValue(bool flag) override
        {
            m_Value = flag;
            for (auto opt : m_Group)
                opt->SetValue(flag);
        }

    private:
        bool m_Value = false;
        std::vector<FlagOption*> m_Group;
    };

    class IntOption final :
        public IOption
    {
    public:
        using IOption::IOption;

        IntOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view metaVar,
                std::string_view description,
                int64_t defaultValue) :
            IOption(parser, name, shortName, metaVar, description),
            m_HasDefault(true),
            m_Default(defaultValue)
        {}

        virtual ~IntOption() = default;

        ARGUE_DELETE_MOVE_COPY(IntOption)

    public:
        bool IsVarOptional() const override { return false; }
        bool HasValue() const override { return m_Parsed || m_HasDefault; }

        bool HasDefaultValue() const { return m_HasDefault; }
        int64_t GetDefaultValue() const { return m_Default; }

        int64_t operator*() const { return GetValue(); }
        int64_t GetValue() const
        {
            if (!m_Parsed && m_HasDefault)
                return m_Default;
            return m_Value;
        }

    protected:
        bool ParseValue(std::string_view val) override;

    private:
        bool m_Parsed = false;
        int64_t m_Value = 0;

        bool m_HasDefault = false;
        int64_t m_Default = 0;
    };

    class StrOption final :
        public IOption
    {
    public:
        using IOption::IOption;

        StrOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view metaVar,
                std::string_view description,
                std::string_view defaultValue) :
            IOption(parser, name, shortName, metaVar, description),
            m_HasDefault(true),
            m_Default(defaultValue)
        {}

        virtual ~StrOption() = default;

        ARGUE_DELETE_MOVE_COPY(StrOption)

    public:
        bool IsVarOptional() const override { return false; }
        bool HasValue() const override { return m_Parsed || m_HasDefault; }

        bool HasDefaultValue() const { return m_HasDefault; }
        const std::string& GetDefaultValue() const { return m_Default; }

        const std::string& operator*() const { return GetValue(); }
        const std::string& GetValue() const
        {
            if (!m_Parsed && m_HasDefault)
                return m_Default;
            return m_Value;
        }

    protected:
        bool ParseValue(std::string_view val) override;

    private:
        bool m_Parsed = false;
        std::string m_Value = "";

        bool m_HasDefault = false;
        std::string m_Default = "";
    };

    class ChoiceOption final :
        public IOption
    {
    public:
        ChoiceOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view metaVar,
                std::string_view description,
                std::initializer_list<std::string_view> choices) :
            IOption(parser, name, shortName, metaVar, description)
        {
            for (auto choice : choices)
                m_Choices.emplace_back(choice);
        }

        ChoiceOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view metaVar,
                std::string_view description,
                std::initializer_list<std::string_view> choices,
                size_t defaultChoiceIdx) :
            IOption(parser, name, shortName, metaVar, description),
            m_HasDefault(true)
        {
            for (auto choice : choices)
                m_Choices.emplace_back(choice);
            if (!m_Choices.empty()) {
                m_DefaultIdx = defaultChoiceIdx < m_Choices.size()
                    ? defaultChoiceIdx
                    : m_Choices.size()-1;
            }
        }

        virtual ~ChoiceOption() = default;

        ARGUE_DELETE_MOVE_COPY(ChoiceOption)

    public:
        void WriteHint(ITextBuilder& hint) const override;

        bool IsVarOptional() const override { return false; }
        bool HasValue() const override { return m_Parsed || m_HasDefault; }

        bool HasDefaultValue() const { return m_HasDefault; }
        std::string_view GetDefaultValue() const
        {
            if (m_Choices.empty() || !m_HasDefault)
                return "";
            return m_Choices[m_DefaultIdx];
        }

        std::string_view operator*() const { return GetValue(); }
        std::string_view GetValue() const
        {
            if (m_Choices.empty()) return "";
            if (!m_Parsed && m_HasDefault)
                return m_Choices[m_DefaultIdx];
            return m_Choices[m_ValueIdx];
        }

    protected:
        bool ParseValue(std::string_view val) override;

    private:
        std::string GetChoiceString() const;

    private:
        bool m_Parsed = false;
        size_t m_ValueIdx = 0;
        std::vector<std::string> m_Choices;

        bool m_HasDefault = false;
        size_t m_DefaultIdx = 0;
    };

    class CollectionOption final :
        public IOption
    {
    public:
        CollectionOption(
                IArgParser& parser,
                std::string_view name,
                std::string_view shortName,
                std::string_view metaVar,
                std::string_view description,
                bool acceptEmptyValues=false) :
            IOption(parser, name, shortName, metaVar, description),
            m_AcceptEmptyValues(acceptEmptyValues)
        {}

        virtual ~CollectionOption() = default;

        ARGUE_DELETE_MOVE_COPY(CollectionOption)

        bool AcceptsEmptyValues() const { return m_AcceptEmptyValues; }

    public:
        bool IsVarOptional() const override { return m_AcceptEmptyValues; }
        bool HasValue() const override { return true; }

        const std::vector<std::string>& operator*() const { return GetValue(); }
        const std::vector<std::string>& GetValue() const { return m_Value; }

    protected:
        bool ParseValue(std::string_view val) override;

    private:
        std::string m_MetaVar;
        std::vector<std::string> m_Value;

        bool m_AcceptEmptyValues = false;
    };

    class StrArgument final :
        public IPositionalArgument
    {
    public:
        using IPositionalArgument::IPositionalArgument;

        StrArgument(
                IArgParser& parser,
                std::string_view metaVar,
                std::string_view description,
                std::string_view defaultValue) :
            IPositionalArgument(parser, metaVar, description),
            m_HasDefault(true),
            m_Default(defaultValue)
        {}

        virtual ~StrArgument() = default;

        ARGUE_DELETE_MOVE_COPY(StrArgument)

    public:
        bool IsVariadic() const override { return false; }
        bool HasDefault() const override { return m_HasDefault; }

        bool Parse(std::string_view arg) override;
        bool HasValue() const override { return m_Parsed || m_HasDefault; }

        const std::string& GetDefaultValue() const { return m_Default; }

        const std::string& operator*() const { return GetValue(); }
        const std::string& GetValue() const
        {
            if (!m_Parsed && m_HasDefault)
                return m_Default;
            return m_Value;
        }

    private:
        bool m_Parsed = false;
        std::string m_Value;

        bool m_HasDefault = false;
        std::string m_Default = "";
    };

    class StrVarArgument final :
        public IPositionalArgument
    {
    public:
        using IPositionalArgument::IPositionalArgument;
        virtual ~StrVarArgument() = default;

        ARGUE_DELETE_MOVE_COPY(StrVarArgument)

    public:
        bool IsVariadic() const override { return true; }

        bool Parse(std::string_view arg) override;
        bool HasValue() const override { return true; }

        const std::vector<std::string>& operator*() const { return GetValue(); }
        const std::vector<std::string>& GetValue() const  { return m_Value; }

    private:
        std::vector<std::string> m_Value;
    };

    class HelpCommand
    {
    public:
        HelpCommand(IArgParser& parser) :
            m_Parser(parser),
            m_Command(m_Parser, "help", "Prints this help message."),
            m_PrintType(m_Command, "print", "P", "TYPE", "Print all subcommands and their options. (default: brief)", {"brief", "full"}, 0),
            m_HelpFor(m_Command, "The path to the command to print the help message for.", "CMD")
        {}

        operator bool() const { return m_Command; }
        void operator()(ITextBuilder& help) const;

    private:
        IArgParser& m_Parser;
        CommandParser m_Command;
        ChoiceOption m_PrintType;
        StrVarArgument m_HelpFor;
    };
}

#endif // _PULSARTOOLS_ARGPARSER_H
