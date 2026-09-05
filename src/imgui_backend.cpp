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
        set_cursor_to_bounds(bounds);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    bool button(std::string_view label, Rect bounds, bool enabled) override {
        set_cursor_to_bounds(bounds);
        if (!enabled) ImGui::BeginDisabled();
        const bool clicked = ImGui::Button(label.data(), {bounds.width, bounds.height});
        if (!enabled) ImGui::EndDisabled();
        return clicked && enabled;
    }

    ImGuiContext& imgui_context() noexcept override { return *ImGui::GetCurrentContext(); }

private:
    static void set_cursor_to_bounds(Rect bounds) {
        // Retained coordinates start at the content area's top-left. ImGui's
        // SetCursorPos instead uses the outer window coordinate system, whose
        // origin lies behind a decorated window's title bar.
        const ImVec2 content_origin = ImGui::GetCursorStartPos();
        ImGui::SetCursorPos({content_origin.x + bounds.x, content_origin.y + bounds.y});
    }
};

class ImGuiLayoutContext final : public LayoutContext {
public:
    Size measure_text(std::string_view value) override {
        const ImVec2 size = ImGui::CalcTextSize(value.data(), value.data() + value.size());
        return {size.x, size.y};
    }

    Size measure_button(std::string_view label) override {
        const ImVec2 label_size = ImGui::CalcTextSize(label.data(), label.data() + label.size());
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        return {label_size.x + 2.0F * padding.x, ImGui::GetFrameHeight()};
    }
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

void layout(UiTree& tree, Size available) {
    ImGuiLayoutContext context;
    tree.apply_default_layout(context);
    tree.layout(available);
}

} // namespace rgui::imgui_backend
