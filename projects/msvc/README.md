# Visual Studio workspace

Open `oracle-redux.sln` in Visual Studio and select `x64` with either the
`Debug` or `Release` configuration.

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
  project to run and already has the local Ages US path configured as its
  debugger argument with `--vasu-scenario`. It renders authentic cartridge
  pixels, executes the bounded retail Vasu script directly from that ROM, and
  uses its collision radii for NPC blocking and wall slide; use `WASD`/arrows
  to move, `Z` to talk, and `F1` for the metatile diagnostic view.
- `OracleRuntimeTests` builds all headless runtime sources and the native test
  executable. Set it as the startup project and press `Ctrl+F5` to run it.
- `SDL3` is the unmodified project from the ignored SDL checkout.

To test Seasons, open `OracleRoomSlice` properties and replace the command
argument under **Debugging** with the path to the Seasons US ROM.

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
under `include/oracle` are picked up without manually editing the solution.
