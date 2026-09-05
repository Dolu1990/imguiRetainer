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

class MeasuringContext final : public rgui::LayoutContext {
public:
    rgui::Size measure_text(std::string_view value) override {
        return {static_cast<float>(value.size() * 10), 16.0F};
    }

    rgui::Size measure_button(std::string_view label) override {
        return {static_cast<float>(label.size() * 10 + 16), 24.0F};
    }
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

    auto automatic_layout = std::make_shared<rgui::Stack>();
    auto automatic_text = std::make_shared<rgui::Text>("text");
    auto automatic_button = std::make_shared<rgui::Button>("go");
    automatic_layout->append(automatic_text);
    automatic_layout->append(automatic_button);
    rgui::UiTree automatic_tree;
    automatic_tree.set_root(automatic_layout);
    MeasuringContext measuring_context;
    automatic_tree.apply_default_layout(measuring_context);
    automatic_tree.layout({100.0F, 100.0F});
    expect(equal(automatic_text->layout_params().preferred.width, 40.0F));
    expect(equal(automatic_text->layout_params().preferred.height, 16.0F));
    expect(equal(automatic_button->layout_params().preferred.width, 36.0F));
    expect(equal(automatic_button->bounds().y, 16.0F));
    return failures == 0 ? 0 : 1;
}
