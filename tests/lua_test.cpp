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
        label:setFontScale(2)
        action:setFontScale(2)
        lua_text_font_scale = label:fontScale()
        lua_button_font_scale = action:fontScale()
        stats = rgui.table(2)
        log = rgui.scrollArea(240, 80)
        stats:setHeader(1, "Stat")
        stats:setHeader(2, "Value")
        stats:setColumnFit(1)
        stats:setColumnWidth(2, 80)
        stats:setColumnJustify(2, "right_center")
        row_color = rgui.Color(1.0, 0.0, 0.0, 1.0)
        stats:setRowColor(2, row_color)
        stats:clearRowColor(2)
        stats:setInnerHorizontalBorders(false)
        stats:setOuterHorizontalBorders(false)
        stats:setHorizontalBorders(true)
        stats:setInnerVerticalBorders(false)
        stats:setOuterVerticalBorders(true)
        stats:setVerticalBorders(false)
        lua_border_state = stats:innerHorizontalBorders()
            and stats:outerHorizontalBorders()
            and not stats:innerVerticalBorders()
            and not stats:outerVerticalBorders()
        alignment_names = {
            "left", "top", "right",
            "left_top", "center_top", "right_top",
            "left_center", "center", "right_center",
            "left_bottom", "center_bottom", "right_bottom"
        }
        alignment_table = rgui.table(1)
        for _, name in ipairs(alignment_names) do
            alignment_table:setColumnJustify(1, name)
        end
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
        overlay = rgui.window("Overlay")
        overlay.backgroundAlpha = 0.4
        overlay.decorated = false
        overlay.movable = false
        overlay.resizable = false
        overlay:setScreenLayout("fill", 160, rgui.Anchor("top_left", "top_left"))
        overlay:clearScreenLayout()
        overlay:setScreenLayout(100, 50,
            rgui.Anchor("top_left", "top_left"), rgui.Anchor("bottom_right", "bottom_right"))
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
        callback_root = rgui.stack("vertical")
        clickable_text = rgui.text("Clickable text")
        clickable_button = rgui.button("Clickable button")
        clickable_text:onClick(function(node) text_callback_value = node.value end)
        clickable_button:onClick(function(node) button_callback_label = node.label end)
        callback_root:append(clickable_text)
        callback_root:append(clickable_button)
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
    const sol::protected_function_result invalid_table_sizing = lua.safe_script(R"(
        stats:setColumnWeight(0, 1)
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_table_width = lua.safe_script(R"(
        stats:setColumnWidth(1, 0)
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_table_justification = lua.safe_script(R"(
        stats:setColumnJustify(0, "left_bottom")
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_table_row = lua.safe_script(R"(
        stats:setRowColor(0, rgui.Color(1, 0, 0, 1))
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_color = lua.safe_script(
        "rgui.Color(1.1, 0, 0, 1)", sol::script_pass_on_error);
    const sol::protected_function_result invalid_justification_name = lua.safe_script(R"(
        stats:setColumnJustify(1, "diagonal")
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_window_alpha = lua.safe_script(R"(
        overlay.backgroundAlpha = -0.1
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_window_extent = lua.safe_script(R"(
        overlay:setScreenLayout("wide", 50, rgui.Anchor("top_left", "top_left"))
    )", sol::script_pass_on_error);
    const sol::protected_function_result invalid_text_font_scale = lua.safe_script(
        "label:setFontScale(0)", sol::script_pass_on_error);
    const sol::protected_function_result invalid_button_font_scale = lua.safe_script(
        "action:setFontScale(-1)", sol::script_pass_on_error);
    if (invalid_anchor_name.valid() || invalid_anchor_fraction.valid() ||
        invalid_table_sizing.valid() || invalid_table_width.valid() ||
        invalid_table_justification.valid() || invalid_justification_name.valid() ||
        invalid_table_row.valid() || invalid_color.valid() ||
        invalid_window_alpha.valid() || invalid_window_extent.valid() ||
        invalid_text_font_scale.valid() || invalid_button_font_scale.valid()) return 1;

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
    const std::shared_ptr<rgui::Text> label = lua["label"];
    const std::shared_ptr<rgui::Button> action = lua["action"];
    const std::shared_ptr<rgui::Button> stretched = lua["stretched"];
    const rgui::Anchor numeric_anchor = lua["numeric_anchor"];
    const std::shared_ptr<rgui::Table> stats = lua["stats"];
    const rgui::Color row_color = lua["row_color"];
    const std::shared_ptr<rgui::ScrollArea> log = lua["log"];
    const std::shared_ptr<rgui::Stack> all_nodes = lua["all_nodes"];
    const rgui::Anchor anchor = panel->anchor(*action);
    const std::optional<rgui::Anchor>& secondAnchor = panel->secondAnchor(*action);
    const rgui::Anchor stretched_anchor = panel->anchor(*stretched);
    const std::optional<rgui::Anchor>& stretched_second_anchor = panel->secondAnchor(*stretched);
    const std::shared_ptr<rgui::Button> failing_button = lua["failing_button"];
    const std::shared_ptr<rgui::Stack> callback_root = lua["callback_root"];
    const std::shared_ptr<rgui::Window> overlay = lua["overlay"];
    const std::shared_ptr<rgui::Text> clickable_text = lua["clickable_text"];
    const std::shared_ptr<rgui::Button> clickable_button = lua["clickable_button"];
    rgui::UiTree click_tree;
    click_tree.setRoot(callback_root);
    clickable_text->activate();
    clickable_button->activate();
    const bool lua_click_callbacks_work = click_tree.flushEvents() == 2 &&
        lua["text_callback_value"].get<std::string>() == "Clickable text" &&
        lua["button_callback_label"].get<std::string>() == "Clickable button";
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
                   && stats->children().size() == 2 && lua["lua_border_state"].get<bool>()
                   && row_color == rgui::Color{1.0F, 0.0F, 0.0F, 1.0F} && !stats->rowColor(1)
                   && stats->innerHorizontalBorders() && stats->outerHorizontalBorders()
                   && !stats->innerVerticalBorders() && !stats->outerVerticalBorders()
                   && log->size().width == 240.0F && log->size().height == 80.0F
                   && all_nodes->children().size() == 7
                   && overlay->backgroundAlpha() == 0.4F && !overlay->decorated() && !overlay->movable() &&
                   !overlay->resizable() && overlay->screenLayout() &&
                   overlay->screenLayout()->secondary
                   && label->fontScale() == 2.0F && action->fontScale() == 2.0F
                   && lua["lua_text_font_scale"].get<float>() == 2.0F
                   && lua["lua_button_font_scale"].get<float>() == 2.0F
                   && callback_error_propagated && lua_click_callbacks_work
               ? 0
               : 1;
}
