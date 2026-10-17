// MIT License © 2026 Binary Dice Games
/// @file schema.cpp
/// @brief element_class, registry and field-type helpers.
#include <stdexcept>
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

// ── field_type ───────────────────────────────────────────────────────────────

const char* type_name(field_type type) {
  switch (type) {
    case field_type::boolean:
      return "true/false";
    case field_type::integer:
      return "integer";
    case field_type::number:
      return "number";
    case field_type::text:
      return "text";
    case field_type::color:
      return "colour";
    case field_type::enumeration:
      return "name";
    case field_type::flags:
      return "flags";
    case field_type::number_array:
      return "list of numbers";
    case field_type::integer_array:
      return "list of integers";
  }
  return "?";
}

// ── element_class ────────────────────────────────────────────────────────────

element_class::element_class(
    std::string name,
    std::string slot,
    std::vector<std::string> accepts,
    std::vector<field_desc> fields,
    std::string description)
    : name_(std::move(name)),
      slot_(std::move(slot)),
      accepts_(std::move(accepts)),
      fields_(std::move(fields)),
      description_(std::move(description)) {}

bool element_class::accepts_slot(std::string_view slot) const {
  for (const std::string& s : accepts_) {
    if (s == slot) {
      return true;
    }
  }
  return false;
}

int element_class::field_index(std::string_view name) const {
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

node element_class::make_node() const {
  node n;
  n.cls = this;
  n.values.reserve(fields_.size());
  for (const field_desc& f : fields_) {
    n.values.push_back(f.default_value);
  }
  return n;
}

// ── registry ─────────────────────────────────────────────────────────────────

void registry::add(std::unique_ptr<element_class> cls) {
  const std::string name(cls->name());
  if (classes_.count(name) != 0) {
    throw std::logic_error("element class '" + name + "' is registered twice");
  }
  const auto fields = cls->fields();
  for (size_t i = 0; i < fields.size(); ++i) {
    for (size_t j = i + 1; j < fields.size(); ++j) {
      if (fields[i].name == fields[j].name) {
        throw std::logic_error("element class '" + name + "' has two fields named '" + fields[i].name + "'");
      }
    }
  }
  classes_.emplace(name, std::move(cls));
}

const element_class* registry::find(std::string_view name) const {
  const auto it = classes_.find(name);
  return it == classes_.end() ? nullptr : it->second.get();
}

std::vector<const element_class*> registry::classes() const {
  std::vector<const element_class*> out;
  for (const auto& [name, cls] : classes_) {
    out.push_back(cls.get());
  }
  return out;
}

std::string registry::names_in_slot(std::string_view slot) const {
  std::string out;
  for (const auto& [name, cls] : classes_) {
    if (cls->slot() == slot) {
      out += (out.empty() ? "" : ", ") + name;
    }
  }
  return out;
}

std::string registry::names_accepting(std::string_view slot) const {
  std::string out;
  for (const auto& [name, cls] : classes_) {
    if (cls->accepts_slot(slot)) {
      out += (out.empty() ? "" : ", ") + name;
    }
  }
  return out;
}

} // namespace bdg::nymph
