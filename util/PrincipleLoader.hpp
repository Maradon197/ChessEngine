#ifndef PRINCIPLELOADER_HPP
#define PRINCIPLELOADER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "Principle.hpp"

struct PrincipleFile {
	std::vector<Principle> principles;
	std::vector<std::string> problems;	//syntax errors, skipped entries and ignored fields, readable for the user
};

//reads the principle JSON file, the layout is documented in data/principles.json
//broken entries are skipped and reported, the rest is still loaded
namespace PrincipleLoader {
	PrincipleFile load(const std::filesystem::path& file);
}

#endif
