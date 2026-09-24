#include "tui.hpp"
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::string repo_path = ".";

    if (argc > 1) {
        repo_path = argv[1];
    }

    // Resolve to absolute path
    repo_path = std::filesystem::absolute(repo_path).string();

    try {
        run_tui(repo_path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
