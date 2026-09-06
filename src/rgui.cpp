#include <rgui/rgui.hpp>

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <string>
#include <utility>

namespace rgui {

std::string_view version() noexcept { return "0.1.0"; }

namespace { std::atomic<NodeId> next_node_id{1}; }

Node::Node() : id_(next_node_id.fetch_add(1, std::memory_order_relaxed)) {}
void Node::set_visible(bool value) noexcept { visible_ = value; }
void Node::set_enabled(bool value) noexcept { enabled_ = value; }
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

void Stack::set_axis(Axis value) noexcept { axis_ = value; }
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
void Window::set_title(std::string_view value) { title_ = value; }
void Window::draw() {
    const std::string title = title_ + "###rgui-" + std::to_string(id());
    const bool draw_contents = ImGui::Begin(title.c_str());
    if (draw_contents) draw_children();
    ImGui::End();
}

Table::Table(std::size_t columns) : headers_(columns) {
    if (columns == 0) throw std::invalid_argument("rgui table requires at least one column");
}
void Table::set_header(std::size_t column, std::string_view value) {
    if (column >= columns()) throw std::out_of_range("rgui table column is out of range");
    headers_[column] = value;
}
std::string_view Table::header(std::size_t column) const {
    if (column >= columns()) throw std::out_of_range("rgui table column is out of range");
    return headers_[column];
}
void Table::draw() {
    const std::string table_id = "##rgui-table-" + std::to_string(id());
    if (!ImGui::BeginTable(table_id.c_str(), static_cast<int>(columns()),
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_SizingStretchSame)) {
        return;
    }
    bool has_headers = false;
    for (const std::string& value : headers_) {
        ImGui::TableSetupColumn(value.empty() ? nullptr : value.c_str());
        has_headers = has_headers || !value.empty();
    }
    if (has_headers) ImGui::TableHeadersRow();
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        ImGui::TableNextColumn();
        draw_child(*child);
    }
    ImGui::EndTable();
}

ScrollArea::ScrollArea(Size size) : size_(size) {
    if (size.width < 0.0F || size.height < 0.0F) {
        throw std::invalid_argument("rgui scroll area size cannot be negative");
    }
}
void ScrollArea::set_size(Size size) {
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
    const float center_x = size.width * 0.5F;
    const float center_y = size.height * 0.5F;
    switch (point) {
    case AnchorPoint::top_left: return {0.0F, 0.0F};
    case AnchorPoint::top: return {center_x, 0.0F};
    case AnchorPoint::top_right: return {size.width, 0.0F};
    case AnchorPoint::left: return {0.0F, center_y};
    case AnchorPoint::center: return {center_x, center_y};
    case AnchorPoint::right: return {size.width, center_y};
    case AnchorPoint::bottom_left: return {0.0F, size.height};
    case AnchorPoint::bottom: return {center_x, size.height};
    case AnchorPoint::bottom_right: return {size.width, size.height};
    }
    return {};
}

float horizontal_fraction(AnchorPoint point) {
    return point_in_rect(point, {1.0F, 1.0F}).x;
}

float vertical_fraction(AnchorPoint point) {
    return point_in_rect(point, {1.0F, 1.0F}).y;
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

AnchoredPanel::AnchoredPanel(Size size, PanelExtent width_extent, PanelExtent height_extent)
    : size_(size), width_extent_(width_extent), height_extent_(height_extent) {
    validate_size(size);
}
void AnchoredPanel::set_size(Size size) { validate_size(size); size_ = size; }
void AnchoredPanel::set_width_extent(PanelExtent value) noexcept { width_extent_ = value; }
void AnchoredPanel::set_height_extent(PanelExtent value) noexcept { height_extent_ = value; }
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
    Container::append(std::move(child));
    anchors_.push_back({anchor, std::nullopt});
}
void AnchoredPanel::append(NodePtr child, Anchor primary_anchor, Anchor secondary_anchor) {
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
void AnchoredPanel::set_anchor(Node& child, Anchor anchor) { anchors_[child_index(child)].primary = anchor; }
Anchor AnchoredPanel::anchor(const Node& child) const { return anchors_[child_index(child)].primary; }
void AnchoredPanel::set_second_anchor(Node& child, std::optional<Anchor> anchor) {
    anchors_[child_index(child)].secondary = anchor;
}
const std::optional<Anchor>& AnchoredPanel::second_anchor(const Node& child) const {
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
            proposal.width = proposed_axis(primary_target.x + child_anchor.offset_x,
                                           secondary_target.x + secondary.offset_x,
                                           horizontal_fraction(child_anchor.self),
                                           horizontal_fraction(secondary.self));
            proposal.height = proposed_axis(primary_target.y + child_anchor.offset_y,
                                            secondary_target.y + secondary.offset_y,
                                            vertical_fraction(child_anchor.self),
                                            vertical_fraction(secondary.self));
        }
        const Size child_size = child.measure(proposal);
        const ImVec2 target = point_in_rect(child_anchor.target, resolved_size);
        const ImVec2 self = point_in_rect(child_anchor.self, child_size);
        ImGui::SetCursorScreenPos({origin.x + target.x + child_anchor.offset_x - self.x,
                                   origin.y + target.y + child_anchor.offset_y - self.y});
        draw_child(child, child_size);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({resolved_size.width, resolved_size.height});
}

Text::Text(std::string value) : value_(std::move(value)) {}
void Text::set_value(std::string_view value) { value_ = value; }
Size Text::measure() const {
    const ImVec2 size = ImGui::CalcTextSize(value_.data(), value_.data() + value_.size());
    return {size.x, size.y};
}
void Text::draw() { ImGui::TextUnformatted(value_.data(), value_.data() + value_.size()); }
Button::Button(std::string label) : label_(std::move(label)) {}
void Button::set_label(std::string_view value) { label_ = value; }
void Button::set_on_click(std::function<void(Button&)> callback) { on_click_ = std::move(callback); }
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

UiTree::~UiTree() noexcept { set_root(nullptr); }
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
    set_root(nullptr);
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
void UiTree::set_root(NodePtr root) {
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
std::size_t UiTree::flush_events() {
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
