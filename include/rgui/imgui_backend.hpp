#pragma once

#include <rgui/ui.hpp>

struct ImGuiContext;

namespace rgui::imgui_backend {

/// ImGui-specific extension available to custom nodes that opt into this backend.
class ImGuiRenderContext : public RenderContext {
public:
    [[nodiscard]] virtual ImGuiContext& imgui_context() noexcept = 0;
};

/// Renders a tree rooted in Window into the caller-owned current Dear ImGui
/// frame/context. Throws std::logic_error for any other root type.
void render(UiTree& tree);

/// Measures unsized leaves with the active ImGui style, then lays out the tree.
void layout(UiTree& tree, Size available);

} // namespace rgui::imgui_backend
