// MIT License © 2026 Binary Dice Games
/// @file test_uml_activity_usecase.cpp
/// @brief UML plan steps 8-9: `ActivityDiagram` (`Action`, `ActivityControl`,
///        `Flow`) and `UseCaseDiagram` (`Actor`, `UseCase`, `UseCaseRelation`).
#include <gtest/gtest.h>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

// ── Activity ─────────────────────────────────────────────────────────────────

const char* const kActivity =
    "image: {width: 500, height: 600}\n" // 1
    "root:\n" // 2
    "  type: ActivityDiagram\n" // 3
    "  children:\n" // 4
    "    - {type: ActivityControl, id: start, kind: Initial}\n" // 5
    "    - {type: Action, id: receive, text: Receive order}\n" // 6
    "    - {type: ActivityControl, id: stock, kind: Decision}\n" // 7
    "    - {type: ActivityControl, id: fork, kind: Fork}\n" // 8
    "    - {type: Action, id: ship, text: Ship}\n" // 9
    "    - {type: Action, id: bill, text: Bill, kind: Send}\n" // 10
    "    - {type: Action, id: cancel, text: Cancel}\n" // 11
    "    - {type: ActivityControl, id: end, kind: Final}\n" // 12
    "    - {type: Flow, from: start, to: receive}\n" // 13
    "    - {type: Flow, from: receive, to: stock}\n" // 14
    "    - {type: Flow, from: stock, to: fork, guard: in stock}\n" // 15
    "    - {type: Flow, from: stock, to: cancel, guard: else}\n" // 16
    "    - {type: Flow, from: fork, to: ship}\n" // 17
    "    - {type: Flow, from: fork, to: bill}\n" // 18
    "    - {type: Flow, from: ship, to: end}\n" // 19
    "    - {type: Flow, from: cancel, to: end}\n"; // 20

TEST(uml_activity, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kActivity)), 1500u);
  expect_round_trip(kActivity);
}

TEST(uml_activity, ForkBarsArePerpendicularToTheFlow) {
  drawn_diagram d = draw_and_inspect(kActivity, {"fork", "start", "end"});
  EXPECT_GT(d.nodes.at("fork").GetWidth(), d.nodes.at("fork").GetHeight());
  EXPECT_LT(d.nodes.at("start").Max.y, d.nodes.at("end").Min.y);
  d = draw_and_inspect(with(kActivity, "type: ActivityDiagram\n", "type: ActivityDiagram\n  direction: LeftRight\n"),
                       {"fork"});
  EXPECT_LT(d.nodes.at("fork").GetWidth(), d.nodes.at("fork").GetHeight());
}

TEST(uml_activity, FlowRules) {
  expect_error(with(kActivity, "from: start, to: receive", "from: receive, to: start"), 13, 0,
               "names an Initial node; nothing may flow into it");
  expect_error(with(kActivity, "from: ship, to: end", "from: end, to: ship"), 19, 0, "names a Final node");
  expect_error(with(kActivity, "from: ship, to: end", "from: ship, to: nowhere"), 19, 0, "unknown id 'nowhere'");
}

// ── Use case ─────────────────────────────────────────────────────────────────

const char* const kUseCase =
    "image: {width: 700, height: 400}\n" // 1
    "root:\n" // 2
    "  type: UseCaseDiagram\n" // 3
    "  system: Online shop\n" // 4
    "  children:\n" // 5
    "    - {type: Actor, id: customer, name: Customer}\n" // 6
    "    - {type: Actor, id: bank, name: Bank, side: Right}\n" // 7
    "    - {type: UseCase, id: buy, name: Buy item}\n" // 8
    "    - {type: UseCase, id: pay, name: Pay}\n" // 9
    "    - {type: UseCaseRelation, from: customer, to: buy}\n" // 10
    "    - {type: UseCaseRelation, from: buy, to: pay, kind: Include}\n" // 11
    "    - {type: UseCaseRelation, from: pay, to: bank}\n"; // 12

TEST(uml_usecase, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kUseCase)), 1500u);
  expect_round_trip(kUseCase);
}

TEST(uml_usecase, ActorsSitOutsideTheBoundary) {
  const figure f = bind_source(kUseCase, builtin_registry());
  ImRect boundary;
  std::map<std::string, ImRect> nodes;
  render_frames(f.image, [&](draw_context& dc) {
    layout_root(f.root, dc);
    dc.draw_node(f.root);
    ImUmlContext* ctx = ImUml::GetCurrentContext();
    for (int i = 0; i < ctx->Diagrams.Size; ++i) {
      const ImUmlDiagram* d = ctx->Diagrams[i];
      if (d->Flags & ImUmlDiagramFlags_MeasureOnly) {
        continue;
      }
      boundary = d->Boundary;
      for (const char* id : {"customer", "bank", "buy", "pay"}) {
        for (int k = 0; k < d->Nodes.Size; ++k) {
          if (d->Nodes[k].Id == ImHashStr(id)) {
            nodes[id] = ImRect(d->Nodes[k].Pos - d->Nodes[k].Size * 0.5f, d->Nodes[k].Pos + d->Nodes[k].Size * 0.5f);
          }
        }
      }
    }
  });
  ASSERT_GT(boundary.GetWidth(), 0.0f);
  EXPECT_LT(nodes["customer"].Max.x, boundary.Min.x);
  EXPECT_GT(nodes["bank"].Min.x, boundary.Max.x);
  EXPECT_TRUE(boundary.Contains(nodes["buy"]));
  EXPECT_TRUE(boundary.Contains(nodes["pay"]));

  // No system: no boundary, fewer drawn pixels.
  const std::string no_system = with(kUseCase, "  system: Online shop\n", "");
  EXPECT_LT(non_background(render(no_system)), non_background(render(kUseCase)));
}

TEST(uml_usecase, RelationKindRules) {
  expect_error(with(kUseCase, "from: buy, to: pay, kind: Include", "from: customer, to: pay, kind: Include"), 11, 0,
               "names an Actor; Include relates two use cases");
  expect_error(with(kUseCase, "from: customer, to: buy}", "from: buy, to: pay}"), 10, 0,
               "Association joins an actor and a use case");
  expect_error(with(kUseCase, "from: pay, to: bank}", "from: pay, to: bank, kind: Generalization}"), 12, 0,
               "two actors or two use cases");
}

} // namespace
} // namespace bdg::nymph
