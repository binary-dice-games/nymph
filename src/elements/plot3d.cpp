// MIT License © 2026 Binary Dice Games
/// @file plot3d.cpp
/// @brief `Plot3D` and the 3D series.
///
/// Schema from wish's `src/ui/plot3d_elements/`; drawing from wish's
/// `src/imgui/imgui_plot3d_renderer.cpp`. As for the 2D elements, rules
/// between fields that wish handled by silently drawing less are errors
/// here (check()), and `Plot3DMesh.indices` are bounds-checked: wish passed
/// them to ImPlot3D unchecked, so an out-of-range index read past the
/// vertex arrays.
#include "elements/plot3d.hpp"
#include <implot3d.h>
#include <implot3d_internal.h> // the current plot's rotation, ranges and items
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "elements/plot_style.hpp"
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

using doubles = std::vector<double>;

const name_table& plot3d_flag_names() {
  static const name_table t{
      {"None", 0},
      {"NoTitle", 1 << 0},
      {"NoLegend", 1 << 1},
      {"NoMouseText", 1 << 2},
      {"NoClip", 1 << 3},
      {"NoMenus", 1 << 4},
      {"Equal", 1 << 5},
      {"NoRotate", 1 << 6},
      {"NoPan", 1 << 7},
      {"NoZoom", 1 << 8},
      {"NoInputs", 1 << 9},
      {"CanvasOnly", (1 << 0) | (1 << 1) | (1 << 2)},
  };
  return t;
}

const name_table& plot3d_axis_flag_names() {
  static const name_table t{
      {"None", 0},
      {"NoLabel", 1 << 0},
      {"NoGridLines", 1 << 1},
      {"NoTickMarks", 1 << 2},
      {"NoTickLabels", 1 << 3},
      {"LockMin", 1 << 4},
      {"LockMax", 1 << 5},
      {"AutoFit", 1 << 6},
      {"Invert", 1 << 7},
      {"PanStretch", 1 << 8},
      {"Lock", (1 << 4) | (1 << 5)},
      {"NoDecorations", (1 << 0) | (1 << 1) | (1 << 3)},
  };
  return t;
}

const char* str(const node& n, size_t i) {
  return n.get<std::string>(i).c_str();
}

/// The ImPlot3DSpec of `n`, a series taking every style field.
ImPlot3DSpec item_spec(const node& n) {
  ImPlot3DSpec spec;
  apply_item_style(spec, n, style::all);
  return spec;
}

// ── Plot3D ───────────────────────────────────────────────────────────────────

namespace plot3d_field {
constexpr size_t title = 0, x_label = 1, y_label = 2, z_label = 3, width = 4, height = 5, flags = 6, x_flags = 7,
                 y_flags = 8, z_flags = 9, colormap = 10;
}

class plot3d_class : public element_class {
 public:
  plot3d_class()
      : element_class(
            "Plot3D",
            std::string(kFigureSlot),
            {kPlot3dItemSlot},
            with_text_style({
                text_field("title", "##plot3d", "Plot title shown at the top; prefix ## to hide the title bar."),
                text_field("x_label", "", "X axis label; empty string = no label."),
                text_field("y_label", "", "Y axis label; empty string = no label."),
                text_field("z_label", "", "Z axis label; empty string = no label."),
                number_field(
                    "width",
                    -1.0,
                    "Plot width in pixels; -1 (default) fills the available width. In a HorizontalLayout a negative "
                    "value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural width.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    400.0,
                    "Plot height in pixels. A Plot3D used as root with no height fills the image. In a VerticalLayout "
                    "a negative value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural "
                    "height (400).",
                    value_range{-1000, 8192}),
                flags_field(
                    "flags",
                    0,
                    plot3d_flag_names(),
                    "ImPlot3DFlags bitmask (combine names with '|'; see implot3d.h)."),
                flags_field(
                    "x_flags",
                    0,
                    plot3d_axis_flag_names(),
                    "ImPlot3DAxisFlags for the X axis (combine names with '|')."),
                flags_field(
                    "y_flags",
                    0,
                    plot3d_axis_flag_names(),
                    "ImPlot3DAxisFlags for the Y axis (combine names with '|')."),
                flags_field(
                    "z_flags",
                    0,
                    plot3d_axis_flag_names(),
                    "ImPlot3DAxisFlags for the Z axis (combine names with '|')."),
                colormap_field(),
            }),
            "A 3D plot (ImPlot3D). Children are 3D series: Plot3DLine, Plot3DSurface, ...") {}

  void adjust_root(node& n, const std::vector<bool>& set) const override {
    if (!set[plot3d_field::height]) {
      n.values[plot3d_field::height] = -1.0; // fill the image (wish nymph's rule)
    }
  }

  /// Natural size (a 0 hint): wish's default height, 400 wide.
  ImVec2 measure(const node&, draw_context&, float) const override {
    return ImVec2(400.0f, 400.0f);
  }

  view_kind viewable() const override {
    return view_kind::plot3d;
  }

  void draw(const node& n, draw_context& dc) const override {
    using namespace plot3d_field;
    const rect r = dc.rect_of(n);
    // The layout engine already applied width / height: draw at the rectangle.
    const ImVec2 size(r.width(), r.height());
    ImGui::SetCursorScreenPos(r.min);

    const int64_t colormap_value = n.get<int64_t>(colormap);
    if (colormap_value != kColormapDefault) {
      ImPlot3D::PushColormap(static_cast<ImPlot3DColormap>(colormap_value));
    }
    if ((n.get<int64_t>(flags) & ImPlot3DFlags_NoInputs) == 0) {
      dc.add_input_area(dc.rect_of(n));
    }
    // In a live view the host's own menu takes the right click.
    const ImPlot3DFlags live_flags = dc.interactive() ? ImPlot3DFlags_NoMenus : 0;
    if (ImPlot3D::BeginPlot(str(n, title), size, static_cast<ImPlot3DFlags>(n.get<int64_t>(flags)) | live_flags)) {
      const std::string& xl = n.get<std::string>(x_label);
      const std::string& yl = n.get<std::string>(y_label);
      const std::string& zl = n.get<std::string>(z_label);
      const auto xf = static_cast<ImPlot3DAxisFlags>(n.get<int64_t>(x_flags));
      const auto yf = static_cast<ImPlot3DAxisFlags>(n.get<int64_t>(y_flags));
      const auto zf = static_cast<ImPlot3DAxisFlags>(n.get<int64_t>(z_flags));
      if (!xl.empty() || !yl.empty() || !zl.empty() || xf != 0 || yf != 0 || zf != 0) {
        ImPlot3D::SetupAxes(
            xl.empty() ? nullptr : xl.c_str(),
            yl.empty() ? nullptr : yl.c_str(),
            zl.empty() ? nullptr : zl.c_str(),
            xf,
            yf,
            zf);
      }
      apply_view(n, dc);
      draw_series(n, dc);
      record_view(n, dc);
      if (dc.edit_mode()) {
        record_title(n, dc);
      }
      ImPlot3D::EndPlot();
    }
    if (colormap_value != kColormapDefault) {
      ImPlot3D::PopColormap();
    }
  }

 private:
  /// Records the title as an edit target: centred at the top of the plot.
  static void record_title(const node& n, draw_context& dc) {
    const std::string& t = n.get<std::string>(plot3d_field::title);
    if (t.empty() || t.compare(0, 2, "##") == 0) {
      return;
    }
    const ImPlot3DPlot& plot = *ImPlot3D::GetCurrentPlot();
    const ImVec2 size = ImGui::CalcTextSize(t.c_str(), ImGui::FindRenderedTextEnd(t.c_str()), true);
    const float x = plot.PlotRect.GetCenter().x - size.x * 0.5f;
    dc.record_target(n, "title", {ImVec2(x, plot.CanvasRect.Min.y), ImVec2(x + size.x, plot.CanvasRect.Min.y + size.y)}, t);
  }

  /// The saved view (the `view` key): axis ranges and rotation.
  static void apply_view(const node& n, draw_context& dc) {
    const element_view* v = dc.view_of(n);
    if (v == nullptr) {
      return;
    }
    const ImPlot3DCond cond = dc.limits_cond();
    const std::optional<view_range>* ranges[] = {&v->x, &v->y, &v->z};
    for (int a = 0; a < 3; ++a) {
      if (*ranges[a]) {
        ImPlot3D::SetupAxisLimits(static_cast<ImAxis3D>(a), (*ranges[a])->min, (*ranges[a])->max, cond);
      }
    }
    if (v->rotation) {
      const auto& q = *v->rotation;
      ImPlot3D::SetupBoxRotation(ImPlot3DQuat(q[0], q[1], q[2], q[3]), false, cond);
    }
  }

  /// The series, as draw_context::draw_children() draws them, each with its
  /// legend entry hidden as the saved view says, and noted when hidden.
  static void draw_series(const node& n, draw_context& dc) {
    dc.set_current_plot(&n);
    const element_view* v = dc.view_of(n);
    for (size_t i = 0; i < n.children.size(); ++i) {
      const node& c = n.children[i];
      ImGui::PushID(static_cast<int>(i));
      const int field = c.cls->field_index("label");
      const char* label = field < 0 ? nullptr : c.get<std::string>(static_cast<size_t>(field)).c_str();
      dc.draw_node(c);
      // Hidden after plotting, as sync_hidden() does for 2D plots.
      ImPlot3DItemGroup& items = *ImPlot3D::GetCurrentContext()->CurrentItems;
      if (ImPlot3DItem* item = label == nullptr ? nullptr : items.GetItem(label)) {
        if (dc.take_rehidden(item->ID)) {
          item->Show = false;
        }
        if (v != nullptr && dc.apply_view() &&
            std::find(v->hidden.begin(), v->hidden.end(), label) != v->hidden.end()) {
          item->Show = false;
        }
        if (!item->Show) {
          dc.record_hidden(label);
        }
      }
      ImGui::PopID();
    }
  }

  /// In a live view, what the user has done to the plot.
  static void record_view(const node& n, draw_context& dc) {
    if (!dc.interactive()) {
      dc.set_current_plot(nullptr);
      return;
    }
    const ImPlot3DPlot& plot = *ImPlot3D::GetCurrentPlot();
    plot_view_record r;
    r.element = &n;
    r.kind = view_kind::plot3d;
    r.area = dc.rect_of(n);
    r.x_min = plot.Axes[0].Range.Min;
    r.x_max = plot.Axes[0].Range.Max;
    r.y_min = plot.Axes[1].Range.Min;
    r.y_max = plot.Axes[1].Range.Max;
    r.z_min = plot.Axes[2].Range.Min;
    r.z_max = plot.Axes[2].Range.Max;
    r.rotation = {plot.Rotation.x, plot.Rotation.y, plot.Rotation.z, plot.Rotation.w};
    dc.record_view(std::move(r));
    dc.set_current_plot(nullptr);
  }
};

// ── Series ───────────────────────────────────────────────────────────────────

constexpr size_t kXs = first_own(style::all);
constexpr size_t kYs = kXs + 1;
constexpr size_t kZs = kXs + 2;

class series3d_class : public element_class {
 public:
  /// A 3D series taking the shared fields in `style`.
  series3d_class(std::string name, std::vector<field_desc> own, std::string description, series_style style = style::all)
      : element_class(
            std::move(name),
            kPlot3dItemSlot,
            {},
            series_fields(style, std::move(own)),
            std::move(description) + " Must be a child of a Plot3D element.") {}

 protected:
  /// xs, ys and zs are non-empty and equally long; returns the length.
  size_t check_xyz(const node& n) const {
    const size_t count = n.get<doubles>(kXs).size();
    if (count == 0) {
      fail_at(n, kXs, "is empty: the series has no data");
    }
    for (size_t f : {kYs, kZs}) {
      const size_t other = n.get<doubles>(f).size();
      if (other != count) {
        fail_at(n, f, "has " + std::to_string(other) + " values but xs has " + std::to_string(count));
      }
    }
    return count;
  }

  /// The xs length is a multiple of `group` (one shape per group).
  void check_groups(const node& n, size_t count, size_t group, const char* shape) const {
    if (count % group != 0) {
      fail_at(
          n,
          kXs,
          "has " + std::to_string(count) + " vertices; each " + shape + " takes " + std::to_string(group) +
              ", so the count must be a multiple of " + std::to_string(group));
    }
  }
};

std::vector<field_desc> xyz_fields(const char* xs, const char* ys, const char* zs) {
  return {numbers_field("xs", xs), numbers_field("ys", ys), numbers_field("zs", zs)};
}

enum class xyz_kind { line, scatter, triangle, quad };

class xyz_series : public series3d_class {
 public:
  xyz_series(std::string name, xyz_kind kind, std::vector<field_desc> fields, std::string description)
      : series3d_class(std::move(name), std::move(fields), std::move(description)), kind_(kind) {}

  void check(const node& n) const override {
    const size_t count = check_xyz(n);
    if (kind_ == xyz_kind::triangle) {
      check_groups(n, count, 3, "triangle");
    } else if (kind_ == xyz_kind::quad) {
      check_groups(n, count, 4, "quad");
    }
  }

  void draw(const node& n, draw_context&) const override {
    const doubles& xs = n.get<doubles>(kXs);
    const doubles& ys = n.get<doubles>(kYs);
    const doubles& zs = n.get<doubles>(kZs);
    const int count = static_cast<int>(xs.size());
    const char* label = str(n, item_field::label);
    const ImPlot3DSpec spec = item_spec(n);
    switch (kind_) {
      case xyz_kind::line:
        ImPlot3D::PlotLine(label, xs.data(), ys.data(), zs.data(), count, spec);
        break;
      case xyz_kind::scatter:
        ImPlot3D::PlotScatter(label, xs.data(), ys.data(), zs.data(), count, spec);
        break;
      case xyz_kind::triangle:
        ImPlot3D::PlotTriangle(label, xs.data(), ys.data(), zs.data(), count, spec);
        break;
      case xyz_kind::quad:
        ImPlot3D::PlotQuad(label, xs.data(), ys.data(), zs.data(), count, spec);
        break;
    }
  }

 private:
  xyz_kind kind_;
};

class surface_class : public series3d_class {
 public:
  static constexpr size_t x_count = kZs + 1, y_count = kZs + 2, scale_min = kZs + 3, scale_max = kZs + 4;

  surface_class()
      : series3d_class(
            "Plot3DSurface",
            {
                numbers_field("xs", "Flattened x_count x y_count array of X coordinates for each grid point."),
                numbers_field("ys", "Flattened x_count x y_count array of Y coordinates for each grid point."),
                numbers_field("zs", "Flattened x_count x y_count array of Z (height) values."),
                int_field("x_count", 2, "Number of grid points along the X (first) dimension.", value_range{2, 4096}),
                int_field("y_count", 2, "Number of grid points along the Y (second) dimension.", value_range{2, 4096}),
                number_field(
                    "scale_min",
                    0.0,
                    "Minimum value used to map Z height to colormap. Both scale_min and scale_max equal to 0 triggers "
                    "auto-range."),
                number_field(
                    "scale_max",
                    0.0,
                    "Maximum value used to map Z height to colormap. Both scale_min and scale_max equal to 0 triggers "
                    "auto-range."),
            },
            "Draws a colored 3-D parametric surface on an x_count x y_count grid. xs, ys, zs are flattened "
            "row-major arrays.") {}

  void check(const node& n) const override {
    const size_t want = static_cast<size_t>(n.get<int64_t>(x_count) * n.get<int64_t>(y_count));
    for (size_t f : {kXs, kYs, kZs}) {
      const size_t have = n.get<doubles>(f).size();
      if (have != want) {
        fail_at(
            n,
            f,
            "has " + std::to_string(have) + " values; x_count x y_count needs " + std::to_string(want) + " (" +
                std::to_string(n.get<int64_t>(x_count)) + " x " + std::to_string(n.get<int64_t>(y_count)) + ")");
      }
    }
  }

  void draw(const node& n, draw_context&) const override {
    ImPlot3D::PlotSurface(
        str(n, item_field::label),
        n.get<doubles>(kXs).data(),
        n.get<doubles>(kYs).data(),
        n.get<doubles>(kZs).data(),
        static_cast<int>(n.get<int64_t>(x_count)),
        static_cast<int>(n.get<int64_t>(y_count)),
        n.get<double>(scale_min),
        n.get<double>(scale_max),
        item_spec(n));
  }
};

class mesh_class : public series3d_class {
 public:
  static constexpr size_t indices = kZs + 1;

  mesh_class()
      : series3d_class(
            "Plot3DMesh",
            {
                numbers_field("xs", "X coordinate of each mesh vertex."),
                numbers_field("ys", "Y coordinate of each mesh vertex."),
                numbers_field("zs", "Z coordinate of each mesh vertex."),
                integers_field(
                    "indices",
                    "Flat list of vertex indices forming triangles; length must be a multiple of 3."),
            },
            "Draws an indexed 3-D triangle mesh. xs/ys/zs are vertex arrays; indices is a flat list of vertex "
            "indices (groups of 3 per triangle).") {}

  void check(const node& n) const override {
    const size_t vertices = check_xyz(n);
    const std::vector<int64_t>& idx = n.get<std::vector<int64_t>>(indices);
    if (idx.empty() || idx.size() % 3 != 0) {
      fail_at(
          n,
          indices,
          "has " + std::to_string(idx.size()) +
              " entries; each triangle takes 3, so it needs a non-zero multiple of 3");
    }
    for (size_t i = 0; i < idx.size(); ++i) {
      if (idx[i] < 0 || static_cast<size_t>(idx[i]) >= vertices) {
        fail_at(
            n,
            indices,
            "entry " + std::to_string(i + 1) + " is " + std::to_string(idx[i]) +
                ", but the vertices are numbered 0 to " + std::to_string(vertices - 1));
      }
    }
  }

  void draw(const node& n, draw_context&) const override {
    const std::vector<int64_t>& idx = n.get<std::vector<int64_t>>(indices);
    std::vector<unsigned int> uidx(idx.begin(), idx.end()); // bounds checked in check()
    const doubles& xs = n.get<doubles>(kXs);
    ImPlot3D::PlotMesh(
        str(n, item_field::label),
        xs.data(),
        n.get<doubles>(kYs).data(),
        n.get<doubles>(kZs).data(),
        uidx.data(),
        static_cast<int>(xs.size()),
        static_cast<int>(uidx.size()),
        item_spec(n));
  }
};

class text3d_class : public series3d_class {
 public:
  static constexpr size_t text = 0, x = text + 1, y = text + 2, z = text + 3, angle = text + 4,
                          offset_x = text + 5, offset_y = text + 6;

  text3d_class()
      : series3d_class(
            "Plot3DText",
            {
                text_field("text", "", "The string to display at the given 3-D position."),
                number_field("x", 0.0, "Plot X coordinate of the text anchor."),
                number_field("y", 0.0, "Plot Y coordinate of the text anchor."),
                number_field("z", 0.0, "Plot Z coordinate of the text anchor."),
                number_field("angle", 0.0, "Rotation angle of the text in degrees.", value_range{-360, 360}),
                number_field("offset_x", 0.0, "Pixel offset applied to the text after projection (horizontal)."),
                number_field("offset_y", 0.0, "Pixel offset applied to the text after projection (vertical)."),
                text_style_fields()[0],
                text_style_fields()[1],
                text_style_fields()[2],
            },
            "Renders a text string at a given 3-D plot coordinate.",
            /*style=*/0) {}

  void check(const node& n) const override {
    if (n.get<std::string>(text).empty()) {
      fail_at(n, text, "is empty: nothing to draw");
    }
  }

  void draw(const node& n, draw_context&) const override {
    ImPlot3D::PlotText(
        str(n, text),
        n.get<double>(x),
        n.get<double>(y),
        n.get<double>(z),
        n.get<double>(angle),
        ImVec2(static_cast<float>(n.get<double>(offset_x)), static_cast<float>(n.get<double>(offset_y))));
  }
};

} // namespace

void register_plot3d(registry& reg) {
  reg.add(std::make_unique<plot3d_class>());
  reg.add(std::make_unique<xyz_series>(
      "Plot3DLine",
      xyz_kind::line,
      xyz_fields(
          "Array of X coordinates, one per data point.",
          "Array of Y coordinates, one per data point.",
          "Array of Z coordinates, one per data point."),
      "Draws a connected 3-D line through (xs[i], ys[i], zs[i]) data points."));
  reg.add(std::make_unique<xyz_series>(
      "Plot3DScatter",
      xyz_kind::scatter,
      xyz_fields(
          "Array of X coordinates, one per data point.",
          "Array of Y coordinates, one per data point.",
          "Array of Z coordinates, one per data point."),
      "Draws a marker at each (xs[i], ys[i], zs[i]) without connecting lines."));
  reg.add(std::make_unique<xyz_series>(
      "Plot3DTriangle",
      xyz_kind::triangle,
      xyz_fields(
          "Vertex X coordinates; length must be a multiple of 3.",
          "Vertex Y coordinates; length must be a multiple of 3.",
          "Vertex Z coordinates; length must be a multiple of 3."),
      "Draws filled 3-D triangles. xs/ys/zs are vertex arrays whose length must be a multiple of 3 (every 3 "
      "consecutive vertices form one triangle)."));
  reg.add(std::make_unique<xyz_series>(
      "Plot3DQuad",
      xyz_kind::quad,
      xyz_fields(
          "Vertex X coordinates; length must be a multiple of 4.",
          "Vertex Y coordinates; length must be a multiple of 4.",
          "Vertex Z coordinates; length must be a multiple of 4."),
      "Draws filled 3-D quads. xs/ys/zs are vertex arrays whose length must be a multiple of 4 (every 4 "
      "consecutive vertices form one quad)."));
  reg.add(std::make_unique<surface_class>());
  reg.add(std::make_unique<mesh_class>());
  reg.add(std::make_unique<text3d_class>());
}

} // namespace bdg::nymph
