# rgui

`rgui` is a C++23 retainer-mode GUI library intended to render with
[Dear ImGui](https://github.com/ocornut/imgui) and expose a clean Lua API via
[sol2](https://github.com/ThePhD/sol2). It currently provides a deliberately
small retained-mode vertical slice: `UiTree`, containers, text, buttons,
direct Dear ImGui drawing, and optional Lua bindings. See
[docs/architecture.md](docs/architecture.md) for the supported boundary and
deferred design decisions.

## Requirements

- CMake 3.25 or newer
- A compiler with C++23 support
- Ninja or another CMake-supported build tool

Dear ImGui and GLFW are pinned as Git submodules under `ext/`; initialize them
after cloning the repository:

```sh
git submodule update --init --recursive
```

sol2 is a pinned submodule. Lua remains owned by the embedding game: enabling
the optional bindings requires its existing Lua CMake target through
`RGUI_LUA_TARGET`.

## ImGui GLFW demo

`rgui_imgui_glfw_demo` is an opt-in GLFW/OpenGL executable that renders a
retained `rgui` tree through Dear ImGui. It is intentionally separate from the
library and is never installed.

With the bundled submodules initialized:

```sh
cmake -S . -B build/imgui-demo \
  -DRGUI_BUILD_IMGUI_GLFW_DEMO=ON
cmake --build build/imgui-demo --target rgui_imgui_glfw_demo
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
