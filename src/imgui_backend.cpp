#include <rgui/imgui_backend.hpp>

#include <imgui.h>

#include <string>

namespace rgui::imgui_backend {
namespace {

void render_node(Node& node) {
    if (!node.visible()) return;

    ImGui::PushID(std::to_string(node.id()).c_str());
    if (auto* button = dynamic_cast<Button*>(&node)) {
        const Rect& r = button->bounds();
        ImGui::SetCursorPos({r.x, r.y});
        if (!button->enabled()) ImGui::BeginDisabled();
        if (ImGui::Button(button->label().data(), {r.width, r.height})) button->activate();
        if (!button->enabled()) ImGui::EndDisabled();
    } else if (auto* text = dynamic_cast<Text*>(&node)) {
        const Rect& r = text->bounds();
        ImGui::SetCursorPos({r.x, r.y});
        ImGui::TextUnformatted(text->value().data(), text->value().data() + text->value().size());
    } else if (auto* container = dynamic_cast<Container*>(&node)) {
        for (const NodePtr& child : container->children()) render_node(*child);
    }
    ImGui::PopID();
}

} // namespace

void render(UiTree& tree) {
    const NodePtr& root = tree.root();
    if (!root || !root->visible()) return;
    if (auto* window = dynamic_cast<Window*>(root.get())) {
        const bool render_contents = ImGui::Begin(window->title().data());
        if (render_contents) {
            for (const NodePtr& child : window->children()) render_node(*child);
        }
        ImGui::End();
        return;
    }
    render_node(*root);
}

} // namespace rgui::imgui_backend
