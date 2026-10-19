// MIT License © 2026 Binary Dice Games
/// @file editor_model.cpp
/// @brief The editor's state: selection, fields, validated changes, undo.
#include "edit/editor_model.hpp"
#include "core/error.hpp"
#include "document/view.hpp"
#include "edit/field_value.hpp"
#include "edit/image_fields.hpp"
#include "edit/source_edit.hpp"
#include "elements/register_builtin.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

bool is_text_style_field(std::string_view name) {
  return name == "font" || name == "font_size" || name == "text_color";
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
  if (!(merge && key == last_key_ && !undo_.empty())) {
    undo_.push_back(source_);
    if (undo_.size() > kMaxUndo) {
      undo_.pop_front();
    }
  }
  redo_.clear();
  last_key_ = key;
  source_ = std::move(next);
  figure_ = std::make_unique<figure>(std::move(replacement));
  commit_result r;
  r.ok = true;
  r.source = source_;
  return r;
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

std::optional<std::string> editor_model::undo() {
  if (undo_.empty()) {
    return std::nullopt;
  }
  redo_.push_back(source_);
  source_ = std::move(undo_.back());
  undo_.pop_back();
  last_key_.clear();
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
  undo_.push_back(source_);
  source_ = std::move(redo_.back());
  redo_.pop_back();
  last_key_.clear();
  refresh();
  if (selection_ && !element_paths(figure_->root).contains(selection_->element)) {
    selection_.reset();
  }
  return source_;
}

} // namespace bdg::nymph
