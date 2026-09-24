#pragma once
#include "repo.hpp"
#include <string>
#include <vector>
#include <unordered_map>

// Each commit gets a GraphNode describing its visual position
struct GraphNode {
    const Commit* commit = nullptr;
    int  lane     = 0;       // which column this commit sits in
    int  n_lanes  = 1;       // total lanes at this row
    // Connectors: for each lane at this row, what symbol to draw
    // '|' straight, '/' merge-left, '\' merge-right, ' ' empty
    std::vector<char> lane_chars;
};

class Graph {
public:
    explicit Graph(const std::vector<Commit>& commits);

    const std::vector<GraphNode>& nodes() const { return nodes_; }

    // Render row i as a string: "* | | commit message …"
    std::string render_row(int i, int max_width = 80) const;

private:
    std::vector<GraphNode> nodes_;
    void build(const std::vector<Commit>& commits);
};
