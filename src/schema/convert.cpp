// MIT License © 2026 Binary Dice Games
/// @file convert.cpp
/// @brief convert_value(): YAML scalars and lists to typed field values.
#include "schema/convert.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include "core/error.hpp"

namespace bdg::nymph {

namespace {

[[noreturn]] void fail(const yaml_node& n, std::string_view what, const std::string& problem) {
  // "PlotLine.color: 'x' is not a colour" / "PlotLine.color must be ..."
  throw error(n.line, n.column, std::string(what) + (problem.rfind(':', 0) == 0 ? "" : " ") + problem);
}

bool is_unquoted(const yaml_node& n) {
  return n.is_scalar() && !n.quoted() && n.style != scalar_style::literal && n.style != scalar_style::folded;
}

/// A quoted or block scalar where a number or boolean was expected gets a
/// hint, since it looks right to a reader.
std::string quoted_hint(const yaml_node& n) {
  return n.is_scalar() && !is_unquoted(n) ? " (remove the quotes)" : "";
}

std::optional<bool> parse_bool(std::string_view s) {
  for (std::string_view t : {"true", "True", "TRUE", "yes", "Yes", "YES", "on", "On", "ON"}) {
    if (s == t) {
      return true;
    }
  }
  for (std::string_view f : {"false", "False", "FALSE", "no", "No", "NO", "off", "Off", "OFF"}) {
    if (s == f) {
      return false;
    }
  }
  return std::nullopt;
}

std::optional<int64_t> parse_int(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
  }
  int64_t v = 0;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return v;
}

std::optional<double> parse_double(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
  }
  double v = 0.0;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size() || !std::isfinite(v)) {
    return std::nullopt;
  }
  return v;
}

bool is_null(const yaml_node& n) {
  return is_unquoted(n) &&
      (n.text.empty() || n.text == "~" || n.text == "null" || n.text == "Null" || n.text == "NULL");
}

std::string format_number(double v) {
  char buf[32];
  const auto r = std::to_chars(buf, buf + sizeof(buf), v);
  return std::string(buf, r.ptr);
}

void check_range(const yaml_node& n, const field_desc& f, std::string_view what, double v) {
  if (f.limits && (v < f.limits->min || v > f.limits->max)) {
    fail(n, what, "must be between " + format_number(f.limits->min) + " and " + format_number(f.limits->max));
  }
}

int64_t to_integer(const yaml_node& n, const field_desc& f, std::string_view what) {
  const std::optional<int64_t> v = is_unquoted(n) ? parse_int(n.text) : std::nullopt;
  if (!v) {
    fail(n, what, "must be a whole number, not '" + n.text + "'" + quoted_hint(n));
  }
  if (*v < std::numeric_limits<int32_t>::min() || *v > std::numeric_limits<int32_t>::max()) {
    fail(n, what, "is out of range: '" + n.text + "'");
  }
  check_range(n, f, what, static_cast<double>(*v));
  return *v;
}

double to_number(const yaml_node& n, const field_desc& f, std::string_view what) {
  const std::optional<double> v = is_unquoted(n) ? parse_double(n.text) : std::nullopt;
  if (!v) {
    fail(n, what, "must be a number, not '" + n.text + "'" + quoted_hint(n));
  }
  check_range(n, f, what, *v);
  return *v;
}

value scalar_value(const yaml_node& n, const field_desc& f, std::string_view what) {
  switch (f.type) {
    case field_type::boolean: {
      const std::optional<bool> b = is_unquoted(n) ? parse_bool(n.text) : std::nullopt;
      if (!b) {
        fail(n, what, "must be true or false, not '" + n.text + "'" + quoted_hint(n));
      }
      return *b;
    }
    case field_type::integer:
      return to_integer(n, f, what);
    case field_type::number:
      return to_number(n, f, what);
    case field_type::text: {
      std::string text = n.text.rfind("$$", 0) == 0 ? n.text.substr(1) : n.text;
      if (f.check != nullptr) {
        const std::string problem = f.check(text);
        if (!problem.empty()) {
          fail(n, what, problem);
        }
      }
      return text;
    }
    case field_type::color:
      if (n.text.empty() && is_unquoted(n)) {
        fail(n, what, "is empty; an unquoted '#' starts a YAML comment, so write the colour in quotes: \"#RRGGBB\"");
      }
      if (!n.text.empty() && !is_hex_color(n.text)) {
        fail(n, what, ": '" + n.text + "' is not a colour; write \"#RRGGBB\" or \"#RRGGBBAA\" in quotes");
      }
      return n.text;
    case field_type::enumeration: {
      if (const std::optional<int64_t> i = is_unquoted(n) ? parse_int(n.text) : std::nullopt) {
        if (!f.names->has_value(*i)) {
          fail(n, what, "has no value " + n.text + " (one of: " + f.names->names() + ")");
        }
        return *i;
      }
      if (const std::optional<int64_t> v = f.names->find(n.text)) {
        return *v;
      }
      fail(n, what, ": unknown value '" + n.text + "' (one of: " + f.names->names() + ")");
    }
    case field_type::flags: {
      if (const std::optional<int64_t> i = is_unquoted(n) ? parse_int(n.text) : std::nullopt) {
        return *i;
      }
      std::string unknown;
      if (const std::optional<int64_t> v = f.names->parse_flags(n.text, unknown)) {
        return *v;
      }
      fail(n, what, ": unknown value '" + unknown + "' (combine with '|': " + f.names->names() + ")");
    }
    case field_type::number_array:
    case field_type::integer_array:
      fail(n, what, "must be a list of numbers, like [1, 2, 3], or a $column reference");
  }
  fail(n, what, "has an unsupported type");
}

} // namespace

bool is_hex_color(std::string_view text) {
  if ((text.size() != 7 && text.size() != 9) || text[0] != '#') {
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }
  return true;
}

value convert_value(const yaml_node& n, const field_desc& f, std::string_view what) {
  const bool is_list_type = f.type == field_type::number_array || f.type == field_type::integer_array;
  if (n.is_mapping() || (n.is_sequence() && !is_list_type)) {
    fail(
        n,
        what,
        std::string("must be a single value (") + type_name(f.type) + "), not a " +
            (n.is_mapping() ? "mapping" : "list"));
  }
  if (!is_list_type) {
    return scalar_value(n, f, what);
  }
  if (!n.is_sequence()) {
    return scalar_value(n, f, what); // reports "must be a list"
  }
  if (f.type == field_type::number_array) {
    std::vector<double> out;
    out.reserve(n.children.size());
    for (const yaml_node& item : n.children) {
      if (!item.is_scalar()) {
        fail(item, what, "items must be numbers, not lists or mappings");
      }
      out.push_back(is_null(item) ? std::numeric_limits<double>::quiet_NaN() : to_number(item, f, what));
    }
    return out;
  }
  std::vector<int64_t> out;
  out.reserve(n.children.size());
  for (const yaml_node& item : n.children) {
    if (!item.is_scalar()) {
      fail(item, what, "items must be whole numbers, not lists or mappings");
    }
    out.push_back(to_integer(item, f, what));
  }
  return out;
}

} // namespace bdg::nymph
