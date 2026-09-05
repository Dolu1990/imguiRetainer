# Development guide

## Project intent

Build a C++23 retainer-mode UI layer whose rendering adapter targets Dear ImGui
and whose scripting adapter targets Lua through sol2. Keep the core UI model
independent of both adapters so a game engine can own its ImGui context, Lua
state, allocator, and dependency versions.

## Build checks

Run these after a source or CMake change:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For multi-config generators, add `--config Debug` to the build and `-C Debug`
to CTest.

## Conventions

- Public headers live below `include/rgui/`.
- Public CMake target: `rgui::rgui`.
- Use `rgui` as the C++ namespace.
- `ext/imgui` and `ext/glfw` are pinned Git submodules. Initialize them with
  `git submodule update --init --recursive` before building graphical tests.
- Keep backend and Lua bindings in separately enabled targets when they are
  added. They should accept imported dependency targets rather than acquiring
  dependencies implicitly.
- Do not expose Dear ImGui, sol2, or Lua headers from core public headers.
- Add a small CTest test for behavior changes; avoid making network access part
  of the default build.
- `rgui_imgui_smoke` is a manual GLFW/OpenGL visual smoke test. Keep
  it on the immediate-mode API until it is replaced by retainer-mode coverage.
