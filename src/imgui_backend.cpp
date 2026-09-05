#include <rgui/imgui_backend.hpp>

#include <imgui.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace rgui::imgui_backend {
namespace {

class Context final : public ImGuiRenderContext {
public:
    void render_child(Node& child) override;

    bool begin_window(NodeId id, std::string_view title, Rect bounds) override {
        // ImGui identifies windows from the string passed to Begin, outside of
        // the regular PushID stack. Keep the human-visible title while making
        // retained-window identity stable and independent of that title.
        const std::string title_copy = std::string{title} + "###rgui-" + std::to_string(id);
        const ImGuiStyle& style = ImGui::GetStyle();
        const float border = style.WindowBorderSize;
        // Core window bounds describe the content rectangle. Dear ImGui sizes
        // windows including their title bar, padding, and border.
        ImGui::SetNextWindowSize({
            bounds.width + 2.0F * (style.WindowPadding.x + border),
            bounds.height + ImGui::GetFrameHeight() + 2.0F * (style.WindowPadding.y + border),
        }, ImGuiCond_Always);
        if (!origins_.empty()) {
            const ImVec2 parent_window = ImGui::GetWindowPos();
            const ImVec2 parent_content = ImGui::GetCursorStartPos();
            ImGui::SetNextWindowPos({
                parent_window.x + parent_content.x + bounds.x - origins_.back().x,
                parent_window.y + parent_content.y + bounds.y - origins_.back().y,
            }, ImGuiCond_Always);
        }
        origins_.push_back({bounds.x, bounds.y});
        return ImGui::Begin(title_copy.c_str());
    }

    void end_window() override {
        ImGui::End();
        origins_.pop_back();
    }

    void text(std::string_view value, Rect bounds) override {
        set_cursor_to_bounds(bounds);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    bool button(std::string_view label, Rect bounds, bool enabled) override {
        set_cursor_to_bounds(bounds);
        if (!enabled) ImGui::BeginDisabled();
        // RenderContext accepts arbitrary string_views, whereas ImGui::Button
        // expects a null-terminated label.
        const std::string label_copy{label};
        const bool clicked = ImGui::Button(label_copy.c_str(), {bounds.width, bounds.height});
        if (!enabled) ImGui::EndDisabled();
        return clicked && enabled;
    }

    ImGuiContext& imgui_context() noexcept override { return *ImGui::GetCurrentContext(); }

private:
    void set_cursor_to_bounds(Rect bounds) const {
        // Retained coordinates start at the content area's top-left. ImGui's
        // SetCursorPos instead uses the outer window coordinate system, whose
        // origin lies behind a decorated window's title bar.
        const ImVec2 content_origin = ImGui::GetCursorStartPos();
        const ImVec2 origin = origins_.empty() ? ImVec2{} : origins_.back();
        ImGui::SetCursorPos({content_origin.x + bounds.x - origin.x,
                             content_origin.y + bounds.y - origin.y});
    }

    std::vector<ImVec2> origins_;
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
    if (dynamic_cast<Window*>(root.get()) == nullptr) {
        throw std::logic_error("rgui ImGui rendering requires a Window root");
    }
    Context context;
    render_node(*root, context);
}

void layout(UiTree& tree, Size available) {
    ImGuiLayoutContext context;
    tree.apply_default_layout(context);
    tree.layout(available);
}

} // namespace rgui::imgui_backend
