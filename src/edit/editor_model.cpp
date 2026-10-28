// MIT License © 2026 Binary Dice Games
/// @file editor_model.cpp
/// @brief The editor's state: selection, fields, validated changes, undo.
#include "edit/editor_model.hpp"
#include <algorithm>
#include <string>
#include <variant>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "document/view.hpp"
#include "edit/csv_file.hpp"
#include "edit/element_starter.hpp"
#include "edit/field_value.hpp"
#include "edit/image_fields.hpp"
#include "edit/source_edit.hpp"
#include "edit/yaml_text.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/fonts_used.hpp"
#include "render/figure_renderer.hpp"
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

namespace {

bool is_text_style_field(std::string_view name) {
  return name == "font" || name == "font_size" || name == "text_color";
}

/// Field `field` of the element at `path` as the source writes it, on one
/// line: the text itself when it is on one line, else a list of its items.
std::string written_text(const std::string& source, const yaml_node& doc, std::string_view path, std::string_view field) {
  const yaml_node* element = find_element(doc, path);
  const yaml_node* value = element != nullptr ? element->find(field) : nullptr;
  if (value == nullptr) {
    return {};
  }
  const yaml_text::text_index t(source);
  const size_t from = t.offset(value->line, value->column);
  const size_t to = yaml_text::value_end(*value, t);
  std::string text = to > from ? source.substr(from, to - from) : std::string();
  if (text.find('\n') == std::string::npos) {
    return text;
  }
  if (value->is_sequence()) {
    std::string list = "[";
    for (size_t i = 0; i < value->children.size(); ++i) {
      list += (i ? ", " : "") + encode_text(value->children[i].text);
    }
    return list + "]";
  }
  return value->is_scalar() ? encode_text(value->text) : std::string();
}

/// A change's default label, from the key apply() merges by:
/// "root/children/1/title" -> "Set title", ".../title/clear" -> "Clear
/// title", "add/<path>" -> "Add element", "data/cell" -> "Edit data".
std::string change_label(const std::string& key) {
  const auto starts = [&](std::string_view prefix) { return key.rfind(prefix, 0) == 0; };
  if (starts("add/")) {
    return "Add element";
  }
  if (starts("remove/")) {
    return "Delete element";
  }
  if (starts("move/")) {
    return "Move element";
  }
  if (starts("duplicate/")) {
    return "Duplicate element";
  }
  if (starts("fields/")) {
    return "Set fields";
  }
  if (starts("top/")) {
    return "Set " + key.substr(4);
  }
  if (starts("data/")) {
    return "Edit data";
  }
  if (key == "source") {
    return "Edit source";
  }
  std::string_view k = key;
  bool clear = false;
  if (k.size() > 6 && k.substr(k.size() - 6) == "/clear") {
    clear = true;
    k.remove_suffix(6);
  }
  const size_t slash = k.rfind('/');
  const std::string field(slash == std::string_view::npos ? k : k.substr(slash + 1));
  return (clear ? "Clear " : "Set ") + field;
}

} // namespace

editor_model::editor_model(std::string source) : source_(std::move(source)), original_(source_) {
  refresh();
}

editor_model::~editor_model() = default;

void editor_model::refresh() {
  figure_ = std::make_unique<figure>(bind_source(source_, builtin_registry()));
}

void editor_model::reset(std::string source) {
  figure next = bind_source(source, builtin_registry()); // throws before anything changes
  figure_ = std::make_unique<figure>(std::move(next));
  source_ = std::move(source);
  original_ = source_;
  undo_.clear();
  redo_.clear();
  last_key_.clear();
  ++version_;
  if (selection_ && !element_paths(figure_->root).contains(selection_->element)) {
    selection_.reset();
  }
}

void editor_model::select(std::optional<edit_target_id> selection) {
  if (selection && selection->element != kImagePath && !element_paths(figure_->root).contains(selection->element)) {
    selection.reset();
  }
  selection_ = std::move(selection);
}

std::string editor_model::selected_type() const {
  if (!selection_) {
    return {};
  }
  if (selection_->element == kImagePath) {
    return "Image";
  }
  const auto paths = element_paths(figure_->root);
  const auto it = paths.find(selection_->element);
  return it == paths.end() ? std::string() : std::string(it->second->cls->name());
}

const field_desc* editor_model::find_field(std::string_view name) const {
  if (!selection_) {
    return nullptr;
  }
  if (selection_->element == kImagePath) {
    for (const field_desc& f : image_fields()) {
      if (f.name == name) {
        return &f;
      }
    }
    return nullptr;
  }
  const auto paths = element_paths(figure_->root);
  const auto it = paths.find(selection_->element);
  if (it == paths.end()) {
    return nullptr;
  }
  const int index = it->second->cls->field_index(name);
  return index < 0 ? nullptr : &it->second->cls->fields()[static_cast<size_t>(index)];
}

std::vector<editable_field> editor_model::fields() const {
  std::vector<editable_field> out;
  if (!selection_) {
    return out;
  }
  std::optional<yaml_node> doc;
  try {
    doc = parse_yaml(source_);
  } catch (const error&) {
  }
  if (selection_->element == kImagePath) {
    const yaml_node* written = doc ? doc->find("image") : nullptr;
    for (size_t i = 0; i < image_fields().size(); ++i) {
      editable_field f;
      f.desc = &image_fields()[i];
      f.current = image_field_value(figure_->image, i);
      f.written = written != nullptr && written->find(f.desc->name) != nullptr;
      out.push_back(std::move(f));
    }
    return out;
  }
  const auto paths = element_paths(figure_->root);
  const auto it = paths.find(selection_->element);
  if (it == paths.end()) {
    return out;
  }
  const node& n = *it->second;
  const bool figure_element = n.cls->slot() == kFigureSlot;
  for (size_t i = 0; i < n.cls->fields().size(); ++i) {
    editable_field f;
    f.desc = &n.cls->fields()[i];
    f.current = n.values[i];
    f.written = n.field_pos[i].line > 0;
    if (doc && f.written) {
      f.bound_to = field_reference(*doc, selection_->element, f.desc->name).value_or(std::string());
      f.source_text = written_text(source_, *doc, selection_->element, f.desc->name);
    }
    if (figure_element && is_text_style_field(f.desc->name) && n.cls->name() != "Label" &&
        n.cls->name() != "SeparatorText") {
      f.scope_note = "applies to all text of this " + std::string(n.cls->name());
    }
    out.push_back(std::move(f));
  }
  return out;
}

commit_result editor_model::fail(const std::string& message, int line, int column) const {
  commit_result r;
  r.source = source_;
  r.error = message;
  r.line = line;
  r.column = column;
  return r;
}

commit_result editor_model::apply(std::string next, const std::string& key, bool merge) {
  if (const std::optional<source_problem> problem = find_source_problem(next)) {
    return fail(problem->message, problem->line, problem->column);
  }
  figure replacement = bind_source(next, builtin_registry());
  try {
    // The view refuses a layout that does not fit. Fonts the host still has
    // to fetch (web) cannot be laid out yet; the view re-checks after they
    // arrive (live_view::set_source, workspace_host::need_fonts()).
    if (fonts_missing(replacement).empty()) {
      (void)layout_figure(replacement);
    }
  } catch (const error& e) {
    bool fitted = false; // an edit may not break a layout that fitted
    try {
      (void)layout_figure(*figure_);
      fitted = true;
    } catch (const error&) {
    }
    if (fitted) {
      return fail(e.what(), e.line(), e.column());
    }
  }
  if (group_ && group_->origin != origin_) {
    // Someone else's change is not part of the group: undoing the group
    // must not undo it (spec/17_agent_connectors/DESIGN.md section 4.4).
    group_.reset();
    last_key_.clear();
  }
  const std::string label = group_ ? group_->label : !label_.empty() ? label_ : change_label(key);
  const std::string origin = group_ ? group_->origin : origin_;
  const bool merged = merge && key == last_key_ && !undo_.empty();
  if (group_ && group_->pushed) {
    // One entry for the whole group: its first change pushed it.
  } else if (!merged) {
    undo_.push_back(undo_entry{source_, label, origin});
    if (undo_.size() > kMaxUndo) {
      undo_.pop_front();
    }
    if (group_) {
      group_->pushed = true;
    }
  }
  redo_.clear();
  last_key_ = key;
  source_ = std::move(next);
  figure_ = std::make_unique<figure>(std::move(replacement));
  ++version_;
  commit_result r;
  r.ok = true;
  r.source = source_;
  r.label = label;
  r.origin = origin;
  last_label_ = label;
  last_origin_ = origin;
  return r;
}

double editor_model::inherited_font_size() const {
  double size = figure_->image.font_size;
  if (!selection_ || selection_->element == kImagePath) {
    return size;
  }
  const auto paths = element_paths(figure_->root);
  const std::string& path = selection_->element;
  // Each ancestor from the root down: the nearest one with a size wins.
  for (size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1)) {
    const auto it = paths.find(std::string_view(path).substr(0, slash));
    if (it == paths.end()) {
      continue;
    }
    const node& n = *it->second;
    if (const int i = n.cls->field_index("font_size"); i >= 0) {
      const double* v = std::get_if<double>(&n.values[static_cast<size_t>(i)]);
      if (v != nullptr && *v > 0.0) {
        size = *v;
      }
    }
  }
  return size;
}

commit_result editor_model::commit(std::string_view field, std::string_view yaml_value, bool merge, bool replace_binding) {
  if (!selection_) {
    return fail("nothing is selected");
  }
  const field_desc* desc = find_field(field);
  if (desc == nullptr) {
    return fail("the selected element has no field '" + std::string(field) + "'");
  }
  try {
    if (!replace_binding) {
      if (const std::optional<yaml_node> doc = parse_yaml(source_);
          doc && field_reference(*doc, selection_->element, field)) {
        return fail("'" + std::string(field) + "' takes its text from the data; edit the data, or replace it");
      }
    }
    return apply(set_field(source_, selection_->element, field, yaml_value), selection_->element + "/" + std::string(field), merge);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::clear(std::string_view field) {
  if (!selection_) {
    return fail("nothing is selected");
  }
  if (find_field(field) == nullptr) {
    return fail("the selected element has no field '" + std::string(field) + "'");
  }
  try {
    std::string next = remove_field(source_, selection_->element, field);
    if (selection_->element == kImagePath) {
      // An image section left without options would not be a mapping.
      const yaml_node doc = parse_yaml(next);
      const yaml_node* image = doc.find("image");
      const bool empty = image != nullptr && ((image->is_mapping() && image->keys.empty()) ||
                                              (image->is_scalar() && image->text.empty() && !image->quoted()));
      if (empty) {
        next = replace_top_level(next, "image", "").source;
      }
    }
    return apply(std::move(next), selection_->element + "/" + std::string(field) + "/clear", false);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

void editor_model::begin_group(std::string label, std::string origin) {
  if (group_) {
    return;
  }
  group_ = open_group{std::move(label), origin.empty() ? std::string("user") : std::move(origin), false};
  last_key_.clear(); // the group's first change never merges into an earlier one
}

void editor_model::end_group() {
  group_.reset();
  last_key_.clear();
}

std::optional<std::string> editor_model::undo() {
  if (undo_.empty()) {
    return std::nullopt;
  }
  group_.reset();
  undo_entry back = std::move(undo_.back());
  undo_.pop_back();
  redo_.push_back(undo_entry{source_, back.label, back.origin});
  last_label_ = "Undo " + back.label;
  last_origin_ = origin_;
  source_ = std::move(back.source);
  last_key_.clear();
  ++version_;
  refresh();
  if (selection_ && !element_paths(figure_->root).contains(selection_->element)) {
    selection_.reset();
  }
  return source_;
}

std::optional<std::string> editor_model::redo() {
  if (redo_.empty()) {
    return std::nullopt;
  }
  group_.reset();
  undo_entry back = std::move(redo_.back());
  redo_.pop_back();
  undo_.push_back(undo_entry{source_, back.label, back.origin});
  last_label_ = "Redo " + back.label;
  last_origin_ = origin_;
  source_ = std::move(back.source);
  last_key_.clear();
  ++version_;
  refresh();
  if (selection_ && !element_paths(figure_->root).contains(selection_->element)) {
    selection_.reset();
  }
  return source_;
}

// ── Structure ────────────────────────────────────────────────────────────────

namespace {

/// The parent of child path "P/children/N"; "" for `root`.
std::string parent_of(std::string_view path) {
  const size_t at = path.rfind("/children/");
  return at == std::string_view::npos ? std::string() : std::string(path.substr(0, at));
}

/// The place of child path "P/children/N".
size_t index_of(std::string_view path) {
  const size_t slash = path.rfind('/');
  return slash == std::string_view::npos ? 0 : static_cast<size_t>(std::stoul(std::string(path.substr(slash + 1))));
}

} // namespace

commit_result editor_model::apply_structure(tree_edit_result r, const std::string& key, const std::string& to_select) {
  std::string next = rename_view_paths(r.source, r.paths);
  std::optional<edit_target_id> kept = selection_;
  commit_result out = apply(std::move(next), key, false);
  if (!out.ok) {
    return out;
  }
  if (kept && kept->element != kImagePath) {
    const std::optional<std::string> moved = r.paths.apply(kept->element);
    kept = moved ? std::optional<edit_target_id>(edit_target_id{*moved, kept->field}) : std::nullopt;
  }
  select(std::move(kept)); // dropped when its element is gone
  if (!to_select.empty()) {
    selection_ = edit_target_id{to_select, ""};
  }
  out.paths = std::move(r.paths);
  out.path = std::move(r.path);
  return out;
}

std::vector<std::string> editor_model::candidate_ids(std::string_view parent) const {
  const auto paths = element_paths(figure_->root);
  // The diagram: the nearest figure at or above `parent`.
  std::string scope(parent);
  while (!scope.empty()) {
    const auto it = paths.find(scope);
    if (it != paths.end() && it->second->cls->fills_slot(kFigureSlot)) {
      break;
    }
    scope = parent_of(scope);
  }
  if (scope.empty()) {
    scope = "root";
  }
  std::vector<std::pair<std::string, std::string>> found; // path, id
  for (const auto& [p, n] : paths) {
    const bool inside = p.size() > scope.size() && p.compare(0, scope.size(), scope) == 0 && p[scope.size()] == '/';
    const int id = n->cls->field_index("id");
    if (!inside || id < 0) {
      continue;
    }
    const std::string& value = n->get<std::string>(static_cast<size_t>(id));
    if (!value.empty()) {
      found.emplace_back(p, value);
    }
  }
  std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return path_before(a.first, b.first); });
  std::vector<std::string> out;
  if (selection_) {
    for (const auto& [p, id] : found) {
      if (p == selection_->element) {
        out.push_back(id);
      }
    }
  }
  for (const auto& [p, id] : found) {
    if (out.empty() || id != out.front()) {
      out.push_back(id);
    }
  }
  return out;
}

commit_result editor_model::add_element(
    std::string_view parent,
    size_t index,
    std::string_view type,
    const std::vector<std::pair<std::string, std::string>>& fields) {
  const element_class* cls = builtin_registry().find(type);
  if (cls == nullptr) {
    return fail("there is no element type '" + std::string(type) + "'");
  }
  const auto paths = element_paths(figure_->root);
  const auto host = paths.find(parent);
  if (host == paths.end()) {
    return fail("there is no element '" + std::string(parent) + "'");
  }
  if (host->second->cls->placement_of(*cls) < 0) {
    return fail(
        std::string(host->second->cls->name()) + " does not take a " + std::string(type) + "; it takes " +
        builtin_registry().names_taken_by(*host->second->cls));
  }
  // The diagram's type decides a few starters (a sequence diagram's notes).
  std::string diagram;
  for (std::string up(parent); !up.empty(); up = parent_of(up)) {
    if (const auto it = paths.find(up); it != paths.end() && it->second->cls->fills_slot(kFigureSlot)) {
      diagram = std::string(it->second->cls->name());
      break;
    }
  }
  try {
    const new_element e = make_element(*cls, ids_in(figure_->root), candidate_ids(parent), diagram);
    tree_edit_result r = insert_child(source_, parent, index, e.yaml);
    for (const auto& [name, value] : fields) {
      if (name == "type") {
        continue;
      }
      r.source = value.empty() ? remove_field(r.source, r.path, name) : set_field(r.source, r.path, name, value);
    }
    const std::string path = r.path;
    return apply_structure(std::move(r), "add/" + path, path);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::add_chart(std::string_view type, chart_placement placement) {
  const element_class* cls = builtin_registry().find(type);
  if (cls == nullptr || !cls->fills_slot(kFigureSlot)) {
    return fail("'" + std::string(type) + "' is not a chart");
  }
  if (figure_->root.cls->placement_of(*cls) >= 0) {
    return add_element("root", kAppend, type);
  }
  try {
    tree_edit_result wrapped =
        wrap_root(source_, placement == chart_placement::below ? "VerticalLayout" : "HorizontalLayout");
    const new_element e = make_element(*cls, ids_in(figure_->root), {});
    tree_edit_result added = insert_child(wrapped.source, "root", kAppend, e.yaml);
    // A root fills the image; in a layout the two share it: both stretch
    // along the layout (when they have the size and do not fix it).
    const std::string size = placement == chart_placement::below ? "height" : "width";
    const auto stretch = [&](const element_class& c, const std::string& path, bool written) {
      if (c.field_index(size) >= 0 && !written) {
        added.source = set_field(added.source, path, size, "-1");
      }
    };
    const int old_size = figure_->root.cls->field_index(size);
    stretch(
        *figure_->root.cls, "root/children/0",
        old_size >= 0 && figure_->root.field_pos[static_cast<size_t>(old_size)].line > 0);
    stretch(*cls, added.path, false);
    wrapped.paths.then(added.paths);
    added.paths = std::move(wrapped.paths);
    const std::string path = added.path;
    return apply_structure(std::move(added), "add/" + path, path);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

std::vector<std::string> editor_model::references(std::string_view path) const {
  return find_references(figure_->root, path);
}

commit_result editor_model::remove_element(std::string_view path, bool with_references) {
  if (path == "root" || path == kImagePath) {
    return fail("the root element cannot be removed");
  }
  if (!element_paths(figure_->root).contains(path)) {
    return fail("there is no element '" + std::string(path) + "'");
  }
  std::vector<std::string> doomed{std::string(path)};
  if (with_references) {
    const std::vector<std::string> refs = references(path);
    doomed.insert(doomed.end(), refs.begin(), refs.end());
  }
  // The last first, so the paths of the others stay valid.
  std::sort(doomed.begin(), doomed.end(), [](const std::string& a, const std::string& b) { return path_before(b, a); });
  try {
    tree_edit_result all;
    all.source = source_;
    for (const std::string& p : doomed) {
      tree_edit_result step = bdg::nymph::remove_element(all.source, p);
      all.source = std::move(step.source);
      all.paths.then(step.paths);
    }
    // The selection goes to the next sibling, else the previous, else the parent.
    std::string next;
    if (selection_ && all.paths.apply(selection_->element) == std::nullopt) {
      const std::string parent = parent_of(path);
      const size_t index = index_of(path);
      const figure after = bind_source(all.source, builtin_registry());
      const auto now = element_paths(after.root);
      const std::string here = parent + "/children/" + std::to_string(index);
      const std::string before = index > 0 ? parent + "/children/" + std::to_string(index - 1) : std::string();
      next = now.contains(here) ? here : !before.empty() && now.contains(before) ? before : parent;
    }
    return apply_structure(std::move(all), "remove/" + std::string(path), next);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::move_element(std::string_view path, size_t index) {
  try {
    tree_edit_result r = bdg::nymph::move_element(source_, path, index);
    if (r.paths.empty()) {
      commit_result same;
      same.ok = true;
      same.source = source_;
      same.path = r.path;
      return same;
    }
    const std::string moved = r.path;
    return apply_structure(std::move(r), "move/" + moved, std::string()); // the selection follows by its path
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::reparent_element(std::string_view path, std::string_view parent, size_t index) {
  try {
    tree_edit_result r = bdg::nymph::reparent_element(source_, path, parent, index);
    if (r.paths.empty()) {
      commit_result same;
      same.ok = true;
      same.source = source_;
      same.path = r.path;
      return same;
    }
    const std::string moved = r.path;
    return apply_structure(std::move(r), "move/" + moved, std::string()); // the selection follows by its path
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::duplicate_element(std::string_view path) {
  try {
    tree_edit_result r = bdg::nymph::duplicate_element(source_, path, ids_in(figure_->root));
    const std::string copy = r.path;
    return apply_structure(std::move(r), "duplicate/" + copy, copy);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::set_fields(
    std::string_view path,
    const std::vector<std::pair<std::string, std::optional<std::string>>>& fields) {
  if (path != kImagePath && !element_paths(figure_->root).contains(path)) {
    return fail("there is no element '" + std::string(path) + "'");
  }
  try {
    std::string next = source_;
    for (const auto& [name, value] : fields) {
      if (name == "type" || name == "children") {
        return fail("'" + name + "' is not a field: add, move or remove elements instead");
      }
      next = value ? set_field(next, path, name, *value) : remove_field(next, path, name);
    }
    return apply(std::move(next), "fields/" + std::string(path), false);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

std::string editor_model::top_level_text(std::string_view key) const {
  try {
    const yaml_node doc = parse_yaml(source_);
    const yaml_node* v = doc.find(key);
    return v != nullptr && v->is_scalar() ? v->text : std::string();
  } catch (const error&) {
    return {};
  }
}

commit_result editor_model::set_top_level_text(std::string_view key, std::string_view text) {
  if (key != "description" && key != "source") {
    return fail("'" + std::string(key) + "' is not a text key of the source");
  }
  try {
    const std::string value = text.empty() ? std::string() : encode_text(text);
    return apply(replace_top_level(source_, key, value).source, "top/" + std::string(key), false);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

commit_result editor_model::set_source(std::string source) {
  commit_result r;
  try {
    r = apply(std::move(source), "source", false);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
  if (r.ok) {
    select(selection_); // dropped when its element is gone
  }
  return r;
}

// ── Data ─────────────────────────────────────────────────────────────────────

template <typename Patch>
commit_result editor_model::apply_data(const std::string& key, Patch patch) {
  try {
    return apply(patch(), key, false);
  } catch (const error& e) {
    return fail(e.what(), e.line(), e.column());
  }
}

template <typename Change>
commit_result editor_model::change_table(std::string_view table, const std::string& key, Change change) {
  return apply_data(key, [&] {
    for (data_table& t : tables()) {
      if (t.name == table) {
        change(t.cells);
        return bdg::nymph::set_table(source_, table, t.cells);
      }
    }
    throw error(0, 0, table.empty() ? std::string("the source has no data") : "the data has no table '" + std::string(table) + "'");
  });
}

std::vector<data_table> editor_model::tables() const {
  try {
    return read_tables(source_);
  } catch (const error&) {
    return {};
  }
}

commit_result editor_model::set_cell(std::string_view table, size_t row, size_t column, std::string_view text) {
  return change_table(table, "data/cell", [&](table_text& t) {
    if (row >= t.rows.size() || column >= t.width()) {
      throw error(0, 0, "the table has no cell at row " + std::to_string(row + 1) + ", column " + std::to_string(column + 1));
    }
    t.rows[row][column] = std::string(text);
  });
}

commit_result editor_model::set_table(std::string_view table, const table_text& cells) {
  return change_table(table, "data/table", [&](table_text& t) {
    if (cells.width() == 0) {
      throw error(0, 0, "a table needs at least one column");
    }
    for (const std::vector<std::string>& row : cells.rows) {
      if (row.size() != cells.width()) {
        throw error(0, 0, "every row needs as many cells as the header");
      }
    }
    t = cells;
  });
}

commit_result editor_model::paste_cells(std::string_view table, size_t row, size_t column, std::string_view text) {
  const std::vector<std::vector<std::string>> block = parse_rows(text);
  return change_table(table, "data/paste", [&](table_text& t) {
    for (size_t r = 0; r < block.size(); ++r) {
      for (size_t c = 0; c < block[r].size(); ++c) {
        while (column + c >= t.width()) {
          // A new column, named like the ones a spreadsheet would show.
          std::string name = "column" + std::to_string(t.width() + 1);
          t.headers.push_back(name);
          for (std::vector<std::string>& existing : t.rows) {
            existing.emplace_back();
          }
        }
        while (row + r >= t.rows.size()) {
          t.rows.emplace_back(t.width());
        }
        t.rows[row + r][column + c] = block[r][c];
      }
    }
  });
}

commit_result editor_model::insert_rows(std::string_view table, size_t index, size_t count) {
  return change_table(table, "data/rows", [&](table_text& t) {
    const size_t at = std::min(index, t.rows.size());
    t.rows.insert(t.rows.begin() + static_cast<long>(at), count, std::vector<std::string>(t.width()));
  });
}

commit_result editor_model::remove_rows(std::string_view table, size_t index, size_t count) {
  return change_table(table, "data/rows", [&](table_text& t) {
    if (index >= t.rows.size()) {
      throw error(0, 0, "the table has no row " + std::to_string(index + 1));
    }
    const size_t n = std::min(count, t.rows.size() - index);
    t.rows.erase(t.rows.begin() + static_cast<long>(index), t.rows.begin() + static_cast<long>(index + n));
  });
}

commit_result editor_model::insert_column(std::string_view table, size_t index, std::string_view name) {
  return change_table(table, "data/columns", [&](table_text& t) {
    if (name.empty()) {
      throw error(0, 0, "a column needs a name");
    }
    if (std::find(t.headers.begin(), t.headers.end(), name) != t.headers.end()) {
      throw error(0, 0, "the table already has a column '" + std::string(name) + "'");
    }
    const size_t at = std::min(index, t.width());
    t.headers.insert(t.headers.begin() + static_cast<long>(at), std::string(name));
    for (std::vector<std::string>& row : t.rows) {
      row.insert(row.begin() + static_cast<long>(at), std::string());
    }
  });
}

commit_result editor_model::remove_column(std::string_view table, size_t index) {
  return change_table(table, "data/columns", [&](table_text& t) {
    if (index >= t.width()) {
      throw error(0, 0, "the table has no column " + std::to_string(index + 1));
    }
    if (t.width() == 1) {
      throw error(0, 0, "a table needs at least one column; delete the table instead");
    }
    t.headers.erase(t.headers.begin() + static_cast<long>(index));
    for (std::vector<std::string>& row : t.rows) {
      row.erase(row.begin() + static_cast<long>(index));
    }
  });
}

commit_result editor_model::rename_column(std::string_view table, std::string_view from, std::string_view to) {
  return apply_data("data/rename", [&] { return bdg::nymph::rename_column(source_, table, from, to); });
}

commit_result editor_model::add_table(std::string_view name, const table_text& cells, std::string_view first_name) {
  return apply_data("data/add", [&] { return bdg::nymph::add_table(source_, name, cells, first_name); });
}

commit_result editor_model::remove_table(std::string_view name) {
  return apply_data("data/remove", [&] { return bdg::nymph::remove_table(source_, name); });
}

commit_result editor_model::rename_table(std::string_view from, std::string_view to) {
  return apply_data("data/rename", [&] { return bdg::nymph::rename_table(source_, from, to); });
}

std::vector<std::string> editor_model::column_references() const {
  std::vector<std::string> out;
  for (const data_table& t : tables()) {
    for (const std::string& h : t.cells.headers) {
      out.push_back(encode_text("$" + (t.name.empty() ? h : t.name + ":" + h)));
    }
  }
  return out;
}

std::string editor_model::fresh_table_name() const {
  const std::vector<data_table> all = tables();
  for (int n = 1;; ++n) {
    const std::string name = "table" + std::to_string(n);
    if (std::none_of(all.begin(), all.end(), [&](const data_table& t) { return t.name == name; })) {
      return name;
    }
  }
}

table_text editor_model::starter_table() {
  return table_text{{"x", "y"}, {{"1", "1"}, {"2", "4"}, {"3", "9"}}};
}

} // namespace bdg::nymph
