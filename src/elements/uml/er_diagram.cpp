// MIT License © 2026 Binary Dice Games
/// @file er_diagram.cpp
/// @brief `ERDiagram`, `Entity` and `Relationship`: database structure as
///        an entity-relationship diagram in crow's foot notation (mermaid's
///        erDiagram; spec/03_uml_diagrams/DESIGN.md section 3).
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& cardinality_names() {
  static const name_table t{
      {"ExactlyOne", ImUmlCardinality_ExactlyOne},
      {"ZeroOrOne", ImUmlCardinality_ZeroOrOne},
      {"ZeroOrMore", ImUmlCardinality_ZeroOrMore},
      {"OneOrMore", ImUmlCardinality_OneOrMore},
  };
  return t;
}

/// One attribute, as written: `type name [PK|FK|UK[, ...]] ["comment"]`.
struct attribute {
  std::string type, name, comment;
  int keys = 0;
};

/// Parses an attribute; nullopt and `problem` set when it is malformed.
std::optional<attribute> parse_attribute(std::string_view text, std::string& problem) {
  attribute a;
  // A trailing double-quoted comment.
  const size_t quote = text.find('"');
  if (quote != std::string_view::npos) {
    const size_t close = text.rfind('"');
    if (close == quote || close + 1 != text.find_last_not_of(" \t") + 1) {
      problem = "has an unterminated comment; write the comment last, in double quotes";
      return std::nullopt;
    }
    a.comment = std::string(text.substr(quote + 1, close - quote - 1));
    text = text.substr(0, quote);
  }
  std::vector<std::string> words;
  std::string word;
  for (char ch : text) {
    if (ch == ' ' || ch == '\t' || ch == ',') {
      if (!word.empty()) {
        words.push_back(word);
        word.clear();
      }
    } else {
      word += ch;
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }
  if (words.size() < 2) {
    problem = "needs a type and a name, like \"int id PK\"";
    return std::nullopt;
  }
  a.type = words[0];
  a.name = words[1];
  for (size_t i = 2; i < words.size(); ++i) {
    if (words[i] == "PK") {
      a.keys |= ImUmlAttributeKeys_PK;
    } else if (words[i] == "FK") {
      a.keys |= ImUmlAttributeKeys_FK;
    } else if (words[i] == "UK") {
      a.keys |= ImUmlAttributeKeys_UK;
    } else {
      problem = "has '" + words[i] + "' after the name; only the keys PK, FK and UK and a \"comment\" may follow";
      return std::nullopt;
    }
  }
  return a;
}

class entity_class : public item_class {
 public:
  entity_class()
      : item_class(
            "Entity",
            kUmlErItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that relationships and notes name."),
                    text_field("name", "", "Name shown in the header (the table name); empty shows the id."),
                    texts_field(
                        "attributes",
                        "One row per attribute, as in mermaid: \"type name\", then optional keys PK, FK, UK "
                        "(comma-separated) and a \"comment\" in double quotes: \"int customer_id FK \\\"buyer\\\"\"."),
                    rank_field(),
                },
                node_color_fields()),
            "An entity (a database table): a header with its name over a row per attribute.") {
    id_ = field("id");
    name_ = field("name");
    attributes_ = field("attributes");
  }

  void check(const node& n) const override {
    const auto& attrs = n.get<std::vector<std::string>>(attributes_);
    for (size_t i = 0; i < attrs.size(); ++i) {
      std::string problem;
      if (!parse_attribute(attrs[i], problem)) {
        fail_at(n, attributes_, "item " + std::to_string(i + 1) + " '" + attrs[i] + "' " + problem);
      }
    }
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    ImUml::BeginEntity(n.get<std::string>(id_).c_str(), or_null(n, name_));
    for (const std::string& text : n.get<std::vector<std::string>>(attributes_)) {
      std::string problem;
      if (const std::optional<attribute> a = parse_attribute(text, problem)) {
        ImUml::EntityAttribute(
            a->type.c_str(), a->name.c_str(), a->keys, a->comment.empty() ? nullptr : a->comment.c_str());
      }
    }
    ImUml::EndEntity();
  }

 private:
  size_t id_, name_, attributes_;
};

class relationship_class : public item_class {
 public:
  relationship_class()
      : item_class(
            "Relationship",
            kUmlErItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the first entity."),
                    text_field("to", "", "Id of the second entity."),
                    enum_field(
                        "from_cardinality",
                        ImUmlCardinality_ExactlyOne,
                        cardinality_names(),
                        "How many `from` entities one `to` relates to, drawn at the `from` end: ExactlyOne (||), "
                        "ZeroOrOne (o|), ZeroOrMore (o{), OneOrMore (|{)."),
                    enum_field(
                        "to_cardinality",
                        ImUmlCardinality_ZeroOrMore,
                        cardinality_names(),
                        "How many `to` entities one `from` relates to, drawn at the `to` end."),
                    text_field("label", "", "Verb phrase drawn beside the line (\"places\")."),
                    bool_field(
                        "identifying",
                        true,
                        "true: solid line (mermaid's --); false: dashed, a non-identifying relationship (..)."),
                },
                edge_layout_fields()),
            "A relationship between two entities, with the cardinality allowed at each end.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    from_card_ = field("from_cardinality");
    to_card_ = field("to_cardinality");
    label_ = field("label");
    identifying_ = field("identifying");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::Relationship(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        static_cast<ImUmlCardinality>(n.get<int64_t>(from_card_)),
        static_cast<ImUmlCardinality>(n.get<int64_t>(to_card_)),
        or_null(n, label_),
        n.get<bool>(identifying_) ? ImUmlRelationshipFlags_None : ImUmlRelationshipFlags_NonIdentifying);
  }

  size_t from_, to_, from_card_, to_card_, label_, identifying_;
};

class er_diagram_class : public diagram_class {
 public:
  er_diagram_class()
      : diagram_class(
            "ERDiagram",
            diagram_kind::er,
            kUmlErItemSlot,
            {},
            "An entity-relationship diagram of a database: entities (tables) with their attributes and keys, "
            "joined by relationships in crow's foot notation, laid out automatically. Children: Entity, "
            "Relationship, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
      } else if (is_a(child, "Relationship")) {
        const auto& rc = static_cast<const relationship_class&>(*child.cls);
        for (const size_t f : {rc.from_, rc.to_}) {
          const node& target = ids.resolve(child, f, child.get<std::string>(f));
          if (!is_a(target, "Entity")) {
            child.cls->fail_at(child, f, "must name an Entity, not a " + std::string(target.cls->name()));
          }
        }
      }
    });
  }
};

} // namespace

void register_er_diagram(registry& reg) {
  reg.add(std::make_unique<er_diagram_class>());
  reg.add(std::make_unique<entity_class>());
  reg.add(std::make_unique<relationship_class>());
}

} // namespace bdg::nymph::uml
