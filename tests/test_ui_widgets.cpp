// MIT License © 2026 Binary Dice Games
/// @file test_ui_widgets.cpp
/// @brief spec/09_ui_mockups PLAN step 4: every widget renders from its
///        fields, formats are checked, labels may repeat.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "nymph.hpp"
#include "schema/registry.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::render_ui;
using test::ui_render;

error source_error(const std::string& source) {
  try {
    check_source(source);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << source;
  return error(0, 0, "");
}

bool says(const error& e, const std::string& part) {
  return std::string(e.what()).find(part) != std::string::npos;
}

std::string in_window(const std::string& children) {
  return "image: {width: 640, height: 480}\n"
         "root:\n"
         "  type: Screen\n"
         "  children:\n"
         "    - type: Window\n"
         "      title: W\n"
         "      x: 10\n"
         "      y: 10\n"
         "      width: 600\n"
         "      height: 440\n"
         "      children:\n" +
      children;
}

/// The smallest valid element of `cls`: list values of the length its name
/// says ("SliderFloat3", "DragIntRange2"), else every field at its default.
std::string minimal(const element_class& cls, const std::string& label) {
  std::string out = "{type: " + std::string(cls.name());
  if (cls.field_index("label") >= 0) {
    out += ", label: \"" + label + "\"";
  }
  const int value = cls.field_index("value");
  if (value >= 0) {
    const field_desc& f = cls.fields()[static_cast<size_t>(value)];
    if (f.type == field_type::number_array || f.type == field_type::integer_array) {
      const char last = cls.name().back();
      const int count = last >= '2' && last <= '4' ? last - '0' : 1;
      out += ", value: [";
      for (int i = 0; i < count; ++i) {
        out += i == 0 ? "1" : ", 2";
      }
      out += "]";
    }
  }
  return out + "}";
}

// ── Every widget ─────────────────────────────────────────────────────────────

TEST(ui_widgets, EveryWidgetRendersTwiceWithTheSameLabel) {
  int widgets = 0;
  for (const element_class* cls : builtin_registry().classes()) {
    if (!cls->fills_slot("ui.widget") || cls->fills_slot("figure")) {
      continue; // figure elements in windows are tested with the containers
    }
    ++widgets;
    const std::string element = minimal(*cls, "Same");
    const std::string source = in_window("        - " + element + "\n        - " + element + "\n");
    SCOPED_TRACE(source);
    EXPECT_NO_THROW(render_ui(source)) << cls->name();
  }
  EXPECT_GE(widgets, 50);
}

TEST(ui_widgets, SeparatorsAreWidgetsInWindowsAndFiguresInLayouts) {
  const ui_render r = render_ui(
      in_window(
          "        - {type: Text, text: a}\n"
          "        - {type: Separator}\n"
          "        - {type: SeparatorText, label: Section}\n"),
      true);
  const node& window = r.fig.root.children[0];
  EXPECT_EQ(window.children[1].cls->placed_as(window.children[1]), "ui.widget");
  bool label_target = false;
  for (const edit_record& t : r.targets) {
    label_target |= t.field == "label" && t.text == "Section" && t.area.min.y > 10;
  }
  EXPECT_TRUE(label_target);
  EXPECT_NO_THROW(check_source("root: {type: VerticalLayout, children: [{type: Separator}]}\n"));
}

// ── Values ───────────────────────────────────────────────────────────────────

TEST(ui_widgets, ValuesStartFromTheSource) {
  const ui_render r = render_ui(in_window(
      "        - {type: SliderFloat3, label: v, value: [0.1, 0.5, 0.9]}\n"
      "        - {type: SliderInt, label: i, value: 42}\n"
      "        - {type: InputText, label: t, value: Hello}\n"
      "        - {type: ColorEdit4, label: c, value: \"#FF000080\"}\n"
      "        - {type: Combo, label: f, items: [a, b, c], selected: 2}\n"
      "        - {type: SliderAngle, label: a, value: 90}\n"));
  const auto& s = r.state;
  EXPECT_FLOAT_EQ(s.at("root/children/0/children/0").floats[2], 0.9f);
  EXPECT_EQ(s.at("root/children/0/children/1").ints[0], 42);
  EXPECT_EQ(s.at("root/children/0/children/2").text, "Hello");
  EXPECT_FLOAT_EQ(s.at("root/children/0/children/3").floats[0], 1.0f);
  EXPECT_NEAR(s.at("root/children/0/children/3").floats[3], 128.0f / 255.0f, 1e-6);
  EXPECT_EQ(s.at("root/children/0/children/4").index, 2);
  EXPECT_NEAR(s.at("root/children/0/children/5").floats[0], 3.14159265f / 2.0f, 1e-5);
}

TEST(ui_widgets, ComponentCountsAndTypesAreChecked) {
  const error three = source_error(in_window("        - {type: SliderFloat3, label: v, value: [1, 2]}\n"));
  EXPECT_TRUE(says(three, "SliderFloat3.value must be 3 numbers, not 2")) << three.what();
  const error integer = source_error(in_window("        - {type: SliderInt, label: v, value: 1.5}\n"));
  EXPECT_TRUE(says(integer, "SliderInt.value")) << integer.what();
  const error range = source_error(in_window("        - {type: DragIntRange2, label: v, value: [1]}\n"));
  EXPECT_TRUE(says(range, "must be two numbers [low, high]")) << range.what();
  const error reversed = source_error(in_window("        - {type: SliderFloat, label: v, min: 1, max: 0}\n"));
  EXPECT_TRUE(says(reversed, "SliderFloat.max must not be below min")) << reversed.what();
}

TEST(ui_widgets, FormatsAreChecked) {
  const error s = source_error(in_window("        - {type: SliderFloat, label: v, format: \"%s\"}\n"));
  EXPECT_TRUE(says(s, "SliderFloat.format")) << s.what();
  const error n = source_error(in_window("        - {type: SliderInt, label: v, format: \"%n\"}\n"));
  EXPECT_TRUE(says(n, "SliderInt.format must format one integer")) << n.what();
  const error f = source_error(in_window("        - {type: DragInt, label: v, format: \"%f\"}\n"));
  EXPECT_TRUE(says(f, "DragInt.format")) << f.what();
  const error two = source_error(in_window("        - {type: SliderInt, label: v, format: \"%d %d\"}\n"));
  EXPECT_TRUE(says(two, "only one")) << two.what();
  EXPECT_NO_THROW(check_source(in_window("        - {type: SliderInt, label: v, format: \"%d%% done\"}\n")));
}

TEST(ui_widgets, TextIsNeverAFormat) {
  // Each text family element draws "%d %s %n" as written; through printf it
  // would read arguments that are not there.
  const char* const text = "50% %d %s %n";
  const ui_render r = render_ui(
      in_window(
          std::string("        - {type: Text, text: \"") + text + "\"}\n" + "        - {type: BulletText, text: \"" +
          text + "\"}\n" + "        - {type: LabelText, label: l, text: \"" + text + "\"}\n" +
          "        - {type: TextColored, text: \"" + text + "\", color: \"#FF0000\"}\n"),
      true);
  for (const edit_record& t : r.targets) {
    if (t.field.empty() && t.element->cls->name() == "Text") {
      // Measured with the same font: the item is exactly the literal text.
      EXPECT_GT(t.area.width(), 80);
      EXPECT_LT(t.area.width(), 120);
    }
  }
}

TEST(ui_widgets, ChoicesTakeAColumnAndCheckTheSelection) {
  const figure f = bind_source(
      "data: {fruit: [Apple, Banana, Cherry]}\n" + in_window("        - {type: Combo, label: f, items: $fruit}\n").substr(0),
      builtin_registry());
  const node& combo = f.root.children[0].children[0];
  EXPECT_EQ(combo.get<std::vector<std::string>>(1), (std::vector<std::string>{"Apple", "Banana", "Cherry"}));
  const error e = source_error(in_window("        - {type: ListBox, label: l, items: [a, b], selected: 2}\n"));
  EXPECT_TRUE(says(e, "ListBox.selected is 2 but there are 2 items")) << e.what();
}

TEST(ui_widgets, InputTextIsLimited) {
  const std::string big(64 * 1024, 'x');
  const error e = source_error(in_window("        - {type: InputText, label: t, value: " + big + "}\n"));
  EXPECT_TRUE(says(e, "InputText.value is longer than 65535 bytes")) << e.what();
}

TEST(ui_widgets, StateShowsInThePicture) {
  const std::string on = in_window("        - {type: RadioButton, label: r, active: true}\n");
  const std::string off = in_window("        - {type: RadioButton, label: r, active: false}\n");
  EXPECT_NE(render_ui(on).img.rgba, render_ui(off).img.rgba);
  const std::string half = in_window("        - {type: ProgressBar, fraction: 0.5}\n");
  const std::string most = in_window("        - {type: ProgressBar, fraction: 0.9}\n");
  EXPECT_NE(render_ui(half).img.rgba, render_ui(most).img.rgba);
}

} // namespace
} // namespace bdg::nymph
