# rgui architecture notes

## Goal

`rgui` is a C++23 retained UI layer for Dear ImGui, with an optional Lua API
implemented through sol2. Dear ImGui is the only rendering target. The game
owns the ImGui context, frame lifecycle, platform backend, allocator, and Lua
state.

## Current implementation state

- Public target: `rgui::rgui`
- Public headers: `<rgui/rgui.hpp>` and `<rgui/ui.hpp>`
- Retained nodes: `Window`, `Stack`, `Table`, `ScrollArea`, `AnchoredPanel`, `Text`, and `Button`
- Each node implements `draw()`; custom nodes may include
  `imgui.h` and use the Dear ImGui API directly.
- `UiTree::draw()` validates that a current ImGui context exists, establishes
  stable root identity, and draws the tree into the caller's current frame.
- `rgui::lua` remains an optional, superproject-only binding target.

The former renderer-neutral draw interface, independent retained geometry
engine, ImGui style extraction, and optional ImGui adapter target were removed.
Trying to reproduce ImGui layout rules outside ImGui made ordinary widgets
awkward and would make tables, tabs, popups, and similar scoped ImGui APIs
needlessly difficult.

## Dependencies

Dear ImGui is pinned under `ext/imgui` and used automatically when
`RGUI_IMGUI_TARGET` is empty, so the repository builds by itself. An embedding
game can instead provide its own ImGui target through `RGUI_IMGUI_TARGET`,
avoiding a second ImGui implementation. GLFW is a pinned submodule used only
by the opt-in graphical demo. sol2 is a pinned submodule; Lua remains supplied
by the embedding game through `RGUI_LUA_TARGET`.

Initialize submodules after cloning:

```sh
git submodule update --init --recursive
```

## Drawing model

`UiTree::draw()` must be called after `ImGui::NewFrame()` and before
`ImGui::Render()`. It does not create an ImGui context or start/end a frame.
`Window::draw()` always pairs `ImGui::Begin()` with `ImGui::End()`, and node
IDs—not labels or child positions—provide stable ImGui identity.

Containers own their children. A vertical `Stack` uses ImGui's normal flow; a
horizontal `Stack` uses `SameLine()` between visible children. `AnchoredPanel`
owns each child's anchor metadata. A primary anchor positions the child; an
optional secondary anchor derives a non-binding `SizeProposal` on each axis
where the two child anchor points differ. The child accepts, adjusts, or
ignores the proposal through `measure(SizeProposal)`, then is drawn with its
accepted size. Its dimensions can be fixed or fill either available ImGui
content axis. A real overlay, table, popup, or other composite should be a
specialized node that expresses ImGui's own begin/end protocol directly.
`Table` is the first such composite: it derives directly from `Node`, owns a
fixed positive number of columns and an explicit resizable row/cell model, and
draws each modeled row through ImGui's table API. Empty and hidden cells retain
their coordinates, and empty rows are still emitted.
Each column can justify its cell content horizontally and vertically with
start, center, or end alignment; the default is start on both axes.

Button and text click callbacks are queued and must be delivered by `UiTree::flushEvents()`
at an application-selected safe point. A queued callback is discarded when its
target is detached, reattached to another tree, or destroyed before dispatch.
Callback errors, including Lua errors, propagate from `flushEvents()` to the
embedding application. Structural changes, including container edits and table
row/cell edits, are not permitted while `UiTree::draw()` is running.

`bindLua` receives an embedding-owned `std::recursive_mutex`. It serializes all
Lua-exposed rgui object access with the embedding application's draw and event
flush points. The embedding must still ensure Lua callbacks are flushed on the
Lua-owning thread; the mutex does not make Lua state access thread-safe.

## Build checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

`rgui_imgui_glfw_demo` is an opt-in manual GLFW/OpenGL visual demo:

```sh
cmake -S . -B build/imgui-demo -DRGUI_BUILD_IMGUI_GLFW_DEMO=ON
cmake --build build/imgui-demo --target rgui_imgui_glfw_demo
```

## Deferred decisions

Add new retained node types only against concrete game requirements. Styling
should be represented as ImGui-facing flags or scoped style operations, rather
than a renderer-neutral theme model. Other deferred areas include focus and
gamepad navigation, input-consumption reporting, docking/modal policy,
animation, localization, accessibility metadata, and final Lua API
ergonomics/error reporting.
