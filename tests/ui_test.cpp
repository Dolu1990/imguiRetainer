#include <rgui/rgui.hpp>

#include <cmath>
#include <memory>
#include <stdexcept>

namespace {

bool equal(float left, float right) { return std::fabs(left - right) < 0.001F; }

int failures = 0;
void expect(bool condition) { if (!condition) ++failures; }

class RecordingContext final : public rgui::RenderContext {
public:
    void render_child(rgui::Node& child) override { child.render(*this); }
    bool begin_window(std::string_view) override { return true; }
    void end_window() override {}
    void text(std::string_view value, rgui::Rect) override { rendered_text = value; }
    bool button(std::string_view label, rgui::Rect, bool enabled) override {
        rendered_button = label;
        return enabled && click_next_button;
    }

    bool click_next_button = false;
    std::string rendered_text;
    std::string rendered_button;
};

class CustomNode final : public rgui::Node {
public:
    void render(rgui::RenderContext& context) override { context.text("custom node", bounds()); }
};

} // namespace

int main() {
    auto root = std::make_shared<rgui::Stack>();
    root->set_gap(5.0F);
    auto first = std::make_shared<rgui::Button>("first");
    auto second = std::make_shared<rgui::Text>("second");
    first->set_layout_params({.preferred = {40.0F, 10.0F}});
    second->set_layout_params({.preferred = {30.0F, 20.0F}});
    root->append(first);
    root->append(second);

    rgui::UiTree tree;
    tree.set_root(root);
    tree.layout({100.0F, 100.0F});
    expect(first->parent() == root.get());
    expect(equal(first->bounds().y, 0.0F));
    expect(equal(second->bounds().y, 15.0F));
    expect(equal(second->bounds().width, 30.0F));
    expect(root->dirty() == rgui::Dirty::none);

    bool activated = false;
    first->set_on_click([&activated](rgui::Button&) { activated = true; });
    first->activate();
    expect(activated);
    activated = false;
    first->set_enabled(false);
    first->activate();
    expect(!activated);

    RecordingContext context;
    CustomNode custom;
    custom.render(context);
    expect(context.rendered_text == "custom node");
    first->set_enabled(true);
    context.click_next_button = true;
    first->render(context);
    expect(context.rendered_button == "first");
    expect(activated);

    const rgui::NodePtr detached = root->remove(*second);
    expect(detached.get() == second.get());
    expect(second->parent() == nullptr);
    bool rejected_multiple_parent = false;
    try {
        root->append(first);
    } catch (const std::logic_error&) {
        rejected_multiple_parent = true;
    }
    expect(rejected_multiple_parent);
    return failures == 0 ? 0 : 1;
}
