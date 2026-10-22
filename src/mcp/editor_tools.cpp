// MIT License © 2026 Binary Dice Games
/// @file editor_tools.cpp
/// @brief The editor's MCP tools (see editor_tools.hpp).
#include "mcp/editor_tools.hpp"
#include <algorithm>
#include <cmath>
#include "core/error.hpp"
#include "document/view.hpp"
#include "document/yaml_doc.hpp"
#include "edit/data_edit.hpp"
#include "edit/field_value.hpp"
#include "edit/image_fields.hpp"
#include "edit/tree_edit.hpp"
#include "edit/yaml_text.hpp"
#include "elements/register_builtin.hpp"
#include "mcp/base64.hpp"
#include "schema/element_class.hpp"
#include "schema/name_table.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::mcp {

namespace {

// ── Arguments ────────────────────────────────────────────────────────────────

std::string string_arg(const json& args, const char* name, bool required = true) {
  if (!args.contains(name)) {
    if (required) {
      throw tool_error(std::string("missing argument '") + name + "'");
    }
    return {};
  }
  if (!args[name].is_string()) {
    throw tool_error(std::string("'") + name + "' must be a string");
  }
  return args[name].get<std::string>();
}

std::optional<int64_t> int_arg(const json& args, const char* name) {
  if (!args.contains(name)) {
    return std::nullopt;
  }
  if (!args[name].is_number_integer() || args[name].get<int64_t>() < 0) {
    throw tool_error(std::string("'") + name + "' must be a whole number from 0");
  }
  return args[name].get<int64_t>();
}

editor_model& model_of(editor_host& host) {
  editor_model* m = host.model();
  if (m == nullptr) {
    throw tool_error("the editor panel is hidden: ask the user to press E in the nymph window");
  }
  return *m;
}

/// Refuses a change when the user changed something since the agent read.
void check_version(const editor_model& m, const json& args) {
  if (args.contains("expected_version")) {
    if (!args["expected_version"].is_number_integer()) {
      throw tool_error("'expected_version' must be the integer editor_state returned");
    }
    if (args["expected_version"].get<uint64_t>() != m.version()) {
      throw tool_error(
          "the chart changed since you read it (version " + std::to_string(m.version()) +
          " now): read editor_state again before changing it");
    }
  }
}

// ── Values ───────────────────────────────────────────────────────────────────

/// A JSON argument as the one-line YAML value of field `f` (nullptr: unknown).
std::string yaml_value(const field_desc* f, const json& v) {
  if (v.is_boolean()) {
    return encode_bool(v.get<bool>());
  }
  if (v.is_number_integer()) {
    return encode_int(v.get<int64_t>());
  }
  if (v.is_number()) {
    return encode_number(v.get<double>());
  }
  if (v.is_string()) {
    const std::string s = v.get<std::string>();
    const bool texty = f == nullptr || f->type == field_type::text || f->type == field_type::color ||
        f->type == field_type::text_array || (!s.empty() && s[0] == '$');
    if (texty || s.empty()) {
      return encode_text(s);
    }
    if (s.find_first_of("\r\n") != std::string::npos) {
      throw tool_error("'" + (f != nullptr ? f->name : std::string("value")) + "' is written on one line");
    }
    return s; // a number, a name, flags "A|B": as written
  }
  if (v.is_array()) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
      const json& item = v[i];
      if (!(item.is_number() || item.is_string() || item.is_boolean() || item.is_null())) {
        throw tool_error("a list holds numbers or texts");
      }
      out += (i ? ", " : "") + (item.is_null() ? std::string("~") : item.is_string() ? encode_text(item.get<std::string>()) : yaml_value(nullptr, item));
    }
    return out + "]";
  }
  throw tool_error("a field value is a string, number, boolean or list (null clears it)");
}

/// The value a field holds, as JSON (enumerations by name).
json json_value(const field_desc& f, const value& v) {
  return std::visit(
      [&](const auto& x) -> json {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, int64_t>) {
          if (f.names != nullptr && f.type == field_type::enumeration) {
            return encode_enum(f, x);
          }
          if (f.names != nullptr && f.type == field_type::flags) {
            return encode_flags(f, x);
          }
          return x;
        } else if constexpr (std::is_same_v<T, double>) {
          return std::isfinite(x) ? json(x) : json(nullptr);
        } else {
          return json(x);
        }
      },
      v);
}

const field_desc* field_of(const editor_model& m, std::string_view path, std::string_view name) {
  if (path == kImagePath) {
    for (const field_desc& f : image_fields()) {
      if (f.name == name) {
        return &f;
      }
    }
    return nullptr;
  }
  const auto paths = element_paths(m.bound().root);
  const auto it = paths.find(path);
  if (it == paths.end()) {
    return nullptr;
  }
  const int i = it->second->cls->field_index(name);
  return i < 0 ? nullptr : &it->second->cls->fields()[static_cast<size_t>(i)];
}

json type_names(const std::vector<const element_class*>& classes) {
  json out = json::array();
  for (const element_class* c : classes) {
    out.push_back(std::string(c->name()));
  }
  return out;
}

/// The written fields of the element at `path`, by name.
json written_fields(editor_model& m, const std::string& path) {
  const std::optional<edit_target_id> kept = m.selection();
  m.select(edit_target_id{path, ""});
  json out = json::object();
  for (const editable_field& f : m.fields()) {
    if (f.written) {
      out[f.desc->name] = f.bound_to.empty() ? json_value(*f.desc, f.current) : json(f.bound_to);
    }
  }
  m.select(kept);
  return out;
}

/// What moved where after a structural change, for the agent.
json moved_paths(const std::vector<std::string>& before, const path_map& map) {
  json out = json::object();
  for (const std::string& p : before) {
    const std::optional<std::string> to = map.apply(p);
    if (to != std::optional<std::string>(p)) {
      out[p] = to ? json(*to) : json(nullptr);
    }
  }
  return out;
}

std::vector<std::string> all_paths(const editor_model& m) {
  std::vector<std::string> out;
  for (const auto& [p, n] : element_paths(m.bound().root)) {
    out.push_back(p);
  }
  return out;
}

/// The result of a change: refused (the loader's message), or applied.
json change_result(editor_host& host, const commit_result& r, const std::string& tool, json extra = json::object()) {
  if (!r.ok) {
    std::string message = r.error;
    if (r.line > 0) {
      message = "line " + std::to_string(r.line) + ":" + std::to_string(r.column) + ": " + message;
    }
    throw tool_error("refused, nothing changed: " + message);
  }
  host.changed(r, tool);
  extra["version"] = model_of(host).version();
  json result = text_result(extra.dump());
  result["structuredContent"] = extra;
  return result;
}

json structured(const json& value) {
  json result = text_result(value.dump());
  result["structuredContent"] = value;
  return result;
}

// ── Reading ──────────────────────────────────────────────────────────────────

json editor_state(editor_host& host, const json&) {
  editor_model& m = model_of(host);
  json state;
  state["file"] = host.file();
  state["modified"] = m.modified();
  state["version"] = m.version();
  const std::optional<std::string> tab = host.data_tab();
  state["tab"] = tab ? json{{"table", *tab}} : json("chart");
  // The tree, with each element's id when it has one.
  const auto paths = element_paths(m.bound().root);
  json tree = json::array();
  for (const edit_element& e : host.tree()) {
    json item{{"path", e.path}, {"type", e.type}, {"depth", e.depth}};
    if (!e.label.empty()) {
      item["label"] = e.label;
    }
    if (const auto it = paths.find(e.path); it != paths.end()) {
      const int id = it->second->cls->field_index("id");
      if (id >= 0 && !it->second->get<std::string>(static_cast<size_t>(id)).empty()) {
        item["id"] = it->second->get<std::string>(static_cast<size_t>(id));
      }
    }
    tree.push_back(std::move(item));
  }
  state["tree"] = std::move(tree);
  if (const std::optional<edit_target_id> sel = m.selection()) {
    json selection{{"path", sel->element}, {"type", m.selected_type()}};
    if (!sel->field.empty()) {
      selection["field"] = sel->field;
    }
    selection["fields"] = written_fields(m, sel->element);
    if (const auto it = paths.find(sel->element); it != paths.end()) {
      selection["takes"] = type_names(builtin_registry().classes_taken_by(*it->second->cls));
    }
    state["selection"] = std::move(selection);
  } else {
    state["selection"] = nullptr;
  }
  json tables = json::array();
  for (const data_table& t : m.tables()) {
    tables.push_back(
        {{"name", t.name},
         {"form", t.form == table_form::csv ? "csv" : "columns"},
         {"columns", t.cells.headers},
         {"rows", t.cells.rows.size()}});
  }
  state["tables"] = std::move(tables);
  // The view on screen: the `view` key a save would write.
  try {
    const yaml_node doc = parse_yaml(host.view_source());
    const yaml_node* view = doc.find("view");
    json views = json::object();
    if (view != nullptr && view->is_mapping()) {
      const std::string source = host.view_source();
      const yaml_text::text_index t(source);
      for (size_t i = 0; i < view->keys.size(); ++i) {
        // The entry's text as written ("{x: [1, 2]}"): small and exact.
        const yaml_node& v = view->children[i];
        const size_t from = t.offset(v.line, v.column);
        views[view->keys[i].text] = source.substr(from, yaml_text::value_end(v, t) - from);
      }
    }
    state["view"] = std::move(views);
  } catch (const std::exception&) {
    state["view"] = json::object();
  }
  const std::string error = host.panel_error();
  state["error"] = error.empty() ? json(nullptr) : json(error);
  return structured(state);
}

json editor_source(editor_host& host, const json&) {
  editor_model& m = model_of(host);
  return structured({{"source", m.source()}, {"version", m.version()}});
}

json editor_element(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  const std::string path = string_arg(args, "path");
  const auto paths = element_paths(m.bound().root);
  const auto it = paths.find(path);
  if (path != kImagePath && it == paths.end()) {
    throw tool_error("there is no element '" + path + "'; editor_state lists them");
  }
  const std::optional<edit_target_id> kept = m.selection();
  m.select(edit_target_id{path, ""});
  json fields = json::array();
  for (const editable_field& f : m.fields()) {
    json field{{"name", f.desc->name}, {"type", type_name(f.desc->type)}, {"written", f.written}};
    field["value"] = f.bound_to.empty() ? json_value(*f.desc, f.current) : json(f.bound_to);
    if (f.desc->names != nullptr) {
      field["names"] = f.desc->names->names();
    }
    fields.push_back(std::move(field));
  }
  const std::string type = m.selected_type();
  m.select(kept);
  json out{{"path", path}, {"type", type}, {"fields", std::move(fields)}};
  if (it != paths.end()) {
    out["takes"] = type_names(builtin_registry().classes_taken_by(*it->second->cls));
    json children = json::array();
    for (size_t i = 0; i < it->second->children.size(); ++i) {
      children.push_back(path + "/children/" + std::to_string(i));
    }
    out["children"] = std::move(children);
    out["referenced_by"] = m.references(path);
  }
  return structured(out);
}

const data_table& find_table(const std::vector<data_table>& tables, const std::string& name) {
  for (const data_table& t : tables) {
    if (t.name == name) {
      return t;
    }
  }
  std::string names;
  for (const data_table& t : tables) {
    names += (names.empty() ? "" : ", ") + (t.name.empty() ? std::string("\"\" (the single table)") : t.name);
  }
  throw tool_error("there is no table '" + name + "'" + (names.empty() ? "; the chart has no data" : "; tables: " + names));
}

json editor_table(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  const std::vector<data_table> tables = m.tables();
  const data_table& t = find_table(tables, string_arg(args, "table", false));
  const size_t offset = static_cast<size_t>(int_arg(args, "offset").value_or(0));
  const size_t limit = static_cast<size_t>(int_arg(args, "limit").value_or(200));
  table_text part{t.cells.headers, {}};
  for (size_t r = offset; r < t.cells.rows.size() && r < offset + limit; ++r) {
    part.rows.push_back(t.cells.rows[r]);
  }
  return structured(
      {{"table", t.name},
       {"rows", t.cells.rows.size()},
       {"offset", offset},
       {"csv", csv_text(part)},
       {"version", m.version()}});
}

json editor_view_image(editor_host& host, const json& args) {
  const int max_width = static_cast<int>(int_arg(args, "max_width").value_or(1200));
  int width = 0;
  int height = 0;
  const std::vector<uint8_t> png = host.view_image(max_width, width, height);
  if (png.empty()) {
    throw tool_error("there is nothing on screen to capture");
  }
  json result;
  result["content"] = json::array(
      {{{"type", "image"}, {"data", base64_encode(png)}, {"mimeType", "image/png"}},
       {{"type", "text"}, {"text", "The figure as on screen, " + std::to_string(width) + "x" + std::to_string(height) + " pixels."}}});
  return result;
}

// ── Changing ─────────────────────────────────────────────────────────────────

json editor_select(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  if (args.contains("tab")) {
    const std::string tab = string_arg(args, "tab");
    if (tab == "chart") {
      host.show_tab(std::nullopt);
    } else {
      (void)find_table(m.tables(), tab);
      host.show_tab(tab);
    }
  }
  if (args.contains("path")) {
    const std::string path = string_arg(args, "path");
    if (path != kImagePath && !element_paths(m.bound().root).contains(path)) {
      throw tool_error("there is no element '" + path + "'");
    }
    host.select(edit_target_id{path, string_arg(args, "field", false)});
  }
  return structured({{"selection", m.selection() ? json(m.selection()->element) : json(nullptr)}});
}

json editor_set_fields(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  const std::string path = string_arg(args, "path");
  if (!args.contains("fields") || !args["fields"].is_object() || args["fields"].empty()) {
    throw tool_error("'fields' must be an object of field name to value (null clears a field)");
  }
  std::vector<std::pair<std::string, std::optional<std::string>>> fields;
  for (const auto& [name, v] : args["fields"].items()) {
    if (v.is_null()) {
      fields.emplace_back(name, std::nullopt);
    } else {
      fields.emplace_back(name, yaml_value(field_of(m, path, name), v));
    }
  }
  return change_result(host, m.set_fields(path, fields), "editor_set_fields");
}

json editor_add_element(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  const std::string parent = string_arg(args, "parent");
  const std::string type = string_arg(args, "type");
  const element_class* cls = builtin_registry().find(type);
  if (cls == nullptr) {
    throw tool_error("there is no element type '" + type + "'");
  }
  std::vector<std::pair<std::string, std::string>> fields;
  if (args.contains("fields")) {
    if (!args["fields"].is_object()) {
      throw tool_error("'fields' must be an object of field name to value");
    }
    for (const auto& [name, v] : args["fields"].items()) {
      const int i = cls->field_index(name);
      fields.emplace_back(name, v.is_null() ? std::string() : yaml_value(i < 0 ? nullptr : &cls->fields()[static_cast<size_t>(i)], v));
    }
  }
  const std::vector<std::string> before = all_paths(m);
  const std::optional<int64_t> index = int_arg(args, "index");
  const commit_result r = m.add_element(parent, index ? static_cast<size_t>(*index) : kAppend, type, fields);
  return change_result(host, r, "editor_add_element", {{"path", r.path}, {"moved", moved_paths(before, r.paths)}});
}

json editor_remove_element(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  const std::string path = string_arg(args, "path");
  const bool with_refs = args.contains("with_references") && args["with_references"].is_boolean() && args["with_references"].get<bool>();
  const std::vector<std::string> before = all_paths(m);
  const commit_result r = m.remove_element(path, with_refs);
  if (!r.ok && !with_refs && !m.references(path).empty()) {
    json refs = m.references(path);
    throw tool_error(
        "refused, nothing changed: " + r.error + ". Elements name it: " + refs.dump() +
        "; call again with with_references: true to remove them too");
  }
  return change_result(host, r, "editor_remove_element", {{"moved", moved_paths(before, r.paths)}});
}

json editor_move_element(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  const std::string path = string_arg(args, "path");
  const std::optional<int64_t> index = int_arg(args, "index");
  if (!index) {
    throw tool_error("missing argument 'index'");
  }
  const std::vector<std::string> before = all_paths(m);
  const commit_result r = m.move_element(path, static_cast<size_t>(*index));
  return change_result(host, r, "editor_move_element", {{"path", r.path}, {"moved", moved_paths(before, r.paths)}});
}

json editor_set_cells(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  const std::string name = string_arg(args, "table", false);
  const std::vector<data_table> tables = m.tables();
  table_text t = find_table(tables, name).cells;
  if (!args.contains("cells") || !args["cells"].is_array() || args["cells"].empty()) {
    throw tool_error("'cells' must be a list of {row, column, value}");
  }
  for (const json& c : args["cells"]) {
    if (!c.is_object() || !c.contains("row") || !c["row"].is_number_integer() || !c.contains("column") ||
        !c.contains("value")) {
      throw tool_error("each cell is {row, column, value}: row from 0, column a name or a number from 0");
    }
    const int64_t row = c["row"].get<int64_t>();
    size_t column = 0;
    if (c["column"].is_string()) {
      const auto it = std::find(t.headers.begin(), t.headers.end(), c["column"].get<std::string>());
      if (it == t.headers.end()) {
        throw tool_error("the table has no column '" + c["column"].get<std::string>() + "'");
      }
      column = static_cast<size_t>(it - t.headers.begin());
    } else if (c["column"].is_number_integer() && c["column"].get<int64_t>() >= 0) {
      column = static_cast<size_t>(c["column"].get<int64_t>());
    } else {
      throw tool_error("'column' is a name or a number from 0");
    }
    if (column >= t.width()) {
      throw tool_error("the table has " + std::to_string(t.width()) + " columns");
    }
    if (row < 0) {
      throw tool_error("'row' counts from 0");
    }
    while (static_cast<size_t>(row) >= t.rows.size()) {
      t.rows.emplace_back(t.width()); // a row past the end adds rows
    }
    const json& v = c["value"];
    t.rows[static_cast<size_t>(row)][column] = v.is_string() ? v.get<std::string>() : v.is_null() ? std::string() : v.dump();
  }
  return change_result(host, m.set_table(name, t), "editor_set_cells");
}

json editor_set_table(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  const std::string name = string_arg(args, "table", false);
  table_text t;
  try {
    t = table_from_csv(string_arg(args, "csv"));
  } catch (const error& e) {
    throw tool_error(std::string("the CSV does not parse: ") + e.what());
  }
  const std::vector<data_table> tables = m.tables();
  const bool exists = std::any_of(tables.begin(), tables.end(), [&](const data_table& d) { return d.name == name; });
  if (!exists) {
    const bool create = args.contains("create") && args["create"].is_boolean() && args["create"].get<bool>();
    if (!create) {
      (void)find_table(tables, name); // the error, with the table names
    }
    const std::string first = string_arg(args, "first_name", false);
    return change_result(host, m.add_table(name, t, first.empty() ? "table1" : first), "editor_set_table");
  }
  return change_result(host, m.set_table(name, t), "editor_set_table");
}

json editor_remove_table(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  return change_result(host, m.remove_table(string_arg(args, "table", false)), "editor_remove_table");
}

json editor_set_source(editor_host& host, const json& args) {
  editor_model& m = model_of(host);
  check_version(m, args);
  return change_result(host, m.set_source(string_arg(args, "source")), "editor_set_source");
}

json editor_undo_redo(editor_host& host, bool undo) {
  editor_model& m = model_of(host);
  const std::optional<std::string> s = undo ? m.undo() : m.redo();
  if (!s) {
    throw tool_error(undo ? "there is nothing to undo" : "there is nothing to redo");
  }
  commit_result r;
  r.ok = true;
  r.source = *s;
  return change_result(host, r, undo ? "editor_undo" : "editor_redo");
}

// ── Definitions ──────────────────────────────────────────────────────────────

json prop(const char* type, const char* description) {
  return {{"type", type}, {"description", description}};
}

json schema(json properties, std::vector<std::string> required = {}) {
  json s{{"type", "object"}, {"properties", std::move(properties)}, {"additionalProperties", false}};
  if (!required.empty()) {
    s["required"] = required;
  }
  return s;
}

json annotations(const char* title, bool read_only) {
  json a{{"title", title}, {"readOnlyHint", read_only}, {"openWorldHint", false}};
  if (!read_only) {
    a["destructiveHint"] = false; // every change can be undone in the editor
    a["idempotentHint"] = false;
  }
  return a;
}

const char* const kVersion = "The version from editor_state: the change is refused if the user changed the chart since.";

struct definition {
  const char* name;
  const char* title;
  const char* description;
  json input;
  bool read_only;
  json (*handler)(editor_host&, const json&);
};

std::vector<definition> definitions() {
  return {
      {"editor_state", "Editor state",
       "The chart open in the user's nymph editor: file, version, the element tree (path, type, label, id), the "
       "selected element with its written fields and the child types it takes, the data tables, the view on screen "
       "(zoom, pan) and the error the editor shows. Call first: 'this', 'the selected node' and the like mean "
       "the selection.",
       schema(json::object()), true, editor_state},
      {"editor_source", "Editor source", "The whole YAML source of the chart in the editor, and its version.",
       schema(json::object()), true, editor_source},
      {"editor_element", "Editor element",
       "One element's fields (value, written or default, allowed names), its children, the types it takes as "
       "children and the elements that name it by id.",
       schema({{"path", prop("string", "Element path from editor_state, e.g. root/children/2.")}}, {"path"}), true,
       editor_element},
      {"editor_table", "Editor table",
       "A data table's rows as CSV text (header first), from 'offset' (row, from 0) for 'limit' rows (default 200).",
       schema(
           {{"table", prop("string", "Table name; omit or \"\" for the single unnamed table.")},
            {"offset", prop("integer", "First row, from 0.")},
            {"limit", prop("integer", "Most rows to return.")}}),
       true, editor_table},
      {"editor_view_image", "Editor view image",
       "A PNG of the figure as the user sees it in the editor (zoom, pan, selection outline).",
       schema({{"max_width", prop("integer", "Largest width in pixels (default 1200).")}}), true, editor_view_image},
      {"editor_select", "Editor select",
       "Selects an element (and optionally one of its fields) in the editor, and/or shows a tab: 'chart' or a "
       "table name.",
       schema(
           {{"path", prop("string", "Element path.")},
            {"field", prop("string", "A field of it to focus.")},
            {"tab", prop("string", "'chart', or a table's name (\"\" for the single unnamed table).")}}),
       false, editor_select},
      {"editor_set_fields", "Editor set fields",
       "Sets fields of one element as one undo step: values are strings, numbers, booleans or lists; a string "
       "starting with $ is a column reference; null clears a field back to its default. The editor checks the "
       "result like any source and refuses (changing nothing) with the loader's message.",
       schema(
           {{"path", prop("string", "Element path, or 'image' for the image options.")},
            {"fields", prop("object", "Field name to value.")},
            {"expected_version", prop("integer", kVersion)}},
           {"path", "fields"}),
       false, editor_set_fields},
      {"editor_add_element", "Editor add element",
       "Adds an element of 'type' to 'parent' (only types the parent takes: see 'takes'). It starts valid: a "
       "fresh id, data for a series, and a link's from/to taken from the selected node and the diagram's other "
       "ids; 'fields' override them (e.g. {\"from\": \"cart\", \"to\": \"purchases\"} for a FlowLink). Returns its "
       "path and the paths that moved.",
       schema(
           {{"parent", prop("string", "Path of the parent element.")},
            {"type", prop("string", "Element type, e.g. FlowLink.")},
            {"fields", prop("object", "Field name to value, as editor_set_fields.")},
            {"index", prop("integer", "Place among the parent's children, from 0; default last.")},
            {"expected_version", prop("integer", kVersion)}},
           {"parent", "type"}),
       false, editor_add_element},
      {"editor_remove_element", "Editor remove element",
       "Removes an element (not the root). Refused while other elements name it by id unless 'with_references' "
       "is true, which removes those too. Returns the paths that moved.",
       schema(
           {{"path", prop("string", "Element path.")},
            {"with_references", prop("boolean", "Also remove the elements that name it.")},
            {"expected_version", prop("integer", kVersion)}},
           {"path"}),
       false, editor_remove_element},
      {"editor_move_element", "Editor move element", "Moves an element to another place among its siblings.",
       schema(
           {{"path", prop("string", "Element path.")},
            {"index", prop("integer", "New place, from 0.")},
            {"expected_version", prop("integer", kVersion)}},
           {"path", "index"}),
       false, editor_move_element},
      {"editor_set_cells", "Editor set cells",
       "Sets cells of a data table as one undo step; a row past the end adds rows.",
       schema(
           {{"table", prop("string", "Table name; omit for the single unnamed table.")},
            {"cells",
             {{"type", "array"},
              {"description", "Each {row (from 0), column (name or number from 0), value}."},
              {"items", {{"type", "object"}}}}},
            {"expected_version", prop("integer", kVersion)}},
           {"cells"}),
       false, editor_set_cells},
      {"editor_set_table", "Editor set table",
       "Replaces a data table with CSV text (header first), or with create: true adds it as a new table (a "
       "second table names the first 'first_name' and its references $first_name:column).",
       schema(
           {{"table", prop("string", "Table name; \"\" for the single unnamed table.")},
            {"csv", prop("string", "The table as CSV, header first.")},
            {"create", prop("boolean", "Add the table when it does not exist.")},
            {"first_name", prop("string", "With create: the name the single unnamed table gets (default table1).")},
            {"expected_version", prop("integer", kVersion)}},
           {"csv"}),
       false, editor_set_table},
      {"editor_remove_table", "Editor remove table",
       "Removes a data table (refused while elements take data from it).",
       schema({{"table", prop("string", "Table name.")}, {"expected_version", prop("integer", kVersion)}}), false,
       editor_remove_table},
      {"editor_set_source", "Editor set source",
       "Replaces the whole source (one undo step). Prefer the smaller tools; element paths may change.",
       schema({{"source", prop("string", "The new YAML source.")}, {"expected_version", prop("integer", kVersion)}}, {"source"}),
       false, editor_set_source},
      {"editor_undo", "Editor undo", "Undoes the last change in the editor (the user's or yours).",
       schema(json::object()), false, [](editor_host& h, const json&) { return editor_undo_redo(h, true); }},
      {"editor_redo", "Editor redo", "Redoes the last undone change.", schema(json::object()), false,
       [](editor_host& h, const json&) { return editor_undo_redo(h, false); }},
  };
}

} // namespace

json editor_tool_definitions() {
  json out = json::array();
  for (const definition& d : definitions()) {
    out.push_back(
        {{"name", d.name},
         {"title", d.title},
         {"description", d.description},
         {"inputSchema", d.input},
         {"annotations", annotations(d.title, d.read_only)}});
  }
  return out;
}

void add_editor_tools(tool_registry& registry, editor_runner run) {
  for (const definition& d : definitions()) {
    auto handler = d.handler;
    registry.add(
        {{{"name", d.name},
          {"title", d.title},
          {"description", d.description},
          {"inputSchema", d.input},
          {"annotations", annotations(d.title, d.read_only)}},
         [run, handler](const json& args) { return run([handler, args](editor_host& host) { return handler(host, args); }); }});
  }
}

std::string editor_instructions() {
  return "This is the nymph editor the user has open (nymph view --edit). Its tools read and change the chart "
         "on screen as the user would: every change is checked like any nymph source, can be undone, and shows "
         "at once. Start with editor_state: it lists the elements by path, the selection (what 'this' means), "
         "the data tables and the view. Use editor_add_element / editor_set_fields / editor_remove_element for "
         "elements, editor_set_cells / editor_set_table for data, editor_view_image to see the result. Pass "
         "expected_version (from editor_state) so a change the user made meanwhile is not overwritten.";
}

} // namespace bdg::nymph::mcp
