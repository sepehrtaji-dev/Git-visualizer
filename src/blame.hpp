#pragma once
#include "repo.hpp"
#include <string>
#include <vector>

// Render a single blame line:
// "a1b2c3d  2024-01-15  AuthorName  │  actual code line"
std::string render_blame_line(const BlameHunk& hunk, const std::string& line);
