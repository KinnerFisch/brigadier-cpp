/*
 * MIT License
 *
 * Copyright (c) 2022 Tomasz Karpi
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

#include "brigadier.hpp"

namespace brigadier {
	StringReader::StringReader(std::wstring wstring) : string(std::move(wstring)) {}
	std::wstring StringReader::GetString() const { return string; }
	void StringReader::SetCursor(const int cursor) { this->cursor = cursor; }
	int StringReader::GetRemainingLength() const { return static_cast<int>(string.length() - cursor); }
	int StringReader::GetTotalLength() const { return static_cast<int>(string.length()); }
	int StringReader::GetCursor() const { return cursor; }
	std::wstring StringReader::GetRead() const { return string.substr(0, cursor); }
	std::wstring StringReader::GetRemaining() const { return string.substr(cursor); }
	bool StringReader::CanRead(const int length) const { return static_cast<size_t>(cursor) + length <= string.length(); }
	bool StringReader::CanRead() const { return CanRead(1); }
	wchar_t StringReader::Peek() const { return Peek(0); }
	wchar_t StringReader::Peek(const int offset) const { return string.at(cursor + offset); }
	wchar_t StringReader::Read() { return string.at(cursor++); }
	void StringReader::Skip() { cursor++; }
	bool StringReader::IsQuotedStringStart(wchar_t c) { return c == SYNTAX_DOUBLE_QUOTE || c == SYNTAX_SINGLE_QUOTE; }
	void StringReader::SkipWhitespace() { while (CanRead() && Peek() == L' ') Skip(); }
	bool StringReader::IsAllowedInUnquotedString(const wchar_t c) {
		return (c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c == L'_' || c == L'-') || (c == L'.' || c == L'+');
	}
	bool StringReader::IsAllowedNumberImpl(const wchar_t c, const bool allow_float, const bool allow_negative) {
		return (c >= L'0' && c <= L'9') || (allow_float && c == L'.') || (allow_negative && c == L'-');
	}
	ExceptionContext::ExceptionContext(StringReader const &reader) : input(reader.GetString()), cursor(reader.GetCursor()) {}
	std::wstring const &CommandSyntaxException::What() const { return msg; }
	int CommandSyntaxException::GetCursor() const { return ctx.cursor; }
	std::wstring CommandSyntaxException::GetInput() const { return ctx.input; }
	bool CompareNoCase::operator()(Suggestion const &a, Suggestion const &b) const {
#ifdef __unix__
		return wcscasecmp(a.GetText().c_str(), b.GetText().c_str()) < 0;
#else
		return _wcsicmp(a.GetText().c_str(), b.GetText().c_str()) < 0;
#endif
	}
	Suggestion::Suggestion(const StringRange range, std::wstring text, std::wstring tooltip) : range(range), text(std::move(text)), tooltip(std::move(tooltip)) {}
	Suggestion::Suggestion(const StringRange range, std::wstring text) : range(range), text(std::move(text)) {}
	Suggestion::Suggestion(std::wstring text, const StringRange range, std::wstring tooltip) : range(range), text(std::move(text)), tooltip(std::move(tooltip)) {}
	Suggestion::Suggestion(std::wstring text, const StringRange range) : range(range), text(std::move(text)) {}
	Suggestions::Suggestions(const StringRange range, std::set<Suggestion, CompareNoCase> suggestions) : range(range), suggestions(std::move(suggestions)) {}
	Suggestions::Suggestions() : Suggestions(StringRange::At(0), {}) {}
	SuggestionsBuilder::SuggestionsBuilder(const std::wstring& input, const std::wstring &inputLowerCase, const int start, bool *cancel) :
		start(start), input(input), inputLowerCase(inputLowerCase), remaining(input.substr(start)),
		remainingLowerCase(inputLowerCase.substr(start)), cancel(cancel) {}
	namespace detail {
		std::wstring EscapeStringArgument(const std::wstring& input) {
			std::wstring result = L"\"";
			for (const auto c: input) {
				if (c == L'\\' || c == L'\"') result += L'\\';
				result += c;
			}
			result += L'\"';
			return result;
		}
		std::wstring EscapeStringArgumentIfRequired(const std::wstring& input) {
			for (const auto c: input) if (!StringReader::IsAllowedInUnquotedString(c)) return EscapeStringArgument(input);
			return { input };
		}
	}
	// ============================================================
	// StringReader
	// ============================================================

	std::wstring StringReader::ReadUnquotedString() {
		const int start = cursor;
		while (CanRead() && IsAllowedInUnquotedString(Peek())) Skip();
		return string.substr(start, cursor - start);
	}

	std::wstring StringReader::ReadQuotedString() {
		if (!CanRead()) return {};
		const wchar_t next = Peek();
		if (!IsQuotedStringStart(next)) throw CommandSyntaxException::BuiltInExceptions::ReaderExpectedStartOfQuote(*this);
		Skip();
		return ReadStringUntil(next);
	}

	std::wstring StringReader::ReadStringUntil(wchar_t terminator) {
		std::wstring result;
		result.reserve(GetRemainingLength());

		bool escaped = false;
		while (CanRead()) {
			const wchar_t c = Read();
			if (escaped) {
				if (c == terminator || c == SYNTAX_ESCAPE) {
					result += c;
					escaped = false;
				} else {
					SetCursor(GetCursor() - 1);
					throw CommandSyntaxException::BuiltInExceptions::ReaderInvalidEscape(*this, c);
				}
			} else if (c == SYNTAX_ESCAPE) escaped = true;
			else if (c == terminator) return result;
			else result += c;
		}

		throw CommandSyntaxException::BuiltInExceptions::ReaderExpectedEndOfQuote(*this);
	}

	std::wstring StringReader::ReadString() {
		if (!CanRead()) return {};
		const wchar_t next = Peek();
		if (IsQuotedStringStart(next)) {
			Skip();
			return ReadStringUntil(next);
		}
		return std::wstring(ReadUnquotedString());
	}

	void StringReader::Expect(const wchar_t c) {
		if (!CanRead() || Peek() != c) throw CommandSyntaxException::BuiltInExceptions::ReaderExpectedSymbol(*this, c);
		Skip();
	}

	// ============================================================
	// BuiltInExceptionProvider (non-template only)
	// ============================================================

	CommandSyntaxException BuiltInExceptionProvider::ReaderExpectedStartOfQuote(const ExceptionContext &ctx) {
		return { ctx, L"Expected quote to start a string" };
	}

	CommandSyntaxException BuiltInExceptionProvider::ReaderExpectedEndOfQuote(const ExceptionContext &ctx) {
		return { ctx, L"Unclosed quoted string" };
	}

	CommandSyntaxException BuiltInExceptionProvider::ReaderExpectedValue(const ExceptionContext &ctx) {
		return { ctx, L"Expected value" };
	}

	CommandSyntaxException BuiltInExceptionProvider::DispatcherUnknownCommand(const ExceptionContext &ctx) {
		return { ctx, L"Unknown command" };
	}

	CommandSyntaxException BuiltInExceptionProvider::DispatcherUnknownArgument(const ExceptionContext &ctx) {
		return { ctx, L"Incorrect argument for command" };
	}

	CommandSyntaxException BuiltInExceptionProvider::DispatcherExpectedArgumentSeparator(const ExceptionContext &ctx) {
		return { ctx, L"Expected whitespace to end one argument, but found trailing data" };
	}

	// ============================================================
	// StringRange
	// ============================================================

	std::wstring StringRange::Get(const StringReader& reader) const { return reader.GetString().substr(start, end - start); }

	std::wstring StringRange::Get(const std::wstring& string) const { return string.substr(start, end - start); }

	// ============================================================
	// Suggestion
	// ============================================================

	std::wstring Suggestion::Apply(const std::wstring& input) const {
		if (range.GetStart() == 0 && range.GetEnd() == input.length()) return text;
		std::wstring result;
		result.reserve(range.GetStart() + text.length() + input.length() - (std::min)(range.GetEnd(), static_cast<int>(input.length())));
		if (range.GetStart() > 0) result.append(input.substr(0, range.GetStart()));
		result.append(text);
		if (static_cast<size_t>(range.GetEnd()) < input.length()) result.append(input.substr(range.GetEnd()));
		return result;
	}

	void Suggestion::Expand(const std::wstring& command, const StringRange range) {
		if (this->range == range) return;

		if (range.GetStart() < this->range.GetStart()) text.insert(0, command.substr(range.GetStart(), this->range.GetStart() - range.GetStart()));
		if (range.GetEnd() > this->range.GetEnd()) text.append(command.substr(this->range.GetEnd(), range.GetEnd() - this->range.GetEnd()));

		this->range = range;
	}

	// ============================================================
	// Suggestions
	// ============================================================

	std::future<Suggestions> Suggestions::Empty() {
		std::promise<Suggestions> f;
		f.set_value(Suggestions());
		return f.get_future();
	}

	Suggestions Suggestions::Merge(const std::wstring &command, std::vector<Suggestions> const &input, const bool *cancel) {
		if (input.empty()) return {};
		if (input.size() == 1) return input.front();

		std::vector<Suggestion> suggestions;
		for (auto &sugs: input) suggestions.insert(suggestions.end(), sugs.GetList().begin(), sugs.GetList().end());
		return Create(command, suggestions, cancel);
	}

	Suggestions Suggestions::Create(const std::wstring &command, std::vector<Suggestion> &suggestions, const bool *cancel) {
		if (suggestions.empty()) return {};
		int start = std::numeric_limits<int>::max();
		int end = std::numeric_limits<int>::min();
		for (auto &suggestion: suggestions) {
			if (cancel && *cancel) return {};
			start = (std::min)(suggestion.GetRange().GetStart(), start);
			end = (std::max)(suggestion.GetRange().GetEnd(), end);
		}
		std::set<Suggestion, CompareNoCase> suggest;
		const auto range = StringRange(start, end);
		for (auto &suggestion: suggestions) {
			if (cancel && *cancel) return {};
			suggestion.Expand(command, range);
		}
		for (auto &suggestion: suggestions) {
			if (cancel && *cancel) return { range, std::move(suggest) };
			suggest.insert(std::move(suggestion));
		}
		return { range, std::move(suggest) };
	}

	// ============================================================
	// SuggestionsBuilder (non-template only)
	// ============================================================

	Suggestions SuggestionsBuilder::Build(bool *cancel) {
		auto ret = Suggestions::Create(input, result, cancel);
		if (cancel != nullptr) *cancel = false;
		result.clear();
		return ret;
	}

	std::future<Suggestions> SuggestionsBuilder::BuildFuture() {
		return std::async(std::launch::async, &SuggestionsBuilder::Build, this, this->cancel);
	}

	SuggestionsBuilder &SuggestionsBuilder::Suggest(const std::wstring& text) {
		if (text == remaining) return *this;
		result.emplace_back(StringRange::Between(start, static_cast<int>(input.length())), text);
		return *this;
	}

	SuggestionsBuilder &SuggestionsBuilder::Suggest(const std::wstring& text, const std::wstring& tooltip) {
		if (text == remaining) return *this;
		result.emplace_back(StringRange::Between(start, static_cast<int>(input.length())), text, tooltip);
		return *this;
	}

	int SuggestionsBuilder::AutoSuggest(const std::wstring& text, const std::wstring& input) {
		if (text.compare(0, (std::min)(input.length(), text.length()), input, 0, (std::min)(input.length(), text.length())) == 0) {
			Suggest(text);
			return 1;
		}
		return 0;
	}

	int SuggestionsBuilder::AutoSuggest(const std::wstring& text, const std::wstring& tooltip, const std::wstring& input) {
		if (text.compare(0, (std::min)(input.length(), text.length()), input, 0, (std::min)(input.length(), text.length())) == 0) {
			Suggest(text, tooltip);
			return 1;
		}
		return 0;
	}

	SuggestionsBuilder &SuggestionsBuilder::Add(SuggestionsBuilder const &other) {
		result.insert(result.end(), other.result.begin(), other.result.end());
		return *this;
	}

	void SuggestionsBuilder::SetOffset(const int start) {
		this->start = start;
		remaining = input.substr(start);
		remainingLowerCase = inputLowerCase.substr(start);
		Restart();
	}

	void SuggestionsBuilder::Restart() { result.clear(); }

	// ============================================================
	// CharArgumentType
	// ============================================================

	wchar_t CharArgumentType::Parse(StringReader &reader) {
		if (reader.CanRead()) return reader.Read();
		throw CommandSyntaxException::BuiltInExceptions::ReaderExpectedValue(reader);
	}
} // namespace brigadier