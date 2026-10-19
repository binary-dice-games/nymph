// MIT License © 2026 Binary Dice Games
/// @file test_inspector.cpp
/// @brief spec/08_view_editor plan step 9: the inspector chooses a widget per
///        field type, draws without an OpenGL context, and applies changes
///        through the model.
#include <gtest/gtest.h>
#include <imgui.h>
#include <string>
#include <vector>
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "elements/register_builtin.hpp"
#include "render/frame_setup.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {
namespace {

const char* const kSource =
    "image: {width: 400, height: 900}\n"
    "root:\n"
    "  type: VerticalLayout\n"
    "  children:\n"
    "    - {type: Label, text: Report}\n"
    "    - type: Plot\n"
    "      title: Revenue\n"
    "      legend_flags: Outside\n"
    "      text_color: \"#2471A3\"\n"
    "      children:\n"
    "        - {type: PlotLine, label: a, ys: [1, 3, 2]}\n"
    "    - type: Flowchart\n"
    "      children:\n"
    "        - {type: FlowNode, id: a, text: Start, text_color: \"#C0392B\"}\n";

/// An ImGui context that can run frames without any rendering backend.
class headless_imgui {
 public:
  headless_imgui() {
    const context_switch on(contexts_);
    configure(*find_theme("light"), "test", ImGuiBackendFlags_RendererHasTextures);
    load_fonts(dc_);
  }

  /// One frame of `f` inside a window.
  template <typename Fn>
  void frame(Fn f) {
    const context_switch on(contexts_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(360, 700);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(360, 700));
    ImGui::Begin("panel");
    f();
    ImGui::End();
    ImGui::Render();
  }

 private:
  imgui_contexts contexts_;
  image_options options_;
  draw_context dc_{options_};
};

TEST(inspector, EachFieldTypeGetsItsWidget) {
  const registry& reg = builtin_registry();
  const element_class* plot = reg.find("Plot");
  const element_class* node = reg.find("FlowNode");
  ASSERT_NE(plot, nullptr);
  ASSERT_NE(node, nullptr);
  const auto kind = [](const element_class* cls, const char* name) {
    return widget_for(cls->fields()[static_cast<size_t>(cls->field_index(name))]);
  };
  EXPECT_EQ(kind(plot, "title"), widget_kind::text);
  EXPECT_EQ(kind(plot, "width"), widget_kind::number);
  EXPECT_EQ(kind(plot, "font"), widget_kind::font);
  EXPECT_EQ(kind(plot, "font_size"), widget_kind::number);
  EXPECT_EQ(kind(plot, "text_color"), widget_kind::color);
  EXPECT_EQ(kind(plot, "legend_location"), widget_kind::choice);
  EXPECT_EQ(kind(plot, "legend_flags"), widget_kind::flags);
  EXPECT_EQ(kind(node, "text"), widget_kind::multiline_text); // "a line break starts a new line"
  EXPECT_EQ(kind(node, "shape"), widget_kind::choice);

  // Every field of every element has some widget; lists are only shown.
  int lists = 0, bools = 0, ints = 0;
  for (const element_class* cls : reg.classes()) {
    for (const field_desc& f : cls->fields()) {
      const widget_kind k = widget_for(f);
      switch (f.type) {
        case field_type::boolean:
          EXPECT_EQ(k, widget_kind::checkbox);
          ++bools;
          break;
        case field_type::integer:
          EXPECT_EQ(k, widget_kind::integer);
          ++ints;
          break;
        case field_type::number_array:
        case field_type::integer_array:
        case field_type::text_array:
          EXPECT_EQ(k, widget_kind::read_only);
          ++lists;
          break;
        default:
          break;
      }
    }
  }
  EXPECT_GT(lists, 5);
  EXPECT_GT(bools, 5);
  EXPECT_GT(ints, 5);
}

TEST(inspector, DrawsEveryElementOfTheExampleWithoutAnOpenGLContext) {
  headless_imgui gui;
  editor_model model(kSource);
  inspector panel;
  live_view view;
  view.set_source(kSource);
  const std::vector<edit_element> tree = view.element_list();
  ASSERT_EQ(tree.size(), 6u);
  // Nothing selected, then each element, with and without a focused field.
  for (const edit_target_id& id : {edit_target_id{"", ""}, edit_target_id{"root", ""},
                                   edit_target_id{"root/children/0", "text"}, edit_target_id{"root/children/1", "title"},
                                   edit_target_id{"root/children/1/children/0", ""},
                                   edit_target_id{"root/children/2/children/0", "text"}}) {
    model.select(id.element.empty() ? std::nullopt : std::optional<edit_target_id>(id));
    for (int i = 0; i < 2; ++i) {
      inspector_events events;
      gui.frame([&] { events = panel.draw(model, tree, "status"); });
      EXPECT_FALSE(events.source);
      EXPECT_FALSE(events.select);
      EXPECT_FALSE(events.save);
    }
  }
  EXPECT_EQ(panel.error(), "");
}

TEST(inspector, TypingATextChangesTheSourceThroughTheModel) {
  editor_model model(kSource);
  inspector panel;
  model.select(edit_target_id{"root/children/1", "title"});
  const inspector_events ok = panel.edit_text(model, "title", "Sales 2026");
  ASSERT_TRUE(ok.source);
  EXPECT_NE(ok.source->find("title: Sales 2026"), std::string::npos);
  EXPECT_EQ(panel.error(), "");

  // Emptying a text whose default is empty removes the field; otherwise
  // the empty text is written.
  model.select(edit_target_id{"root/children/0", "text"});
  const inspector_events label = panel.edit_text(model, "text", "");
  ASSERT_TRUE(label.source);
  EXPECT_EQ(label.source->find("text: Report"), std::string::npos);
  model.select(edit_target_id{"root/children/1", "title"});
  const inspector_events none = panel.edit_text(model, "title", "");
  ASSERT_TRUE(none.source);
  EXPECT_NE(none.source->find("title: \"\""), std::string::npos) << *none.source;
}

TEST(inspector, ARefusedTextShowsTheErrorAndLeavesTheSource) {
  editor_model model(kSource);
  inspector panel;
  model.select(edit_target_id{"root/children/1", "font"});
  const std::string before = model.source();
  const inspector_events refused = panel.edit_text(model, "font", "NoSuchFont.ttf");
  EXPECT_FALSE(refused.source);
  EXPECT_NE(panel.error(), "");
  EXPECT_EQ(model.source(), before);
  // A good value clears the error.
  const inspector_events fine = panel.edit_text(model, "font", "default");
  EXPECT_TRUE(fine.source);
  EXPECT_EQ(panel.error(), "");
}

TEST(inspector, FlushWithNothingTypedDoesNothing) {
  editor_model model(kSource);
  inspector panel;
  EXPECT_FALSE(panel.flush(model).source);
}

} // namespace
} // namespace bdg::nymph
