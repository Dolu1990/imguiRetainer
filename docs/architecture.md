# rgui architecture notes

This document records the project state and the architectural boundary agreed
so far. It is intentionally a planning document: it does not prescribe the
retainer-mode API before that design work has happened.

## Goal

`rgui` is a C++23 library that lets a game provide custom GUI in Lua. It will
offer a retainer-mode UI API, render through Dear ImGui, and bind the scripting
surface with sol2.

The intended environments are Linux and Windows. CMake is the build system.

## Current implementation state

The library is a small buildable skeleton, not a UI implementation yet.

- Public target: `rgui::rgui`
- Public include: `<rgui/rgui.hpp>`
- Public namespace: `rgui`
- Current API: `rgui::version()` only
- C++ standard: C++23
- Unit-test target: `rgui_version_test` (registered with CTest as
  `rgui.version`)
- Basic example: `rgui_hello`

The installed CMake package is named `rgui`; consumers use:

```cmake
find_package(rgui CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE rgui::rgui)
```

## Dependencies

Third-party code is pinned as Git submodules:

| Dependency | Location | Current role |
| --- | --- | --- |
| Dear ImGui | `ext/imgui` | Rendering backend and immediate-mode smoke test |
| GLFW | `ext/glfw` | Window/OpenGL context for the smoke test |

Initialize them after cloning:

```sh
git submodule update --init --recursive
```

sol2 and Lua are not integrated yet. This is deliberate: the final binding
must allow an embedding game engine to own the Lua state, allocator, Lua
version, and dependency targets. Do not silently fetch dependencies during a
normal core-library build.

## Existing ImGui harness

The opt-in `rgui_imgui_smoke` executable uses GLFW plus OpenGL and the regular
immediate-mode Dear ImGui API. It displays a simple window with increment,
reset, and toggle controls. It is a manual visual smoke test; it is not
installed and is not part of the public library API.

```sh
cmake -S . -B build/imgui-smoke -DRGUI_BUILD_IMGUI_SMOKE_TEST=ON
cmake --build build/imgui-smoke --target rgui_imgui_smoke
```

`RGUI_IMGUI_SOURCE_DIR` and `RGUI_GLFW_SOURCE_DIR` can override the bundled
submodule locations. This supports a parent game engine that owns alternate
checkouts.

## Agreed architectural boundary

The core retainer-mode model must not expose Dear ImGui, sol2, or Lua headers
in its public headers. Keep the following independently buildable concerns:

1. **Core model** — retained widget tree, properties, layout/state, and event
   semantics. This must be usable without an ImGui context or Lua state.
2. **ImGui backend** — converts the core model into Dear ImGui calls. The
   embedding application retains ownership of the ImGui context and frame
   lifecycle.
3. **Lua/sol2 binding** — exposes a stable, script-friendly subset of the core
   API. The embedding application retains ownership of the Lua state and
   chooses how to load scripts and report errors.

The detailed shape of these APIs is still undecided.

## Architecture discussion checklist

The next design discussion should resolve these points before adding a large
public API:

- Widget-tree ownership and mutation rules (C++ and Lua).
- Stable widget identity and ImGui ID mapping.
- Layout and styling model, including inheritance and invalidation.
- State ownership: persistent widget state versus immediate-frame input.
- Event model, callback lifetime, and safe Lua error handling.
- Frame lifecycle: who starts/ends an ImGui frame and when rgui renders.
- Lua ergonomics: userdata shape, property access, callbacks, and GC behavior.
- Error/diagnostic policy and test strategy for headless core behavior.

## Verification status

The following have been successfully built on Linux:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake -S . -B build/imgui-smoke -DRGUI_BUILD_IMGUI_SMOKE_TEST=ON
cmake --build build/imgui-smoke --target rgui_imgui_smoke
```

The second command verifies compilation and linkage of the graphical harness;
running it requires a desktop display environment.
