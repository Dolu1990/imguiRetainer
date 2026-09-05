#pragma once

#include <rgui/ui.hpp>

struct ImGuiContext;

namespace rgui::imgui_backend {

/// ImGui-specific extension available to custom nodes that opt into this backend.
class ImGuiRenderContext : public RenderContext {
public:
    [[nodiscard]] virtual ImGuiContext& imgui_context() noexcept = 0;
};

/// Renders the tree into the caller-owned current Dear ImGui frame/context.
void render(UiTree& tree);

} // namespace rgui::imgui_backend
