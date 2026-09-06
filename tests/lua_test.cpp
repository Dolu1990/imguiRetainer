#include <rgui/lua.hpp>
#include <rgui/rgui.hpp>

#include <sol/sol.hpp>

#include <memory>
#include <stdexcept>

int main() {
    sol::state lua;
    lua.open_libraries(sol::lib::base);
    rgui::bind_lua(lua);

    const sol::protected_function_result result = lua.safe_script(R"(
        panel = rgui.anchored_panel(320, 100)
        full_panel = rgui.anchored_panel("fill", "fill")
        label = rgui.text("Status")
        action = rgui.button("Continue")
        stats = rgui.table(2)
        stats:set_header(1, "Stat")
        stats:set_header(2, "Value")
        stats:append(rgui.text("Health"))
        stats:append(rgui.text("100"))
        panel:append(label, "top_left", "top_left", 12, 12)
        panel:append(action, "top", "top", 0, 32)
        panel:set_second_anchor(action, "top_right", "top_right", -12, 32)
    )", sol::script_pass_on_error);
    if (!result.valid()) return 1;

    const sol::protected_function_result callback_result = lua.safe_script(R"(
        failing_button = rgui.button("Fails")
        failing_button:on_click(function() error("intentional callback failure") end)
    )", sol::script_pass_on_error);
    if (!callback_result.valid()) return 1;

    const std::shared_ptr<rgui::AnchoredPanel> panel = lua["panel"];
    const std::shared_ptr<rgui::AnchoredPanel> full_panel = lua["full_panel"];
    const std::shared_ptr<rgui::Button> action = lua["action"];
    const std::shared_ptr<rgui::Table> stats = lua["stats"];
    const rgui::Anchor anchor = panel->anchor(*action);
    const std::optional<rgui::Anchor>& second_anchor = panel->second_anchor(*action);
    const std::shared_ptr<rgui::Button> failing_button = lua["failing_button"];
    rgui::UiTree callback_tree;
    callback_tree.set_root(failing_button);
    failing_button->activate();
    bool callback_error_propagated = false;
    try {
        (void)callback_tree.flush_events();
    } catch (const std::runtime_error&) {
        callback_error_propagated = true;
    }
    return panel->size().width == 320.0F && panel->size().height == 100.0F &&
                   full_panel->width_extent() == rgui::PanelExtent::fill &&
                   full_panel->height_extent() == rgui::PanelExtent::fill &&
                   anchor.self == rgui::AnchorPoint::top && anchor.target == rgui::AnchorPoint::top &&
                   anchor.offset_x == 0.0F && anchor.offset_y == 32.0F
                   && second_anchor && second_anchor->self == rgui::AnchorPoint::top_right &&
                   second_anchor->target == rgui::AnchorPoint::top_right &&
                   second_anchor->offset_x == -12.0F && second_anchor->offset_y == 32.0F
                   && stats->columns() == 2 && stats->header(0) == "Stat" && stats->header(1) == "Value"
                   && stats->children().size() == 2
                   && callback_error_propagated
               ? 0
               : 1;
}
