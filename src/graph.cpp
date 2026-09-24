#include "graph.hpp"
#include <unordered_set>
#include <algorithm>
#include <sstream>

// ── Graph layout ──────────────────────────────────────────────────────────────
// Algorithm:
//  1. Map each commit OID → index
//  2. Track "active lanes" — each lane carries the OID it expects as next child
//  3. For each commit (topological order):
//     a. Find its lane (whichever active lane is waiting for it, or new lane)
//     b. Replace that lane's expected OID with first parent
//     c. Add new lanes for additional parents (merges)
//     d. Record lane_chars for rendering

Graph::Graph(const std::vector<Commit>& commits) {
    build(commits);
}

void Graph::build(const std::vector<Commit>& commits) {
    // lane_owner[i] = OID that lane i is "tracking" (waiting to see)
    std::vector<std::string> lane_owner; // active lanes
    nodes_.resize(commits.size());

    for (size_t i = 0; i < commits.size(); ++i) {
        const Commit& c = commits[i];
        nodes_[i].commit = &commits[i];

        // Find which lane belongs to this commit
        int my_lane = -1;
        for (int l = 0; l < (int)lane_owner.size(); ++l) {
            if (lane_owner[l] == c.oid) {
                my_lane = l;
                break;
            }
        }
        if (my_lane == -1) {
            // No lane waiting for us — open a new one
            my_lane = static_cast<int>(lane_owner.size());
            lane_owner.push_back(c.oid);
        }

        nodes_[i].lane = my_lane;

        // Update lane: replace our slot with first parent
        if (!c.parent_oids.empty()) {
            lane_owner[my_lane] = c.parent_oids[0];
        } else {
            // Root commit — close lane (mark empty)
            lane_owner[my_lane] = "";
        }

        // Add lanes for additional parents (merges)
        for (size_t p = 1; p < c.parent_oids.size(); ++p) {
            // Check if parent already tracked
            bool found = false;
            for (auto& lo : lane_owner) {
                if (lo == c.parent_oids[p]) { found = true; break; }
            }
            if (!found) {
                lane_owner.push_back(c.parent_oids[p]);
            }
        }

        // Compact: remove trailing empty lanes
        while (!lane_owner.empty() && lane_owner.back().empty())
            lane_owner.pop_back();

        // Build lane_chars for this row
        int total = std::max((int)lane_owner.size(), my_lane + 1);
        nodes_[i].n_lanes = total;
        nodes_[i].lane_chars.assign(total, '|');
        nodes_[i].lane_chars[my_lane] = '*';

        // Mark empty lanes
        for (int l = 0; l < total; ++l) {
            if (l < (int)lane_owner.size() && lane_owner[l].empty())
                nodes_[i].lane_chars[l] = ' ';
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────
std::string Graph::render_row(int i, int max_width) const {
    const GraphNode& node = nodes_[i];
    std::ostringstream oss;

    // Draw lane columns
    for (int l = 0; l < node.n_lanes; ++l) {
        char ch = (l < (int)node.lane_chars.size()) ? node.lane_chars[l] : ' ';
        oss << ch;
        if (l + 1 < node.n_lanes) oss << ' ';
    }

    oss << ' ';

    // Commit info
    const Commit* c = node.commit;
    if (c) {
        // Refs (branch/tag decorations)
        if (!c->refs.empty()) {
            oss << '(';
            for (size_t r = 0; r < c->refs.size(); ++r) {
                if (r) oss << ", ";
                oss << c->refs[r];
            }
            oss << ") ";
        }

        oss << c->short_oid << ' ';

        // Truncate message to fit
        int used = static_cast<int>(oss.str().size());
        int space = max_width - used - 1;
        if (space > 0) {
            std::string msg = c->message;
            if ((int)msg.size() > space)
                msg = msg.substr(0, space - 1) + "…";
            oss << msg;
        }
    }

    return oss.str();
}
