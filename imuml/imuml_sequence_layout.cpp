// MIT License © 2026 Binary Dice Games
/// @file imuml_sequence_layout.cpp
/// @brief The sequence diagram layout: participants in columns, events in
///        rows (spec/03_uml_diagrams/DESIGN.md section 4).
#include "imuml_internal.h"

namespace {

float RoundPx(float v) {
  return ImFloor(v + 0.5f);
}

struct Requirement {
  int A, B; ///< Columns, A < B.
  float Need; ///< X[B] - X[A] must be at least this.
  int Order;
};

struct OpenFrame {
  int Event; ///< FragmentBegin event index.
  ImVector<int> Elses;
  float MinX, MaxX; ///< Extent of what is inside.
  bool Any;
};

void Extend(ImVector<OpenFrame*>& frames, float x0, float x1) {
  for (int i = 0; i < frames.Size; ++i) {
    OpenFrame& f = *frames[i];
    f.MinX = f.Any ? ImMin(f.MinX, x0) : x0;
    f.MaxX = f.Any ? ImMax(f.MaxX, x1) : x1;
    f.Any = true;
  }
}

} // namespace

void ImUml::LayoutSequence(const ImUmlSeqInput& in, ImUmlSeqOutput* out) {
  const int np = in.Participants.Size;
  const int ne = in.Events.Size;
  out->Participants.resize(np);
  out->Events.resize(ne);
  out->Activations.resize(0);
  out->Size = ImVec2(0, 0);
  for (int i = 0; i < ne; ++i) {
    ImUmlSeqEventOut& eo = out->Events[i];
    eo.Y = eo.Y2 = eo.X1 = eo.X2 = 0.0f;
    eo.Rect = ImRect(0, 0, 0, 0);
    eo.TextPos = ImVec2(0, 0);
    eo.Number = 0;
  }
  if (np == 0) {
    return;
  }
  const float aw = in.ActivationWidth;
  const float half_aw = RoundPx(aw * 0.5f);

  // ── Columns ──
  ImVector<float> gap;
  gap.resize(ImMax(0, np - 1));
  for (int i = 0; i + 1 < np; ++i) {
    gap[i] =
        ImCeil(in.Participants[i].BoxSize.x * 0.5f + in.Participants[i + 1].BoxSize.x * 0.5f + in.ParticipantSpacing);
  }
  ImVector<Requirement> reqs;
  for (int i = 0; i < ne; ++i) {
    const ImUmlSeqEventIn& ev = in.Events[i];
    if (ev.A < 0 || ev.A >= np) {
      continue;
    }
    Requirement r = {0, 0, 0.0f, i};
    if (ev.Type == ImUmlSeqEventType_Message) {
      if (ev.B < 0 || ev.B >= np) {
        continue;
      }
      if (ev.A == ev.B) {
        if (ev.A + 1 < np) {
          r.A = ev.A;
          r.B = ev.A + 1;
          r.Need = ImMax(ev.TextSize.x + 6.0f, in.SelfLoopWidth) + aw + 12.0f;
          reqs.push_back(r);
        }
        continue;
      }
      r.A = ImMin(ev.A, ev.B);
      r.B = ImMax(ev.A, ev.B);
      r.Need = ev.TextSize.x + 16.0f + aw * 2.0f;
      if (ev.Kind == ImUmlMessageKind_Create) {
        r.Need += in.Participants[ev.B].BoxSize.x * 0.5f;
      }
      reqs.push_back(r);
    } else if (ev.Type == ImUmlSeqEventType_NoteSide) {
      const bool right = ev.Side == ImUmlSide_Right;
      if (right && ev.A + 1 < np) {
        r.A = ev.A;
        r.B = ev.A + 1;
        r.Need = ev.TextSize.x + aw + 16.0f;
        reqs.push_back(r);
      } else if (!right && ev.A > 0) {
        r.A = ev.A - 1;
        r.B = ev.A;
        r.Need = ev.TextSize.x + aw + 16.0f;
        reqs.push_back(r);
      }
    } else if (ev.Type == ImUmlSeqEventType_NoteOver) {
      const int b = (ev.B < 0 || ev.B >= np) ? ev.A : ev.B;
      const int lo = ImMin(ev.A, b), hi = ImMax(ev.A, b);
      // A note over one participant must clear its neighbours' lifelines.
      if (lo == hi) {
        if (lo + 1 < np) {
          Requirement rr = {lo, lo + 1, ev.TextSize.x * 0.5f + 10.0f, i};
          reqs.push_back(rr);
        }
        if (lo > 0) {
          Requirement rl = {lo - 1, lo, ev.TextSize.x * 0.5f + 10.0f, i};
          reqs.push_back(rl);
        }
      }
    }
  }
  // Shortest spans first, so a wide label over several columns only widens
  // what the narrower ones left too small.
  for (int i = 1; i < reqs.Size; ++i) {
    Requirement v = reqs[i];
    int j = i - 1;
    while (j >= 0 && (reqs[j].B - reqs[j].A > v.B - v.A ||
                      (reqs[j].B - reqs[j].A == v.B - v.A && reqs[j].Order > v.Order))) {
      reqs[j + 1] = reqs[j];
      --j;
    }
    reqs[j + 1] = v;
  }
  for (int i = 0; i < reqs.Size; ++i) {
    const Requirement& r = reqs[i];
    float d = 0.0f;
    for (int k = r.A; k < r.B; ++k) {
      d += gap[k];
    }
    if (d < r.Need) {
      const float add = ImCeil((r.Need - d) / (float)(r.B - r.A));
      for (int k = r.A; k < r.B; ++k) {
        gap[k] += add;
      }
    }
  }
  ImVector<float> x;
  x.resize(np);
  x[0] = 0.0f;
  for (int i = 1; i < np; ++i) {
    x[i] = x[i - 1] + gap[i - 1];
  }

  // ── Rows ──
  float head_h = 0.0f;
  for (int i = 0; i < np; ++i) {
    head_h = ImMax(head_h, in.Participants[i].BoxSize.y);
  }
  for (int i = 0; i < np; ++i) {
    ImUmlSeqParticipantOut& po = out->Participants[i];
    po.X = x[i];
    po.TopY = head_h - in.Participants[i].BoxSize.y;
    po.LifelineStart = head_h;
    po.LifelineEnd = 0.0f;
    po.BottomY = -1.0f;
    po.Destroyed = false;
  }
  ImVector<bool> created_seen;
  created_seen.resize(np);
  for (int i = 0; i < np; ++i) {
    created_seen[i] = !in.Participants[i].Created;
  }
  // Activation stacks: start y per open activation.
  ImVector<ImVector<float>*> active;
  active.resize(np);
  for (int i = 0; i < np; ++i) {
    active[i] = IM_NEW(ImVector<float>)();
  }
  auto close_activation = [&](int p, float y_end) {
    ImVector<float>& st = *active[p];
    if (st.empty()) {
      return;
    }
    const float y0 = st.back();
    const int depth = st.Size - 1;
    st.pop_back();
    ImUmlSeqActivation a;
    a.Participant = p;
    const float left = x[p] - half_aw + depth * half_aw;
    a.Rect = ImRect(left, y0, left + aw, ImMax(y_end, y0 + 4.0f));
    out->Activations.push_back(a);
  };
  // Edge of participant p's activation bars on the side facing `towards`.
  auto edge_x = [&](int p, float towards) -> float {
    const int depth = active[p]->Size;
    if (depth == 0) {
      return x[p];
    }
    const float left = x[p] - half_aw;
    const float right = x[p] - half_aw + (depth - 1) * half_aw + aw;
    return towards >= x[p] ? right : left;
  };

  ImVector<OpenFrame*> frames;
  float y = head_h + in.MessageSpacing;
  int number = 0;
  for (int i = 0; i < ne; ++i) {
    const ImUmlSeqEventIn& ev = in.Events[i];
    ImUmlSeqEventOut& eo = out->Events[i];
    const bool valid_a = ev.A >= 0 && ev.A < np;
    switch (ev.Type) {
      case ImUmlSeqEventType_Message: {
        if (!valid_a || ev.B < 0 || ev.B >= np) {
          break;
        }
        eo.Number = ++number;
        const int a = ev.A, b = ev.B;
        if (a == b) {
          eo.TextPos = ImVec2(RoundPx(x[a] + aw + 6.0f), y);
          eo.Y = RoundPx(y + ev.TextSize.y + 3.0f);
          eo.Y2 = eo.Y + 18.0f;
          eo.X1 = edge_x(a, x[a] + 1.0f);
          if (ev.Flags & ImUmlMessageFlags_Activate) {
            active[b]->push_back(eo.Y2 - 4.0f);
          }
          eo.X2 = edge_x(a, x[a] + 1.0f);
          if (ev.Flags & ImUmlMessageFlags_Deactivate) {
            close_activation(a, eo.Y2);
          }
          Extend(frames, x[a] - half_aw, ImMax(eo.TextPos.x + ev.TextSize.x, eo.X1 + in.SelfLoopWidth));
          y = eo.Y2 + in.MessageSpacing;
          break;
        }
        eo.Y = RoundPx(y + ev.TextSize.y + 3.0f);
        eo.X1 = edge_x(a, x[b]);
        if (ev.Kind == ImUmlMessageKind_Create && !created_seen[b]) {
          created_seen[b] = true;
          const float h = in.Participants[b].BoxSize.y;
          out->Participants[b].TopY = RoundPx(eo.Y - h * 0.5f);
          out->Participants[b].LifelineStart = out->Participants[b].TopY + h;
          const float half_w = in.Participants[b].BoxSize.x * 0.5f;
          eo.X2 = x[b] + (x[a] < x[b] ? -half_w : half_w);
          if (ev.Flags & ImUmlMessageFlags_Activate) {
            active[b]->push_back(out->Participants[b].LifelineStart);
          }
        } else {
          if (ev.Flags & ImUmlMessageFlags_Activate) {
            active[b]->push_back(eo.Y);
          }
          eo.X2 = edge_x(b, x[a]);
        }
        if (ev.Flags & ImUmlMessageFlags_Deactivate) {
          close_activation(a, eo.Y);
        }
        if (ev.Kind == ImUmlMessageKind_Destroy) {
          while (!active[b]->empty()) {
            close_activation(b, eo.Y);
          }
          out->Participants[b].Destroyed = true;
          out->Participants[b].LifelineEnd = eo.Y;
        }
        const float mid = (eo.X1 + eo.X2) * 0.5f;
        eo.TextPos = ImVec2(RoundPx(mid - ev.TextSize.x * 0.5f), y);
        Extend(frames, ImMin(eo.X1, eo.X2), ImMax(eo.X1, eo.X2));
        y = eo.Y + in.MessageSpacing;
        if (ev.Kind == ImUmlMessageKind_Create) {
          y = ImMax(y, out->Participants[b].LifelineStart + in.MessageSpacing);
        }
        break;
      }
      case ImUmlSeqEventType_NoteOver: {
        if (!valid_a) {
          break;
        }
        const int b = (ev.B < 0 || ev.B >= np) ? ev.A : ev.B;
        const float lo = ImMin(x[ev.A], x[b]), hi = ImMax(x[ev.A], x[b]);
        const float width = ImMax(ev.TextSize.x, hi - lo + (lo == hi ? 0.0f : 2.0f * 20.0f));
        const float c = RoundPx((lo + hi) * 0.5f);
        eo.Rect = ImRect(c - RoundPx(width * 0.5f), y, c - RoundPx(width * 0.5f) + ImCeil(width), y + ev.TextSize.y);
        eo.TextPos = eo.Rect.Min;
        Extend(frames, eo.Rect.Min.x, eo.Rect.Max.x);
        y = eo.Rect.Max.y + in.MessageSpacing;
        break;
      }
      case ImUmlSeqEventType_NoteSide: {
        if (!valid_a) {
          break;
        }
        const float x0 = ev.Side == ImUmlSide_Right ? x[ev.A] + aw + 6.0f : x[ev.A] - aw - 6.0f - ev.TextSize.x;
        eo.Rect = ImRect(RoundPx(x0), y, RoundPx(x0) + ev.TextSize.x, y + ev.TextSize.y);
        eo.TextPos = eo.Rect.Min;
        Extend(frames, eo.Rect.Min.x, eo.Rect.Max.x);
        y = eo.Rect.Max.y + in.MessageSpacing;
        break;
      }
      case ImUmlSeqEventType_FragmentBegin: {
        OpenFrame* f = IM_NEW(OpenFrame)();
        f->Event = i;
        f->Any = false;
        f->MinX = f->MaxX = 0.0f;
        frames.push_back(f);
        eo.Rect.Min.y = y;
        eo.TextPos = ImVec2(0.0f, y);
        y += ev.TextSize.y + in.MessageSpacing;
        break;
      }
      case ImUmlSeqEventType_FragmentElse: {
        if (!frames.empty()) {
          frames.back()->Elses.push_back(i);
        }
        y += in.MessageSpacing * 0.5f;
        eo.Rect.Min.y = eo.Rect.Max.y = RoundPx(y);
        eo.TextPos = ImVec2(0.0f, RoundPx(y + 3.0f));
        y = RoundPx(y + 3.0f + ev.TextSize.y + in.MessageSpacing);
        break;
      }
      case ImUmlSeqEventType_FragmentEnd: {
        if (frames.empty()) {
          break;
        }
        OpenFrame* f = frames.back();
        frames.pop_back();
        const ImUmlSeqEventIn& begin = in.Events[f->Event];
        const float margin = 10.0f;
        float x1, x2;
        if (begin.A >= 0 && begin.A < np && begin.B >= 0 && begin.B < np) {
          x1 = ImMin(x[begin.A], x[begin.B]) - in.Participants[ImMin(begin.A, begin.B)].BoxSize.x * 0.5f;
          x2 = ImMax(x[begin.A], x[begin.B]) + in.Participants[ImMax(begin.A, begin.B)].BoxSize.x * 0.5f;
        } else if (f->Any) {
          x1 = f->MinX - margin;
          x2 = f->MaxX + margin;
        } else {
          x1 = x[0] - margin;
          x2 = x[0] + margin;
        }
        x2 = ImMax(x2, x1 + begin.TextSize.x + 20.0f);
        x1 = RoundPx(x1);
        x2 = RoundPx(x2);
        ImUmlSeqEventOut& bo = out->Events[f->Event];
        bo.Rect = ImRect(x1, bo.Rect.Min.y, x2, RoundPx(y - in.MessageSpacing * 0.5f));
        bo.TextPos.x = x1;
        eo.Rect = bo.Rect;
        for (int k = 0; k < f->Elses.Size; ++k) {
          ImUmlSeqEventOut& el = out->Events[f->Elses[k]];
          el.Rect.Min.x = x1;
          el.Rect.Max.x = x2;
          el.TextPos.x = x1;
        }
        Extend(frames, x1, x2);
        IM_DELETE(f);
        y = bo.Rect.Max.y + in.MessageSpacing;
        break;
      }
      default:
        break;
    }
  }
  for (int i = 0; i < frames.Size; ++i) {
    IM_DELETE(frames[i]);
  }
  y = RoundPx(y);
  for (int p = 0; p < np; ++p) {
    while (!active[p]->empty()) {
      close_activation(p, y);
    }
    IM_DELETE(active[p]);
    if (!out->Participants[p].Destroyed) {
      out->Participants[p].LifelineEnd = y;
      if (in.Mirror) {
        out->Participants[p].BottomY = y;
      }
    }
  }
  float bottom = y;
  if (in.Mirror) {
    bottom += head_h;
  }

  // ── Bounds ──
  float min_x = 1e30f, max_x = -1e30f;
  for (int p = 0; p < np; ++p) {
    min_x = ImMin(min_x, x[p] - in.Participants[p].BoxSize.x * 0.5f);
    max_x = ImMax(max_x, x[p] + in.Participants[p].BoxSize.x * 0.5f);
  }
  for (int i = 0; i < ne; ++i) {
    const ImUmlSeqEventIn& ev = in.Events[i];
    const ImUmlSeqEventOut& eo = out->Events[i];
    if (ev.Type == ImUmlSeqEventType_Message && ev.A == ev.B && ev.A >= 0 && ev.A < np) {
      max_x = ImMax(max_x, ImMax(eo.TextPos.x + ev.TextSize.x, eo.X1 + in.SelfLoopWidth));
    } else if (ev.Type != ImUmlSeqEventType_Message && ev.Type != ImUmlSeqEventType_FragmentElse) {
      if (eo.Rect.Max.x > eo.Rect.Min.x) {
        min_x = ImMin(min_x, eo.Rect.Min.x);
        max_x = ImMax(max_x, eo.Rect.Max.x);
      }
    }
  }
  const float shift = RoundPx(-min_x);
  for (int p = 0; p < np; ++p) {
    out->Participants[p].X += shift;
  }
  for (int i = 0; i < ne; ++i) {
    ImUmlSeqEventOut& eo = out->Events[i];
    eo.X1 += shift;
    eo.X2 += shift;
    eo.TextPos.x += shift;
    eo.Rect.Min.x += shift;
    eo.Rect.Max.x += shift;
  }
  for (int i = 0; i < out->Activations.Size; ++i) {
    out->Activations[i].Rect.Min.x += shift;
    out->Activations[i].Rect.Max.x += shift;
  }
  out->Size = ImVec2(ImCeil(max_x - min_x), ImCeil(bottom));
}
