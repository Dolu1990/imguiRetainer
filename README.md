# rgui

`rgui` is a C++23 retainer-mode GUI library intended to render with
[Dear ImGui](https://github.com/ocornut/imgui) and expose a clean Lua API via
[sol2](https://github.com/ThePhD/sol2). The public API is not designed yet;
this repository currently provides the portable build and packaging foundation
on which it can be developed.

## Requirements

- CMake 3.25 or newer
- A compiler with C++23 support
- Ninja or another CMake-supported build tool

Dear ImGui and GLFW are pinned as Git submodules under `ext/`; initialize them
after cloning the repository:

```sh
git submodule update --init --recursive
```

sol2 and Lua are deliberately not added yet. The eventual binding should let a
parent game engine provide its own Lua runtime and dependency targets.

## ImGui GLFW demo

`rgui_imgui_glfw_demo` is an opt-in GLFW/OpenGL executable that demonstrates
basic Dear ImGui rendering. It displays a small immediate-mode window with
increment, reset, and toggle controls. It is intentionally separate from the
retainer-mode library and is never installed.

With the bundled submodules initialized:

```sh
cmake -S . -B build/imgui-demo \
  -DRGUI_BUILD_IMGUI_GLFW_DEMO=ON
cmake --build build/imgui-demo --target rgui_imgui_glfw_demo
```

The source locations can still be overridden when the game engine owns a
different dependency checkout:

```sh
  -DRGUI_IMGUI_SOURCE_DIR=/path/to/imgui \
  -DRGUI_GLFW_SOURCE_DIR=/path/to/glfw
```

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

On Visual Studio, configure with `cmake -S . -B build` and build the generated
solution or run `cmake --build build --config Debug`. The test command then
becomes `ctest --test-dir build -C Debug --output-on-failure`.

## Consume from another CMake project

Use the installed package target:

```cmake
find_package(rgui CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE rgui::rgui)
```

Or add this directory with `add_subdirectory` and link the same target.

## Layout

- `include/` — public headers
- `src/` — library implementation
- `ext/` — pinned third-party Git submodules (Dear ImGui and GLFW)
- `demo/` — opt-in graphical demonstration applications
- `tests/` — CTest tests without an external test framework
- `cmake/` — install-package support

See [AGENTS.md](AGENTS.md) for development conventions and
[docs/architecture.md](docs/architecture.md) for the preserved project context,
dependency roles, and the retainer-mode architecture discussion checklist.
