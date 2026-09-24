# gitviz — TUI Git Graph Visualizer

A fast, keyboard-driven terminal UI for exploring git repositories — built with **C++20**, **libgit2**, and **FTXUI**.

![C++](https://img.shields.io/badge/C++-20-blue?logo=cplusplus)
![libgit2](https://img.shields.io/badge/libgit2-latest-orange)
![FTXUI](https://img.shields.io/badge/FTXUI-v5-green)

---

## Features

| Screen | Key | What it shows |
|--------|-----|---------------|
| **Graph view** | default | Coloured branch graph, commit hash, author, date |
| **Commit detail** | `Enter` | Full commit info + list of changed files |
| **Diff viewer** | `d` | Syntax-coloured unified diff per file |
| **Blame view** | `b` | Line-by-line blame with author + commit |
| **Branch list** | `B` | All local branches |

### Keybindings

| Key | Action |
|-----|--------|
| `↑` / `k` | Move up |
| `↓` / `j` | Move down |
| `Enter` | Open commit detail |
| `d` | Open diff for selected file |
| `b` | Open blame view |
| `B` | Open branch list |
| `Esc` / `q` | Go back / quit |

---

## Project Structure

```
gitviz/
├── CMakeLists.txt
└── src/
    ├── main.cpp       # Entry point & arg parsing
    ├── repo.hpp/cpp   # libgit2 wrapper (log, diff, blame, branches)
    ├── graph.hpp/cpp  # Branch lane layout algorithm
    ├── diff.hpp/cpp   # Patch parser & file summary renderer
    ├── blame.hpp/cpp  # Blame line renderer
    └── tui.hpp/cpp    # Full FTXUI TUI — screens & routing
```

---

## Build

### Requirements

```bash
# Ubuntu / Debian
sudo apt install libgit2-dev pkg-config cmake build-essential

# macOS
brew install libgit2 cmake
```

> **FTXUI** is fetched automatically via CMake FetchContent — no manual install needed.

### Compile

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run

```bash
./gitviz                     # visualize current directory
./gitviz /path/to/any/repo   # visualize a specific repo
```

---

## Architecture

### Graph Layout Algorithm

1. Walk all refs (branches/tags) with libgit2 in topological + time order
2. Track **active lanes** — each lane carries the OID it expects next
3. For each commit: find its lane, assign `*`, update parent tracking
4. Extra parents (merges) open new lanes
5. Render each lane as `│`, `*`, or ` ` with per-lane ANSI colours

### Screens & Routing

All screens are FTXUI `Renderer` + `CatchEvent` pairs, composed under a single root router that switches on an enum state machine.

---

## License

MIT
