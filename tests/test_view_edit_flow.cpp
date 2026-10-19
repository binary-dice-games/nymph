// MIT License © 2026 Binary Dice Games
/// @file test_view_edit_flow.cpp
/// @brief spec/08_view_editor plan step 10: a click in the figure, a change
///        in the editor, the figure redrawn, the result saved: live_view,
///        editor_model and inspector together, without a window.
#include <gtest/gtest.h>
#include <string>
#include "document/bind.hpp"
#include "edit/editor_model.hpp"
#include "edit/field_value.hpp"
#include "edit/inspector.hpp"
#include "elements/register_builtin.hpp"
#include "live_view_driver.hpp"
#include "render/figure_renderer.hpp"
#include "render/soft_rasterizer.hpp"

namespace bdg::nymph {
namespace {

const char* const kChart =
    "image: {width: 480, height: 320}\n"
    "root:\n"
    "  type: Plot\n"
    "  title: Revenue\n"
    "  x_label: month\n"
    "  children:\n"
    "    - {type: PlotLine, label: revenue, ys: [1, 3, 2, 4]}\n";

const char* const kFlow =
    "image: {width: 400, height: 260}\n"
    "root:\n"
    "  type: Flowchart\n"
    "  children:\n"
    "    - {type: FlowNode, id: a, text: Alpha}\n"
    "    - {type: FlowNode, id: b, text: Beta}\n"
    "    - {type: FlowLink, from: a, to: b, label: go}\n";

image rasterized(driven_view& d) {
  return rasterize(*d.last, d.v.textures(), d.v.clear_color());
}

/// What a click in the figure and a change in the panel do, as the window
/// wires them.
struct session {
  explicit session(const std::string& source, float w, float h) : d(source, w, h), model(source) {
    d.v.set_edit_mode(true);
    d.run();
  }

  /// A click at the centre of `field` of `element`, selected in the model.
  void click(const std::string& element, const std::string& field) {
    const std::optional<edit_target> t = d.v.target_for(element, field);
    ASSERT_TRUE(t) << element << "." << field;
    d.click(t->x + t->width / 2, t->y + t->height / 2);
    const std::optional<edit_target> clicked = d.v.take_edit_click();
    ASSERT_TRUE(clicked);
    EXPECT_EQ(clicked->element, element);
    EXPECT_EQ(clicked->field, field);
    model.select(edit_target_id{clicked->element, clicked->field});
    d.v.set_selection(model.selection());
  }

  /// Applies a committed change to the figure.
  void show(const commit_result& r) {
    ASSERT_TRUE(r.ok) << r.error;
    d.v.set_source(r.source);
    d.run();
  }

  /// Takes the outlines away (no selection, pointer outside), so the frame is the figure alone.
  void plain() {
    d.v.set_selection(std::nullopt);
    d.send({pointer_event::kind::leave, 0.0f, 0.0f, 0});
  }

  driven_view d;
  editor_model model;
};

TEST(view_edit_flow, ClickingAPlotTitleAndChangingItRedrawsAsTheEditedSourceWould) {
  session s(kChart, 480, 320);
  s.click("root", "title");
  s.show(s.model.commit("title", encode_text("Sales 2026")));

  const image live = rasterized(s.d);
  const image reference = render_figure(bind_source(s.model.source(), builtin_registry()));
  ASSERT_EQ(live.width, reference.width);
  // The selection outline is drawn on top: look at a row above the title.
  s.plain();
  EXPECT_TRUE(rasterized(s.d).rgba == reference.rgba);
  // The title moved with its new text: the target is found again, and says so.
  EXPECT_EQ(s.d.v.target_for("root", "title")->text, "Sales 2026");
  EXPECT_NE(s.model.source().find("title: Sales 2026"), std::string::npos);
}

TEST(view_edit_flow, ChangingAFlowchartNodesTextAndColourLeavesTheRestAlone) {
  session s(kFlow, 400, 260);
  s.click("root/children/1", "text");
  s.show(s.model.commit("text", encode_text("Beta, renamed")));
  s.show(s.model.commit("text_color", *encode_color("#C0392B")));
  EXPECT_EQ(s.d.v.target_for("root/children/1", "text")->text, "Beta, renamed");
  EXPECT_TRUE(s.d.v.target_for("root/children/0", "text")); // the other node is still there
  s.plain();
  const image reference = render_figure(bind_source(s.model.source(), builtin_registry()));
  EXPECT_TRUE(rasterized(s.d).rgba == reference.rgba);
  // The undo puts the figure back as it was.
  const std::optional<std::string> back = s.model.undo();
  ASSERT_TRUE(back);
  ASSERT_TRUE(s.model.undo());
  s.d.v.set_source(s.model.source());
  s.d.run();
  EXPECT_EQ(s.model.source(), kFlow);
  EXPECT_EQ(s.d.v.target_for("root/children/1", "text")->text, "Beta");
}

TEST(view_edit_flow, TheSavedYamlHasTheEditAndTheViewTheUserLeft) {
  session s(kChart, 480, 320);
  // Zoom the plot first: a wheel notch over its centre.
  const plot_view plot = s.d.plot();
  s.d.wheel(plot.x + plot.width / 2, plot.y + plot.height / 2, 2);
  s.click("root", "title");
  s.show(s.model.commit("title", encode_text("Zoomed")));
  const std::string saved = s.d.v.source_with_view();
  EXPECT_NE(saved.find("title: Zoomed"), std::string::npos);
  EXPECT_NE(saved.find("\nview:"), std::string::npos);
  EXPECT_NE(saved.find("root:"), std::string::npos);
  // The saved text is a valid source of the same figure.
  EXPECT_NO_THROW(bind_source(saved, builtin_registry()));
}

TEST(view_edit_flow, ABadColourIsRefusedAndTheFigureStaysAsItWas) {
  session s(kChart, 480, 320);
  s.click("root", "title");
  const std::string before = s.model.source();
  EXPECT_FALSE(encode_color("red")); // the widget never produces this
  const commit_result bad = s.model.commit("text_color", "red");
  EXPECT_FALSE(bad.ok);
  EXPECT_NE(bad.error.find("text_color"), std::string::npos);
  EXPECT_EQ(s.model.source(), before);
  EXPECT_FALSE(s.model.can_undo());
  // Through the inspector the same refusal shows as its error line.
  inspector panel;
  panel.edit_text(s.model, "font", "NoSuchFont.ttf");
  EXPECT_NE(panel.error(), "");
  EXPECT_EQ(s.model.source(), before);
}

// ── User-interface mockups (spec/09_ui_mockups PLAN step 13) ─────────────────

const char* const kMockup =
    "image: {width: 480, height: 320}\n"
    "root:\n"
    "  type: Screen\n"
    "  children:\n"
    "    - type: Window\n"
    "      title: Settings\n"
    "      x: 20\n"
    "      y: 20\n"
    "      width: 300\n"
    "      height: 200\n"
    "      children:\n"
    "        - {type: Text, text: Account}\n"
    "        - {type: Button, label: Save}\n"
    "        - {type: Checkbox, label: Remember me}\n";

TEST(view_edit_flow, AClickOnAWidgetSelectsItsLabelInsteadOfPressingIt) {
  session s(kMockup, 480, 320);
  s.click("root/children/0/children/1", "label");
  EXPECT_FALSE(s.d.v.take_click()); // the button was not pressed
  s.show(s.model.commit("label", encode_text("Save changes")));
  s.plain();
  const image reference = render_figure(bind_source(s.model.source(), builtin_registry()));
  EXPECT_TRUE(rasterized(s.d).rgba == reference.rgba);
}

TEST(view_edit_flow, AWindowTitleAndATextAreEditable) {
  session s(kMockup, 480, 320);
  s.click("root/children/0", "title");
  s.show(s.model.commit("title", encode_text("Preferences")));
  s.click("root/children/0/children/0", "text");
  s.show(s.model.commit("text", encode_text("Your account")));
  EXPECT_NE(s.model.source().find("title: Preferences"), std::string::npos);
  EXPECT_NE(s.model.source().find("text: Your account"), std::string::npos);
}

TEST(view_edit_flow, EditModeAloneDrawsTheSameMockup) {
  driven_view plain(kMockup, 480, 320);
  driven_view editing(kMockup, 480, 320);
  editing.v.set_edit_mode(true);
  editing.run();
  EXPECT_TRUE(rasterized(plain).rgba == rasterized(editing).rgba);
}

} // namespace
} // namespace bdg::nymph
