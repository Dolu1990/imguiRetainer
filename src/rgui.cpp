#include <rgui/rgui.hpp>

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace rgui {

std::string_view version() noexcept { return "0.1.0"; }

namespace { std::atomic<NodeId> next_node_id{1}; }

Node::Node() : id_(next_node_id.fetch_add(1, std::memory_order_relaxed)) {}
void Node::setVisible(bool value) noexcept { visible_ = value; }
void Node::setEnabled(bool value) noexcept { enabled_ = value; }
Size Node::measure() const { return {}; }
Size Node::measure(const SizeProposal&) const { return measure(); }
void Node::draw(Size) { draw(); }
void Node::set_tree_recursive(UiTree* tree) noexcept { if (tree_ != tree) { tree_ = tree; ++attachment_generation_; } }

Container::~Container() { for (const NodePtr& child : children_) child->parent_ = nullptr; }
void Container::set_tree_recursive(UiTree* tree) noexcept {
    Node::set_tree_recursive(tree);
    for (const NodePtr& child : children_) child->set_tree_recursive(tree);
}
void Container::append(NodePtr child) {
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
    const auto position = std::find_if(children_.begin(), children_.end(), [&child](const NodePtr& candidate) { return candidate.get() == &child; });
    if (position == children_.end()) throw std::logic_error("rgui node is not a child of this container");
    NodePtr result = std::move(*position);
    children_.erase(position);
    result->parent_ = nullptr;
    result->set_tree_recursive(nullptr);
    return result;
}
void Container::clear() {
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
void Window::draw() {
    const std::string title = title_ + "###rgui-" + std::to_string(id());
    const bool draw_contents = ImGui::Begin(title.c_str());
    if (draw_contents) draw_children();
    ImGui::End();
}

Table::Table(std::size_t columns) : columns_(columns) {
    if (columns == 0) throw std::invalid_argument("rgui table requires at least one column");
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
void Table::setVerticalBorders(bool value) noexcept { vertical_borders_ = value; }

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
    ImGuiTableFlags flags = ImGuiTableFlags_BordersH | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchSame;
    if (vertical_borders_) flags |= ImGuiTableFlags_BordersV;
    const std::string table_id = "##rgui-table-" + std::to_string(id());
    if (!ImGui::BeginTable(table_id.c_str(), static_cast<int>(columns()), flags)) {
        return;
    }
    bool has_headers = false;
    for (const Column& configured : columns_) {
        const ImGuiTableColumnFlags column_flags = configured.sizing == ColumnSizing::stretch
                                                       ? ImGuiTableColumnFlags_WidthStretch
                                                       : ImGuiTableColumnFlags_WidthFixed;
        ImGui::TableSetupColumn(configured.header.empty() ? nullptr : configured.header.c_str(),
                                column_flags, configured.width_or_weight);
        has_headers = has_headers || !configured.header.empty();
    }
    if (has_headers) ImGui::TableHeadersRow();
    std::vector<NodePtr> visible_children;
    visible_children.reserve(children_.size());
    for (const NodePtr& child : children_) {
        if (child->visible()) visible_children.push_back(child);
    }
    for (std::size_t row_start = 0; row_start < visible_children.size(); row_start += columns()) {
        const std::size_t row_end = std::min(row_start + columns(), visible_children.size());
        float row_height = 0.0F;
        for (std::size_t index = row_start; index < row_end; ++index) {
            row_height = std::max(row_height, visible_children[index]->measure().height);
        }
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
        for (std::size_t index = row_start; index < row_end; ++index) {
            const std::size_t column_index = index - row_start;
            ImGui::TableSetColumnIndex(static_cast<int>(column_index));
            const Column& configured = columns_[column_index];
            const Size content_size = visible_children[index]->measure();
            const ImVec2 cursor = ImGui::GetCursorPos();
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float offset_x = justification_offset(configured.horizontal_justification,
                                                         available.x, content_size.width);
            const float offset_y = justification_offset(configured.vertical_justification,
                                                         row_height, content_size.height);
            if (offset_x != 0.0F || offset_y != 0.0F) {
                ImGui::SetCursorPos({cursor.x + offset_x, cursor.y + offset_y});
            }
            draw_child(*visible_children[index]);
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
void ScrollArea::draw() {
    const std::string area_id = "##rgui-scroll-area-" + std::to_string(id());
    const bool draw_contents = ImGui::BeginChild(area_id.c_str(), {size_.width, size_.height},
                                                 ImGuiChildFlags_Borders);
    if (draw_contents) draw_children();
    ImGui::EndChild();
}

namespace {
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
} // namespace

AnchoredPanel::AnchoredPanel(Size size, PanelExtent widthExtent, PanelExtent heightExtent)
    : size_(size), width_extent_(widthExtent), height_extent_(heightExtent) {
    validate_size(size);
}
void AnchoredPanel::setSize(Size size) { validate_size(size); size_ = size; }
void AnchoredPanel::setWidthExtent(PanelExtent value) noexcept { width_extent_ = value; }
void AnchoredPanel::setHeightExtent(PanelExtent value) noexcept { height_extent_ = value; }
Size AnchoredPanel::measure() const {
    Size result = size_;
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
            proposal.width = proposed_axis(primary_target.x + child_anchor.offsetX,
                                           secondary_target.x + secondary.offsetX,
                                           child_anchor.self.x, secondary.self.x);
            proposal.height = proposed_axis(primary_target.y + child_anchor.offsetY,
                                            secondary_target.y + secondary.offsetY,
                                            child_anchor.self.y, secondary.self.y);
        }
        const Size child_size = child.measure(proposal);
        const ImVec2 target = point_in_rect(child_anchor.target, resolved_size);
        const ImVec2 self = point_in_rect(child_anchor.self, child_size);
        ImGui::SetCursorScreenPos({origin.x + target.x + child_anchor.offsetX - self.x,
                                   origin.y + target.y + child_anchor.offsetY - self.y});
        draw_child(child, child_size);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({resolved_size.width, resolved_size.height});
}

Text::Text(std::string value) : value_(std::move(value)) {}
void Text::setValue(std::string_view value) { value_ = value; }
Size Text::measure() const {
    const ImVec2 size = ImGui::CalcTextSize(value_.data(), value_.data() + value_.size());
    return {size.x, size.y};
}
void Text::draw() { ImGui::TextUnformatted(value_.data(), value_.data() + value_.size()); }
Button::Button(std::string label) : label_(std::move(label)) {}
void Button::setLabel(std::string_view value) { label_ = value; }
void Button::setOnClick(std::function<void(Button&)> callback) { on_click_ = std::move(callback); }
void Button::activate() { if (visible() && enabled() && on_click_ && tree_) tree_->enqueue_event(weak_from_this(), attachment_generation_, on_click_); }
Size Button::measure() const {
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
    if (!enabled()) ImGui::BeginDisabled();
    const bool clicked = ImGui::Button(label_.c_str(), {resolved_size.width, resolved_size.height});
    if (!enabled()) ImGui::EndDisabled();
    if (clicked && enabled()) activate();
}

UiTree::~UiTree() noexcept { setRoot(nullptr); }
UiTree::UiTree(UiTree&& other) noexcept
    : root_(std::move(other.root_)), events_(std::move(other.events_)) {
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
    if (root_) root_->set_tree_recursive(this);
    for (Event& event : events_) {
        if (const std::shared_ptr<Node> target = event.target.lock()) {
            event.attachment_generation = target->attachment_generation_;
        }
    }
    return *this;
}
void UiTree::setRoot(NodePtr root) {
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
    const std::string id = std::to_string(root_->id());
    ImGui::PushID(id.c_str()); root_->draw(); ImGui::PopID();
}
void UiTree::enqueue_event(const std::weak_ptr<Node>& target, std::uint64_t attachment_generation, std::function<void(Button&)> callback) { events_.push_back({target, attachment_generation, std::move(callback)}); }
std::size_t UiTree::flushEvents() {
    std::vector<Event> events = std::move(events_); events_.clear(); std::size_t invoked = 0;
    for (Event& event : events) {
        const std::shared_ptr<Node> target = event.target.lock();
        if (!target || target->tree_ != this || target->attachment_generation_ != event.attachment_generation) continue;
        const std::shared_ptr<Button> button = std::dynamic_pointer_cast<Button>(target);
        if (!button) continue;
        event.callback(*button); ++invoked;
    }
    return invoked;
}

} // namespace rgui
