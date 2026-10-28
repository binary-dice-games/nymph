// MIT License © 2026 Binary Dice Games
/// @file yaml_writer.cpp
/// @brief The element model as nymph YAML, in the style of `examples/`,
///        with a line map back to the mermaid text
///        (spec/16_mermaid_import/DESIGN.md section 3).
#include "edit/field_value.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

// ── element ─────────────────────────────────────────────────────────────────

element& element::text(std::string_view key, std::string_view value) {
  if (key == "title") {
    title = std::string(value);
  }
  if (!value.empty()) {
    set(key, yaml_text(value));
  }
  return *this;
}

element& element::name(std::string_view key, std::string_view value) {
  set(key, std::string(value));
  return *this;
}

element& element::flag(std::string_view key, bool value) {
  set(key, encode_bool(value));
  return *this;
}

element& element::number(std::string_view key, double value) {
  set(key, encode_number(value));
  return *this;
}

element& element::texts(std::string_view key, const std::vector<std::string>& values) {
  if (values.empty()) {
    return *this;
  }
  std::string list = "[";
  for (size_t i = 0; i < values.size(); ++i) {
    list += (i == 0 ? "" : ", ") + yaml_text(values[i]);
  }
  set(key, list + "]");
  return *this;
}

element& element::raw(std::string_view key, std::string yaml) {
  set(key, std::move(yaml));
  return *this;
}

const std::string* element::get(std::string_view key) const {
  for (const field& f : fields) {
    if (f.key == key) {
      return &f.value;
    }
  }
  return nullptr;
}

void element::set(std::string_view key, std::string yaml) {
  for (field& f : fields) {
    if (f.key == key) {
      f.value = std::move(yaml);
      return;
    }
  }
  fields.push_back({std::string(key), std::move(yaml)});
}

// ── Writer ──────────────────────────────────────────────────────────────────

namespace {

struct writer {
  std::string out;
  std::vector<pos>& map;

  void put(const std::string& text, pos p) {
    out += text;
    out += '\n';
    map.push_back(p);
  }

  /// `{type: T, k: v, ...}`
  static std::string flow(const element& e) {
    std::string s = "{type: " + e.type;
    for (const field& f : e.fields) {
      s += ", " + f.key + ": " + f.value;
    }
    return s + "}";
  }

  /// The fields and children of `e` as block lines at `indent`; the first
  /// line (`type:`) is prefixed by `first` (a list item's "- ").
  void block(const element& e, size_t indent, const std::string& first) {
    const std::string pad(indent, ' ');
    put(pad.substr(0, pad.size() - first.size()) + first + "type: " + e.type, e.at);
    for (const field& f : e.fields) {
      put(pad + f.key + ": " + f.value, e.at);
    }
    if (!e.children.empty()) {
      put(pad + "children:", e.at);
      for (const element& c : e.children) {
        item(c, indent + 2);
      }
    }
  }

  void item(const element& e, size_t indent) {
    if (e.children.empty()) {
      put(std::string(indent, ' ') + "- " + flow(e), e.at);
    } else {
      block(e, indent + 2, "- ");
    }
  }
};

} // namespace

std::string write_yaml(const document& doc, const std::string& kind, std::vector<pos>& line_map) {
  line_map.clear();
  writer w{{}, line_map};
  const pos top = doc.root.at;
  w.put("nymph: 1", top);
  w.put("description: " + yaml_text(doc.description), top);
  if (doc.width > 0 && doc.height > 0) {
    w.put("image: {width: " + std::to_string(doc.width) + ", height: " + std::to_string(doc.height) + "}", top);
  }
  w.put("source: " + yaml_text("Converted from a mermaid " + kind + "."), top);
  w.put("root:", top);
  w.block(doc.root, 2, "");
  if (!doc.data.empty()) {
    w.put("data: |", doc.data_at);
    size_t at = 0;
    size_t row = 0;
    while (at < doc.data.size()) {
      size_t end = doc.data.find('\n', at);
      if (end == std::string::npos) {
        end = doc.data.size();
      }
      const pos p = row < doc.data_lines.size() ? doc.data_lines[row] : doc.data_at;
      w.put("  " + doc.data.substr(at, end - at), p);
      at = end + 1;
      ++row;
    }
  }
  return w.out;
}

} // namespace bdg::nymph::mermaid
