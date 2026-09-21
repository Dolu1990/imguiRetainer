#include <rgui/rgui.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
int failures = 0;
void expect(bool condition) { if (!condition) ++failures; }
class CustomNode final : public rgui::Node {
public:
    void draw() override { ImGui::TextUnformatted("custom node"); }
};

class LayoutScaleNode final : public rgui::Node {
public:
    [[nodiscard]] float effectiveScale() const noexcept { return layoutScale(); }
    void draw() override {}
};

class ScaleMutationNode final : public rgui::Node {
public:
    ScaleMutationNode(rgui::UiTree& tree, bool& rejected) : tree_(tree), rejected_(rejected) {}
    void draw() override {
        try {
            tree_.setLayoutScale(1.0F);
        } catch (const std::logic_error&) {
            rejected_ = true;
        }
    }
private:
    rgui::UiTree& tree_;
    bool& rejected_;
};

class RecordingNode final : public rgui::Node {
public:
    explicit RecordingNode(rgui::Size size) : size_(size) {}
    [[nodiscard]] rgui::Size measure() const override { return size_; }
    void draw() override { position_ = ImGui::GetCursorScreenPos(); }
    [[nodiscard]] ImVec2 position() const noexcept { return position_; }
private:
    rgui::Size size_;
    ImVec2 position_{};
};

class ProposalRecordingNode final : public rgui::Node {
public:
    [[nodiscard]] rgui::Size measure() const override { return {10.0F, 8.0F}; }
    [[nodiscard]] rgui::Size measure(const rgui::SizeProposal& proposal) const override {
        proposal_ = proposal;
        rgui::Size result = measure();
        if (proposal.width) result.width = *proposal.width;
        if (proposal.height) result.height = *proposal.height;
        return result;
    }
    void draw() override {}
    void draw(rgui::Size resolved_size) override { drawn_size_ = resolved_size; }
    [[nodiscard]] const rgui::SizeProposal& proposal() const noexcept { return proposal_; }
    [[nodiscard]] rgui::Size drawn_size() const noexcept { return drawn_size_; }
private:
    mutable rgui::SizeProposal proposal_;
    rgui::Size drawn_size_{};
};

class ScrollRecordingNode final : public rgui::Node {
public:
    void draw() override {
        window_size_ = ImGui::GetWindowSize();
        for (int index = 0; index < 20; ++index) ImGui::TextUnformatted("scrollable content");
        scroll_max_y_ = ImGui::GetScrollMaxY();
    }
    [[nodiscard]] ImVec2 window_size() const noexcept { return window_size_; }
    [[nodiscard]] float scroll_max_y() const noexcept { return scroll_max_y_; }
private:
    ImVec2 window_size_{};
    float scroll_max_y_ = 0.0F;
};

class TableWidthRecordingNode final : public rgui::Node {
public:
    explicit TableWidthRecordingNode(rgui::Size size = {}) : size_(size) {}
    [[nodiscard]] rgui::Size measure() const override { return size_; }
    void draw() override { width_ = ImGui::GetContentRegionAvail().x; }
    [[nodiscard]] float width() const noexcept { return width_; }
private:
    rgui::Size size_;
    float width_ = 0.0F;
};

class TablePositionRecordingNode final : public rgui::Node {
public:
    explicit TablePositionRecordingNode(rgui::Size size) : size_(size) {}
    [[nodiscard]] rgui::Size measure() const override { return size_; }
    void draw() override {
        position_ = ImGui::GetCursorScreenPos();
        remaining_width_ = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy({size_.width, size_.height});
    }
    [[nodiscard]] ImVec2 position() const noexcept { return position_; }
    [[nodiscard]] float remainingWidth() const noexcept { return remaining_width_; }
private:
    rgui::Size size_;
    ImVec2 position_{};
    float remaining_width_ = 0.0F;
};

class ReplaceDuringDrawNode final : public rgui::Node {
public:
    ReplaceDuringDrawNode(rgui::Container& parent, rgui::NodePtr replacement, bool& rejected)
        : parent_(parent), replacement_(std::move(replacement)), rejected_(rejected) {}
    void draw() override {
        try {
            (void)parent_.replace(*this, replacement_);
        } catch (const std::logic_error&) {
            rejected_ = true;
        }
    }
private:
    rgui::Container& parent_;
    rgui::NodePtr replacement_;
    bool& rejected_;
};

class SetTableCellDuringDrawNode final : public rgui::Node {
public:
    SetTableCellDuringDrawNode(rgui::Table& table, bool& rejected)
        : table_(table), rejected_(rejected) {}
    void draw() override {
        try {
            table_.setCell(0, 0, shared_from_this());
        } catch (const std::logic_error&) {
            rejected_ = true;
        }
    }
private:
    rgui::Table& table_;
    bool& rejected_;
};
} // namespace

int main() {
    static_assert(!std::is_copy_constructible_v<rgui::UiTree>);
    static_assert(!std::is_copy_assignable_v<rgui::UiTree>);
    static_assert(!std::is_base_of_v<rgui::Container, rgui::Table>);
    auto root = std::make_shared<rgui::Window>("test window");
    auto first = std::make_shared<rgui::Button>("first");
    auto second = std::make_shared<rgui::Text>("second");
    expect(first->fontScale() == 1.0F && second->fontScale() == 1.0F);
    first->setFontScale(2.0F);
    second->setFontScale(2.0F);
    expect(first->fontScale() == 2.0F && second->fontScale() == 2.0F);
    for (const float invalid : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        bool text_rejected = false;
        bool button_rejected = false;
        try { second->setFontScale(invalid); } catch (const std::invalid_argument&) { text_rejected = true; }
        try { first->setFontScale(invalid); } catch (const std::invalid_argument&) { button_rejected = true; }
        expect(text_rejected && button_rejected && second->fontScale() == 2.0F && first->fontScale() == 2.0F);
    }
    first->setFontScale(1.0F);
    second->setFontScale(1.0F);
    root->append(first);
    root->append(second);
    rgui::UiTree tree;
    tree.setRoot(root);
    expect(first->parent() == root.get());

    auto replaceable = std::make_shared<rgui::Button>("replaceable");
    auto replacement = std::make_shared<rgui::Table>(1);
    bool replaced_callback_called = false;
    replaceable->setOnClick([&replaced_callback_called](rgui::Node&) { replaced_callback_called = true; });
    root->append(replaceable);
    replaceable->activate();
    const rgui::NodePtr replaced = root->replace(*replaceable, replacement);
    expect(replaced == replaceable && replaceable->parent() == nullptr && replacement->parent() == root.get() &&
           root->children().size() == 3 && root->children()[2] == replacement && tree.flushEvents() == 0 &&
           !replaced_callback_called);
    bool rejected_null_replacement = false;
    bool rejected_parented_replacement = false;
    bool rejected_cycle_replacement = false;
    try { (void)root->replace(*replacement, nullptr); } catch (const std::invalid_argument&) { rejected_null_replacement = true; }
    try { (void)root->replace(*replacement, first); } catch (const std::logic_error&) { rejected_parented_replacement = true; }
    try { (void)root->replace(*replacement, root); } catch (const std::logic_error&) { rejected_cycle_replacement = true; }
    expect(rejected_null_replacement && rejected_parented_replacement && rejected_cycle_replacement &&
           root->children()[2] == replacement);

    auto anchored_old = std::make_shared<rgui::Text>("anchored old");
    auto anchored_new = std::make_shared<rgui::Button>("anchored new");
    rgui::AnchoredPanel replacement_panel({100.0F, 50.0F});
    const rgui::Anchor primary{{0.25F, 0.0F}, {0.25F, 0.0F}, 4.0F, 8.0F};
    const rgui::Anchor secondary{{0.75F, 1.0F}, {0.75F, 1.0F}, -4.0F, -8.0F};
    replacement_panel.append(anchored_old, primary, secondary);
    (void)replacement_panel.replace(*anchored_old, anchored_new);
    const rgui::Anchor replaced_primary = replacement_panel.anchor(*anchored_new);
    const std::optional<rgui::Anchor>& replaced_secondary = replacement_panel.secondAnchor(*anchored_new);
    expect(replaced_primary.self == primary.self && replaced_primary.target == primary.target &&
           replaced_primary.offsetX == primary.offsetX && replaced_primary.offsetY == primary.offsetY &&
           replaced_secondary && replaced_secondary->self == secondary.self &&
           replaced_secondary->target == secondary.target && replaced_secondary->offsetX == secondary.offsetX &&
           replaced_secondary->offsetY == secondary.offsetY);

    rgui::Window overlay("overlay");
    expect(overlay.backgroundAlpha() == 1.0F && overlay.decorated() && overlay.movable() && overlay.resizable());
    overlay.setBackgroundAlpha(0.4F);
    overlay.setDecorated(false);
    overlay.setMovable(false);
    overlay.setResizable(false);
    bool rejected_window_alpha = false;
    try { overlay.setBackgroundAlpha(1.1F); } catch (const std::invalid_argument&) { rejected_window_alpha = true; }
    expect(rejected_window_alpha && overlay.backgroundAlpha() == 0.4F && !overlay.decorated() &&
           !overlay.movable() && !overlay.resizable());

    bool activated = false;
    first->setOnClick([&activated](rgui::Node&) { activated = true; });
    first->activate();
    expect(!activated);
    expect(tree.flushEvents() == 1);
    expect(activated);
    activated = false;
    first->setEnabled(false);
    first->activate();
    expect(tree.flushEvents() == 0);

    bool snapshot_callback = false;
    first->setEnabled(true);
    first->setOnClick([&snapshot_callback](rgui::Node&) { snapshot_callback = true; });
    first->activate();
    first->setOnClick([](rgui::Node&) {});
    first->setLabel("changed while queued");
    expect(tree.flushEvents() == 1);
    expect(snapshot_callback);

    auto clickable_text = std::make_shared<rgui::Text>("clickable");
    root->append(clickable_text);
    rgui::Node* clicked_text_target = nullptr;
    clickable_text->setOnClick([&clicked_text_target](rgui::Node& node) {
        clicked_text_target = &node;
    });
    clickable_text->activate();
    expect(clicked_text_target == nullptr);
    expect(tree.flushEvents() == 1);
    expect(clicked_text_target == clickable_text.get());
    clicked_text_target = nullptr;
    clickable_text->setEnabled(false);
    clickable_text->activate();
    expect(tree.flushEvents() == 0);
    clickable_text->setEnabled(true);

    bool text_snapshot_callback = false;
    clickable_text->setOnClick([&text_snapshot_callback](rgui::Node&) {
        text_snapshot_callback = true;
    });
    clickable_text->activate();
    clickable_text->setOnClick([](rgui::Node&) {});
    expect(tree.flushEvents() == 1);
    expect(text_snapshot_callback);

    bool detached_text_called = false;
    clickable_text->setOnClick([&detached_text_called](rgui::Node&) {
        detached_text_called = true;
    });
    clickable_text->activate();
    const rgui::NodePtr removed_text = root->remove(*clickable_text);
    expect(tree.flushEvents() == 0);
    expect(!detached_text_called);

    auto self_removing = std::make_shared<rgui::Button>("remove me");
    root->append(self_removing);
    self_removing->setOnClick([&root](rgui::Node& button) { (void)root->remove(button); });
    self_removing->activate();
    expect(tree.flushEvents() == 1);
    expect(self_removing->parent() == nullptr);

    auto discarded = std::make_shared<rgui::Button>("discarded");
    bool discarded_called = false;
    discarded->setOnClick([&discarded_called](rgui::Node&) { discarded_called = true; });
    root->append(discarded);
    discarded->activate();
    const rgui::NodePtr removed_discarded = root->remove(*discarded);
    expect(tree.flushEvents() == 0);
    expect(!discarded_called);

    int deferred_dispatches = 0;
    first->setOnClick([&deferred_dispatches](rgui::Node& node) {
        ++deferred_dispatches;
        if (deferred_dispatches == 1) static_cast<rgui::Button&>(node).activate();
    });
    first->activate();
    expect(tree.flushEvents() == 1);
    expect(tree.pendingEventCount() == 1);
    expect(tree.flushEvents() == 1);
    expect(deferred_dispatches == 2);

    auto transferable_root = std::make_shared<rgui::Window>("transferable");
    {
        rgui::UiTree temporary_tree;
        temporary_tree.setRoot(transferable_root);
    }
    rgui::UiTree replacement_tree;
    replacement_tree.setRoot(transferable_root);
    expect(replacement_tree.root() == transferable_root);

    auto moved_root = std::make_shared<rgui::Window>("moved");
    auto moved_button = std::make_shared<rgui::Button>("moved button");
    bool moved_callback = false;
    moved_button->setOnClick([&moved_callback](rgui::Node&) { moved_callback = true; });
    moved_root->append(moved_button);
    rgui::UiTree original_tree;
    original_tree.setRoot(moved_root);
    moved_button->activate();
    rgui::UiTree moved_tree = std::move(original_tree);
    expect(moved_tree.flushEvents() == 1 && moved_callback);

    const rgui::NodePtr detached = root->remove(*second);
    expect(detached.get() == second.get());
    expect(second->parent() == nullptr);
    bool rejected_multiple_parent = false;
    try { root->append(first); } catch (const std::logic_error&) { rejected_multiple_parent = true; }
    expect(rejected_multiple_parent);

    auto panel = std::make_shared<rgui::AnchoredPanel>(rgui::Size{100.0F, 50.0F});
    auto panel_origin = std::make_shared<RecordingNode>(rgui::Size{1.0F, 1.0F});
    auto centred = std::make_shared<RecordingNode>(rgui::Size{20.0F, 10.0F});
    auto stretched = std::make_shared<ProposalRecordingNode>();
    panel->append(panel_origin);
    panel->append(centred, {{0.5F, 0.0F}, {0.5F, 0.0F}});
    panel->append(stretched,
                  {{0.0F, 0.0F}, {0.0F, 0.0F}, 10.0F, 15.0F},
                  {{1.0F, 0.0F}, {1.0F, 0.0F}, -10.0F, 15.0F});
    root->append(panel);
    expect(panel->measure().width == 100.0F && panel->measure().height == 50.0F);
    expect(panel->anchor(*centred).self == rgui::AnchorPoint{0.5F, 0.0F});
    bool rejected_invalid_anchor = false;
    try {
        panel->setAnchor(*centred, {{-0.1F, 0.0F}, {0.0F, 0.0F}});
    } catch (const std::invalid_argument&) {
        rejected_invalid_anchor = true;
    }
    expect(rejected_invalid_anchor);

    auto table = std::make_shared<rgui::Table>(2);
    table->setHeader(0, "Name");
    table->setHeader(1, "Value");
    table->setCell(0, 0, std::make_shared<rgui::Text>("Health"));
    table->setCell(0, 1, std::make_shared<rgui::Text>("100"));
    root->append(table);
    expect(table->columns() == 2 && table->rows() == 1 && table->header(0) == "Name" &&
           table->cell(0, 0) != nullptr && table->cell(0, 1) != nullptr);
    const rgui::Color red{1.0F, 0.0F, 0.0F, 1.0F};
    expect(red.red() == 1.0F && red.green() == 0.0F && red.blue() == 0.0F && red.alpha() == 1.0F);
    bool rejected_color = true;
    for (const float invalid : {-0.1F, 1.1F, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        bool component_rejected = false;
        try { static_cast<void>(rgui::Color(invalid, 0.0F, 0.0F, 1.0F)); }
        catch (const std::invalid_argument&) { component_rejected = true; }
        rejected_color = rejected_color && component_rejected;
    }
    expect(rejected_color);
    table->setRowColor(3, red);
    expect(table->rows() == 4 && table->rowColor(3) && *table->rowColor(3) == red && !table->rowColor(2));
    table->clearRowColor(3);
    expect(!table->rowColor(3));
    expect(table->innerHorizontalBorders() && table->outerHorizontalBorders() &&
           table->innerVerticalBorders() && table->outerVerticalBorders());
    table->setInnerHorizontalBorders(false);
    expect(!table->innerHorizontalBorders() && table->outerHorizontalBorders() &&
           table->innerVerticalBorders() && table->outerVerticalBorders());
    table->setOuterHorizontalBorders(false);
    expect(!table->innerHorizontalBorders() && !table->outerHorizontalBorders() &&
           table->innerVerticalBorders() && table->outerVerticalBorders());
    table->setHorizontalBorders(true);
    expect(table->innerHorizontalBorders() && table->outerHorizontalBorders() &&
           table->innerVerticalBorders() && table->outerVerticalBorders());
    table->setInnerVerticalBorders(false);
    expect(table->innerHorizontalBorders() && table->outerHorizontalBorders() &&
           !table->innerVerticalBorders() && table->outerVerticalBorders());
    table->setOuterVerticalBorders(false);
    expect(table->innerHorizontalBorders() && table->outerHorizontalBorders() &&
           !table->innerVerticalBorders() && !table->outerVerticalBorders());
    table->setVerticalBorders(false);
    expect(table->innerHorizontalBorders() && table->outerHorizontalBorders() &&
           !table->innerVerticalBorders() && !table->outerVerticalBorders());
    table->setVerticalBorders(true);
    table->setColumnFit(0);
    table->setColumnWeight(1, 2.0F);
    bool rejected_table_column = false;
    bool rejected_table_width = false;
    bool rejected_table_weight = false;
    try { table->setColumnFit(2); } catch (const std::out_of_range&) { rejected_table_column = true; }
    try { table->setColumnWidth(0, 0.0F); } catch (const std::invalid_argument&) { rejected_table_width = true; }
    try { table->setColumnWeight(1, -1.0F); } catch (const std::invalid_argument&) { rejected_table_weight = true; }
    expect(rejected_table_column && rejected_table_width && rejected_table_weight);
    bool rejected_empty_table = false;
    try { static_cast<void>(rgui::Table(0)); } catch (const std::invalid_argument&) { rejected_empty_table = true; }
    expect(rejected_empty_table);

    auto model_table = std::make_shared<rgui::Table>(2);
    expect(model_table->rows() == 0);
    auto retained_cell = std::make_shared<rgui::Button>("retained cell");
    model_table->setCell(2, 1, retained_cell);
    expect(model_table->rows() == 3 && model_table->cell(2, 1) == retained_cell &&
           model_table->cell(0, 0) == nullptr && retained_cell->parent() == model_table.get());
    model_table->setCell(2, 1, retained_cell);
    auto replacement_cell = std::make_shared<rgui::Text>("replacement cell");
    model_table->setCell(2, 1, replacement_cell);
    expect(retained_cell->parent() == nullptr && replacement_cell->parent() == model_table.get());
    model_table->clearCell(2, 1);
    expect(model_table->cell(2, 1) == nullptr && replacement_cell->parent() == nullptr);
    model_table->setCell(1, 0, replacement_cell);
    model_table->resizeRows(1);
    expect(model_table->rows() == 1 && replacement_cell->parent() == nullptr);
    model_table->resizeRows(3);
    expect(model_table->rows() == 3 && model_table->cell(2, 1) == nullptr);
    bool rejected_cell_row = false;
    bool rejected_cell_column = false;
    bool rejected_clear_row = false;
    bool rejected_null_cell = false;
    bool rejected_parented_cell = false;
    bool rejected_table_cycle = false;
    try { static_cast<void>(model_table->cell(3, 0)); } catch (const std::out_of_range&) { rejected_cell_row = true; }
    try { static_cast<void>(model_table->cell(0, 2)); } catch (const std::out_of_range&) { rejected_cell_column = true; }
    try { model_table->clearCell(3, 0); } catch (const std::out_of_range&) { rejected_clear_row = true; }
    try { model_table->setCell(0, 0, nullptr); } catch (const std::invalid_argument&) { rejected_null_cell = true; }
    try { model_table->setCell(0, 0, first); } catch (const std::logic_error&) { rejected_parented_cell = true; }
    auto cyclic_table = std::make_shared<rgui::Table>(1);
    try { cyclic_table->setCell(0, 0, cyclic_table); } catch (const std::logic_error&) { rejected_table_cycle = true; }
    expect(rejected_cell_row && rejected_cell_column && rejected_clear_row && rejected_null_cell &&
           rejected_parented_cell && rejected_table_cycle);
    auto table_button = std::make_shared<rgui::Button>("table callback");
    bool detached_table_callback_called = false;
    table_button->setOnClick([&detached_table_callback_called](rgui::Node&) {
        detached_table_callback_called = true;
    });
    model_table->setCell(0, 0, table_button);
    root->append(model_table);
    table_button->activate();
    model_table->clearCell(0, 0);
    expect(tree.flushEvents() == 0 && !detached_table_callback_called);
    auto destructor_cell = std::make_shared<rgui::Text>("destructor cell");
    {
        auto temporary_table = std::make_shared<rgui::Table>(1);
        temporary_table->setCell(0, 0, destructor_cell);
    }
    expect(destructor_cell->parent() == nullptr);

    auto justified_table = std::make_shared<rgui::Table>(3);
    for (std::size_t column = 0; column < justified_table->columns(); ++column) {
        justified_table->setColumnWidth(column, 100.0F);
    }
    justified_table->setColumnJustify(0, rgui::Justification::start, rgui::Justification::start);
    justified_table->setColumnJustify(1, rgui::Justification::center, rgui::Justification::center);
    justified_table->setColumnJustify(2, rgui::Justification::end, rgui::Justification::end);
    bool rejected_justification_column = false;
    try {
        justified_table->setColumnJustify(3, rgui::Justification::start, rgui::Justification::start);
    } catch (const std::out_of_range&) {
        rejected_justification_column = true;
    }
    expect(rejected_justification_column);
    auto justified_start = std::make_shared<TablePositionRecordingNode>(rgui::Size{20.0F, 30.0F});
    auto justified_center = std::make_shared<TablePositionRecordingNode>(rgui::Size{20.0F, 20.0F});
    auto justified_end = std::make_shared<TablePositionRecordingNode>(rgui::Size{20.0F, 10.0F});
    justified_table->setCell(0, 0, justified_start);
    justified_table->setCell(0, 1, justified_center);
    justified_table->setCell(0, 2, justified_end);

    auto score_table = std::make_shared<rgui::Table>(3);
    score_table->setHeader(0, "Player");
    score_table->setHeader(1, "Score");
    score_table->setHeader(2, "Details");
    score_table->setColumnFit(0);
    score_table->setColumnWidth(1, 96.0F);
    score_table->setColumnWeight(2, 2.0F);
    auto player_width = std::make_shared<TableWidthRecordingNode>();
    auto score_width = std::make_shared<TableWidthRecordingNode>();
    auto details_width = std::make_shared<TableWidthRecordingNode>();
    score_table->setCell(0, 0, player_width);
    score_table->setCell(0, 1, score_width);
    score_table->setCell(0, 2, details_width);

    auto default_table = std::make_shared<rgui::Table>(2);
    auto default_first_width = std::make_shared<TableWidthRecordingNode>();
    auto default_second_width = std::make_shared<TableWidthRecordingNode>();
    default_table->setCell(0, 0, default_first_width);
    default_table->setCell(0, 1, default_second_width);

    auto replace_fit_parent = std::make_shared<rgui::Stack>();
    auto replace_fit_old = std::make_shared<rgui::Text>("old table");
    auto replace_fit_new = std::make_shared<rgui::Table>(2);
    auto replace_fit_width = std::make_shared<TableWidthRecordingNode>(rgui::Size{175.0F, 10.0F});
    replace_fit_new->setHeader(0, "Name");
    replace_fit_new->setColumnFit(0);
    replace_fit_new->setColumnWeight(1, 1.0F);
    replace_fit_new->setCell(0, 0, replace_fit_width);
    replace_fit_new->setCell(0, 1, std::make_shared<rgui::Text>("value"));
    replace_fit_parent->append(replace_fit_old);
    (void)replace_fit_parent->replace(*replace_fit_old, replace_fit_new);

    auto logical_rows = std::make_shared<rgui::Table>(2);
    auto hidden_slot = std::make_shared<TablePositionRecordingNode>(rgui::Size{20.0F, 10.0F});
    auto first_visible_slot = std::make_shared<TablePositionRecordingNode>(rgui::Size{20.0F, 10.0F});
    auto next_row_first_slot = std::make_shared<TablePositionRecordingNode>(rgui::Size{20.0F, 10.0F});
    hidden_slot->setVisible(false);
    logical_rows->setCell(0, 0, hidden_slot);
    logical_rows->setCell(0, 1, first_visible_slot);
    logical_rows->setCell(1, 0, next_row_first_slot);
    logical_rows->setRowColor(1, rgui::Color{0.0F, 1.0F, 0.0F, 1.0F});

    auto scrollArea = std::make_shared<rgui::ScrollArea>(rgui::Size{160.0F, 40.0F});
    auto scroll_contents = std::make_shared<ScrollRecordingNode>();
    scrollArea->append(scroll_contents);
    root->append(scrollArea);
    expect(scrollArea->size().width == 160.0F && scrollArea->size().height == 40.0F);
    bool replacement_during_draw_rejected = false;
    root->append(std::make_shared<ReplaceDuringDrawNode>(*root, std::make_shared<rgui::Text>("replacement"),
                                                         replacement_during_draw_rejected));
    bool table_mutation_during_draw_rejected = false;
    auto mutation_table = std::make_shared<rgui::Table>(1);
    mutation_table->setCell(0, 0,
        std::make_shared<SetTableCellDuringDrawNode>(*mutation_table, table_mutation_during_draw_rejected));
    root->append(mutation_table);

    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = {640.0F, 480.0F};
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();

    auto stable_id_root = std::make_shared<rgui::Window>("stable id test root");
    auto stable_id_button = std::make_shared<rgui::Button>("before");
    auto stable_id_text = std::make_shared<rgui::Text>("text before");
    stable_id_root->append(stable_id_button);
    stable_id_root->append(stable_id_text);
    rgui::UiTree stable_id_tree;
    stable_id_tree.setRoot(stable_id_root);
    bool stable_id_button_clicked = false;
    stable_id_button->setOnClick([&stable_id_button_clicked](rgui::Node&) {
        stable_id_button_clicked = true;
    });

    ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("stable widget identity");
    stable_id_button->draw();
    const ImGuiID button_id_before = ImGui::GetItemID();
    const ImVec2 button_min = ImGui::GetItemRectMin();
    const ImVec2 button_max = ImGui::GetItemRectMax();
    stable_id_text->draw();
    const ImGuiID text_id_before = ImGui::GetItemID();
    ImGui::End();
    ImGui::EndFrame();

    stable_id_button->setLabel("after");
    stable_id_text->setValue("text after");
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("stable widget identity");
    stable_id_button->draw();
    const ImGuiID button_id_after = ImGui::GetItemID();
    stable_id_text->draw();
    const ImGuiID text_id_after = ImGui::GetItemID();
    ImGui::End();
    ImGui::EndFrame();
    expect(button_id_before == button_id_after && text_id_before == text_id_after &&
           button_id_before != 0 && text_id_before != 0);

    ImGui::GetIO().MousePos = {(button_min.x + button_max.x) * 0.5F,
                               (button_min.y + button_max.y) * 0.5F};
    ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = true;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("stable widget identity");
    stable_id_button->draw();
    ImGui::End();
    ImGui::EndFrame();

    ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = false;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("stable widget identity");
    stable_id_button->draw();
    ImGui::End();
    ImGui::EndFrame();
    expect(stable_id_tree.flushEvents() == 1 && stable_id_button_clicked);

    ImGui::GetIO().MousePos = {-1000.0F, -1000.0F};
    ImGui::NewFrame();

    rgui::Text scaled_text("font scaling");
    const rgui::Size normal_text_size = scaled_text.measure();
    const float font_size_before = ImGui::GetFontSize();
    scaled_text.setFontScale(2.0F);
    const rgui::Size scaled_text_size = scaled_text.measure();
    expect(std::fabs(scaled_text_size.width - normal_text_size.width * 2.0F) < 1.0F &&
           std::fabs(scaled_text_size.height - normal_text_size.height * 2.0F) < 1.0F &&
           ImGui::GetFontSize() == font_size_before);
    scaled_text.draw();
    expect(ImGui::GetFontSize() == font_size_before);

    rgui::Button scaled_button("font scaling");
    const rgui::Size normal_button_size = scaled_button.measure();
    const ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
    scaled_button.setFontScale(2.0F);
    const rgui::Size scaled_button_size = scaled_button.measure();
    expect(std::fabs(scaled_button_size.width -
                     ((normal_button_size.width - frame_padding.x * 2.0F) * 2.0F + frame_padding.x * 2.0F)) < 1.0F &&
           std::fabs(scaled_button_size.height -
                     ((normal_button_size.height - frame_padding.y * 2.0F) * 2.0F + frame_padding.y * 2.0F)) < 1.0F &&
           ImGui::GetFontSize() == font_size_before);
    scaled_button.draw();
    expect(ImGui::GetFontSize() == font_size_before);
    CustomNode custom;
    custom.draw();
    tree.draw();
    expect(replacement_during_draw_rejected && table_mutation_during_draw_rejected);
    ImGui::SetNextWindowSize({500.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("replacement fit table test");
    replace_fit_parent->draw();
    ImGui::End();
    expect(replace_fit_width->width() >= 174.0F);
    overlay.setScreenLayout({{200.0F, 100.0F}, rgui::PanelExtent::fixed, rgui::PanelExtent::fixed,
                             {{0.5F, 0.5F}, {0.5F, 0.5F}}});
    overlay.draw();
    const std::string overlay_name = "overlay###rgui-" + std::to_string(overlay.id());
    ImGuiWindow* overlay_window = ImGui::FindWindowByName(overlay_name.c_str());
    expect(overlay.screenLayout() && overlay_window && std::fabs(overlay_window->Pos.x - 220.0F) < 1.0F &&
           std::fabs(overlay_window->Pos.y - 190.0F) < 1.0F && std::fabs(overlay_window->Size.x - 200.0F) < 1.0F &&
           std::fabs(overlay_window->Size.y - 100.0F) < 1.0F &&
           (overlay_window->Flags & (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) ==
               (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize));
    expect(centred->position().x - panel_origin->position().x == 40.0F);
    expect(centred->position().y == panel_origin->position().y);
    expect(stretched->proposal().width && *stretched->proposal().width == 80.0F);
    expect(!stretched->proposal().height);
    expect(stretched->drawn_size().width == 80.0F && stretched->drawn_size().height == 8.0F);
    expect(first->measure(rgui::SizeProposal{120.0F, std::nullopt}).width == 120.0F);
    ImGui::Begin("fill panel test");
    auto fill_panel = std::make_shared<rgui::AnchoredPanel>(
        rgui::Size{0.0F, 0.0F}, rgui::PanelExtent::fill, rgui::PanelExtent::fill);
    const rgui::Size available{ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y};
    const rgui::Size filled = fill_panel->measure();
    expect(filled.width == available.width && filled.height == available.height);
    fill_panel->draw();
    ImGui::End();
    ImGui::Begin("scroll area test");
    scrollArea->draw();
    ImGui::End();
    ImGui::SetNextWindowSize({500.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("score table test");
    score_table->draw();
    ImGui::End();
    expect(score_width->width() >= 95.0F && score_width->width() <= 97.0F);
    expect(details_width->width() > score_width->width());
    expect(player_width->width() > 0.0F);
    ImGui::SetNextWindowSize({500.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("default table sizing test");
    default_table->draw();
    ImGui::End();
    expect(default_first_width->width() >= default_second_width->width() - 1.0F &&
           default_first_width->width() <= default_second_width->width() + 1.0F);
    ImGui::SetNextWindowSize({400.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("logical table rows test");
    logical_rows->draw();
    ImGui::End();
    expect(first_visible_slot->position().x > next_row_first_slot->position().x + 1.0F &&
           next_row_first_slot->position().y > first_visible_slot->position().y + 1.0F);
    ImGui::SetNextWindowSize({400.0F, 200.0F}, ImGuiCond_Always);
    ImGui::Begin("justified table test");
    justified_table->draw();
    ImGui::End();
    const float center_offset = justified_start->remainingWidth() - justified_center->remainingWidth();
    const float end_offset = justified_start->remainingWidth() - justified_end->remainingWidth();
    const float column_pitch = justified_center->position().x - justified_start->position().x - center_offset;
    expect(std::fabs(center_offset - 40.0F) < 1.0F);
    expect(std::fabs(end_offset - 80.0F) < 1.0F);
    expect(std::fabs((justified_end->position().x - justified_start->position().x - end_offset) -
                     column_pitch * 2.0F) < 1.0F);
    expect(std::fabs((justified_center->position().y - justified_start->position().y) - 5.0F) < 1.0F);
    expect(std::fabs((justified_end->position().y - justified_start->position().y) - 20.0F) < 1.0F);
    ImGui::EndFrame();
    ImGui::NewFrame();
    rgui::WindowLayout stretched_layout{{0.0F, 0.0F}, rgui::PanelExtent::fixed, rgui::PanelExtent::fixed,
                                        {{0.0F, 0.0F}, {0.0F, 0.0F}}};
    stretched_layout.secondary = rgui::Anchor{{1.0F, 1.0F}, {1.0F, 1.0F}};
    overlay.setScreenLayout(stretched_layout);
    overlay.draw();
    expect(std::fabs(overlay_window->Size.x - 640.0F) < 1.0F && std::fabs(overlay_window->Size.y - 480.0F) < 1.0F);
    ImGui::Begin("scroll area test");
    scrollArea->draw();
    ImGui::End();
    expect(scroll_contents->scroll_max_y() > 0.0F);
    ImGui::EndFrame();
    ImGui::NewFrame();
    overlay.setScreenLayout({{0.0F, 0.0F}, rgui::PanelExtent::fill, rgui::PanelExtent::fill,
                             {{0.0F, 0.0F}, {0.0F, 0.0F}}});
    overlay.draw();
    expect(std::fabs(overlay_window->Size.x - 640.0F) < 1.0F && std::fabs(overlay_window->Size.y - 480.0F) < 1.0F);
    overlay.clearScreenLayout();
    expect(!overlay.screenLayout());
    ImGui::EndFrame();

    rgui::UiTree layout_tree;
    expect(layout_tree.layoutScale() == 1.0F);
    for (const float invalid : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try { layout_tree.setLayoutScale(invalid); } catch (const std::invalid_argument&) { rejected = true; }
        expect(rejected && layout_tree.layoutScale() == 1.0F);
    }

    auto scale_root = std::make_shared<rgui::Window>("layout scale test");
    scale_root->setDecorated(false);
    scale_root->setMovable(false);
    scale_root->setResizable(false);
    scale_root->setScreenLayout({{200.0F, 100.0F}, rgui::PanelExtent::fixed, rgui::PanelExtent::fixed,
                                 {{0.5F, 0.5F}, {0.5F, 0.5F}, 10.0F, 20.0F}});
    auto scale_stack = std::make_shared<rgui::Stack>();
    auto scale_panel = std::make_shared<rgui::AnchoredPanel>(rgui::Size{100.0F, 50.0F});
    auto scale_proposal = std::make_shared<ProposalRecordingNode>();
    scale_panel->append(scale_proposal,
                        {{0.0F, 0.0F}, {0.0F, 0.0F}, 10.0F, 5.0F},
                        {{1.0F, 1.0F}, {1.0F, 1.0F}, -10.0F, -5.0F});
    scale_stack->append(scale_panel);
    auto scale_scroll = std::make_shared<rgui::ScrollArea>(rgui::Size{80.0F, 30.0F});
    auto scale_scroll_contents = std::make_shared<ScrollRecordingNode>();
    scale_scroll->append(scale_scroll_contents);
    scale_stack->append(scale_scroll);
    auto scale_table = std::make_shared<rgui::Table>(3);
    scale_table->setColumnWidth(0, 40.0F);
    scale_table->setColumnFit(1);
    scale_table->setColumnWeight(2, 2.0F);
    auto scale_fixed_width = std::make_shared<TableWidthRecordingNode>();
    auto scale_fit_width = std::make_shared<TableWidthRecordingNode>(rgui::Size{50.0F, 10.0F});
    auto scale_stretch_width = std::make_shared<TableWidthRecordingNode>();
    scale_table->setCell(0, 0, scale_fixed_width);
    scale_table->setCell(0, 1, scale_fit_width);
    scale_table->setCell(0, 2, scale_stretch_width);
    scale_stack->append(scale_table);
    auto scale_fill_panel = std::make_shared<rgui::AnchoredPanel>(
        rgui::Size{0.0F, 0.0F}, rgui::PanelExtent::fill, rgui::PanelExtent::fill);
    scale_stack->append(scale_fill_panel);
    bool scale_change_rejected = false;
    scale_stack->append(std::make_shared<ScaleMutationNode>(layout_tree, scale_change_rejected));
    auto scale_text = std::make_shared<rgui::Text>("natural text");
    auto scale_button = std::make_shared<rgui::Button>("natural button");
    rgui::Text natural_text_baseline("natural text");
    rgui::Button natural_button_baseline("natural button");
    rgui::Size natural_text_size;
    rgui::Size natural_button_size;
    scale_stack->append(scale_text);
    scale_stack->append(scale_button);
    scale_root->append(scale_stack);
    layout_tree.setRoot(scale_root);
    auto detached_scale_node = std::make_shared<LayoutScaleNode>();
    expect(detached_scale_node->effectiveScale() == 1.0F);
    auto attached_scale_node = std::make_shared<LayoutScaleNode>();
    scale_stack->append(attached_scale_node);

    layout_tree.setLayoutScale(2.0F);
    expect(layout_tree.layoutScale() == 2.0F && attached_scale_node->effectiveScale() == 2.0F &&
           scale_panel->size().width == 100.0F && scale_panel->size().height == 50.0F &&
           scale_scroll->size().width == 80.0F && scale_scroll->size().height == 30.0F &&
           scale_root->screenLayout()->size.width == 200.0F &&
           scale_root->screenLayout()->size.height == 100.0F);
    ImGui::NewFrame();
    natural_text_size = natural_text_baseline.measure();
    natural_button_size = natural_button_baseline.measure();
    layout_tree.draw();
    const std::string scale_window_name = "layout scale test###rgui-" + std::to_string(scale_root->id());
    ImGuiWindow* scale_window = ImGui::FindWindowByName(scale_window_name.c_str());
    expect(scale_window && std::fabs(scale_window->Pos.x - 140.0F) < 1.0F &&
           std::fabs(scale_window->Pos.y - 180.0F) < 1.0F &&
           std::fabs(scale_window->Size.x - 400.0F) < 1.0F &&
           std::fabs(scale_window->Size.y - 200.0F) < 1.0F);
    expect(scale_proposal->proposal().width && *scale_proposal->proposal().width == 160.0F &&
           scale_proposal->proposal().height && *scale_proposal->proposal().height == 80.0F &&
           scale_proposal->drawn_size().width == 160.0F && scale_proposal->drawn_size().height == 80.0F);
    expect(std::fabs(scale_scroll_contents->window_size().x - 160.0F) < 1.0F &&
           std::fabs(scale_scroll_contents->window_size().y - 60.0F) < 1.0F);
    const float fixed_table_width_scaled = scale_fixed_width->width();
    const float fit_table_width_scaled = scale_fit_width->width();
    expect(fixed_table_width_scaled >= 78.0F && fixed_table_width_scaled <= 82.0F &&
           fit_table_width_scaled >= 49.0F && fit_table_width_scaled <= 51.0F &&
           std::fabs(scale_text->measure().width - natural_text_size.width) < 0.01F &&
           std::fabs(scale_text->measure().height - natural_text_size.height) < 0.01F &&
           std::fabs(scale_button->measure().width - natural_button_size.width) < 0.01F &&
           std::fabs(scale_button->measure().height - natural_button_size.height) < 0.01F);
    ImGui::Begin("layout fill scale test");
    const ImVec2 fill_available = ImGui::GetContentRegionAvail();
    const rgui::Size filled_scale_panel = scale_fill_panel->measure();
    expect(std::fabs(filled_scale_panel.width - fill_available.x) < 0.01F &&
           std::fabs(filled_scale_panel.height - fill_available.y) < 0.01F);
    ImGui::End();
    ImGui::EndFrame();

    layout_tree.setLayoutScale(1.0F);
    expect(layout_tree.layoutScale() == 1.0F);
    ImGui::NewFrame();
    layout_tree.draw();
    scale_window = ImGui::FindWindowByName(scale_window_name.c_str());
    expect(scale_window && std::fabs(scale_window->Pos.x - 230.0F) < 1.0F &&
           std::fabs(scale_window->Pos.y - 210.0F) < 1.0F &&
           std::fabs(scale_window->Size.x - 200.0F) < 1.0F &&
           std::fabs(scale_window->Size.y - 100.0F) < 1.0F);
    expect(scale_proposal->proposal().width && *scale_proposal->proposal().width == 80.0F &&
           scale_proposal->proposal().height && *scale_proposal->proposal().height == 40.0F &&
           scale_proposal->drawn_size().width == 80.0F && scale_proposal->drawn_size().height == 40.0F);
    expect(std::fabs(scale_scroll_contents->window_size().x - 80.0F) < 1.0F &&
           std::fabs(scale_scroll_contents->window_size().y - 30.0F) < 1.0F &&
           std::fabs(scale_fixed_width->width() * 2.0F - fixed_table_width_scaled) < 2.0F &&
           std::fabs(scale_fit_width->width() - fit_table_width_scaled) < 1.0F);
    expect(scale_change_rejected);
    ImGui::EndFrame();

    rgui::WindowLayout secondary_window_layout{{1.0F, 1.0F}, rgui::PanelExtent::fixed,
                                               rgui::PanelExtent::fixed,
                                               {{0.0F, 0.0F}, {0.0F, 0.0F}, 10.0F, 20.0F}};
    secondary_window_layout.secondary = rgui::Anchor{{1.0F, 1.0F}, {1.0F, 1.0F}, -10.0F, -20.0F};
    scale_root->setScreenLayout(secondary_window_layout);
    layout_tree.setLayoutScale(2.0F);
    ImGui::NewFrame();
    layout_tree.draw();
    scale_window = ImGui::FindWindowByName(scale_window_name.c_str());
    expect(scale_window && std::fabs(scale_window->Pos.x - 20.0F) < 1.0F &&
           std::fabs(scale_window->Pos.y - 40.0F) < 1.0F &&
           std::fabs(scale_window->Size.x - 600.0F) < 1.0F &&
           std::fabs(scale_window->Size.y - 400.0F) < 1.0F);
    ImGui::EndFrame();

    layout_tree.setLayoutScale(1.0F);
    ImGui::NewFrame();
    layout_tree.draw();
    scale_window = ImGui::FindWindowByName(scale_window_name.c_str());
    expect(scale_window && std::fabs(scale_window->Pos.x - 10.0F) < 1.0F &&
           std::fabs(scale_window->Pos.y - 20.0F) < 1.0F &&
           std::fabs(scale_window->Size.x - 620.0F) < 1.0F &&
           std::fabs(scale_window->Size.y - 440.0F) < 1.0F);
    ImGui::EndFrame();

    layout_tree.setLayoutScale(2.0F);
    rgui::UiTree moved_layout_tree = std::move(layout_tree);
    expect(moved_layout_tree.layoutScale() == 2.0F);
    rgui::UiTree assigned_layout_tree;
    assigned_layout_tree.setLayoutScale(4.0F);
    assigned_layout_tree = std::move(moved_layout_tree);
    expect(assigned_layout_tree.layoutScale() == 2.0F);

    ImGui::DestroyContext();
    return failures == 0 ? 0 : 1;
}
