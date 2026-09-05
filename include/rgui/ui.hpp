#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rgui {

using NodeId = std::uint64_t;

struct Size { float width = 0.0F; float height = 0.0F; };
struct Rect { float x = 0.0F; float y = 0.0F; float width = 0.0F; float height = 0.0F; };
struct Insets { float left = 0.0F; float top = 0.0F; float right = 0.0F; float bottom = 0.0F; };

enum class Dirty : unsigned char { none = 0, paint = 1, layout = 2, structure = 4 };
constexpr Dirty operator|(Dirty left, Dirty right) noexcept {
    return static_cast<Dirty>(static_cast<unsigned char>(left) | static_cast<unsigned char>(right));
}
constexpr bool contains(Dirty value, Dirty flag) noexcept {
    return (static_cast<unsigned char>(value) & static_cast<unsigned char>(flag)) != 0;
}

struct LayoutParams {
    Size preferred{};
    Size minimum{};
    Size maximum{1000000.0F, 1000000.0F};
    Insets margin{};
    float grow = 0.0F;
};

class Container;
class Node;
using NodePtr = std::shared_ptr<Node>;

/// A retained UI node. Nodes have one owning parent at most.
class Node {
public:
    Node();
    virtual ~Node() = default;
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    [[nodiscard]] NodeId id() const noexcept { return id_; }
    [[nodiscard]] Node* parent() const noexcept { return parent_; }
    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] const LayoutParams& layout_params() const noexcept { return layout_; }
    [[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }
    [[nodiscard]] Dirty dirty() const noexcept { return dirty_; }

    void set_visible(bool value) noexcept;
    void set_enabled(bool value) noexcept;
    void set_layout_params(LayoutParams value) noexcept;
    void invalidate(Dirty flags = Dirty::paint) noexcept;
    virtual void clear_dirty_recursive() noexcept;

    [[nodiscard]] virtual Size measure(Size available) const noexcept;
    virtual void arrange(Rect bounds) noexcept;

private:
    friend class Container;
    NodeId id_;
    Node* parent_ = nullptr;
    bool visible_ = true;
    bool enabled_ = true;
    LayoutParams layout_{};
    Rect bounds_{};
    Dirty dirty_ = Dirty::structure | Dirty::layout | Dirty::paint;
};

class Container : public Node {
public:
    ~Container() override;
    void append(NodePtr child);
    [[nodiscard]] NodePtr remove(Node& child);
    void clear();
    [[nodiscard]] const std::vector<NodePtr>& children() const noexcept { return children_; }
    void clear_dirty_recursive() noexcept override;

protected:
    std::vector<NodePtr> children_;
};

enum class Axis { horizontal, vertical };
enum class Align { start, center, end, stretch };

/// Arranges children sequentially along an axis.
class Stack : public Container {
public:
    explicit Stack(Axis axis = Axis::vertical) noexcept : axis_(axis) {}
    void set_axis(Axis value) noexcept;
    void set_gap(float value) noexcept;
    void set_align(Align value) noexcept;
    [[nodiscard]] Axis axis() const noexcept { return axis_; }
    [[nodiscard]] float gap() const noexcept { return gap_; }
    [[nodiscard]] Align align() const noexcept { return align_; }
    [[nodiscard]] Size measure(Size available) const noexcept override;
    void arrange(Rect bounds) noexcept override;

private:
    Axis axis_;
    Align align_ = Align::start;
    float gap_ = 0.0F;
};

/// A container that gives each child its own preferred rectangle at the same origin.
class Overlay final : public Container {
public:
    [[nodiscard]] Size measure(Size available) const noexcept override;
    void arrange(Rect bounds) noexcept override;
};

class Window final : public Stack {
public:
    explicit Window(std::string title = {});
    [[nodiscard]] std::string_view title() const noexcept { return title_; }
    void set_title(std::string_view value);
private:
    std::string title_;
};

class Text final : public Node {
public:
    explicit Text(std::string value = {});
    [[nodiscard]] std::string_view value() const noexcept { return value_; }
    void set_value(std::string_view value);
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
private:
    std::string label_;
    std::function<void(Button&)> on_click_;
};

/// Owns one root and runs retained layout in logical pixels.
class UiTree final {
public:
    void set_root(NodePtr root);
    [[nodiscard]] const NodePtr& root() const noexcept { return root_; }
    void layout(Size available) noexcept;
private:
    NodePtr root_;
};

} // namespace rgui
