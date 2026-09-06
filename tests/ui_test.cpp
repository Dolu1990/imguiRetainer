#include <rgui/rgui.hpp>

#include <imgui.h>

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
int failures = 0;
void expect(bool condition) { if (!condition) ++failures; }
class CustomNode final : public rgui::Node {
public:
    void draw() override { ImGui::TextUnformatted("custom node"); }
};

class RecordingNode final : public rgui::Node {
public:
    explicit RecordingNode(rgui::Size size) : size_(size) {}
    [[nodiscard]] rgui::Size measure() const override { return size_; }
    void draw() override { position_ = ImGui::GetCursorScreenPos(); }
    [[nodiscard]] ImVec2 position() const noexcept { return position_; }
private:
    rgui::Size size_;
    ImVec2 position_{};
};
} // namespace

int main() {
    static_assert(!std::is_copy_constructible_v<rgui::UiTree>);
    static_assert(!std::is_copy_assignable_v<rgui::UiTree>);
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

    auto transferable_root = std::make_shared<rgui::Window>("transferable");
    {
        rgui::UiTree temporary_tree;
        temporary_tree.set_root(transferable_root);
    }
    rgui::UiTree replacement_tree;
    replacement_tree.set_root(transferable_root);
    expect(replacement_tree.root() == transferable_root);

    auto moved_root = std::make_shared<rgui::Window>("moved");
    auto moved_button = std::make_shared<rgui::Button>("moved button");
    bool moved_callback = false;
    moved_button->set_on_click([&moved_callback](rgui::Button&) { moved_callback = true; });
    moved_root->append(moved_button);
    rgui::UiTree original_tree;
    original_tree.set_root(moved_root);
    moved_button->activate();
    rgui::UiTree moved_tree = std::move(original_tree);
    expect(moved_tree.flush_events() == 1 && moved_callback);

    const rgui::NodePtr detached = root->remove(*second);
    expect(detached.get() == second.get());
    expect(second->parent() == nullptr);
    bool rejected_multiple_parent = false;
    try { root->append(first); } catch (const std::logic_error&) { rejected_multiple_parent = true; }
    expect(rejected_multiple_parent);

    auto panel = std::make_shared<rgui::AnchoredPanel>(rgui::Size{100.0F, 50.0F});
    auto panel_origin = std::make_shared<RecordingNode>(rgui::Size{1.0F, 1.0F});
    auto centred = std::make_shared<RecordingNode>(rgui::Size{20.0F, 10.0F});
    panel->append(panel_origin);
    panel->append(centred, {rgui::AnchorPoint::top, rgui::AnchorPoint::top});
    root->append(panel);
    expect(panel->measure().width == 100.0F && panel->measure().height == 50.0F);
    expect(panel->anchor(*centred).self == rgui::AnchorPoint::top);

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
    expect(centred->position().x - panel_origin->position().x == 40.0F);
    expect(centred->position().y == panel_origin->position().y);
    ImGui::Begin("fill panel test");
    auto fill_panel = std::make_shared<rgui::AnchoredPanel>(
        rgui::Size{0.0F, 0.0F}, rgui::PanelExtent::fill, rgui::PanelExtent::fill);
    const rgui::Size available{ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y};
    const rgui::Size filled = fill_panel->measure();
    expect(filled.width == available.width && filled.height == available.height);
    fill_panel->draw();
    ImGui::End();
    ImGui::EndFrame();
    ImGui::DestroyContext();
    return failures == 0 ? 0 : 1;
}
