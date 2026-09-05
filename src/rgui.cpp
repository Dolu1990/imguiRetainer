#include <rgui/rgui.hpp>

#include <algorithm>
#include <atomic>
#include <stdexcept>

namespace rgui {

std::string_view version() noexcept {
    return "0.1.0";
}

namespace {

std::atomic<NodeId> next_node_id{1};

float clamp(float value, float minimum, float maximum) noexcept {
    return std::clamp(value, minimum, maximum);
}

Size clamp_size(Size value, const LayoutParams& params) noexcept {
    return {clamp(value.width, params.minimum.width, params.maximum.width),
            clamp(value.height, params.minimum.height, params.maximum.height)};
}

float primary(Size size, Axis axis) noexcept {
    return axis == Axis::horizontal ? size.width : size.height;
}

float cross(Size size, Axis axis) noexcept {
    return axis == Axis::horizontal ? size.height : size.width;
}

} // namespace

Node::Node() : id_(next_node_id.fetch_add(1, std::memory_order_relaxed)) {}

void Node::set_visible(bool value) noexcept {
    if (visible_ != value) { visible_ = value; invalidate(Dirty::layout | Dirty::paint); }
}

void Node::set_enabled(bool value) noexcept {
    if (enabled_ != value) { enabled_ = value; invalidate(Dirty::paint); }
}

void Node::set_layout_params(LayoutParams value) noexcept {
    layout_ = value;
    invalidate(Dirty::layout);
}

void Node::invalidate(Dirty flags) noexcept {
    dirty_ = dirty_ | flags;
    if (parent_ != nullptr) parent_->invalidate(flags);
}

void Node::clear_dirty_recursive() noexcept { dirty_ = Dirty::none; }

Size Node::measure(Size) const noexcept { return clamp_size(layout_.preferred, layout_); }

void Node::arrange(Rect bounds) noexcept { bounds_ = bounds; }

Container::~Container() {
    for (const NodePtr& child : children_) child->parent_ = nullptr;
}

void Container::append(NodePtr child) {
    if (!child) throw std::invalid_argument("rgui cannot append a null node");
    if (child->parent_ != nullptr) throw std::logic_error("rgui node already has a parent");
    for (Node* ancestor = this; ancestor != nullptr; ancestor = ancestor->parent_) {
        if (ancestor == child.get()) throw std::logic_error("rgui cannot introduce a tree cycle");
    }
    child->parent_ = this;
    children_.push_back(std::move(child));
    invalidate(Dirty::structure | Dirty::layout);
}

NodePtr Container::remove(Node& child) {
    const auto position = std::find_if(children_.begin(), children_.end(),
        [&child](const NodePtr& candidate) { return candidate.get() == &child; });
    if (position == children_.end()) throw std::logic_error("rgui node is not a child of this container");
    NodePtr result = std::move(*position);
    children_.erase(position);
    result->parent_ = nullptr;
    invalidate(Dirty::structure | Dirty::layout);
    return result;
}

void Container::clear() {
    for (const NodePtr& child : children_) child->parent_ = nullptr;
    children_.clear();
    invalidate(Dirty::structure | Dirty::layout);
}

void Container::clear_dirty_recursive() noexcept {
    Node::clear_dirty_recursive();
    for (const NodePtr& child : children_) child->clear_dirty_recursive();
}

void Stack::set_axis(Axis value) noexcept { if (axis_ != value) { axis_ = value; invalidate(Dirty::layout); } }
void Stack::set_gap(float value) noexcept { if (gap_ != value) { gap_ = std::max(0.0F, value); invalidate(Dirty::layout); } }
void Stack::set_align(Align value) noexcept { if (align_ != value) { align_ = value; invalidate(Dirty::layout); } }

Size Stack::measure(Size available) const noexcept {
    float sum = 0.0F;
    float maximum = 0.0F;
    std::size_t visible_children = 0;
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        const Size desired = child->measure(available);
        const Insets& margin = child->layout_params().margin;
        const float main_margin = axis_ == Axis::horizontal ? margin.left + margin.right : margin.top + margin.bottom;
        const float cross_margin = axis_ == Axis::horizontal ? margin.top + margin.bottom : margin.left + margin.right;
        sum += primary(desired, axis_) + main_margin;
        maximum = std::max(maximum, cross(desired, axis_) + cross_margin);
        ++visible_children;
    }
    if (visible_children > 1) sum += gap_ * static_cast<float>(visible_children - 1);
    Size result = axis_ == Axis::horizontal ? Size{sum, maximum} : Size{maximum, sum};
    return clamp_size(result, layout_params());
}

void Stack::arrange(Rect bounds) noexcept {
    Node::arrange(bounds);
    float total_grow = 0.0F;
    float fixed = 0.0F;
    std::size_t visible_children = 0;
    const Size available{bounds.width, bounds.height};
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        const Insets& m = child->layout_params().margin;
        fixed += primary(child->measure(available), axis_) + (axis_ == Axis::horizontal ? m.left + m.right : m.top + m.bottom);
        total_grow += std::max(0.0F, child->layout_params().grow);
        ++visible_children;
    }
    if (visible_children > 1) fixed += gap_ * static_cast<float>(visible_children - 1);
    float cursor = axis_ == Axis::horizontal ? bounds.x : bounds.y;
    const float extra = std::max(0.0F, primary(available, axis_) - fixed);
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        const LayoutParams& p = child->layout_params();
        const Insets& m = p.margin;
        Size desired = child->measure(available);
        float main = primary(desired, axis_) + (total_grow > 0.0F ? extra * std::max(0.0F, p.grow) / total_grow : 0.0F);
        const float available_cross = cross(available, axis_);
        const float before_main = axis_ == Axis::horizontal ? m.left : m.top;
        const float after_main = axis_ == Axis::horizontal ? m.right : m.bottom;
        const float before_cross = axis_ == Axis::horizontal ? m.top : m.left;
        const float after_cross = axis_ == Axis::horizontal ? m.bottom : m.right;
        const float natural_cross = cross(desired, axis_);
        float child_cross = align_ == Align::stretch ? std::max(0.0F, available_cross - before_cross - after_cross) : natural_cross;
        child_cross = std::min(child_cross, std::max(0.0F, available_cross - before_cross - after_cross));
        float cross_position = (axis_ == Axis::horizontal ? bounds.y : bounds.x) + before_cross;
        const float slack = std::max(0.0F, available_cross - before_cross - after_cross - child_cross);
        if (align_ == Align::center) cross_position += slack / 2.0F;
        if (align_ == Align::end) cross_position += slack;
        cursor += before_main;
        child->arrange(axis_ == Axis::horizontal
            ? Rect{cursor, cross_position, std::max(0.0F, main), child_cross}
            : Rect{cross_position, cursor, child_cross, std::max(0.0F, main)});
        cursor += main + after_main + gap_;
    }
}

Size Overlay::measure(Size available) const noexcept {
    Size result{};
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        const Size desired = child->measure(available);
        result.width = std::max(result.width, desired.width);
        result.height = std::max(result.height, desired.height);
    }
    return clamp_size(result, layout_params());
}

void Overlay::arrange(Rect bounds) noexcept {
    Node::arrange(bounds);
    for (const NodePtr& child : children_) if (child->visible()) {
        const Size desired = child->measure({bounds.width, bounds.height});
        child->arrange({bounds.x, bounds.y, desired.width, desired.height});
    }
}

Window::Window(std::string title) : title_(std::move(title)) {}
void Window::set_title(std::string_view value) { title_ = value; invalidate(Dirty::paint); }
Text::Text(std::string value) : value_(std::move(value)) {}
void Text::set_value(std::string_view value) { value_ = value; invalidate(Dirty::paint | Dirty::layout); }
Button::Button(std::string label) : label_(std::move(label)) {}
void Button::set_label(std::string_view value) { label_ = value; invalidate(Dirty::paint | Dirty::layout); }
void Button::set_on_click(std::function<void(Button&)> callback) { on_click_ = std::move(callback); }
void Button::activate() { if (visible() && enabled() && on_click_) on_click_(*this); }

void UiTree::set_root(NodePtr root) {
    if (root && root->parent() != nullptr) throw std::logic_error("rgui root already has a parent");
    root_ = std::move(root);
}

void UiTree::layout(Size available) noexcept {
    if (!root_) return;
    root_->arrange({0.0F, 0.0F, std::max(0.0F, available.width), std::max(0.0F, available.height)});
    root_->clear_dirty_recursive();
}

} // namespace rgui
