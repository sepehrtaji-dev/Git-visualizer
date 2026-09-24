#pragma once
#include <git2.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// ── Data structures ───────────────────────────────────────────────────────────

struct Commit {
    std::string oid;           // full SHA
    std::string short_oid;     // 7-char
    std::string message;       // first line
    std::string author;
    std::string email;
    std::string date;          // formatted
    std::vector<std::string> parent_oids;
    std::vector<std::string> refs; // branch/tag names pointing here
};

struct FileDiff {
    std::string path;
    int additions = 0;
    int deletions = 0;
    std::string patch;         // full unified diff text
};

struct BlameHunk {
    std::string commit_oid;
    std::string author;
    std::string date;
    size_t orig_line;
    size_t final_line;
    size_t lines_in_hunk;
};

struct BlameResult {
    std::string path;
    std::vector<std::string> lines;
    std::vector<BlameHunk>   hunks;  // one per line (expanded)
};

// ── Repository wrapper ────────────────────────────────────────────────────────

class Repo {
public:
    explicit Repo(const std::string& path);
    ~Repo();

    // Commit graph
    std::vector<Commit> log(int max_commits = 500) const;

    // Diff for a single commit
    std::vector<FileDiff> diff_commit(const std::string& oid) const;

    // Blame for a file at HEAD
    BlameResult blame(const std::string& filepath) const;

    // Branch names
    std::vector<std::string> branches() const;
    std::string              current_branch() const;

    const std::string& path() const { return path_; }

private:
    std::string  path_;
    git_repository* repo_ = nullptr;

    Commit        make_commit(git_commit* c) const;
    std::string   format_time(git_time_t t, int offset) const;
};
