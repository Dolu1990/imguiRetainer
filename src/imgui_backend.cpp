#include <rgui/imgui_backend.hpp>

#include <imgui.h>

#include <string>

namespace rgui::imgui_backend {
namespace {

class Context final : public ImGuiRenderContext {
public:
    void render_child(Node& child) override;

    bool begin_window(std::string_view title) override {
        const std::string title_copy{title};
        return ImGui::Begin(title_copy.c_str());
    }

    void end_window() override { ImGui::End(); }

    void text(std::string_view value, Rect bounds) override {
        ImGui::SetCursorPos({bounds.x, bounds.y});
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    bool button(std::string_view label, Rect bounds, bool enabled) override {
        ImGui::SetCursorPos({bounds.x, bounds.y});
        if (!enabled) ImGui::BeginDisabled();
        const bool clicked = ImGui::Button(label.data(), {bounds.width, bounds.height});
        if (!enabled) ImGui::EndDisabled();
        return clicked && enabled;
    }

    ImGuiContext& imgui_context() noexcept override { return *ImGui::GetCurrentContext(); }
};

void render_node(Node& node, RenderContext& context) {
    if (!node.visible()) return;
    ImGui::PushID(std::to_string(node.id()).c_str());
    node.render(context);
    ImGui::PopID();
}

void Context::render_child(Node& child) { render_node(child, *this); }

} // namespace

void render(UiTree& tree) {
    const NodePtr& root = tree.root();
    if (!root || !root->visible()) return;
    Context context;
    render_node(*root, context);
}

} // namespace rgui::imgui_backend
