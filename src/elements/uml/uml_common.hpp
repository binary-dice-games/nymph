// MIT License © 2026 Binary Dice Games
/// @file uml_common.hpp
/// @brief What the UML diagram elements share: the diagram base class (fields,
///        ImUml scope, fit check, id checks), the child base class, `Note`,
///        and small field helpers (spec/03_uml_diagrams/DESIGN.md section 4).
#pragma once

#include <imuml.h>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "elements/uml/uml.hpp"
#include "render/draw_context.hpp"
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::uml {

enum class diagram_kind { class_diagram, sequence, state, activity, use_case, flowchart, er, data_flow, network };

/// @brief The kind of the diagram whose ImUml scope is open, so `Note`
///        calls the matching ImUml function.
diagram_kind current_kind();

// ── Children ─────────────────────────────────────────────────────────────────

/// @brief Base of every UML child element. Knows its `id` field and the
///        fields that name other nodes, which the diagram checks.
class item_class : public element_class {
 public:
  /// @param ref_fields Names of the text or text-list fields holding ids.
  item_class(
      std::string name,
      std::string slot,
      std::vector<std::string> accepts,
      std::vector<field_desc> fields,
      std::string description,
      std::vector<std::string> ref_fields = {});

  /// @brief Index of the `id` field, or -1.
  int id_field() const {
    return id_field_;
  }
  /// @brief Indices of the fields that name nodes.
  const std::vector<size_t>& ref_fields() const {
    return ref_fields_;
  }
  /// @brief Whether the element is an edge (counted against the edge limit).
  bool is_edge() const {
    return is_edge_;
  }

  /// @brief Index of field `name`; the field must exist.
  size_t field(std::string_view name) const;

 protected:
  void set_edge(bool edge) {
    is_edge_ = edge;
  }

 private:
  int id_field_ = -1;
  std::vector<size_t> ref_fields_;
  bool is_edge_ = false;
};

/// @brief The nodes of a diagram by id.
struct id_table {
  std::map<std::string, const node*, std::less<>> by_id;
  std::vector<std::string> ids; ///< In order, for suggestions.

  const node* find(std::string_view id) const;
  /// @brief The node `id` names; otherwise fails at field `field` of `n`
  ///        with a suggestion.
  const node& resolve(const node& n, size_t field, const std::string& id) const;
};

/// @brief The `id` of a node of an item class ("" for others).
const std::string& id_of(const node& n);

/// @brief Whether `n` is an element of type `type`.
inline bool is_a(const node& n, std::string_view type) {
  return n.cls->name() == type;
}

/// @brief The rules of `Note` that depend on the diagram kind; diagrams call
///        it for each Note in their subtree.
void check_note(const node& n, bool sequence);

/// @brief Calls `f` on every node below `n` (not `n` itself), depth first
///        in order; `depth` is 1 for `n`'s children.
void walk(const node& n, const std::function<void(const node&, size_t depth)>& f);

// ── Diagrams ─────────────────────────────────────────────────────────────────

/// @brief Base of the five diagram elements: their shared fields (title,
///        size hints, layout, zoom, colours, text style), the ImUml scope,
///        the fit check, and the id and reference checks.
class diagram_class : public element_class {
 public:
  diagram_class(
      std::string name,
      diagram_kind kind,
      const char* item_slot,
      std::vector<field_desc> extra_fields,
      std::string description);

  void check(const node& n) const override;
  ImVec2 measure(const node& n, draw_context& dc, float width) const override;
  /// @throws nymph::error when the diagram needs a zoom below `min_zoom` to fit.
  void arrange(const node& n, draw_context& dc, const rect& r) const override;
  void draw(const node& n, draw_context& dc) const override;
  /// @brief The `-Italic` sibling of `font`, used for abstract names.
  std::vector<std::string> implied_fonts(const node& n, const std::string& font) const override;

 protected:
  /// @brief The diagram's own rules (edge kinds, UML constraints), after
  ///        ids and references were checked.
  virtual void check_rules(const node& n, const id_table& ids) const = 0;
  /// @brief Setup calls of the diagram kind, after the shared ones.
  virtual void setup(const node& n) const {
    (void)n;
  }
  size_t field(std::string_view name) const;
  diagram_kind kind() const {
    return kind_;
  }

 private:
  /// Runs the ImUml scope: measure only when `r` is null, else draws at `r`.
  ImUmlDiagramInfo submit(const node& n, draw_context& dc, const rect* r) const;

  diagram_kind kind_;
  size_t title_, min_zoom_, max_zoom_, line_color_, fill_color_, node_spacing_;
  int direction_ = -1, edge_style_ = -1, rank_spacing_ = -1;
};

// ── Field helpers ────────────────────────────────────────────────────────────

/// @brief `"#RRGGBB[AA]"` as an ImU32; 0 for "" (the diagram's colour).
ImU32 color_u32(const std::string& hex);
/// @brief The field as a C string, or nullptr when it is empty.
const char* or_null(const node& n, size_t field);
/// @brief Calls ImUml::SetNextItemColors from the node's `color` and
///        (when present) `fill_color` fields.
void set_next_colors(const node& n);
/// @brief Calls ImUml::SetNextNodeRank from the node's `rank` field.
void set_next_rank(const node& n);
/// @brief Calls ImUml::SetNextEdgeLayout from `weight`, `min_length`, `constraint`.
void set_next_edge_layout(const node& n);

/// @brief `color` and `fill_color`.
std::vector<field_desc> node_color_fields();
/// @brief `rank` (dot's rank=same).
field_desc rank_field();
/// @brief `weight`, `min_length`, `constraint` (dot's attributes) and `color`.
std::vector<field_desc> edge_layout_fields();

/// @brief Appends `b` to `a`.
std::vector<field_desc> concat(std::vector<field_desc> a, const std::vector<field_desc>& b);

// ── Registration of each family ──────────────────────────────────────────────

void register_note(registry& reg);
void register_class_diagram(registry& reg);
void register_sequence_diagram(registry& reg);
void register_state_diagram(registry& reg);
void register_activity_diagram(registry& reg);
void register_usecase_diagram(registry& reg);
void register_flowchart(registry& reg);
void register_er_diagram(registry& reg);
void register_dataflow_diagram(registry& reg);
void register_network_diagram(registry& reg);

} // namespace bdg::nymph::uml
