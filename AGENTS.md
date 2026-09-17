# Development guide

## Project intent

Build a C++23 retainer-mode UI layer that draws directly through Dear ImGui,
with an optional Lua scripting API through sol2. Keep Dear ImGui, sol2, and Lua
out of the core public headers so a game engine can own its ImGui context, Lua
state, allocator, and dependency versions.

The current implementation state, third-party dependency roles, verified build
commands, and unresolved architecture decisions are recorded in
`docs/architecture.md`. Read that document before changing the public API or
adding ImGui/Lua integration targets.

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
- `ext/imgui`, `ext/glfw`, and `ext/sol2` are pinned Git submodules. Initialize them with
  `git submodule update --init --recursive` before building graphical tests.
- Keep Lua bindings in a separately enabled target. They should accept imported
  dependency targets rather than acquiring dependencies implicitly.
- Do not expose Dear ImGui, sol2, or Lua headers from core public headers.
- Add a small CTest test for behavior changes; avoid making network access part
  of the default build.
- `rgui_imgui_glfw_demo` is a manual GLFW/OpenGL visual demo that exercises a
  retained `UiTree`; it is not installed and does not replace automated tests.
