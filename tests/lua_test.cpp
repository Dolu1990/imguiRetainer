#include <rgui/lua.hpp>
#include <rgui/rgui.hpp>

#include <sol/sol.hpp>

#include <memory>

int main() {
    sol::state lua;
    lua.open_libraries(sol::lib::base);
    rgui::bind_lua(lua);

    const sol::protected_function_result result = lua.safe_script(R"(
        panel = rgui.anchored_panel(320, 100)
        label = rgui.text("Status")
        action = rgui.button("Continue")
        panel:append(label, "top_left", "top_left", 12, 12)
        panel:append(action, "top", "top", 0, 32)
    )", sol::script_pass_on_error);
    if (!result.valid()) return 1;

    const std::shared_ptr<rgui::AnchoredPanel> panel = lua["panel"];
    const std::shared_ptr<rgui::Button> action = lua["action"];
    const rgui::Anchor anchor = panel->anchor(*action);
    return panel->size().width == 320.0F && panel->size().height == 100.0F &&
                   anchor.self == rgui::AnchorPoint::top && anchor.target == rgui::AnchorPoint::top &&
                   anchor.offset_x == 0.0F && anchor.offset_y == 32.0F
               ? 0
               : 1;
}
