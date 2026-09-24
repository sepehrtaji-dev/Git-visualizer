#pragma once
#include <string>
#include <vector>

// ── Data structures ───────────────────────────────────────────────────────────

struct Commit {
    std::string oid;
    std::string short_oid;
    std::string message;
    std::string author;
    std::string email;
    std::string date;
    std::vector<std::string> parent_oids;
    std::vector<std::string> refs;
};

struct FileDiff {
    std::string path;
    int additions = 0;
    int deletions = 0;
    std::string patch;
};

struct BlameHunk {
    std::string commit_oid;
    std::string author;
    std::string date;
    size_t      final_line = 0;
};

struct BlameResult {
    std::string              path;
    std::vector<std::string> lines;
    std::vector<BlameHunk>   hunks; // one per line
};

// ── Repository wrapper ────────────────────────────────────────────────────────

class Repo {
public:
    explicit Repo(const std::string& path);

    std::vector<Commit>      log(int max_commits = 500) const;
    std::vector<FileDiff>    diff_commit(const std::string& oid) const;
    BlameResult              blame(const std::string& filepath) const;
    std::vector<std::string> branches() const;
    std::string              current_branch() const;

    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::string run(const std::string& cmd) const;
};
