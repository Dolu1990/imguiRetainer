#pragma once

#include <cstdint>
#include <cstddef>
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
class Button;
class Node;
class RenderContext;
class LayoutContext;
class UiTree;
using NodePtr = std::shared_ptr<Node>;

/// Renderer-neutral operations used by retained nodes during a render pass.
class RenderContext {
public:
    virtual ~RenderContext() = default;
    virtual void render_child(Node& child) = 0;
    /// `id` is the stable identity of this window, independent of its title.
    [[nodiscard]] virtual bool begin_window(NodeId id, std::string_view title, Rect bounds) = 0;
    virtual void end_window() = 0;
    virtual void text(std::string_view value, Rect bounds) = 0;
    [[nodiscard]] virtual bool button(std::string_view label, Rect bounds, bool enabled) = 0;
};

/// Renderer-supplied intrinsic measurements for nodes without a preferred size.
class LayoutContext {
public:
    virtual ~LayoutContext() = default;
    [[nodiscard]] virtual Size measure_text(std::string_view value) = 0;
    [[nodiscard]] virtual Size measure_button(std::string_view label) = 0;
};

/// A retained UI node. Nodes have one owning parent at most.
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
    [[nodiscard]] const LayoutParams& layout_params() const noexcept { return layout_; }
    [[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }
    [[nodiscard]] Dirty dirty() const noexcept { return dirty_; }

    void set_visible(bool value) noexcept;
    void set_enabled(bool value) noexcept;
    /// Stores a normalized layout description: dimensions, margins, and grow
    /// are non-negative, and each maximum is at least its minimum.
    void set_layout_params(LayoutParams value) noexcept;
    void invalidate(Dirty flags = Dirty::paint) noexcept;
    virtual void clear_dirty_recursive() noexcept;
    virtual void apply_default_layout(LayoutContext& context);

    [[nodiscard]] virtual Size measure(Size available) const noexcept;
    virtual void arrange(Rect bounds) noexcept;
    virtual void render(RenderContext& context) = 0;

protected:
    /// Updates dimensions owned by the renderer's intrinsic measurement pass.
    /// A non-zero preferred dimension supplied through set_layout_params opts
    /// that dimension out of automatic measurement until it is reset to zero.
    void set_automatic_preferred_size(Size value) noexcept;

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
    LayoutParams layout_{};
    Rect bounds_{};
    Dirty dirty_ = Dirty::structure | Dirty::layout | Dirty::paint;
    bool automatic_preferred_width_ = true;
    bool automatic_preferred_height_ = true;
};

class Container : public Node {
public:
    ~Container() override;
    void append(NodePtr child);
    [[nodiscard]] NodePtr remove(Node& child);
    void clear();
    [[nodiscard]] const std::vector<NodePtr>& children() const noexcept { return children_; }
    void clear_dirty_recursive() noexcept override;
    void apply_default_layout(LayoutContext& context) override;
    void render(RenderContext& context) override;

protected:
    void set_tree_recursive(UiTree* tree) noexcept override;
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
    void render(RenderContext& context) override;
private:
    std::string title_;
};

class Text final : public Node {
public:
    explicit Text(std::string value = {});
    [[nodiscard]] std::string_view value() const noexcept { return value_; }
    void set_value(std::string_view value);
    void apply_default_layout(LayoutContext& context) override;
    void render(RenderContext& context) override;
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
    void apply_default_layout(LayoutContext& context) override;
    void render(RenderContext& context) override;
private:
    std::string label_;
    std::function<void(Button&)> on_click_;
};

/// Owns one root and runs retained layout in logical pixels.
class UiTree final {
public:
    void set_root(NodePtr root);
    [[nodiscard]] const NodePtr& root() const noexcept { return root_; }
    void apply_default_layout(LayoutContext& context);
    void layout(Size available) noexcept;
    /// Invokes a FIFO snapshot of pending UI callbacks. Events queued by a
    /// callback wait for a later flush. Returns the number of callbacks run.
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
