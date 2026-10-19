// MIT License © 2026 Binary Dice Games
/// @file view.cpp
/// @brief bind_view(), write_view().
#include "document/view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "core/error.hpp"
#include "core/suggest.hpp"
#include "document/yaml_doc.hpp"
#include "schema/element_class.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

namespace {

[[noreturn]] void fail(const yaml_node& at, const std::string& message) {
  throw error(at.line, at.column, "view: " + message);
}

double number(const yaml_node& n, const std::string& what) {
  if (n.is_scalar() && !n.quoted() && !n.text.empty()) {
    char* end = nullptr;
    const double v = std::strtod(n.text.c_str(), &end);
    if (end == n.text.c_str() + n.text.size() && std::isfinite(v)) {
      return v;
    }
  }
  fail(n, what + " must be a number, not '" + n.text + "'");
}

std::vector<double> numbers(const yaml_node& n, size_t count, const std::string& what) {
  if (!n.is_sequence() || n.children.size() != count) {
    fail(n, what + " must be a list of " + std::to_string(count) + " numbers");
  }
  std::vector<double> out;
  for (const yaml_node& c : n.children) {
    out.push_back(number(c, what));
  }
  return out;
}

view_range range(const yaml_node& n, const std::string& what) {
  const std::vector<double> v = numbers(n, 2, what);
  if (!(v[0] < v[1])) {
    fail(n, what + " must be [min, max] with min < max");
  }
  return {v[0], v[1]};
}

std::vector<std::string_view> keys_of(view_kind kind) {
  switch (kind) {
    case view_kind::plot2d:
      return {"x", "y", "y2", "hidden"};
    case view_kind::gantt:
      return {"x", "hidden"};
    case view_kind::plot3d:
      return {"x", "y", "z", "rotation", "hidden"};
    case view_kind::diagram:
      return {"zoom", "center"};
    case view_kind::ui_window:
      return {"x", "y", "width", "height", "collapsed", "closed", "scroll"};
    case view_kind::ui_scroll:
      return {"scroll"};
    case view_kind::ui_open:
      return {"open"};
    case view_kind::ui_selected:
      return {"selected", "closed"};
    case view_kind::ui_index:
      return {"selected"};
    case view_kind::ui_flag:
    case view_kind::ui_number:
    case view_kind::ui_text:
    case view_kind::ui_color:
      return {"value"};
    case view_kind::ui_active:
      return {"active"};
    case view_kind::ui_checked:
      return {"checked"};
    case view_kind::none:
      break;
  }
  return {};
}

/// The key a kind's `flag` is written under.
const char* flag_key(view_kind kind) {
  switch (kind) {
    case view_kind::ui_open:
      return "open";
    case view_kind::ui_selected:
      return "selected";
    case view_kind::ui_active:
      return "active";
    case view_kind::ui_checked:
      return "checked";
    default:
      return "value";
  }
}

bool boolean(const yaml_node& n, const std::string& what) {
  if (n.is_scalar() && !n.quoted() && (n.text == "true" || n.text == "false")) {
    return n.text == "true";
  }
  fail(n, what + " must be true or false, not '" + n.text + "'");
}

/// "#RRGGBB" or "#RRGGBBAA".
bool is_color(const std::string& text) {
  if ((text.size() != 7 && text.size() != 9) || text[0] != '#') {
    return false;
  }
  return std::all_of(text.begin() + 1, text.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
  });
}

/// Reads one key of a user-interface entry into `v`.
void ui_key(view_kind kind, const yaml_node& key, const yaml_node& value, const std::string& what, element_view& v) {
  const std::string& k = key.text;
  if (kind == view_kind::ui_window && (k == "x" || k == "y")) {
    auto p = v.position.value_or(std::array<double, 2>{0, 0});
    const double d = number(value, what);
    if (d < 0 || d > 8192) {
      fail(value, what + " must be between 0 and 8192");
    }
    p[k == "x" ? 0 : 1] = d;
    v.position = p;
  } else if (k == "width" || k == "height") {
    auto sz = v.size.value_or(std::array<double, 2>{0, 0});
    const double d = number(value, what);
    if (d < 1 || d > 8192) {
      fail(value, what + " must be between 1 and 8192");
    }
    sz[k == "width" ? 0 : 1] = d;
    v.size = sz;
  } else if (k == "scroll") {
    const std::vector<double> sc = numbers(value, 2, what);
    v.scroll = std::array<double, 2>{sc[0], sc[1]};
  } else if (k == "collapsed") {
    v.collapsed = boolean(value, what);
  } else if (k == "closed") {
    v.closed = boolean(value, what);
  } else if (kind == view_kind::ui_index) {
    const double d = number(value, what);
    if (d != std::floor(d) || d < -1 || d > 1e6) {
      fail(value, what + " must be an item index, from 0 (or -1 for none)");
    }
    v.index = static_cast<int64_t>(d);
  } else if (kind == view_kind::ui_number) {
    if (value.is_sequence()) {
      if (value.children.empty() || value.children.size() > 4) {
        fail(value, what + " must be a number or a list of 1 to 4 numbers");
      }
      v.numbers = numbers(value, value.children.size(), what);
    } else {
      v.numbers = std::vector<double>{number(value, what)};
    }
  } else if (kind == view_kind::ui_text || kind == view_kind::ui_color) {
    if (!value.is_scalar()) {
      fail(value, what + " must be text");
    }
    if (kind == view_kind::ui_color && !is_color(value.text)) {
      fail(value, what + " must be a colour, \"#RRGGBB\" or \"#RRGGBBAA\"");
    }
    v.text = value.text;
  } else {
    v.flag = boolean(value, what);
  }
}

bool is_ui(view_kind kind) {
  return kind >= view_kind::ui_window;
}

void index(const node& n, const std::string& path, std::map<std::string, const node*, std::less<>>& out) {
  out[path] = &n;
  for (size_t i = 0; i < n.children.size(); ++i) {
    index(n.children[i], path + "/children/" + std::to_string(i), out);
  }
}

/// Shortest text that reads back as `v`.
std::string num(double v) {
  char buf[40];
  for (int digits = 6; digits <= 17; ++digits) {
    std::snprintf(buf, sizeof(buf), "%.*g", digits, v);
    if (std::strtod(buf, nullptr) == v) {
      break;
    }
  }
  return buf;
}

/// A YAML double-quoted scalar.
std::string quoted(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
      out += buf;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

} // namespace

std::map<std::string, const node*, std::less<>> element_paths(const node& root) {
  std::map<std::string, const node*, std::less<>> out;
  index(root, "root", out);
  return out;
}

view_map bind_view(const yaml_node& view, const std::map<std::string, const node*, std::less<>>& paths) {
  if (!view.is_mapping()) {
    fail(view, "must be a mapping of element paths, like root/children/1: {x: [0, 10]}");
  }
  view_map out;
  for (size_t i = 0; i < view.keys.size(); ++i) {
    const auto it = paths.find(view.keys[i].text);
    const view_kind kind = it == paths.end() ? view_kind::none : it->second->cls->viewable();
    if (kind == view_kind::none) {
      continue; // saved for an element that is no longer there
    }
    const yaml_node& entry = view.children[i];
    if (!entry.is_mapping()) {
      fail(entry, "the entry of " + view.keys[i].text + " must be a mapping, like {x: [0, 10]}");
    }
    const std::vector<std::string_view> allowed = keys_of(kind);
    element_view v;
    for (size_t k = 0; k < entry.keys.size(); ++k) {
      const yaml_node& key = entry.keys[k];
      const yaml_node& value = entry.children[k];
      const std::string what = view.keys[i].text + "." + key.text;
      if (std::find(allowed.begin(), allowed.end(), key.text) == allowed.end()) {
        std::string list;
        for (std::string_view a : allowed) {
          list += (list.empty() ? "" : ", ") + std::string(a);
        }
        fail(
            key,
            "unknown key '" + key.text + "'" + did_you_mean(key.text, allowed) + " for " +
                std::string(it->second->cls->name()) + " (keys: " + list + ")");
      }
      if (is_ui(kind)) {
        ui_key(kind, key, value, what, v);
      } else if (key.text == "x") {
        v.x = range(value, what);
      } else if (key.text == "y") {
        v.y = range(value, what);
      } else if (key.text == "y2") {
        v.y2 = range(value, what);
      } else if (key.text == "z") {
        v.z = range(value, what);
      } else if (key.text == "hidden") {
        if (!value.is_sequence()) {
          fail(value, what + " must be a list of legend labels");
        }
        for (const yaml_node& label : value.children) {
          if (!label.is_scalar()) {
            fail(label, what + " must be a list of legend labels");
          }
          v.hidden.push_back(label.text);
        }
      } else if (key.text == "zoom") {
        const double z = number(value, what);
        if (z < 0.25 || z > 8.0) {
          fail(value, what + " must be between 0.25 and 8");
        }
        v.zoom = z;
      } else if (key.text == "center") {
        const std::vector<double> c = numbers(value, 2, what);
        v.center = std::array<double, 2>{c[0], c[1]};
      } else if (key.text == "rotation") {
        const std::vector<double> q = numbers(value, 4, what);
        const double norm = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (norm < 1e-9) {
          fail(value, what + " must be a quaternion [x, y, z, w], not all 0");
        }
        v.rotation = std::array<double, 4>{q[0] / norm, q[1] / norm, q[2] / norm, q[3] / norm};
      }
    }
    if (kind == view_kind::ui_window && v.size && ((*v.size)[0] == 0 || (*v.size)[1] == 0)) {
      fail(entry, "the entry of " + view.keys[i].text + " needs both width and height, or neither");
    }
    if (!v.empty()) {
      out[view.keys[i].text] = std::move(v);
    }
  }
  return out;
}

std::string write_view(const view_map& views, const std::map<std::string, view_kind, std::less<>>& kinds) {
  std::string out;
  for (const auto& [path, v] : views) {
    if (v.empty()) {
      continue;
    }
    std::string fields;
    const auto add = [&](const std::string& field) { fields += (fields.empty() ? "" : ", ") + field; };
    const auto kind_it = kinds.find(path);
    const view_kind kind = kind_it == kinds.end() ? view_kind::none : kind_it->second;
    if (v.position) {
      add("x: " + num((*v.position)[0]) + ", y: " + num((*v.position)[1]));
    }
    if (v.size) {
      add("width: " + num((*v.size)[0]) + ", height: " + num((*v.size)[1]));
    }
    if (v.collapsed) {
      add(std::string("collapsed: ") + (*v.collapsed ? "true" : "false"));
    }
    if (v.scroll) {
      add("scroll: [" + num((*v.scroll)[0]) + ", " + num((*v.scroll)[1]) + "]");
    }
    if (v.flag) {
      add(std::string(flag_key(kind)) + ": " + (*v.flag ? "true" : "false"));
    }
    if (v.closed) {
      add(std::string("closed: ") + (*v.closed ? "true" : "false"));
    }
    if (v.index) {
      add("selected: " + std::to_string(*v.index));
    }
    if (v.numbers) {
      if (v.numbers->size() == 1) {
        add("value: " + num(v.numbers->front()));
      } else {
        std::string list;
        for (const double d : *v.numbers) {
          list += (list.empty() ? "" : ", ") + num(d);
        }
        add("value: [" + list + "]");
      }
    }
    if (v.text) {
      add("value: " + quoted(*v.text));
    }
    const auto add_range = [&](const char* name, const std::optional<view_range>& r) {
      if (r) {
        add(std::string(name) + ": [" + num(r->min) + ", " + num(r->max) + "]");
      }
    };
    add_range("x", v.x);
    add_range("y", v.y);
    add_range("y2", v.y2);
    add_range("z", v.z);
    if (!v.hidden.empty()) {
      std::string list;
      for (const std::string& h : v.hidden) {
        list += (list.empty() ? "" : ", ") + quoted(h);
      }
      add("hidden: [" + list + "]");
    }
    if (v.zoom) {
      add("zoom: " + num(*v.zoom));
    }
    if (v.center) {
      add("center: [" + num((*v.center)[0]) + ", " + num((*v.center)[1]) + "]");
    }
    if (v.rotation) {
      const auto& q = *v.rotation;
      add("rotation: [" + num(q[0]) + ", " + num(q[1]) + ", " + num(q[2]) + ", " + num(q[3]) + "]");
    }
    out += path + ": {" + fields + "}\n";
  }
  return out;
}

} // namespace bdg::nymph
