#include <rgui/rgui.hpp>

#include <imgui.h>

#include <memory>
#include <stdexcept>

namespace {
int failures = 0;
void expect(bool condition) { if (!condition) ++failures; }
class CustomNode final : public rgui::Node {
public:
    void draw() override { ImGui::TextUnformatted("custom node"); }
};
} // namespace

int main() {
    auto root = std::make_shared<rgui::Window>("test window");
    auto first = std::make_shared<rgui::Button>("first");
    auto second = std::make_shared<rgui::Text>("second");
    root->append(first);
    root->append(second);
    rgui::UiTree tree;
    tree.set_root(root);
    expect(first->parent() == root.get());

    bool activated = false;
    first->set_on_click([&activated](rgui::Button&) { activated = true; });
    first->activate();
    expect(!activated);
    expect(tree.flush_events() == 1);
    expect(activated);
    activated = false;
    first->set_enabled(false);
    first->activate();
    expect(tree.flush_events() == 0);

    bool snapshot_callback = false;
    first->set_enabled(true);
    first->set_on_click([&snapshot_callback](rgui::Button&) { snapshot_callback = true; });
    first->activate();
    first->set_on_click([](rgui::Button&) {});
    first->set_label("changed while queued");
    expect(tree.flush_events() == 1);
    expect(snapshot_callback);

    auto self_removing = std::make_shared<rgui::Button>("remove me");
    root->append(self_removing);
    self_removing->set_on_click([&root](rgui::Button& button) { (void)root->remove(button); });
    self_removing->activate();
    expect(tree.flush_events() == 1);
    expect(self_removing->parent() == nullptr);

    auto discarded = std::make_shared<rgui::Button>("discarded");
    bool discarded_called = false;
    discarded->set_on_click([&discarded_called](rgui::Button&) { discarded_called = true; });
    root->append(discarded);
    discarded->activate();
    const rgui::NodePtr removed_discarded = root->remove(*discarded);
    expect(tree.flush_events() == 0);
    expect(!discarded_called);

    int deferred_dispatches = 0;
    first->set_on_click([&deferred_dispatches](rgui::Button& button) {
        ++deferred_dispatches;
        if (deferred_dispatches == 1) button.activate();
    });
    first->activate();
    expect(tree.flush_events() == 1);
    expect(tree.pending_event_count() == 1);
    expect(tree.flush_events() == 1);
    expect(deferred_dispatches == 2);

    const rgui::NodePtr detached = root->remove(*second);
    expect(detached.get() == second.get());
    expect(second->parent() == nullptr);
    bool rejected_multiple_parent = false;
    try { root->append(first); } catch (const std::logic_error&) { rejected_multiple_parent = true; }
    expect(rejected_multiple_parent);

    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = {640.0F, 480.0F};
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    CustomNode custom;
    custom.draw();
    tree.draw();
    ImGui::EndFrame();
    ImGui::DestroyContext();
    return failures == 0 ? 0 : 1;
}
