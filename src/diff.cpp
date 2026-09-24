#include "diff.hpp"
#include <sstream>

std::vector<DiffLine> parse_patch(const std::string& patch) {
    std::vector<DiffLine> lines;
    std::istringstream ss(patch);
    std::string line;
    while (std::getline(ss, line)) {
        DiffLine dl;
        if (!line.empty() && line[0] == '+' && !(line.size() > 1 && line[1] == '+' && line[2] == '+'))
            dl.type = DiffLine::Type::Added;
        else if (!line.empty() && line[0] == '-' && !(line.size() > 1 && line[1] == '-' && line[2] == '-'))
            dl.type = DiffLine::Type::Removed;
        else if (line.size() > 1 && (line[0] == '@' || line.substr(0,3) == "---" || line.substr(0,3) == "+++"))
            dl.type = DiffLine::Type::Header;
        else
            dl.type = DiffLine::Type::Context;
        dl.text = line;
        lines.push_back(std::move(dl));
    }
    return lines;
}

std::string render_file_summary(const FileDiff& fd) {
    return "+" + std::to_string(fd.additions) +
           " -" + std::to_string(fd.deletions) +
           "  " + fd.path;
}
