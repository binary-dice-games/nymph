// MIT License © 2026 Binary Dice Games
/// @file bind.cpp
/// @brief bind(): source text -> yaml_node tree -> checked figure.
#include "document/bind.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "core/suggest.hpp"
#include "document/table.hpp"
#include "document/yaml_doc.hpp"
#include "render/text_style.hpp"
#include "schema/convert.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

[[noreturn]] void fail(const yaml_node& at, const std::string& message) {
  throw error(at.line, at.column, message);
}

// ── Column references ────────────────────────────────────────────────────────

/// `$name` (not `$$...`): a column reference.
bool is_reference(const yaml_node& n) {
  return n.is_scalar() && n.text.size() >= 2 && n.text[0] == '$' && n.text[1] != '$';
}

bool is_array(field_type t) {
  return t == field_type::number_array || t == field_type::integer_array;
}

class binder {
 public:
  binder(const registry& reg, const data_set* data) : reg_(reg), data_(data) {}

  /// Binds element `n`. `parent` is null for `root`. `explicitly_set`
  /// receives which fields the source wrote.
  node element(const yaml_node& n, const element_class* parent, std::vector<bool>* explicitly_set = nullptr) {
    if (!n.is_mapping()) {
      fail(n, "an element must be a mapping with a 'type', like {type: Plot}");
    }
    const element_class& cls = element_type(n, parent);
    node out = cls.make_node();
    out.line = n.line;
    out.column = n.column;
    std::vector<bool> set(cls.fields().size(), false);
    for (size_t i = 0; i < n.keys.size(); ++i) {
      const yaml_node& key = n.keys[i];
      const yaml_node& val = n.children[i];
      if (key.text == "type") {
        continue;
      }
      if (key.text == "children") {
        children(cls, key, val, out);
        continue;
      }
      const int index = cls.field_index(key.text);
      if (index < 0) {
        std::vector<std::string> names;
        for (const field_desc& f : cls.fields()) {
          names.push_back(f.name);
        }
        if (!cls.accepts().empty()) {
          names.push_back("children");
        }
        fail(key, std::string(cls.name()) + " has no field '" + key.text + "'" + did_you_mean(key.text, names));
      }
      out.values[static_cast<size_t>(index)] = field_value(cls, cls.fields()[static_cast<size_t>(index)], val);
      out.field_pos[static_cast<size_t>(index)] = source_pos{val.line, val.column};
      set[static_cast<size_t>(index)] = true;
    }
    cls.check(out);
    if (explicitly_set != nullptr) {
      *explicitly_set = std::move(set);
    }
    return out;
  }

 private:
  /// The element's class, checked against where it is placed.
  const element_class& element_type(const yaml_node& n, const element_class* parent) {
    const yaml_node* type = n.find("type");
    if (type == nullptr) {
      fail(n, "this element has no 'type' (for example type: Plot)");
    }
    if (!type->is_scalar() || type->text.empty()) {
      fail(*type, "'type' must be an element type name, like Plot");
    }
    const element_class* cls = reg_.find(type->text);
    if (cls == nullptr) {
      std::vector<std::string> names;
      for (const element_class* c : reg_.classes()) {
        names.emplace_back(c->name());
      }
      fail(
          *type,
          "unknown element type '" + type->text + "'" + did_you_mean(type->text, names) +
              "; types allowed here: " + allowed_here(parent));
    }
    if (parent == nullptr && cls->slot() != kFigureSlot) {
      fail(*type, "'" + type->text + "' cannot be the root; it goes inside: " + goes_in(*cls));
    }
    if (parent != nullptr && !parent->accepts_slot(cls->slot())) {
      fail(
          *type,
          "'" + type->text + "' cannot be placed in " + std::string(parent->name()) +
              "; it goes inside: " + goes_in(*cls));
    }
    return *cls;
  }

  std::string allowed_here(const element_class* parent) const {
    if (parent == nullptr) {
      return reg_.names_in_slot(kFigureSlot);
    }
    std::string out;
    for (const std::string& slot : parent->accepts()) {
      const std::string names = reg_.names_in_slot(slot);
      out += (out.empty() || names.empty() ? "" : ", ") + names;
    }
    return out;
  }

  std::string goes_in(const element_class& cls) const {
    const std::string names = reg_.names_accepting(cls.slot());
    return names.empty() ? "nothing accepts it" : names;
  }

  void children(const element_class& cls, const yaml_node& key, const yaml_node& val, node& out) {
    if (cls.accepts().empty()) {
      fail(key, std::string(cls.name()) + " takes no children");
    }
    if (val.is_sequence() || val.is_mapping()) {
      // A mapping is wish's named-children form; the names are not used.
      for (const yaml_node& child : val.children) {
        out.children.push_back(element(child, &cls));
      }
      return;
    }
    if (val.is_scalar() && val.text.empty() && !val.quoted()) {
      return; // `children:` with nothing after it
    }
    fail(val, "'children' must be a list of elements");
  }

  value field_value(const element_class& cls, const field_desc& f, const yaml_node& v) {
    const std::string what = std::string(cls.name()) + "." + f.name;
    if (is_reference(v)) {
      return single_reference(f, v, what);
    }
    if (v.is_sequence() && is_array(f.type)) {
      size_t references = 0;
      for (const yaml_node& item : v.children) {
        references += is_reference(item) ? 1 : 0;
      }
      if (references > 0) {
        for (const yaml_node& item : v.children) {
          if (!is_reference(item)) {
            fail(item, what + " mixes $column references and literal values; use only one kind");
          }
        }
        return interleaved_references(f, v);
      }
    }
    return convert_value(v, f, what);
  }

  /// A resolved reference: a column of one table.
  struct column_ref {
    const table* t = nullptr;
    int index = -1;
  };

  [[noreturn]] void
  unknown_column(const yaml_node& ref, const std::string& name, const table& t, const std::string& where) const {
    std::vector<std::string> names;
    for (const cell& h : t.headers) {
      names.push_back(h.text);
    }
    fail(
        ref,
        "unknown column '" + name + "'" + where + did_you_mean(name, names) + " (columns: " + t.header_list() + ")");
  }

  [[noreturn]] void unknown_table(const yaml_node& ref, const std::string& name) const {
    std::vector<std::string> names;
    for (const cell& n : data_->names) {
      names.push_back(n.text);
    }
    fail(ref, "unknown table '" + name + "'" + did_you_mean(name, names) + " (tables: " + data_->name_list() + ")");
  }

  /// Resolves `$column`, `$table:column` (spec/01_core_features/DESIGN.md
  /// section 3, column references).
  column_ref column(const yaml_node& ref) const {
    const std::string text = ref.text.substr(1);
    if (data_ == nullptr || data_->tables.empty() || (!data_->named() && data_->tables[0].headers.empty())) {
      fail(ref, "unknown column '" + text + "': the source has no data");
    }
    if (!data_->named()) {
      const table& t = data_->tables[0];
      const int index = t.find_column(text);
      if (index < 0) {
        unknown_column(ref, text, t, "");
      }
      return {&t, index};
    }
    // $table:column, when the text before the first ':' names a table.
    const size_t colon = text.find(':');
    if (colon != std::string::npos) {
      const int ti = data_->find_table(text.substr(0, colon));
      if (ti >= 0) {
        const table& t = data_->tables[static_cast<size_t>(ti)];
        const std::string name = text.substr(colon + 1);
        const int index = t.find_column(name);
        if (index < 0) {
          unknown_column(ref, name, t, " in table '" + text.substr(0, colon) + "'");
        }
        return {&t, index};
      }
    }
    // A bare name: fine with one table, ambiguous with several.
    std::vector<size_t> having;
    for (size_t i = 0; i < data_->tables.size(); ++i) {
      if (data_->tables[i].find_column(text) >= 0) {
        having.push_back(i);
      }
    }
    if (data_->tables.size() == 1 && !having.empty()) {
      return {&data_->tables[0], data_->tables[0].find_column(text)};
    }
    if (!having.empty()) {
      std::string list;
      for (size_t i : having) {
        list += (list.empty() ? "" : ", ") + data_->names[i].text;
      }
      fail(
          ref,
          "'$" + text + "' needs a table, as there are several: write $" + data_->names[having[0]].text + ":" + text +
              " (tables with column '" + text + "': " + list + ")");
    }
    if (colon != std::string::npos) {
      unknown_table(ref, text.substr(0, colon));
    }
    if (data_->tables.size() == 1) {
      unknown_column(ref, text, data_->tables[0], " in table '" + data_->names[0].text + "'");
    }
    fail(
        ref,
        "unknown column '" + text + "'; with several tables write $table:column (tables: " + data_->name_list() + ")");
  }

  /// Column `index` of `t` as whole numbers (values are rounded, as wish
  /// does).
  static std::vector<int64_t> integer_column(const column_ref& c) {
    const table& t = *c.t;
    const std::vector<double> numbers = t.numbers(c.index);
    const std::vector<cell>& cells = t.columns[static_cast<size_t>(c.index)];
    const std::string& header = t.headers[static_cast<size_t>(c.index)].text;
    std::vector<int64_t> out(numbers.size());
    for (size_t r = 0; r < numbers.size(); ++r) {
      const double v = numbers[r];
      if (std::isnan(v)) {
        throw error(
            cells[r].line,
            cells[r].column,
            "column '" + header + "', row " + std::to_string(r + 1) + ": an empty cell where a whole number is needed");
      }
      if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) {
        throw error(
            cells[r].line,
            cells[r].column,
            "column '" + header + "', row " + std::to_string(r + 1) + ": '" + cells[r].text + "' is out of range");
      }
      out[r] = std::llround(v);
    }
    return out;
  }

  value single_reference(const field_desc& f, const yaml_node& ref, const std::string& what) {
    if (!is_array(f.type) && f.type != field_type::text) {
      fail(ref, what + " cannot take a $column reference (only list and text fields can)");
    }
    const column_ref c = column(ref);
    if (f.type == field_type::number_array) {
      return c.t->numbers(c.index);
    }
    if (f.type == field_type::integer_array) {
      return integer_column(c);
    }
    std::string joined; // text: the cells, one per line
    const std::vector<cell>& cells = c.t->columns[static_cast<size_t>(c.index)];
    for (size_t r = 0; r < cells.size(); ++r) {
      joined += (r ? "\n" : "") + cells[r].text;
    }
    return joined;
  }

  /// `[$a, $b, $c]`: the columns interleaved by row, a row-major grid. The
  /// columns may come from different tables of the same length.
  value interleaved_references(const field_desc& f, const yaml_node& list) {
    std::vector<column_ref> cols;
    for (const yaml_node& item : list.children) {
      cols.push_back(column(item));
      if (cols.back().t->row_count() != cols[0].t->row_count()) {
        fail(
            item,
            "'" + item.text + "' has " + std::to_string(cols.back().t->row_count()) + " rows but '" +
                list.children[0].text + "' has " + std::to_string(cols[0].t->row_count()) +
                "; interleaved columns need the same number of rows");
      }
    }
    const size_t rows = cols.empty() ? 0 : cols[0].t->row_count();
    const size_t width = cols.size();
    if (f.type == field_type::number_array) {
      std::vector<double> out(rows * width);
      for (size_t c = 0; c < width; ++c) {
        const std::vector<double> col = cols[c].t->numbers(cols[c].index);
        for (size_t r = 0; r < rows; ++r) {
          out[r * width + c] = col[r];
        }
      }
      return out;
    }
    std::vector<int64_t> out(rows * width);
    for (size_t c = 0; c < width; ++c) {
      const std::vector<int64_t> col = integer_column(cols[c]);
      for (size_t r = 0; r < rows; ++r) {
        out[r * width + c] = col[r];
      }
    }
    return out;
  }

  const registry& reg_;
  const data_set* data_;
};

// ── Top level ────────────────────────────────────────────────────────────────

/// `source` (how the data was made) is accepted as any value and never
/// looked at (spec/01_core_features/DESIGN.md section 3).
constexpr std::string_view kTopKeys[] = {"nymph", "description", "image", "root", "data", "source"};
constexpr std::string_view kImageKeys[] =
    {"width", "height", "scale", "theme", "padding", "font", "font_size", "text_color"};

int image_int(const yaml_node& v, const field_desc& f) {
  return static_cast<int>(std::get<int64_t>(convert_value(v, f, "image." + f.name)));
}

image_options bind_image(const yaml_node& image) {
  if (!image.is_mapping()) {
    fail(image, "'image' must be a mapping, like {width: 800, height: 500}");
  }
  static const field_desc width = int_field("width", 800, "", value_range{16, 4096});
  static const field_desc height = int_field("height", 500, "", value_range{16, 4096});
  static const field_desc scale = int_field("scale", 1, "", value_range{1, 4});
  static const field_desc padding = int_field("padding", 8, "", value_range{0, 2048});
  image_options out;
  for (size_t i = 0; i < image.keys.size(); ++i) {
    const yaml_node& key = image.keys[i];
    const yaml_node& v = image.children[i];
    if (key.text == "width") {
      out.width = image_int(v, width);
    } else if (key.text == "height") {
      out.height = image_int(v, height);
    } else if (key.text == "scale") {
      out.scale = image_int(v, scale);
    } else if (key.text == "padding") {
      out.padding = image_int(v, padding);
    } else if (key.text == "font") {
      static const field_desc font = text_style_fields()[0];
      out.font = std::get<std::string>(convert_value(v, font, "image.font"));
    } else if (key.text == "font_size") {
      static const field_desc size = number_field("font_size", 16.0, "", value_range{1, kMaxFontSize});
      out.font_size = std::get<double>(convert_value(v, size, "image.font_size"));
    } else if (key.text == "text_color") {
      static const field_desc color = color_field("text_color", "");
      out.text_color = std::get<std::string>(convert_value(v, color, "image.text_color"));
    } else if (key.text == "theme") {
      if (!v.is_scalar() || !is_known_theme(v.text)) {
        fail(v, "image.theme: unknown theme '" + v.text + "' (one of: light, dark, classic)");
      }
      out.theme = v.text;
    } else {
      fail(
          key,
          "image has no option '" + key.text + "'" + did_you_mean(key.text, kImageKeys) +
              " (options: width, height, scale, theme, padding, font, font_size, text_color)");
    }
  }
  const yaml_node& at_scale = image.find("scale") ? *image.find("scale") : image;
  if (out.width * out.scale > 8192 || out.height * out.scale > 8192) {
    fail(
        at_scale,
        "image width and height times scale must not exceed 8192 (" + std::to_string(out.width) + "x" +
            std::to_string(out.height) + " at scale " + std::to_string(out.scale) + ")");
  }
  if (out.padding * 2 >= out.width || out.padding * 2 >= out.height) {
    fail(
        image.find("padding") ? *image.find("padding") : image,
        "image.padding leaves no room in a " + std::to_string(out.width) + "x" + std::to_string(out.height) + " image");
  }
  return out;
}

} // namespace

figure bind_source(std::string_view source, const registry& reg) {
  if (source.size() > kMaxSourceBytes) {
    throw error(0, 0, "the source is larger than 16 MiB");
  }
  const yaml_node doc = parse_yaml(source);
  if (!doc.is_mapping()) {
    fail(doc, "a nymph source is a mapping with a 'root' key (and optionally description, image, data, source)");
  }
  for (const yaml_node& key : doc.keys) {
    if (std::find(std::begin(kTopKeys), std::end(kTopKeys), key.text) == std::end(kTopKeys)) {
      fail(
          key,
          "unknown key '" + key.text + "'" + did_you_mean(key.text, kTopKeys) +
              " (keys: nymph, description, image, root, data, source)");
    }
  }

  figure out;
  if (const yaml_node* version = doc.find("nymph")) {
    if (!version->is_scalar() || version->quoted() || version->text != "1") {
      fail(*version, "unsupported format version '" + version->text + "' (this nymph reads version 1)");
    }
  }
  if (const yaml_node* description = doc.find("description")) {
    if (!description->is_scalar()) {
      fail(*description, "'description' must be text");
    }
    out.description = description->text;
  }
  if (const yaml_node* image = doc.find("image")) {
    out.image = bind_image(*image);
  }
  data_set data;
  const yaml_node* data_node = doc.find("data");
  if (data_node != nullptr) {
    data = parse_data_set(*data_node);
  }
  const yaml_node* root = doc.find("root");
  if (root == nullptr) {
    throw error(1, 1, "missing 'root': the element to draw, like root: {type: Plot, children: [...]}");
  }
  binder b(reg, data_node != nullptr ? &data : nullptr);
  std::vector<bool> set;
  out.root = b.element(*root, nullptr, &set);
  out.root.cls->adjust_root(out.root, set);
  return out;
}

} // namespace bdg::nymph
