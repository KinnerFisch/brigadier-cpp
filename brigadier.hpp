/*
 * MIT License
 *
 * Copyright (c) 2022 Tomasz Karpiñski
 * Copyright (c) 2026 KinnerFisch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <set>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <memory>
#include <map>
#include <future>
#include <tuple>
#include <algorithm>
#include <limits>
#include <optional>

// Following code makes that you don't have to specify command source type inside arguments.
// Command source type is automatically distributed from dispatcher.

#define REGISTER_ARGTYPE(type, name)                                      \
using name = type

#define REGISTER_ARGTYPE_TEMPL(type, name)                                \
template<typename... Args>                                                \
using name = type<Args...>

#define REGISTER_ARGTYPE_SPEC(type, name, ...)                            \
using name = type<__VA_ARGS__>

#define REGISTER_ARGTYPE_SPEC_TEMPL(type, name, ...)                      \
template<typename... Args>                                                \
using name = type<__VA_ARGS__, Args...>

#ifdef __has_include
#  if __has_include("nameof.hpp")
#    include "nameof.hpp"
#    define HAS_NAMEOF
#  endif
#  if __has_include("magic_enum.hpp")
#    include "magic_enum.hpp"
#    define HAS_MAGICENUM
#  endif
#endif

namespace brigadier {
	template<typename S>
	class CommandNode;
	template<typename S>
	class CommandContext;

	template<typename S>
	class IArgumentCommandNode;
	template<typename S>
	class LiteralCommandNode;
	template<typename S>
	class RootCommandNode;
	template<typename S>
	class CommandDispatcher;

	template<typename S, typename T, typename node_type>
	class ArgumentBuilder;
	template<typename S>
	class MultiArgumentBuilder;
	template<typename S>
	class LiteralArgumentBuilder;
	template<typename S, typename T>
	class RequiredArgumentBuilder;

	class StringReader {
		static constexpr wchar_t SYNTAX_ESCAPE = L'\\';
		static constexpr wchar_t SYNTAX_SINGLE_QUOTE = L'\'';
		static constexpr wchar_t SYNTAX_DOUBLE_QUOTE = L'"';

	public:
		StringReader(std::wstring wstring);

		StringReader() = default;

		std::wstring GetString() const;
		void SetCursor(int cursor);
		int GetRemainingLength() const;
		int GetTotalLength() const;
		int GetCursor() const;
		std::wstring GetRead() const;
		std::wstring GetRemaining() const;
		bool CanRead(int length) const;
		bool CanRead() const;
		wchar_t Peek() const;
		wchar_t Peek(int offset) const;
		wchar_t Read();
		void Skip();

		static bool IsQuotedStringStart(wchar_t c);

		void SkipWhitespace();

		template<typename T>
		T ReadValue();

		static bool IsAllowedInUnquotedString(wchar_t c);

		template<bool allow_float = true, bool allow_negative = true>
		static bool IsAllowedNumber(const wchar_t c) { return IsAllowedNumberImpl(c, allow_float, allow_negative); }

		// defined in brigadier.cpp
		std::wstring ReadUnquotedString();

		std::wstring ReadQuotedString();

		std::wstring ReadStringUntil(wchar_t terminator);

		std::wstring ReadString();

		void Expect(wchar_t c);

	private:
		template<typename S> friend class CommandNode;
		template<typename S> friend class LiteralCommandNode;
		static bool IsAllowedNumberImpl(wchar_t c, bool allow_float, bool allow_negative);
		std::wstring string;
		int cursor = 0;
	};

	class BuiltInExceptionProvider;

	struct ExceptionContext {
		ExceptionContext (std::nullptr_t) {}

		ExceptionContext() = default;

		ExceptionContext(StringReader const &reader);

		std::wstring input;
		int cursor = -1;
	};

	class CommandSyntaxException {
	public:
		static constexpr int context_amount = 10;
		using BuiltInExceptions = BuiltInExceptionProvider;

		template<typename... Args>
		CommandSyntaxException(ExceptionContext ctx, Args &&... args) : ctx(ctx), msg(std::move(CreateMessageApplyContext(ctx, args...))) {}

		std::wstring const &What() const;

	private:
		template<typename T>
		static void Add(std::wostringstream &stream, T &&value) { stream << value; }

		template<typename T, typename... Args>
		static void Add(std::wostringstream &stream, T &&value, Args &&... args) {
			stream << value;
			Add(stream, args...);
		}

		template<typename... Args>
		static std::wstring CreateMessage(Args &&... args) {
			std::wostringstream s;
			Add(s, args...);
			return s.str();
		}

		template<typename... Args>
		static std::wstring CreateMessageApplyContext(ExceptionContext ctx, Args &&... args) {
			if (ctx.cursor < 0) return CreateMessage(args...);
			return CreateMessage(args..., L" at position ", ctx.cursor, L": ", ctx.cursor > context_amount ? L"..." : L"",
				ctx.input.substr((std::max)(0, ctx.cursor - context_amount), context_amount), L"<--[HERE]");
		}

	public:
		int GetCursor() const;

		std::wstring GetInput() const;

	private:
		ExceptionContext ctx;
		std::wstring msg;
	};

	class BuiltInExceptionProvider {
	public:
		template<typename T0, typename T1>
		static CommandSyntaxException ValueTooLow(ExceptionContext ctx, T0 found, T1 min) {
			return CommandSyntaxException(ctx, L"Value must not be less than ", min, L", found ", found);
		}

		template<typename T0, typename T1>
		static CommandSyntaxException ValueTooHigh(ExceptionContext ctx, T0 found, T1 max) {
			return CommandSyntaxException(ctx, L"Value must not be more than ", max, L", found ", found);
		}

		template<typename T0>
		static CommandSyntaxException LiteralIncorrect(ExceptionContext ctx, T0 const &expected) {
			return CommandSyntaxException(ctx, L"Expected literal ", expected);
		}

		static CommandSyntaxException ReaderExpectedStartOfQuote(const ExceptionContext &ctx);

		static CommandSyntaxException ReaderExpectedEndOfQuote(const ExceptionContext &ctx);

		template<typename T0>
		static CommandSyntaxException ReaderInvalidEscape(ExceptionContext ctx, T0 const &character) {
			return CommandSyntaxException(ctx, L"Invalid escape sequence '", character, L"' in quoted string");
		}

		template<typename T0>
		static CommandSyntaxException ReaderInvalidValue(ExceptionContext ctx, T0 const &value) {
			return CommandSyntaxException(ctx, L"Invalid value '", value, L"'");
		}

		static CommandSyntaxException ReaderExpectedValue(const ExceptionContext &ctx);

		template<typename T0>
		static CommandSyntaxException ReaderExpectedSymbol(ExceptionContext ctx, T0 const &symbol) {
			return CommandSyntaxException(ctx, L"Expected '", symbol, L"'");
		}

		template<typename T0>
		static CommandSyntaxException ReaderExpectedOneOf(ExceptionContext ctx, T0 const &symbols) {
			return CommandSyntaxException(ctx, L"Expected one of `", symbols, L"`");
		}

		static CommandSyntaxException DispatcherUnknownCommand(const ExceptionContext &ctx);

		static CommandSyntaxException DispatcherUnknownArgument(const ExceptionContext &ctx);

		static CommandSyntaxException DispatcherExpectedArgumentSeparator(const ExceptionContext &ctx);

		template<typename T0>
		static CommandSyntaxException DispatcherParseException(ExceptionContext ctx, T0 const &message) {
			return CommandSyntaxException(ctx, L"Could not parse command: ", message);
		}
	};

	template<typename T>
	T StringReader::ReadValue() {
		const int start = cursor;
		std::wstring value;
		if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
			while (CanRead() && IsAllowedNumber<std::is_floating_point_v<T>, std::is_signed_v<T> >(Peek())) Skip();
			value = string.substr(start, cursor - start);
		} else value = ReadString();

		if (value.empty()) throw CommandSyntaxException::BuiltInExceptions::ReaderExpectedValue(*this);

		if constexpr (std::is_same_v<T, bool>) {
			if (value == L"true") return true;
			if (value == L"false") return false;
			cursor = start;
			throw CommandSyntaxException::BuiltInExceptions::ReaderInvalidValue(*this, value);
		} else {
			T ret{};
			std::wistringstream s(value);
			s >> ret;

			if (s.eof() && !s.bad() && !s.fail()) return ret;
			cursor = start;
			throw CommandSyntaxException::BuiltInExceptions::ReaderInvalidValue(*this, value);
		}
	}

	class StringRange {
	public:
		StringRange(const int start, const int end) : start(start), end(end) {}

		int GetStart() const { return start; }
		int GetEnd() const { return end; }

		static StringRange At(const int pos) { return { pos, pos }; }
		static StringRange Between(const int start, const int end) { return { start, end }; }

		static StringRange Encompassing(StringRange const &a, StringRange const &b) {
			return { (std::min)(a.GetStart(), b.GetStart()), (std::max)(a.GetEnd(), b.GetEnd()) };
		}

		// defined in brigadier.cpp
		std::wstring Get(const StringReader& reader) const;

		std::wstring Get(const std::wstring& string) const;

		bool IsEmpty() const { return start == end; }
		int GetLength() const { return end - start; }

		bool operator==(StringRange const &other) const { return (start == other.start && end == other.end); }

	private:
		int start = 0;
		int end = 0;
	};

	class Suggestions;
	class SuggestionsBuilder;

	class Suggestion {
	public:
		Suggestion(StringRange range, std::wstring text, std::wstring tooltip);

		Suggestion(StringRange range, std::wstring text);

		StringRange GetRange() const { return range; }
		std::wstring const &GetText() const { return text; }
		std::wstring GetTooltip() const { return tooltip; }

		// defined in brigadier.cpp
		std::wstring Apply(const std::wstring& input) const;

		void Expand(const std::wstring& command, StringRange range);

		friend class Suggestions;
		friend class SuggestionsBuilder;

		Suggestion(std::wstring text, StringRange range, std::wstring tooltip);

		Suggestion(std::wstring text, StringRange range);

	private:
		StringRange range;
		std::wstring text;
		std::wstring tooltip;
	};

	struct CompareNoCase {
		bool operator()(Suggestion const &a, Suggestion const &b) const;
	};

	class Suggestions {
	public:
		Suggestions(StringRange range, std::set<Suggestion, CompareNoCase> suggestions);

		Suggestions();

		StringRange GetRange() const { return range; }
		std::set<Suggestion, CompareNoCase> const &GetList() const { return suggestions; }
		bool IsEmpty() const { return suggestions.empty(); }

		// defined in brigadier.cpp
		static std::future<Suggestions> Empty();

		static Suggestions Merge(const std::wstring &command, std::vector<Suggestions> const &input, const bool *cancel = nullptr);

		static Suggestions Create(const std::wstring &command, std::vector<Suggestion> &suggestions, const bool *cancel = nullptr);

	private:
		StringRange range;
		std::set<Suggestion, CompareNoCase> suggestions;
	};

	class SuggestionsBuilder {
	public:
		SuggestionsBuilder(const std::wstring& input, const std::wstring &inputLowerCase, int start, bool *cancel = nullptr);

		int GetStart() const { return start; }
		std::wstring GetInput() const { return input; }
		std::wstring GetInputLowerCase() const { return inputLowerCase; }
		std::wstring GetRemaining() const { return remaining; }
		std::wstring GetRemainingLowerCase() const { return remainingLowerCase; }

		// defined in brigadier.cpp
		Suggestions Build(bool *cancel = nullptr);

		std::future<Suggestions> BuildFuture();

		SuggestionsBuilder &Suggest(const std::wstring& text);

		SuggestionsBuilder &Suggest(const std::wstring& text, const std::wstring& tooltip);

		int AutoSuggest(const std::wstring& text, const std::wstring& input);

		int AutoSuggest(const std::wstring& text, const std::wstring& tooltip, const std::wstring& input);

		SuggestionsBuilder &Add(SuggestionsBuilder const &other);

		void SetOffset(int start);

		void Restart();

		template<typename T>
		SuggestionsBuilder &Suggest(T value) {
			result.emplace_back(std::move(std::to_wstring(value)), StringRange::Between(start, static_cast<int>(input.length())));
			return *this;
		}

		template<typename T>
		SuggestionsBuilder &Suggest(T value, std::wstring tooltip) {
			result.emplace_back(std::move(std::to_wstring(value)), StringRange::Between(start, static_cast<int>(input.length())), tooltip);
			return *this;
		}

		template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
		int AutoSuggest(T value, const std::wstring& input) {
			if (std::wstring val = std::to_wstring(value); val.compare(0, (std::min)(input.length(), val.length()), input, 0, (std::min)(input.length(), val.length())) == 0) {
				result.emplace_back(std::move(val), StringRange::Between(start, static_cast<int>(input.length())));
				return 1;
			}
			return 0;
		}

		template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
		int AutoSuggest(T value, const std::wstring& tooltip, const std::wstring& input) {
			if (std::wstring val = std::to_wstring(value); val.compare(0, (std::min)(input.length(), val.length()), input, 0, (std::min)(input.length(), val.length())) == 0) {
				result.emplace_back(std::move(val), StringRange::Between(start, static_cast<int>(input.length())), tooltip);
				return 1;
			}
			return 0;
		}

		template<typename Container>
		int AutoSuggest(Container const &init) {
			int counter = 0;
			for (auto &val: init) counter += AutoSuggest(val, GetRemaining());
			return counter;
		}

		template<typename Container>
		int AutoSuggestLowerCase(Container const &init) {
			int counter = 0;
			for (auto &val: init) counter += AutoSuggest(val, GetRemainingLowerCase());
			return counter;
		}

		~SuggestionsBuilder() = default;

	private:
		int start = 0;
		std::wstring input;
		std::wstring inputLowerCase;
		std::wstring remaining;
		std::wstring remainingLowerCase;
		std::vector<Suggestion> result;
		bool *cancel = nullptr;
	};

	template<typename... Ts>
	using Predicate = bool(*)(Ts &&... args);
	template<typename S>
	using AmbiguityConsumer = void(*)(CommandNode<S> *parent, CommandNode<S> *child, CommandNode<S> *sibling, std::set<std::wstring> &inputs);
	template<typename S>
	using Command = int(*)(CommandContext<S> &context);
	template<typename S>
	using RedirectModifier = std::vector<S>(*)(CommandContext<S> &context);
	template<typename S>
	using SingleRedirectModifier = S(*)(CommandContext<S> &context);
	template<typename S>
	using ResultConsumer = void(*)(CommandContext<S> &context, bool success, int result);
	template<typename S>
	using SuggestionProvider = std::future<Suggestions>(*)(CommandContext<S> &context, SuggestionsBuilder &builder);

	enum class CommandNodeType {
		RootCommandNode,
		LiteralCommandNode,
		ArgumentCommandNode
	};

	template<typename S>
	class CommandNode {
	public:
		CommandNode(Command<S> command, Predicate<S &> requirement, std::shared_ptr<CommandNode> redirect, RedirectModifier<S> modifier, const bool forks)
			: command(std::move(command)), requirement(std::move(requirement)), redirect(std::move(redirect)), modifier(std::move(modifier)), forks(forks) {}

		CommandNode() = default;

		virtual ~CommandNode() = default;

		Command<S> GetCommand() const { return command; }

		std::map<std::wstring, std::shared_ptr<CommandNode<S> >, std::less<> > const &GetChildren() const { return children; }

		std::shared_ptr<CommandNode<S> > GetChild(std::wstring name) const {
			auto found = children.find(name);
			if (found != children.end()) return found->second;
			return nullptr;
		}

		std::shared_ptr<CommandNode> GetRedirect() const { return redirect; }

		RedirectModifier<S> GetRedirectModifier() const { return modifier; }

		Predicate<S &> GetRequirement() { return requirement; }

		bool IsFork() const { return forks; }

		bool CanUse(S &source) {
			if (requirement) return requirement(source);
			return true;
		}

		void AddChild(std::shared_ptr<CommandNode> node) {
			if (node == nullptr) return;

			if (node->GetNodeType() == CommandNodeType::RootCommandNode)
				throw std::runtime_error("Cannot add a RootCommandNode as a child to any other CommandNode");

			auto child = children.find(node->GetName());
			if (child != children.end()) {
				// We've found something to merge onto
				auto child_node = child->second;

				if (child_node->GetNodeType() != node->GetNodeType()) throw std::runtime_error("Node type (literal/argument) mismatch!");

				auto node_command = node->GetCommand();
				if (node_command != nullptr) child_node->command = node_command;
				for (auto &[name, grandchild]: node->GetChildren()) child_node->AddChild(grandchild);
			} else {
				children.emplace(node->GetName(), node);
				if (node->GetNodeType() == CommandNodeType::LiteralCommandNode)
					literals.emplace_back(std::move(std::static_pointer_cast<LiteralCommandNode<S> >(std::move(node))));
				else if (node->GetNodeType() == CommandNodeType::ArgumentCommandNode)
					arguments.emplace_back(std::move(std::static_pointer_cast<IArgumentCommandNode<S> >(std::move(node))));
			}
		}

		void FindAmbiguities(AmbiguityConsumer<S> consumer) {
			for (auto const &[child_name, child]: children) {
				for (auto const &[sibling_name, sibling]: children) {
					if (child == sibling) continue;

					std::set<std::wstring> matches;

					for (auto input: child->GetExamples()) if (sibling->IsValidInput(input)) matches.insert(input);

					if (!matches.empty()) consumer(this, child, sibling, matches);
				}

				child.FindAmbiguities(consumer);
			}
		}

		std::tuple<std::shared_ptr<CommandNode> *, size_t> GetRelevantNodes(StringReader &input) {
			if (!literals.empty()) {
				const int cursor = input.GetCursor();
				while (input.CanRead() && input.Peek() != L' ') input.Skip();
			std::wstring text = input.string.substr(cursor, input.GetCursor() - cursor);
				input.SetCursor(cursor);
				auto literal = children.find(text);
				if (literal != children.end() && literal->second->GetNodeType() ==
				    CommandNodeType::LiteralCommandNode) {
					return std::tuple<std::shared_ptr<CommandNode> *, size_t>(&literal->second, 1);
				}
				return std::tuple<std::shared_ptr<CommandNode> *, size_t>(reinterpret_cast<std::shared_ptr<CommandNode> *>(arguments.data()), arguments.size());
			}
			return std::tuple<std::shared_ptr<CommandNode> *, size_t>(reinterpret_cast<std::shared_ptr<CommandNode> *>(arguments.data()), arguments.size());
		}

		bool HasCommand() {
			if (GetCommand() != nullptr) return true;
			for (auto const &[name, child]: children) if (child && child->HasCommand()) return true;
			return false;
		}

		virtual std::wstring const &GetName() = 0;

		virtual std::wstring GetUsageText() = 0;

		virtual std::vector<std::wstring> GetExamples() = 0;

		virtual void Parse(StringReader &reader, CommandContext<S> &contextBuilder) = 0;

		virtual std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) = 0;

		virtual CommandNodeType GetNodeType() = 0;

	protected:
		template<typename _S, typename T, typename node_type>
		friend class ArgumentBuilder;
		template<typename _S>
		friend class MultiArgumentBuilder;
		template<typename _S>
		friend class CommandDispatcher;
		template<typename _S, typename T>
		friend class RequiredArgumentBuilder;
		template<typename _S>
		friend class LiteralArgumentBuilder;

		virtual bool IsValidInput(std::wstring input) = 0;

		virtual std::wstring GetSortedKey() = 0;

	private:
		std::map<std::wstring, std::shared_ptr<CommandNode>, std::less<> > children;
		std::vector<std::shared_ptr<LiteralCommandNode<S> > > literals;
		std::vector<std::shared_ptr<IArgumentCommandNode<S> > > arguments;
		Command<S> command = nullptr;
		Predicate<S &> requirement = nullptr;
		std::shared_ptr<CommandNode> redirect = nullptr;
		RedirectModifier<S> modifier = nullptr;
		bool forks = false;
	};

	template<typename S>
	class RootCommandNode : public CommandNode<S> {
	public:
		RootCommandNode() : CommandNode<S>(nullptr, [](S &) { return true; }, nullptr,
		                                   [](auto s)-> std::vector<S> { return {s.GetSource()}; }, false) {}

		~RootCommandNode() override = default;

		std::wstring const &GetName() override {
			static const std::wstring blank;
			return blank;
		}

		std::wstring GetUsageText() override { return {}; }
		std::vector<std::wstring> GetExamples() override { return {}; }

		void Parse(StringReader &reader, CommandContext<S> &contextBuilder) override {}

		std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) override { return Suggestions::Empty(); }

		CommandNodeType GetNodeType() override { return CommandNodeType::RootCommandNode; }

	protected:
		bool IsValidInput(std::wstring input) override { return false; }
		std::wstring GetSortedKey() override { return {}; }
	};

	template<typename S>
	class LiteralCommandNode : public CommandNode<S> {
	public:
		LiteralCommandNode(const std::wstring& literal, std::shared_ptr<Command<S> > command, Predicate<S &> requirement,
		                   std::shared_ptr<CommandNode<S> > redirect, RedirectModifier<S> modifier, const bool forks)
			: CommandNode<S>(command, requirement, redirect, modifier, forks), literal(literal), literalLowerCase(literal) {
			std::transform(literalLowerCase.begin(), literalLowerCase.end(), literalLowerCase.begin(), [](const wchar_t c) {
				return std::tolower(c);
			});
		}

		LiteralCommandNode(const std::wstring& literal)
			: literal(literal), literalLowerCase(literal) {
			std::transform(literalLowerCase.begin(), literalLowerCase.end(), literalLowerCase.begin(), [](const wchar_t c) {
				return std::tolower(c);
			});
		}

		~LiteralCommandNode() override = default;

		std::wstring const &GetName() override { return literal; }
		std::wstring GetUsageText() override { return literal; }
		std::vector<std::wstring> GetExamples() override { return {literal}; }

		void Parse(StringReader &reader, CommandContext<S> &contextBuilder) override {
			const int start = reader.GetCursor();
			const int end = Parse(reader);
			if (end > -1) {
				contextBuilder.WithNode(this, StringRange::Between(start, end));
				return;
			}

			throw CommandSyntaxException::BuiltInExceptions::LiteralIncorrect(reader, literal);
		}

		std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) override {
			if (builder.AutoSuggest(literalLowerCase, builder.GetRemainingLowerCase())) return builder.BuildFuture();
			return Suggestions::Empty();
		}

		CommandNodeType GetNodeType() override { return CommandNodeType::LiteralCommandNode; }

	protected:
		bool IsValidInput(std::wstring input) override {
			StringReader reader(std::move(input));
			return Parse(reader) > -1;
		}

		std::wstring GetSortedKey() override { return literal; }

	private:
		int Parse(StringReader &reader) const {
			const int start = reader.GetCursor();
			if (reader.CanRead(static_cast<int>(literal.length()))) {
				if (reader.string.compare(start, literal.length(), literal) == 0) {
					const int end = static_cast<int>(start + literal.length());
					reader.SetCursor(end);
					if (!reader.CanRead() || reader.Peek() == L' ') return end;
					reader.SetCursor(start);
				}
			}
			return -1;
		}

		std::wstring literal;
		std::wstring literalLowerCase;
	};

	template<typename T>
	class ArgumentType {
	public:
		using type = T;

		template<typename... Args>
		static CommandSyntaxException CommandParseException(ExceptionContext const &ctx, Args &&... args) {
			return CommandSyntaxException(ctx, L"Error during parsing value of type '", GetTypeName(), L"': ", std::forward<Args>(args)...);
		}

		static T Parse(StringReader &reader) { return reader.ReadValue<T>(); }

		template<typename S>
		static std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) { return Suggestions::Empty(); }

		static constexpr std::wstring_view GetTypeName() {
#ifdef HAS_NAMEOF
			auto nameof_type = std::string(NAMEOF_TYPE(T));
			nameof_type = nameof_type.substr(nameof_type.find(NAMEOF_SHORT_TYPE(T)));
			return std::wstring(nameof_type.begin(), nameof_type.end()); // Short type + template list
#else
			return L"";
#endif
		}

		static std::vector<std::wstring> GetExamples() { return {}; }
	};

	REGISTER_ARGTYPE_TEMPL(ArgumentType, Type);

	enum class StringArgType {
		SINGLE_WORD,
		QUOTABLE_PHRASE,
		GREEDY_PHRASE
	};

	namespace detail {
		std::wstring EscapeStringArgument(const std::wstring& input);
		std::wstring EscapeStringArgumentIfRequired(const std::wstring& input);
	}

	template<StringArgType strType>
	class StringArgumentType : public ArgumentType<std::wstring> {
	public:
		StringArgumentType() = default;

		static StringArgType GetType() { return strType; }

		static std::wstring Parse(StringReader &reader) {
			if (strType == StringArgType::GREEDY_PHRASE) {
				std::wstring text(reader.GetRemaining());
				reader.SetCursor(reader.GetTotalLength());
				return text;
			}
			if (strType == StringArgType::SINGLE_WORD) return std::wstring(reader.ReadUnquotedString());
			return reader.ReadString();
		}

		static constexpr std::wstring_view GetTypeName() {
			if constexpr (strType == StringArgType::GREEDY_PHRASE) return L"words";
			else if constexpr (strType == StringArgType::SINGLE_WORD) return L"word";
			else return L"string";
		}

		static std::vector<std::wstring> GetExamples() {
			if constexpr (strType == StringArgType::GREEDY_PHRASE) return {L"word", L"words with spaces", L"\"and symbols\""};
			else if constexpr (strType == StringArgType::SINGLE_WORD) return {L"word", L"words_with_underscores"};
			else return {L"\"quoted phrase\"", L"word", L"\"\""};
		}

		static std::wstring EscapeIfRequired(const std::wstring& input) { return detail::EscapeStringArgumentIfRequired(input); }

		static std::wstring Escape(const std::wstring& input) { return detail::EscapeStringArgument(input); }
	};

	REGISTER_ARGTYPE_SPEC(StringArgumentType, Word, StringArgType::SINGLE_WORD);
	REGISTER_ARGTYPE_SPEC(StringArgumentType, String, StringArgType::QUOTABLE_PHRASE);
	REGISTER_ARGTYPE_SPEC(StringArgumentType, GreedyString, StringArgType::GREEDY_PHRASE);

	class BoolArgumentType : public ArgumentType<bool> {
	public:
		template<typename S>
		static std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) {
			builder.AutoSuggestLowerCase(std::initializer_list({L"true", L"false"}));
			return builder.BuildFuture();
		}

		static constexpr std::wstring_view GetTypeName() { return L"bool"; }

		static std::vector<std::wstring> GetExamples() { return {L"true", L"false"}; }
	};

	REGISTER_ARGTYPE(BoolArgumentType, Bool);

	class CharArgumentType : public ArgumentType<wchar_t> {
	public:
		// defined in brigadier.cpp
		static wchar_t Parse(StringReader &reader);

		static constexpr std::wstring_view GetTypeName() { return L"char"; }

		static std::vector<std::wstring> GetExamples() { return {L"c", L"@", L"."}; }
	};

	REGISTER_ARGTYPE(CharArgumentType, Char);

	template<typename T>
	class ArithmeticArgumentType : public ArgumentType<T> {
		static_assert(std::is_arithmetic_v<T>, "T must be a number");

	public:
		ArithmeticArgumentType(T minimum = std::numeric_limits<T>::lowest(), T maximum = std::numeric_limits<T>::max()) : minimum(minimum), maximum(maximum) {}

		T GetMinimum() { return minimum; }

		T GetMaximum() { return maximum; }

		T Parse(StringReader &reader) {
			const int start = reader.GetCursor();
			T result = reader.ReadValue<T>();
			if (result < minimum) {
				reader.SetCursor(start);
				throw CommandSyntaxException::BuiltInExceptions::ValueTooLow(reader, result, minimum);
			}
			if (result > maximum) {
				reader.SetCursor(start);
				throw CommandSyntaxException::BuiltInExceptions::ValueTooHigh(reader, result, maximum);
			}
			return result;
		}

		static constexpr std::wstring_view GetTypeName() {
			if constexpr (std::is_floating_point_v<T>) return L"float";
			else if constexpr (std::is_integral_v<T>) {
				if constexpr (std::is_signed_v<T>) return L"int";
				else return L"uint";
			} else return ArgumentType<T>::GetTypeName();
		}

		static std::vector<std::wstring> GetExamples() {
			if constexpr (std::is_floating_point_v<T>) return {L"0", L"1.2", L".5", L"-1", L"-.5", L"-1234.56"};
			else if constexpr (std::is_integral_v<T>) {
				if constexpr (std::is_signed_v<T>) return {L"0", L"123", L"-123"};
				else return {L"0", L"123"};
			} else return {};
		}

	private:
		T minimum;
		T maximum;
	};

	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, Float, float);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, Double, double);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, Integer, int);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, Long, long long);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, INT16, int16_t);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, UINT16, uint16_t);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, INT32, int32_t);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, UINT32, uint32_t);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, INT64, int64_t);
	REGISTER_ARGTYPE_SPEC(ArithmeticArgumentType, UINT64, uint64_t);
	REGISTER_ARGTYPE_TEMPL(ArithmeticArgumentType, Number);

#ifdef HAS_MAGICENUM
	template<typename T>
	class EnumArgumentType : public ArgumentType<T> {
		static_assert(std::is_enum_v<T>, "T must be enum");

	public:
		T Parse(StringReader &reader) {
			int start = reader.GetCursor();
			auto str = reader.ReadString();
			auto result = magic_enum::enum_cast<T>(str);
			if (!result.has_value()) throw CommandSyntaxException::BuiltInExceptions::ReaderInvalidValue(reader, str);
			return result.value();
		}

		template<typename S>
		std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) {
			static constexpr auto names = magic_enum::enum_names<T>();
			builder.AutoSuggestLowerCase(names);
			return builder.BuildFuture();
		}

		static constexpr std::wstring_view GetTypeName() { return L"enum"; }

		static inline std::vector<std::wstring> GetExamples() {
			static constexpr auto names = magic_enum::enum_names<T>();
			return std::vector<std::wstring>(names.begin(), names.end());
		}
	};
	REGISTER_ARGTYPE_TEMPL(EnumArgumentType, Enum);
#endif

	struct TypeInfo {
		TypeInfo(const size_t hash) : hash(hash) {}

		template<typename ArgType>
		static constexpr size_t Create() {
			return (reinterpret_cast<uintptr_t>(ArgType::GetTypeName().data()) + (sizeof(typename ArgType::type) << 24) + (sizeof(ArgType) << 8));
		}

		bool operator==(TypeInfo const &other) const { return hash == other.hash; }
		bool operator!=(TypeInfo const &other) const { return hash != other.hash; }
		size_t hash = 0;
	};

	template<typename S>
	class IParsedArgument {
	public:
		IParsedArgument(const int start, const int end, const TypeInfo typeInfo) : range(start, end), typeInfo(typeInfo) {}

		virtual ~IParsedArgument() = default;

		StringRange GetRange() const { return range; }
		TypeInfo GetTypeInfo() const { return typeInfo; }

	protected:
		StringRange range;
		TypeInfo typeInfo;
	};

	template<typename S, typename ArgType>
	class ParsedArgument : public IParsedArgument<S> {
	public:
		using T = ArgType::type;

		ParsedArgument(const int start, const int end, T result) : IParsedArgument<S>(start, end, TypeInfo(TypeInfo::Create<ArgType>())), result(std::move(result)) {}

		~ParsedArgument() override = default;

		T &GetResult() { return result; }
		T const &GetResult() const { return result; }

	private:
		T result;
	};

	template<typename S>
	struct ParsedCommandNode {
		ParsedCommandNode(CommandNode<S> *node, StringRange range) : node(node), range(range) {}

		CommandNode<S> *GetNode() const { return node; }
		StringRange GetRange() const { return range; }

	private:
		CommandNode<S> *node;
		StringRange range;
	};

	template<typename S>
	class SuggestionContext {
	public:
		SuggestionContext(CommandNode<S> *parent, int startPos) : parent(parent), startPos(startPos) {}

		CommandNode<S> *parent;
		int startPos;
	};

	template<typename S, typename T>
	class ArgumentCommandNode;

	namespace detail {
		template<typename S>
		class CommandContextInternal;
	}

	template<typename S>
	class CommandContext {
	public:
		CommandContext(S source, CommandNode<S> *root, int start) : source(std::move(source)),
			context(std::make_unique<detail::CommandContextInternal<S> >(root, start)) {}

		CommandContext(S source, CommandNode<S> *root, StringRange range) : source(std::move(source)),
			context(std::make_unique<detail::CommandContextInternal<S> >(root, range)) {}

		CommandContext GetFor(S source) const;

		CommandContext *GetChild() const;

		CommandContext *GetLastChild() const;

		CommandContext *GetParent() const;

		CommandContext *GetLastParent() const;

		Command<S> GetCommand() const;

		S &GetSource();

		S const &GetSource() const;

		RedirectModifier<S> GetRedirectModifier() const;

		StringRange GetRange() const;

		std::wstring GetInput() const;

		CommandNode<S> *GetRootNode() const;

		std::vector<ParsedCommandNode<S> > &GetNodes() const;

	protected:
		detail::CommandContextInternal<S> *GetInternalContext() const;

	public:
		bool HasNodes() const;

		bool IsForked() const;

		template<typename ArgType>
		ArgType::type GetArgument(const std::wstring &name);

		template<typename ArgType>
		ArgType::type GetArgumentOr(const std::wstring &name, const ArgType::type &default_value);

		~CommandContext();

	protected:
		CommandContext &WithInput(std::wstring input);

		CommandContext &WithSource(S source);

		CommandContext &WithArgument(std::wstring name, std::shared_ptr<IParsedArgument<S> > argument);

		CommandContext &WithCommand(Command<S> command);

		CommandContext &WithNode(CommandNode<S> *node, StringRange range);

		CommandContext &WithChildContext(CommandContext childContext);

		void Reset() {
			detail::CommandContextInternal<S> &ctx = *context;
			input = {};
			ctx.arguments.clear();
			ctx.command = nullptr;
			ctx.nodes.clear();
			ctx.parent = nullptr;
			ctx.child = {};
			ctx.modifier = nullptr;
			ctx.forks = false;
		}

		void Reset(S src, CommandNode<S> *root) {
			Reset();
			source = std::move(src);
			detail::CommandContextInternal<S> &ctx = *context;
			ctx.rootNode = root;
		}

	public:
		void Reset(S source, CommandNode<S> *root, const int start) {
			Reset(source, root);
			detail::CommandContextInternal<S> &ctx = *context;
			ctx.range = StringRange::At(start);
		}

		void Reset(S source, CommandNode<S> *root, StringRange range) {
			Reset(source, root);
			detail::CommandContextInternal<S> &ctx = *context;
			ctx.range = std::move(range);
		}

	protected:
		SuggestionContext<S> FindSuggestionContext(int cursor);

		void Merge(CommandContext other);

	private:
		friend class CommandDispatcher<S>;
		friend class LiteralCommandNode<S>;
		friend class CommandNode<S>;
		template<typename _S, typename T>
		friend class ArgumentCommandNode;

		S source;
		std::wstring input = {};
		std::shared_ptr<detail::CommandContextInternal<S> > context = nullptr;
	};

	namespace detail {
		template<typename S>
		class CommandContextInternal {
		public:
			CommandContextInternal(CommandNode<S> *root, int start) : rootNode(root), range(StringRange::At(start)) {}

			CommandContextInternal(CommandNode<S> *root, StringRange range) : rootNode(root), range(range) {}

		protected:
			friend class CommandContext<S>;

			std::map<std::wstring, std::shared_ptr<IParsedArgument<S> >, std::less<> > arguments;
			Command<S> command = nullptr;
			CommandNode<S> *rootNode = nullptr;
			std::vector<ParsedCommandNode<S> > nodes;
			StringRange range;
			CommandContext<S> *parent = nullptr;
			std::optional<CommandContext<S> > child = {};
			RedirectModifier<S> modifier = nullptr;
			bool forks = false;
		};
	}

	template<typename S>
	template<typename ArgType>
	ArgType::type CommandContext<S>::GetArgument(const std::wstring &name) {
		auto argument = context->arguments.find(name);

		if (argument == context->arguments.end()) throw CommandSyntaxException(StringReader(), L"No such argument '" + name + L"' exists on this command");
		auto &parsed = argument->second;
		if (parsed->GetTypeInfo() != TypeInfo(TypeInfo::Create<ArgType>()))
			throw CommandSyntaxException(StringReader(), L"Argument '" + name + L"' has been acquired using wrong type");

		return static_cast<ParsedArgument<S, ArgType> *>(parsed.get())->GetResult();
	}

	template<typename S>
	template<typename ArgType>
	ArgType::type CommandContext<S>::GetArgumentOr(const std::wstring &name, const typename ArgType::type &default_value) {
		auto argument = context->arguments.find(name);

		if (argument == context->arguments.end()) return std::move(default_value);
		auto &parsed = argument->second;
		if (parsed->GetTypeInfo() != TypeInfo(TypeInfo::Create<ArgType>()))
			throw CommandSyntaxException(StringReader(), L"Argument '" + name + L"' has been acquired using wrong type");

		return static_cast<ParsedArgument<S, ArgType> *>(parsed.get())->GetResult();
	}

	template<typename S>
	CommandContext<S> CommandContext<S>::GetFor(S source) const {
		CommandContext<S> result = *this;
		result.source = std::move(source);
		return result;
	}

	template<typename S>
	CommandContext<S> *CommandContext<S>::GetChild() const {
		if (context->child.has_value()) return &context->child.value();
		return nullptr;
	}

	template<typename S>
	CommandContext<S> *CommandContext<S>::GetParent() const { return context->parent; }

	template<typename S>
	CommandContext<S> *CommandContext<S>::GetLastParent() const {
		auto *result = const_cast<CommandContext *>(this);
		while (result->GetParent() != nullptr) result = result->GetParent();
		return result;
	}

	template<typename S>
	detail::CommandContextInternal<S> *CommandContext<S>::GetInternalContext() const { return context.get(); }

	template<typename S>
	CommandContext<S> *CommandContext<S>::GetLastChild() const {
		auto *result = const_cast<CommandContext *>(this);
		while (result->GetChild() != nullptr) result = result->GetChild();
		return result;
	}

	template<typename S>
	Command<S> CommandContext<S>::GetCommand() const { return context->command; }


	template<typename S>
	S &CommandContext<S>::GetSource() { return source; }

	template<typename S>
	S const &CommandContext<S>::GetSource() const { return source; }

	template<typename S>
	RedirectModifier<S> CommandContext<S>::GetRedirectModifier() const { return context->modifier; }

	template<typename S>
	StringRange CommandContext<S>::GetRange() const { return context->range; }

	template<typename S>
	std::wstring CommandContext<S>::GetInput() const { return context->input; }

	template<typename S>
	CommandNode<S> *CommandContext<S>::GetRootNode() const { return context->rootNode; }

	template<typename S>
	std::vector<ParsedCommandNode<S> > &CommandContext<S>::GetNodes() const { return context->nodes; }

	template<typename S>
	bool CommandContext<S>::HasNodes() const { return context->nodes.size() > 0; }

	template<typename S>
	bool CommandContext<S>::IsForked() const { return context->forks; }

	template<typename S>
	CommandContext<S>::~CommandContext() {
		if (context && context->child.has_value()) context->child->context->parent = nullptr;
	}

	template<typename S>
	CommandContext<S> &CommandContext<S>::WithInput(std::wstring input) {
		this->input = std::move(input);
		return *this;
	}

	template<typename S>
	CommandContext<S> &CommandContext<S>::WithSource(S source) {
		context->source = std::move(source);
		return *this;
	}

	template<typename S>
	CommandContext<S> &CommandContext<S>::WithArgument(std::wstring name, std::shared_ptr<IParsedArgument<S> > argument) {
		context->arguments.emplace(name, std::move(argument));
		return *this;
	}

	template<typename S>
	CommandContext<S> &CommandContext<S>::WithCommand(Command<S> command) {
		context->command = command;
		return *this;
	}

	template<typename S>
	CommandContext<S> &CommandContext<S>::WithNode(CommandNode<S> *node, StringRange range) {
		if (node) {
			context->nodes.emplace_back(node, range);
			context->range = StringRange::Encompassing(context->range, range);
			context->modifier = node->GetRedirectModifier();
			context->forks = node->IsFork();
		}
		return *this;
	}

	template<typename S>
	CommandContext<S> &CommandContext<S>::WithChildContext(CommandContext childContext) {
		context->child = childContext;
		context->child->context->parent = this;
		return *this;
	}

	template<typename S>
	SuggestionContext<S> CommandContext<S>::FindSuggestionContext(int cursor) {
		if (auto &ctx = context; ctx->range.GetStart() <= cursor) {
			if (ctx->range.GetEnd() < cursor) {
				if (ctx->child.has_value()) return ctx->child->FindSuggestionContext(cursor);
				if (HasNodes()) {
					auto &last = ctx->nodes.back();
					return SuggestionContext<S>(last.GetNode(), last.GetRange().GetEnd() + 1);
				}
				return SuggestionContext<S>(ctx->rootNode, ctx->range.GetStart());
			}
			auto prev = ctx->rootNode;
			for (auto &node: ctx->nodes) {
				if (auto nodeRange = node.GetRange(); nodeRange.GetStart() <= cursor && cursor <= nodeRange.GetEnd())
					return SuggestionContext<S>(prev, nodeRange.GetStart());
				prev = node.GetNode();
			}
			if (prev == nullptr) throw std::runtime_error("Can't find node before cursor");
			return SuggestionContext<S>(prev, ctx->range.GetStart());
		}
		throw std::runtime_error("Can't find node before cursor");
	}

	template<typename S>
	void CommandContext<S>::Merge(CommandContext<S> other) {
		detail::CommandContextInternal<S> *ctx = other.GetInternalContext();
		for (auto &arg: ctx->arguments) context->arguments.emplace(std::move(arg));
		context->command = std::move(ctx->command);
		source = std::move(other.source);
		context->nodes.reserve(context->nodes.size() + ctx->nodes.size());
		for (auto &node: ctx->nodes) {
			context->range = StringRange::Encompassing(context->range, ctx->range);
			context->modifier = node.GetNode()->GetRedirectModifier();
			context->forks = node.GetNode()->IsFork();

			context->nodes.emplace_back(std::move(node));
		}
		if (ctx->child.has_value()) {
			ctx->child->context->parent = this;
			if (context->child.has_value()) {
				context->child->Merge(*ctx->child);
				ctx->child = {};
			} else context->child = std::move(ctx->child);
		}
	}

	template<typename S>
	class IArgumentCommandNode : public CommandNode<S> {
	protected:
		IArgumentCommandNode(std::wstring name) : name(std::move(name)) {}

		~IArgumentCommandNode() override = default;

	public:
		std::wstring const &GetName() override { return name; }

		CommandNodeType GetNodeType() override { return CommandNodeType::ArgumentCommandNode; }

	protected:
		std::wstring GetSortedKey() override { return name; }

		std::wstring name;
	};

	template<typename S, typename T>
	class ArgumentCommandNode : public IArgumentCommandNode<S> {
		const std::wstring USAGE_ARGUMENT_OPEN = L"<";
		const std::wstring USAGE_ARGUMENT_CLOSE = L">";

	public:
		template<typename... Args>
		ArgumentCommandNode(std::wstring name, Args &&... args) : IArgumentCommandNode<S>(name), type(std::forward<Args>(args)...) {}

		~ArgumentCommandNode() override = default;

		SuggestionProvider<S> const &GetCustomSuggestions() const { return customSuggestions; }

		T const &GetType() { return type; }

		std::wstring GetUsageText() override {
			std::wstring ret;
			constexpr auto typeName = T::GetTypeName();
			ret.reserve(
				this->name.size() + USAGE_ARGUMENT_OPEN.size() + USAGE_ARGUMENT_CLOSE.size() + typeName.size() > 0 ? typeName.size() + 2 : 0);
			ret = USAGE_ARGUMENT_OPEN;
			if constexpr (typeName.size() > 0) {
				ret += typeName;
				ret += L": ";
			}
			ret += this->name;
			ret += USAGE_ARGUMENT_CLOSE;
			return ret;
		}

		std::vector<std::wstring> GetExamples() override { return type.GetExamples(); }

		void Parse(StringReader &reader, CommandContext<S> &contextBuilder) override {
			int start = reader.GetCursor();
			using Type = T::type;
			Type result = type.Parse(reader);
			std::shared_ptr<ParsedArgument<S, T> > parsed = std::make_shared<ParsedArgument<S, T> >(start, reader.GetCursor(), std::move(result));

			contextBuilder.WithArgument(this->name, parsed);
			contextBuilder.WithNode(this, parsed->GetRange());
		}

		std::future<Suggestions> ListSuggestions(CommandContext<S> &context, SuggestionsBuilder &builder) override {
			if (customSuggestions == nullptr) return type.template ListSuggestions<S>(context, builder);
			return customSuggestions(context, builder);
		}

	protected:
		bool IsValidInput(const std::wstring input) override {
			try {
				auto reader = StringReader(input);
				type.Parse(reader);
				return !reader.CanRead() || reader.Peek() == L' ';
			} catch (CommandSyntaxException const &) {
				return false;
			}
		}

	private:
		friend class RequiredArgumentBuilder<S, T>;
		T type;
		SuggestionProvider<S> customSuggestions = nullptr;
	};

	template<typename S>
	class MultiArgumentBuilder {
	public:
		using B = MultiArgumentBuilder;

		MultiArgumentBuilder(std::vector<std::shared_ptr<CommandNode<S> > > nodes, const int master = -1) : nodes(std::move(nodes)), master(master) {}

		MultiArgumentBuilder(MultiArgumentBuilder const &) = delete; // no copying. Use reference or GetThis().

		B *GetThis() { return this; }

		template<template<typename...> typename Next, typename Type = void, typename... Args>
		auto Then(Args &&... args) {
			for (auto &node: nodes) if (node->redirect != nullptr) throw std::runtime_error("Cannot add children to a redirected node");

			if constexpr (std::is_same_v<Type, void>) {
				using next_node = Next<S>::node_type;
				next_node node_builder(std::forward<Args>(args)...);
				auto new_node = std::make_shared<next_node>(std::move(node_builder));
				for (auto &node: nodes) node->AddChild(new_node);
				return Next<S>(std::move(new_node));
			} else {
				using next_node = Next<S, Type>::node_type;
				next_node node_builder(std::forward<Args>(args)...);
				auto new_node = std::make_shared<next_node>(std::move(node_builder));
				for (auto &node: nodes) node->AddChild(new_node);
				return Next<S, Type>(std::move(new_node));
			}
		}

		auto Then(std::shared_ptr<LiteralCommandNode<S> > argument) {
			for (auto &node: nodes)
				if (node->redirect != nullptr) throw std::runtime_error("Cannot add children to a redirected node");
			for (auto &node: nodes) if (argument != nullptr) node->AddChild(argument);
			return GetBuilder(std::move(argument));
		}

		template<typename T>
		auto Then(std::shared_ptr<ArgumentCommandNode<S, T> > argument) {
			for (auto &node: nodes)
				if (node->redirect != nullptr) throw std::runtime_error("Cannot add children to a redirected node");
			for (auto &node: nodes) if (argument != nullptr) node->AddChild(argument);
			return GetBuilder(std::move(argument));
		}

		B &Executes(Command<S> command, const bool only_master = true) {
			for (size_t i = 0; i < nodes.size(); ++i) {
				if (master == -1 || master == i || !only_master) {
					auto &node = nodes[i];
					node->command = command;
				}
			}
			return *GetThis();
		}

		B &Requires(Predicate<S &> requirement, bool only_master = true) {
			for (size_t i = 0; i < nodes.size(); ++i) {
				if (master == -1 || master == i || !only_master) {
					auto &node = nodes[i];
					node->requirement = requirement;
				}
			}
			return *GetThis();
		}

		auto Redirect(std::shared_ptr<CommandNode<S> > target, const bool only_master = true) {
			return Forward(std::move(target), nullptr, false, only_master);
		}

		auto Redirect(std::shared_ptr<CommandNode<S> > target, SingleRedirectModifier<S> modifier, const bool only_master = true) {
			return Forward(std::move(target), modifier ? [modifier](CommandContext<S> &context) -> std::vector<S> {
				return {modifier(context)};
			} : nullptr, false, only_master);
		}

		auto Fork(std::shared_ptr<CommandNode<S> > target, SingleRedirectModifier<S> modifier, const bool only_master = true) {
			return Forward(std::move(target), modifier ? [modifier](CommandContext<S> &context) -> std::vector<S> {
				return {modifier(context)};
			} : nullptr, true, only_master);
		}

		auto Fork(std::shared_ptr<CommandNode<S> > target, const RedirectModifier<S> modifier, const bool only_master = true) {
			return Forward(std::move(target), modifier, true, only_master);
		}

		void Forward(std::shared_ptr<CommandNode<S> > target, RedirectModifier<S> modifier, bool fork, const bool only_master = true) {
			for (size_t i = 0; i < nodes.size(); ++i) {
				if (master == -1 || master == i || !only_master) {
					auto &node = nodes[i];
					if (node->GetChildren().size() > 0) throw std::runtime_error("Cannot forward a node with children");
				}
			}
			for (size_t i = 0; i < nodes.size(); ++i) {
				if (master == -1 || master == i || !only_master) {
					auto &node = nodes[i];
					node->redirect = std::move(target);
					node->modifier = modifier;
					node->forks = fork;
				}
			}
		}

	protected:
		std::vector<std::shared_ptr<CommandNode<S> > > nodes;
		int master = -1;
	};

	// multi builder
	template<typename S>
	MultiArgumentBuilder<S> GetBuilder(std::vector<std::shared_ptr<CommandNode<S> > > nodes, int master = -1) {
		return MultiArgumentBuilder<S>(std::move(nodes), master);
	}

	template<typename S, typename B, typename NodeType>
	class ArgumentBuilder {
	public:
		using node_type = NodeType;

		ArgumentBuilder(std::shared_ptr<node_type> node) {
			if (node) this->node = std::move(node);
			else throw std::runtime_error("Cannot build empty node");
		}

		ArgumentBuilder(ArgumentBuilder const &) = delete; // no copying. Use reference or GetThis().

		B *GetThis() { return static_cast<B *>(this); }

		std::shared_ptr<node_type> GetNode() const { return node; }

		std::shared_ptr<CommandNode<S> > GetCommandNode() const { return std::static_pointer_cast<CommandNode<S> >(node); }

		operator std::shared_ptr<node_type>() const { return GetNode(); }
		operator std::shared_ptr<CommandNode<S> >() const { return GetCommandNode(); }

		template<template<typename...> typename Next, typename Type = void, typename... Args>
		auto Then(Args &&... args) {
			if (node->redirect != nullptr) throw std::runtime_error("Cannot add children to a redirected node");

			if constexpr (std::is_same_v<Type, void>) {
				using next_node = Next<S>::node_type;
				next_node node_builder(std::forward<Args>(args)...);
				auto &name = node_builder.GetName();
				auto arg = node->children.find(name);
				if (arg == node->children.end()) {
					auto new_node = std::make_shared<next_node>(std::move(node_builder));
					node->AddChild(new_node);
					return Next<S>(std::move(new_node));
				}
				auto &arg_ptr = arg->second;
				if (arg_ptr && node_builder.GetNodeType() != arg_ptr->GetNodeType()) throw std::runtime_error("Node type (literal/argument) mismatch!");
				return Next<S>(std::static_pointer_cast<next_node>(arg_ptr));
			} else {
				using next_node = Next<S, Type>::node_type;
				next_node node_builder(std::forward<Args>(args)...);
				auto &name = node_builder.GetName();
				auto arg = node->children.find(name);
				if (arg == node->children.end()) {
					auto new_node = std::make_shared<next_node>(std::move(node_builder));
					node->AddChild(new_node);
					return Next<S, Type>(std::move(new_node));
				}
				auto &arg_ptr = arg->second;
				if (arg_ptr && node_builder.GetNodeType() != arg_ptr->GetNodeType()) throw std::runtime_error("Node type (literal/argument) mismatch!");
				return Next<S, Type>(std::static_pointer_cast<next_node>(arg_ptr));
			}
		}

		template<template<typename...> typename Next, typename Type = void, typename... Args>
		auto ThenOptional(Args &&... args) {
			auto opt = Then<Next, Type, Args...>(std::forward<Args>(args)...);
			return MultiArgumentBuilder<S>({opt.GetCommandNode(), GetCommandNode()}, 0);
		}

		auto Then(std::shared_ptr<LiteralCommandNode<S> > argument) {
			if (node->redirect != nullptr) throw std::runtime_error("Cannot add children to a redirected node");
			if (argument != nullptr) node->AddChild(std::move(argument));
			return GetBuilder(std::move(node->GetChild(argument)));
		}

		template<typename T>
		auto Then(std::shared_ptr<ArgumentCommandNode<S, T> > argument) {
			if (node->redirect != nullptr) throw std::runtime_error("Cannot add children to a redirected node");
			if (argument != nullptr) node->AddChild(std::move(argument));
			return GetBuilder(std::move(node->GetChild(argument)));
		}

		B &Executes(Command<S> command) {
			node->command = command;
			return *GetThis();
		}

		B &Requires(Predicate<S &> requirement) {
			node->requirement = requirement;
			return *GetThis();
		}

		auto Redirect(std::shared_ptr<CommandNode<S> > target) { return Forward(std::move(target), nullptr, false); }

		auto Redirect(std::shared_ptr<CommandNode<S> > target, SingleRedirectModifier<S> modifier) {
			return Forward(std::move(target), modifier ? [modifier](CommandContext<S> &context) -> std::vector<S> {
				return {modifier(context)};
			} : nullptr, false);
		}

		auto Fork(std::shared_ptr<CommandNode<S> > target, SingleRedirectModifier<S> modifier) {
			return Forward(std::move(target), modifier ? [modifier](CommandContext<S> &context) -> std::vector<S> {
				return {modifier(context)};
			} : nullptr, true);
		}

		auto Fork(std::shared_ptr<CommandNode<S> > target, const RedirectModifier<S> modifier) { return Forward(std::move(target), modifier, true); }

		void Forward(std::shared_ptr<CommandNode<S> > target, RedirectModifier<S> modifier, bool fork) {
			if (node->GetChildren().size() > 0) throw std::runtime_error("Cannot forward a node with children");
			node->redirect = std::move(target);
			node->modifier = modifier;
			node->forks = fork;
		}

	protected:
		template<typename _S, typename _B>
		friend class RequiredArgumentBuilder;

		std::shared_ptr<node_type> node;
	};

	template<typename S>
	class LiteralArgumentBuilder : public ArgumentBuilder<S, LiteralArgumentBuilder<S>, LiteralCommandNode<S> > {
	public:
		LiteralArgumentBuilder(std::shared_ptr<LiteralCommandNode<S> > node) : ArgumentBuilder<S, LiteralArgumentBuilder, LiteralCommandNode<S> >(std::move(node)) {}
	};

	REGISTER_ARGTYPE_TEMPL(LiteralArgumentBuilder, Literal);

	// single builder
	template<typename S>
	LiteralArgumentBuilder<S> GetBuilder(std::shared_ptr<LiteralCommandNode<S> > node) { return LiteralArgumentBuilder<S>(std::move(node)); }

	// new single builder
	template<typename S, typename... Args>
	LiteralArgumentBuilder<S> MakeLiteral(Args &&... args) {
		return LiteralArgumentBuilder<S>(std::make_shared<LiteralCommandNode<S> >(std::forward<Args>(args)...));
	}

	template<typename S, typename T>
	class RequiredArgumentBuilder : public ArgumentBuilder<S, RequiredArgumentBuilder<S, T>, ArgumentCommandNode<S, T> > {
	public:
		RequiredArgumentBuilder(std::shared_ptr<ArgumentCommandNode<S, T> > node) :
			ArgumentBuilder<S, RequiredArgumentBuilder, ArgumentCommandNode<S, T> >(std::move(node)) {}

		RequiredArgumentBuilder &Suggests(SuggestionProvider<S> provider) {
			this->node->suggestionsProvider = provider;
			return *this;
		}
	};

	REGISTER_ARGTYPE_TEMPL(RequiredArgumentBuilder, Argument);

	// single builder
	template<typename S, template<typename...> typename Spec, typename... Types>
	RequiredArgumentBuilder<S, Spec<S, Types...> > GetBuilder(
		std::shared_ptr<ArgumentCommandNode<S, Spec<S, Types...> > > node) {
		return RequiredArgumentBuilder<S, Spec<S, Types...> >(std::move(node));
	}

	// new single builder
	template<typename S, template<typename...> typename Spec, typename Type, typename... Args>
	RequiredArgumentBuilder<S, Spec<S, Type> > MakeArgument(Args &&... args) {
		return RequiredArgumentBuilder<S, Spec<S, Type> >(std::make_shared<ArgumentCommandNode<S, Spec<S, Type> > >(std::forward<Args>(args)...));
	}

	template<typename S>
	class ParseResults {
	public:
		ParseResults(CommandContext<S> context, StringReader reader, std::map<CommandNode<S> *, CommandSyntaxException> exceptions)
			: context(std::move(context)), exceptions(std::move(exceptions)), reader(std::move(reader)) {}

		ParseResults(CommandContext<S> context, StringReader reader)
			: context(std::move(context)), reader(std::move(reader)) {}

		ParseResults(CommandContext<S> context) : ParseResults(std::move(context), StringReader()) {}

		CommandContext<S> const &GetContext() const { return context; }
		StringReader const &GetReader() const { return reader; }
		std::map<CommandNode<S> *, CommandSyntaxException> const &GetExceptions() const { return exceptions; }

		bool IsBetterThan(ParseResults const &other) const {
			if (!GetReader().CanRead() && other.GetReader().CanRead()) return true;
			if (GetReader().CanRead() && !other.GetReader().CanRead()) return false;
			if (GetExceptions().empty() && !other.GetExceptions().empty()) return true;
			if (!GetExceptions().empty() && other.GetExceptions().empty()) return false;
			return false;
		}

		void Reset(StringReader new_reader) {
			exceptions.clear();
			reader = std::move(new_reader);
		}

		void Reset(S source, CommandNode<S> *root, int start, StringReader reader = {}) {
			Reset(std::move(reader));
			context.Reset(std::move(source), root, start);
		}

		void Reset(S source, CommandNode<S> *root, StringRange range, StringReader reader = {}) {
			Reset(std::move(reader));
			context.Reset(std::move(source), root, std::move(range));
		}

	private:
		template<typename _S>
		friend class CommandDispatcher;

		CommandContext<S> context;
		std::map<CommandNode<S> *, CommandSyntaxException> exceptions;
		StringReader reader;
	};

	/**
	 *   The core command dispatcher, for registering, parsing, and executing commands.
	 *
	 *   \param <S> a custom "source" type, such as a user or originator of a command
	 */
	template<typename S>
	class CommandDispatcher {
	public:
		/**
		 *       The string required to separate individual arguments in an input string
		 */
		const std::wstring ARGUMENT_SEPARATOR = L" ";

		/**
		 *       The wchar_t required to separate individual arguments in an input string
		 */
		static constexpr wchar_t ARGUMENT_SEPARATOR_CHAR = L' ';

	private:
		const std::wstring USAGE_OPTIONAL_OPEN = L"[";
		const std::wstring USAGE_OPTIONAL_CLOSE = L"]";
		const std::wstring USAGE_REQUIRED_OPEN = L"(";
		const std::wstring USAGE_REQUIRED_CLOSE = L")";
		const std::wstring USAGE_OR = L"|";

	public:
		/**
		 *       Create a new CommandDispatcher with the specified root node.
		 *
		 *       This is often useful to copy existing or pre-defined command trees.
		 *
		 *       \param root the existing RootCommandNode to use as the basis for this tree
		 */
		CommandDispatcher(RootCommandNode<S> *root) : root(std::make_shared<RootCommandNode<S> >(*root)) {}

		/**
		 *       Creates a new CommandDispatcher with an empty command tree.
		 */
		CommandDispatcher() : root(std::make_shared<RootCommandNode<S> >()) {}

		/**
		 *       Utility method for registering new commands.
		 *
		 *       \param args these arguments are forwarded to builder to node constructor. The first param is always node name.
		 *       \return the builder with node added to this tree
		 */
		template<template<typename...> typename Next = Literal, typename Type = void, typename... Args>
		auto Register(Args &&... args) {
			if constexpr (std::is_same_v<Type, void>) {
				using next_node = Next<S>::node_type;
				next_node node_builder(std::forward<Args>(args)...);
				auto &name = node_builder.GetName();
				auto arg = root->children.find(name);
				if (arg == root->children.end()) {
					auto new_node = std::make_shared<next_node>(std::move(node_builder));
					root->AddChild(new_node);
					return Next<S>(std::move(new_node));
				}
				auto &arg_ptr = arg->second;
				if (arg_ptr && node_builder.GetNodeType() != arg_ptr->GetNodeType()) throw std::runtime_error("Node type (literal/argument) mismatch!");
				return Next<S>(std::static_pointer_cast<next_node>(arg_ptr));
			} else {
				using next_node = Next<S, Type>::node_type;
				next_node node_builder(std::forward<Args>(args)...);
				auto &name = node_builder.GetName();
				auto arg = root->children.find(name);
				if (arg == root->children.end()) {
					auto new_node = std::make_shared<next_node>(std::move(node_builder));
					root->AddChild(new_node);
					return Next<S, Type>(std::move(new_node));
				}
				auto &arg_ptr = arg->second;
				if (arg_ptr && node_builder.GetNodeType() != arg_ptr->GetNodeType()) throw std::runtime_error("Node type (literal/argument) mismatch!");
				return Next<S, Type>(std::static_pointer_cast<next_node>(arg_ptr));
			}
		}

		/**
		 *       Sets a callback to be informed of the result of every command.
		 *
		 *       \param consumer the new result consumer to be called
		 */
		void SetConsumer(ResultConsumer<S> consumer) { this->consumer = consumer; }

		/**
		 *       Gets the root of this command tree.
		 *
		 *       This is often useful as a target of a ArgumentBuilder::Redirect(CommandNode),
		 *       GetAllUsage(CommandNode, Object, boolean) or GetSmartUsage(CommandNode, Object).
		 *       You may also use it to clone the command tree via CommandDispatcher(RootCommandNode).
		 *
		 *       \return root of the command tree
		 */
		std::shared_ptr<RootCommandNode<S> > GetRoot() const { return root; }

		/**
		 *       Parses and executes a given command.
		 *
		 *       This is a shortcut to first Parse(StringReader, Object) and then Execute(ParseResults).
		 *
		 *       It is recommended to parse and execute as separate steps, as parsing is often the most expensive step, and easiest to cache.
		 *
		 *       If this command returns a value, then it successfully executed something. If it could not parse the command, or the execution was a failure,
		 *       then an exception will be thrown. Most exceptions will be of type CommandSyntaxException, but it is possible that a std::runtime_error
		 *       may bubble up from the result of a command. The meaning behind the returned result is arbitrary, and will depend
		 *       entirely on what command was performed.
		 *
		 *       If the command passes through a node that is CommandNode::IsFork() then it will be 'forked'.
		 *       A forked command will not bubble up any CommandSyntaxException's, and the 'result' returned will turn into
		 *       'amount of successful commands executes'.
		 *
		 *       After each and any command is ran, a registered callback given to SetConsumer(ResultConsumer)
		 *       will be notified of the result and success of the command. You can use that method to gather more meaningful
		 *       results than this method will return, especially when a command forks.
		 *
		 *       \param input a command string to parse and execute
		 *       \param source a custom "source" object, usually representing the originator of this command
		 *       \return a numeric result from a "command" that was performed
		 *       \throws CommandSyntaxException if the command failed to parse or execute
		 *       \throws std::runtime_error if the command failed to execute and was not handled gracefully
		 *       \see Parse(String, Object)
		 *       \see Parse(StringReader, Object)
		 *       \see Execute(ParseResults)
		 *       \see Execute(StringReader, Object)
		 */
		int Execute(const std::wstring &input, S source) {
			auto reader = StringReader(input);
			return Execute(reader, std::move(source));
		}

		/**
		 *       Parses and executes a given command.
		 *
		 *       This is a shortcut to first Parse(StringReader, Object) and then Execute(ParseResults).
		 *
		 *       It is recommended to parse and execute as separate steps, as parsing is often the most expensive step, and easiest to cache.
		 *
		 *       If this command returns a value, then it successfully executed something. If it could not parse the command, or the execution was a failure,
		 *       then an exception will be thrown. Most exceptions will be of type CommandSyntaxException, but it is possible that a std::runtime_error
		 *       may bubble up from the result of a command. The meaning behind the returned result is arbitrary, and will depend
		 *       entirely on what command was performed.
		 *
		 *       If the command passes through a node that is CommandNode::IsFork() then it will be 'forked'.
		 *       A forked command will not bubble up any CommandSyntaxException's, and the 'result' returned will turn into
		 *       'amount of successful commands executes'.
		 *
		 *       After each and any command is ran, a registered callback given to SetConsumer(ResultConsumer)
		 *       will be notified of the result and success of the command. You can use that method to gather more meaningful
		 *       results than this method will return, especially when a command forks.
		 *
		 *       \param input a command string to parse & execute
		 *       \param source a custom "source" object, usually representing the originator of this command
		 *       \return a numeric result from a "command" that was performed
		 *       \throws CommandSyntaxException if the command failed to parse or execute
		 *       \throws std::runtime_error if the command failed to execute and was not handled gracefully
		 *       \see Parse(String, Object)
		 *       \see Parse(StringReader, Object)
		 *       \see Execute(ParseResults)
		 *       \see Execute(String, Object)
		 */
		int Execute(StringReader &input, S source) {
			auto parse = Parse(input, std::move(source));
			return Execute(parse);
		}

		/**
		 *       Executes a given pre-parsed command.
		 *
		 *       If this command returns a value, then it successfully executed something. If the execution was a failure,
		 *       then an exception will be thrown.
		 *       Most exceptions will be of type CommandSyntaxException, but it is possible that a std::runtime_error
		 *       may bubble up from the result of a command. The meaning behind the returned result is arbitrary, and will depend
		 *       entirely on what command was performed.
		 *
		 *       If the command passes through a node that is CommandNode::IsFork() then it will be 'forked'.
		 *       A forked command will not bubble up any CommandSyntaxException's, and the 'result' returned will turn into
		 *       'amount of successful commands executes'.
		 *
		 *       After each and any command is ran, a registered callback given to SetConsumer(ResultConsumer)
		 *       will be notified of the result and success of the command. You can use that method to gather more meaningful
		 *       results than this method will return, especially when a command forks.
		 *
		 *       \param parse the result of a successful Parse(StringReader, Object)
		 *       \return a numeric result from a "command" that was performed.
		 *       \throws CommandSyntaxException if the command failed to parse or execute
		 *       \throws std::runtime_error if the command failed to execute and was not handled gracefully
		 *       \see Parse(String, Object)
		 *       \see Parse(StringReader, Object)
		 *       \see Execute(String, Object)
		 *       \see Execute(StringReader, Object)
		 */
		int Execute(ParseResults<S> &parse) {
			if (parse.GetReader().CanRead()) {
				if (parse.GetExceptions().size() == 1) throw*parse.GetExceptions().begin();
				if (parse.GetContext().GetRange().IsEmpty()) throw CommandSyntaxException::BuiltInExceptions::DispatcherUnknownCommand(parse.GetReader());
				throw CommandSyntaxException::BuiltInExceptions::DispatcherUnknownArgument(parse.GetReader());
			}

			int result = 0;
			int successfulForks = 0;
			bool forked = false;
			bool foundCommand = false;
			auto command = parse.GetReader().GetString();
			auto original = parse.GetContext();
			original.WithInput(command);
			std::vector<CommandContext<S> > contexts = {original};
			std::vector<CommandContext<S> > next;

			while (!contexts.empty()) {
				for (auto &context: contexts) {
					CommandContext<S> *child = context.GetChild();
					if (child != nullptr) {
						forked |= context.IsForked();
						if (child->HasNodes()) {
							foundCommand = true;
							RedirectModifier<S> modifier = context.GetRedirectModifier();
							if (modifier == nullptr) next.push_back(child->GetFor(context.GetSource()));
							else {
								try {
									if (auto results = modifier(context); !results.empty())
										for (auto &source: results) next.push_back(child->GetFor(std::move(source)));
								} catch (CommandSyntaxException const &) {
									consumer(context, false, 0);
									if (!forked) throw;
								}
							}
						}
					} else if (context.GetCommand() != nullptr) {
						foundCommand = true;
						try {
							int value = context.GetCommand()(context);
							result += value;
							consumer(context, true, value);
							successfulForks++;
						} catch (CommandSyntaxException const &) {
							consumer(context, false, 0);
							if (!forked) throw;
						}
					}
				}

				contexts = std::move(next);
			}

			if (!foundCommand) {
				consumer(original, false, 0);
				throw CommandSyntaxException::BuiltInExceptions::DispatcherUnknownCommand(parse.GetReader());
			}

			return forked ? successfulForks : result;
		}

		/**
		 *       Parses a given command.
		 *
		 *       The result of this method can be cached, and it is advised to do so where appropriate. Parsing is often the
		 *       most expensive step, and this allows you to essentially "precompile" a command if it will be ran often.
		 *
		 *       If the command passes through a node that is CommandNode::IsFork() then the resulting context will be marked as 'forked'.
		 *       Forked contexts may contain child contexts, which may be modified by the RedirectModifier attached to the fork.
		 *
		 *       Parsing a command can never fail, you will always be provided with a new ParseResults.
		 *       However, that does not mean that it will always parse into a valid command. You should inspect the returned results
		 *       to check for validity. If its ParseResults::GetReader() StringReader::CanRead() then it did not finish
		 *       parsing successfully. You can use that position as an indicator to the user where the command stopped being valid.
		 *       You may inspect ParseResults::GetExceptions() if you know the parse failed, as it will explain why it could
		 *       not find any valid commands. It may contain multiple exceptions, one for each "potential node" that it could have visited,
		 *       explaining why it did not go down that node.
		 *
		 *       When you eventually call Execute(ParseResults) with the result of this method, the above error checking
		 *       will occur. You only need to inspect it yourself if you wish to handle that yourself.
		 *
		 *       \param command a command string to parse
		 *       \param source a custom "source" object, usually representing the originator of this command
		 *       \return the result of parsing this command
		 *       \see Parse(StringReader, Object)
		 *       \see Execute(ParseResults)
		 *       \see Execute(String, Object)
		 */
		ParseResults<S> Parse(const std::wstring &command, S source) {
			auto reader = StringReader(command);
			return Parse(reader, std::move(source));
		}

		/**
		 *       Parses a given command.
		 *
		 *       The result of this method can be cached, and it is advised to do so where appropriate. Parsing is often the
		 *       most expensive step, and this allows you to essentially "precompile" a command if it will be ran often.
		 *
		 *       If the command passes through a node that is CommandNode::IsFork() then the resulting context will be marked as 'forked'.
		 *       Forked contexts may contain child contexts, which may be modified by the RedirectModifier attached to the fork.
		 *
		 *       Parsing a command can never fail, you will always be provided with a new ParseResults.
		 *       However, that does not mean that it will always parse into a valid command. You should inspect the returned results
		 *       to check for validity. If its ParseResults::GetReader() StringReader::CanRead() then it did not finish
		 *       parsing successfully. You can use that position as an indicator to the user where the command stopped being valid.
		 *       You may inspect ParseResults::GetExceptions() if you know the parse failed, as it will explain why it could
		 *       not find any valid commands. It may contain multiple exceptions, one for each "potential node" that it could have visited,
		 *       explaining why it did not go down that node.
		 *
		 *       When you eventually call Execute(ParseResults) with the result of this method, the above error checking
		 *       will occur. You only need to inspect it yourself if you wish to handle that yourself.
		 *
		 *       \param command a command string to parse
		 *       \param source a custom "source" object, usually representing the originator of this command
		 *       \return the result of parsing this command
		 *       \see Parse(String, Object)
		 *       \see Execute(ParseResults)
		 *       \see Execute(String, Object)
		 */
		ParseResults<S> Parse(StringReader &command, S source) {
			ParseResults<S> result(CommandContext<S>(std::move(source), root.get(), command.GetCursor()), command);
			ParseNodes(root.get(), result);
			return result;
		}

	private:
		void ParseNodes(CommandNode<S> *node, ParseResults<S> &result) {
			if (!node) return;

			S &source = result.context.GetSource();

			std::optional<ParseResults<S> > best_potential = {};
			std::optional<ParseResults<S> > current_result_ctx = {}; // delay initialization

			const int cursor = result.reader.GetCursor();

			auto [relevant_nodes, relevant_node_count] = node->GetRelevantNodes(result.reader);

			for (size_t i = 0; i < relevant_node_count; ++i) {
				auto &child = relevant_nodes[i];

				if (!child->CanUse(source)) continue;

				// initialize current context
				if (current_result_ctx.has_value()) {
					// context already exists so we have to reset it (avoid memory reallocation)
					current_result_ctx->Reset(source, result.context.GetRootNode(), result.GetContext().GetRange(), result.GetReader());
				} else {
					// create context
					current_result_ctx = ParseResults<S>(CommandContext<S>(source, result.GetContext().GetRootNode(),
						result.GetContext().GetRange()), result.GetReader());
				}

				auto &current_result = current_result_ctx.value();

				StringReader &reader = current_result.reader;
				CommandContext<S> &context = current_result.context;

				try {
					try {
						child->Parse(reader, context);
					} catch (std::runtime_error const &ex) {
						throw CommandSyntaxException::BuiltInExceptions::DispatcherParseException(reader, ex.what());
					}
					if (reader.CanRead() && reader.Peek() != ARGUMENT_SEPARATOR_CHAR)
						throw CommandSyntaxException::BuiltInExceptions::DispatcherExpectedArgumentSeparator(reader);
				} catch (CommandSyntaxException ex) {
					result.exceptions.emplace(child.get(), std::move(ex));
					reader.SetCursor(cursor);
					continue;
				}

				context.WithCommand(child->GetCommand());

				if (reader.CanRead(child->GetRedirect() == nullptr ? 2 : 1)) {
					reader.Skip();
					if (child->GetRedirect() != nullptr) {
						ParseResults<S> child_result(CommandContext<S>(source, child->GetRedirect().get(), reader.GetCursor()), reader);
						ParseNodes(child->GetRedirect().get(), child_result);
						result.context.Merge(std::move(context));
						result.context.WithChildContext(std::move(child_result.context));
						result.exceptions = std::move(child_result.exceptions);
						result.reader = std::move(child_result.reader);
						return;
					}
					ParseNodes(child.get(), current_result);
				}

				if (best_potential.has_value()) {
					if (current_result.IsBetterThan(*best_potential)) best_potential = std::move(current_result_ctx);
				} else best_potential = std::move(current_result_ctx);
			}

			if (best_potential.has_value()) {
				result.exceptions.clear();
				result.reader = std::move(best_potential->reader);
				result.context.Merge(std::move(best_potential->context));
			}
		}

	public:
		/**
		 *       Gets all possible executable commands following the given node.
		 *
		 *       You may use GetRoot() as a target to get all usage data for the entire command tree.
		 *
		 *       The returned syntax will be in "simple" form: `<param>` and `literal`. "Optional" nodes will be
		 *       listed as multiple entries: the parent node, and the child nodes.
		 *       For example, a required literal "foo" followed by an optional param "int" will be two nodes:
		 *         foo
		 *         foo <int>
		 *
		 *       The path to the specified node will NOT be prepended to the output, as there can theoretically be many
		 *       ways to reach a given node. It will only give you paths relative to the specified node, not absolute from root.
		 *
		 *       \param node target node to get child usage strings for
		 *       \param source a custom "source" object, usually representing the originator of this command
		 *       \param restricted if true, commands that the source cannot access will not be mentioned
		 *       \return array of full usage strings under the target node
		 */
		std::vector<std::wstring> GetAllUsage(CommandNode<S> *node, S source, bool restricted) {
			std::vector<std::wstring> result;
			GetAllUsage(node, std::move(source), result, {}, restricted);
			return result;
		}

	private:
		void GetAllUsage(CommandNode<S> *node, S source, std::vector<std::wstring> &result, std::wstring prefix, bool restricted) {
			if (!node) return;

			if (restricted && !node->CanUse(source)) return;

			if (node->GetCommand()) result.push_back(prefix);

			if (node->GetRedirect()) {
				if (prefix.empty()) prefix = node->GetUsageText();
				prefix = node->GetUsageText();
				prefix += ARGUMENT_SEPARATOR;
				if (node->GetRedirect() == root) prefix += L"...";
				else {
					prefix += L"-> ";
					prefix += node->GetRedirect()->GetUsageText();
				}
				result.emplace_back(std::move(prefix));
			} else if (!node->GetChildren().empty()) {
				for (auto const &[name, child]: node->GetChildren()) {
					std::wstring next_prefix = prefix;
					if (!next_prefix.empty()) next_prefix += ARGUMENT_SEPARATOR;
					next_prefix += child->GetUsageText();
					GetAllUsage(child.get(), std::move(source), result, std::move(next_prefix), restricted);
				}
			}
		}

	public:
		/**
		 *       Gets the possible executable commands from a specified node.
		 *
		 *       You may use GetRoot() as a target to get usage data for the entire command tree.
		 *
		 *       The returned syntax will be in "smart" form: `<param>`, `literal`, `[optional]` and `(either|or)`.
		 *       These forms may be mixed and matched to provide as much information about the child nodes as it can, without being too verbose.
		 *       For example, a required literal "foo" followed by an optional param "int" can be compressed into one string:
		 *           `foo [<int>]`
		 *
		 *       The path to the specified node will NOT be prepended to the output, as there can theoretically be many
		 *       ways to reach a given node. It will only give you paths relative to the specified node, not absolute from root.
		 *
		 *       The returned usage will be restricted to only commands that the provided source can use.
		 *
		 *       \param node target node to get child usage strings for
		 *       \param source a custom "source" object, usually representing the originator of this command
		 *       \return array of full usage strings under the target node
		 */
		std::map<CommandNode<S> *, std::wstring> GetSmartUsage(CommandNode<S> *node, S source) {
			std::map<CommandNode<S> *, std::wstring> result;

			for (auto const &[name, child]: node->GetChildren()) {
				if (std::wstring usage = GetSmartUsage(child.get(), std::move(source), node->GetCommand() != nullptr, false); !usage.empty())
					result[child.get()] = std::move(usage);
			}
			return result;
		}

	private:
		std::wstring GetSmartUsage(CommandNode<S> *node, S source, bool optional, bool deep) {
			if (!node) return {};

			if (!node->CanUse(source)) return {};

			std::wstring self;
			if (optional) {
				self = USAGE_OPTIONAL_OPEN;
				self += node->GetUsageText();
				self += USAGE_OPTIONAL_CLOSE;
			} else self = node->GetUsageText();
			const bool childOptional = node->GetCommand() != nullptr;
			std::wstring open = childOptional ? USAGE_OPTIONAL_OPEN : USAGE_REQUIRED_OPEN;
			std::wstring close = childOptional ? USAGE_OPTIONAL_CLOSE : USAGE_REQUIRED_CLOSE;

			if (!deep) {
				if (node->GetRedirect()) {
					self += ARGUMENT_SEPARATOR;
					if (node->GetRedirect() == root) self += L"...";
					else {
						self += L"-> ";
						self += node->GetRedirect()->GetUsageText();
					}
					return self;
				}
				std::vector<CommandNode<S> *> children;
				for (auto const &[name, child]: node->GetChildren()) if (child->CanUse(source)) children.push_back(child.get());
				if (children.size() == 1) {
					if (std::wstring usage = GetSmartUsage(children[0], source, childOptional, childOptional); !usage.empty()) {
						self += ARGUMENT_SEPARATOR;
						self += usage;
						return self;
					}
				} else if (children.size() > 1) {
					std::set<std::wstring> childUsage;
					for (auto child: children) {
						if (std::wstring usage = GetSmartUsage(child, source, childOptional, true); !usage.empty()) childUsage.insert(usage);
					}
					if (childUsage.size() == 1) {
						std::wstring usage = *childUsage.begin();
						self += ARGUMENT_SEPARATOR;
						if (childOptional) {
							self += USAGE_OPTIONAL_OPEN;
							self += usage;
							self += USAGE_OPTIONAL_CLOSE;
						} else self += usage;
						return self;
					}
					if (childUsage.size() > 1) {
						std::wstring builder(open);
						int count = 0;
						for (auto child: children) {
							if (count > 0) builder += USAGE_OR;
							builder += child->GetUsageText();
							count++;
						}
						if (count > 0) {
							self += ARGUMENT_SEPARATOR;
							self += builder;
							self += close;
							return self;
						}
					}
				}
			}

			return self;
		}

	public:
		/**
		 *       Gets suggestions for a parsed input string on what comes next.
		 *
		 *       As it is ultimately up to custom argument types to provide suggestions, it may be an asynchronous operation,
		 *       for example getting in-game data or player names etc. As such, this method returns a future and no guarantees
		 *       are made to when or how the future completes.
		 *
		 *       The suggestions provided will be in the context of the end of the parsed input string, but may suggest
		 *       new or replacement strings for earlier in the input string. For example, if the end of the string was
		 *       `foobar` but an argument preferred it to be `minecraft:foobar`, it will suggest a replacement for that
		 *       whole segment of the input.
		 *
		 *       \param parse the result of a Parse(StringReader, Object)
		 *       \param cancel a pointer to a bool that can cancel future when set to true. Result will be empty in such a case.
		 *       \return a future that will eventually resolve into a Suggestions object
		 */
		std::future<Suggestions> GetCompletionSuggestions(ParseResults<S> &parse, bool *cancel = nullptr) {
			return GetCompletionSuggestions(parse, parse.GetReader().GetTotalLength(), cancel);
		}

		/**
		 *       Gets suggestions for a parsed input string on what comes next.
		 *
		 *       As it is ultimately up to custom argument types to provide suggestions, it may be an asynchronous operation,
		 *       for example getting in-game data or player names etc. As such, this method returns a future and no guarantees
		 *       are made to when or how the future completes.
		 *
		 *       The suggestions provided will be in the context of the end of the parsed input string, but may suggest
		 *       new or replacement strings for earlier in the input string. For example, if the end of the string was
		 *       `foobar` but an argument preferred it to be `minecraft:foobar`, it will suggest a replacement for that
		 *       whole segment of the input.
		 *
		 *       \param parse the result of a Parse(StringReader, Object)
		 *       \param cursor the place where the suggestions should be considered
		 *       \param cancel a pointer to a bool that can cancel future when set to true. Result will be empty in such a case.
		 *       \return a future that will eventually resolve into a Suggestions object
		 */
		std::future<Suggestions> GetCompletionSuggestions(ParseResults<S> &parse, int cursor, bool *cancel = nullptr) {
			return std::async(std::launch::async, [](ParseResults<S> *parse, int cursor, bool *cancel) {
				auto context = parse->GetContext();

				SuggestionContext<S> nodeBeforeCursor = context.FindSuggestionContext(cursor);
				CommandNode<S> *parent = nodeBeforeCursor.parent;
				int start = std::min(nodeBeforeCursor.startPos, cursor);

				const std::wstring fullInput = parse->GetReader().GetString();
				std::wstring truncatedInput = fullInput.substr(0, cursor);
				std::wstring truncatedInputLowerCase(truncatedInput);
				std::transform(truncatedInputLowerCase.begin(), truncatedInputLowerCase.end(),
				               truncatedInputLowerCase.begin(), [](const wchar_t c) { return std::tolower(c); });

				context.WithInput(truncatedInput);

				std::vector<std::future<Suggestions> > futures;
				std::vector<SuggestionsBuilder> builders;
				const size_t max_size = parent->GetChildren().size();
				futures.reserve(max_size);
				builders.reserve(max_size);
				for (auto const &[name, node]: parent->GetChildren()) {
					try {
						builders.emplace_back(truncatedInput, truncatedInputLowerCase, start, cancel);
						futures.push_back(node->ListSuggestions(context, builders.back()));
					} catch (CommandSyntaxException const &) {}
				}

				std::vector<Suggestions> suggestions;
				for (auto &future: futures) suggestions.emplace_back(future.get());
				return Suggestions::Merge(fullInput, suggestions);
			}, &parse, cursor, cancel);
		}

		/**
		 *       Finds a valid path to a given node on the command tree.
		 *
		 *       There may theoretically be multiple paths to a node on the tree, especially with the use of forking or redirecting.
		 *       As such, this method makes no guarantees about which path it finds. It will not look at forks or redirects,
		 *       and find the first instance of the target node on the tree.
		 *
		 *       The only guarantee made is that for the same command tree and the same version of this library, the result of
		 *       this method will ALWAYS be a valid input for FindNode(Collection), which should return the same node
		 *       as provided to this method.
		 *
		 *       \param target the target node you are finding a path for
		 *       \return a path to the resulting node, or an empty list if it was not found
		 */
		std::vector<std::wstring> GetPath(CommandNode<S> *target) {
			std::vector<std::vector<CommandNode<S> *> > nodes;
			AddPaths(root.get(), nodes, {});

			for (std::vector<CommandNode<S> *> &list: nodes) {
				if (list.back() == target) {
					std::vector<std::wstring> result;
					result.reserve(list.size());
					for (auto node: list) if (node != root.get()) result.push_back(node->GetName());
					return result;
				}
			}

			return {};
		}

		/**
		 *       Finds a node by its path
		 *
		 *       Paths may be generated with GetPath(CommandNode), and are guaranteed (for the same tree, and the
		 *       same version of this library) to always produce the same valid node by this method.
		 *
		 *       If a node could not be found at the specified path, then nullptr will be returned.
		 *
		 *       \param path a generated path to a node
		 *       \return the node at the given path, or null if not found
		 */
		CommandNode<S> *FindNode(std::vector<std::wstring> const &path) {
			CommandNode<S> *node = root.get();
			for (auto &name: path) {
				node = node->GetChild(name);
				if (node == nullptr) return nullptr;
			}
			return node;
		}

	public:
		/**
		 *       Scans the command tree for potential ambiguous commands.
		 *
		 *       This is a shortcut for CommandNode::FindAmbiguities(AmbiguityConsumer) on GetRoot().
		 *
		 *       Ambiguities are detected by testing every CommandNode::GetExamples() on one node verses every sibling
		 *       node. This is not fool proof, and relies a lot on the providers of the used argument types to give good examples.
		 *
		 *       \param consumer a callback to be notified of potential ambiguities
		 */
		void FindAmbiguities(AmbiguityConsumer<S> consumer) {
			if (!consumer) return;
			root->FindAmbiguities(consumer);
		}

	private:
		void AddPaths(CommandNode<S> *node, std::vector<std::vector<CommandNode<S> *> > &result, std::vector<CommandNode<S> *> parents) {
			parents.push_back(node);
			result.push_back(parents);

			for (auto const &[name, child]: node->GetChildren()) AddPaths(child.get(), result, parents);
		}

		std::shared_ptr<RootCommandNode<S> > root;
		ResultConsumer<S> consumer = [](CommandContext<S> &context, bool success, int result) {
		};
	};
}
