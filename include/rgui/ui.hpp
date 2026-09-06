#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ImGuiContext;

namespace rgui {

using NodeId = std::uint64_t;

enum class Dirty : unsigned char { none = 0, paint = 1, structure = 2 };
constexpr Dirty operator|(Dirty left, Dirty right) noexcept {
    return static_cast<Dirty>(static_cast<unsigned char>(left) | static_cast<unsigned char>(right));
}
constexpr bool contains(Dirty value, Dirty flag) noexcept {
    return (static_cast<unsigned char>(value) & static_cast<unsigned char>(flag)) != 0;
}

class Container;
class Button;
class Node;
class UiTree;
using NodePtr = std::shared_ptr<Node>;

/// A retained Dear ImGui node. Implementations may use the current ImGui
/// context directly; `context` makes that dependency explicit without
/// requiring imgui.h in this public header.
class Node : public std::enable_shared_from_this<Node> {
public:
    Node();
    virtual ~Node() = default;
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    [[nodiscard]] NodeId id() const noexcept { return id_; }
    [[nodiscard]] Node* parent() const noexcept { return parent_; }
    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] Dirty dirty() const noexcept { return dirty_; }

    void set_visible(bool value) noexcept;
    void set_enabled(bool value) noexcept;
    void invalidate(Dirty flags = Dirty::paint) noexcept;
    virtual void clear_dirty_recursive() noexcept;
    virtual void draw(ImGuiContext& context) = 0;

private:
    friend class Container;
    friend class Button;
    friend class UiTree;
    virtual void set_tree_recursive(UiTree* tree) noexcept;
    NodeId id_;
    Node* parent_ = nullptr;
    UiTree* tree_ = nullptr;
    std::uint64_t attachment_generation_ = 0;
    bool visible_ = true;
    bool enabled_ = true;
    Dirty dirty_ = Dirty::structure | Dirty::paint;
};

class Container : public Node {
public:
    ~Container() override;
    void append(NodePtr child);
    [[nodiscard]] NodePtr remove(Node& child);
    void clear();
    [[nodiscard]] const std::vector<NodePtr>& children() const noexcept { return children_; }
    void clear_dirty_recursive() noexcept override;
    void draw(ImGuiContext& context) override;

protected:
    void draw_children(ImGuiContext& context);
    void set_tree_recursive(UiTree* tree) noexcept override;
    std::vector<NodePtr> children_;
};

enum class Axis { horizontal, vertical };

/// Draws children in Dear ImGui's natural flow. Horizontal stacks separate
/// items with SameLine(); vertical stacks leave ImGui's default spacing.
class Stack : public Container {
public:
    explicit Stack(Axis axis = Axis::vertical) noexcept : axis_(axis) {}
    void set_axis(Axis value) noexcept;
    [[nodiscard]] Axis axis() const noexcept { return axis_; }
    void draw(ImGuiContext& context) override;

private:
    Axis axis_;
};

/// A generic grouping container. Use a custom node for an ImGui overlay.
class Overlay final : public Container {};

class Window final : public Stack {
public:
    explicit Window(std::string title = {});
    [[nodiscard]] std::string_view title() const noexcept { return title_; }
    void set_title(std::string_view value);
    void draw(ImGuiContext& context) override;
private:
    std::string title_;
};

class Text final : public Node {
public:
    explicit Text(std::string value = {});
    [[nodiscard]] std::string_view value() const noexcept { return value_; }
    void set_value(std::string_view value);
    void draw(ImGuiContext& context) override;
private:
    std::string value_;
};

class Button final : public Node {
public:
    explicit Button(std::string label = {});
    [[nodiscard]] std::string_view label() const noexcept { return label_; }
    void set_label(std::string_view value);
    void set_on_click(std::function<void(Button&)> callback);
    void activate();
    void draw(ImGuiContext& context) override;
private:
    std::string label_;
    std::function<void(Button&)> on_click_;
};

/// Owns one root and draws it into the caller-owned current Dear ImGui frame.
class UiTree final {
public:
    void set_root(NodePtr root);
    [[nodiscard]] const NodePtr& root() const noexcept { return root_; }
    void draw();
    [[nodiscard]] std::size_t flush_events();
    [[nodiscard]] std::size_t pending_event_count() const noexcept { return events_.size(); }
private:
    friend class Button;
    struct Event {
        std::weak_ptr<Node> target;
        std::uint64_t attachment_generation = 0;
        std::function<void(Button&)> callback;
    };

    void enqueue_event(const std::weak_ptr<Node>& target, std::uint64_t attachment_generation,
                       std::function<void(Button&)> callback);
    NodePtr root_;
    std::vector<Event> events_;
};

} // namespace rgui
