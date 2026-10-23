// MIT License © 2026 Binary Dice Games
/// @file mermaid.cpp
/// @brief import_mermaid(): read, convert, size, write and check
///        (spec/16_mermaid_import/DESIGN.md sections 3 and 7).
#include <algorithm>
#include <cmath>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "mermaid/mermaid_internal.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph {

namespace {

using namespace mermaid;

/// The smallest and largest image side an import writes.
constexpr int kMinSide = 320;
constexpr int kMaxSide = 4096;
/// Pixels added around the natural size (nymph's default padding is 8 on
/// each side; a little more keeps edges off the border).
constexpr int kMargin = 24;

std::string kind_name(const std::string& kind) {
  if (kind == "flowchart" || kind == "flowchart-elk" || kind == "graph") {
    return "Flowchart";
  }
  if (kind == "sequenceDiagram") {
    return "Sequence diagram";
  }
  if (kind == "classDiagram" || kind == "classDiagram-v2") {
    return "Class diagram";
  }
  if (kind == "stateDiagram" || kind == "stateDiagram-v2") {
    return "State diagram";
  }
  if (kind == "erDiagram") {
    return "Entity-relationship diagram";
  }
  if (kind == "gantt") {
    return "Gantt chart";
  }
  if (kind == "pie") {
    return "Pie chart";
  }
  if (kind == "mindmap") {
    return "Mind map";
  }
  if (kind == "sankey-beta" || kind == "sankey") {
    return "Sankey diagram";
  }
  return "Chart";
}

document convert(const source_text& src, context& ctx) {
  const std::string& k = src.kind;
  if (k == "pie") {
    return convert_pie(src, ctx);
  }
  if (k == "flowchart" || k == "flowchart-elk" || k == "graph") {
    return convert_flowchart(src, ctx);
  }
  if (k == "sequenceDiagram") {
    return convert_sequence(src, ctx);
  }
  if (k == "classDiagram" || k == "classDiagram-v2") {
    return convert_class(src, ctx);
  }
  if (k == "stateDiagram" || k == "stateDiagram-v2") {
    return convert_state(src, ctx);
  }
  if (k == "erDiagram") {
    return convert_er(src, ctx);
  }
  if (k == "gantt") {
    return convert_gantt(src, ctx);
  }
  if (k == "mindmap") {
    return convert_mindmap(src, ctx);
  }
  if (k == "sankey-beta" || k == "sankey") {
    return convert_sankey(src, ctx);
  }
  if (k == "xychart-beta" || k == "xychart") {
    return convert_xychart(src, ctx);
  }
  ctx.fail(src.header_pos, "mermaid " + k + " diagrams are not supported");
}

/// The description: accTitle and accDescr, else what it is.
std::string description_of(const source_text& src, const document& doc) {
  if (!src.acc_descr.empty()) {
    return src.acc_title.empty() ? src.acc_descr : src.acc_title + ": " + src.acc_descr;
  }
  if (!src.acc_title.empty()) {
    return src.acc_title;
  }
  if (!doc.description.empty()) {
    return doc.description;
  }
  std::string text = kind_name(src.kind) + " converted from mermaid";
  if (!doc.root.title.empty()) {
    text += ": " + doc.root.title;
  }
  return text + ".";
}

/// Runs `step` on the YAML; a nymph::error it throws is re-thrown at the
/// mermaid position of the YAML line it names.
template <typename Step>
void at_mermaid(const std::vector<pos>& map, pos fallback, Step&& step) {
  try {
    step();
  } catch (const error& e) {
    if (e.line() <= 0) {
      throw;
    }
    pos p = static_cast<size_t>(e.line()) <= map.size() ? map[static_cast<size_t>(e.line()) - 1] : fallback;
    if (p.line <= 0) {
      p = fallback;
    }
    throw error(p.line, p.column, std::string(e.what()) + " (nymph YAML line " + std::to_string(e.line()) + ")");
  }
}

int side(float natural) {
  const int s = static_cast<int>(std::ceil(natural)) + kMargin;
  return std::clamp(s, kMinSide, kMaxSide);
}

} // namespace

mermaid_import import_mermaid(std::string_view text) {
  if (text.size() > kMaxSourceBytes) {
    throw error(0, 0, "the mermaid text is larger than " + std::to_string(kMaxSourceBytes / (1024 * 1024)) + " MB");
  }
  context ctx;
  const source_text src = read_source(text, ctx);
  document doc = convert(src, ctx);
  doc.description = description_of(src, doc);

  std::vector<pos> map;
  if (doc.width == 0 || doc.fit_width_only) {
    // Measure the diagram once, without an image size, and write its own.
    const std::string probe = write_yaml(doc, src.kind, map);
    ImVec2 natural(0.0f, 0.0f);
    at_mermaid(map, src.header_pos, [&] { natural = measure_figure(bind_source(probe, builtin_registry())); });
    if (doc.fit_width_only) {
      doc.height = side(natural.y);
    } else {
      doc.width = side(natural.x);
      doc.height = side(natural.y);
      // Too large for the biggest image even at the default floor of 50%:
      // let the diagram shrink further, and say so in its min_zoom.
      const float room = static_cast<float>(kMaxSide - kMargin);
      const float zoom = std::min({1.0f, room / std::max(natural.x, 1.0f), room / std::max(natural.y, 1.0f)});
      if (zoom < 0.5f && doc.root.get("min_zoom") == nullptr) {
        doc.root.number("min_zoom", std::max(0.05, std::floor(zoom * 100.0) / 100.0));
      }
    }
  }
  mermaid_import result;
  result.source = write_yaml(doc, src.kind, map);
  result.kind = src.kind;
  at_mermaid(map, src.header_pos, [&] { check_source(result.source); });
  result.warnings = std::move(ctx.warnings());
  std::stable_sort(result.warnings.begin(), result.warnings.end(), [](const import_warning& a, const import_warning& b) {
    return a.line != b.line ? a.line < b.line : a.column < b.column;
  });
  return result;
}

} // namespace bdg::nymph
