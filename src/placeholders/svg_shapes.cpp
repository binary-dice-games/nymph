// MIT License © 2026 Binary Dice Games
/// @file svg_shapes.cpp
/// @brief parse_svg(): the placeholder SVG subset into shapes.
#include "placeholders/svg_shapes.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include "core/charconv.hpp"

namespace bdg::nymph {

namespace {

/// Control point distance of a quarter circle drawn as a cubic curve.
constexpr float kArc = 0.5522847498f;

ImVec2 add(ImVec2 a, ImVec2 b) {
  return ImVec2(a.x + b.x, a.y + b.y);
}
ImVec2 sub(ImVec2 a, ImVec2 b) {
  return ImVec2(a.x - b.x, a.y - b.y);
}
ImVec2 lerp(ImVec2 a, ImVec2 b, float t) {
  return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

bool space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

// ── Numbers ──────────────────────────────────────────────────────────────────

/// Reads numbers separated by spaces and commas, as SVG writes them
/// ("10,20 -3.5.5" is 10, 20, -3.5, 0.5).
class number_reader {
 public:
  explicit number_reader(std::string_view text) : text_(text) {}

  void skip_separators() {
    while (pos_ < text_.size() && (space(text_[pos_]) || text_[pos_] == ',')) {
      ++pos_;
    }
  }

  bool at_end() {
    skip_separators();
    return pos_ >= text_.size();
  }

  /// Whether a number starts here (after separators).
  bool at_number() {
    skip_separators();
    if (pos_ >= text_.size()) {
      return false;
    }
    const char c = text_[pos_];
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
  }

  /// The next character, consumed (path commands).
  char next_char() {
    skip_separators();
    return pos_ < text_.size() ? text_[pos_++] : '\0';
  }

  float number(std::string_view what) {
    skip_separators();
    size_t start = pos_;
    if (start < text_.size() && text_[start] == '+') {
      ++start; // from_chars takes no '+'
    }
    double v = 0.0;
    const auto r = from_chars_double(text_.data() + start, text_.data() + text_.size(), v);
    if (r.ec != std::errc() || !std::isfinite(v)) {
      throw svg_error("a number was expected in " + std::string(what) + " at '" +
                      std::string(text_.substr(pos_, 12)) + "'");
    }
    pos_ = static_cast<size_t>(r.ptr - text_.data());
    return static_cast<float>(v);
  }

  ImVec2 point(std::string_view what) {
    const float x = number(what);
    const float y = number(what);
    return ImVec2(x, y);
  }

 private:
  std::string_view text_;
  size_t pos_ = 0;
};

// ── Shapes ───────────────────────────────────────────────────────────────────

/// Closed subpath of four cubic quarter arcs around an ellipse.
svg_subpath ellipse(ImVec2 c, float rx, float ry) {
  svg_subpath p;
  p.start = ImVec2(c.x + rx, c.y);
  p.closed = true;
  const float kx = rx * kArc, ky = ry * kArc;
  p.segments = {
      {true, ImVec2(c.x + rx, c.y + ky), ImVec2(c.x + kx, c.y + ry), ImVec2(c.x, c.y + ry)},
      {true, ImVec2(c.x - kx, c.y + ry), ImVec2(c.x - rx, c.y + ky), ImVec2(c.x - rx, c.y)},
      {true, ImVec2(c.x - rx, c.y - ky), ImVec2(c.x - kx, c.y - ry), ImVec2(c.x, c.y - ry)},
      {true, ImVec2(c.x + kx, c.y - ry), ImVec2(c.x + rx, c.y - ky), ImVec2(c.x + rx, c.y)},
  };
  return p;
}

svg_subpath rectangle(float x, float y, float w, float h, float rx, float ry) {
  svg_subpath p;
  p.closed = true;
  const auto line = [&](ImVec2 to) { p.segments.push_back({false, {}, {}, to}); };
  if (rx <= 0.0f || ry <= 0.0f) {
    p.start = ImVec2(x, y);
    line(ImVec2(x + w, y));
    line(ImVec2(x + w, y + h));
    line(ImVec2(x, y + h));
    line(ImVec2(x, y));
    return p;
  }
  const float kx = rx * kArc, ky = ry * kArc;
  const auto curve = [&](ImVec2 c1, ImVec2 c2, ImVec2 to) { p.segments.push_back({true, c1, c2, to}); };
  p.start = ImVec2(x + rx, y);
  line(ImVec2(x + w - rx, y));
  curve(ImVec2(x + w - rx + kx, y), ImVec2(x + w, y + ry - ky), ImVec2(x + w, y + ry));
  line(ImVec2(x + w, y + h - ry));
  curve(ImVec2(x + w, y + h - ry + ky), ImVec2(x + w - rx + kx, y + h), ImVec2(x + w - rx, y + h));
  line(ImVec2(x + rx, y + h));
  curve(ImVec2(x + rx - kx, y + h), ImVec2(x, y + h - ry + ky), ImVec2(x, y + h - ry));
  line(ImVec2(x, y + ry));
  curve(ImVec2(x, y + ry - ky), ImVec2(x + rx - kx, y), ImVec2(x + rx, y));
  return p;
}

/// Appends the elliptical arc from `from` to `to` (SVG's endpoint form, F.6.5)
/// to `p` as cubic curves of at most a quarter turn each.
void append_arc(
    svg_subpath& p, ImVec2 from, float rx, float ry, float rotation_deg, bool large, bool sweep, ImVec2 to) {
  if (from.x == to.x && from.y == to.y) {
    return;
  }
  rx = std::fabs(rx);
  ry = std::fabs(ry);
  if (rx == 0.0f || ry == 0.0f) {
    p.segments.push_back({false, {}, {}, to});
    return;
  }
  const double phi = static_cast<double>(rotation_deg) * 3.14159265358979323846 / 180.0;
  const double cp = std::cos(phi), sp = std::sin(phi);
  const double dx = (static_cast<double>(from.x) - to.x) * 0.5, dy = (static_cast<double>(from.y) - to.y) * 0.5;
  const double x1 = cp * dx + sp * dy, y1 = -sp * dx + cp * dy;
  double rxd = rx, ryd = ry;
  const double lambda = x1 * x1 / (rxd * rxd) + y1 * y1 / (ryd * ryd);
  if (lambda > 1.0) {
    rxd *= std::sqrt(lambda);
    ryd *= std::sqrt(lambda);
  }
  const double num = rxd * rxd * ryd * ryd - rxd * rxd * y1 * y1 - ryd * ryd * x1 * x1;
  const double den = rxd * rxd * y1 * y1 + ryd * ryd * x1 * x1;
  double coef = den == 0.0 ? 0.0 : std::sqrt(std::max(0.0, num / den));
  if (large == sweep) {
    coef = -coef;
  }
  const double cx1 = coef * rxd * y1 / ryd, cy1 = -coef * ryd * x1 / rxd;
  const double cx = cp * cx1 - sp * cy1 + (static_cast<double>(from.x) + to.x) * 0.5;
  const double cy = sp * cx1 + cp * cy1 + (static_cast<double>(from.y) + to.y) * 0.5;
  const double theta1 = std::atan2((y1 - cy1) / ryd, (x1 - cx1) / rxd);
  double delta = std::atan2((-y1 - cy1) / ryd, (-x1 - cx1) / rxd) - theta1;
  const double two_pi = 6.28318530717958647692;
  if (!sweep && delta > 0.0) {
    delta -= two_pi;
  } else if (sweep && delta < 0.0) {
    delta += two_pi;
  }
  const int pieces = std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / (two_pi / 4.0) - 1e-9)));
  const double step = delta / pieces;
  const double k = 4.0 / 3.0 * std::tan(step / 4.0);
  double a = theta1;
  for (int i = 0; i < pieces; ++i) {
    const double b = a + step;
    const double ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b);
    const auto map = [&](double ex, double ey) {
      return ImVec2(
          static_cast<float>(cp * rxd * ex - sp * ryd * ey + cx), static_cast<float>(sp * rxd * ex + cp * ryd * ey + cy));
    };
    const ImVec2 c1 = map(ca - k * sa, sa + k * ca);
    const ImVec2 c2 = map(cb + k * sb, sb - k * cb);
    // The last piece ends exactly at `to`, not at a rounded point.
    const ImVec2 end = i + 1 == pieces ? to : map(cb, sb);
    p.segments.push_back({true, c1, c2, end});
    a = b;
  }
}

/// The subpaths of a path's `d`.
std::vector<svg_subpath> parse_path(std::string_view d, bool allow_arcs) {
  std::vector<svg_subpath> out;
  number_reader in(d);
  ImVec2 cur(0.0f, 0.0f);
  ImVec2 last_cubic_c2 = cur, last_quad = cur;
  char prev = '\0';
  char cmd = '\0';
  const auto current = [&]() -> svg_subpath& {
    if (out.empty()) {
      throw svg_error("path data must start with M (a move)");
    }
    return out.back();
  };
  while (!in.at_end()) {
    if (!in.at_number()) {
      cmd = in.next_char();
    } else if (cmd == '\0') {
      throw svg_error("path data must start with M (a move)");
    } else if (cmd == 'M') {
      cmd = 'L'; // pairs after a move are lines
    } else if (cmd == 'm') {
      cmd = 'l';
    } else if (cmd == 'Z' || cmd == 'z') {
      throw svg_error("numbers after Z in path data");
    }
    const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
    const ImVec2 base = rel ? cur : ImVec2(0.0f, 0.0f);
    const auto pt = [&]() { return add(base, in.point("path data")); };
    switch (std::toupper(static_cast<unsigned char>(cmd))) {
      case 'M': {
        svg_subpath p;
        p.start = pt();
        cur = p.start;
        out.push_back(std::move(p));
        break;
      }
      case 'L': {
        const ImVec2 to = pt();
        current().segments.push_back({false, {}, {}, to});
        cur = to;
        break;
      }
      case 'H': {
        const float x = in.number("path data") + (rel ? cur.x : 0.0f);
        cur = ImVec2(x, cur.y);
        current().segments.push_back({false, {}, {}, cur});
        break;
      }
      case 'V': {
        const float y = in.number("path data") + (rel ? cur.y : 0.0f);
        cur = ImVec2(cur.x, y);
        current().segments.push_back({false, {}, {}, cur});
        break;
      }
      case 'C': {
        const ImVec2 c1 = pt(), c2 = pt(), to = pt();
        current().segments.push_back({true, c1, c2, to});
        last_cubic_c2 = c2;
        cur = to;
        break;
      }
      case 'S': {
        const bool after_cubic = prev == 'C' || prev == 'S';
        const ImVec2 c1 = after_cubic ? sub(add(cur, cur), last_cubic_c2) : cur;
        const ImVec2 c2 = pt(), to = pt();
        current().segments.push_back({true, c1, c2, to});
        last_cubic_c2 = c2;
        cur = to;
        break;
      }
      case 'Q':
      case 'T': {
        const bool smooth = std::toupper(static_cast<unsigned char>(cmd)) == 'T';
        const bool after_quad = prev == 'Q' || prev == 'T';
        const ImVec2 q = smooth ? (after_quad ? sub(add(cur, cur), last_quad) : cur) : pt();
        const ImVec2 to = pt();
        // A quadratic curve as the cubic with the same shape.
        current().segments.push_back({true, lerp(cur, q, 2.0f / 3.0f), lerp(to, q, 2.0f / 3.0f), to});
        last_quad = q;
        cur = to;
        break;
      }
      case 'Z':
        current().closed = true;
        cur = current().start;
        break;
      case 'A': {
        if (!allow_arcs) {
          throw svg_error("arcs (A) are not supported in placeholders; draw them with C curves");
        }
        const float rx = in.number("path data"), ry = in.number("path data"), rotation = in.number("path data");
        const bool large = in.number("path data") != 0.0f, sweep = in.number("path data") != 0.0f;
        const ImVec2 to = pt();
        append_arc(current(), cur, rx, ry, rotation, large, sweep, to);
        cur = to;
        break;
      }
      default:
        throw svg_error(std::string("unknown path command '") + cmd + "'");
    }
    prev = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
  }
  return out;
}

// ── XML ──────────────────────────────────────────────────────────────────────

struct element {
  std::string name;
  std::map<std::string, std::string, std::less<>> attributes;
  bool self_closing = false;
};

/// Reads `<name a="1" b='2' ...>` or `.../>` starting after '<'.
element read_tag(std::string_view text, size_t& pos) {
  element e;
  const size_t name_start = pos;
  while (pos < text.size() && !space(text[pos]) && text[pos] != '>' && text[pos] != '/') {
    ++pos;
  }
  e.name = std::string(text.substr(name_start, pos - name_start));
  for (;;) {
    while (pos < text.size() && space(text[pos])) {
      ++pos;
    }
    if (pos >= text.size()) {
      throw svg_error("unterminated <" + e.name + "> tag");
    }
    if (text[pos] == '>') {
      ++pos;
      return e;
    }
    if (text.substr(pos, 2) == "/>") {
      pos += 2;
      e.self_closing = true;
      return e;
    }
    const size_t key_start = pos;
    while (pos < text.size() && text[pos] != '=' && !space(text[pos]) && text[pos] != '>') {
      ++pos;
    }
    const std::string key(text.substr(key_start, pos - key_start));
    while (pos < text.size() && space(text[pos])) {
      ++pos;
    }
    if (pos >= text.size() || text[pos] != '=') {
      throw svg_error("attribute '" + key + "' of <" + e.name + "> has no value");
    }
    ++pos;
    while (pos < text.size() && space(text[pos])) {
      ++pos;
    }
    if (pos >= text.size() || (text[pos] != '"' && text[pos] != '\'')) {
      throw svg_error("attribute '" + key + "' of <" + e.name + "> needs a quoted value");
    }
    const char quote = text[pos++];
    const size_t end = text.find(quote, pos);
    if (end == std::string_view::npos) {
      throw svg_error("unterminated value of attribute '" + key + "'");
    }
    e.attributes[key] = std::string(text.substr(pos, end - pos));
    pos = end + 1;
  }
}

/// The colour role of a `fill` / `stroke` value; nullopt for "none".
std::optional<svg_role> role_of(const std::string& value, std::string_view what) {
  const std::string v = lower(value);
  if (v == "none") {
    return std::nullopt;
  }
  if (v == "#000" || v == "#000000" || v == "black") {
    return svg_role::tint;
  }
  if (v == "#808080" || v == "gray" || v == "grey") {
    return svg_role::half;
  }
  if (v == "#fff" || v == "#ffffff" || v == "white") {
    return svg_role::cutout;
  }
  throw svg_error(
      std::string(what) + " '" + value + "' is not one of the placeholder colours: #000000 (tint), #808080 (half tint), "
      "#FFFFFF (cut-out) or none");
}

float attribute(const element& e, const char* name, std::optional<float> fallback = std::nullopt) {
  const auto it = e.attributes.find(name);
  if (it == e.attributes.end()) {
    if (fallback) {
      return *fallback;
    }
    throw svg_error("<" + e.name + "> needs the attribute '" + name + "'");
  }
  number_reader in(it->second);
  const float v = in.number(std::string("'") + name + "'");
  if (!in.at_end()) {
    throw svg_error("attribute '" + std::string(name) + "' of <" + e.name + "> must be one number");
  }
  return v;
}

/// The attributes each element may have (presentation ones included).
const std::map<std::string, std::vector<std::string>, std::less<>>& allowed_attributes() {
  static const std::map<std::string, std::vector<std::string>, std::less<>> t{
      {"path", {"d"}},
      {"rect", {"x", "y", "width", "height", "rx", "ry"}},
      {"circle", {"cx", "cy", "r"}},
      {"ellipse", {"cx", "cy", "rx", "ry"}},
      {"polygon", {"points"}},
      {"line", {"x1", "y1", "x2", "y2"}},
  };
  return t;
}

/// The subpaths of one shape element.
std::vector<svg_subpath> geometry(const element& e, bool allow_arcs) {
  if (e.name == "path") {
    const auto it = e.attributes.find("d");
    if (it == e.attributes.end()) {
      throw svg_error("<path> needs the attribute 'd'");
    }
    return parse_path(it->second, allow_arcs);
  }
  if (e.name == "rect") {
    const float w = attribute(e, "width"), h = attribute(e, "height");
    const auto rx_set = e.attributes.count("rx") != 0, ry_set = e.attributes.count("ry") != 0;
    float rx = attribute(e, "rx", 0.0f), ry = attribute(e, "ry", 0.0f);
    if (rx_set && !ry_set) {
      ry = rx;
    } else if (ry_set && !rx_set) {
      rx = ry;
    }
    return {rectangle(
        attribute(e, "x", 0.0f), attribute(e, "y", 0.0f), w, h, std::min(rx, w * 0.5f), std::min(ry, h * 0.5f))};
  }
  if (e.name == "circle") {
    const float r = attribute(e, "r");
    return {ellipse(ImVec2(attribute(e, "cx", 0.0f), attribute(e, "cy", 0.0f)), r, r)};
  }
  if (e.name == "ellipse") {
    return {ellipse(ImVec2(attribute(e, "cx", 0.0f), attribute(e, "cy", 0.0f)), attribute(e, "rx"), attribute(e, "ry"))};
  }
  if (e.name == "polygon") {
    const auto it = e.attributes.find("points");
    if (it == e.attributes.end()) {
      throw svg_error("<polygon> needs the attribute 'points'");
    }
    number_reader in(it->second);
    svg_subpath p;
    p.closed = true;
    p.start = in.point("points");
    while (!in.at_end()) {
      p.segments.push_back({false, {}, {}, in.point("points")});
    }
    if (p.segments.size() < 2) {
      throw svg_error("<polygon> needs at least three points");
    }
    return {p};
  }
  // line
  svg_subpath p;
  p.start = ImVec2(attribute(e, "x1", 0.0f), attribute(e, "y1", 0.0f));
  p.segments.push_back({false, {}, {}, ImVec2(attribute(e, "x2", 0.0f), attribute(e, "y2", 0.0f))});
  return {p};
}

/// A `text` or `content` rect: a rect that is never drawn.
svg_rect reserved_rect(const element& e, const std::string& id) {
  if (e.name != "rect") {
    throw svg_error("the shape element with id '" + id + "' must be a <rect>");
  }
  const auto none = [&](const char* key) {
    const auto it = e.attributes.find(key);
    return it != e.attributes.end() && lower(it->second) == "none";
  };
  if (!none("fill") || !none("stroke")) {
    throw svg_error("the rect with id '" + id + "' is never drawn: give it fill=\"none\" and stroke=\"none\"");
  }
  const float x = attribute(e, "x", 0.0f), y = attribute(e, "y", 0.0f);
  svg_rect r;
  r.present = true;
  r.min = ImVec2(x, y);
  r.max = ImVec2(x + attribute(e, "width"), y + attribute(e, "height"));
  if (!(r.max.x > r.min.x && r.max.y > r.min.y)) {
    throw svg_error("the rect with id '" + id + "' needs a positive width and height");
  }
  return r;
}

void add_shape(svg_image& image, const element& e, svg_dialect dialect) {
  const bool shape_file = dialect == svg_dialect::shape;
  const char* const kind = shape_file ? "a shape" : "a placeholder";
  const auto allowed = allowed_attributes().find(e.name);
  if (allowed == allowed_attributes().end()) {
    throw svg_error("<" + e.name + "> is not allowed in " + kind + " (only path, rect, circle, ellipse, polygon, line)");
  }
  std::string id;
  for (const auto& [key, value] : e.attributes) {
    if (shape_file && key == "id") {
      id = value;
      continue;
    }
    const bool presentation = key == "fill" || key == "stroke" || key == "stroke-width" ||
        key == "stroke-linejoin" || key == "stroke-linecap" || (shape_file && key == "stroke-dasharray");
    if (!presentation && std::find(allowed->second.begin(), allowed->second.end(), key) == allowed->second.end()) {
      throw svg_error("attribute '" + key + "' is not allowed on <" + e.name + "> in " + kind);
    }
  }
  if (id == "text" || id == "content") {
    svg_rect& slot = id == "text" ? image.text : image.content;
    if (slot.present) {
      throw svg_error("more than one element with id '" + id + "'");
    }
    slot = reserved_rect(e, id);
    return;
  }
  const auto fill_it = e.attributes.find("fill");
  const auto stroke_it = e.attributes.find("stroke");
  // SVG's defaults: filled black, not stroked (a line has nothing to fill).
  const std::optional<svg_role> fill = e.name == "line" ? std::nullopt
      : fill_it == e.attributes.end()                   ? std::optional<svg_role>(svg_role::tint)
                                                        : role_of(fill_it->second, "fill");
  const std::optional<svg_role> stroke =
      stroke_it == e.attributes.end() ? std::nullopt : role_of(stroke_it->second, "stroke");
  std::vector<svg_subpath> paths = geometry(e, shape_file);
  if (id == "outline") {
    if (image.has_outline) {
      throw svg_error("more than one element with id 'outline'");
    }
    if (paths.size() != 1 || !paths[0].closed) {
      throw svg_error("the element with id 'outline' must be one closed shape");
    }
    image.outline = paths[0];
    image.has_outline = true;
  } else if (!fill && !stroke) {
    throw svg_error("<" + e.name + "> is neither filled nor stroked");
  }
  if (!fill && !stroke) {
    return; // an outline that is only a border for clipping
  }
  if (fill) {
    svg_shape s;
    s.role = *fill;
    s.subpaths = paths;
    image.shapes.push_back(std::move(s));
  }
  if (stroke) {
    svg_shape s;
    s.role = *stroke;
    s.stroke = true;
    s.width = attribute(e, "stroke-width", 1.0f);
    if (!(s.width > 0.0f)) {
      throw svg_error("stroke-width must be above 0");
    }
    if (const auto it = e.attributes.find("stroke-dasharray"); it != e.attributes.end()) {
      number_reader dash(it->second);
      s.dash = dash.number("stroke-dasharray");
      s.gap = dash.at_end() ? s.dash : dash.number("stroke-dasharray");
      if (!(s.dash > 0.0f && s.gap > 0.0f) || !dash.at_end()) {
        throw svg_error("stroke-dasharray takes one or two numbers above 0 (dash and gap)");
      }
    }
    s.subpaths = std::move(paths);
    image.shapes.push_back(std::move(s));
  }
}

} // namespace

svg_image parse_svg(std::string_view text, svg_dialect dialect) {
  const bool shape_file = dialect == svg_dialect::shape;
  svg_image image;
  bool in_svg = false, done = false;
  size_t pos = 0;
  while (pos < text.size()) {
    if (space(text[pos])) {
      ++pos;
      continue;
    }
    if (text[pos] != '<') {
      throw svg_error("text outside tags is not allowed in a placeholder");
    }
    // The comment markers are built from parts: the web viewer is inlined
    // into HTML, where the sequences must not appear (NymphClaudeApp.cmake).
    static const std::string kOpen = std::string("<!") + "--", kClose = std::string("--") + ">";
    if (text.substr(pos, 4) == kOpen) {
      const size_t end = text.find(kClose, pos + 4);
      if (end == std::string_view::npos) {
        throw svg_error("unterminated comment");
      }
      pos = end + 3;
      continue;
    }
    if (text.substr(pos, 2) == "<?") {
      const size_t end = text.find("?>", pos + 2);
      if (end == std::string_view::npos || in_svg) {
        throw svg_error("misplaced XML declaration");
      }
      pos = end + 2;
      continue;
    }
    if (text.substr(pos, 6) == "</svg>") {
      if (!in_svg) {
        throw svg_error("</svg> without <svg>");
      }
      in_svg = false;
      done = true;
      pos += 6;
      continue;
    }
    if (text.substr(pos, 2) == "</") {
      throw svg_error("only <svg> may have children; write shapes as self-closing tags (<path ... />)");
    }
    ++pos;
    const element e = read_tag(text, pos);
    if (done) {
      throw svg_error("content after </svg>");
    }
    if (!in_svg) {
      if (e.name != "svg" || e.self_closing) {
        throw svg_error("a placeholder must be one <svg> element");
      }
      for (const auto& [key, value] : e.attributes) {
        const bool data = shape_file && (key == "data-slice" || key == "data-grow");
        if (key != "xmlns" && key != "viewBox" && key != "width" && key != "height" && key != "version" &&
            key != "preserveAspectRatio" && !data) {
          throw svg_error("attribute '" + key + "' is not allowed on <svg> in a placeholder");
        }
      }
      const auto view = e.attributes.find("viewBox");
      if (view == e.attributes.end()) {
        throw svg_error("<svg> needs a viewBox");
      }
      number_reader in(view->second);
      image.view_min = in.point("viewBox");
      image.view_size = in.point("viewBox");
      if (!(image.view_size.x > 0.0f && image.view_size.y > 0.0f)) {
        throw svg_error("the viewBox must have a positive width and height");
      }
      const auto aspect = e.attributes.find("preserveAspectRatio");
      if (aspect != e.attributes.end()) {
        if (aspect->second == "none") {
          image.stretch = true;
        } else if (aspect->second != "xMidYMid meet" && aspect->second != "xMidYMid") {
          throw svg_error("preserveAspectRatio must be 'none' or 'xMidYMid meet'");
        }
      }
      if (const auto slice = e.attributes.find("data-slice"); slice != e.attributes.end()) {
        number_reader sin(slice->second);
        for (float& v : image.slice) {
          v = sin.number("data-slice");
          if (v < 0.0f) {
            throw svg_error("data-slice margins must not be negative");
          }
        }
        if (!sin.at_end()) {
          throw svg_error("data-slice takes four numbers: left top right bottom");
        }
        if (image.slice[0] + image.slice[2] >= image.view_size.x || image.slice[1] + image.slice[3] >= image.view_size.y) {
          throw svg_error("data-slice margins must be smaller than the viewBox");
        }
        if (!image.stretch) {
          throw svg_error("data-slice needs preserveAspectRatio=\"none\" (an icon has no slices)");
        }
      }
      if (const auto grow = e.attributes.find("data-grow"); grow != e.attributes.end()) {
        if (grow->second == "proportional") {
          image.grow_proportional = true;
        } else if (grow->second != "margins") {
          throw svg_error("data-grow must be 'margins' or 'proportional'");
        }
      }
      in_svg = true;
      continue;
    }
    if (!e.self_closing) {
      throw svg_error("<" + e.name + "> must be self-closing (<" + e.name + " ... />): groups are not allowed");
    }
    add_shape(image, e, dialect);
  }
  if (!done) {
    throw svg_error("a placeholder must be one complete <svg> element");
  }
  if (image.shapes.empty()) {
    throw svg_error("the placeholder draws nothing");
  }
  const auto inside = [&](const svg_rect& r) {
    return !r.present ||
        (r.min.x >= image.view_min.x && r.min.y >= image.view_min.y && r.max.x <= image.view_min.x + image.view_size.x &&
         r.max.y <= image.view_min.y + image.view_size.y);
  };
  if (!inside(image.text) || !inside(image.content)) {
    throw svg_error("the text and content rects must lie inside the viewBox");
  }
  return image;
}

std::vector<ImVec2> flatten(const svg_subpath& subpath, int segments) {
  std::vector<ImVec2> out{subpath.start};
  ImVec2 cur = subpath.start;
  for (const svg_segment& s : subpath.segments) {
    if (!s.cubic) {
      out.push_back(s.to);
    } else {
      for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float u = 1.0f - t;
        out.emplace_back(
            u * u * u * cur.x + 3 * u * u * t * s.c1.x + 3 * u * t * t * s.c2.x + t * t * t * s.to.x,
            u * u * u * cur.y + 3 * u * u * t * s.c1.y + 3 * u * t * t * s.c2.y + t * t * t * s.to.y);
      }
    }
    cur = s.to;
  }
  return out;
}

} // namespace bdg::nymph
