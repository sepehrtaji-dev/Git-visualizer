#pragma once
#include "repo.hpp"
#include <string>
#include <vector>

struct DiffLine {
    enum class Type { Context, Added, Removed, Header };
    Type        type;
    std::string text;
};

// Parse a raw unified diff patch string into structured lines
std::vector<DiffLine> parse_patch(const std::string& patch);

// Render a FileDiff summary line: "+12 -3  src/foo.cpp"
std::string render_file_summary(const FileDiff& fd);
