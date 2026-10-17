// MIT License © 2026 Binary Dice Games
/// @file test_uml_sequence.cpp
/// @brief UML plan steps 10-11: `SequenceDiagram`, `Participant`, `Message`,
///        `Fragment`, `Else` and sequence notes.
#include <gtest/gtest.h>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

const char* const kExample =
    "image: {width: 720, height: 560}\n" // 1
    "root:\n" // 2
    "  type: SequenceDiagram\n" // 3
    "  autonumber: true\n" // 4
    "  children:\n" // 5
    "    - {type: Participant, id: user, name: User, kind: Actor}\n" // 6
    "    - {type: Participant, id: web, name: Web app}\n" // 7
    "    - {type: Participant, id: db, name: Database, kind: Database}\n" // 8
    "    - {type: Message, from: user, to: web, text: \"POST /login\", activate: true}\n" // 9
    "    - type: Fragment\n" // 10
    "      kind: Alt\n" // 11
    "      label: valid password\n" // 12
    "      children:\n" // 13
    "        - {type: Message, from: web, to: db, text: load user}\n" // 14
    "        - {type: Message, from: db, to: web, text: row, kind: Reply}\n" // 15
    "        - {type: Else, label: otherwise}\n" // 16
    "        - {type: Message, from: web, to: user, text: \"401\", kind: Reply}\n" // 17
    "    - {type: Message, from: web, to: user, text: \"200 OK\", kind: Reply, deactivate: true}\n" // 18
    "    - {type: Note, text: session cookie set, over: [user, web]}\n"; // 19

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

TEST(uml_sequence, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kExample)), 2000u);
  expect_round_trip(kExample);
}

TEST(uml_sequence, ColumnsRowsActivationsAndFragments) {
  const drawn_diagram d = draw_and_inspect(kExample, {});
  ASSERT_EQ(d.columns.size(), 3u);
  EXPECT_LT(d.columns[0], d.columns[1]);
  EXPECT_LT(d.columns[1], d.columns[2]);
  ASSERT_EQ(d.activations.size(), 1u); // web, from message 1 to the 200 OK
  // Events (in order): msg, frag begin, msg, msg, else, msg, frag end, msg, note.
  ASSERT_EQ(d.events.size(), 9u);
  EXPECT_LT(d.events[0].Y, d.events[2].Y);
  const ImRect frame = d.events[1].Rect;
  for (int m : {2, 3, 5}) {
    EXPECT_GT(d.events[m].Y, frame.Min.y);
    EXPECT_LT(d.events[m].Y, frame.Max.y);
  }
  EXPECT_LT(d.events[7].Y, d.events[8].Rect.Min.y);
  EXPECT_EQ(d.events[7].Number, 5);
}

TEST(uml_sequence, ParticipantRules) {
  expect_error(with(kExample, "    - {type: Participant, id: db, name: Database, kind: Database}\n", "") +
                   "    - {type: Participant, id: db}\n",
               19, 0, "declared after the first message");
  expect_error(with(kExample, "from: web, to: db, text: load user", "from: web, to: dbx, text: load user"), 14, 0,
               "unknown id 'dbx' (did you mean 'db'?)");
  expect_error(with(kExample, "kind: Reply, deactivate: true", "kind: Reply, deactivate: true}\n    - {type: Message, "
                                                               "from: web, to: user, deactivate: true"),
               19, 0, "which has none open");
}

TEST(uml_sequence, CreateAndDestroyRules) {
  const std::string created = with(kExample, "kind: Database}", "kind: Database, created: true}");
  expect_error(created, 14, 0, "before the Create message that makes it");
  const std::string ok = with(created, "from: web, to: db, text: load user", "from: web, to: db, text: new, kind: Create");
  EXPECT_NO_THROW(check_source(ok));
  expect_error(with(ok, "kind: Database, created: true}", "kind: Database, created: true}\n    - {type: Participant, id: q}") +
                   "    - {type: Message, from: user, to: q, kind: Create}\n",
               21, 0, "not waiting to be created");
  const std::string destroyed = with(kExample, "text: row, kind: Reply}", "text: row, kind: Reply}\n"
                                                                          "        - {type: Message, from: web, to: db, "
                                                                          "kind: Destroy}");
  EXPECT_NO_THROW(check_source(destroyed));
  expect_error(with(destroyed, "from: web, to: user, text: \"401\"", "from: web, to: db, text: \"401\""), 18, 0,
               "destroyed at line 16");
}

TEST(uml_sequence, ElseAndNoteRules) {
  expect_error(with(kExample, "kind: Alt", "kind: Loop"), 16, 0, "Else separates the operands of an Alt");
  expect_error(with(kExample, "        - {type: Message, from: web, to: user, text: \"401\", kind: Reply}\n", ""), 16, 0,
               "not first or last");
  expect_error(with(kExample, "over: [user, web]", "attach: [user, web]"), 19, 0, "takes one participant");
  expect_error(with(kExample, "over: [user, web]", "text2: x"), 19, 0, "Note has no field 'text2'");
  expect_error(with(kExample, "over: [user, web]", "over: [user, web], attach: [db]"), 19, 0, "either over");
  EXPECT_NO_THROW(check_source(with(kExample, "over: [user, web]", "attach: [db], side: Left")));
}

TEST(uml_sequence, ColourAndMirror) {
  const std::string red = with(kExample, "text: \"POST /login\"", "text: \"POST /login\", color: \"#FF0000\"");
  EXPECT_EQ(count_color(render(kExample), {255, 0, 0, 255}), 0u);
  EXPECT_GT(count_color(render(red), {255, 0, 0, 255}), 20u);
  const std::string no_mirror = with(kExample, "  autonumber: true\n", "  autonumber: true\n  mirror_participants: false\n");
  EXPECT_LT(non_background(render(no_mirror)), non_background(render(kExample)));
}

} // namespace
} // namespace bdg::nymph
