# Oracle Redux repository instructions

These instructions apply to the entire repository and exist so work can
continue safely across Codex sessions.

Before changing code, read:

1. `PROJECT_STATUS.md` for the current checkpoint and next queue;
2. `docs/development/conventions.md` for source placement and C++ rules;
3. the relevant architecture and reverse-engineering documents for the slice.

Every completed implementation slice must:

- update `PROJECT_STATUS.md` with what changed, what remains, and verification;
- update `specs/slices.json` when status, priority, gaps, dependencies, or
  developer-scenario coverage changes;
- update or add the relevant reverse-engineering/architecture documentation;
- keep public C++ headers as `.h` files under `include/oracle`;
- place implementation files in the documented responsibility subfolder;
- keep every room-slice source explicitly visible in the MSVC `src` filter
  tree, and preserve recursive globs for the headless test project;
- run the proportional CMake and MSVC verification listed in the status file;
- run `python tools/slice_status.py --check`;
- be committed as one coherent checkpoint, leaving the worktree clean.

Do not commit ROMs, extracted copyrighted assets, build products, local SDL
checkouts, or Visual Studio user state. Gameplay remains a native C++
behavioral reimplementation backed by a user-supplied validated ROM.
