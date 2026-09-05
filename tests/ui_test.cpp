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
    bool begin_window(rgui::NodeId id, std::string_view, rgui::Rect) override {
        begun_window_id = id;
        return true;
    }
    void end_window() override {}
    void text(std::string_view value, rgui::Rect) override { rendered_text = value; }
    bool button(std::string_view label, rgui::Rect, bool enabled) override {
        rendered_button = label;
        return enabled && click_next_button;
    }

    bool click_next_button = false;
    rgui::NodeId begun_window_id = 0;
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

    auto window = std::make_shared<rgui::Window>("test window");
    window->render(context);
    expect(context.begun_window_id == window->id());

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

    automatic_text->set_value("longer text");
    automatic_tree.apply_default_layout(measuring_context);
    automatic_tree.layout({200.0F, 100.0F});
    expect(equal(automatic_text->layout_params().preferred.width, 110.0F));

    auto limited_grow = std::make_shared<rgui::Stack>(rgui::Axis::horizontal);
    auto capped = std::make_shared<rgui::Text>("capped");
    capped->set_layout_params({.preferred = {10.0F, 10.0F}, .maximum = {20.0F, 20.0F}, .grow = 1.0F});
    auto remaining = std::make_shared<rgui::Text>("remaining");
    remaining->set_layout_params({.preferred = {10.0F, 10.0F}, .maximum = {100.0F, 20.0F}, .grow = 1.0F});
    limited_grow->append(capped);
    limited_grow->append(remaining);
    rgui::UiTree grow_tree;
    grow_tree.set_root(limited_grow);
    grow_tree.layout({100.0F, 20.0F});
    expect(equal(capped->bounds().width, 20.0F));
    expect(equal(remaining->bounds().width, 80.0F));

    auto overlay = std::make_shared<rgui::Overlay>();
    auto overlay_child = std::make_shared<rgui::Text>("overlay");
    overlay_child->set_layout_params({.preferred = {20.0F, 10.0F}, .margin = {2.0F, 3.0F, 4.0F, 5.0F}});
    overlay->append(overlay_child);
    rgui::UiTree overlay_tree;
    overlay_tree.set_root(overlay);
    expect(equal(overlay->measure({100.0F, 100.0F}).width, 26.0F));
    expect(equal(overlay->measure({100.0F, 100.0F}).height, 18.0F));
    overlay_tree.layout({100.0F, 100.0F});
    expect(equal(overlay_child->bounds().x, 2.0F));
    expect(equal(overlay_child->bounds().y, 3.0F));

    auto stretched = std::make_shared<rgui::Stack>(rgui::Axis::horizontal);
    stretched->set_align(rgui::Align::stretch);
    auto maximum_height = std::make_shared<rgui::Text>("limited");
    maximum_height->set_layout_params({.preferred = {10.0F, 10.0F}, .maximum = {20.0F, 15.0F}});
    stretched->append(maximum_height);
    rgui::UiTree stretch_tree;
    stretch_tree.set_root(stretched);
    stretch_tree.layout({100.0F, 100.0F});
    expect(equal(maximum_height->bounds().height, 15.0F));

    rgui::LayoutParams invalid_params{};
    invalid_params.minimum = {-1.0F, 10.0F};
    invalid_params.maximum = {5.0F, -2.0F};
    invalid_params.margin = {-1.0F, 2.0F, -3.0F, 4.0F};
    invalid_params.grow = -1.0F;
    maximum_height->set_layout_params(invalid_params);
    const rgui::LayoutParams& normalized = maximum_height->layout_params();
    expect(equal(normalized.minimum.width, 0.0F));
    expect(equal(normalized.minimum.height, 10.0F));
    expect(equal(normalized.maximum.width, 5.0F));
    expect(equal(normalized.maximum.height, 10.0F));
    expect(equal(normalized.margin.left, 0.0F));
    expect(equal(normalized.margin.right, 0.0F));
    expect(equal(normalized.grow, 0.0F));
    return failures == 0 ? 0 : 1;
}
