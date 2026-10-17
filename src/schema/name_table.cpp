// MIT License © 2026 Binary Dice Games
/// @file name_table.cpp
/// @brief name_table: parsing and printing named enum and flag values.
#include "schema/name_table.hpp"

namespace bdg::nymph {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }
  return s;
}

} // namespace

std::optional<int64_t> name_table::find(std::string_view name) const {
  for (const name_entry& e : entries_) {
    if (e.name == name) {
      return e.value;
    }
  }
  return std::nullopt;
}

bool name_table::has_value(int64_t value) const {
  for (const name_entry& e : entries_) {
    if (e.value == value) {
      return true;
    }
  }
  return false;
}

std::string name_table::names() const {
  std::string out;
  for (const name_entry& e : entries_) {
    out += (out.empty() ? "" : ", ") + e.name;
  }
  return out;
}

std::optional<int64_t> name_table::parse_flags(std::string_view text, std::string& unknown) const {
  int64_t result = 0;
  size_t pos = 0;
  for (;;) {
    const size_t bar = text.find('|', pos);
    const std::string_view part = trim(text.substr(pos, bar == std::string_view::npos ? text.npos : bar - pos));
    const std::optional<int64_t> v = find(part);
    if (!v) {
      unknown = std::string(part);
      return std::nullopt;
    }
    result |= *v;
    if (bar == std::string_view::npos) {
      return result;
    }
    pos = bar + 1;
  }
}

std::string name_table::format_enum(int64_t value) const {
  for (const name_entry& e : entries_) {
    if (e.value == value) {
      return e.name;
    }
  }
  return std::to_string(value);
}

std::string name_table::format_flags(int64_t value) const {
  if (value == 0) {
    for (const name_entry& e : entries_) {
      if (e.value == 0) {
        return e.name;
      }
    }
    return "0";
  }
  std::string out;
  int64_t left = value;
  for (const name_entry& e : entries_) {
    if (e.value != 0 && (left & e.value) == e.value) {
      out += (out.empty() ? "" : "|") + e.name;
      left &= ~e.value;
    }
  }
  if (left != 0) {
    out += (out.empty() ? "" : "|") + std::to_string(left);
  }
  return out;
}

} // namespace bdg::nymph
