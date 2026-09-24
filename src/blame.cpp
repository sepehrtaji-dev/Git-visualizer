#include "blame.hpp"
#include <sstream>
#include <iomanip>

std::string render_blame_line(const BlameHunk& hunk, const std::string& line) {
    std::ostringstream oss;
    oss << std::left
        << std::setw(8)  << hunk.commit_oid << "  "
        << std::setw(16) << hunk.date.substr(0, 10) << "  "
        << std::setw(16) << hunk.author.substr(0, 15) << "  │  "
        << line;
    return oss.str();
}
