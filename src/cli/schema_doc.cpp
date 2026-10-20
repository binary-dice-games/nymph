// MIT License © 2026 Binary Dice Games
/// @file schema_doc.cpp
/// @brief schema_text() / schema_markdown().
#include "cli/schema_doc.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <sstream>

namespace bdg::nymph {

namespace {

std::string number(double v) {
  char buf[32];
  const auto r = std::to_chars(buf, buf + sizeof(buf), v);
  return std::string(buf, r.ptr);
}

/// "Plot" / "Plot, Plot3D" / "root, VerticalLayout" for where `cls` goes.
std::string goes_in(const element_class& cls, const registry& reg) {
  std::string out = cls.fills_slot(kFigureSlot) ? "root" : "";
  const std::string parents = reg.names_accepting(cls);
  if (!parents.empty()) {
    out += (out.empty() ? "" : ", ") + parents;
  }
  return out;
}

/// Names of the classes `cls` takes as children, or "".
std::string takes(const element_class& cls, const registry& reg) {
  return reg.names_taken_by(cls);
}

/// "one of: A, B" / "combine with '|': A, B" / "16 to 4096" / "".
std::string constraint(const field_desc& f) {
  if (f.names != nullptr) {
    return std::string(f.type == field_type::flags ? "combine with '|': " : "one of: ") + f.names->names();
  }
  if (f.limits) {
    return number(f.limits->min) + " to " + number(f.limits->max);
  }
  return "";
}

std::string md_escape(std::string s) {
  std::string out;
  for (char c : s) {
    if (c == '|') {
      out += "\\|";
    } else if (c == '\n') {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

} // namespace

std::string format_default(const field_desc& f) {
  if (const auto* b = std::get_if<bool>(&f.default_value)) {
    return *b ? "true" : "false";
  }
  if (const auto* i = std::get_if<int64_t>(&f.default_value)) {
    if (f.names != nullptr) {
      return f.type == field_type::flags ? f.names->format_flags(*i) : f.names->format_enum(*i);
    }
    return std::to_string(*i);
  }
  if (const auto* d = std::get_if<double>(&f.default_value)) {
    return number(*d);
  }
  if (const auto* s = std::get_if<std::string>(&f.default_value)) {
    return "\"" + *s + "\"";
  }
  return "[]";
}

std::vector<const element_class*> classes_in_doc_order(const registry& reg) {
  std::vector<const element_class*> all = reg.classes(); // by name
  std::vector<std::string> slots{std::string(kFigureSlot)};
  for (const element_class* c : all) {
    if (std::find(slots.begin(), slots.end(), c->slot()) == slots.end()) {
      slots.emplace_back(c->slot());
    }
  }
  std::sort(slots.begin() + 1, slots.end());
  std::vector<const element_class*> out;
  for (const std::string& slot : slots) {
    for (const element_class* c : all) {
      if (c->slot() == slot) {
        out.push_back(c);
      }
    }
  }
  return out;
}

std::string schema_text(const element_class& cls, const registry& reg) {
  std::ostringstream os;
  os << cls.name() << "\n";
  os << "  " << cls.description() << "\n";
  os << "  goes in: " << goes_in(cls, reg) << "\n";
  const std::string children = takes(cls, reg);
  if (!children.empty()) {
    os << "  children: " << children << "\n";
  }
  if (cls.fields().empty()) {
    os << "  fields: none\n";
  }
  for (const field_desc& f : cls.fields()) {
    os << "  " << f.name << ": " << type_name(f.type) << ", default " << format_default(f);
    const std::string c = constraint(f);
    if (!c.empty()) {
      os << "; " << c;
    }
    os << "\n      " << f.description << "\n";
  }
  return os.str();
}

std::string schema_text(const registry& reg) {
  std::string out;
  for (const element_class* c : classes_in_doc_order(reg)) {
    out += (out.empty() ? "" : "\n") + schema_text(*c, reg);
  }
  return out;
}

namespace {

/// The conventions every page of the reference relies on.
constexpr const char* kMarkdownConventions =
    "Every element is a mapping with a `type`, any of the fields below, and\n"
    "`children` when it takes them. List fields also take a `$column`\n"
    "(or `$table:column`) reference; text fields take one too (the cells\n"
    "joined by newlines).\n";

/// The `## Name` section of one class.
void write_class_section(std::ostream& os, const element_class& c, const registry& reg) {
  os << "\n## " << c.name() << "\n\n" << md_escape(std::string(c.description())) << "\n\n";
  os << "Goes in: " << goes_in(c, reg) << ".";
  const std::string children = takes(c, reg);
  if (!children.empty()) {
    os << " Children: " << children << ".";
  }
  os << "\n";
  if (c.fields().empty()) {
    os << "\nNo fields.\n";
    return;
  }
  os << "\n| Field | Type | Default | Description |\n|---|---|---|---|\n";
  for (const field_desc& f : c.fields()) {
    std::string desc = md_escape(f.description);
    const std::string extra = constraint(f);
    if (!extra.empty()) {
      desc += " (" + md_escape(extra) + ")";
    }
    os << "| `" << f.name << "` | " << type_name(f.type) << " | `" << md_escape(format_default(f)) << "` | " << desc
       << " |\n";
  }
}

/// What goes on the page of a `figure` class: the classes it takes,
/// directly or not, that are not `figure` classes themselves (those have
/// pages of their own).
struct page_members {
  std::vector<const element_class*> inner; ///< inlined after the class, in reading order
  std::vector<const element_class*> linked; ///< `figure` classes it takes, linked
};

page_members members_of(const element_class& top, const registry& reg) {
  const std::vector<const element_class*> all = classes_in_doc_order(reg);
  std::set<const element_class*> seen;
  std::set<const element_class*> linked;
  std::vector<const element_class*> queue{&top};
  for (size_t i = 0; i < queue.size(); ++i) {
    for (const std::string& slot : queue[i]->accepts()) {
      for (const element_class* c : all) {
        if (!c->fills_slot(slot)) {
          continue;
        }
        if (c->fills_slot(kFigureSlot)) {
          linked.insert(c);
        } else if (seen.insert(c).second) {
          queue.push_back(c);
        }
      }
    }
  }
  page_members out;
  for (const element_class* c : all) {
    if (seen.count(c) != 0) {
      out.inner.push_back(c);
    }
    if (linked.count(c) != 0) {
      out.linked.push_back(c);
    }
  }
  return out;
}

} // namespace

std::string schema_markdown(const registry& reg) {
  std::ostringstream os;
  os << "# nymph element reference\n\n"
     << "Generated by `nymph schema --markdown`; do not edit by hand (a test\n"
     << "checks that this file matches the program).\n\n"
     << kMarkdownConventions;
  for (const element_class* c : classes_in_doc_order(reg)) {
    write_class_section(os, *c, reg);
  }
  return os.str();
}

std::vector<doc_page> schema_markdown_pages(const registry& reg) {
  std::vector<const element_class*> tops;
  for (const element_class* c : classes_in_doc_order(reg)) {
    if (c->fills_slot(kFigureSlot)) {
      tops.push_back(c);
    }
  }
  std::sort(tops.begin(), tops.end(), [](const element_class* a, const element_class* b) {
    return a->name() < b->name();
  });

  std::ostringstream root;
  root << "# nymph element reference\n\n"
       << "Generated by `nymph schema --markdown --paged docs` (every build of\n"
       << "`nymph` refreshes it; a test checks that these files match the\n"
       << "program). Do not edit by hand.\n\n"
       << kMarkdownConventions << "\n"
       << "The reference is in pages, one per kind of figure, so that only the\n"
       << "page of the figure being built needs reading. Open the page of the\n"
       << "element you put under `root`: it holds that element and every type\n"
       << "that goes inside it (a class diagram's classes and relations, a\n"
       << "plot's series, a screen's windows and widgets). Elements that fit in\n"
       << "many places (layouts, labels) have a page of their own, linked from\n"
       << "the pages that take them.\n\n"
       << "| Page | What it draws | Also documents |\n|---|---|---|\n";

  std::map<const element_class*, page_members> members;
  for (const element_class* top : tops) {
    members[top] = members_of(*top, reg);
  }

  std::vector<doc_page> pages;
  for (const element_class* top : tops) {
    page_members m = members[top];
    // A class that a linked page already documents (a Screen's popups take
    // the widgets its windows take) is read there, not repeated here. Two
    // pages that link each other (a Screen holds windows, a window holds a
    // Screen) split the shared classes: the page with fewer classes owns
    // them, ties going to the lower name.
    std::set<const element_class*> elsewhere;
    for (const element_class* other : m.linked) {
      const page_members& theirs = members[other];
      const bool mutual = std::find(theirs.linked.begin(), theirs.linked.end(), top) != theirs.linked.end();
      const bool owns = theirs.inner.size() < m.inner.size() ||
          (theirs.inner.size() == m.inner.size() && other->name() < top->name());
      if (!mutual || owns) {
        elsewhere.insert(theirs.inner.begin(), theirs.inner.end());
      }
    }
    m.inner.erase(
        std::remove_if(m.inner.begin(), m.inner.end(), [&](const element_class* c) { return elsewhere.count(c) != 0; }),
        m.inner.end());
    const std::string name(top->name());
    std::string also;
    for (const element_class* c : m.inner) {
      also += (also.empty() ? "" : ", ") + std::string(c->name());
    }
    root << "| [" << name << "](elements/" << name << ".md) | " << md_escape(std::string(top->description())) << " | "
         << also << " |\n";

    std::ostringstream os;
    os << "# " << name << " (nymph element reference)\n\n"
       << "Part of the [nymph element reference](../elements.md), which has the\n"
       << "conventions these tables follow.\n";
    if (!m.linked.empty()) {
      os << "\nOther elements it takes have their own pages:";
      for (size_t i = 0; i < m.linked.size(); ++i) {
        const std::string linked_name(m.linked[i]->name());
        os << (i == 0 ? " " : ", ") << "[" << linked_name << "](" << linked_name << ".md)";
      }
      os << ".\n";
    }
    write_class_section(os, *top, reg);
    for (const element_class* c : m.inner) {
      write_class_section(os, *c, reg);
    }
    pages.push_back({"elements/" + name + ".md", os.str()});
  }
  pages.insert(pages.begin(), doc_page{"elements.md", root.str()});
  return pages;
}

} // namespace bdg::nymph
