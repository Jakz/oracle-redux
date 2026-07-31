# Visual Studio workspace

Open `oracle-redux.sln` in Visual Studio and select `x64` with either the
`Debug` or `Release` configuration.

The solution groups the executable, tests, SDL dependency, and persistent
project guidance separately. Each native project also has filters matching the
physical responsibility folders under `src`, so Solution Explorer no longer
presents the engine as one flat source list.

## One-time SDL setup

The solution deliberately does not vendor SDL. If `reference/SDL` is missing,
run this from the repository root:

```powershell
git clone --depth 1 --branch release-3.4.10 `
  https://github.com/libsdl-org/SDL.git reference/SDL
```

The solution references SDL's official `VisualC/SDL/SDL.vcxproj`. Building
`OracleRoomSlice` therefore builds `SDL3.dll` automatically into the same
configuration output directory as the executable.

## Projects

- `OracleRoomSlice` builds the interactive BYO-ROM renderer. It is the default
  project to run and has the local Ages US path plus `--scenario-menu`
  configured as its debugger arguments. Press `F5`, choose a scenario by
  number or name in the console, and keep the same startup project for every
  implemented feature. The menu includes `latest`, `explore`, `chest`,
  `vasu`, `octorok`, `hole`, and `atlas`. The hole scenario scans the supplied
  campaign ROM for an ordinary hole with an adjacent safe spawn, so it works
  for both Ages and Seasons without extracted or hard-coded room content. It
  renders authentic
  cartridge pixels plus Octorok, projectile, Link attack, and sword OAM, runs
  the shared native enemy/part/item state paths, applies enemy and projectile
  damage, and exposes `X` as the original arc-based sword swing. Defeated red
  Octoroks now retain hit flashing and knockback, animate the original death
  puff, and can leave collectible bouncing heart or rupee parts.
  Legacy `--octorok-scenario`, `--vasu-scenario`, and `--chest-scenario`
  remain compatibility aliases; use
  `WASD`/arrows to move, `Z` to talk, and `F1` for the metatile diagnostic
  view. Use `--explore`, or explicit `--group` and `--room` arguments, to start
  in the original free-roaming room view instead.
- `OracleRuntimeTests` builds all headless runtime sources and the native test
  executable. It remains in the solution but is excluded from **Build
  Solution** so the same engine sources are not compiled twice during normal
  gameplay iteration. Right-click it and choose **Build**, then set it as the
  startup project and press `Ctrl+F5`, when you want to run the native suite.
- `SDL3` is the unmodified project from the ignored SDL checkout.

To test Seasons, open `OracleRoomSlice` properties, select **Debugging**, and
replace the ROM path in the evaluated command arguments while keeping
`--scenario-menu`. Visual Studio stores this local override outside Git. For
a fixed automated launch, replace the menu argument with `--scenario
octorok` (or another name). Run `--list-scenarios` to print the complete
catalog and exit.

To measure performance without manually closing the window, append
`--benchmark-frames 120` to the debugger arguments. The console reports total
FPS and separate update, render, and presentation durations. The in-game
diagnostic header also shows rolling FPS.

Visual Studio stores the chosen startup project in its local `.vs` state, not
in the tracked solution. Keep `OracleRoomSlice` selected when testing gameplay;
the startup project does not need to change as new slices are added.

Project-wide breadth and remaining gaps are tracked in
`../../specs/slices.json`. Run `python ../../tools/slice_status.py` from this
directory, or the equivalent command from the repository root, for the live
cross-subsystem summary.

## Command-line build

From a Visual Studio developer shell:

```powershell
msbuild projects\msvc\oracle-redux.sln `
  /m /p:Configuration=Debug /p:Platform=x64
```

Outputs are written to `projects/msvc/x64/Debug` or
`projects/msvc/x64/Release`. Those directories and Visual Studio's local `.vs`
state are ignored.

The project files use recursive source globs. New engine `.cpp` files anywhere
under `src`, native test `.cpp` files under `tests/cpp`, and public `.h` files
under `include/oracle` are picked up without manually editing the project.
Place new files according to `docs/development/conventions.md` and add a
matching filter entry when introducing a durable responsibility folder.
