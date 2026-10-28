// MIT License © 2026 Binary Dice Games
/// @file charts.cpp
/// @brief mermaid `sankey-beta` → `SankeyDiagram` and `xychart-beta` →
///        `Plot` (spec/16_mermaid_import/DESIGN.md section 4.7).
#include <algorithm>
#include "edit/data_edit.hpp"
#include "edit/field_value.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

/// One CSV row (RFC 4180 quotes, `""` for a quote); nullopt for an
/// unterminated quote.
std::optional<std::vector<std::string>> csv_row(std::string_view text) {
  std::vector<std::string> cells;
  std::string cell;
  bool quoted = false;
  bool was_quoted = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (quoted) {
      if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
        cell += '"';
        ++i;
      } else if (c == '"') {
        quoted = false;
      } else {
        cell += c;
      }
    } else if (c == '"') {
      quoted = true;
      was_quoted = true;
    } else if (c == ',') {
      cells.push_back(was_quoted ? cell : std::string(trim(cell)));
      cell.clear();
      was_quoted = false;
    } else {
      cell += c;
    }
  }
  if (quoted) {
    return std::nullopt;
  }
  cells.push_back(was_quoted ? cell : std::string(trim(cell)));
  return cells;
}

/// `[a, "b c", 3]` → its items (quotes dropped).
std::vector<std::string> bracket_list(std::string_view text, pos at, context& ctx) {
  text = trim(text);
  if (text.size() < 2 || text.front() != '[' || text.back() != ']') {
    ctx.fail(at, "expected a list in brackets: [a, b, c]");
  }
  std::vector<std::string> items;
  for (const std::string& part : split_outside_quotes(text.substr(1, text.size() - 2), ',')) {
    items.push_back(label_text(part));
  }
  if (items.size() == 1 && items[0].empty()) {
    items.clear();
  }
  return items;
}

/// An optional leading `"title"` or bare word before `[` or a range.
std::string leading_title(std::string_view& text) {
  text = trim(text);
  if (!text.empty() && text.front() == '"') {
    const size_t close = text.find('"', 1);
    if (close != std::string_view::npos) {
      std::string title = label_text(text.substr(0, close + 1));
      text = trim(text.substr(close + 1));
      return title;
    }
  }
  if (!text.empty() && text.front() != '[' && text.find("-->") == std::string_view::npos) {
    std::string title(text);
    text = {};
    return title;
  }
  if (!text.empty() && text.front() != '[') {
    // `title lo --> hi`: a title is the words before the first number.
    const size_t space = text.find(' ');
    if (space != std::string_view::npos && !parse_number(text.substr(0, space))) {
      std::string title(text.substr(0, space));
      text = trim(text.substr(space));
      return title;
    }
  }
  return {};
}

/// `lo --> hi`.
std::pair<double, double> range_of(std::string_view text, pos at, context& ctx) {
  const size_t arrow = text.find("-->");
  const std::optional<double> lo = arrow == std::string_view::npos ? std::nullopt : parse_number(text.substr(0, arrow));
  const std::optional<double> hi = arrow == std::string_view::npos ? std::nullopt : parse_number(text.substr(arrow + 3));
  if (!lo || !hi) {
    ctx.fail(at, "expected a range: min --> max");
  }
  return {*lo, *hi};
}

std::pair<std::string_view, std::string_view> word_and_rest(std::string_view text) {
  text = trim(text);
  const size_t space = text.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return {text, {}};
  }
  return {text.substr(0, space), trim(text.substr(space))};
}

} // namespace

// ── sankey ──────────────────────────────────────────────────────────────────

document convert_sankey(const source_text& src, context& ctx) {
  document doc;
  doc.width = 800;
  doc.height = 500;
  std::string csv = "source, target, value\n";
  doc.data_lines.push_back(src.header_pos);
  size_t rows = 0;
  for (const line& l : src.body) {
    const pos at = l.start();
    const std::optional<std::vector<std::string>> cells = csv_row(trim(l.text));
    if (!cells) {
      ctx.fail(at, "a quote in this row is not closed");
    }
    if (cells->size() != 3) {
      ctx.fail(at, "a sankey row is source,target,value (found " + std::to_string(cells->size()) + " cells)");
    }
    const std::optional<double> value = parse_number((*cells)[2]);
    if (!value) {
      ctx.fail(at, "'" + (*cells)[2] + "' is not a number");
    }
    csv += csv_cell((*cells)[0]) + ", " + csv_cell((*cells)[1]) + ", " + encode_number(*value) + "\n";
    doc.data_lines.push_back(at);
    ++rows;
  }
  if (rows == 0) {
    ctx.fail(src.header_pos, "the sankey diagram has no rows (source,target,value)");
  }
  csv.pop_back();
  doc.data = csv;
  doc.data_at = src.header_pos;
  element root("SankeyDiagram", src.header_pos);
  root.text("title", src.title);
  root.raw("from", "$source").raw("to", "$target").raw("values", "$value");
  doc.root = root;
  return doc;
}

// ── xychart ─────────────────────────────────────────────────────────────────

document convert_xychart(const source_text& src, context& ctx) {
  document doc;
  doc.width = 800;
  doc.height = 450;
  const bool horizontal = trim(src.header_rest) == "horizontal";
  if (!src.header_rest.empty() && !horizontal) {
    ctx.fail(src.header_rest_pos, "expected 'horizontal' or nothing after xychart-beta");
  }
  std::string title = src.title;
  std::string x_title;
  std::string y_title;
  std::vector<std::string> categories;
  std::optional<std::pair<double, double>> x_range;
  std::optional<std::pair<double, double>> y_range;
  struct series {
    bool bar;
    std::string name;
    std::vector<double> values;
    pos at;
  };
  std::vector<series> all;
  for (const line& l : src.body) {
    const pos at = l.start();
    const auto [word, rest] = word_and_rest(l.text);
    std::string_view r = rest;
    if (word == "title") {
      title = label_text(r);
    } else if (word == "x-axis") {
      x_title = leading_title(r);
      if (!r.empty() && r.front() == '[') {
        categories = bracket_list(r, at, ctx);
      } else if (!r.empty()) {
        x_range = range_of(r, at, ctx);
      }
    } else if (word == "y-axis") {
      y_title = leading_title(r);
      if (!r.empty()) {
        y_range = range_of(r, at, ctx);
      }
    } else if (word == "bar" || word == "line") {
      series s{word == "bar", leading_title(r), {}, at};
      for (const std::string& item : bracket_list(r, at, ctx)) {
        const std::optional<double> v = parse_number(item);
        if (!v) {
          ctx.fail(at, "'" + item + "' is not a number");
        }
        s.values.push_back(*v);
      }
      all.push_back(s);
    } else {
      ctx.fail(at, "expected title, x-axis, y-axis, bar or line, found '" + std::string(word) + "'");
    }
  }
  if (all.empty()) {
    ctx.fail(src.header_pos, "the chart has no bar or line series");
  }
  const size_t n = all.front().values.size();
  for (const series& s : all) {
    if (s.values.size() != n) {
      ctx.fail(s.at, "every series needs " + std::to_string(n) + " values, like the first");
    }
  }
  if (!categories.empty() && categories.size() != n) {
    ctx.fail(src.header_pos, "x-axis has " + std::to_string(categories.size()) + " categories for " + std::to_string(n) + " values");
  }
  // Positions: the categories' indexes, or spread over the x range.
  std::vector<double> xs(n);
  for (size_t i = 0; i < n; ++i) {
    xs[i] = x_range && n > 1 ? x_range->first + (x_range->second - x_range->first) * double(i) / double(n - 1)
                             : double(i);
  }
  // Several bar series stand side by side: each gets its own positions.
  const int bar_count = static_cast<int>(std::count_if(all.begin(), all.end(), [](const series& s) { return s.bar; }));
  const double group = 0.67;
  const double width = bar_count > 1 ? group / bar_count : group;
  std::string csv = "x";
  int bars = 0;
  int lines = 0;
  std::vector<std::string> columns;
  std::vector<std::string> positions; ///< the xs column of each series
  for (const series& s : all) {
    columns.push_back(s.bar ? "bar" + std::to_string(++bars) : "line" + std::to_string(++lines));
    positions.push_back(s.bar && bar_count > 1 ? "x" + std::to_string(bars) : "x");
    if (positions.back() != "x") {
      csv += ", " + positions.back();
    }
    csv += ", " + columns.back();
  }
  csv += "\n";
  doc.data_lines.push_back(src.header_pos);
  for (size_t i = 0; i < n; ++i) {
    csv += encode_number(xs[i]);
    int k = 0;
    for (const series& s : all) {
      if (s.bar && bar_count > 1) {
        const double offset = (k++ - (bar_count - 1) / 2.0) * width;
        csv += ", " + encode_number(xs[i] + offset * (x_range && n > 1 ? xs[1] - xs[0] : 1.0));
      }
      csv += ", " + encode_number(s.values[i]);
    }
    csv += "\n";
    doc.data_lines.push_back(all.front().at);
  }
  csv.pop_back();
  doc.data = csv;
  doc.data_at = src.header_pos;

  element plot("Plot", src.header_pos);
  plot.text("title", title);
  // Horizontal: the categories go up the Y axis, the values along X.
  plot.text(horizontal ? "y_label" : "x_label", x_title).text(horizontal ? "x_label" : "y_label", y_title);
  const std::string cat = horizontal ? "y" : "x";
  const std::string val = horizontal ? "x" : "y";
  if (!categories.empty()) {
    std::string ticks = "[";
    for (size_t i = 0; i < n; ++i) {
      ticks += (i == 0 ? "" : ", ") + encode_number(xs[i]);
    }
    plot.raw(cat + "_ticks", ticks + "]").texts(cat + "_tick_labels", categories);
  }
  if (y_range) {
    plot.number(val + "_min", y_range->first).number(val + "_max", y_range->second);
  } else if (bars > 0) {
    double lo = 0.0;
    double hi = 0.0;
    for (const series& s : all) {
      for (double v : s.values) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
      }
    }
    if (hi > lo) {
      plot.number(val + "_min", lo).number(val + "_max", hi + (hi - lo) * 0.05);
    }
  }
  if (std::all_of(all.begin(), all.end(), [](const series& s) { return s.name.empty(); })) {
    plot.name("flags", "NoLegend");
  }
  for (size_t i = 0; i < all.size(); ++i) {
    const series& s = all[i];
    element e(s.bar ? (horizontal ? "PlotBarsH" : "PlotBars") : "PlotLine", s.at);
    e.text("label", s.name.empty() ? columns[i] : s.name);
    if (!horizontal) {
      e.raw("xs", "$" + positions[i]).raw("ys", "$" + columns[i]);
    } else {
      // Horizontal: the values along X, the positions up Y (PlotBarsH too).
      e.raw("xs", "$" + columns[i]).raw("ys", "$" + positions[i]);
    }
    if (s.bar && bar_count > 1) {
      e.number("bar_size", width * (x_range && n > 1 ? xs[1] - xs[0] : 1.0));
    }
    plot.children.push_back(e);
  }
  doc.root = plot;
  return doc;
}

} // namespace bdg::nymph::mermaid
