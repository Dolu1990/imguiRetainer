# rgui

`rgui` is a C++23 retainer-mode GUI library intended to render with
[Dear ImGui](https://github.com/ocornut/imgui) and expose a clean Lua API via
[sol2](https://github.com/ThePhD/sol2). It currently provides a deliberately
small retained-mode vertical slice: `UiTree`, flow and anchored containers, text, buttons,
direct Dear ImGui drawing, and optional Lua bindings. See
[docs/architecture.md](docs/architecture.md) for the supported boundary and
deferred design decisions.

## Anchored layout

`AnchoredPanel` stores anchors on behalf of its children, so reusable nodes do
not need position or alignment state. Its dimensions are fixed by default:

```cpp
auto panel = std::make_shared<rgui::AnchoredPanel>(rgui::Size{320.0F, 100.0F});
panel->append(std::make_shared<rgui::Text>("Status"));
panel->append(std::make_shared<rgui::Button>("Continue"), {
    {0.5F, 0.0F}, {0.5F, 0.0F}, 0.0F, 32.0F,
});
```

The panel asks each child for `measure()` before resolving the anchor. Built-in
text and buttons measure themselves; a custom node should override `measure()`
when it will be placed in an anchored panel. A child may also have a second
anchor. Where its two `self` points differ on an axis, the panel offers the
intervening size on that axis through `measure(SizeProposal)`. Nodes may accept
or ignore that offer. For example, this stretches a button horizontally while
keeping its intrinsic height:

```cpp
panel->append(std::make_shared<rgui::Button>("Continue"),
    {{0.0F, 0.0F}, {0.0F, 0.0F}, 12.0F, 32.0F},
    {{1.0F, 0.0F}, {1.0F, 0.0F}, -12.0F, 32.0F});
```

The optional Lua binding takes self x/y then target x/y fractions (followed by
pixel offsets):

```lua
local panel = rgui.anchoredPanel(320, 100)
panel:append(rgui.text("Status"), 0, 0, 0, 0, 12, 12)
panel:append(rgui.button("Continue"), 0.5, 0, 0.5, 0, 0, 32)
```

Lua accepts the same two-anchor form, or `setSecondAnchor`, using another
`self`, `target`, and offset pair after the primary anchor arguments.

Either Lua dimension can instead be `"fill"`, which resolves each frame to the
available content width or height of its containing ImGui window. It therefore
tracks a resized window:

```lua
local full_width = rgui.anchoredPanel("fill", 100)
local full_surface = rgui.anchoredPanel("fill", "fill")
```

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
