// MIT License © 2026 Binary Dice Games
/// @file tree_menu.cpp
/// @brief The element tree's menu and dialogs (see tree_menu.hpp).
#include "edit/tree_menu.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include "core/error.hpp"
#include "document/view.hpp"
#include "edit/element_icons.hpp"
#include "edit/element_starter.hpp"
#include "edit/field_value.hpp"
#include "edit/image_fields.hpp"
#include "edit/inspector.hpp"
#include "edit/source_edit.hpp"
#include "edit/tree_edit.hpp"
#include "edit/yaml_text.hpp"
#include "elements/register_builtin.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

namespace {

constexpr const char* kNewDialog = "New element";
constexpr const char* kDeleteDialog = "Delete element";

std::string parent_of(std::string_view path) {
  const size_t at = path.rfind("/children/");
  return at == std::string_view::npos ? std::string() : std::string(path.substr(0, at));
}

size_t index_of(std::string_view path) {
  const size_t slash = path.rfind('/');
  return slash == std::string_view::npos ? 0 : static_cast<size_t>(std::stoul(std::string(path.substr(slash + 1))));
}

/// What a change did, for the inspector: the new source and moved paths,
/// or the refusal.
void report(const commit_result& r, inspector_events& events) {
  if (r.ok) {
    events.source = r.source;
    events.paths.then(r.paths);
    events.error.clear();
  } else {
    events.error = r.line > 0 ? "line " + std::to_string(r.line) + ": " + r.error : r.error;
  }
}

/// The node at `path` of the model's figure, or nullptr.
const node* node_at(const editor_model& model, std::string_view path) {
  const auto paths = element_paths(model.bound().root);
  const auto it = paths.find(path);
  return it == paths.end() ? nullptr : it->second;
}

/// The type of the figure `parent` belongs to (the nearest at or above it).
std::string diagram_type(const editor_model& model, std::string parent) {
  for (; !parent.empty(); parent = parent_of(parent)) {
    const node* n = node_at(model, parent);
    if (n != nullptr && n->cls->fills_slot(kFigureSlot)) {
      return std::string(n->cls->name());
    }
  }
  return {};
}

/// An icon and a menu item.
bool type_item(const element_class& cls) {
  const float icon = ImGui::GetTextLineHeight();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::PushID(cls.name().data(), cls.name().data() + cls.name().size());
  ImGui::SetCursorScreenPos(ImVec2(at.x + icon + ImGui::GetStyle().ItemInnerSpacing.x, at.y));
  const std::string label(cls.name());
  const bool chosen = ImGui::MenuItem(label.c_str());
  if (!cls.description().empty()) {
    ImGui::SetItemTooltip("%s", std::string(cls.description()).c_str());
  }
  draw_element_icon(ImGui::GetWindowDrawList(), at, icon, cls.name());
  ImGui::PopID();
  return chosen;
}

} // namespace

// ── Choices ──────────────────────────────────────────────────────────────────

std::vector<type_group> child_types(const element_class& parent) {
  std::vector<type_group> out;
  std::vector<const element_class*> listed;
  const registry& reg = builtin_registry();
  for (const std::string& slot : parent.accepts()) {
    type_group g{slot, {}};
    for (const element_class* cls : reg.classes()) {
      if (cls->fills_slot(slot) && std::find(listed.begin(), listed.end(), cls) == listed.end()) {
        listed.push_back(cls);
        g.types.push_back(cls);
      }
    }
    if (!g.types.empty()) {
      out.push_back(std::move(g));
    }
  }
  return out;
}

std::optional<drop_place>
plan_drop(const editor_model& model, const std::string& dragged, const edit_element& row, float fraction, bool open) {
  const node* moved = dragged == "root" ? nullptr : node_at(model, dragged);
  if (moved == nullptr) {
    return std::nullopt;
  }
  const size_t from = index_of(dragged);
  const std::string from_parent = parent_of(dragged);
  const auto place = [&](const std::string& parent, size_t index) -> std::optional<drop_place> {
    const node* p = parent.empty() ? nullptr : node_at(model, parent);
    if (p == nullptr || parent == dragged || parent.compare(0, dragged.size() + 1, dragged + "/") == 0) {
      return std::nullopt;
    }
    const auto accepted = p->cls->accepts();
    if (std::none_of(accepted.begin(), accepted.end(), [&](const std::string& slot) { return moved->cls->fills_slot(slot); })) {
      return std::nullopt;
    }
    if (parent == from_parent && (index == from || index == from + 1)) {
      return std::nullopt; // the element stays where it is
    }
    return drop_place{parent, index};
  };
  const auto before = [&] { return place(parent_of(row.path), index_of(row.path)); };
  const auto after = [&] { return place(parent_of(row.path), index_of(row.path) + 1); };
  const auto inside = [&] {
    const node* r = node_at(model, row.path);
    return r == nullptr ? std::nullopt : place(row.path, r->children.size());
  };
  const bool beside = row.path != "root";
  const std::optional<drop_place> first = fraction < 0.25f ? (beside ? before() : std::nullopt)
      : fraction > 0.75f && !open                          ? (beside ? after() : std::nullopt)
                                                           : inside();
  if (first) {
    return first;
  }
  if (fraction < 0.25f || fraction > 0.75f) {
    return inside();
  }
  if (!beside) {
    return std::nullopt;
  }
  return fraction < 0.5f ? before() : (open ? std::nullopt : after());
}

std::vector<const element_class*> chart_types() {
  std::vector<const element_class*> out;
  for (const element_class* cls : builtin_registry().classes()) {
    if (cls->fills_slot(kFigureSlot)) {
      out.push_back(cls);
    }
  }
  return out;
}

element_draft element_draft::make(const editor_model& model, std::string parent, size_t index, std::string type) {
  element_draft d;
  d.parent = std::move(parent);
  d.index = index;
  d.type = std::move(type);
  const element_class* cls = builtin_registry().find(d.type);
  if (cls == nullptr) {
    d.error = "there is no element type '" + d.type + "'";
    return d;
  }
  const new_element e =
      make_element(*cls, ids_in(model.bound().root), model.candidate_ids(d.parent), diagram_type(model, d.parent));
  // The starter's fields, each value's text as written.
  const yaml_node starter = parse_yaml(e.yaml);
  const yaml_text::text_index t(e.yaml);
  for (size_t i = 0; i < starter.keys.size(); ++i) {
    if (starter.keys[i].text == "type") {
      continue;
    }
    const yaml_node& v = starter.children[i];
    const size_t from = t.offset(v.line, v.column);
    std::string text = e.yaml.substr(from, yaml_text::value_end(v, t) - from);
    if (text == "\"\"") {
      text.clear(); // a link end to fill
    }
    d.fields.emplace_back(starter.keys[i].text, text);
  }
  d.starter_count = d.fields.size();
  for (const field_desc& f : cls->fields()) {
    const bool listed = std::any_of(d.fields.begin(), d.fields.end(), [&](const auto& p) { return p.first == f.name; });
    if (!listed) {
      d.fields.emplace_back(f.name, "");
    }
  }
  d.check(model);
  return d;
}

std::vector<std::pair<std::string, std::string>> element_draft::written() const {
  std::vector<std::pair<std::string, std::string>> out;
  for (const auto& [name, text] : fields) {
    if (!text.empty()) {
      out.emplace_back(name, text);
    }
  }
  return out;
}

bool element_draft::check(const editor_model& model) {
  try {
    tree_edit_result r = insert_child(model.source(), parent, index, "{type: " + type + "}");
    for (const auto& [name, text] : written()) {
      r.source = set_field(r.source, r.path, name, text);
    }
    const std::optional<source_problem> problem = find_source_problem(r.source);
    error = problem ? problem->message : std::string();
  } catch (const nymph::error& e) {
    error = e.what();
  }
  return error.empty();
}

// ── Requests ─────────────────────────────────────────────────────────────────

void tree_menu::add(editor_model& model, const std::string& parent, size_t index, const std::string& type, inspector_events& events) {
  const commit_result r = model.add_element(parent, index, type);
  if (r.ok) {
    report(r, events);
    events.select = model.selection();
    return;
  }
  // As started it does not bind (a link with no ids to name, a rule of the
  // diagram): the dialog asks for the fields.
  draft_ = element_draft::make(model, parent, index, type);
  if (draft_->error.empty()) {
    draft_->error = r.error;
  }
  open_draft_ = true;
}

bool tree_menu::insert_draft(editor_model& model, inspector_events& events) {
  if (!draft_ || !draft_->check(model)) {
    return false;
  }
  const commit_result r = model.add_element(draft_->parent, draft_->index, draft_->type, draft_->written());
  if (!r.ok) {
    draft_->error = r.error;
    return false;
  }
  report(r, events);
  events.select = model.selection();
  draft_.reset();
  return true;
}

void tree_menu::remove(editor_model& model, const std::string& path, inspector_events& events) {
  std::vector<std::string> refs = model.references(path);
  if (refs.empty()) {
    report(model.remove_element(path), events);
    events.select = model.selection();
    return;
  }
  pending_delete_ = std::make_pair(path, std::move(refs));
  open_delete_ = true;
}

void tree_menu::duplicate(editor_model& model, const std::string& path, inspector_events& events) {
  report(model.duplicate_element(path), events);
  events.select = model.selection();
}

void tree_menu::move(editor_model& model, const std::string& path, int delta, inspector_events& events) {
  const size_t index = index_of(path);
  if (delta < 0 && index == 0) {
    return;
  }
  const node* p = node_at(model, parent_of(path));
  if (delta > 0 && (p == nullptr || index + 1 >= p->children.size())) {
    return;
  }
  report(model.move_element(path, delta < 0 ? index - 1 : index + 1), events);
  events.select = model.selection();
}

void tree_menu::drop(editor_model& model, const std::string& path, const drop_place& place, inspector_events& events) {
  // Within one parent the index counts the list without the element.
  const size_t index =
      place.parent == parent_of(path) && place.index > index_of(path) ? place.index - 1 : place.index;
  report(model.reparent_element(path, place.parent, index), events);
  events.select = model.selection();
}

void tree_menu::confirm_delete(editor_model& model, bool with_references, inspector_events& events) {
  if (!pending_delete_) {
    return;
  }
  report(model.remove_element(pending_delete_->first, with_references), events);
  events.select = model.selection();
  pending_delete_.reset();
}

// ── Drawing ──────────────────────────────────────────────────────────────────

void tree_menu::row(editor_model& model, const edit_element& e, inspector_events& events) {
  if (e.path == kImagePath) {
    return;
  }
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    events.select = edit_target_id{e.path, ""};
  }
  if (ImGui::BeginPopupContextItem("##menu")) {
    items(model, e, events);
    ImGui::EndPopup();
  }
}

void tree_menu::type_menu(
    editor_model& model,
    const char* label,
    const std::vector<type_group>& groups,
    const std::string& parent,
    size_t index,
    inspector_events& events) {
  if (groups.empty() || !ImGui::BeginMenu(label)) {
    return;
  }
  for (const type_group& g : groups) {
    if (groups.size() > 1) {
      ImGui::SeparatorText(g.slot.c_str());
    }
    for (const element_class* cls : g.types) {
      if (type_item(*cls)) {
        add(model, parent, index, std::string(cls->name()), events);
      }
    }
  }
  ImGui::EndMenu();
}

void tree_menu::items(editor_model& model, const edit_element& e, inspector_events& events) {
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    ImGui::CloseCurrentPopup();
  }
  const node* n = node_at(model, e.path);
  if (n == nullptr) {
    ImGui::TextDisabled("(changed: reopen the menu)");
    return;
  }
  const bool is_root = e.path == "root";
  ImGui::TextDisabled("%s", e.type.c_str());
  ImGui::Separator();
  const uint64_t version = model.version();
  type_menu(model, "Add child", child_types(*n->cls), e.path, kAppend, events);
  if (model.version() != version) {
    return; // the figure was rebuilt: `n` is gone
  }
  if (is_root && n->cls->placement_of(*builtin_registry().find("Plot")) < 0) {
    // The root is not a layout: a new chart goes next to it.
    for (const auto& [label, placement] :
         {std::pair<const char*, chart_placement>{"Add chart below", chart_placement::below},
          std::pair<const char*, chart_placement>{"Add chart to the right", chart_placement::right}}) {
      if (ImGui::BeginMenu(label)) {
        for (const element_class* cls : chart_types()) {
          if (type_item(*cls)) {
            const commit_result r = model.add_chart(cls->name(), placement);
            report(r, events);
            events.select = model.selection();
          }
        }
        ImGui::EndMenu();
      }
    }
  }
  if (is_root) {
    return;
  }
  const std::string parent = parent_of(e.path);
  const size_t index = index_of(e.path);
  const node* p = node_at(model, parent);
  const size_t siblings = p != nullptr ? p->children.size() : 0;
  if (p != nullptr) {
    const std::vector<type_group> groups = child_types(*p->cls);
    type_menu(model, "Insert before", groups, parent, index, events);
    type_menu(model, "Insert after", groups, parent, index + 1, events);
    if (model.version() != version) {
      return;
    }
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
    duplicate(model, e.path, events);
  }
  if (ImGui::MenuItem("Move up", "Alt+Up", false, index > 0)) {
    move(model, e.path, -1, events);
  }
  if (ImGui::MenuItem("Move down", "Alt+Down", false, index + 1 < siblings)) {
    move(model, e.path, +1, events);
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Delete", "Del")) {
    remove(model, e.path, events);
  }
}

void tree_menu::open_at(std::string path, float x, float y) {
  figure_menu_ = std::move(path);
  figure_x_ = x;
  figure_y_ = y;
  open_figure_menu_ = true;
}

void tree_menu::dialogs(editor_model& model, inspector_events& events) {
  // The menu of an element right-clicked in the figure: the tree row's menu.
  if (open_figure_menu_) {
    ImGui::OpenPopup("##figure_menu");
    open_figure_menu_ = false;
  }
  ImGui::SetNextWindowPos(ImVec2(figure_x_, figure_y_), ImGuiCond_Appearing);
  if (ImGui::BeginPopup("##figure_menu")) {
    const node* n = node_at(model, figure_menu_);
    if (n == nullptr) {
      ImGui::CloseCurrentPopup();
    } else {
      items(model, edit_element{figure_menu_, std::string(n->cls->name()), "", 0}, events);
    }
    ImGui::EndPopup();
  }
  if (open_draft_) {
    ImGui::OpenPopup(kNewDialog);
    open_draft_ = false;
  }
  if (open_delete_) {
    ImGui::OpenPopup(kDeleteDialog);
    open_delete_ = false;
  }
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  const ImVec2 middle(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
  const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

  ImGui::SetNextWindowPos(middle, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(360, 0), ImVec2(560, vp->Size.y * 0.85f));
  if (ImGui::BeginPopupModal(kNewDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
    if (!draft_) {
      ImGui::CloseCurrentPopup();
    } else {
      element_draft& d = *draft_;
      ImGui::Text("New %s in %s", d.type.c_str(), d.parent.c_str());
      ImGui::TextDisabled("Values are YAML, as in the source.");
      ImGui::Separator();
      const std::vector<std::string> ids = model.candidate_ids(d.parent);
      const element_class* cls = builtin_registry().find(d.type);
      bool changed = false;
      bool others_open = false;
      bool others_header = false;
      for (size_t i = 0; i < d.fields.size(); ++i) {
        auto& [name, text] = d.fields[i];
        // The starter's fields first; the rest under a header.
        const bool starter = i < d.starter_count;
        if (!starter && !others_header) {
          others_header = true;
          others_open = ImGui::CollapsingHeader("Other fields");
        }
        if (!starter && !others_open) {
          continue;
        }
        ImGui::PushID(static_cast<int>(i));
        char buffer[512];
        std::strncpy(buffer, text.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer))) {
          text = buffer;
          changed = true;
        }
        if (cls != nullptr) {
          const int index = cls->field_index(name);
          if (index >= 0 && !cls->fields()[static_cast<size_t>(index)].description.empty()) {
            ImGui::SetItemTooltip("%s", cls->fields()[static_cast<size_t>(index)].description.c_str());
          }
        }
        // Link ends: pick an id of the diagram.
        if (!ids.empty() && (name == "from" || name == "to" || name == "over" || name == "attach")) {
          ImGui::SameLine();
          if (ImGui::SmallButton("ids")) {
            ImGui::OpenPopup("##ids");
          }
          if (ImGui::BeginPopup("##ids")) {
            for (const std::string& id : ids) {
              if (ImGui::Selectable(id.c_str())) {
                text = name == "over" || name == "attach" ? "[" + encode_text(id) + "]" : encode_text(id);
                changed = true;
              }
            }
            ImGui::EndPopup();
          }
        }
        ImGui::PopID();
      }
      if (changed) {
        d.check(model);
      }
      if (!d.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
        ImGui::TextWrapped("%s", d.error.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
      }
      ImGui::Spacing();
      ImGui::BeginDisabled(!d.error.empty());
      if (ImGui::Button("Insert", ImVec2(110, 0)) && insert_draft(model, events)) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(110, 0)) || escape) {
        draft_.reset();
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

  ImGui::SetNextWindowPos(middle, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(kDeleteDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
    if (!pending_delete_) {
      ImGui::CloseCurrentPopup();
    } else {
      const auto& [path, refs] = *pending_delete_;
      const node* n = node_at(model, path);
      ImGui::Text(
          "%s is named by %d other element%s:", n != nullptr ? std::string(n->cls->name()).c_str() : path.c_str(),
          static_cast<int>(refs.size()), refs.size() == 1 ? "" : "s");
      for (const std::string& r : refs) {
        const node* ref = node_at(model, r);
        ImGui::BulletText("%s  %s", ref != nullptr ? std::string(ref->cls->name()).c_str() : "", r.c_str());
      }
      ImGui::Spacing();
      if (ImGui::Button("Delete with them")) {
        confirm_delete(model, true, events);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("Delete only this")) {
        confirm_delete(model, false, events); // usually refused: they would name nothing
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel") || escape) {
        pending_delete_.reset();
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
}

} // namespace bdg::nymph
