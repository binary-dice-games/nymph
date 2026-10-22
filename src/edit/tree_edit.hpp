// MIT License © 2026 Binary Dice Games
/// @file tree_edit.hpp
/// @brief Structural text patches to a nymph YAML source: insert, remove,
///        move and duplicate elements, wrap the root in a layout, and keep
///        the `view` key's paths in step
///        (spec/14_editor_workspace/DESIGN.md section 3, `tree_edit`). Like
///        source_edit, they leave every other byte as written and do not
///        validate the result: the caller binds it.
#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "schema/node.hpp"

namespace bdg::nymph {

/// @brief Where element paths went after a structural change: each path's
///        new path, or nothing when the element was removed.
///
/// Rules are grouped in stages applied one after the other (a change made
/// of several steps); within a stage the first rule whose `from` is the
/// path or one of its ancestors applies, so descendants move with their
/// element. A path no rule matches is unchanged.
class path_map {
 public:
  /// @brief Adds a rule to the current stage: `from` and its descendants
  ///        go to `to` (nullopt: removed).
  void add(std::string from, std::optional<std::string> to);

  /// @brief Appends the stages of `next`, applied after these.
  void then(const path_map& next);

  /// @brief The new path of `path`; nullopt when it was removed.
  std::optional<std::string> apply(std::string_view path) const;

  bool empty() const {
    return stages_.empty();
  }

 private:
  struct rule {
    std::string from;
    std::optional<std::string> to;
  };
  std::vector<std::vector<rule>> stages_;
};

/// @brief A structural change: the new source, where paths went, and the
///        path of the element it made or moved (empty for a removal).
struct tree_edit_result {
  std::string source;
  path_map paths;
  std::string path;
};

/// @brief Index meaning "after the last child".
inline constexpr size_t kAppend = std::numeric_limits<size_t>::max();

/// @brief Inserts an element into `parent`'s `children` at `index` (kAppend:
///        last), as a list item in the style of its siblings (a block item
///        `- {...}` or a flow item); adds the `children` key when missing.
/// @param element_yaml The element as one line of flow YAML, `{type: X, ...}`.
/// @throws nymph::error for an unknown parent, an index past the end, a
///         multi-line `element_yaml`, or a `children` value that is not a list.
tree_edit_result
insert_child(std::string_view source, std::string_view parent, size_t index, std::string_view element_yaml);

/// @brief Removes the element at `path` (not `root`) with its lines; the
///        last child leaves `children: []`.
/// @throws nymph::error for an unknown path or `root`.
tree_edit_result remove_element(std::string_view source, std::string_view path);

/// @brief Moves the element at `path` to position `index` among its siblings
///        (clamped to the last), its text kept as written.
/// @throws nymph::error for an unknown path or `root`.
tree_edit_result move_element(std::string_view source, std::string_view path, size_t index);

/// @brief Copies the element at `path` after itself. Every `id` in the copy
///        gets a fresh value (not in `used_ids`), and the copy's own text
///        fields that named one of its old ids name the new one.
/// @throws nymph::error for an unknown path or `root`.
tree_edit_result duplicate_element(std::string_view source, std::string_view path, const std::set<std::string>& used_ids);

/// @brief Makes `root` the first child of a new root of type `layout_type`
///        (`VerticalLayout`, `HorizontalLayout`), re-indenting it.
/// @throws nymph::error for a source without `root` or whose top level is
///         not a block mapping.
tree_edit_result wrap_root(std::string_view source, std::string_view layout_type);

/// @brief Renames the keys of the top-level `view` mapping by `map` and
///        drops the entries of removed elements (the whole key when none is
///        left). The source unchanged when it has no `view` mapping.
std::string rename_view_paths(std::string_view source, const path_map& map);

/// @brief A value for an `id` field: `base` followed by the smallest number
///        from 1 that makes it not in `used` (`node1`, `node2`).
std::string fresh_id(std::string_view base, const std::set<std::string>& used);

/// @brief Every non-empty `id` value in the tree under `root`.
std::set<std::string> ids_in(const node& root);

/// @brief The elements that refer to the element at `path` or one of its
///        descendants by `id`: those of the same diagram (the nearest
///        ancestor that is a figure) whose text field, or an item of whose
///        text list, equals such an id. In document order, the removed
///        subtree left out.
std::vector<std::string> find_references(const node& root, std::string_view path);

/// @brief Compares element paths in document order (`root/children/2`
///        before `root/children/10`, a parent before its children).
bool path_before(std::string_view a, std::string_view b);

} // namespace bdg::nymph
