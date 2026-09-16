#include <rgui/lua.hpp>

#include <rgui/ui.hpp>

#include <sol/sol.hpp>

#include <cmath>
#include <memory>
#include <mutex>
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

std::pair<Justification, Justification> table_justification_from_string(const std::string& value) {
    if (value == "left") return {Justification::start, Justification::center};
    if (value == "top") return {Justification::center, Justification::start};
    if (value == "right") return {Justification::end, Justification::center};
    if (value == "bottom") return {Justification::center, Justification::end};
    if (value == "left_top") return {Justification::start, Justification::start};
    if (value == "center_top") return {Justification::center, Justification::start};
    if (value == "right_top") return {Justification::end, Justification::start};
    if (value == "left_center") return {Justification::start, Justification::center};
    if (value == "center") return {Justification::center, Justification::center};
    if (value == "right_center") return {Justification::end, Justification::center};
    if (value == "left_bottom") return {Justification::start, Justification::end};
    if (value == "center_bottom") return {Justification::center, Justification::end};
    if (value == "right_bottom") return {Justification::end, Justification::end};
    throw std::invalid_argument(
        "table justification must be left, top, right, left_top, center_top, right_top, "
        "left_center, center, right_center, left_bottom, center_bottom, or right_bottom");
}

Anchor anchor_from_lua(float selfX, float selfY, float targetX, float targetY,
                       float offsetX, float offsetY) {
    return {{selfX, selfY}, {targetX, targetY}, offsetX, offsetY};
}

AnchorPoint anchor_point_from_lua(const std::string& value) {
    if (value == "top_left") return {0.0F, 0.0F};
    if (value == "top") return {0.5F, 0.0F};
    if (value == "top_right") return {1.0F, 0.0F};
    if (value == "left") return {0.0F, 0.5F};
    if (value == "center") return {0.5F, 0.5F};
    if (value == "right") return {1.0F, 0.5F};
    if (value == "bottom_left") return {0.0F, 1.0F};
    if (value == "bottom") return {0.5F, 1.0F};
    if (value == "bottom_right") return {1.0F, 1.0F};
    throw std::invalid_argument(
        "anchor point must be top_left, top, top_right, left, center, right, "
        "bottom_left, bottom, or bottom_right");
}

void validate_anchor_fraction(float value) {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
        throw std::invalid_argument("anchor fractions must be finite values from 0 to 1");
    }
}

Anchor anchor_from_lua(const std::string& self, const std::string& target,
                       float offsetX = 0.0F, float offsetY = 0.0F) {
    return {anchor_point_from_lua(self), anchor_point_from_lua(target), offsetX, offsetY};
}

Anchor checked_anchor_from_lua(float selfX, float selfY, float targetX, float targetY,
                               float offsetX = 0.0F, float offsetY = 0.0F) {
    validate_anchor_fraction(selfX);
    validate_anchor_fraction(selfY);
    validate_anchor_fraction(targetX);
    validate_anchor_fraction(targetY);
    return anchor_from_lua(selfX, selfY, targetX, targetY, offsetX, offsetY);
}

std::pair<float, PanelExtent> panel_extent_from_lua(const sol::object& value) {
    if (value.is<float>()) return {value.as<float>(), PanelExtent::fixed};
    if (value.is<std::string>() && value.as<std::string>() == "fill") return {0.0F, PanelExtent::fill};
    throw std::invalid_argument("panel extent must be a number or the string 'fill'");
}

struct LuaNodeCallback {
    sol::function function;
    std::function<void(sol::function&, std::vector<sol::object>&)> execute;
};

sol::object node_to_lua(sol::function& callback, Node& node) {
    if (auto* text = dynamic_cast<Text*>(&node)) {
        return sol::make_object(callback.lua_state(), std::ref(*text));
    }
    if (auto* button = dynamic_cast<Button*>(&node)) {
        return sol::make_object(callback.lua_state(), std::ref(*button));
    }
    return sol::make_object(callback.lua_state(), std::ref(node));
}

} // namespace



void callbackExecuteDefault(sol::function& callback, std::vector<sol::object>& args){
    sol::protected_function_result result = callback(sol::as_args(args));
    if (!result.valid()) {
        sol::error error = result;
        throw std::runtime_error(error.what());
    }
}

void bindLua(sol::state_view state, std::recursive_mutex& mutex,
             std::function<void(sol::function&, std::vector<sol::object>&)> callbackExecute) {
    sol::table api = state["rgui"].get_or_create<sol::table>();
    const auto locked = [&mutex](auto&& body) -> decltype(auto) {
        std::lock_guard guard(mutex);
        return std::forward<decltype(body)>(body)();
    };

    api.new_usertype<Anchor>("Anchor",
        sol::call_constructor, sol::factories(
            [](const std::string& self, const std::string& target) {
                return anchor_from_lua(self, target);
            },
            [](const std::string& self, const std::string& target, float offsetX, float offsetY) {
                return anchor_from_lua(self, target, offsetX, offsetY);
            },
            [](float selfX, float selfY, float targetX, float targetY) {
                return checked_anchor_from_lua(selfX, selfY, targetX, targetY);
            },
            [](float selfX, float selfY, float targetX, float targetY, float offsetX, float offsetY) {
                return checked_anchor_from_lua(selfX, selfY, targetX, targetY, offsetX, offsetY);
            }),
        "selfX", sol::property(
            [](const Anchor& anchor) { return anchor.self.x; },
            [](Anchor& anchor, float value) { validate_anchor_fraction(value); anchor.self.x = value; }),
        "selfY", sol::property(
            [](const Anchor& anchor) { return anchor.self.y; },
            [](Anchor& anchor, float value) { validate_anchor_fraction(value); anchor.self.y = value; }),
        "targetX", sol::property(
            [](const Anchor& anchor) { return anchor.target.x; },
            [](Anchor& anchor, float value) { validate_anchor_fraction(value); anchor.target.x = value; }),
        "targetY", sol::property(
            [](const Anchor& anchor) { return anchor.target.y; },
            [](Anchor& anchor, float value) { validate_anchor_fraction(value); anchor.target.y = value; }),
        "offsetX", &Anchor::offsetX,
        "offsetY", &Anchor::offsetY);

    state.new_usertype<Node>("rgui.Node", sol::no_constructor,
        "id", [locked](const Node& node) { return locked([&] { return node.id(); }); },
        "visible", sol::property(
            [locked](const Node& node) { return locked([&] { return node.visible(); }); },
            [locked](Node& node, bool value) { locked([&] { node.setVisible(value); }); }),
        "enabled", sol::property(
            [locked](const Node& node) { return locked([&] { return node.enabled(); }); },
            [locked](Node& node, bool value) { locked([&] { node.setEnabled(value); }); }));
    state.new_usertype<Container>("rgui.Container", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "append", [locked](Container& parent, Node& child) {
            locked([&] { parent.append(child.shared_from_this()); });
        },
        "clear", [locked](Container& container) { locked([&] { container.clear(); }); });
    state.new_usertype<Stack>("rgui.Stack", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "axis", sol::property(
            [locked](const Stack& stack) { return locked([&] { return axis_to_string(stack.axis()); }); },
            [locked](Stack& stack, const std::string& value) {
                locked([&] { stack.setAxis(axis_from_string(value)); });
            }));
    state.new_usertype<Window>("rgui.Window", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "title", sol::property(
            [locked](const Window& window) { return locked([&] { return std::string(window.title()); }); },
            [locked](Window& window, const std::string& value) { locked([&] { window.setTitle(value); }); }));
    state.new_usertype<Table>("rgui.Table", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "columns", [locked](const Table& table) { return locked([&] { return table.columns(); }); },
        "setHeader", [locked](Table& table, std::size_t column, const std::string& value) {
            locked([&] {
                if (column == 0) throw std::invalid_argument("table column indices start at 1");
                table.setHeader(column - 1, value);
            });
        },
        "header", [locked](const Table& table, std::size_t column) {
            return locked([&] {
                if (column == 0) throw std::invalid_argument("table column indices start at 1");
                return std::string(table.header(column - 1));
            });
        },
        "setColumnFit", [locked](Table& table, std::size_t column) {
            locked([&] {
                if (column == 0) throw std::invalid_argument("table column indices start at 1");
                table.setColumnFit(column - 1);
            });
        },
        "setColumnWidth", [locked](Table& table, std::size_t column, float width) {
            locked([&] {
                if (column == 0) throw std::invalid_argument("table column indices start at 1");
                table.setColumnWidth(column - 1, width);
            });
        },
        "setColumnWeight", [locked](Table& table, std::size_t column, float weight) {
            locked([&] {
                if (column == 0) throw std::invalid_argument("table column indices start at 1");
                table.setColumnWeight(column - 1, weight);
            });
        },
        "setColumnJustify", [locked](Table& table, std::size_t column, const std::string& value) {
            locked([&] {
                if (column == 0) throw std::invalid_argument("table column indices start at 1");
                const auto [horizontal, vertical] = table_justification_from_string(value);
                table.setColumnJustify(column - 1, horizontal, vertical);
            });
        },
        "setVerticalBorders", [locked](Table& table, bool value) {
            locked([&] { table.setVerticalBorders(value); });
        });
    state.new_usertype<ScrollArea>("rgui.ScrollArea", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "setSize", [locked](ScrollArea& area, float width, float height) {
            locked([&] { area.setSize(Size{width, height}); });
        });
    state.new_usertype<AnchoredPanel>("rgui.AnchoredPanel", sol::no_constructor,
        sol::base_classes, sol::bases<Container, Node>(),
        "append", sol::overload(
            [locked](AnchoredPanel& panel, Node& child) {
                locked([&] { panel.append(child.shared_from_this()); });
            },
            [locked](AnchoredPanel& panel, Node& child, Anchor anchor) {
                locked([&] { panel.append(child.shared_from_this(), anchor); });
            },
            [locked](AnchoredPanel& panel, Node& child, Anchor primary, Anchor secondary) {
                locked([&] { panel.append(child.shared_from_this(), primary, secondary); });
            },
            [locked](AnchoredPanel& panel, Node& child, float selfX, float selfY,
               float targetX, float targetY, float offsetX, float offsetY) {
                locked([&] {
                    panel.append(child.shared_from_this(),
                                 anchor_from_lua(selfX, selfY, targetX, targetY, offsetX, offsetY));
                });
            },
            [locked](AnchoredPanel& panel, Node& child, float primarySelfX, float primarySelfY,
               float primaryTargetX, float primaryTargetY, float primaryOffsetX, float primaryOffsetY,
               float secondarySelfX, float secondarySelfY, float secondaryTargetX, float secondaryTargetY,
               float secondaryOffsetX, float secondaryOffsetY) {
                locked([&] {
                    panel.append(child.shared_from_this(),
                                 anchor_from_lua(primarySelfX, primarySelfY, primaryTargetX, primaryTargetY,
                                                 primaryOffsetX, primaryOffsetY),
                                 anchor_from_lua(secondarySelfX, secondarySelfY, secondaryTargetX, secondaryTargetY,
                                                 secondaryOffsetX, secondaryOffsetY));
                });
            }),
        "setAnchor", sol::overload(
            [locked](AnchoredPanel& panel, Node& child, Anchor anchor) {
                locked([&] { panel.setAnchor(child, anchor); });
            },
            [locked](AnchoredPanel& panel, Node& child, float selfX, float selfY,
                     float targetX, float targetY, float offsetX, float offsetY) {
                locked([&] { panel.setAnchor(child, anchor_from_lua(selfX, selfY, targetX, targetY, offsetX, offsetY)); });
            }),
        "setSecondAnchor", sol::overload(
            [locked](AnchoredPanel& panel, Node& child, Anchor anchor) {
                locked([&] { panel.setSecondAnchor(child, anchor); });
            },
            [locked](AnchoredPanel& panel, Node& child, float selfX, float selfY,
                     float targetX, float targetY, float offsetX, float offsetY) {
                locked([&] { panel.setSecondAnchor(child, anchor_from_lua(selfX, selfY, targetX, targetY, offsetX, offsetY)); });
            }),
        "clearSecondAnchor", [locked](AnchoredPanel& panel, Node& child) {
            locked([&] { panel.setSecondAnchor(child, std::nullopt); });
        });
    state.new_usertype<Text>("rgui.Text", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "value", sol::property(
            [locked](const Text& text) { return locked([&] { return std::string(text.value()); }); },
            [locked](Text& text, const std::string& value) { locked([&] { text.setValue(value); }); }),
        "onClick", [locked, callbackExecute](Text& text, sol::function callback) {
            auto luaCallback = std::make_shared<LuaNodeCallback>(
                LuaNodeCallback{std::move(callback), callbackExecute});
            locked([&] {
                text.setOnClick([luaCallback](Node& node) {
                    std::vector<sol::object> args{node_to_lua(luaCallback->function, node)};
                    luaCallback->execute(luaCallback->function, args);
                });
            });
        },
        "activate", [locked](Text& text) { locked([&] { text.activate(); }); });
    state.new_usertype<Button>("rgui.Button", sol::no_constructor,
        sol::base_classes, sol::bases<Node>(),
        "label", sol::property(
            [locked](const Button& button) { return locked([&] { return std::string(button.label()); }); },
            [locked](Button& button, const std::string& value) { locked([&] { button.setLabel(value); }); }),
        "onClick", [locked, callbackExecute](Button& button, sol::function callback) {
            auto luaCallback = std::make_shared<LuaNodeCallback>(
                LuaNodeCallback{std::move(callback), callbackExecute});
            locked([&] {
                button.setOnClick([luaCallback](Node& node) {
                    std::vector<sol::object> args{node_to_lua(luaCallback->function, node)};
                    luaCallback->execute(luaCallback->function, args);
                });
            });
        },
        "activate", [locked](Button& button) { locked([&] { button.activate(); }); });
    state.new_usertype<UiTree>("rgui.UiTree", sol::constructors<UiTree()>(),
        "setRoot", [locked](UiTree& tree, Node& root) {
            locked([&] { tree.setRoot(root.shared_from_this()); });
        },
        "draw", [locked](UiTree& tree) { locked([&] { tree.draw(); }); },
        "flushEvents", [locked](UiTree& tree) { return locked([&] { return tree.flushEvents(); }); });

    api.set_function("stack", [locked](const std::string& axis) {
        return locked([&] { return std::make_shared<Stack>(axis_from_string(axis)); });
    });
    api.set_function("anchoredPanel", [locked](const sol::object& width, const sol::object& height) {
        return locked([&] {
            const auto [width_value, width_extent] = panel_extent_from_lua(width);
            const auto [height_value, height_extent] = panel_extent_from_lua(height);
            return std::make_shared<AnchoredPanel>(Size{width_value, height_value}, width_extent, height_extent);
        });
    });
    api.set_function("window", [locked](const std::string& title) {
        return locked([&] { return std::make_shared<Window>(title); });
    });
    api.set_function("table", [locked](std::size_t columns) {
        return locked([&] { return std::make_shared<Table>(columns); });
    });
    api.set_function("scrollArea", [locked](float width, float height) {
        return locked([&] { return std::make_shared<ScrollArea>(Size{width, height}); });
    });
    api.set_function("text", [locked](const std::string& value) {
        return locked([&] { return std::make_shared<Text>(value); });
    });
    api.set_function("button", [locked](const std::string& label) {
        return locked([&] { return std::make_shared<Button>(label); });
    });
    api.set_function("tree", [locked] { return locked([] { return UiTree{}; }); });
}

} // namespace rgui
