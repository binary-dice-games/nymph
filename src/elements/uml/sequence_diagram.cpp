// MIT License © 2026 Binary Dice Games
/// @file sequence_diagram.cpp
/// @brief `SequenceDiagram`, `Participant`, `Message`, `Fragment` and `Else`
///        (spec/03_uml_diagrams/DESIGN.md section 3).
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& participant_kind_names() {
  static const name_table t{
      {"Participant", ImUmlParticipantKind_Participant},
      {"Actor", ImUmlParticipantKind_Actor},
      {"Boundary", ImUmlParticipantKind_Boundary},
      {"Control", ImUmlParticipantKind_Control},
      {"Entity", ImUmlParticipantKind_Entity},
      {"Database", ImUmlParticipantKind_Database},
  };
  return t;
}

const name_table& message_kind_names() {
  static const name_table t{
      {"Sync", ImUmlMessageKind_Sync},
      {"Async", ImUmlMessageKind_Async},
      {"Reply", ImUmlMessageKind_Reply},
      {"Create", ImUmlMessageKind_Create},
      {"Destroy", ImUmlMessageKind_Destroy},
  };
  return t;
}

const name_table& fragment_kind_names() {
  static const name_table t{
      {"Loop", ImUmlFragmentKind_Loop},
      {"Alt", ImUmlFragmentKind_Alt},
      {"Opt", ImUmlFragmentKind_Opt},
      {"Par", ImUmlFragmentKind_Par},
      {"Critical", ImUmlFragmentKind_Critical},
      {"Break", ImUmlFragmentKind_Break},
      {"Group", ImUmlFragmentKind_Group},
  };
  return t;
}

class participant_class : public item_class {
 public:
  participant_class()
      : item_class(
            "Participant",
            kUmlSequenceItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that messages and notes name."),
                    text_field("name", "", "Name shown in the box; empty shows the id."),
                    enum_field(
                        "kind",
                        ImUmlParticipantKind_Participant,
                        participant_kind_names(),
                        "Participant is a box; Actor a stick figure; Boundary, Control, Entity the robustness "
                        "icons; Database a cylinder."),
                    bool_field(
                        "created",
                        false,
                        "true: the participant appears at the Create message that makes it, not at the top."),
                },
                node_color_fields()),
            "A participant: a column with its lifeline. Declare every participant before the first message; "
            "columns follow declaration order.") {
    id_ = field("id");
    name_ = field("name");
    kind_ = field("kind");
    created_ = field("created");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    ImUml::Participant(
        n.get<std::string>(id_).c_str(),
        or_null(n, name_),
        static_cast<ImUmlParticipantKind>(n.get<int64_t>(kind_)),
        n.get<bool>(created_) ? ImUmlParticipantFlags_Created : ImUmlParticipantFlags_None);
  }

  size_t id_, name_, kind_, created_;
};

class message_class : public item_class {
 public:
  message_class()
      : item_class(
            "Message",
            kUmlSequenceItemSlot,
            {},
            {
                text_field("from", "", "Id of the sending participant."),
                text_field("to", "", "Id of the receiving participant; the same as from for a self message."),
                text_field("text", "", "Text drawn above the arrow."),
                enum_field(
                    "kind",
                    ImUmlMessageKind_Sync,
                    message_kind_names(),
                    "Sync: solid line, filled arrow. Async: open arrow. Reply: dashed, open arrow. Create: dashed "
                    "arrow to a participant with created: true. Destroy: the target's lifeline ends in a cross."),
                bool_field("activate", false, "Start an activation bar on `to` (mermaid's +)."),
                bool_field("deactivate", false, "End the latest activation bar of `from` (mermaid's -)."),
                color_field("color", "Line colour; empty for the diagram's."),
            },
            "A message between two participants, one row of the diagram. Rows follow the order of the children.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    text_ = field("text");
    kind_ = field("kind");
    activate_ = field("activate");
    deactivate_ = field("deactivate");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    ImUmlMessageFlags flags = 0;
    flags |= n.get<bool>(activate_) ? ImUmlMessageFlags_Activate : 0;
    flags |= n.get<bool>(deactivate_) ? ImUmlMessageFlags_Deactivate : 0;
    ImUml::Message(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        n.get<std::string>(text_).c_str(),
        static_cast<ImUmlMessageKind>(n.get<int64_t>(kind_)),
        flags);
  }

  size_t from_, to_, text_, kind_, activate_, deactivate_;
};

class fragment_class : public item_class {
 public:
  fragment_class()
      : item_class(
            "Fragment",
            kUmlSequenceItemSlot,
            {kUmlSequenceItemSlot, kUmlNoteSlot},
            {
                enum_field(
                    "kind",
                    ImUmlFragmentKind_Loop,
                    fragment_kind_names(),
                    "The operator shown in the frame's tab (loop, alt, opt, par, critical, break); Group is a "
                    "filled box with no tab (mermaid's rect)."),
                text_field("label", "", "Condition or title, shown as [label] next to the tab."),
                texts_field(
                    "over",
                    "Ids of the leftmost and rightmost participants the frame covers; empty covers those its "
                    "messages touch."),
                color_field("fill_color", "Group only: the box's colour; empty for the theme's."),
            },
            "A combined fragment around messages: loop, alt (with Else), opt, par, critical, break or group.",
            {"over"}) {
    kind_ = field("kind");
    label_ = field("label");
    over_ = field("over");
    fill_ = field("fill_color");
  }

  void draw(const node& n, draw_context& dc) const override {
    const auto& over = n.get<std::vector<std::string>>(over_);
    ImUml::SetNextItemColors(color_u32(n.get<std::string>(fill_)), 0);
    ImUml::BeginFragment(
        static_cast<ImUmlFragmentKind>(n.get<int64_t>(kind_)),
        n.get<std::string>(label_).c_str(),
        over.size() == 2 ? over[0].c_str() : nullptr,
        over.size() == 2 ? over[1].c_str() : nullptr);
    dc.draw_children(n);
    ImUml::EndFragment();
  }

  size_t kind_, label_, over_, fill_;
};

class else_class : public item_class {
 public:
  else_class()
      : item_class(
            "Else",
            kUmlSequenceItemSlot,
            {},
            {text_field("label", "", "Condition of the next operand, shown as [label].")},
            "Separates the operands of an Alt, Par or Critical fragment.") {
    label_ = field("label");
  }

  void draw(const node& n, draw_context&) const override {
    ImUml::FragmentElse(n.get<std::string>(label_).c_str());
  }

  size_t label_;
};

/// Replays the diagram in time order to check what depends on it.
class sequence_checker {
 public:
  explicit sequence_checker(const id_table& ids) : ids_(ids) {}

  void diagram(const node& n) {
    bool seen_other = false;
    for (const node& child : n.children) {
      if (is_a(child, "Participant")) {
        if (seen_other) {
          throw error(
              child.line,
              child.column,
              "Participant '" + id_of(child) + "' is declared after the first message or note; declare every "
              "participant first (their order is the column order)");
        }
        const auto& pc = static_cast<const participant_class&>(*child.cls);
        if (child.get<bool>(pc.created_)) {
          pending_.emplace(id_of(child), &child);
        }
        continue;
      }
      seen_other = true;
      item(child, nullptr, 0, 0);
    }
    for (const auto& [id, p] : pending_) {
      const auto& pc = static_cast<const participant_class&>(*p->cls);
      p->cls->fail_at(*p, pc.created_, "is true but no Create message to '" + id + "' follows");
    }
  }

 private:
  void participant_ref(const node& n, size_t field, const std::string& id) {
    const node& target = ids_.resolve(n, field, id);
    if (!is_a(target, "Participant")) {
      n.cls->fail_at(n, field, "must name a Participant, not a " + std::string(target.cls->name()));
    }
    if (destroyed_.count(id) != 0) {
      n.cls->fail_at(n, field, "names '" + id + "', destroyed at line " + std::to_string(destroyed_[id]));
    }
    if (pending_.count(id) != 0) {
      n.cls->fail_at(n, field, "names '" + id + "' before the Create message that makes it");
    }
  }

  void item(const node& n, const node* fragment, size_t index, size_t count) {
    if (is_a(n, "Participant")) {
      throw error(
          n.line, n.column, "Participant '" + id_of(n) + "' must be a direct child of the SequenceDiagram");
    }
    if (is_a(n, "Note")) {
      check_note(n, true);
      for (const char* f : {"over", "attach"}) {
        const size_t fi = static_cast<size_t>(n.cls->field_index(f));
        for (const std::string& id : n.get<std::vector<std::string>>(fi)) {
          participant_ref(n, fi, id);
        }
      }
      return;
    }
    if (is_a(n, "Else")) {
      bool ok = false;
      if (fragment != nullptr) {
        const auto& fc = static_cast<const fragment_class&>(*fragment->cls);
        const int64_t k = fragment->get<int64_t>(fc.kind_);
        ok = k == ImUmlFragmentKind_Alt || k == ImUmlFragmentKind_Par || k == ImUmlFragmentKind_Critical;
      }
      if (!ok) {
        throw error(n.line, n.column, "Else separates the operands of an Alt, Par or Critical Fragment; it cannot go here");
      }
      if (index == 0 || index + 1 == count) {
        throw error(
            n.line, n.column, "Else must sit between two operands: not first or last in its Fragment");
      }
      return;
    }
    if (is_a(n, "Fragment")) {
      const auto& fc = static_cast<const fragment_class&>(*n.cls);
      const auto& over = n.get<std::vector<std::string>>(fc.over_);
      if (!over.empty() && over.size() != 2) {
        n.cls->fail_at(n, fc.over_, "takes two participants (leftmost and rightmost), or none");
      }
      for (const std::string& id : over) {
        participant_ref(n, fc.over_, id);
      }
      for (size_t i = 0; i < n.children.size(); ++i) {
        item(n.children[i], &n, i, n.children.size());
      }
      return;
    }
    if (!is_a(n, "Message")) {
      return;
    }
    const auto& mc = static_cast<const message_class&>(*n.cls);
    const std::string& from = n.get<std::string>(mc.from_);
    const std::string& to = n.get<std::string>(mc.to_);
    const int64_t kind = n.get<int64_t>(mc.kind_);
    participant_ref(n, mc.from_, from);
    if (kind == ImUmlMessageKind_Create) {
      const node& target = ids_.resolve(n, mc.to_, to);
      if (!is_a(target, "Participant")) {
        n.cls->fail_at(n, mc.to_, "must name a Participant, not a " + std::string(target.cls->name()));
      }
      if (pending_.count(to) == 0) {
        n.cls->fail_at(
            n,
            mc.to_,
            "names '" + to + "', which is not waiting to be created; give it created: true and send this as its "
            "first message");
      }
      pending_.erase(to);
    } else {
      participant_ref(n, mc.to_, to);
    }
    if (n.get<bool>(mc.activate_)) {
      active_[to]++;
    }
    if (n.get<bool>(mc.deactivate_)) {
      if (active_[from] == 0) {
        n.cls->fail_at(n, mc.deactivate_, "ends an activation of '" + from + "', which has none open");
      }
      active_[from]--;
    }
    if (kind == ImUmlMessageKind_Destroy) {
      destroyed_[to] = n.line;
      active_[to] = 0;
    }
  }

  const id_table& ids_;
  std::map<std::string, const node*> pending_; ///< created: true, Create message not yet seen
  std::map<std::string, int> destroyed_; ///< id -> line of the Destroy message
  std::map<std::string, int> active_;
};

class sequence_diagram_class : public diagram_class {
 public:
  sequence_diagram_class()
      : diagram_class(
            "SequenceDiagram",
            diagram_kind::sequence,
            kUmlSequenceItemSlot,
            {
                bool_field("autonumber", false, "Number the messages (1., 2., ...)."),
                bool_field("mirror_participants", true, "Repeat the participant boxes at the bottom."),
            },
            "A UML sequence diagram: participants in columns, children in time order top to bottom. Children: "
            "Participant (first), Message, Fragment, Note.") {
    autonumber_ = field("autonumber");
    mirror_ = field("mirror_participants");
  }

 protected:
  void setup(const node& n) const override {
    ImUmlSequenceFlags flags = 0;
    flags |= n.get<bool>(autonumber_) ? ImUmlSequenceFlags_Autonumber : 0;
    flags |= n.get<bool>(mirror_) ? ImUmlSequenceFlags_MirrorParticipants : 0;
    ImUml::SetupSequence(flags);
  }

  void check_rules(const node& n, const id_table& ids) const override {
    sequence_checker(ids).diagram(n);
  }

 private:
  size_t autonumber_, mirror_;
};

} // namespace

void register_sequence_diagram(registry& reg) {
  reg.add(std::make_unique<sequence_diagram_class>());
  reg.add(std::make_unique<participant_class>());
  reg.add(std::make_unique<message_class>());
  reg.add(std::make_unique<fragment_class>());
  reg.add(std::make_unique<else_class>());
}

} // namespace bdg::nymph::uml
