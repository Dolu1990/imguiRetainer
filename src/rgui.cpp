#include <rgui/rgui.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace rgui {

std::string_view version() noexcept { return "0.1.0"; }

namespace {
std::atomic<NodeId> next_node_id{1};
ImVec2 point_in_rect(AnchorPoint point, Size size);
void validate_anchor(Anchor anchor);
std::optional<float> proposed_axis(float primary_target, float secondary_target,
                                   float primary_self, float secondary_self);
void validate_size(Size size);
void validate_font_scale(float scale);
void validate_color_component(float value);

float scale_length(float value, float scale) { return value * scale; }
Size scale_size(Size size, float scale) {
    size.width = scale_length(size.width, scale);
    size.height = scale_length(size.height, scale);
    return size;
}

class ScopedFontScale final {
public:
    explicit ScopedFontScale(float scale) {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * scale);
    }
    ~ScopedFontScale() { ImGui::PopFont(); }
    ScopedFontScale(const ScopedFontScale&) = delete;
    ScopedFontScale& operator=(const ScopedFontScale&) = delete;
};

std::string node_id_string(NodeId id) {
    return std::to_string(id);
}

void draw_text_with_id(const char* text, const char* text_end, ImGuiID item_id) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    ImGuiContext& context = *GImGui;

    if (text == text_end) text = text_end = "";
    if (!text_end) text_end = text + ImStrlen(text);

    const char* line = text;
    const ImVec2 text_pos(window->DC.CursorPos.x,
                          window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    const float wrap_pos_x = window->DC.TextWrapPos;
    const bool wrap_enabled = wrap_pos_x >= 0.0F;
    if (text_end - text <= 2000 || wrap_enabled) {
        const float wrap_width = wrap_enabled
                                     ? ImGui::CalcWrapWidthForPos(window->DC.CursorPos, wrap_pos_x)
                                     : 0.0F;
        const ImVec2 text_size = ImGui::CalcTextSize(text, text_end, false, wrap_width);
        const ImRect bounds(text_pos, {text_pos.x + text_size.x, text_pos.y + text_size.y});
        ImGui::ItemSize(text_size, 0.0F);
        if (!ImGui::ItemAdd(bounds, item_id)) return;
        ImGui::RenderTextWrapped(bounds.Min, text, text_end, wrap_width);
        return;
    }

    const float line_height = ImGui::GetTextLineHeight();
    ImVec2 position = text_pos;
    ImVec2 text_size(0.0F, 0.0F);
    if (!context.LogEnabled) {
        const int lines_skippable = static_cast<int>((window->ClipRect.Min.y - text_pos.y) / line_height);
        if (lines_skippable > 0) {
            int lines_skipped = 0;
            while (line < text_end && lines_skipped < lines_skippable) {
                const char* line_end = static_cast<const char*>(ImMemchr(line, '\n', text_end - line));
                if (!line_end) line_end = text_end;
                line = line_end + 1;
                ++lines_skipped;
            }
            position.y += lines_skipped * line_height;
        }
    }

    if (line < text_end) {
        ImRect line_bounds(position, {position.x + FLT_MAX, position.y + line_height});
        while (line < text_end) {
            if (ImGui::IsClippedEx(line_bounds, item_id)) break;
            const char* line_end = static_cast<const char*>(ImMemchr(line, '\n', text_end - line));
            if (!line_end) line_end = text_end;
            text_size.x = ImMax(text_size.x, ImGui::CalcTextSize(line, line_end).x);
            ImGui::RenderText(position, line, line_end, false);
            line = line_end + 1;
            line_bounds.Min.y += line_height;
            line_bounds.Max.y += line_height;
            position.y += line_height;
        }

        int lines_skipped = 0;
        while (line < text_end) {
            const char* line_end = static_cast<const char*>(ImMemchr(line, '\n', text_end - line));
            if (!line_end) line_end = text_end;
            line = line_end + 1;
            ++lines_skipped;
        }
        position.y += lines_skipped * line_height;
    }
    text_size.y = position.y - text_pos.y;

    const ImRect bounds(text_pos, {text_pos.x + text_size.x, text_pos.y + text_size.y});
    ImGui::ItemSize(text_size, 0.0F);
    ImGui::ItemAdd(bounds, item_id);
}
}

Node::Node() : id_(next_node_id.fetch_add(1, std::memory_order_relaxed)) {}
void Node::setVisible(bool value) noexcept { visible_ = value; }
void Node::setEnabled(bool value) noexcept { enabled_ = value; }
Size Node::measure() const { return {}; }
Size Node::measure(const SizeProposal&) const { return measure(); }
void Node::draw(Size) { draw(); }
float Node::layoutScale() const noexcept { return tree_ ? tree_->layoutScale() : 1.0F; }
void Node::set_tree_recursive(UiTree* tree) noexcept { if (tree_ != tree) { tree_ = tree; ++attachment_generation_; } }

Color::Color(float red, float green, float blue, float alpha)
    : red_(red), green_(green), blue_(blue), alpha_(alpha) {
    validate_color_component(red);
    validate_color_component(green);
    validate_color_component(blue);
    validate_color_component(alpha);
}

Container::~Container() { for (const NodePtr& child : children_) child->parent_ = nullptr; }
void Container::set_tree_recursive(UiTree* tree) noexcept {
    Node::set_tree_recursive(tree);
    for (const NodePtr& child : children_) child->set_tree_recursive(tree);
}
void Container::append(NodePtr child) {
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    if (!child) throw std::invalid_argument("rgui cannot append a null node");
    if (child->parent_) throw std::logic_error("rgui node already has a parent");
    for (Node* ancestor = this; ancestor; ancestor = ancestor->parent_) {
        if (ancestor == child.get()) throw std::logic_error("rgui cannot introduce a tree cycle");
    }
    child->parent_ = this;
    child->set_tree_recursive(tree_);
    children_.push_back(std::move(child));
}
NodePtr Container::remove(Node& child) {
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    const auto position = std::find_if(children_.begin(), children_.end(), [&child](const NodePtr& candidate) { return candidate.get() == &child; });
    if (position == children_.end()) throw std::logic_error("rgui node is not a child of this container");
    NodePtr result = std::move(*position);
    children_.erase(position);
    result->parent_ = nullptr;
    result->set_tree_recursive(nullptr);
    return result;
}
NodePtr Container::replace(Node& old_child, NodePtr new_child) {
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    if (!new_child) throw std::invalid_argument("rgui cannot replace with a null node");
    const auto position = std::find_if(children_.begin(), children_.end(), [&old_child](const NodePtr& candidate) {
        return candidate.get() == &old_child;
    });
    if (position == children_.end()) throw std::logic_error("rgui node is not a child of this container");
    if (new_child->parent_) throw std::logic_error("rgui replacement node already has a parent");
    for (Node* ancestor = this; ancestor; ancestor = ancestor->parent_) {
        if (ancestor == new_child.get()) throw std::logic_error("rgui cannot introduce a tree cycle");
    }

    NodePtr old_child_ptr = std::move(*position);
    old_child_ptr->parent_ = nullptr;
    old_child_ptr->set_tree_recursive(nullptr);
    new_child->parent_ = this;
    new_child->set_tree_recursive(tree_);
    *position = std::move(new_child);
    return old_child_ptr;
}
void Container::clear() {
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    for (const NodePtr& child : children_) { child->parent_ = nullptr; child->set_tree_recursive(nullptr); }
    children_.clear();
}
void Container::draw_child(Node& child) {
    if (!child.visible()) return;
    const std::string id = std::to_string(child.id());
    ImGui::PushID(id.c_str()); child.draw(); ImGui::PopID();
}
void Container::draw_child(Node& child, Size resolved_size) {
    if (!child.visible()) return;
    const std::string id = std::to_string(child.id());
    ImGui::PushID(id.c_str()); child.draw(resolved_size); ImGui::PopID();
}
void Container::draw_children() {
    for (const NodePtr& child : children_) {
        draw_child(*child);
    }
}
void Container::draw() { draw_children(); }

void Stack::setAxis(Axis value) noexcept { axis_ = value; }
void Stack::draw() {
    bool first = true;
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        if (axis_ == Axis::horizontal && !first) ImGui::SameLine();
        draw_child(*child);
        first = false;
    }
}

Window::Window(std::string title) : title_(std::move(title)) {}
void Window::setTitle(std::string_view value) { title_ = value; }
void Window::setBackgroundAlpha(float value) {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
        throw std::invalid_argument("rgui window background alpha must be finite and from 0 to 1");
    }
    background_alpha_ = value;
}
void Window::setDecorated(bool value) noexcept { decorated_ = value; }
void Window::setMovable(bool value) noexcept { movable_ = value; }
void Window::setResizable(bool value) noexcept { resizable_ = value; }
void Window::setScreenLayout(WindowLayout value) {
    validate_size(value.size);
    validate_anchor(value.primary);
    if (value.secondary) validate_anchor(*value.secondary);
    const bool width_automatic = value.widthExtent == PanelExtent::automatic;
    const bool height_automatic = value.heightExtent == PanelExtent::automatic;
    if (width_automatic != height_automatic) {
        throw std::invalid_argument("rgui window automatic sizing must be used for both dimensions");
    }
    if (width_automatic && value.secondary) {
        throw std::invalid_argument("rgui window automatic sizing cannot use a secondary anchor");
    }
    screen_layout_ = std::move(value);
}
void Window::clearScreenLayout() noexcept { screen_layout_.reset(); }
void Window::draw() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (!decorated_) flags |= ImGuiWindowFlags_NoTitleBar;
    if (!movable_) flags |= ImGuiWindowFlags_NoMove;
    if (!resizable_) flags |= ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(background_alpha_);
    if (screen_layout_) {
        const WindowLayout& layout = *screen_layout_;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float scale = layoutScale();
        const ImVec2 target = point_in_rect(layout.primary.target, {viewport->Size.x, viewport->Size.y});
        const ImVec2 position{viewport->Pos.x + target.x + scale_length(layout.primary.offsetX, scale),
                              viewport->Pos.y + target.y + scale_length(layout.primary.offsetY, scale)};
        if (layout.widthExtent == PanelExtent::automatic) {
            flags |= ImGuiWindowFlags_AlwaysAutoResize;
            ImGui::SetNextWindowPos(position, ImGuiCond_Always,
                                    {layout.primary.self.x, layout.primary.self.y});
        } else {
            Size size = layout.size;
            if (layout.widthExtent == PanelExtent::fill) size.width = viewport->Size.x;
            else size.width = scale_length(size.width, scale);
            if (layout.heightExtent == PanelExtent::fill) size.height = viewport->Size.y;
            else size.height = scale_length(size.height, scale);
            if (layout.secondary) {
                const Anchor& secondary = *layout.secondary;
                const ImVec2 secondary_target = point_in_rect(secondary.target, {viewport->Size.x, viewport->Size.y});
                if (const auto width = proposed_axis(position.x,
                                                     viewport->Pos.x + secondary_target.x + scale_length(secondary.offsetX, scale),
                                                     layout.primary.self.x, secondary.self.x)) size.width = *width;
                if (const auto height = proposed_axis(position.y,
                                                      viewport->Pos.y + secondary_target.y + scale_length(secondary.offsetY, scale),
                                                      layout.primary.self.y, secondary.self.y)) size.height = *height;
            }
            const ImVec2 self = point_in_rect(layout.primary.self, {size.width, size.height});
            ImGui::SetNextWindowPos({position.x - self.x, position.y - self.y}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({size.width, size.height}, ImGuiCond_Always);
        }
    }
    const std::string title = title_ + "###rgui-" + std::to_string(id());
    const bool draw_contents = ImGui::Begin(title.c_str(), nullptr, flags);
    if (draw_contents) draw_children();
    ImGui::End();
}

Table::Table(std::size_t columns) : columns_(columns) {
    if (columns == 0) throw std::invalid_argument("rgui table requires at least one column");
}
Table::~Table() {
    for (Row& configured_row : rows_) {
        for (NodePtr& child : configured_row.cells) {
            if (child) child->parent_ = nullptr;
        }
    }
}
void Table::set_tree_recursive(UiTree* tree) noexcept {
    Node::set_tree_recursive(tree);
    for (Row& configured_row : rows_) {
        for (const NodePtr& child : configured_row.cells) {
            if (child) child->set_tree_recursive(tree);
        }
    }
}
Table::Row& Table::row(std::size_t index) {
    if (index >= rows()) throw std::out_of_range("rgui table row is out of range");
    return rows_[index];
}
const Table::Row& Table::row(std::size_t index) const {
    if (index >= rows()) throw std::out_of_range("rgui table row is out of range");
    return rows_[index];
}
void Table::detach(NodePtr& child) noexcept {
    if (!child) return;
    child->parent_ = nullptr;
    child->set_tree_recursive(nullptr);
    child.reset();
}
void Table::resizeRows(std::size_t count) {
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    if (count < rows()) {
        for (std::size_t index = count; index < rows(); ++index) {
            for (NodePtr& child : rows_[index].cells) detach(child);
        }
        rows_.resize(count);
        return;
    }
    rows_.reserve(count);
    while (rows() < count) rows_.push_back(Row{std::vector<NodePtr>(columns()), std::nullopt});
}
NodePtr Table::cell(std::size_t row_index, std::size_t column_index) const {
    static_cast<void>(column(column_index));
    return row(row_index).cells[column_index];
}
void Table::setCell(std::size_t row_index, std::size_t column_index, NodePtr child) {
    static_cast<void>(column(column_index));
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    if (!child) throw std::invalid_argument("rgui cannot set a null table cell");
    if (row_index < rows() && rows_[row_index].cells[column_index] == child) return;
    if (child->parent_) throw std::logic_error("rgui node already has a parent");
    for (Node* ancestor = this; ancestor; ancestor = ancestor->parent_) {
        if (ancestor == child.get()) throw std::logic_error("rgui cannot introduce a tree cycle");
    }
    if (row_index >= rows()) resizeRows(row_index + 1);
    NodePtr& destination = rows_[row_index].cells[column_index];
    detach(destination);
    child->parent_ = this;
    child->set_tree_recursive(tree_);
    destination = std::move(child);
}
void Table::clearCell(std::size_t row_index, std::size_t column_index) {
    static_cast<void>(column(column_index));
    if (tree_ && tree_->drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    detach(row(row_index).cells[column_index]);
}
void Table::draw_cell(Node& child) {
    if (!child.visible()) return;
    const std::string child_id = std::to_string(child.id());
    ImGui::PushID(child_id.c_str()); child.draw(); ImGui::PopID();
}
Table::Column& Table::column(std::size_t index) {
    if (index >= columns()) throw std::out_of_range("rgui table column is out of range");
    return columns_[index];
}
const Table::Column& Table::column(std::size_t index) const {
    if (index >= columns()) throw std::out_of_range("rgui table column is out of range");
    return columns_[index];
}
void Table::setHeader(std::size_t column, std::string_view value) {
    this->column(column).header = value;
}
std::string_view Table::header(std::size_t column) const {
    return this->column(column).header;
}
void Table::setColumnFit(std::size_t column) {
    Column& configured = this->column(column);
    configured.sizing = ColumnSizing::fit;
    configured.width_or_weight = 0.0F;
}
void Table::setColumnWidth(std::size_t column, float width) {
    if (!std::isfinite(width) || width <= 0.0F) {
        throw std::invalid_argument("rgui table column width must be a positive finite value");
    }
    Column& configured = this->column(column);
    configured.sizing = ColumnSizing::fixed;
    configured.width_or_weight = width;
}
void Table::setColumnWeight(std::size_t column, float weight) {
    if (!std::isfinite(weight) || weight <= 0.0F) {
        throw std::invalid_argument("rgui table column weight must be a positive finite value");
    }
    Column& configured = this->column(column);
    configured.sizing = ColumnSizing::stretch;
    configured.width_or_weight = weight;
}
void Table::setColumnJustify(std::size_t column, Justification horizontal,
                             Justification vertical) {
    Column& configured = this->column(column);
    configured.horizontal_justification = horizontal;
    configured.vertical_justification = vertical;
}
void Table::setRowColor(std::size_t row, Color color) {
    if (row >= rows()) resizeRows(row + 1);
    rows_[row].color = color;
}
void Table::clearRowColor(std::size_t row_index) {
    row(row_index).color.reset();
}
std::optional<Color> Table::rowColor(std::size_t row_index) const {
    return row(row_index).color;
}
void Table::setInnerHorizontalBorders(bool value) noexcept { inner_horizontal_borders_ = value; }
void Table::setOuterHorizontalBorders(bool value) noexcept { outer_horizontal_borders_ = value; }
void Table::setInnerVerticalBorders(bool value) noexcept { inner_vertical_borders_ = value; }
void Table::setOuterVerticalBorders(bool value) noexcept { outer_vertical_borders_ = value; }
void Table::setHorizontalBorders(bool value) noexcept {
    setInnerHorizontalBorders(value);
    setOuterHorizontalBorders(value);
}
void Table::setVerticalBorders(bool value) noexcept {
    setInnerVerticalBorders(value);
    setOuterVerticalBorders(value);
}

namespace {
float justification_offset(Justification justification, float available, float content) {
    const float remaining = std::max(0.0F, available - content);
    switch (justification) {
    case Justification::start: return 0.0F;
    case Justification::center: return remaining * 0.5F;
    case Justification::end: return remaining;
    }
    return 0.0F;
}
} // namespace

void Table::draw() {
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
    if (inner_horizontal_borders_) flags |= ImGuiTableFlags_BordersInnerH;
    if (outer_horizontal_borders_) flags |= ImGuiTableFlags_BordersOuterH;
    if (inner_vertical_borders_) flags |= ImGuiTableFlags_BordersInnerV;
    if (outer_vertical_borders_) flags |= ImGuiTableFlags_BordersOuterV;
    const std::string table_id = "##rgui-table-" + std::to_string(id());
    if (!ImGui::BeginTable(table_id.c_str(), static_cast<int>(columns()), flags)) {
        return;
    }

    // ImGui lays out an automatic-width column before it has seen that frame's
    // cells, so a newly created table gets a deliberately minimal first-frame
    // work rectangle. Retained children already provide their natural sizes;
    // use those sizes as the fixed-column request so a fitted column is usable
    // immediately, including when a table replaces another node.
    std::vector<float> fit_widths(columns(), 1.0F);
    for (std::size_t column_index = 0; column_index < columns(); ++column_index) {
        const Column& configured = columns_[column_index];
        if (configured.sizing != ColumnSizing::fit) continue;
        fit_widths[column_index] = ImGui::CalcTextSize(configured.header.c_str()).x;
        for (const Row& configured_row : rows_) {
            const NodePtr& child = configured_row.cells[column_index];
            if (child && child->visible()) {
                fit_widths[column_index] = std::max(fit_widths[column_index], child->measure().width);
            }
        }
        fit_widths[column_index] = std::max(fit_widths[column_index], 1.0F);
    }

    bool has_headers = false;
    for (std::size_t column_index = 0; column_index < columns(); ++column_index) {
        const Column& configured = columns_[column_index];
        const ImGuiTableColumnFlags column_flags = configured.sizing == ColumnSizing::stretch
                                                       ? ImGuiTableColumnFlags_WidthStretch
                                                       : ImGuiTableColumnFlags_WidthFixed;
        ImGui::TableSetupColumn(configured.header.empty() ? nullptr : configured.header.c_str(),
                                column_flags, configured.sizing == ColumnSizing::fit
                                                  ? fit_widths[column_index]
                                                  : configured.sizing == ColumnSizing::fixed
                                                        ? scale_length(configured.width_or_weight, layoutScale())
                                                        : configured.width_or_weight);
        has_headers = has_headers || !configured.header.empty();
    }
    if (has_headers) ImGui::TableHeadersRow();
    for (std::size_t row_index = 0; row_index < rows(); ++row_index) {
        const Row& configured_row = rows_[row_index];
        float row_height = 0.0F;
        for (const NodePtr& child : configured_row.cells) {
            if (!child || !child->visible()) continue;
            row_height = std::max(row_height, child->measure().height);
        }
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
        if (const std::optional<Color>& color = configured_row.color) {
            const ImU32 packed = ImGui::ColorConvertFloat4ToU32(
                {color->red(), color->green(), color->blue(), color->alpha()});
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, packed);
        }
        for (std::size_t column_index = 0; column_index < columns(); ++column_index) {
            const NodePtr& child = configured_row.cells[column_index];
            if (!child || !child->visible()) continue;
            ImGui::TableSetColumnIndex(static_cast<int>(column_index));
            const Column& configured = columns_[column_index];
            const Size content_size = child->measure();
            const ImVec2 cursor = ImGui::GetCursorPos();
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float offset_x = justification_offset(configured.horizontal_justification,
                                                         available.x, content_size.width);
            const float offset_y = justification_offset(configured.vertical_justification,
                                                         row_height, content_size.height);
            if (offset_x != 0.0F || offset_y != 0.0F) {
                ImGui::SetCursorPos({cursor.x + offset_x, cursor.y + offset_y});
            }
            draw_cell(*child);
        }
    }
    ImGui::EndTable();
}

ScrollArea::ScrollArea(Size size) : size_(size) {
    if (size.width < 0.0F || size.height < 0.0F) {
        throw std::invalid_argument("rgui scroll area size cannot be negative");
    }
}
void ScrollArea::setSize(Size size) {
    if (size.width < 0.0F || size.height < 0.0F) {
        throw std::invalid_argument("rgui scroll area size cannot be negative");
    }
    size_ = size;
}
void ScrollArea::setBordered(bool value) noexcept { bordered_ = value; }
void ScrollArea::draw() {
    const std::string area_id = "##rgui-scroll-area-" + std::to_string(id());
    const Size physical_size = scale_size(size_, layoutScale());
    const ImGuiChildFlags child_flags = bordered_ ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
    const bool draw_contents = ImGui::BeginChild(area_id.c_str(), {physical_size.width, physical_size.height},
                                                 child_flags);
    if (draw_contents) draw_children();
    ImGui::EndChild();
}

namespace {
void validate_font_scale(float scale) {
    if (!std::isfinite(scale) || scale <= 0.0F) {
        throw std::invalid_argument("rgui font scale must be finite and greater than zero");
    }
}
void validate_color_component(float value) {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
        throw std::invalid_argument("rgui color components must be finite values from 0 to 1");
    }
}

ImVec2 point_in_rect(AnchorPoint point, Size size) {
    return {point.x * size.width, point.y * size.height};
}

void validate_anchor_point(AnchorPoint point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0.0F || point.x > 1.0F ||
        point.y < 0.0F || point.y > 1.0F) {
        throw std::invalid_argument("rgui anchor fractions must be finite values from 0 to 1");
    }
}

void validate_anchor(Anchor anchor) {
    validate_anchor_point(anchor.self);
    validate_anchor_point(anchor.target);
}

std::optional<float> proposed_axis(float primary_target, float secondary_target,
                                   float primary_self, float secondary_self) {
    const float denominator = secondary_self - primary_self;
    if (denominator == 0.0F) return std::nullopt;
    const float result = (secondary_target - primary_target) / denominator;
    return result >= 0.0F ? std::optional<float>{result} : std::nullopt;
}

void validate_size(Size size) {
    if (size.width < 0.0F || size.height < 0.0F) {
        throw std::invalid_argument("rgui panel size cannot be negative");
    }
}
void validate_panel_extent(PanelExtent extent) {
    if (extent == PanelExtent::automatic) {
        throw std::invalid_argument("rgui anchored panel dimensions must be fixed or fill");
    }
}
} // namespace

AnchoredPanel::AnchoredPanel(Size size, PanelExtent widthExtent, PanelExtent heightExtent)
    : size_(size), width_extent_(widthExtent), height_extent_(heightExtent) {
    validate_size(size);
    validate_panel_extent(widthExtent);
    validate_panel_extent(heightExtent);
}
void AnchoredPanel::setSize(Size size) { validate_size(size); size_ = size; }
void AnchoredPanel::setWidthExtent(PanelExtent value) { validate_panel_extent(value); width_extent_ = value; }
void AnchoredPanel::setHeightExtent(PanelExtent value) { validate_panel_extent(value); height_extent_ = value; }
Size AnchoredPanel::measure() const {
    Size result = size_;
    const float scale = layoutScale();
    if (width_extent_ == PanelExtent::fixed) result.width = scale_length(result.width, scale);
    if (height_extent_ == PanelExtent::fixed) result.height = scale_length(result.height, scale);
    if (!ImGui::GetCurrentContext()) return result;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (width_extent_ == PanelExtent::fill) result.width = available.x;
    if (height_extent_ == PanelExtent::fill) result.height = available.y;
    return result;
}
void AnchoredPanel::append(NodePtr child) { append(std::move(child), {}); }
void AnchoredPanel::append(NodePtr child, Anchor anchor) {
    validate_anchor(anchor);
    Container::append(std::move(child));
    anchors_.push_back({anchor, std::nullopt});
}
void AnchoredPanel::append(NodePtr child, Anchor primary_anchor, Anchor secondary_anchor) {
    validate_anchor(primary_anchor);
    validate_anchor(secondary_anchor);
    Container::append(std::move(child));
    anchors_.push_back({primary_anchor, secondary_anchor});
}
std::size_t AnchoredPanel::child_index(const Node& child) const {
    const auto position = std::find_if(children_.begin(), children_.end(), [&child](const NodePtr& candidate) {
        return candidate.get() == &child;
    });
    if (position == children_.end()) throw std::logic_error("rgui node is not a child of this anchored panel");
    return static_cast<std::size_t>(position - children_.begin());
}
NodePtr AnchoredPanel::remove(Node& child) {
    const std::size_t index = child_index(child);
    NodePtr result = Container::remove(child);
    anchors_.erase(anchors_.begin() + static_cast<std::ptrdiff_t>(index));
    return result;
}
void AnchoredPanel::clear() {
    Container::clear();
    anchors_.clear();
}
void AnchoredPanel::setAnchor(Node& child, Anchor anchor) {
    validate_anchor(anchor);
    anchors_[child_index(child)].primary = anchor;
}
Anchor AnchoredPanel::anchor(const Node& child) const { return anchors_[child_index(child)].primary; }
void AnchoredPanel::setSecondAnchor(Node& child, std::optional<Anchor> anchor) {
    if (anchor) validate_anchor(*anchor);
    anchors_[child_index(child)].secondary = anchor;
}
const std::optional<Anchor>& AnchoredPanel::secondAnchor(const Node& child) const {
    return anchors_[child_index(child)].secondary;
}
void AnchoredPanel::draw() {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const Size resolved_size = measure();
    for (std::size_t index = 0; index < children_.size(); ++index) {
        Node& child = *children_[index];
        if (!child.visible()) continue;
        const ChildAnchors& child_anchors = anchors_[index];
        const Anchor& child_anchor = child_anchors.primary;
        SizeProposal proposal;
        if (child_anchors.secondary) {
            const Anchor& secondary = *child_anchors.secondary;
            const ImVec2 primary_target = point_in_rect(child_anchor.target, resolved_size);
            const ImVec2 secondary_target = point_in_rect(secondary.target, resolved_size);
            const float scale = layoutScale();
            proposal.width = proposed_axis(primary_target.x + scale_length(child_anchor.offsetX, scale),
                                           secondary_target.x + scale_length(secondary.offsetX, scale),
                                           child_anchor.self.x, secondary.self.x);
            proposal.height = proposed_axis(primary_target.y + scale_length(child_anchor.offsetY, scale),
                                            secondary_target.y + scale_length(secondary.offsetY, scale),
                                            child_anchor.self.y, secondary.self.y);
        }
        const Size child_size = child.measure(proposal);
        const ImVec2 target = point_in_rect(child_anchor.target, resolved_size);
        const ImVec2 self = point_in_rect(child_anchor.self, child_size);
        const float scale = layoutScale();
        ImGui::SetCursorScreenPos({origin.x + target.x + scale_length(child_anchor.offsetX, scale) - self.x,
                                   origin.y + target.y + scale_length(child_anchor.offsetY, scale) - self.y});
        draw_child(child, child_size);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({resolved_size.width, resolved_size.height});
}

Text::Text(std::string value) : value_(std::move(value)) {}
void Text::setValue(std::string_view value) { value_ = value; }
void Text::setFontScale(float scale) { validate_font_scale(scale); font_scale_ = scale; }
void Text::setOnClick(std::function<void(Node&)> callback) { on_click_ = std::move(callback); }
void Text::activate() { if (visible() && enabled() && on_click_ && tree_) tree_->enqueue_event(weak_from_this(), attachment_generation_, on_click_); }
Size Text::measure() const {
    const ScopedFontScale font(font_scale_);
    const ImVec2 size = ImGui::CalcTextSize(value_.data(), value_.data() + value_.size());
    return {size.x, size.y};
}
void Text::draw() {
    {
        const ScopedFontScale font(font_scale_);
        const std::string id = node_id_string(id_);
        draw_text_with_id(value_.data(), value_.data() + value_.size(), ImGui::GetID(id.c_str()));
    }
    if (enabled() && ImGui::IsItemClicked(ImGuiMouseButton_Left)) activate();
}
Button::Button(std::string label) : label_(std::move(label)) {}
void Button::setLabel(std::string_view value) { label_ = value; }
void Button::setFontScale(float scale) { validate_font_scale(scale); font_scale_ = scale; }
void Button::setOnClick(std::function<void(Node&)> callback) { on_click_ = std::move(callback); }
void Button::activate() { if (visible() && enabled() && on_click_ && tree_) tree_->enqueue_event(weak_from_this(), attachment_generation_, on_click_); }
Size Button::measure() const {
    const ScopedFontScale font(font_scale_);
    const ImVec2 text_size = ImGui::CalcTextSize(label_.c_str(), nullptr, true);
    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    return {text_size.x + padding.x * 2.0F, ImGui::GetFrameHeight()};
}
Size Button::measure(const SizeProposal& proposal) const {
    Size result = measure();
    if (proposal.width && *proposal.width > 0.0F) result.width = *proposal.width;
    if (proposal.height && *proposal.height > 0.0F) result.height = *proposal.height;
    return result;
}
void Button::draw() {
    draw(measure());
}
void Button::draw(Size resolved_size) {
    const ScopedFontScale font(font_scale_);
    if (!enabled()) ImGui::BeginDisabled();
    const std::string stable_label = label_ + "###rgui-" + node_id_string(id_);
    const bool clicked = ImGui::Button(stable_label.c_str(), {resolved_size.width, resolved_size.height});
    if (!enabled()) ImGui::EndDisabled();
    if (clicked && enabled()) activate();
}

Selectable::Selectable(std::string label) : label_(std::move(label)) {}
void Selectable::setLabel(std::string_view value) { label_ = value; }
void Selectable::setSelected(bool value) noexcept { selected_ = value; }
void Selectable::setOnClick(std::function<void(Node&)> callback) { on_click_ = std::move(callback); }
void Selectable::activate() {
    if (visible() && enabled() && on_click_ && tree_) {
        tree_->enqueue_event(weak_from_this(), attachment_generation_, on_click_);
    }
}
Size Selectable::measure() const {
    const ImVec2 text_size = ImGui::CalcTextSize(label_.c_str(), nullptr, true);
    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    return {text_size.x + padding.x * 2.0F, text_size.y + padding.y * 2.0F};
}
void Selectable::draw() {
    if (!enabled()) ImGui::BeginDisabled();
    const std::string stable_label = label_ + "###rgui-selectable-" + node_id_string(id_);
    const bool clicked = ImGui::Selectable(stable_label.c_str(), selected_);
    if (!enabled()) ImGui::EndDisabled();
    if (clicked && enabled()) activate();
}

UiTree::~UiTree() noexcept { setRoot(nullptr); }
UiTree::UiTree(UiTree&& other) noexcept
    : root_(std::move(other.root_)), events_(std::move(other.events_)),
      layout_scale_(other.layout_scale_) {
    if (root_) root_->set_tree_recursive(this);
    for (Event& event : events_) {
        if (const std::shared_ptr<Node> target = event.target.lock()) {
            event.attachment_generation = target->attachment_generation_;
        }
    }
}
UiTree& UiTree::operator=(UiTree&& other) noexcept {
    if (this == &other) return *this;
    setRoot(nullptr);
    events_.clear();
    root_ = std::move(other.root_);
    events_ = std::move(other.events_);
    layout_scale_ = other.layout_scale_;
    if (root_) root_->set_tree_recursive(this);
    for (Event& event : events_) {
        if (const std::shared_ptr<Node> target = event.target.lock()) {
            event.attachment_generation = target->attachment_generation_;
        }
    }
    return *this;
}
void UiTree::setLayoutScale(float scale) {
    if (drawing_) throw std::logic_error("rgui cannot change layout scale while drawing");
    if (!std::isfinite(scale) || scale <= 0.0F) {
        throw std::invalid_argument("rgui layout scale must be finite and greater than zero");
    }
    layout_scale_ = scale;
}
void UiTree::setRoot(NodePtr root) {
    if (drawing_) throw std::logic_error("rgui cannot change structure while drawing");
    if (root && root->parent()) throw std::logic_error("rgui root already has a parent");
    if (root && root->tree_ && root->tree_ != this) throw std::logic_error("rgui root already belongs to a tree");
    if (root_ == root) return;
    if (root_) root_->set_tree_recursive(nullptr);
    root_ = std::move(root);
    if (root_) root_->set_tree_recursive(this);
}
void UiTree::draw() {
    if (!root_ || !root_->visible()) return;
    ImGuiContext* const context = ImGui::GetCurrentContext();
    if (!context) throw std::logic_error("rgui drawing requires a current Dear ImGui context");
    struct DrawingGuard final {
        explicit DrawingGuard(bool& drawing) : drawing_(drawing) { drawing_ = true; }
        ~DrawingGuard() { drawing_ = false; }
        bool& drawing_;
    } guard(drawing_);
    const std::string id = std::to_string(root_->id());
    ImGui::PushID(id.c_str()); root_->draw(); ImGui::PopID();
}
void UiTree::enqueue_event(const std::weak_ptr<Node>& target, std::uint64_t attachment_generation, std::function<void(Node&)> callback) { events_.push_back({target, attachment_generation, std::move(callback)}); }
std::size_t UiTree::flushEvents() {
    std::vector<Event> events = std::move(events_); events_.clear(); std::size_t invoked = 0;
    for (Event& event : events) {
        const std::shared_ptr<Node> target = event.target.lock();
        if (!target || target->tree_ != this || target->attachment_generation_ != event.attachment_generation) continue;
        event.callback(*target); ++invoked;
    }
    return invoked;
}

} // namespace rgui
