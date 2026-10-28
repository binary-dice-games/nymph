// MIT License © 2026 Binary Dice Games
/// @file bind.cpp
/// @brief bind(): source text -> yaml_node tree -> checked figure.
#include "document/bind.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <optional>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "core/suggest.hpp"
#include "document/table.hpp"
#include "document/view.hpp"
#include "document/yaml_doc.hpp"
#include "render/text_style.hpp"
#include "schema/convert.hpp"
#include "schema/element_class.hpp"
#include "themes/theme_catalog.hpp"

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

/// Fields inherited by everything inside the element that writes them (and
/// from the image): a bare theme rule for one of them is the image's.
bool is_text_style_field(std::string_view name) {
  return name == "font" || name == "font_size" || name == "text_color";
}

class binder {
 public:
  binder(const registry& reg, const data_set* data) : reg_(reg), data_(data) {}

  /// The theme for the fields the source leaves out; `at` is where the
  /// source names it, for errors in its values.
  void set_theme(const chart_theme* theme, source_pos at) {
    theme_ = theme;
    theme_at_ = at;
  }

  /// A field the image writes, which a theme then leaves to it everywhere.
  void written_above(const std::string& field) {
    ++written_above_[field];
  }

  /// Binds element `n`. `parent` is null for `root`. `explicitly_set`
  /// receives which fields the source wrote.
  node element(const yaml_node& n, const element_class* parent, std::vector<bool>* explicitly_set = nullptr) {
    if (!n.is_mapping()) {
      fail(n, "an element must be a mapping with a 'type', like {type: Plot}");
    }
    const element_class& cls = element_type(n, parent);
    node out = cls.make_node();
    out.placed_as = placement(cls, parent);
    out.line = n.line;
    out.column = n.column;
    std::vector<bool> set(cls.fields().size(), false);
    // What this element writes passes down: a theme leaves those fields of
    // its descendants to inherit it.
    std::vector<std::string> writes;
    for (const yaml_node& key : n.keys) {
      if (key.text != "type" && key.text != "children" && cls.field_index(key.text) >= 0) {
        writes.push_back(key.text);
        ++written_above_[key.text];
      }
    }
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
    apply_theme(cls, set, out);
    for (const std::string& w : writes) {
      if (--written_above_[w] == 0) {
        written_above_.erase(w);
      }
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
    if (parent == nullptr && !cls->fills_slot(kFigureSlot)) {
      fail(*type, "'" + type->text + "' cannot be the root; it goes inside: " + goes_in(*cls));
    }
    if (parent != nullptr && parent->placement_of(*cls) < 0) {
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
    return reg_.names_taken_by(*parent);
  }

  std::string goes_in(const element_class& cls) const {
    const std::string names = reg_.names_accepting(cls);
    return names.empty() ? "nothing accepts it" : names;
  }

  /// The index of the slot `cls` takes under `parent` (the root is a figure).
  static unsigned placement(const element_class& cls, const element_class* parent) {
    if (parent != nullptr) {
      return static_cast<unsigned>(parent->placement_of(cls));
    }
    const auto slots = cls.slots();
    return static_cast<unsigned>(std::find(slots.begin(), slots.end(), kFigureSlot) - slots.begin());
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
    if (v.is_sequence() && (is_array(f.type) || f.type == field_type::text_array)) {
      size_t references = 0;
      for (const yaml_node& item : v.children) {
        references += is_reference(item) ? 1 : 0;
      }
      if (references > 0 && f.type == field_type::text_array) {
        fail(
            v,
            what + " takes one $column reference for the whole list (" + what +
                ": $col), not references inside it; write $$ for a literal '$'");
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
    if (!is_array(f.type) && f.type != field_type::text && f.type != field_type::text_array) {
      fail(ref, what + " cannot take a $column reference (only list and text fields can)");
    }
    const column_ref c = column(ref);
    if (f.type == field_type::number_array) {
      return c.t->numbers(c.index);
    }
    if (f.type == field_type::integer_array) {
      return integer_column(c);
    }
    const std::vector<cell>& column_cells = c.t->columns[static_cast<size_t>(c.index)];
    if (f.type == field_type::text_array) {
      std::vector<std::string> texts;
      texts.reserve(column_cells.size());
      for (const cell& cl : column_cells) {
        texts.push_back(cl.text);
      }
      return texts;
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

  /// Gives every field the element leaves out the theme's value, unless
  /// an enclosing element (or the image) writes that field.
  void apply_theme(const element_class& cls, const std::vector<bool>& set, node& out) const {
    if (theme_ == nullptr) {
      return;
    }
    for (size_t i = 0; i < set.size(); ++i) {
      const field_desc& f = cls.fields()[i];
      if (set[i] || written_above_.contains(f.name)) {
        continue;
      }
      const theme_rule* rule = theme_->rule_for(cls, f.name, !is_text_style_field(f.name));
      if (rule == nullptr) {
        continue;
      }
      try {
        out.values[i] = convert_value(rule->value, f, std::string(cls.name()) + "." + f.name);
      } catch (const error& e) {
        throw error(
            theme_at_.line,
            theme_at_.column,
            "theme '" + theme_->id + "', " + rule->key + " (its line " + std::to_string(rule->value.line) +
                "): " + e.what());
      }
    }
  }

  const registry& reg_;
  const data_set* data_;
  const chart_theme* theme_ = nullptr;
  source_pos theme_at_;
  std::map<std::string, int, std::less<>> written_above_;
};

// ── Top level ────────────────────────────────────────────────────────────────

/// `source` (how the data was made) is accepted as any value and never
/// looked at (spec/01_core_features/DESIGN.md section 3).
constexpr std::string_view kTopKeys[] = {"nymph", "description", "image", "root", "data", "source", "view"};
constexpr std::string_view kImageKeys[] =
    {"width", "height", "scale", "theme", "padding", "font", "font_size", "text_color", "background", "palette"};

int image_int(const yaml_node& v, const field_desc& f) {
  return static_cast<int>(std::get<int64_t>(convert_value(v, f, "image." + f.name)));
}

/// Sets image option `key` (not `theme`) from `v`; `what` prefixes messages.
void bind_image_option(image_options& out, const yaml_node& key, const yaml_node& v) {
  static const field_desc width = int_field("width", 800, "", value_range{16, 4096});
  static const field_desc height = int_field("height", 500, "", value_range{16, 4096});
  static const field_desc scale = int_field("scale", 1, "", value_range{1, 4});
  static const field_desc padding = int_field("padding", 8, "", value_range{0, 2048});
  static const field_desc color = color_field("color", "");
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
    out.text_color = std::get<std::string>(convert_value(v, color, "image.text_color"));
  } else if (key.text == "background") {
    out.background = std::get<std::string>(convert_value(v, color, "image.background"));
  } else if (key.text == "palette") {
    if (!v.is_sequence()) {
      fail(v, "image.palette must be a list of colours, like [\"#4c72b0\", \"#dd8452\"]");
    }
    out.palette.clear();
    for (const yaml_node& c : v.children) {
      std::string text = std::get<std::string>(convert_value(c, color, "image.palette"));
      if (text.empty()) {
        fail(c, "image.palette: an empty colour; give every series colour as \"#RRGGBB\"");
      }
      out.palette.push_back(std::move(text));
    }
  } else {
    fail(
        key,
        "image has no option '" + key.text + "'" + did_you_mean(key.text, kImageKeys) +
            " (options: width, height, scale, theme, padding, font, font_size, text_color, background, palette)");
  }
}

/// The source's `image` options; `written` receives the keys it writes.
image_options bind_image(const yaml_node& image, std::vector<std::string>& written) {
  if (!image.is_mapping()) {
    fail(image, "'image' must be a mapping, like {width: 800, height: 500}");
  }
  image_options out;
  for (size_t i = 0; i < image.keys.size(); ++i) {
    const yaml_node& key = image.keys[i];
    const yaml_node& v = image.children[i];
    written.push_back(key.text);
    if (key.text == "theme") {
      if (!v.is_scalar() || !is_known_theme(v.text)) {
        std::vector<std::string> names;
        for (const chart_theme& t : chart_themes()) {
          names.push_back(t.id);
        }
        fail(v, "image.theme: unknown theme '" + v.text + "'" + did_you_mean(v.text, names) + " (themes: " +
                chart_theme_list() + ")");
      }
      out.theme = v.text;
    } else {
      bind_image_option(out, key, v);
    }
  }
  return out;
}

/// The size checks of the image options, once the theme's are in.
void check_image(const image_options& out, const yaml_node& image) {
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
}

} // namespace

data_replacement replace_data(std::string_view source, std::string_view data_yaml) {
  return replace_top_level(source, "data", data_yaml);
}

data_replacement replace_top_level(std::string_view source, std::string_view key, std::string_view value_yaml) {
  const yaml_node doc = parse_yaml(source);
  if (!doc.is_mapping() || doc.keys.empty() || doc.keys.front().column != 1) {
    fail(doc, "replacing '" + std::string(key) + "' needs a source whose top level is 'key: value' lines");
  }
  // The source's lines, without their line breaks.
  std::vector<std::string_view> lines;
  for (size_t at = 0; at <= source.size();) {
    const size_t nl = source.find('\n', at);
    const size_t end = nl == std::string_view::npos ? source.size() : nl;
    lines.push_back(source.substr(at, end - at));
    if (nl == std::string_view::npos) {
      break;
    }
    at = nl + 1;
  }
  if (!lines.empty() && lines.back().empty()) {
    lines.pop_back(); // the text after the last line break
  }
  // The entry spans its key's line up to the next top-level key.
  size_t from = lines.size(), to = lines.size();
  for (size_t i = 0; i < doc.keys.size(); ++i) {
    if (doc.keys[i].text == key) {
      from = static_cast<size_t>(doc.keys[i].line - 1);
      to = i + 1 < doc.keys.size() ? static_cast<size_t>(doc.keys[i + 1].line - 1) : lines.size();
    }
  }
  std::string out;
  for (size_t i = 0; i < from; ++i) {
    out.append(lines[i]).append("\n");
  }
  data_replacement r;
  if (value_yaml.empty()) { // remove the key
    for (size_t i = to; i < lines.size(); ++i) {
      out.append(lines[i]).append("\n");
    }
    r.source = std::move(out);
    return r;
  }
  out.append(key).append(":\n");
  r.first_line = static_cast<int>(from) + 2;
  std::string_view rest = value_yaml;
  while (!rest.empty()) {
    const size_t nl = rest.find('\n');
    const std::string_view line = rest.substr(0, nl);
    out.append(line.empty() ? "" : "  ").append(line).append("\n");
    ++r.line_count;
    rest = nl == std::string_view::npos ? std::string_view() : rest.substr(nl + 1);
  }
  for (size_t i = to; i < lines.size(); ++i) {
    out.append(lines[i]).append("\n");
  }
  r.source = std::move(out);
  return r;
}

figure bind_source(std::string_view source, const registry& reg, const bind_options& options) {
  if (!options.theme.empty() && !is_known_theme(options.theme)) {
    throw std::invalid_argument("unknown theme '" + options.theme + "' (themes: " + chart_theme_list() + ")");
  }
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
              " (keys: nymph, description, image, root, data, source, view)");
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
  std::vector<std::string> image_written;
  source_pos theme_at;
  const yaml_node empty_image = parse_yaml("{}");
  const yaml_node* image = doc.find("image");
  if (image != nullptr) {
    out.image = bind_image(*image, image_written);
    for (size_t i = 0; i < image->keys.size(); ++i) {
      if (image->keys[i].text == "font") {
        out.image_font_pos = {image->children[i].line, image->children[i].column};
      }
      if (image->keys[i].text == "theme") {
        theme_at = {image->children[i].line, image->children[i].column};
      }
    }
  }
  if (!options.theme.empty()) {
    out.image.theme = options.theme;
  }
  if (options.custom_theme != nullptr) {
    out.image.theme = options.custom_theme->id;
  }
  const chart_theme* theme =
      options.custom_theme != nullptr ? options.custom_theme : find_chart_theme(out.image.theme);
  out.image.base = theme->base;
  // The theme's image options, for those the source leaves out.
  for (const auto& [key, v] : theme->image) {
    if (std::find(image_written.begin(), image_written.end(), key) == image_written.end()) {
      yaml_node name;
      name.text = key;
      bind_image_option(out.image, name, v);
    }
  }
  check_image(out.image, image != nullptr ? *image : empty_image);
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
  b.set_theme(theme, theme_at);
  for (const std::string& key : image_written) {
    if (is_text_style_field(key)) {
      b.written_above(key);
    }
  }
  std::vector<bool> set;
  out.root = b.element(*root, nullptr, &set);
  out.root.cls->adjust_root(out.root, set);
  if (const yaml_node* view = doc.find("view")) {
    out.views = bind_view(*view, element_paths(out.root));
  }
  return out;
}

} // namespace bdg::nymph
