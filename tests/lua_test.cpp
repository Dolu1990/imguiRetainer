#include <rgui/lua.hpp>
#include <rgui/rgui.hpp>

#include <sol/sol.hpp>

#include <memory>
#include <mutex>
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
        panel:append(label, 0, 0, 0, 0, 12, 12)
        panel:append(action, 0.5, 0, 0.5, 0, 0, 32)
        panel:setSecondAnchor(action, 1, 0, 1, 0, -12, 32)
    )", sol::script_pass_on_error);
    if (!result.valid()) return 1;

    const sol::protected_function_result callback_result = lua.safe_script(R"(
        failing_button = rgui.button("Fails")
        failing_button:onClick(function() error("intentional callback failure") end)
    )", sol::script_pass_on_error);
    if (!callback_result.valid()) return 1;

    const std::shared_ptr<rgui::AnchoredPanel> panel = lua["panel"];
    const std::shared_ptr<rgui::AnchoredPanel> full_panel = lua["full_panel"];
    const std::shared_ptr<rgui::Button> action = lua["action"];
    const std::shared_ptr<rgui::Table> stats = lua["stats"];
    const std::shared_ptr<rgui::ScrollArea> log = lua["log"];
    const rgui::Anchor anchor = panel->anchor(*action);
    const std::optional<rgui::Anchor>& secondAnchor = panel->secondAnchor(*action);
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
                   anchor.self == rgui::AnchorPoint{0.5F, 0.0F} &&
                   anchor.target == rgui::AnchorPoint{0.5F, 0.0F} &&
                   anchor.offsetX == 0.0F && anchor.offsetY == 32.0F
                   && secondAnchor && secondAnchor->self == rgui::AnchorPoint{1.0F, 0.0F} &&
                   secondAnchor->target == rgui::AnchorPoint{1.0F, 0.0F} &&
                   secondAnchor->offsetX == -12.0F && secondAnchor->offsetY == 32.0F
                   && stats->columns() == 2 && stats->header(0) == "Stat" && stats->header(1) == "Value"
                   && stats->children().size() == 2
                   && log->size().width == 240.0F && log->size().height == 80.0F
                   && callback_error_propagated
               ? 0
               : 1;
}
