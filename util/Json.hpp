#ifndef JSON_HPP
#define JSON_HPP

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

//minimal JSON DOM, strict RFC 8259 (no comments, no trailing commas), strings are UTF-8
struct JsonValue {
	enum class Type {NUL, BOOLEAN, NUMBER, STRING, ARRAY, OBJECT};

	Type type = Type::NUL;
	bool boolean = false;
	double number = 0;
	std::string string;
	std::vector<JsonValue> array;
	std::vector<std::pair<std::string, JsonValue>> object;	//file order, keys are unique

	bool isNull() const { return type == Type::NUL; }
	bool isBool() const { return type == Type::BOOLEAN; }
	bool isNumber() const { return type == Type::NUMBER; }
	bool isString() const { return type == Type::STRING; }
	bool isArray() const { return type == Type::ARRAY; }
	bool isObject() const { return type == Type::OBJECT; }

	const JsonValue* find(std::string_view key) const;	//nullptr if missing or not an object
};

struct JsonError {
	std::string message;
	int line = 0;	//1-based
	int column = 0;	//1-based, in bytes
};

namespace Json {
	std::optional<JsonValue> parse(std::string_view text, JsonError* error = nullptr);
	const char* getTypeName(JsonValue::Type type);
}

#endif
