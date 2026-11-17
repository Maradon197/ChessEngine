#include "PrincipleLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "Json.hpp"

namespace {

	constexpr int MAX_SPOT_ELO = 4000;

	//names as written in the file, in enum order
	constexpr std::array<std::string_view, static_cast<std::size_t>(GamePart::COUNT)> GAME_PART_NAMES = {
		"opening", "midgame", "attacking midgame", "defending midgame", "endgame"
	};
	constexpr std::array<std::string_view, static_cast<std::size_t>(PrincipleFocus::COUNT)> FOCUS_NAMES = {
		"pawn", "knight", "bishop", "rook", "queen", "king", "space", "strategy", "coordination"
	};
	constexpr std::array<std::string_view, 9> KNOWN_FIELDS = {
		"title", "spotElo", "description", "gamePart", "focus", "explanation", "image", "enabled", "useful"
	};

	//"Attacking-Midgame", "attacking_midgame" and "attacking midgame" all become "attackingmidgame"
	std::string normalize(std::string_view text) {
		std::string result;
		for (char character : text) {
			if (character == ' ' || character == '-' || character == '_')
				continue;
			result += (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a') : character;
		}
		return result;
	}

	template <typename Enum, std::size_t COUNT>
	std::optional<Enum> findName(const std::array<std::string_view, COUNT>& names, std::string_view text) {
		const std::string wanted = normalize(text);
		for (std::size_t index = 0; index < COUNT; ++index)
			if (normalize(names[index]) == wanted)
				return static_cast<Enum>(index);
		return std::nullopt;
	}

	template <std::size_t COUNT>
	std::string listNames(const std::array<std::string_view, COUNT>& names) {
		std::string result;
		for (std::string_view name : names)
			result += (result.empty() ? "" : ", ") + std::string(name);
		return result;
	}

	std::string toUtf8(const std::filesystem::path& path) {
		const std::u8string text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	std::filesystem::path fromUtf8(const std::string& text) {
		return std::filesystem::path(std::u8string(text.begin(), text.end()));
	}

	class EntryReader {
	public:
		EntryReader(const JsonValue& entry, std::size_t index, std::vector<std::string>& problems)
			: entry(entry), problems(problems), label("principle " + std::to_string(index + 1)) {}

		void setTitle(const std::string& title) {
			label += " \"" + title + "\"";
		}

		void report(const std::string& text) const {
			problems.push_back(label + ": " + text);
		}

		//nullptr if missing; reports a wrong type
		const JsonValue* get(std::string_view key, JsonValue::Type type, bool required) const {
			const JsonValue* value = entry.find(key);
			if (value == nullptr) {
				if (required)
					report("\"" + std::string(key) + "\" is missing");
				return nullptr;
			}
			if (value->type != type) {
				report("\"" + std::string(key) + "\" must be " + Json::getTypeName(type) + ", not " + Json::getTypeName(value->type));
				return nullptr;
			}
			return value;
		}

		const JsonValue& entry;

	private:
		std::vector<std::string>& problems;
		std::string label;
	};

	std::optional<Principle> readPrinciple(const JsonValue& entry, std::size_t index, const std::filesystem::path& baseDirectory,
		std::unordered_set<std::string>& titles, std::vector<std::string>& problems) {
		EntryReader reader(entry, index, problems);
		if (!entry.isObject()) {
			reader.report("must be an object { ... }, skipped");
			return std::nullopt;
		}

		Principle principle;
		const JsonValue* title = reader.get("title", JsonValue::Type::STRING, true);
		if (title == nullptr || title->string.empty()) {
			if (title != nullptr)
				reader.report("\"title\" is empty");
			reader.report("skipped");
			return std::nullopt;
		}
		principle.title = title->string;
		reader.setTitle(principle.title);
		if (!titles.insert(principle.title).second) {
			reader.report("another principle already has this title, skipped");
			return std::nullopt;
		}

		bool valid = true;
		if (const JsonValue* spotElo = reader.get("spotElo", JsonValue::Type::NUMBER, true)) {
			if (spotElo->number != std::floor(spotElo->number) || spotElo->number < 0 || spotElo->number > MAX_SPOT_ELO) {
				reader.report("\"spotElo\" must be a whole number from 0 to " + std::to_string(MAX_SPOT_ELO));
				valid = false;
			}
			principle.spotElo = static_cast<int>(spotElo->number);
		} else {
			valid = false;
		}

		if (const JsonValue* description = reader.get("description", JsonValue::Type::STRING, true))
			principle.description = description->string;
		else
			valid = false;

		if (const JsonValue* gamePart = reader.get("gamePart", JsonValue::Type::STRING, true)) {
			const std::optional<GamePart> part = findName<GamePart>(GAME_PART_NAMES, gamePart->string);
			if (part)
				principle.gamePart = *part;
			else {
				reader.report("unknown \"gamePart\" \"" + gamePart->string + "\", use one of: " + listNames(GAME_PART_NAMES));
				valid = false;
			}
		} else {
			valid = false;
		}

		if (const JsonValue* focus = reader.get("focus", JsonValue::Type::STRING, true)) {
			const std::optional<PrincipleFocus> parsed = findName<PrincipleFocus>(FOCUS_NAMES, focus->string);
			if (parsed)
				principle.focus = *parsed;
			else {
				reader.report("unknown \"focus\" \"" + focus->string + "\", use one of: " + listNames(FOCUS_NAMES));
				valid = false;
			}
		} else {
			valid = false;
		}

		if (!valid) {
			reader.report("skipped");
			return std::nullopt;
		}

		//optional fields: a broken one is ignored, the principle is still loaded
		if (const JsonValue* explanation = entry.find("explanation")) {
			if (explanation->isString()) {
				principle.explanation = explanation->string;
			} else if (explanation->isArray()) {	//list of lines, easier to write by hand
				for (const JsonValue& line : explanation->array) {
					if (!line.isString()) {
						reader.report("every line in \"explanation\" must be a text, ignored the explanation");
						principle.explanation.clear();
						break;
					}
					principle.explanation += (principle.explanation.empty() ? "" : "\n") + line.string;
				}
			} else {
				reader.report("\"explanation\" must be a text or a list of texts, ignored");
			}
		}

		if (const JsonValue* image = reader.get("image", JsonValue::Type::STRING, false); image != nullptr && !image->string.empty()) {
			std::filesystem::path path = fromUtf8(image->string);
			if (path.is_relative())
				path = baseDirectory / path;
			std::error_code error;
			if (std::filesystem::is_regular_file(path, error))
				principle.imagePath = toUtf8(path.lexically_normal());
			else
				reader.report("image \"" + image->string + "\" not found, using the placeholder");
		}

		if (const JsonValue* enabled = reader.get("enabled", JsonValue::Type::BOOLEAN, false))
			principle.enabled = enabled->boolean;

		for (const auto& [key, value] : entry.object) {
			if (key.empty() || key[0] == '_' || std::find(KNOWN_FIELDS.begin(), KNOWN_FIELDS.end(), key) != KNOWN_FIELDS.end())
				continue;
			if (key == "difficulty")
				reader.report("\"difficulty\" is ignored, it follows from \"spotElo\"");
			else
				reader.report("unknown field \"" + key + "\" ignored");
		}
		if (entry.find("useful") != nullptr)
			reader.report("\"useful\" is ignored, the engine decides it");

		return principle;
	}

}

PrincipleFile PrincipleLoader::load(const std::filesystem::path& file) {
	PrincipleFile result;
	const std::string fileName = toUtf8(file.filename());

	std::ifstream stream(file, std::ios::binary);
	if (!stream) {
		result.problems.push_back("could not open " + toUtf8(file));
		return result;
	}
	const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

	JsonError error;
	const std::optional<JsonValue> document = Json::parse(text, &error);
	if (!document) {
		result.problems.push_back(fileName + " line " + std::to_string(error.line) + ", column " + std::to_string(error.column) + ": " + error.message);
		return result;
	}

	const JsonValue* list = document->find("principles");
	if (list == nullptr || !list->isArray()) {
		result.problems.push_back(fileName + ": needs an object with a \"principles\" list");
		return result;
	}

	std::unordered_set<std::string> titles;
	const std::filesystem::path baseDirectory = file.parent_path();
	for (std::size_t index = 0; index < list->array.size(); ++index)
		if (std::optional<Principle> principle = readPrinciple(list->array[index], index, baseDirectory, titles, result.problems))
			result.principles.push_back(std::move(*principle));
	return result;
}
