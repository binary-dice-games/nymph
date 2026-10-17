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
#include "document/table.hpp"
#include "document/yaml_doc.hpp"
#include "schema/convert.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

// ── Suggestions ──────────────────────────────────────────────────────────────

size_t edit_distance(std::string_view a, std::string_view b) {
  std::vector<size_t> row(b.size() + 1);
  for (size_t j = 0; j <= b.size(); ++j) {
    row[j] = j;
  }
  for (size_t i = 1; i <= a.size(); ++i) {
    size_t diagonal = row[0];
    row[0] = i;
    for (size_t j = 1; j <= b.size(); ++j) {
      const size_t above = row[j];
      row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0u : 1u)});
      diagonal = above;
    }
  }
  return row[b.size()];
}

/// " (did you mean 'x'?)" for the closest candidate within two edits, else "".
template <typename Range>
std::string did_you_mean(std::string_view word, const Range& candidates) {
  std::string best;
  size_t best_distance = 3;
  for (const auto& candidate : candidates) {
    const std::string_view c(candidate);
    const size_t d = edit_distance(word, c);
    if (d < best_distance && d < c.size()) {
      best_distance = d;
      best = std::string(c);
    }
  }
  return best.empty() ? "" : " (did you mean '" + best + "'?)";
}

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
  binder(const registry& reg, const table* data) : reg_(reg), data_(data) {}

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
      set[static_cast<size_t>(index)] = true;
    }
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

  int column(const yaml_node& ref) const {
    const std::string name = ref.text.substr(1);
    if (data_ == nullptr || data_->headers.empty()) {
      fail(ref, "unknown column '" + name + "': the source has no data");
    }
    const int index = data_->find_column(name);
    if (index < 0) {
      std::vector<std::string> names;
      for (const cell& h : data_->headers) {
        names.push_back(h.text);
      }
      fail(
          ref,
          "unknown column '" + name + "'" + did_you_mean(name, names) + " (columns: " + data_->header_list() + ")");
    }
    return index;
  }

  /// Column `index` as whole numbers (values are rounded, as wish does).
  std::vector<int64_t> integer_column(int index) const {
    const std::vector<double> numbers = data_->numbers(index);
    const std::vector<cell>& cells = data_->columns[static_cast<size_t>(index)];
    std::vector<int64_t> out(numbers.size());
    for (size_t r = 0; r < numbers.size(); ++r) {
      const double v = numbers[r];
      if (std::isnan(v)) {
        throw error(
            cells[r].line,
            cells[r].column,
            "column '" + data_->headers[static_cast<size_t>(index)].text + "', row " + std::to_string(r + 1) +
                ": an empty cell where a whole number is needed");
      }
      if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) {
        throw error(
            cells[r].line,
            cells[r].column,
            "column '" + data_->headers[static_cast<size_t>(index)].text + "', row " + std::to_string(r + 1) + ": '" +
                cells[r].text + "' is out of range");
      }
      out[r] = std::llround(v);
    }
    return out;
  }

  value single_reference(const field_desc& f, const yaml_node& ref, const std::string& what) {
    if (!is_array(f.type) && f.type != field_type::text) {
      fail(ref, what + " cannot take a $column reference (only list and text fields can)");
    }
    const int index = column(ref);
    if (f.type == field_type::number_array) {
      return data_->numbers(index);
    }
    if (f.type == field_type::integer_array) {
      return integer_column(index);
    }
    std::string joined; // text: the cells, one per line
    const std::vector<cell>& cells = data_->columns[static_cast<size_t>(index)];
    for (size_t r = 0; r < cells.size(); ++r) {
      joined += (r ? "\n" : "") + cells[r].text;
    }
    return joined;
  }

  /// `[$a, $b, $c]`: the columns interleaved by row, a row-major grid.
  value interleaved_references(const field_desc& f, const yaml_node& list) {
    const size_t rows = data_ != nullptr ? data_->row_count() : 0;
    const size_t width = list.children.size();
    if (f.type == field_type::number_array) {
      std::vector<double> out(rows * width);
      for (size_t c = 0; c < width; ++c) {
        const std::vector<double> col = data_->numbers(column(list.children[c]));
        for (size_t r = 0; r < rows; ++r) {
          out[r * width + c] = col[r];
        }
      }
      return out;
    }
    std::vector<int64_t> out(rows * width);
    for (size_t c = 0; c < width; ++c) {
      const std::vector<int64_t> col = integer_column(column(list.children[c]));
      for (size_t r = 0; r < rows; ++r) {
        out[r * width + c] = col[r];
      }
    }
    return out;
  }

  const registry& reg_;
  const table* data_;
};

// ── Top level ────────────────────────────────────────────────────────────────

constexpr std::string_view kTopKeys[] = {"nymph", "description", "image", "root", "data"};
constexpr std::string_view kImageKeys[] = {"width", "height", "scale", "theme", "padding"};

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
    } else if (key.text == "theme") {
      if (!v.is_scalar() || !is_known_theme(v.text)) {
        fail(v, "image.theme: unknown theme '" + v.text + "' (one of: light, dark, classic)");
      }
      out.theme = v.text;
    } else {
      fail(
          key,
          "image has no option '" + key.text + "'" + did_you_mean(key.text, kImageKeys) +
              " (options: width, height, scale, theme, padding)");
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
    fail(doc, "a nymph source is a mapping with a 'root' key (and optionally description, image, data)");
  }
  for (const yaml_node& key : doc.keys) {
    if (std::find(std::begin(kTopKeys), std::end(kTopKeys), key.text) == std::end(kTopKeys)) {
      fail(
          key,
          "unknown key '" + key.text + "'" + did_you_mean(key.text, kTopKeys) +
              " (keys: nymph, description, image, root, data)");
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
  table data;
  const yaml_node* data_node = doc.find("data");
  if (data_node != nullptr) {
    data = parse_data(*data_node);
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
