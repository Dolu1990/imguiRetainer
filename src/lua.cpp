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
    if (value == "topLeft") return AnchorPoint::topLeft;
    if (value == "top") return AnchorPoint::top;
    if (value == "topRight") return AnchorPoint::topRight;
    if (value == "left") return AnchorPoint::left;
    if (value == "center") return AnchorPoint::center;
    if (value == "right") return AnchorPoint::right;
    if (value == "bottomLeft") return AnchorPoint::bottomLeft;
    if (value == "bottom") return AnchorPoint::bottom;
    if (value == "bottomRight") return AnchorPoint::bottomRight;
    throw std::invalid_argument("anchor point must be topLeft, top, topRight, left, center, right, bottomLeft, bottom, or bottomRight");
}

Anchor anchor_from_lua(const std::string& self, const std::string& target, float offsetX, float offsetY) {
    return {anchor_point_from_string(self), anchor_point_from_string(target), offsetX, offsetY};
}

std::pair<float, PanelExtent> panel_extent_from_lua(const sol::object& value) {
    if (value.is<float>()) return {value.as<float>(), PanelExtent::fixed};
    if (value.is<std::string>() && value.as<std::string>() == "fill") return {0.0F, PanelExtent::fill};
    throw std::invalid_argument("panel extent must be a number or the string 'fill'");
}

NodePtr node_from_lua(const sol::object& value) {
    if (value.is<std::shared_ptr<Window>>()) return value.as<std::shared_ptr<Window>>();
    if (value.is<std::shared_ptr<Stack>>()) return value.as<std::shared_ptr<Stack>>();
    if (value.is<std::shared_ptr<Table>>()) return value.as<std::shared_ptr<Table>>();
    if (value.is<std::shared_ptr<ScrollArea>>()) return value.as<std::shared_ptr<ScrollArea>>();
    if (value.is<std::shared_ptr<AnchoredPanel>>()) return value.as<std::shared_ptr<AnchoredPanel>>();
    if (value.is<std::shared_ptr<Text>>()) return value.as<std::shared_ptr<Text>>();
    if (value.is<std::shared_ptr<Button>>()) return value.as<std::shared_ptr<Button>>();
    throw std::invalid_argument("expected an rgui node");
}

} // namespace

void bindLua(sol::state_view state) {
    sol::table api = state["rgui"].get_or_create<sol::table>();

    state.new_usertype<Node>("rgui.Node", sol::no_constructor,
        "id", &Node::id,
        "visible", sol::property(&Node::visible, &Node::setVisible),
        "enabled", sol::property(&Node::enabled, &Node::setEnabled));
    state.new_usertype<Container>("rgui.Container", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "append", [](Container& parent, const sol::object& child) { parent.append(node_from_lua(child)); },
        "clear", &Container::clear);
    state.new_usertype<Stack>("rgui.Stack", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "axis", sol::property(
            [](const Stack& stack) { return axis_to_string(stack.axis()); },
            [](Stack& stack, const std::string& value) { stack.setAxis(axis_from_string(value)); }));
    state.new_usertype<Window>("rgui.Window", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "title", sol::property(
            [](const Window& window) { return std::string(window.title()); },
            [](Window& window, const std::string& value) { window.setTitle(value); }));
    state.new_usertype<Table>("rgui.Table", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "columns", &Table::columns,
        "setHeader", [](Table& table, std::size_t column, const std::string& value) {
            if (column == 0) throw std::invalid_argument("table column indices start at 1");
            table.setHeader(column - 1, value);
        },
        "header", [](const Table& table, std::size_t column) {
            if (column == 0) throw std::invalid_argument("table column indices start at 1");
            return std::string(table.header(column - 1));
        });
    state.new_usertype<ScrollArea>("rgui.ScrollArea", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "setSize", [](ScrollArea& area, float width, float height) {
            area.setSize(Size{width, height});
        });
    state.new_usertype<AnchoredPanel>("rgui.AnchoredPanel", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "append", sol::overload(
            [](AnchoredPanel& panel, const sol::object& child) { panel.append(node_from_lua(child)); },
            [](AnchoredPanel& panel, const sol::object& child, const std::string& self,
               const std::string& target, float offsetX, float offsetY) {
                panel.append(node_from_lua(child), anchor_from_lua(self, target, offsetX, offsetY));
            },
            [](AnchoredPanel& panel, const sol::object& child, const std::string& primarySelf,
               const std::string& primaryTarget, float primaryOffsetX, float primaryOffsetY,
               const std::string& secondarySelf, const std::string& secondaryTarget,
               float secondaryOffsetX, float secondaryOffsetY) {
                panel.append(node_from_lua(child),
                             anchor_from_lua(primarySelf, primaryTarget, primaryOffsetX, primaryOffsetY),
                             anchor_from_lua(secondarySelf, secondaryTarget, secondaryOffsetX, secondaryOffsetY));
            }),
        "setAnchor", [](AnchoredPanel& panel, const sol::object& child, const std::string& self,
                           const std::string& target, float offsetX, float offsetY) {
            panel.setAnchor(*node_from_lua(child), anchor_from_lua(self, target, offsetX, offsetY));
        },
        "setSecondAnchor", [](AnchoredPanel& panel, const sol::object& child, const std::string& self,
                                  const std::string& target, float offsetX, float offsetY) {
            panel.setSecondAnchor(*node_from_lua(child), anchor_from_lua(self, target, offsetX, offsetY));
        },
        "clearSecondAnchor", [](AnchoredPanel& panel, const sol::object& child) {
            panel.setSecondAnchor(*node_from_lua(child), std::nullopt);
        });
    state.new_usertype<Text>("rgui.Text", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "value", sol::property(
            [](const Text& text) { return std::string(text.value()); },
            [](Text& text, const std::string& value) { text.setValue(value); }));
    state.new_usertype<Button>("rgui.Button", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "label", sol::property(
            [](const Button& button) { return std::string(button.label()); },
            [](Button& button, const std::string& value) { button.setLabel(value); }),
        "onClick", [](Button& button, sol::protected_function callback) {
            button.setOnClick([callback = std::move(callback)](Button& clicked) mutable {
                sol::protected_function_result result = callback(clicked);
                if (!result.valid()) {
                    sol::error error = result;
                    throw std::runtime_error(error.what());
                }
            });
        },
        "activate", &Button::activate);
    state.new_usertype<UiTree>("rgui.UiTree", sol::constructors<UiTree()>(),
        "setRoot", [](UiTree& tree, const sol::object& root) { tree.setRoot(node_from_lua(root)); },
        "draw", &UiTree::draw,
        "flushEvents", &UiTree::flushEvents);

    api.set_function("stack", [](const std::string& axis) {
        return std::make_shared<Stack>(axis_from_string(axis));
    });
    api.set_function("anchoredPanel", [](const sol::object& width, const sol::object& height) {
        const auto [width_value, width_extent] = panel_extent_from_lua(width);
        const auto [height_value, height_extent] = panel_extent_from_lua(height);
        return std::make_shared<AnchoredPanel>(Size{width_value, height_value}, width_extent, height_extent);
    });
    api.set_function("window", [](const std::string& title) { return std::make_shared<Window>(title); });
    api.set_function("table", [](std::size_t columns) { return std::make_shared<Table>(columns); });
    api.set_function("scrollArea", [](float width, float height) {
        return std::make_shared<ScrollArea>(Size{width, height});
    });
    api.set_function("text", [](const std::string& value) { return std::make_shared<Text>(value); });
    api.set_function("button", [](const std::string& label) { return std::make_shared<Button>(label); });
    api.set_function("tree", [] { return UiTree{}; });
}

} // namespace rgui
