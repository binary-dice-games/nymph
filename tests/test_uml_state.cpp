// MIT License © 2026 Binary Dice Games
/// @file test_uml_state.cpp
/// @brief UML plan steps 7 and 12: `StateDiagram`, `State`, `Transition`,
///        composite states.
#include <gtest/gtest.h>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

const char* const kExample =
    "image: {width: 700, height: 400}\n" // 1
    "root:\n" // 2
    "  type: StateDiagram\n" // 3
    "  direction: LeftRight\n" // 4
    "  children:\n" // 5
    "    - {type: State, id: start, kind: Initial}\n" // 6
    "    - {type: State, id: idle, name: Idle}\n" // 7
    "    - {type: State, id: running, name: Running, activities: [\"entry / start timer\"]}\n" // 8
    "    - {type: State, id: done, kind: Final}\n" // 9
    "    - {type: Transition, from: start, to: idle}\n" // 10
    "    - {type: Transition, from: idle, to: running, label: \"start [ready] / log\"}\n" // 11
    "    - {type: Transition, from: running, to: idle, label: pause}\n" // 12
    "    - {type: Transition, from: running, to: done}\n"; // 13

const char* const kComposite =
    "image: {width: 600, height: 600}\n" // 1
    "root:\n" // 2
    "  type: StateDiagram\n" // 3
    "  children:\n" // 4
    "    - {type: State, id: start, kind: Initial}\n" // 5
    "    - {type: State, id: stopped, name: Stopped}\n" // 6
    "    - type: State\n" // 7
    "      id: active\n" // 8
    "      children:\n" // 9
    "        - {type: State, id: a0, kind: Initial}\n" // 10
    "        - {type: State, id: playing}\n" // 11
    "        - {type: State, id: paused}\n" // 12
    "        - {type: Transition, from: a0, to: playing}\n" // 13
    "        - {type: Transition, from: playing, to: paused}\n" // 14
    "    - {type: Transition, from: start, to: stopped}\n" // 15
    "    - {type: Transition, from: stopped, to: playing}\n"; // 16

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

TEST(uml_state, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kExample)), 1500u);
  expect_round_trip(kExample);
}

TEST(uml_state, InitialIsADotFinalABullseye) {
  const drawn_diagram d = draw_and_inspect(kExample, {"start", "done"});
  ASSERT_EQ(d.nodes.size(), 2u);
  EXPECT_LT(d.nodes.at("start").GetWidth(), 20.0f);
  // Left to right: start is leftmost, done rightmost.
  EXPECT_LT(d.nodes.at("start").Max.x, d.nodes.at("done").Min.x);
}

TEST(uml_state, TransitionRules) {
  expect_error(with(kExample, "from: running, to: done", "from: done, to: running"), 13, 0, "names a Final state");
  expect_error(with(kExample, "from: start, to: idle", "from: idle, to: start"), 10, 0, "names an Initial state");
  expect_error(
      with(kExample, "    - {type: Transition, from: running, to: done}\n",
           "    - {type: Transition, from: running, to: done}\n    - {type: Transition, from: start, to: done}\n"),
      14, 0, "second transition out of the Initial state");
  expect_error(with(kExample, "kind: Final}", "kind: Final, activities: [x]}"), 9, 0, "for Simple states only");
}

TEST(uml_state, CompositeChildrenLieInsideIt) {
  const drawn_diagram d = draw_and_inspect(kComposite, {"active", "a0", "playing", "paused", "stopped"});
  ASSERT_EQ(d.nodes.size(), 5u);
  const ImRect outer = d.nodes.at("active");
  for (const char* inner : {"a0", "playing", "paused"}) {
    EXPECT_TRUE(outer.Contains(d.nodes.at(inner))) << inner;
    EXPECT_GT(d.nodes.at(inner).Min.y, outer.Min.y + 10.0f) << inner; // below the header
  }
  EXPECT_FALSE(outer.Overlaps(d.nodes.at("stopped")));
  expect_round_trip(kComposite);
}

TEST(uml_state, CompositeErrors) {
  expect_error(with(kComposite, "id: active\n", "id: active\n      kind: Choice\n"), 11, 0, "pseudo-state (Choice)");
  expect_error(with(kComposite, "from: stopped, to: playing", "from: active, to: playing"), 16, 0,
               "connects a composite state with a state inside it");
  expect_error(with(kComposite, "id: paused}", "id: stopped}"), 12, 0, "id 'stopped' is already used");
  std::string deep = "root:\n  type: StateDiagram\n  children:\n";
  std::string indent = "    ";
  for (int i = 0; i < 9; ++i) {
    deep += indent + "- type: State\n" + indent + "  id: s" + std::to_string(i) + "\n" + indent + "  children:\n";
    indent += "    ";
  }
  deep += indent + "- {type: State, id: leaf}\n";
  EXPECT_TRUE(contains(source_error(deep), "levels are allowed"));
}

} // namespace
} // namespace bdg::nymph
