// MIT License © 2026 Binary Dice Games
/// @file style_sheet.cpp
/// @brief mermaid's class and style statements for the class, state and ER
///        converters (spec/16_mermaid_import/DESIGN.md section 4).
#include <algorithm>
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

std::pair<std::string_view, std::string_view> split_word(std::string_view text) {
  text = trim(text);
  const size_t space = text.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return {text, {}};
  }
  return {text.substr(0, space), trim(text.substr(space))};
}

std::vector<std::string> names_of(std::string_view list) {
  std::vector<std::string> out;
  std::string_view l = trim(list);
  if (l.size() >= 2 && l.front() == '"' && l.back() == '"') {
    l = l.substr(1, l.size() - 2);
  }
  for (const std::string& part : split_outside_quotes(l, ',')) {
    if (!part.empty()) {
      out.push_back(part);
    }
  }
  return out;
}

} // namespace

bool style_sheet::read(std::string_view word, std::string_view rest, pos at, bool class_assigns, context& ctx) {
  if (word == "classDef") {
    const auto [names, styles] = split_word(rest);
    const style st = parse_style(styles, at, ctx);
    for (const std::string& name : names_of(names)) {
      auto it = std::find_if(defs_.begin(), defs_.end(), [&](const auto& d) { return d.first == name; });
      if (it == defs_.end()) {
        defs_.push_back({name, st});
      } else {
        merge_style(it->second, st);
      }
    }
    return true;
  }
  if ((word == "class" && class_assigns) || word == "cssClass") {
    std::string_view r = trim(rest);
    size_t split = std::string_view::npos;
    if (!r.empty() && r.front() == '"') {
      split = r.find('"', 1);
      split = split == std::string_view::npos ? split : split + 1;
    } else {
      split = r.find_last_of(" \t");
    }
    if (split == std::string_view::npos || split >= r.size()) {
      ctx.fail(at, std::string(word) + " needs ids and a class name: " + std::string(word) + " a,b name");
    }
    for (const std::string& id : names_of(r.substr(0, split))) {
      for (const std::string& cls : names_of(r.substr(split))) {
        assigned_.push_back({id, cls, at});
      }
    }
    return true;
  }
  if (word == "style") {
    const auto [id, styles] = split_word(rest);
    direct_.push_back({std::string(id), parse_style(styles, at, ctx), at});
    return true;
  }
  return false;
}

void style_sheet::assign(const std::string& id, const std::string& cls, pos at) {
  assigned_.push_back({id, cls, at});
}

style style_sheet::of(const std::string& id, context& ctx) const {
  style s;
  for (const auto& [name, st] : defs_) {
    if (name == "default") {
      merge_style(s, st);
    }
  }
  for (const assignment& a : assigned_) {
    if (a.id != id) {
      continue;
    }
    auto it = std::find_if(defs_.begin(), defs_.end(), [&](const auto& d) { return d.first == a.cls; });
    if (it == defs_.end()) {
      ctx.warn(a.at, "class '" + a.cls + "' has no classDef; ignored");
      continue;
    }
    merge_style(s, it->second);
  }
  for (const direct& d : direct_) {
    if (d.id == id) {
      merge_style(s, d.s);
    }
  }
  return s;
}

void style_sheet::check_ids(const std::vector<std::string>& ids, context& ctx) const {
  auto known = [&](const std::string& id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
  for (const assignment& a : assigned_) {
    if (!known(a.id)) {
      ctx.warn(a.at, "'" + a.id + "' is not in the diagram; its class is ignored");
    }
  }
  for (const direct& d : direct_) {
    if (!known(d.id)) {
      ctx.warn(d.at, "'" + d.id + "' is not in the diagram; its style is ignored");
    }
  }
}

} // namespace bdg::nymph::mermaid
