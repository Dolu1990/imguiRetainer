#include <rgui/rgui.hpp>

#include <imgui.h>

#include <cmath>
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
        for (int index = 0; index < 20; ++index) ImGui::TextUnformatted("scrollable content");
        scroll_max_y_ = ImGui::GetScrollMaxY();
    }
    [[nodiscard]] float scroll_max_y() const noexcept { return scroll_max_y_; }
private:
    float scroll_max_y_ = 0.0F;
};

class TableWidthRecordingNode final : public rgui::Node {
public:
    void draw() override { width_ = ImGui::GetContentRegionAvail().x; }
    [[nodiscard]] float width() const noexcept { return width_; }
private:
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
} // namespace

int main() {
    static_assert(!std::is_copy_constructible_v<rgui::UiTree>);
    static_assert(!std::is_copy_assignable_v<rgui::UiTree>);
    auto root = std::make_shared<rgui::Window>("test window");
    auto first = std::make_shared<rgui::Button>("first");
    auto second = std::make_shared<rgui::Text>("second");
    root->append(first);
    root->append(second);
    rgui::UiTree tree;
    tree.setRoot(root);
    expect(first->parent() == root.get());

    bool activated = false;
    first->setOnClick([&activated](rgui::Button&) { activated = true; });
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
    first->setOnClick([&snapshot_callback](rgui::Button&) { snapshot_callback = true; });
    first->activate();
    first->setOnClick([](rgui::Button&) {});
    first->setLabel("changed while queued");
    expect(tree.flushEvents() == 1);
    expect(snapshot_callback);

    auto self_removing = std::make_shared<rgui::Button>("remove me");
    root->append(self_removing);
    self_removing->setOnClick([&root](rgui::Button& button) { (void)root->remove(button); });
    self_removing->activate();
    expect(tree.flushEvents() == 1);
    expect(self_removing->parent() == nullptr);

    auto discarded = std::make_shared<rgui::Button>("discarded");
    bool discarded_called = false;
    discarded->setOnClick([&discarded_called](rgui::Button&) { discarded_called = true; });
    root->append(discarded);
    discarded->activate();
    const rgui::NodePtr removed_discarded = root->remove(*discarded);
    expect(tree.flushEvents() == 0);
    expect(!discarded_called);

    int deferred_dispatches = 0;
    first->setOnClick([&deferred_dispatches](rgui::Button& button) {
        ++deferred_dispatches;
        if (deferred_dispatches == 1) button.activate();
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
    moved_button->setOnClick([&moved_callback](rgui::Button&) { moved_callback = true; });
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
    table->append(std::make_shared<rgui::Text>("Health"));
    table->append(std::make_shared<rgui::Text>("100"));
    root->append(table);
    expect(table->columns() == 2 && table->header(0) == "Name" && table->children().size() == 2);
    expect(table->verticalBorders());
    table->setVerticalBorders(false);
    expect(!table->verticalBorders());
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
    justified_table->append(justified_start);
    justified_table->append(justified_center);
    justified_table->append(justified_end);

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
    score_table->append(player_width);
    score_table->append(score_width);
    score_table->append(details_width);

    auto default_table = std::make_shared<rgui::Table>(2);
    auto default_first_width = std::make_shared<TableWidthRecordingNode>();
    auto default_second_width = std::make_shared<TableWidthRecordingNode>();
    default_table->append(default_first_width);
    default_table->append(default_second_width);

    auto scrollArea = std::make_shared<rgui::ScrollArea>(rgui::Size{160.0F, 40.0F});
    auto scroll_contents = std::make_shared<ScrollRecordingNode>();
    scrollArea->append(scroll_contents);
    root->append(scrollArea);
    expect(scrollArea->size().width == 160.0F && scrollArea->size().height == 40.0F);

    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = {640.0F, 480.0F};
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    CustomNode custom;
    custom.draw();
    tree.draw();
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
    ImGui::Begin("scroll area test");
    scrollArea->draw();
    ImGui::End();
    expect(scroll_contents->scroll_max_y() > 0.0F);
    ImGui::EndFrame();
    ImGui::DestroyContext();
    return failures == 0 ? 0 : 1;
}
