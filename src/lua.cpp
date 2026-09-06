#include <rgui/lua.hpp>

#include <rgui/ui.hpp>

#include <sol/sol.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace rgui {
namespace {

Axis axis_from_string(const std::string& value) {
    if (value == "horizontal") return Axis::horizontal;
    if (value == "vertical") return Axis::vertical;
    throw std::invalid_argument("axis must be 'horizontal' or 'vertical'");
}

const char* axis_to_string(Axis value) noexcept {
    return value == Axis::horizontal ? "horizontal" : "vertical";
}

AnchorPoint anchor_point_from_string(const std::string& value) {
    if (value == "top_left") return AnchorPoint::top_left;
    if (value == "top") return AnchorPoint::top;
    if (value == "top_right") return AnchorPoint::top_right;
    if (value == "left") return AnchorPoint::left;
    if (value == "center") return AnchorPoint::center;
    if (value == "right") return AnchorPoint::right;
    if (value == "bottom_left") return AnchorPoint::bottom_left;
    if (value == "bottom") return AnchorPoint::bottom;
    if (value == "bottom_right") return AnchorPoint::bottom_right;
    throw std::invalid_argument("anchor point must be top_left, top, top_right, left, center, right, bottom_left, bottom, or bottom_right");
}

Anchor anchor_from_lua(const std::string& self, const std::string& target, float offset_x, float offset_y) {
    return {anchor_point_from_string(self), anchor_point_from_string(target), offset_x, offset_y};
}

std::pair<float, PanelExtent> panel_extent_from_lua(const sol::object& value) {
    if (value.is<float>()) return {value.as<float>(), PanelExtent::fixed};
    if (value.is<std::string>() && value.as<std::string>() == "fill") return {0.0F, PanelExtent::fill};
    throw std::invalid_argument("panel extent must be a number or the string 'fill'");
}

NodePtr node_from_lua(const sol::object& value) {
    if (value.is<std::shared_ptr<Window>>()) return value.as<std::shared_ptr<Window>>();
    if (value.is<std::shared_ptr<Stack>>()) return value.as<std::shared_ptr<Stack>>();
    if (value.is<std::shared_ptr<AnchoredPanel>>()) return value.as<std::shared_ptr<AnchoredPanel>>();
    if (value.is<std::shared_ptr<Text>>()) return value.as<std::shared_ptr<Text>>();
    if (value.is<std::shared_ptr<Button>>()) return value.as<std::shared_ptr<Button>>();
    throw std::invalid_argument("expected an rgui node");
}

} // namespace

void bind_lua(sol::state_view state) {
    sol::table api = state["rgui"].get_or_create<sol::table>();

    state.new_usertype<Node>("rgui.Node", sol::no_constructor,
        "id", &Node::id,
        "visible", sol::property(&Node::visible, &Node::set_visible),
        "enabled", sol::property(&Node::enabled, &Node::set_enabled));
    state.new_usertype<Container>("rgui.Container", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "append", [](Container& parent, const sol::object& child) { parent.append(node_from_lua(child)); },
        "clear", &Container::clear);
    state.new_usertype<Stack>("rgui.Stack", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "axis", sol::property(
            [](const Stack& stack) { return axis_to_string(stack.axis()); },
            [](Stack& stack, const std::string& value) { stack.set_axis(axis_from_string(value)); }));
    state.new_usertype<Window>("rgui.Window", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "title", sol::property(
            [](const Window& window) { return std::string(window.title()); },
            [](Window& window, const std::string& value) { window.set_title(value); }));
    state.new_usertype<AnchoredPanel>("rgui.AnchoredPanel", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "append", sol::overload(
            [](AnchoredPanel& panel, const sol::object& child) { panel.append(node_from_lua(child)); },
            [](AnchoredPanel& panel, const sol::object& child, const std::string& self,
               const std::string& target, float offset_x, float offset_y) {
                panel.append(node_from_lua(child), anchor_from_lua(self, target, offset_x, offset_y));
            }),
        "set_anchor", [](AnchoredPanel& panel, const sol::object& child, const std::string& self,
                           const std::string& target, float offset_x, float offset_y) {
            panel.set_anchor(*node_from_lua(child), anchor_from_lua(self, target, offset_x, offset_y));
        });
    state.new_usertype<Text>("rgui.Text", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "value", sol::property(
            [](const Text& text) { return std::string(text.value()); },
            [](Text& text, const std::string& value) { text.set_value(value); }));
    state.new_usertype<Button>("rgui.Button", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "label", sol::property(
            [](const Button& button) { return std::string(button.label()); },
            [](Button& button, const std::string& value) { button.set_label(value); }),
        "on_click", [](Button& button, sol::protected_function callback) {
            button.set_on_click([callback = std::move(callback)](Button& clicked) mutable {
                sol::protected_function_result result = callback(clicked);
                if (!result.valid()) {
                    sol::error error = result;
                    throw std::runtime_error(error.what());
                }
            });
        },
        "activate", &Button::activate);
    state.new_usertype<UiTree>("rgui.UiTree", sol::constructors<UiTree()>(),
        "set_root", [](UiTree& tree, const sol::object& root) { tree.set_root(node_from_lua(root)); },
        "draw", &UiTree::draw,
        "flush_events", &UiTree::flush_events);

    api.set_function("stack", [](const std::string& axis) {
        return std::make_shared<Stack>(axis_from_string(axis));
    });
    api.set_function("anchored_panel", [](const sol::object& width, const sol::object& height) {
        const auto [width_value, width_extent] = panel_extent_from_lua(width);
        const auto [height_value, height_extent] = panel_extent_from_lua(height);
        return std::make_shared<AnchoredPanel>(Size{width_value, height_value}, width_extent, height_extent);
    });
    api.set_function("window", [](const std::string& title) { return std::make_shared<Window>(title); });
    api.set_function("text", [](const std::string& value) { return std::make_shared<Text>(value); });
    api.set_function("button", [](const std::string& label) { return std::make_shared<Button>(label); });
    api.set_function("tree", [] { return UiTree{}; });
}

} // namespace rgui
