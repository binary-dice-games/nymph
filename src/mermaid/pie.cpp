// MIT License © 2026 Binary Dice Games
/// @file pie.cpp
/// @brief mermaid `pie` → a `Plot` with one `PlotPieChart` over a data
///        table (spec/16_mermaid_import/DESIGN.md section 4.7).
#include "edit/data_edit.hpp"
#include "edit/field_value.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

/// Reads `showData` and `title ...` from the start of `text`.
void header_words(std::string_view text, pos at, std::string& title, pos& title_at, bool& show_data, const line* ln,
                  size_t offset) {
  text = trim(text);
  while (!text.empty()) {
    if (starts_with(text, "showData")) {
      show_data = true;
      text = trim(text.substr(8));
      continue;
    }
    if (starts_with(text, "title")) {
      title = label_text(trim(text.substr(5)));
      title_at = ln != nullptr ? ln->at(offset) : at;
      return;
    }
    return;
  }
}

} // namespace

document convert_pie(const source_text& src, context& ctx) {
  document doc;
  doc.width = 560;
  doc.height = 440;
  std::string title = src.title;
  pos title_at = src.title_pos;
  bool show_data = false;
  header_words(src.header_rest, src.header_rest_pos, title, title_at, show_data, nullptr, 0);
  pos show_at = src.header_rest_pos;

  std::string csv = "label, value\n";
  doc.data_lines.push_back(src.header_pos);
  size_t slices = 0;
  for (const line& l : src.body) {
    const std::string_view t = trim(l.text);
    const size_t first = l.text.find_first_not_of(" \t");
    if (starts_with(t, "title") || starts_with(t, "showData")) {
      if (starts_with(t, "showData")) {
        show_at = l.at(first);
      }
      header_words(t, l.at(first), title, title_at, show_data, &l, first);
      continue;
    }
    if (t.front() != '"' && t.front() != '\'') {
      ctx.fail(l.at(first), "expected a slice: \"label\" : value");
    }
    const char quote = t.front();
    const size_t close = t.find(quote, 1);
    if (close == std::string_view::npos) {
      ctx.fail(l.at(first), "the label's quote is not closed");
    }
    const std::string label = label_text(t.substr(1, close - 1));
    std::string_view rest = trim(t.substr(close + 1));
    if (!starts_with(rest, ":")) {
      ctx.fail(l.at(first + close + 1), "expected ':' and a value after the label");
    }
    rest = trim(rest.substr(1));
    const std::optional<double> value = parse_number(rest);
    const pos value_at = l.at(l.text.find(rest.empty() ? t.substr(close + 1) : rest, first + close + 1));
    if (!value) {
      ctx.fail(value_at, "'" + std::string(rest) + "' is not a number");
    }
    if (*value <= 0.0) {
      ctx.fail(value_at, "a slice's value must be above 0");
    }
    csv += csv_cell(label) + ", " + encode_number(*value) + "\n";
    doc.data_lines.push_back(l.at(first));
    ++slices;
  }
  if (slices == 0) {
    ctx.fail(src.header_pos, "the pie has no slices; add lines such as \"Dogs\" : 42");
  }
  if (show_data) {
    ctx.warn(show_at, "showData is ignored (the legend shows the labels)");
  }
  csv.pop_back();
  doc.data = csv;
  doc.data_at = src.header_pos;

  element plot("Plot", src.header_pos);
  plot.text("title", title);
  plot.name("flags", "Equal")
      .name("legend_location", "East")
      .name("legend_flags", "Outside")
      .name("x_flags", "NoDecorations")
      .name("y_flags", "NoDecorations")
      .number("x_min", 0)
      .number("x_max", 1);
  element pie("PlotPieChart", src.header_pos);
  pie.raw("labels", "$label").raw("values", "$value").flag("normalize", true);
  plot.children.push_back(pie);
  doc.root = plot;
  (void)title_at;
  return doc;
}

} // namespace bdg::nymph::mermaid
