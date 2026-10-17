// MIT License © 2026 Binary Dice Games
/// @file uml_test_util.hpp
/// @brief Helpers of the UML tests (spec/03_uml_diagrams): render a source,
///        read back the laid out diagram, find errors, count pixels.
#pragma once

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "imuml_internal.h"
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "layout/layout.hpp"
#include "nymph.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph::uml_test {

using rgba = std::array<uint8_t, 4>;

inline image render(const std::string& source) {
  return render_figure(bind_source(source, builtin_registry()));
}

/// The error check_source() reports for `source`.
inline error source_error(const std::string& source) {
  try {
    check_source(source);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << source;
  return error(0, 0, "");
}

inline bool contains(const error& e, const std::string& part) {
  return std::string(e.what()).find(part) != std::string::npos;
}

/// Expects check_source(`source`) to fail at `line` (and `column` if not 0)
/// with a message containing `part`.
inline void expect_error(const std::string& source, int line, int column, const std::string& part) {
  const error e = source_error(source);
  EXPECT_EQ(e.line(), line) << e.what();
  if (column != 0) {
    EXPECT_EQ(e.column(), column) << e.what();
  }
  EXPECT_TRUE(contains(e, part)) << "message: " << e.what() << "\nexpected: " << part;
}

/// What the drawn diagram looked like: node rectangles (diagram
/// coordinates, zoom 1) by id, and the info of the drawing pass.
struct drawn_diagram {
  std::map<std::string, ImRect> nodes;
  ImUmlDiagramInfo info;
  std::vector<ImRect> activations; ///< Sequence diagrams.
  std::vector<ImUmlSeqEventOut> events;
  std::vector<float> columns; ///< Sequence participants' x.
};

/// Renders `source` and returns its (single) diagram as laid out, looking
/// up the nodes named in `ids`.
inline drawn_diagram draw_and_inspect(const std::string& source, const std::vector<std::string>& ids) {
  const figure f = bind_source(source, builtin_registry());
  drawn_diagram out;
  render_frames(f.image, [&](draw_context& dc) {
    layout_root(f.root, dc);
    dc.draw_node(f.root);
    out.info = ImUml::GetLastDiagramInfo();
    ImUmlContext* ctx = ImUml::GetCurrentContext();
    const ImUmlDiagram* drawn = nullptr;
    for (int i = 0; i < ctx->Diagrams.Size; ++i) {
      if (!(ctx->Diagrams[i]->Flags & ImUmlDiagramFlags_MeasureOnly)) {
        drawn = ctx->Diagrams[i];
      }
    }
    if (drawn == nullptr) {
      return;
    }
    out.nodes.clear();
    for (const std::string& id : ids) {
      const ImGuiID h = ImHashStr(id.c_str());
      for (int i = 0; i < drawn->Nodes.Size; ++i) {
        const ImUmlNodeRec& n = drawn->Nodes[i];
        if (n.Id == h) {
          out.nodes[id] = ImRect(n.Pos - n.Size * 0.5f, n.Pos + n.Size * 0.5f);
        }
      }
    }
    out.activations.clear();
    for (int i = 0; i < drawn->Seq.Activations.Size; ++i) {
      out.activations.push_back(drawn->Seq.Activations[i].Rect);
    }
    out.events.assign(drawn->Seq.Events.Data, drawn->Seq.Events.Data + drawn->Seq.Events.Size);
    out.columns.clear();
    for (int i = 0; i < drawn->Seq.Participants.Size; ++i) {
      out.columns.push_back(drawn->Seq.Participants[i].X);
    }
  });
  return out;
}

inline size_t count_color(const image& img, rgba c, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1) {
  x1 = x1 < 0 ? img.width : x1;
  y1 = y1 < 0 ? img.height : y1;
  size_t n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const uint8_t* p = img.at(x, y);
      n += (rgba{p[0], p[1], p[2], p[3]} == c) ? 1 : 0;
    }
  }
  return n;
}

/// Number of pixels in the region that differ from the top-left pixel (the
/// background).
inline size_t non_background(const image& img, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1) {
  const uint8_t* bg = img.at(0, 0);
  const rgba b{bg[0], bg[1], bg[2], bg[3]};
  x1 = x1 < 0 ? img.width : x1;
  y1 = y1 < 0 ? img.height : y1;
  return static_cast<size_t>((x1 - x0) * (y1 - y0)) - count_color(img, b, x0, y0, x1, y1);
}

/// PNG round trip and determinism of a source.
inline void expect_round_trip(const std::string& source) {
  const std::vector<uint8_t> a = render_png(source);
  const std::vector<uint8_t> b = render_png(source);
  EXPECT_EQ(a, b) << "rendering twice gave different bytes";
  const std::optional<std::string> back = extract_source(a);
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(*back, source);
}

} // namespace bdg::nymph::uml_test
