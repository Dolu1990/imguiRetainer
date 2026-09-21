#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rgui {

using NodeId = std::uint64_t;

/// A size in scale-1 logical pixels. Built-in layout nodes convert fixed
/// dimensions to Dear ImGui pixels while measuring or drawing.
struct Size {
    float width = 0.0F;
    float height = 0.0F;
};

/// A non-binding size offered to a node by its parent. An absent axis leaves
/// that axis at the node's preferred size.
struct SizeProposal {
    std::optional<float> width;
    std::optional<float> height;
};

/// An RGBA color whose components are finite values from zero to one.
class Color {
public:
    Color(float red, float green, float blue, float alpha);
    [[nodiscard]] float red() const noexcept { return red_; }
    [[nodiscard]] float green() const noexcept { return green_; }
    [[nodiscard]] float blue() const noexcept { return blue_; }
    [[nodiscard]] float alpha() const noexcept { return alpha_; }
    constexpr bool operator==(const Color&) const = default;

private:
    float red_;
    float green_;
    float blue_;
    float alpha_;
};

class Container;
class Table;
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
    void setVisible(bool value) noexcept;
    void setEnabled(bool value) noexcept;
    /// Returns this node's preferred size in the current Dear ImGui context.
    /// Custom nodes that participate in an AnchoredPanel should override this.
    [[nodiscard]] virtual Size measure() const;
    /// Returns the size this node accepts for a parent-proposed size. The
    /// default implementation preserves intrinsic sizing by ignoring it.
    [[nodiscard]] virtual Size measure(const SizeProposal& proposal) const;
    virtual void draw() = 0;
    /// Draws using the size accepted by measure(SizeProposal). The default
    /// preserves existing custom nodes by calling draw().
    virtual void draw(Size resolved_size);

protected:
    /// Returns the owning tree's layout scale, or 1.0 for a detached node.
    /// Custom nodes should use this when converting their own logical geometry
    /// to Dear ImGui pixels.
    [[nodiscard]] float layoutScale() const noexcept;

private:
    friend class Container;
    friend class Table;
    friend class Button;
    friend class Text;
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
    /// Structural changes must occur outside UiTree::draw().
    virtual void append(NodePtr child);
    [[nodiscard]] virtual NodePtr remove(Node& child);
    /// Replaces a direct child in place and returns the detached former child.
    /// Structural changes must occur outside UiTree::draw().
    [[nodiscard]] NodePtr replace(Node& old_child, NodePtr new_child);
    virtual void clear();
    [[nodiscard]] const std::vector<NodePtr>& children() const noexcept { return children_; }
    void draw() override;

protected:
    void draw_child(Node& child);
    void draw_child(Node& child, Size resolved_size);
    void draw_children();
    void set_tree_recursive(UiTree* tree) noexcept override;
    std::vector<NodePtr> children_;
};

enum class Axis { horizontal, vertical };

/// Positions content within an available horizontal or vertical extent.
enum class Justification { start, center, end };

/// Draws children in Dear ImGui's natural flow. Horizontal stacks separate
/// items with SameLine(); vertical stacks leave ImGui's default spacing.
class Stack : public Container {
public:
    explicit Stack(Axis axis = Axis::vertical) noexcept : axis_(axis) {}
    void setAxis(Axis value) noexcept;
    [[nodiscard]] Axis axis() const noexcept { return axis_; }
    void draw() override;

private:
    Axis axis_;
};

/// Draws an explicit grid of retained nodes as a Dear ImGui table. Empty and
/// hidden cells retain their coordinates. Rows and columns use zero-based
/// indices in C++.
class Table final : public Node {
public:
    explicit Table(std::size_t columns);
    ~Table() override;
    [[nodiscard]] std::size_t columns() const noexcept { return columns_.size(); }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_.size(); }
    /// Resizes the explicit row model. Removed cells are detached.
    void resizeRows(std::size_t rows);
    /// Returns the node at a coordinate, or nullptr for an empty cell.
    [[nodiscard]] NodePtr cell(std::size_t row, std::size_t column) const;
    /// Inserts or replaces a cell. A row beyond rows() grows the table.
    void setCell(std::size_t row, std::size_t column, NodePtr child);
    /// Empties an existing cell and detaches its node.
    void clearCell(std::size_t row, std::size_t column);
    void setHeader(std::size_t column, std::string_view value);
    [[nodiscard]] std::string_view header(std::size_t column) const;
    /// Sizes this column to its header and cell contents.
    void setColumnFit(std::size_t column);
    /// Requests a positive fixed width in logical pixels for this column.
    void setColumnWidth(std::size_t column, float width);
    /// Makes this column share remaining width in proportion to a positive weight.
    void setColumnWeight(std::size_t column, float weight);
    /// Positions every cell in this column within its available cell rectangle.
    void setColumnJustify(std::size_t column, Justification horizontal,
                          Justification vertical);
    /// Overrides a row background color, growing the table if necessary.
    void setRowColor(std::size_t row, Color color);
    /// Removes a row background override and restores Dear ImGui's default.
    void clearRowColor(std::size_t row);
    [[nodiscard]] std::optional<Color> rowColor(std::size_t row) const;
    void setInnerHorizontalBorders(bool value) noexcept;
    [[nodiscard]] bool innerHorizontalBorders() const noexcept { return inner_horizontal_borders_; }
    void setOuterHorizontalBorders(bool value) noexcept;
    [[nodiscard]] bool outerHorizontalBorders() const noexcept { return outer_horizontal_borders_; }
    void setInnerVerticalBorders(bool value) noexcept;
    [[nodiscard]] bool innerVerticalBorders() const noexcept { return inner_vertical_borders_; }
    void setOuterVerticalBorders(bool value) noexcept;
    [[nodiscard]] bool outerVerticalBorders() const noexcept { return outer_vertical_borders_; }
    void setHorizontalBorders(bool value) noexcept;
    void setVerticalBorders(bool value) noexcept;
    void draw() override;

private:
    enum class ColumnSizing { fit, fixed, stretch };
    struct Column {
        std::string header;
        ColumnSizing sizing = ColumnSizing::stretch;
        float width_or_weight = 1.0F;
        Justification horizontal_justification = Justification::start;
        Justification vertical_justification = Justification::start;
    };
    struct Row {
        std::vector<NodePtr> cells;
        std::optional<Color> color;
    };
    [[nodiscard]] Column& column(std::size_t index);
    [[nodiscard]] const Column& column(std::size_t index) const;
    [[nodiscard]] Row& row(std::size_t index);
    [[nodiscard]] const Row& row(std::size_t index) const;
    void detach(NodePtr& child) noexcept;
    void draw_cell(Node& child);
    void set_tree_recursive(UiTree* tree) noexcept override;
    std::vector<Column> columns_;
    std::vector<Row> rows_;
    bool inner_horizontal_borders_ = true;
    bool outer_horizontal_borders_ = true;
    bool inner_vertical_borders_ = true;
    bool outer_vertical_borders_ = true;
};

/// Draws children in a bordered fixed-size region. The fixed size is in
/// logical pixels; Dear ImGui adds scrollbars when the children's contents
/// overflow the scaled region.
class ScrollArea final : public Container {
public:
    explicit ScrollArea(Size size);
    [[nodiscard]] Size size() const noexcept { return size_; }
    void setSize(Size size);
    void draw() override;

private:
    Size size_;
};

/// A normalized point within a rectangle. `{0.0F, 0.0F}` is its top-left
/// corner and `{1.0F, 1.0F}` is its bottom-right corner.
struct AnchorPoint {
    float x = 0.0F;
    float y = 0.0F;

    constexpr bool operator==(const AnchorPoint&) const = default;
};

/// Positions a child relative to an AnchoredPanel. `self` and `target` are
/// normalized points in their respective rectangles; the child is placed so
/// these points coincide before the logical-pixel offset is applied. A second
/// anchor can be supplied to an AnchoredPanel child; differing self points on
/// an axis derive a size proposal for that axis. Fractions must be finite and
/// in the inclusive range from zero to one.
struct Anchor {
    AnchorPoint self{};
    AnchorPoint target{};
    float offsetX = 0.0F;
    float offsetY = 0.0F;
};

/// How an AnchoredPanel resolves one of its dimensions.
enum class PanelExtent { fixed, fill };

/// Describes an opt-in main-viewport layout for a Window. Fixed dimensions and
/// anchor offsets are in logical pixels. The primary anchor positions the
/// window; a secondary anchor may derive its width and/or height when its
/// corresponding self coordinate differs.
struct WindowLayout {
    Size size{};
    PanelExtent widthExtent = PanelExtent::fixed;
    PanelExtent heightExtent = PanelExtent::fixed;
    Anchor primary{};
    std::optional<Anchor> secondary;
};

/// A retained Dear ImGui window. Screen layout, when configured, is resolved
/// against Dear ImGui's main viewport on every draw. Fixed dimensions and
/// offsets are converted from logical pixels using the owning UiTree's layout
/// scale.
class Window final : public Container {
public:
    explicit Window(std::string title = {});
    [[nodiscard]] std::string_view title() const noexcept { return title_; }
    void setTitle(std::string_view value);
    [[nodiscard]] float backgroundAlpha() const noexcept { return background_alpha_; }
    void setBackgroundAlpha(float value);
    [[nodiscard]] bool decorated() const noexcept { return decorated_; }
    void setDecorated(bool value) noexcept;
    [[nodiscard]] bool movable() const noexcept { return movable_; }
    void setMovable(bool value) noexcept;
    [[nodiscard]] bool resizable() const noexcept { return resizable_; }
    void setResizable(bool value) noexcept;
    void setScreenLayout(WindowLayout value);
    [[nodiscard]] const std::optional<WindowLayout>& screenLayout() const noexcept { return screen_layout_; }
    void clearScreenLayout() noexcept;
    void draw() override;
private:
    std::string title_;
    float background_alpha_ = 1.0F;
    bool decorated_ = true;
    bool movable_ = true;
    bool resizable_ = true;
    std::optional<WindowLayout> screen_layout_;
};

/// A retained layout surface. Fixed dimensions use the supplied logical Size;
/// fill dimensions use the current Dear ImGui content region when drawn.
/// It owns each child's placement while the child remains responsible for
/// measuring and drawing itself.
class AnchoredPanel final : public Container {
public:
    explicit AnchoredPanel(Size size, PanelExtent widthExtent = PanelExtent::fixed,
                           PanelExtent heightExtent = PanelExtent::fixed);
    [[nodiscard]] Size size() const noexcept { return size_; }
    void setSize(Size size);
    [[nodiscard]] PanelExtent widthExtent() const noexcept { return width_extent_; }
    [[nodiscard]] PanelExtent heightExtent() const noexcept { return height_extent_; }
    void setWidthExtent(PanelExtent value) noexcept;
    void setHeightExtent(PanelExtent value) noexcept;
    [[nodiscard]] Size measure() const override;

    void append(NodePtr child) override;
    void append(NodePtr child, Anchor anchor);
    void append(NodePtr child, Anchor primary_anchor, Anchor secondary_anchor);
    [[nodiscard]] NodePtr remove(Node& child) override;
    void clear() override;
    void setAnchor(Node& child, Anchor anchor);
    [[nodiscard]] Anchor anchor(const Node& child) const;
    void setSecondAnchor(Node& child, std::optional<Anchor> anchor);
    [[nodiscard]] const std::optional<Anchor>& secondAnchor(const Node& child) const;
    void draw() override;

private:
    [[nodiscard]] std::size_t child_index(const Node& child) const;
    Size size_;
    PanelExtent width_extent_;
    PanelExtent height_extent_;
    struct ChildAnchors {
        Anchor primary;
        std::optional<Anchor> secondary;
    };
    std::vector<ChildAnchors> anchors_;
};

class Text final : public Node {
public:
    explicit Text(std::string value = {});
    [[nodiscard]] std::string_view value() const noexcept { return value_; }
    void setValue(std::string_view value);
    [[nodiscard]] float fontScale() const noexcept { return font_scale_; }
    void setFontScale(float scale);
    void setOnClick(std::function<void(Node&)> callback);
    void activate();
    [[nodiscard]] Size measure() const override;
    void draw() override;
private:
    std::string value_;
    float font_scale_ = 1.0F;
    std::function<void(Node&)> on_click_;
};

class Button final : public Node {
public:
    explicit Button(std::string label = {});
    [[nodiscard]] std::string_view label() const noexcept { return label_; }
    void setLabel(std::string_view value);
    [[nodiscard]] float fontScale() const noexcept { return font_scale_; }
    void setFontScale(float scale);
    void setOnClick(std::function<void(Node&)> callback);
    void activate();
    [[nodiscard]] Size measure() const override;
    [[nodiscard]] Size measure(const SizeProposal& proposal) const override;
    void draw() override;
    void draw(Size resolved_size) override;
private:
    std::string label_;
    float font_scale_ = 1.0F;
    std::function<void(Node&)> on_click_;
};

/// Owns one root and draws it into the caller-owned current Dear ImGui frame.
class UiTree final {
public:
    UiTree() = default;
    ~UiTree() noexcept;
    UiTree(const UiTree&) = delete;
    UiTree& operator=(const UiTree&) = delete;
    UiTree(UiTree&& other) noexcept;
    UiTree& operator=(UiTree&& other) noexcept;

    /// Sets the logical-pixel to Dear ImGui-pixel scale for this tree. The
    /// value must be finite and greater than zero, and cannot change while the
    /// tree is drawing.
    void setLayoutScale(float scale);
    [[nodiscard]] float layoutScale() const noexcept { return layout_scale_; }

    void setRoot(NodePtr root);
    [[nodiscard]] const NodePtr& root() const noexcept { return root_; }
    void draw();
    [[nodiscard]] std::size_t flushEvents();
    [[nodiscard]] std::size_t pendingEventCount() const noexcept { return events_.size(); }
private:
    friend class Button;
    friend class Container;
    friend class Table;
    friend class Text;
    struct Event {
        std::weak_ptr<Node> target;
        std::uint64_t attachment_generation = 0;
        std::function<void(Node&)> callback;
    };

    void enqueue_event(const std::weak_ptr<Node>& target, std::uint64_t attachment_generation,
                       std::function<void(Node&)> callback);
    NodePtr root_;
    std::vector<Event> events_;
    float layout_scale_ = 1.0F;
    bool drawing_ = false;
};

} // namespace rgui
