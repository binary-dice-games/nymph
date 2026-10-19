// MIT License © 2026 Binary Dice Games
/// @file field_value.cpp
/// @brief Typed values to one-line YAML (see field_value.hpp).
#include "edit/field_value.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"

namespace bdg::nymph {

namespace {

bool is_hex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// Whether `text` as a plain YAML scalar reads back as the same text.
bool plain_is_safe(std::string_view text) {
  if (text.empty() || text.front() == ' ' || text.back() == ' ') {
    return false;
  }
  // YAML 1.1 words a reader may turn into booleans or null.
  static constexpr std::string_view words[] = {"~",   "null", "Null", "NULL", "true", "True", "TRUE", "false",
                                               "False", "FALSE", "yes", "Yes",  "YES",  "no",   "No",   "NO",
                                               "on",   "On",   "ON",   "off",  "Off",  "OFF",  "y",    "n"};
  for (std::string_view w : words) {
    if (text == w) {
      return false;
    }
  }
  // First character: no YAML indicator.
  const unsigned char first = static_cast<unsigned char>(text.front());
  if (std::string_view("-?:,[]{}#&*!|>'\"%@`").find(static_cast<char>(first)) != std::string_view::npos) {
    return false;
  }
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x20 || c == 0x7f) {
      return false;
    }
    if (c == ':' && (i + 1 == text.size() || text[i + 1] == ' ')) {
      return false;
    }
    if (c == '#' && i > 0 && text[i - 1] == ' ') {
      return false;
    }
    if (c == ',' || c == '[' || c == ']' || c == '{' || c == '}') {
      return false; // would end or open a flow collection
    }
  }
  return true;
}

} // namespace

std::string encode_text(std::string_view text) {
  if (plain_is_safe(text)) {
    return std::string(text);
  }
  std::string out = "\"";
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

std::string encode_bool(bool value) {
  return value ? "true" : "false";
}

std::string encode_int(int64_t value) {
  return std::to_string(value);
}

std::string encode_number(double value) {
  if (!std::isfinite(value)) {
    return "0";
  }
  char buf[40];
  for (int digits = 6; digits <= 17; ++digits) {
    std::snprintf(buf, sizeof(buf), "%.*g", digits, value);
    if (std::strtod(buf, nullptr) == value) {
      break;
    }
  }
  return buf;
}

std::optional<std::string> encode_color(std::string_view color) {
  if ((color.size() != 7 && color.size() != 9) || color.front() != '#') {
    return std::nullopt;
  }
  for (size_t i = 1; i < color.size(); ++i) {
    if (!is_hex(color[i])) {
      return std::nullopt;
    }
  }
  return "\"" + std::string(color) + "\"";
}

std::string encode_enum(const field_desc& f, int64_t value) {
  return f.names != nullptr ? f.names->format_enum(value) : std::to_string(value);
}

std::string encode_flags(const field_desc& f, int64_t value) {
  const std::string text = f.names != nullptr ? f.names->format_flags(value) : std::to_string(value);
  return text.find('|') == std::string::npos ? text : "\"" + text + "\"";
}

std::optional<source_problem> check_source(std::string_view source) {
  try {
    bind_source(source, builtin_registry());
  } catch (const error& e) {
    return source_problem{e.line(), e.column(), e.what()};
  }
  return std::nullopt;
}

} // namespace bdg::nymph
