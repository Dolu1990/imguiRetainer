#include <rgui/rgui.hpp>

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <string>

namespace rgui {

std::string_view version() noexcept { return "0.1.0"; }

namespace { std::atomic<NodeId> next_node_id{1}; }

Node::Node() : id_(next_node_id.fetch_add(1, std::memory_order_relaxed)) {}
void Node::set_visible(bool value) noexcept { visible_ = value; }
void Node::set_enabled(bool value) noexcept { enabled_ = value; }
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
Text::Text(std::string value) : value_(std::move(value)) {}
void Text::set_value(std::string_view value) { value_ = value; }
void Text::draw() { ImGui::TextUnformatted(value_.data(), value_.data() + value_.size()); }
Button::Button(std::string label) : label_(std::move(label)) {}
void Button::set_label(std::string_view value) { label_ = value; }
void Button::set_on_click(std::function<void(Button&)> callback) { on_click_ = std::move(callback); }
void Button::activate() { if (visible() && enabled() && on_click_ && tree_) tree_->enqueue_event(weak_from_this(), attachment_generation_, on_click_); }
void Button::draw() {
    if (!enabled()) ImGui::BeginDisabled();
    const bool clicked = ImGui::Button(label_.c_str());
    if (!enabled()) ImGui::EndDisabled();
    if (clicked && enabled()) activate();
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
