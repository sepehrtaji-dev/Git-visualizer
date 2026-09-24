#include "repo.hpp"
#include <git2.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unordered_map>
#include <unordered_set>

// ── RAII helpers ──────────────────────────────────────────────────────────────
static void check(int err, const char* ctx) {
    if (err < 0) {
        const git_error* e = git_error_last();
        throw std::runtime_error(std::string(ctx) + ": " +
                                 (e ? e->message : "unknown error"));
    }
}

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
Repo::Repo(const std::string& path) : path_(path) {
    git_libgit2_init();
    check(git_repository_open_ext(&repo_, path.c_str(), 0, nullptr),
          "open repository");
}

Repo::~Repo() {
    if (repo_) git_repository_free(repo_);
    git_libgit2_shutdown();
}

// ── Helpers ───────────────────────────────────────────────────────────────────
std::string Repo::format_time(git_time_t t, int offset) const {
    time_t tt = static_cast<time_t>(t + offset * 60);
    struct tm tm_info;
    gmtime_r(&tt, &tm_info);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_info);
    return buf;
}

Commit Repo::make_commit(git_commit* c) const {
    Commit commit;

    // OID
    const git_oid* oid = git_commit_id(c);
    char sha[GIT_OID_HEXSZ + 1];
    git_oid_tostr(sha, sizeof(sha), oid);
    commit.oid       = sha;
    commit.short_oid = commit.oid.substr(0, 7);

    // Message (first line only)
    const char* msg = git_commit_message(c);
    if (msg) {
        std::string full(msg);
        auto nl = full.find('\n');
        commit.message = (nl != std::string::npos) ? full.substr(0, nl) : full;
    }

    // Author
    const git_signature* sig = git_commit_author(c);
    if (sig) {
        commit.author = sig->name  ? sig->name  : "";
        commit.email  = sig->email ? sig->email : "";
        commit.date   = format_time(sig->when.time, sig->when.offset);
    }

    // Parents
    unsigned int pcount = git_commit_parentcount(c);
    for (unsigned int i = 0; i < pcount; ++i) {
        const git_oid* pid = git_commit_parent_id(c, i);
        char psha[GIT_OID_HEXSZ + 1];
        git_oid_tostr(psha, sizeof(psha), pid);
        commit.parent_oids.push_back(psha);
    }

    return commit;
}

// ── log() ─────────────────────────────────────────────────────────────────────
std::vector<Commit> Repo::log(int max_commits) const {
    // Build ref → oid map for decorations
    std::unordered_map<std::string, std::vector<std::string>> ref_map;

    git_reference_iterator* ref_iter = nullptr;
    if (git_reference_iterator_new(&ref_iter, repo_) == 0) {
        git_reference* ref = nullptr;
        while (git_reference_next(&ref, ref_iter) == 0) {
            git_reference* resolved = nullptr;
            if (git_reference_resolve(&resolved, ref) == 0) {
                const git_oid* oid = git_reference_target(resolved);
                if (oid) {
                    char sha[GIT_OID_HEXSZ + 1];
                    git_oid_tostr(sha, sizeof(sha), oid);
                    std::string name = git_reference_shorthand(ref);
                    ref_map[sha].push_back(name);
                }
                git_reference_free(resolved);
            }
            git_reference_free(ref);
        }
        git_reference_iterator_free(ref_iter);
    }

    // Walk commits
    git_revwalk* walker = nullptr;
    check(git_revwalk_new(&walker, repo_), "revwalk_new");
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
    git_revwalk_push_glob(walker, "refs/heads/*");
    git_revwalk_push_glob(walker, "refs/remotes/*");

    std::vector<Commit> commits;
    git_oid oid;
    int count = 0;

    while (git_revwalk_next(&oid, walker) == 0 && count < max_commits) {
        git_commit* c = nullptr;
        if (git_commit_lookup(&c, repo_, &oid) != 0) continue;

        Commit commit = make_commit(c);

        // Attach ref decorations
        auto it = ref_map.find(commit.oid);
        if (it != ref_map.end())
            commit.refs = it->second;

        commits.push_back(std::move(commit));
        git_commit_free(c);
        ++count;
    }

    git_revwalk_free(walker);
    return commits;
}

// ── diff_commit() ─────────────────────────────────────────────────────────────
std::vector<FileDiff> Repo::diff_commit(const std::string& oid_str) const {
    git_oid oid;
    check(git_oid_fromstr(&oid, oid_str.c_str()), "oid_fromstr");

    git_commit* commit = nullptr;
    check(git_commit_lookup(&commit, repo_, &oid), "commit_lookup");

    git_tree* new_tree = nullptr;
    check(git_commit_tree(&new_tree, commit), "commit_tree");

    git_diff* diff = nullptr;

    if (git_commit_parentcount(commit) == 0) {
        // Initial commit — diff against empty tree
        check(git_diff_tree_to_tree(&diff, repo_, nullptr, new_tree, nullptr),
              "diff_tree_to_tree");
    } else {
        git_commit* parent = nullptr;
        check(git_commit_parent(&parent, commit, 0), "commit_parent");
        git_tree* old_tree = nullptr;
        check(git_commit_tree(&old_tree, parent), "parent_tree");
        check(git_diff_tree_to_tree(&diff, repo_, old_tree, new_tree, nullptr),
              "diff_tree_to_tree");
        git_tree_free(old_tree);
        git_commit_free(parent);
    }

    // Collect stats per file
    std::vector<FileDiff> results;
    size_t ndeltas = git_diff_num_deltas(diff);

    for (size_t i = 0; i < ndeltas; ++i) {
        const git_diff_delta* delta = git_diff_get_delta(diff, i);
        FileDiff fd;
        fd.path = delta->new_file.path;

        git_patch* patch = nullptr;
        if (git_patch_from_diff(&patch, diff, i) == 0) {
            size_t adds = 0, dels = 0;
            git_patch_line_stats(nullptr, &adds, &dels, patch);
            fd.additions = static_cast<int>(adds);
            fd.deletions = static_cast<int>(dels);

            // Get patch text
            git_buf buf = GIT_BUF_INIT;
            if (git_patch_to_buf(&buf, patch) == 0) {
                fd.patch = std::string(buf.ptr, buf.size);
                git_buf_dispose(&buf);
            }
            git_patch_free(patch);
        }
        results.push_back(std::move(fd));
    }

    git_diff_free(diff);
    git_tree_free(new_tree);
    git_commit_free(commit);
    return results;
}

// ── blame() ───────────────────────────────────────────────────────────────────
BlameResult Repo::blame(const std::string& filepath) const {
    BlameResult result;
    result.path = filepath;

    // Read file content at HEAD
    git_reference* head_ref = nullptr;
    check(git_repository_head(&head_ref, repo_), "repository_head");
    const git_oid* head_oid = git_reference_target(head_ref);

    git_commit* head_commit = nullptr;
    check(git_commit_lookup(&head_commit, repo_, head_oid), "head_commit");
    git_tree* tree = nullptr;
    check(git_commit_tree(&tree, head_commit), "commit_tree");

    git_tree_entry* entry = nullptr;
    check(git_tree_entry_bypath(&entry, tree, filepath.c_str()), "tree_entry");

    git_blob* blob = nullptr;
    check(git_blob_lookup(&blob, repo_,
                          git_tree_entry_id(entry)), "blob_lookup");

    // Split blob into lines
    const char* raw  = static_cast<const char*>(git_blob_rawcontent(blob));
    git_object_size_t size = git_blob_rawsize(blob);
    std::string content(raw, size);
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line))
        result.lines.push_back(line);

    git_blob_free(blob);
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(head_commit);
    git_reference_free(head_ref);

    // Run blame
    git_blame* blame = nullptr;
    check(git_blame_file(&blame, repo_, filepath.c_str(), nullptr),
          "blame_file");

    // Expand hunks per line
    for (size_t lineno = 1; lineno <= result.lines.size(); ++lineno) {
        const git_blame_hunk* h = git_blame_get_hunk_byline(blame, lineno);
        if (!h) continue;

        BlameHunk bh;
        char sha[8] = {};
        git_oid_tostr(sha, sizeof(sha), &h->final_commit_id);
        bh.commit_oid     = sha;
        bh.orig_line      = h->orig_start_line_number;
        bh.final_line     = lineno;
        bh.lines_in_hunk  = h->lines_in_hunk;

        if (h->final_signature) {
            bh.author = h->final_signature->name  ? h->final_signature->name  : "";
            bh.date   = format_time(h->final_signature->when.time,
                                    h->final_signature->when.offset);
        }
        result.hunks.push_back(std::move(bh));
    }

    git_blame_free(blame);
    return result;
}

// ── branches() ───────────────────────────────────────────────────────────────
std::vector<std::string> Repo::branches() const {
    std::vector<std::string> result;
    git_branch_iterator* it = nullptr;
    if (git_branch_iterator_new(&it, repo_, GIT_BRANCH_LOCAL) != 0)
        return result;

    git_reference* ref = nullptr;
    git_branch_t type;
    while (git_branch_next(&ref, &type, it) == 0) {
        const char* name = nullptr;
        git_branch_name(&name, ref);
        if (name) result.push_back(name);
        git_reference_free(ref);
    }
    git_branch_iterator_free(it);
    return result;
}

std::string Repo::current_branch() const {
    git_reference* head = nullptr;
    if (git_repository_head(&head, repo_) != 0) return "HEAD";
    std::string name = git_reference_shorthand(head);
    git_reference_free(head);
    return name;
}
