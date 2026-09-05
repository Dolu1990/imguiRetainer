#pragma once

#include <rgui/ui.hpp>

namespace rgui::imgui_backend {

/// Renders the tree into the caller-owned current Dear ImGui frame/context.
void render(UiTree& tree);

} // namespace rgui::imgui_backend
