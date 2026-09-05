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

The library implements a small retained-mode vertical slice. It is intentionally
not yet a general-purpose UI system.

- Public target: `rgui::rgui`
- Public include: `<rgui/rgui.hpp>`
- Public namespace: `rgui`
- Core public API: retained `UiTree`, containers, `Text`, `Button`, layout
  primitives, and `rgui::version()`
- C++ standard: C++23
- Unit-test targets: `rgui_version_test` and `rgui_ui_test` (registered with
  CTest as `rgui.version` and `rgui.ui`)

The installed CMake package is named `rgui`; consumers use:

```cmake
find_package(rgui CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE rgui::rgui)
```

The optional `rgui::imgui` and `rgui::lua` targets are deliberately
superproject-only. Their dependencies are caller-owned CMake targets whose
names cannot be reconstructed reliably by an installed package.

## Dependencies

Third-party code is pinned as Git submodules:

| Dependency | Location | Current role |
| --- | --- | --- |
| Dear ImGui | `ext/imgui` | Rendering backend and immediate-mode demo |
| GLFW | `ext/glfw` | Window/OpenGL context for the demo |

Initialize them after cloning:

```sh
git submodule update --init --recursive
```

sol2 is pinned as the `ext/sol2` Git submodule. Lua remains an embedding-game
dependency: the optional `rgui::lua` adapter requires its caller to provide an
existing Lua CMake target through `RGUI_LUA_TARGET`. The binding accepts a
caller-owned `sol::state_view`; it neither creates nor configures a Lua state.
Do not silently fetch dependencies during a normal core-library build.

## Existing ImGui demo

The opt-in `rgui_imgui_glfw_demo` executable uses GLFW plus OpenGL and the regular
immediate-mode Dear ImGui API. It displays a simple window with increment,
reset, and toggle controls. It is a manual visual demo; it is not
installed and is not part of the public library API.

```sh
cmake -S . -B build/imgui-demo -DRGUI_BUILD_IMGUI_GLFW_DEMO=ON
cmake --build build/imgui-demo --target rgui_imgui_glfw_demo
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

## Core model decisions

The first implementation is intentionally a small vertical slice, rather than
a general CSS-like UI system. It establishes the ownership and frame semantics
that later widgets and Lua bindings must follow.

- `UiTree` owns one shared root node. A `Container` owns its children with
  `std::shared_ptr`; each child has at most one non-owning `Node*` parent.
  `append` rejects null children, cycles, and implicit reparenting. `remove`
  returns the detached shared pointer. There are no raw-pointer add/remove
  overloads: they make `enable_shared_from_this` lifetime failures too easy.
- Each `Node` receives a stable process-unique `NodeId`. Backends must use it
  for toolkit identity and must not derive identity from labels or child order.
- Nodes have `structure`, `layout`, and `paint` invalidation flags. Mutations
  propagate invalidation to ancestors. `UiTree::layout` performs a retained
  layout pass and clears the flags once it has arranged the current tree.
- Geometry uses logical pixels: `Size`, `Rect`, margins, preferred/minimum/
  maximum size, and flex-style `grow`. `grow` distributes only space that fits
  within each child's maximum size. DPI scaling belongs to the embedding
  game when it selects the logical available size.
- Layout is a two-stage `measure(available)` / `arrange(bounds)` protocol.
  The initial containers are `Stack` (horizontal or vertical, gap and
  cross-axis alignment) and `Overlay`. Exact text/font measurement is not a
  core concern; the game or rendering adapter may set suitable preferred sizes.
- `Text` and `Button` are the initial leaves. A button callback receives the
  button and only runs when the node is visible and enabled.
- Rendering is virtual through a renderer-neutral `RenderContext`; the backend
  has no closed type switch for built-in widgets. Applications
  can add `Node` subclasses and compose context operations. The ImGui adapter
  also supplies an `ImGuiRenderContext` extension for deliberately
  ImGui-specific custom nodes; such nodes include `imgui.h` themselves.

The core API includes no Dear ImGui, sol2, or Lua headers. The optional
`rgui::imgui` target is enabled with `RGUI_BUILD_IMGUI_BACKEND=ON` and requires
the embedding build to pass an existing `RGUI_IMGUI_TARGET`; rgui never fetches
or creates that dependency. The adapter renders into the caller-owned current
ImGui frame. It uses node IDs, calls `End` after every `Begin`, and leaves frame
creation, context ownership, platform integration, and `ImGui::Render` to the
game. A retained `Window`'s bounds describe its content rectangle; the adapter
converts that into an ImGui outer-window size. Nested retained windows preserve
their parent-relative coordinate origin.

The optional `rgui::lua` target is enabled with `RGUI_BUILD_LUA_BINDINGS=ON`.
It registers a focused retained-tree API using `rgui::bind_lua(sol::state_view)`.
The manual `rgui_lua_demo` target uses GLFW/OpenGL and Dear ImGui to render a
Lua-built retained tree:

```sh
cmake -S . -B build/lua-demo -DRGUI_BUILD_LUA_DEMO=ON
cmake --build build/lua-demo --target rgui_lua_demo
./build/lua-demo/demo/lua/rgui_lua_demo
```

## Deferred decisions

The following stay deliberately outside the first slice and should be added
against concrete game requirements: queued mutations during event dispatch,
style/theme inheritance, focus and gamepad navigation, input-consumption
reporting, scrolling/clipping, modal/layer management, animation, localization
and accessibility metadata. The initial Lua binding exists, but its API shape,
callback-error reporting, and GC semantics should be revisited before it is
treated as stable.

## Architecture discussion checklist

The next design discussion should resolve these points before adding a large
public API:

- Styling model, including inheritance and invalidation.
- State ownership: persistent widget state versus immediate-frame input.
- Event model, callback lifetime, queued mutation, and safe Lua error handling.
- Frame lifecycle: who starts/ends an ImGui frame and when rgui renders.
- Lua ergonomics: userdata shape, property access, callbacks, and GC behavior.
- Error/diagnostic policy and test strategy for headless core behavior.

## Verification status

The following have been successfully built on Linux:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake -S . -B build/imgui-demo -DRGUI_BUILD_IMGUI_GLFW_DEMO=ON
cmake --build build/imgui-demo --target rgui_imgui_glfw_demo
```

The second command verifies compilation and linkage of the graphical harness;
running it requires a desktop display environment.
