#include <rgui/lua.hpp>
#include <rgui/rgui.hpp>

#include <sol/sol.hpp>

#include <memory>
#include <mutex>
#include <iostream>
#include <stdexcept>

int main() {
    sol::state lua;
    lua.open_libraries(sol::lib::base);
    std::recursive_mutex mutex;
    rgui::bindLua(lua, mutex, rgui::callbackExecuteDefault);

    const sol::protected_function_result result = lua.safe_script(R"(
        panel = rgui.anchoredPanel(320, 100)
        full_panel = rgui.anchoredPanel("fill", "fill")
        label = rgui.text("Status")
        action = rgui.button("Continue")
        stats = rgui.table(2)
        log = rgui.scrollArea(240, 80)
        stats:setHeader(1, "Stat")
        stats:setHeader(2, "Value")
        stats:append(rgui.text("Health"))
        stats:append(rgui.text("100"))
        log:append(rgui.text("First log entry"))
        all_nodes = rgui.stack("vertical")
        all_nodes:append(rgui.window("Window"))
        all_nodes:append(rgui.stack("horizontal"))
        all_nodes:append(rgui.table(1))
        all_nodes:append(rgui.scrollArea(100, 50))
        all_nodes:append(rgui.anchoredPanel(100, 50))
        all_nodes:append(rgui.text("Text"))
        all_nodes:append(rgui.button("Button"))
        panel:append(label, 0, 0, 0, 0, 12, 12)
        named_anchor = rgui.Anchor("top", "top", 0, 32)
        numeric_anchor = rgui.Anchor(0, 0, 0, 0)
        numeric_anchor.selfX = 0.5
        numeric_anchor.selfY = 0
        numeric_anchor.targetX = 0.5
        numeric_anchor.targetY = 0
        numeric_anchor.offsetX = 0
        numeric_anchor.offsetY = 32
        panel:append(action, named_anchor)
        panel:setAnchor(action, numeric_anchor)
        panel:setSecondAnchor(action, rgui.Anchor("top_right", "top_right", -12, 32))
        stretched = rgui.button("Stretched")
        panel:append(stretched,
            rgui.Anchor("top_left", "top_left", 12, 64),
            rgui.Anchor("top_right", "top_right", -12, 64))
    )", sol::script_pass_on_error);
    if (!result.valid()) {
        const sol::error error = result;
        std::cerr << error.what() << '\n';
        return 1;
    }

    const sol::protected_function_result invalid_anchor_name = lua.safe_script(
        "rgui.Anchor('invalid', 'top_left')", sol::script_pass_on_error);
    const sol::protected_function_result invalid_anchor_fraction = lua.safe_script(
        "rgui.Anchor(-0.1, 0, 0, 0)", sol::script_pass_on_error);
    if (invalid_anchor_name.valid() || invalid_anchor_fraction.valid()) return 1;

    const sol::protected_function_result callback_result = lua.safe_script(R"(
        failing_button = rgui.button("Fails")
        failing_button:onClick(function() error("intentional callback failure") end)
    )", sol::script_pass_on_error);
    if (!callback_result.valid()) {
        const sol::error error = callback_result;
        std::cerr << error.what() << '\n';
        return 1;
    }

    const std::shared_ptr<rgui::AnchoredPanel> panel = lua["panel"];
    const std::shared_ptr<rgui::AnchoredPanel> full_panel = lua["full_panel"];
    const std::shared_ptr<rgui::Button> action = lua["action"];
    const std::shared_ptr<rgui::Button> stretched = lua["stretched"];
    const rgui::Anchor numeric_anchor = lua["numeric_anchor"];
    const std::shared_ptr<rgui::Table> stats = lua["stats"];
    const std::shared_ptr<rgui::ScrollArea> log = lua["log"];
    const std::shared_ptr<rgui::Stack> all_nodes = lua["all_nodes"];
    const rgui::Anchor anchor = panel->anchor(*action);
    const std::optional<rgui::Anchor>& secondAnchor = panel->secondAnchor(*action);
    const rgui::Anchor stretched_anchor = panel->anchor(*stretched);
    const std::optional<rgui::Anchor>& stretched_second_anchor = panel->secondAnchor(*stretched);
    const std::shared_ptr<rgui::Button> failing_button = lua["failing_button"];
    rgui::UiTree callback_tree;
    callback_tree.setRoot(failing_button);
    failing_button->activate();
    bool callback_error_propagated = false;
    try {
        (void)callback_tree.flushEvents();
    } catch (const std::runtime_error&) {
        callback_error_propagated = true;
    }
    return panel->size().width == 320.0F && panel->size().height == 100.0F &&
                   full_panel->widthExtent() == rgui::PanelExtent::fill &&
                   full_panel->heightExtent() == rgui::PanelExtent::fill &&
                   numeric_anchor.self == rgui::AnchorPoint{0.5F, 0.0F} &&
                   numeric_anchor.target == rgui::AnchorPoint{0.5F, 0.0F} &&
                   numeric_anchor.offsetX == 0.0F && numeric_anchor.offsetY == 32.0F &&
                   anchor.self == rgui::AnchorPoint{0.5F, 0.0F} &&
                   anchor.target == rgui::AnchorPoint{0.5F, 0.0F} &&
                   anchor.offsetX == 0.0F && anchor.offsetY == 32.0F
                   && secondAnchor && secondAnchor->self == rgui::AnchorPoint{1.0F, 0.0F} &&
                   secondAnchor->target == rgui::AnchorPoint{1.0F, 0.0F} &&
                   secondAnchor->offsetX == -12.0F && secondAnchor->offsetY == 32.0F
                   && stretched_anchor.self == rgui::AnchorPoint{0.0F, 0.0F} &&
                   stretched_anchor.target == rgui::AnchorPoint{0.0F, 0.0F} &&
                   stretched_anchor.offsetX == 12.0F && stretched_anchor.offsetY == 64.0F
                   && stretched_second_anchor && stretched_second_anchor->self == rgui::AnchorPoint{1.0F, 0.0F} &&
                   stretched_second_anchor->target == rgui::AnchorPoint{1.0F, 0.0F} &&
                   stretched_second_anchor->offsetX == -12.0F && stretched_second_anchor->offsetY == 64.0F
                   && stats->columns() == 2 && stats->header(0) == "Stat" && stats->header(1) == "Value"
                   && stats->children().size() == 2
                   && log->size().width == 240.0F && log->size().height == 80.0F
                   && all_nodes->children().size() == 7
                   && callback_error_propagated
               ? 0
               : 1;
}
