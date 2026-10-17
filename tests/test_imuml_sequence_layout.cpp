// MIT License © 2026 Binary Dice Games
/// @file test_imuml_sequence_layout.cpp
/// @brief UML plan steps 10-11: the sequence layout (columns, rows,
///        activations, create / destroy, fragments). Pure: no ImGui context.
#include <gtest/gtest.h>
#include <cmath>
#include "imuml_internal.h"

namespace {

ImUmlSeqInput participants(int n, float w = 60, float h = 24) {
  ImUmlSeqInput in;
  for (int i = 0; i < n; ++i) {
    ImUmlSeqParticipantIn p;
    p.BoxSize = ImVec2(w, h);
    p.Created = false;
    in.Participants.push_back(p);
  }
  return in;
}

ImUmlSeqEventIn event(int type, int a = -1, int b = -1, ImVec2 text = ImVec2(40, 14)) {
  ImUmlSeqEventIn e;
  e.Type = type;
  e.A = a;
  e.B = b;
  e.Kind = 0;
  e.Flags = 0;
  e.Side = ImUmlSide_Right;
  e.TextSize = text;
  return e;
}

ImUmlSeqEventIn message(int a, int b, float text_w = 40, int kind = ImUmlMessageKind_Sync, int flags = 0) {
  ImUmlSeqEventIn e = event(ImUmlSeqEventType_Message, a, b, ImVec2(text_w, 14));
  e.Kind = kind;
  e.Flags = flags;
  return e;
}

TEST(imuml_sequence_layout, ColumnsInOrderAndGapsFitLabels) {
  ImUmlSeqInput in = participants(3);
  in.Events.push_back(message(0, 1, 300));
  in.Events.push_back(message(0, 2, 50));
  ImUmlSeqOutput out;
  ImUml::LayoutSequence(in, &out);
  EXPECT_LT(out.Participants[0].X, out.Participants[1].X);
  EXPECT_LT(out.Participants[1].X, out.Participants[2].X);
  EXPECT_GE(out.Participants[1].X - out.Participants[0].X, 300.0f);
  // Without messages the gap is the boxes plus the spacing.
  EXPECT_EQ(out.Participants[2].X - out.Participants[1].X, 60.0f + in.ParticipantSpacing);
}

TEST(imuml_sequence_layout, RowsFollowSubmissionOrder) {
  ImUmlSeqInput in = participants(2);
  in.Events.push_back(message(0, 1));
  in.Events.push_back(message(1, 1)); // self
  in.Events.push_back(event(ImUmlSeqEventType_NoteOver, 0, 1, ImVec2(80, 30)));
  in.Events.push_back(message(1, 0));
  ImUmlSeqOutput out;
  ImUml::LayoutSequence(in, &out);
  EXPECT_LT(out.Events[0].Y, out.Events[1].Y);
  EXPECT_LT(out.Events[1].Y, out.Events[1].Y2);
  EXPECT_LT(out.Events[1].Y2, out.Events[2].Rect.Min.y);
  EXPECT_LT(out.Events[2].Rect.Max.y, out.Events[3].Y);
  EXPECT_EQ(out.Events[3].Number, 3);
  EXPECT_GT(out.Size.y, out.Events[3].Y);
}

TEST(imuml_sequence_layout, NestedActivationsAreOffset) {
  ImUmlSeqInput in = participants(2);
  in.Events.push_back(message(0, 1, 40, ImUmlMessageKind_Sync, ImUmlMessageFlags_Activate));
  in.Events.push_back(message(0, 1, 40, ImUmlMessageKind_Sync, ImUmlMessageFlags_Activate));
  in.Events.push_back(message(1, 0, 40, ImUmlMessageKind_Reply, ImUmlMessageFlags_Deactivate));
  in.Events.push_back(message(1, 0, 40, ImUmlMessageKind_Reply, ImUmlMessageFlags_Deactivate));
  ImUmlSeqOutput out;
  ImUml::LayoutSequence(in, &out);
  ASSERT_EQ(out.Activations.Size, 2);
  const ImRect& inner = out.Activations[0].Rect; // closed first
  const ImRect& outer = out.Activations[1].Rect;
  EXPECT_GT(inner.Min.x, outer.Min.x);
  EXPECT_GT(inner.Min.y, outer.Min.y);
  EXPECT_LT(inner.Max.y, outer.Max.y);
  // A message from the left ends at the first bar it meets: the outer one.
  EXPECT_EQ(out.Events[1].X2, outer.Min.x);
  // A reply leaving the stack starts at its outer side, the inner bar.
  EXPECT_EQ(out.Events[2].X1, outer.Min.x);
}

TEST(imuml_sequence_layout, CreateAndDestroy) {
  ImUmlSeqInput in = participants(2);
  in.Participants[1].Created = true;
  in.Events.push_back(message(0, 0));
  in.Events.push_back(message(0, 1, 40, ImUmlMessageKind_Create));
  in.Events.push_back(message(0, 1, 40, ImUmlMessageKind_Destroy));
  ImUmlSeqOutput out;
  ImUml::LayoutSequence(in, &out);
  const ImUmlSeqParticipantOut& made = out.Participants[1];
  EXPECT_NEAR(made.TopY + 12.0f, out.Events[1].Y, 1.0f); // box centred on the create arrow
  EXPECT_GT(made.TopY, out.Participants[0].TopY);
  EXPECT_TRUE(made.Destroyed);
  EXPECT_EQ(made.LifelineEnd, out.Events[2].Y);
  EXPECT_LT(made.BottomY, 0.0f); // no mirrored box
}

TEST(imuml_sequence_layout, FragmentsEncloseTheirMessages) {
  ImUmlSeqInput in = participants(3);
  ImUmlSeqEventIn outer = event(ImUmlSeqEventType_FragmentBegin, -1, -1, ImVec2(60, 20));
  in.Events.push_back(outer); // 0
  in.Events.push_back(message(0, 1)); // 1
  in.Events.push_back(event(ImUmlSeqEventType_FragmentBegin, -1, -1, ImVec2(60, 20))); // 2
  in.Events.push_back(message(1, 2)); // 3
  in.Events.push_back(event(ImUmlSeqEventType_FragmentEnd)); // 4
  in.Events.push_back(event(ImUmlSeqEventType_FragmentElse, -1, -1, ImVec2(50, 14))); // 5
  in.Events.push_back(message(1, 0)); // 6
  in.Events.push_back(event(ImUmlSeqEventType_FragmentEnd)); // 7
  ImUmlSeqInput explicit_over = in;
  ImUmlSeqOutput out;
  ImUml::LayoutSequence(in, &out);
  const ImRect frame = out.Events[0].Rect;
  const ImRect inner = out.Events[2].Rect;
  for (int m : {1, 3, 6}) {
    const ImUmlSeqEventOut& e = out.Events[m];
    EXPECT_TRUE(frame.Contains(ImVec2(e.X1, e.Y)) && frame.Contains(ImVec2(e.X2, e.Y))) << "message " << m;
  }
  EXPECT_TRUE(frame.Contains(inner));
  EXPECT_GT(inner.Min.x, frame.Min.x);
  EXPECT_GT(out.Events[5].Rect.Min.y, inner.Max.y); // the else line is below the nested fragment
  EXPECT_EQ(out.Events[5].Rect.Min.x, frame.Min.x);

  // `over` sets the span: participants 0..2 in full.
  explicit_over.Events[2].A = 0;
  explicit_over.Events[2].B = 2;
  ImUml::LayoutSequence(explicit_over, &out);
  EXPECT_LE(out.Events[2].Rect.Min.x, out.Participants[0].X - 30.0f);
  EXPECT_GE(out.Events[2].Rect.Max.x, out.Participants[2].X + 30.0f);
}

TEST(imuml_sequence_layout, WholePixels) {
  ImUmlSeqInput in = participants(3, 57, 23);
  in.Events.push_back(message(0, 2, 133));
  in.Events.push_back(message(2, 1, 77, ImUmlMessageKind_Reply, ImUmlMessageFlags_Activate));
  ImUmlSeqOutput out;
  ImUml::LayoutSequence(in, &out);
  for (int i = 0; i < out.Participants.Size; ++i) {
    EXPECT_EQ(out.Participants[i].X, std::floor(out.Participants[i].X));
  }
  for (int i = 0; i < out.Events.Size; ++i) {
    EXPECT_EQ(out.Events[i].Y, std::floor(out.Events[i].Y));
  }
}

} // namespace
