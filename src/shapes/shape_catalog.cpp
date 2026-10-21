// MIT License © 2026 Binary Dice Games
/// @file shape_catalog.cpp
/// @brief The shape catalog: parsing the embedded files and registering
///        them with ImUml.
#include "shapes/shape_catalog.hpp"
#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include "core/suggest.hpp"
#include "placeholders/placeholders.hpp"

namespace bdg::nymph {

namespace {

/// A parsed shape and its flattened form, ready for ImUml::AddShape().
struct shape_data {
  std::string name;
  std::string category;
  svg_image image;
  std::vector<ImVec2> points;
  std::vector<ImUmlShapePath> paths;
  std::vector<ImVec2> outline;
};

/// Segments per cubic curve: fixed, so output does not depend on ImGui's
/// tessellation settings.
constexpr int kCurveSegments = 12;

int role_of(svg_role role) {
  switch (role) {
    case svg_role::half:
      return ImUmlShapeRole_Half;
    case svg_role::cutout:
      return ImUmlShapeRole_Fill;
    default:
      return ImUmlShapeRole_Line;
  }
}

/// Flattens `d.image` into the points and paths ImUml takes.
void flatten_into(shape_data& d) {
  for (const svg_shape& shape : d.image.shapes) {
    for (const svg_subpath& sub : shape.subpaths) {
      ImUmlShapePath path{};
      path.Role = role_of(shape.role);
      path.Stroke = shape.stroke;
      path.Closed = sub.closed;
      path.Width = shape.width;
      path.Dash = shape.dash;
      path.Gap = shape.gap;
      path.PointStart = static_cast<int>(d.points.size());
      const std::vector<ImVec2> pts = flatten(sub, kCurveSegments);
      d.points.insert(d.points.end(), pts.begin(), pts.end());
      path.PointCount = static_cast<int>(pts.size());
      d.paths.push_back(path);
    }
  }
  if (d.image.has_outline) {
    d.outline = flatten(d.image.outline, kCurveSegments);
  }
}

shape_data build(const embedded_file& file) {
  shape_data d;
  const std::string_view full = file.name;
  const size_t slash = full.find('/');
  d.category = std::string(full.substr(0, slash));
  d.name = std::string(full.substr(slash + 1));
  try {
    d.image = parse_svg(file.text(), svg_dialect::shape);
  } catch (const svg_error& e) {
    throw svg_error("shape '" + d.name + "': " + e.what());
  }
  flatten_into(d);
  return d;
}

/// The placeholder images as shapes named "image:<name>" (an org chart's
/// portraits); their colour roles map one to one.
const std::vector<shape_data>& placeholder_shapes() {
  static std::once_flag once;
  static std::vector<shape_data> shapes;
  std::call_once(once, [] {
    for (const embedded_file& f : embedded_placeholders()) {
      shape_data d;
      d.name = "image:" + std::string(f.name);
      d.category = "image";
      d.image = parse_svg(f.text());
      flatten_into(d);
      shapes.push_back(std::move(d));
    }
  });
  return shapes;
}

/// Every shape, parsed on first use, sorted by name.
const std::vector<shape_data>& catalog() {
  static std::once_flag once;
  static std::vector<shape_data> shapes;
  std::call_once(once, [] {
    for (const embedded_file& f : embedded_shapes()) {
      shapes.push_back(build(f));
    }
    std::sort(shapes.begin(), shapes.end(), [](const shape_data& a, const shape_data& b) { return a.name < b.name; });
  });
  return shapes;
}

const shape_data* find_data(std::string_view name) {
  const auto& all = catalog();
  const auto it = std::lower_bound(
      all.begin(), all.end(), name, [](const shape_data& s, std::string_view n) { return s.name < n; });
  return it != all.end() && it->name == name ? &*it : nullptr;
}

} // namespace

const std::vector<std::string>& shape_names() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> out;
    for (const shape_data& s : catalog()) {
      out.push_back(s.name);
    }
    return out;
  }();
  return names;
}

std::string shape_category(std::string_view name) {
  const shape_data* d = find_data(name);
  return d == nullptr ? std::string() : d->category;
}

const svg_image* find_shape(std::string_view name) {
  const shape_data* d = find_data(name);
  return d == nullptr ? nullptr : &d->image;
}

namespace {

void register_one(const shape_data& s) {
  {
    ImUmlShapeDesc d{};
    d.Name = s.name.c_str();
    d.ViewSize = s.image.view_size;
    d.Resizable = s.image.stretch;
    for (int i = 0; i < 4; ++i) {
      d.Slice[i] = s.image.slice[i];
    }
    d.GrowProportional = s.image.grow_proportional;
    d.HasText = s.image.text.present;
    d.TextMin = s.image.text.min;
    d.TextMax = s.image.text.max;
    d.HasContent = s.image.content.present;
    d.ContentMin = s.image.content.min;
    d.ContentMax = s.image.content.max;
    d.Points = s.points.data();
    d.PointCount = static_cast<int>(s.points.size());
    d.Paths = s.paths.data();
    d.PathCount = static_cast<int>(s.paths.size());
    d.Outline = s.outline.empty() ? nullptr : s.outline.data();
    d.OutlineCount = static_cast<int>(s.outline.size());
    ImUml::AddShape(d);
  }
}

} // namespace

void register_shapes() {
  for (const shape_data& s : catalog()) {
    register_one(s);
  }
  for (const shape_data& s : placeholder_shapes()) {
    register_one(s);
  }
}

std::string shapes_text(std::string_view name) {
  std::ostringstream out;
  if (name.empty()) {
    std::vector<std::string> lines;
    for (const shape_data& s : catalog()) {
      lines.push_back(s.category + '/' + s.name);
    }
    std::sort(lines.begin(), lines.end());
    for (const std::string& line : lines) {
      out << line << '\n';
    }
    return out.str();
  }
  const shape_data* s = find_data(name);
  if (s == nullptr) {
    throw std::invalid_argument(
        "unknown shape '" + std::string(name) + "'" + did_you_mean(name, shape_names()) +
        " (`nymph shapes` lists them)");
  }
  const svg_image& im = s->image;
  const auto rect = [&](const char* label, const svg_rect& r) {
    if (r.present) {
      out << label << ": " << r.min.x << ' ' << r.min.y << ' ' << (r.max.x - r.min.x) << ' ' << (r.max.y - r.min.y)
          << '\n';
    }
  };
  out << s->category << '/' << s->name << '\n';
  out << "size: " << im.view_size.x << " x " << im.view_size.y << '\n';
  out << "mode: " << (im.stretch ? "resizable" : "icon (keeps its aspect ratio, the label goes below)") << '\n';
  if (im.stretch) {
    out << "slice: " << im.slice[0] << ' ' << im.slice[1] << ' ' << im.slice[2] << ' ' << im.slice[3] << '\n';
    out << "grow: " << (im.grow_proportional ? "proportional" : "margins") << '\n';
  }
  rect("text", im.text);
  rect("content", im.content);
  return out.str();
}

} // namespace bdg::nymph
