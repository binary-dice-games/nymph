// MIT License © 2026 Binary Dice Games
/// @file element_class.hpp
/// @brief An element type (`Plot`, `PlotLine`, ...): its fields, where it
///        may be placed, and how it draws.
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "schema/field_desc.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

class draw_context;

/// @brief Slot of elements that can be `root` or a layout child.
inline constexpr std::string_view kFigureSlot = "figure";

/// @brief Base of every element type. Subclasses pass their schema to the
///        constructor and implement draw().
///
/// Field order is fixed: a node's `values[i]` is field `i`, so draw()
/// reads fields through index constants kept next to the class.
class element_class {
 public:
  /// @param name        Type name used in sources (`PlotLine`).
  /// @param slot        What the element is (`figure`, `plot2d.item`).
  /// @param accepts     Slots of the children it takes; empty for none.
  /// @param fields      Its fields, in index order.
  /// @param description One or two sentences for `nymph schema`.
  element_class(
      std::string name,
      std::string slot,
      std::vector<std::string> accepts,
      std::vector<field_desc> fields,
      std::string description);
  virtual ~element_class() = default;

  element_class(const element_class&) = delete;
  element_class& operator=(const element_class&) = delete;

  std::string_view name() const {
    return name_;
  }
  std::string_view slot() const {
    return slot_;
  }
  std::span<const std::string> accepts() const {
    return accepts_;
  }
  std::span<const field_desc> fields() const {
    return fields_;
  }
  std::string_view description() const {
    return description_;
  }

  /// @brief Whether a child of slot `slot` may be placed in this element.
  bool accepts_slot(std::string_view slot) const;

  /// @brief Index of field `name`, or -1.
  int field_index(std::string_view name) const;

  /// @brief A node of this class with every field at its default.
  node make_node() const;

  /// @brief Called by bind() on the `root` node only, after its fields are
  ///        set. `explicitly_set[i]` tells whether field `i` was written in
  ///        the source. Default: nothing. `Plot` uses it to fill the image
  ///        when its `height` was left out (wish nymph's rule).
  virtual void adjust_root(node& n, const std::vector<bool>& explicitly_set) const {
    (void)n;
    (void)explicitly_set;
  }

  /// @brief Draws `n` (a node of this class) into the open figure frame.
  virtual void draw(const node& n, draw_context& dc) const = 0;

 private:
  std::string name_;
  std::string slot_;
  std::vector<std::string> accepts_;
  std::vector<field_desc> fields_;
  std::string description_;
};

} // namespace bdg::nymph
