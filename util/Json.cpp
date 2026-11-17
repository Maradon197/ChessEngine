#include "Json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>

namespace {

	constexpr int MAX_DEPTH = 256;

	struct ParseFailure {
		std::string message;
		std::size_t offset;
	};

	class Parser {
	public:
		explicit Parser(std::string_view text) : text(text) {}

		JsonValue parseDocument() {
			if (text.substr(0, 3) == "\xEF\xBB\xBF")	//UTF-8 BOM
				position = 3;
			skipWhitespace();
			JsonValue value = parseValue(0);
			skipWhitespace();
			if (position < text.size())
				fail("unexpected content after the end of the document");
			return value;
		}

	private:
		[[noreturn]] void fail(const std::string& message) const {
			throw ParseFailure{ message, position };
		}

		bool atEnd() const { return position >= text.size(); }
		char peek() const { return atEnd() ? '\0' : text[position]; }

		void expect(char character, const char* what) {
			if (peek() != character)
				fail(std::string("expected ") + what);
			++position;
		}

		void skipWhitespace() {
			while (!atEnd() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\n' || text[position] == '\r'))
				++position;
		}

		JsonValue parseValue(int depth) {
			if (depth > MAX_DEPTH)
				fail("nested too deeply");
			switch (peek()) {
				case '{': return parseObject(depth);
				case '[': return parseArray(depth);
				case '"': {
					JsonValue value;
					value.type = JsonValue::Type::STRING;
					value.string = parseString();
					return value;
				}
				case 't': return parseLiteral("true", JsonValue::Type::BOOLEAN, true);
				case 'f': return parseLiteral("false", JsonValue::Type::BOOLEAN, false);
				case 'n': return parseLiteral("null", JsonValue::Type::NUL, false);
				default:
					if (peek() == '-' || (peek() >= '0' && peek() <= '9'))
						return parseNumber();
					if (atEnd())
						fail("unexpected end of the file, expected a value");
					fail("expected a value (text in double quotes, number, true, false, null, [ or {)");
			}
		}

		JsonValue parseLiteral(std::string_view literal, JsonValue::Type type, bool boolean) {
			if (text.substr(position, literal.size()) != literal)
				fail("unknown word, did you forget the double quotes around a text?");
			position += literal.size();
			JsonValue value;
			value.type = type;
			value.boolean = boolean;
			return value;
		}

		JsonValue parseObject(int depth) {
			JsonValue value;
			value.type = JsonValue::Type::OBJECT;
			++position;	//{
			skipWhitespace();
			if (peek() == '}') {
				++position;
				return value;
			}
			while (true) {
				if (peek() != '"')
					fail("expected a key in double quotes");
				const std::size_t keyStart = position;
				std::string key = parseString();
				const bool duplicate = std::any_of(value.object.begin(), value.object.end(),
					[&key](const auto& member) { return member.first == key; });
				if (duplicate) {
					position = keyStart;
					fail("duplicate key \"" + key + "\"");
				}
				skipWhitespace();
				expect(':', "':' after the key");
				skipWhitespace();
				JsonValue member = parseValue(depth + 1);
				value.object.emplace_back(std::move(key), std::move(member));
				skipWhitespace();
				if (peek() == ',') {
					++position;
					skipWhitespace();
					if (peek() == '}')
						fail("trailing comma before '}'");
					continue;
				}
				expect('}', "',' or '}' after the value");
				return value;
			}
		}

		JsonValue parseArray(int depth) {
			JsonValue value;
			value.type = JsonValue::Type::ARRAY;
			++position;	//[
			skipWhitespace();
			if (peek() == ']') {
				++position;
				return value;
			}
			while (true) {
				value.array.push_back(parseValue(depth + 1));
				skipWhitespace();
				if (peek() == ',') {
					++position;
					skipWhitespace();
					if (peek() == ']')
						fail("trailing comma before ']'");
					continue;
				}
				expect(']', "',' or ']' after the value");
				return value;
			}
		}

		std::string parseString() {
			++position;	//opening quote
			std::string result;
			while (true) {
				if (atEnd())
					fail("text is missing its closing double quote");
				const char character = text[position];
				if (character == '"') {
					++position;
					return result;
				}
				if (static_cast<unsigned char>(character) < 0x20)
					fail(character == '\n' ? "line break inside a text, write \\n instead" : "control character inside a text");
				if (character != '\\') {
					result += character;
					++position;
					continue;
				}

				++position;
				switch (peek()) {
					case '"': result += '"'; break;
					case '\\': result += '\\'; break;
					case '/': result += '/'; break;
					case 'b': result += '\b'; break;
					case 'f': result += '\f'; break;
					case 'n': result += '\n'; break;
					case 'r': result += '\r'; break;
					case 't': result += '\t'; break;
					case 'u': {
						++position;
						std::uint32_t codePoint = parseHex4();
						if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {	//high surrogate, needs a low one
							if (text.substr(position, 2) != "\\u")
								fail("unpaired surrogate in \\u escape");
							position += 2;
							const std::uint32_t low = parseHex4();
							if (low < 0xDC00 || low > 0xDFFF)
								fail("invalid low surrogate in \\u escape");
							codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
						} else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
							fail("unpaired surrogate in \\u escape");
						}
						appendUtf8(result, codePoint);
						continue;	//parseHex4 already advanced
					}
					default:
						fail("unknown escape sequence, use \\\\ for a backslash");
				}
				++position;
			}
		}

		std::uint32_t parseHex4() {
			if (position + 4 > text.size())
				fail("\\u needs four hex digits");
			std::uint32_t value = 0;
			for (int index = 0; index < 4; ++index) {
				const char digit = text[position++];
				value <<= 4;
				if (digit >= '0' && digit <= '9') value |= digit - '0';
				else if (digit >= 'a' && digit <= 'f') value |= digit - 'a' + 10;
				else if (digit >= 'A' && digit <= 'F') value |= digit - 'A' + 10;
				else {
					--position;
					fail("\\u needs four hex digits");
				}
			}
			return value;
		}

		static void appendUtf8(std::string& out, std::uint32_t codePoint) {
			if (codePoint < 0x80) {
				out += static_cast<char>(codePoint);
			} else if (codePoint < 0x800) {
				out += static_cast<char>(0xC0 | (codePoint >> 6));
				out += static_cast<char>(0x80 | (codePoint & 0x3F));
			} else if (codePoint < 0x10000) {
				out += static_cast<char>(0xE0 | (codePoint >> 12));
				out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (codePoint & 0x3F));
			} else {
				out += static_cast<char>(0xF0 | (codePoint >> 18));
				out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
				out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (codePoint & 0x3F));
			}
		}

		JsonValue parseNumber() {
			const std::size_t start = position;
			const auto skipDigits = [this] {
				const std::size_t before = position;
				while (!atEnd() && text[position] >= '0' && text[position] <= '9')
					++position;
				return position > before;
			};

			if (peek() == '-')
				++position;
			if (peek() == '0') {
				++position;
				if (peek() >= '0' && peek() <= '9')
					fail("numbers can't have leading zeros");
			} else if (!skipDigits()) {
				fail("expected a digit");
			}
			if (peek() == '.') {
				++position;
				if (!skipDigits())
					fail("expected a digit after the decimal point");
			}
			if (peek() == 'e' || peek() == 'E') {
				++position;
				if (peek() == '+' || peek() == '-')
					++position;
				if (!skipDigits())
					fail("expected a digit in the exponent");
			}

			JsonValue value;
			value.type = JsonValue::Type::NUMBER;
			const auto [end, error] = std::from_chars(text.data() + start, text.data() + position, value.number);
			if (error != std::errc{} || end != text.data() + position) {
				position = start;
				fail("number out of range");
			}
			return value;
		}

		std::string_view text;
		std::size_t position = 0;
	};

}

const JsonValue* JsonValue::find(std::string_view key) const {
	for (const auto& [name, value] : object)
		if (name == key)
			return &value;
	return nullptr;
}

std::optional<JsonValue> Json::parse(std::string_view text, JsonError* error) {
	try {
		return Parser(text).parseDocument();
	} catch (const ParseFailure& failure) {
		if (error != nullptr) {
			const std::size_t offset = std::min(failure.offset, text.size());
			const std::string_view before = text.substr(0, offset);
			const std::size_t lineStart = before.rfind('\n');
			error->message = failure.message;
			error->line = 1 + static_cast<int>(std::count(before.begin(), before.end(), '\n'));
			error->column = 1 + static_cast<int>(lineStart == std::string_view::npos ? offset : offset - lineStart - 1);
		}
		return std::nullopt;
	}
}

const char* Json::getTypeName(JsonValue::Type type) {
	switch (type) {
		case JsonValue::Type::NUL: return "null";
		case JsonValue::Type::BOOLEAN: return "true/false";
		case JsonValue::Type::NUMBER: return "a number";
		case JsonValue::Type::STRING: return "a text";
		case JsonValue::Type::ARRAY: return "a list";
		case JsonValue::Type::OBJECT: return "an object";
		default: return "?";
	}
}
