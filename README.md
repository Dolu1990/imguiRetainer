# imguiRetainer

`imguiRetainer` is a C++23 retainer-mode GUI library intended to render with
[Dear ImGui](https://github.com/ocornut/imgui) and expose a clean Lua API via
[sol2](https://github.com/ThePhD/sol2). The public API is not designed yet;
this repository currently provides the portable build and packaging foundation
on which it can be developed.

## Requirements

- CMake 3.25 or newer
- A compiler with C++23 support
- Ninja or another CMake-supported build tool

Dear ImGui, sol2, and Lua are deliberately not fetched by this starter project.
The eventual integration should be opt-in and should let the parent game engine
provide its own dependency targets.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/examples/imguiRetainer_hello
```

On Visual Studio, configure with `cmake -S . -B build` and build the generated
solution or run `cmake --build build --config Debug`. The test command then
becomes `ctest --test-dir build -C Debug --output-on-failure`.

## Consume from another CMake project

Use the installed package target:

```cmake
find_package(imguiRetainer CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE imguiRetainer::imguiRetainer)
```

Or add this directory with `add_subdirectory` and link the same target.

## Layout

- `include/` — public headers
- `src/` — library implementation
- `examples/` — small executable examples
- `tests/` — CTest tests without an external test framework
- `cmake/` — install-package support

See [AGENTS.md](AGENTS.md) for development conventions and the planned ImGui /
Lua integration boundaries.
