// MIT License © 2026 Binary Dice Games
/// @file mermaid_internal.hpp
/// @brief Shared pieces of the mermaid converters: the reader's output,
///        the element model the converters fill, text helpers and the
///        converter entry points (spec/16_mermaid_import/DESIGN.md
///        section 3).
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "nymph.hpp"

namespace bdg::nymph::mermaid {

// ── Positions and lines ─────────────────────────────────────────────────────

/// @brief A 1-based line and column (characters) in the mermaid text.
struct pos {
  int line = 0;
  int column = 0;
};

/// @brief One line of the diagram's body, comments removed.
struct line {
  std::string text; ///< The line without its trailing comment or line end.
  int number = 0;   ///< 1-based line in the whole text.

  /// @brief The position of byte `offset` of `text`.
  pos at(size_t offset) const;
  /// @brief The position of the first character that is not a space.
  pos start() const;
};

/// @brief What the reader found: the header, the shared statements and the
///        body lines for the converter.
struct source_text {
  std::string kind;        ///< The header keyword ("flowchart", "pie", ...).
  std::string header_rest; ///< The rest of the header line, trimmed.
  pos header_pos;          ///< Where the keyword is.
  pos header_rest_pos;     ///< Where header_rest starts.
  std::string title;       ///< Front matter `title`, if any.
  pos title_pos;
  std::string acc_title;   ///< `accTitle: ...`
  std::string acc_descr;   ///< `accDescr: ...` or `accDescr { ... }`
  std::vector<line> body;  ///< The lines after the header, without blank ones.
};

// ── Errors and warnings ─────────────────────────────────────────────────────

/// @brief Collects warnings and throws positioned errors.
class context {
 public:
  void warn(pos p, std::string message) {
    warnings_.push_back({p.line, p.column, std::move(message)});
  }
  [[noreturn]] void fail(pos p, const std::string& message) const;
  std::vector<import_warning>& warnings() {
    return warnings_;
  }

 private:
  std::vector<import_warning> warnings_;
};

// ── The element model ───────────────────────────────────────────────────────

/// @brief One field: its key and its value already written as YAML.
struct field {
  std::string key;
  std::string value;
};

/// @brief An element of the nymph source to write.
struct element {
  std::string type;
  std::vector<field> fields;
  std::vector<element> children;
  pos at; ///< The mermaid statement it came from.
  std::string title; ///< The raw text of its `title` field, if any.

  element() = default;
  element(std::string t, pos p) : type(std::move(t)), at(p) {}

  /// @brief Adds a text field (empty texts are skipped).
  element& text(std::string_view key, std::string_view value);
  /// @brief Adds a name (enumeration value) field, written as is.
  element& name(std::string_view key, std::string_view value);
  /// @brief Adds a boolean field.
  element& flag(std::string_view key, bool value);
  /// @brief Adds a number field.
  element& number(std::string_view key, double value);
  /// @brief Adds a list of texts (skipped when empty).
  element& texts(std::string_view key, const std::vector<std::string>& values);
  /// @brief Adds a field written exactly as `yaml`.
  element& raw(std::string_view key, std::string yaml);
  /// @brief The value of a field, or nullptr.
  const std::string* get(std::string_view key) const;
  /// @brief Replaces (or adds) a field written as `yaml`.
  void set(std::string_view key, std::string yaml);
};

/// @brief A whole nymph source to write.
struct document {
  std::string description;
  int width = 0;  ///< 0: sized from the layout (DESIGN.md section 3, sizing)
  int height = 0; ///< 0: as width
  bool fit_width_only = false; ///< size only the height from the layout
  element root;
  std::string data; ///< CSV of the `data` key, or empty; one line per row.
  pos data_at;
  std::vector<pos> data_lines; ///< The mermaid position of each data line (header included), if known.
};

/// @brief The YAML text of `doc`, and for each of its lines the mermaid
///        position it came from.
std::string write_yaml(const document& doc, const std::string& kind, std::vector<pos>& line_map);

// ── Text helpers ────────────────────────────────────────────────────────────

/// @brief `text` without leading and trailing spaces and tabs.
std::string_view trim(std::string_view text);
/// @brief Whether `text` starts with `prefix`.
inline bool starts_with(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}
/// @brief The text as a YAML scalar for a nymph text field: a leading `$`
///        doubled (a literal dollar), quoted where YAML needs it.
std::string yaml_text(std::string_view text);
/// @brief A label as mermaid writes it, decoded: surrounding quotes, markdown
///        string backticks and `**` / `*` / `_` markers dropped, `<br>` as a
///        line break, `#quot;` / `#35;` entity codes.
std::string label_text(std::string_view raw);
/// @brief Whether `c` may be part of a mermaid id (letters, digits, `_`,
///        `-`, `.`, and any non-ASCII byte).
bool is_id_char(char c);
/// @brief A CSS colour (`#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb()`, `rgba()`
///        or a CSS name) as nymph's `"#RRGGBB"`; nullopt if unknown.
std::optional<std::string> css_color(std::string_view value);
/// @brief A whole decimal number (`12`, `-3.5`, `1e3`), or nullopt.
std::optional<double> parse_number(std::string_view text);
/// @brief Splits `text` on `sep`, outside double quotes, trimming parts.
std::vector<std::string> split_outside_quotes(std::string_view text, char sep);
/// @brief "Tag" -> "the kind's name" helper: the closest of `names` to
///        `word`, as " (did you mean 'x'?)", or empty.
std::string suggestion(std::string_view word, const std::vector<std::string>& names);

// ── Styling shared by the graph kinds ───────────────────────────────────────

/// @brief The colours a mermaid style list sets (`fill:#f9f,stroke:#333`).
struct style {
  std::string fill;   ///< nymph "#RRGGBB" or empty
  std::string stroke;
  std::string color;  ///< text colour
};

/// @brief Reads a mermaid style list; unknown or unsupported properties are
///        warnings at `at`.
style parse_style(std::string_view list, pos at, context& ctx);
/// @brief Merges `over` into `base` (non-empty values win).
void merge_style(style& base, const style& over);
/// @brief Writes a style's colours on an element, under the field names it
///        uses: fill_color, `stroke_key` (color or line_color), text_color.
void apply_style(element& e, const style& s, std::string_view stroke_key = "color");

/// @brief `classDef`, `class` / `cssClass` assignments, `:::name` and
///        `style id ...` statements, applied by id at the end.
class style_sheet {
 public:
  /// @brief Reads a styling statement (`word` is its first word); false
  ///        when it is not one. `class_assigns` says whether `class a,b
  ///        name` assigns classes (state diagrams) or declares (class
  ///        diagrams).
  bool read(std::string_view word, std::string_view rest, pos at, bool class_assigns, context& ctx);
  /// @brief Records `id:::name`.
  void assign(const std::string& id, const std::string& cls, pos at);
  /// @brief The style of `id`: `default`, then its classes, then its own.
  style of(const std::string& id, context& ctx) const;
  /// @brief Warns for assignments and styles naming none of `ids`.
  void check_ids(const std::vector<std::string>& ids, context& ctx) const;

 private:
  struct assignment {
    std::string id, cls;
    pos at;
  };
  struct direct {
    std::string id;
    style s;
    pos at;
  };
  std::vector<std::pair<std::string, style>> defs_;
  std::vector<assignment> assigned_;
  std::vector<direct> direct_;
};

// ── Converters ──────────────────────────────────────────────────────────────

document convert_pie(const source_text& src, context& ctx);
document convert_flowchart(const source_text& src, context& ctx);
document convert_sequence(const source_text& src, context& ctx);
document convert_class(const source_text& src, context& ctx);
document convert_state(const source_text& src, context& ctx);
document convert_er(const source_text& src, context& ctx);
document convert_gantt(const source_text& src, context& ctx);
document convert_mindmap(const source_text& src, context& ctx);
document convert_sankey(const source_text& src, context& ctx);
document convert_xychart(const source_text& src, context& ctx);

/// @brief Fails at `at` when `count` passes nymph's `limit` for `what`, so
///        the error names the mermaid line of the first element over it.
void check_limit(size_t count, size_t limit, const std::string& what, pos at, context& ctx);

/// @brief Reads `text` into its header and body (reader.cpp).
/// @throws nymph::error when it is not a mermaid diagram.
source_text read_source(std::string_view text, context& ctx);

} // namespace bdg::nymph::mermaid
