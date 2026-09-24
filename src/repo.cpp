#include "repo.hpp"
#include <stdexcept>
#include <sstream>
#include <array>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
  #define POPEN  _popen
  #define PCLOSE _pclose
#else
  #define POPEN  popen
  #define PCLOSE pclose
#endif

// ── run() — execute a git command and return stdout ───────────────────────────
std::string Repo::run(const std::string& cmd) const {
    // cd into repo path first
    std::string full = "git -C \"" + path_ + "\" " + cmd + " 2>nul";
#ifndef _WIN32
    full = "git -C \"" + path_ + "\" " + cmd + " 2>/dev/null";
#endif

    FILE* pipe = POPEN(full.c_str(), "r");
    if (!pipe) return "";

    std::string result;
    std::array<char, 512> buf;
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();

    PCLOSE(pipe);
    return result;
}

// ── Ctor ──────────────────────────────────────────────────────────────────────
Repo::Repo(const std::string& path) : path_(path) {
    std::string check = run("rev-parse --git-dir");
    if (check.empty())
        throw std::runtime_error("Not a git repository: " + path);
}

// ── split helper ──────────────────────────────────────────────────────────────
static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        parts.push_back(token);
    return parts;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// ── log() ─────────────────────────────────────────────────────────────────────
std::vector<Commit> Repo::log(int max_commits) const {
    // Format: OID|PARENTS|AUTHOR|EMAIL|DATE|REFS|MESSAGE
    std::string fmt = "--pretty=format:%H|%P|%an|%ae|%ai|%D|%s";
    std::string out = run("log --all " + fmt +
                          " -n " + std::to_string(max_commits));

    std::vector<Commit> commits;
    auto lines = split(out, '\n');

    for (auto& line : lines) {
        if (trim(line).empty()) continue;
        auto parts = split(line, '|');
        if (parts.size() < 7) continue;

        Commit c;
        c.oid       = trim(parts[0]);
        c.short_oid = c.oid.size() >= 7 ? c.oid.substr(0, 7) : c.oid;
        c.author    = trim(parts[2]);
        c.email     = trim(parts[3]);
        c.date      = trim(parts[4]).substr(0, 16); // "2024-01-15 10:30"
        c.message   = trim(parts[6]);

        // Parents
        std::string pstr = trim(parts[1]);
        if (!pstr.empty()) {
            for (auto& p : split(pstr, ' '))
                if (!trim(p).empty()) c.parent_oids.push_back(trim(p));
        }

        // Refs (branch/tag decorations)
        std::string refstr = trim(parts[5]);
        if (!refstr.empty()) {
            for (auto& r : split(refstr, ',')) {
                std::string ref = trim(r);
                // Strip "HEAD -> " prefix
                if (ref.substr(0, 7) == "HEAD -> ")
                    ref = ref.substr(7);
                if (!ref.empty() && ref != "HEAD")
                    c.refs.push_back(ref);
            }
        }

        commits.push_back(std::move(c));
    }
    return commits;
}

// ── diff_commit() ─────────────────────────────────────────────────────────────
std::vector<FileDiff> Repo::diff_commit(const std::string& oid) const {
    // Get list of changed files with stats
    std::string stats = run("show --stat --format= " + oid);
    // Get full patch
    std::string patch = run("show --format= -p " + oid);

    std::vector<FileDiff> results;

    // Parse patch into per-file sections
    std::istringstream ss(patch);
    std::string line;
    FileDiff current;
    bool in_file = false;

    auto flush = [&]() {
        if (in_file && !current.path.empty()) {
            results.push_back(current);
            current = FileDiff{};
        }
    };

    while (std::getline(ss, line)) {
        if (line.substr(0, 11) == "diff --git ") {
            flush();
            in_file = true;
            current.patch += line + "\n";
            // Extract path: "diff --git a/foo.cpp b/foo.cpp"
            auto bpos = line.rfind(" b/");
            if (bpos != std::string::npos)
                current.path = line.substr(bpos + 3);
        } else if (in_file) {
            current.patch += line + "\n";
            if (!line.empty() && line[0] == '+' &&
                !(line.size() > 1 && line[1] == '+'))
                ++current.additions;
            else if (!line.empty() && line[0] == '-' &&
                     !(line.size() > 1 && line[1] == '-'))
                ++current.deletions;
        }
    }
    flush();

    return results;
}

// ── blame() ───────────────────────────────────────────────────────────────────
BlameResult Repo::blame(const std::string& filepath) const {
    BlameResult result;
    result.path = filepath;

    // Porcelain blame: each hunk has header lines then content lines
    std::string out = run("blame --porcelain \"" + filepath + "\"");

    std::istringstream ss(out);
    std::string line;

    BlameHunk current_hunk;
    bool has_hunk = false;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        // A porcelain hunk header starts with 40-char SHA
        if (line.size() >= 40 &&
            std::all_of(line.begin(), line.begin() + 40, ::isxdigit)) {
            current_hunk = BlameHunk{};
            current_hunk.commit_oid = line.substr(0, 7);
            auto parts = split(line, ' ');
            if (parts.size() >= 3)
                current_hunk.final_line = std::stoul(parts[2]);
            has_hunk = true;
        } else if (has_hunk && line.substr(0, 7) == "author ") {
            current_hunk.author = trim(line.substr(7));
        } else if (has_hunk && line.substr(0, 12) == "author-time ") {
            // Unix timestamp → date string
            time_t t = std::stol(trim(line.substr(12)));
            char buf[16];
            struct tm* tm_info = gmtime(&t);
            strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);
            current_hunk.date = buf;
        } else if (has_hunk && !line.empty() && line[0] == '\t') {
            // Content line (prefixed with tab)
            result.lines.push_back(line.substr(1));
            result.hunks.push_back(current_hunk);
            has_hunk = false;
        }
    }

    return result;
}

// ── branches() ───────────────────────────────────────────────────────────────
std::vector<std::string> Repo::branches() const {
    std::string out = run("branch --format=%(refname:short)");
    std::vector<std::string> result;
    for (auto& b : split(out, '\n')) {
        std::string t = trim(b);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

std::string Repo::current_branch() const {
    return trim(run("rev-parse --abbrev-ref HEAD"));
}
