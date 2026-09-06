#include <rgui/rgui.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
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

float non_negative_finite(float value) noexcept {
    return std::isfinite(value) && value > 0.0F ? value : 0.0F;
}

LayoutParams normalize_layout_params(LayoutParams value) noexcept {
    value.preferred = {non_negative_finite(value.preferred.width),
                       non_negative_finite(value.preferred.height)};
    value.minimum = {non_negative_finite(value.minimum.width),
                     non_negative_finite(value.minimum.height)};
    value.maximum = {
        std::max(value.minimum.width, non_negative_finite(value.maximum.width)),
        std::max(value.minimum.height, non_negative_finite(value.maximum.height)),
    };
    value.margin = {non_negative_finite(value.margin.left), non_negative_finite(value.margin.top),
                    non_negative_finite(value.margin.right), non_negative_finite(value.margin.bottom)};
    value.grow = non_negative_finite(value.grow);
    return value;
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
    layout_ = normalize_layout_params(value);
    automatic_preferred_width_ = layout_.preferred.width == 0.0F;
    automatic_preferred_height_ = layout_.preferred.height == 0.0F;
    invalidate(Dirty::layout);
}

void Node::set_automatic_preferred_size(Size value) noexcept {
    bool changed = false;
    if (automatic_preferred_width_ && layout_.preferred.width != value.width) {
        layout_.preferred.width = value.width;
        changed = true;
    }
    if (automatic_preferred_height_ && layout_.preferred.height != value.height) {
        layout_.preferred.height = value.height;
        changed = true;
    }
    if (changed) invalidate(Dirty::layout);
}

void Node::invalidate(Dirty flags) noexcept {
    dirty_ = dirty_ | flags;
    if (parent_ != nullptr) parent_->invalidate(flags);
}

void Node::set_tree_recursive(UiTree* tree) noexcept {
    if (tree_ == tree) return;
    tree_ = tree;
    ++attachment_generation_;
}

void Node::clear_dirty_recursive() noexcept { dirty_ = Dirty::none; }

void Node::apply_default_layout(LayoutContext&) {}

Size Node::measure(Size) const noexcept { return clamp_size(layout_.preferred, layout_); }

void Node::arrange(Rect bounds) noexcept { bounds_ = bounds; }

Container::~Container() {
    for (const NodePtr& child : children_) child->parent_ = nullptr;
}

void Container::set_tree_recursive(UiTree* tree) noexcept {
    Node::set_tree_recursive(tree);
    for (const NodePtr& child : children_) child->set_tree_recursive(tree);
}

void Container::append(NodePtr child) {
    if (!child) throw std::invalid_argument("rgui cannot append a null node");
    if (child->parent_ != nullptr) throw std::logic_error("rgui node already has a parent");
    for (Node* ancestor = this; ancestor != nullptr; ancestor = ancestor->parent_) {
        if (ancestor == child.get()) throw std::logic_error("rgui cannot introduce a tree cycle");
    }
    child->parent_ = this;
    child->set_tree_recursive(tree_);
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
    result->set_tree_recursive(nullptr);
    invalidate(Dirty::structure | Dirty::layout);
    return result;
}

void Container::clear() {
    for (const NodePtr& child : children_) {
        child->parent_ = nullptr;
        child->set_tree_recursive(nullptr);
    }
    children_.clear();
    invalidate(Dirty::structure | Dirty::layout);
}

void Container::clear_dirty_recursive() noexcept {
    Node::clear_dirty_recursive();
    for (const NodePtr& child : children_) child->clear_dirty_recursive();
}

void Container::apply_default_layout(LayoutContext& context) {
    for (const NodePtr& child : children_) child->apply_default_layout(context);
}

void Container::render(RenderContext& context) {
    for (const NodePtr& child : children_) context.render_child(*child);
}

void Stack::set_axis(Axis value) noexcept { if (axis_ != value) { axis_ = value; invalidate(Dirty::layout); } }
void Stack::set_gap(float value) noexcept {
    value = non_negative_finite(value);
    if (gap_ != value) { gap_ = value; invalidate(Dirty::layout); }
}
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
    float fixed = 0.0F;
    std::size_t visible_children = 0;
    const Size available{bounds.width, bounds.height};
    for (const NodePtr& child : children_) {
        if (!child->visible()) continue;
        const Insets& m = child->layout_params().margin;
        fixed += primary(child->measure(available), axis_) + (axis_ == Axis::horizontal ? m.left + m.right : m.top + m.bottom);
        ++visible_children;
    }
    if (visible_children > 1) fixed += gap_ * static_cast<float>(visible_children - 1);
    const float extra = std::max(0.0F, primary(available, axis_) - fixed);
    std::vector<float> main_sizes(children_.size(), 0.0F);
    std::vector<bool> can_grow(children_.size(), false);
    for (std::size_t index = 0; index < children_.size(); ++index) {
        const NodePtr& child = children_[index];
        if (!child->visible()) continue;
        main_sizes[index] = primary(child->measure(available), axis_);
        can_grow[index] = child->layout_params().grow > 0.0F
            && primary(child->layout_params().maximum, axis_) > main_sizes[index];
    }
    float remaining = extra;
    while (remaining > 0.001F) {
        float active_grow = 0.0F;
        for (std::size_t index = 0; index < children_.size(); ++index) {
            if (can_grow[index]) active_grow += children_[index]->layout_params().grow;
        }
        if (active_grow == 0.0F) break;

        float distributed = 0.0F;
        for (std::size_t index = 0; index < children_.size(); ++index) {
            if (!can_grow[index]) continue;
            const float maximum = primary(children_[index]->layout_params().maximum, axis_);
            const float share = remaining * children_[index]->layout_params().grow / active_grow;
            const float added = std::min(share, std::max(0.0F, maximum - main_sizes[index]));
            main_sizes[index] += added;
            distributed += added;
            if (main_sizes[index] >= maximum - 0.001F) can_grow[index] = false;
        }
        if (distributed <= 0.001F) break;
        remaining -= distributed;
    }

    float cursor = axis_ == Axis::horizontal ? bounds.x : bounds.y;
    for (std::size_t index = 0; index < children_.size(); ++index) {
        const NodePtr& child = children_[index];
        if (!child->visible()) continue;
        const LayoutParams& p = child->layout_params();
        const Insets& m = p.margin;
        Size desired = child->measure(available);
        const float main = main_sizes[index];
        const float available_cross = cross(available, axis_);
        const float before_main = axis_ == Axis::horizontal ? m.left : m.top;
        const float after_main = axis_ == Axis::horizontal ? m.right : m.bottom;
        const float before_cross = axis_ == Axis::horizontal ? m.top : m.left;
        const float after_cross = axis_ == Axis::horizontal ? m.bottom : m.right;
        const float natural_cross = cross(desired, axis_);
        const float cross_limit = std::max(0.0F, available_cross - before_cross - after_cross);
        float child_cross = align_ == Align::stretch ? cross_limit : natural_cross;
        child_cross = std::min({child_cross, cross_limit, cross(p.maximum, axis_)});
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
        const Insets& margin = child->layout_params().margin;
        result.width = std::max(result.width, desired.width + margin.left + margin.right);
        result.height = std::max(result.height, desired.height + margin.top + margin.bottom);
    }
    return clamp_size(result, layout_params());
}

void Overlay::arrange(Rect bounds) noexcept {
    Node::arrange(bounds);
    for (const NodePtr& child : children_) if (child->visible()) {
        const Insets& margin = child->layout_params().margin;
        const Size available{std::max(0.0F, bounds.width - margin.left - margin.right),
                             std::max(0.0F, bounds.height - margin.top - margin.bottom)};
        const Size desired = child->measure(available);
        child->arrange({bounds.x + margin.left, bounds.y + margin.top,
                        std::min(desired.width, available.width),
                        std::min(desired.height, available.height)});
    }
}

Window::Window(std::string title) : title_(std::move(title)) {}
void Window::set_title(std::string_view value) { title_ = value; invalidate(Dirty::paint); }
void Window::render(RenderContext& context) {
    const bool render_contents = context.begin_window(id(), title_, bounds());
    if (render_contents) Container::render(context);
    context.end_window();
}
Text::Text(std::string value) : value_(std::move(value)) {}
void Text::set_value(std::string_view value) { value_ = value; invalidate(Dirty::paint | Dirty::layout); }
void Text::apply_default_layout(LayoutContext& context) {
    set_automatic_preferred_size(context.measure_text(value_));
}
void Text::render(RenderContext& context) { context.text(value_, bounds()); }
Button::Button(std::string label) : label_(std::move(label)) {}
void Button::set_label(std::string_view value) { label_ = value; invalidate(Dirty::paint | Dirty::layout); }
void Button::set_on_click(std::function<void(Button&)> callback) { on_click_ = std::move(callback); }
void Button::activate() {
    if (!visible() || !enabled() || !on_click_ || tree_ == nullptr) return;
    tree_->enqueue_event(weak_from_this(), attachment_generation_, on_click_);
}
void Button::apply_default_layout(LayoutContext& context) {
    set_automatic_preferred_size(context.measure_button(label_));
}
void Button::render(RenderContext& context) {
    if (context.button(label_, bounds(), enabled())) activate();
}

void UiTree::set_root(NodePtr root) {
    if (root && root->parent() != nullptr) throw std::logic_error("rgui root already has a parent");
    if (root && root->tree_ != nullptr && root->tree_ != this) {
        throw std::logic_error("rgui root already belongs to a tree");
    }
    if (root_ == root) return;
    if (root_) root_->set_tree_recursive(nullptr);
    root_ = std::move(root);
    if (root_) root_->set_tree_recursive(this);
}

void UiTree::apply_default_layout(LayoutContext& context) {
    if (root_) root_->apply_default_layout(context);
}

void UiTree::layout(Size available) noexcept {
    if (!root_) return;
    root_->arrange({0.0F, 0.0F, std::max(0.0F, available.width), std::max(0.0F, available.height)});
    root_->clear_dirty_recursive();
}

void UiTree::enqueue_event(const std::weak_ptr<Node>& target, std::uint64_t attachment_generation,
                           std::function<void(Button&)> callback) {
    events_.push_back({target, attachment_generation, std::move(callback)});
}

std::size_t UiTree::flush_events() {
    std::vector<Event> events = std::move(events_);
    events_.clear();
    std::size_t invoked = 0;
    for (Event& event : events) {
        const std::shared_ptr<Node> target = event.target.lock();
        if (!target || target->tree_ != this || target->attachment_generation_ != event.attachment_generation) {
            continue;
        }
        const std::shared_ptr<Button> button = std::dynamic_pointer_cast<Button>(target);
        if (!button) continue;
        event.callback(*button);
        ++invoked;
    }
    return invoked;
}

} // namespace rgui
