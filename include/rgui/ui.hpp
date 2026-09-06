#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rgui {

using NodeId = std::uint64_t;

/// A size in Dear ImGui pixels.
struct Size {
    float width = 0.0F;
    float height = 0.0F;
};

class Container;
class Button;
class Node;
class UiTree;
using NodePtr = std::shared_ptr<Node>;

/// A retained Dear ImGui node. Implementations use the current ImGui context
/// directly and may include imgui.h in their implementation.
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
    void set_visible(bool value) noexcept;
    void set_enabled(bool value) noexcept;
    /// Returns this node's preferred size in the current Dear ImGui context.
    /// Custom nodes that participate in an AnchoredPanel should override this.
    [[nodiscard]] virtual Size measure() const;
    virtual void draw() = 0;

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
};

class Container : public Node {
public:
    ~Container() override;
    virtual void append(NodePtr child);
    [[nodiscard]] virtual NodePtr remove(Node& child);
    virtual void clear();
    [[nodiscard]] const std::vector<NodePtr>& children() const noexcept { return children_; }
    void draw() override;

protected:
    void draw_child(Node& child);
    void draw_children();
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
    void draw() override;

private:
    Axis axis_;
};

class Window final : public Container {
public:
    explicit Window(std::string title = {});
    [[nodiscard]] std::string_view title() const noexcept { return title_; }
    void set_title(std::string_view value);
    void draw() override;
private:
    std::string title_;
};

enum class AnchorPoint {
    top_left, top, top_right,
    left, center, right,
    bottom_left, bottom, bottom_right,
};

/// Positions a child relative to an AnchoredPanel. Both points refer to their
/// respective rectangles; the child is placed so these points coincide before
/// the pixel offset is applied.
struct Anchor {
    AnchorPoint self = AnchorPoint::top_left;
    AnchorPoint target = AnchorPoint::top_left;
    float offset_x = 0.0F;
    float offset_y = 0.0F;
};

/// A fixed-size retained layout surface. It owns each child's placement while
/// the child remains responsible for measuring and drawing itself.
class AnchoredPanel final : public Container {
public:
    explicit AnchoredPanel(Size size);
    [[nodiscard]] Size size() const noexcept { return size_; }
    void set_size(Size size);
    [[nodiscard]] Size measure() const override { return size_; }

    void append(NodePtr child) override;
    void append(NodePtr child, Anchor anchor);
    [[nodiscard]] NodePtr remove(Node& child) override;
    void clear() override;
    void set_anchor(Node& child, Anchor anchor);
    [[nodiscard]] Anchor anchor(const Node& child) const;
    void draw() override;

private:
    [[nodiscard]] std::size_t child_index(const Node& child) const;
    Size size_;
    std::vector<Anchor> anchors_;
};

class Text final : public Node {
public:
    explicit Text(std::string value = {});
    [[nodiscard]] std::string_view value() const noexcept { return value_; }
    void set_value(std::string_view value);
    [[nodiscard]] Size measure() const override;
    void draw() override;
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
    [[nodiscard]] Size measure() const override;
    void draw() override;
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
