// MIT License © 2026 Binary Dice Games
/// @file imuml_items.cpp
/// @brief The End* functions: measure the recorded items, lay them out
///        (cached under a hash of the items), fit them to the reserved
///        space and draw them.
#include "imuml_internal.h"

extern ImUmlContext* GImUml;

namespace ImUml {
ImUmlStr AddString(ImUmlDiagram* d, const char* text, const char* end);
const char* StrBegin(const ImUmlDiagram* d, const ImUmlStr& s);
const char* StrEnd(const ImUmlDiagram* d, const ImUmlStr& s);
} // namespace ImUml

namespace {

using ImUml::StrBegin;
using ImUml::StrEnd;

const float kMargin = 4.0f; // around the content, so outlines are not clipped

float RoundPx(float v) {
  return ImFloor(v + 0.5f);
}

ImVec2 CeilPx(const ImVec2& v) {
  return ImVec2(ImCeil(v.x), ImCeil(v.y));
}

// ── Text ─────────────────────────────────────────────────────────────────────

ImVec2 TextSize(const ImUmlTextStyle& ts, float zoom, const char* b, const char* e, ImFont* font = nullptr) {
  if (b == e) {
    return ImVec2(0.0f, 0.0f);
  }
  ImFont* f = font != nullptr ? font : ts.Font;
  return f->CalcTextSizeA(ts.Size * zoom, FLT_MAX, 0.0f, b, e);
}

float LineHeight(const ImUmlTextStyle& ts, float zoom) {
  return ts.Size * zoom;
}

/// Draws each line of [b, e) centred on `cx`, the first line's top at `y`.
void TextCentered(ImDrawList* dl, const ImUmlTextStyle& ts, float zoom, float cx, float y, const char* b, const char* e,
                  ImU32 col, ImFont* font = nullptr) {
  ImFont* f = font != nullptr ? font : ts.Font;
  const float size = ts.Size * zoom;
  while (b < e) {
    const char* nl = (const char*)memchr(b, '\n', (size_t)(e - b));
    const char* line_end = nl != nullptr ? nl : e;
    const float w = f->CalcTextSizeA(size, FLT_MAX, 0.0f, b, line_end).x;
    dl->AddText(f, size, ImVec2(ImFloor(cx - w * 0.5f + 0.5f), ImFloor(y + 0.5f)), col, b, line_end);
    y += size;
    b = nl != nullptr ? nl + 1 : e;
  }
}

void TextAt(ImDrawList* dl, const ImUmlTextStyle& ts, float zoom, const ImVec2& p, const char* b, const char* e,
            ImU32 col, ImFont* font = nullptr) {
  if (b == e) {
    return;
  }
  dl->AddText(font != nullptr ? font : ts.Font, ts.Size * zoom, ImVec2(ImFloor(p.x + 0.5f), ImFloor(p.y + 0.5f)), col,
              b, e);
}

const char* StereotypeOf(const ImUmlDiagram* d, const ImUmlNodeRec& n) {
  if (!n.Stereotype.Empty()) {
    return nullptr; // the custom one, drawn from the buffer
  }
  switch (n.Kind) {
    case ImUmlClassKind_Abstract:
      return "\xC2\xAB" "abstract\xC2\xBB";
    case ImUmlClassKind_Interface:
      return "\xC2\xABinterface\xC2\xBB";
    case ImUmlClassKind_Enum:
      return "\xC2\xAB" "enumeration\xC2\xBB";
    default:
      (void)d;
      return "";
  }
}

/// Builds "«text»" for a custom stereotype into `buf`.
const char* GuillemetText(const ImUmlDiagram* d, const ImUmlNodeRec& n, char* buf, int size) {
  const char* fixed = StereotypeOf(d, n);
  if (fixed != nullptr) {
    return fixed;
  }
  ImFormatString(buf, (size_t)size, "\xC2\xAB%.*s\xC2\xBB", n.Stereotype.Len, StrBegin(d, n.Stereotype));
  return buf;
}

// ── Node geometry ────────────────────────────────────────────────────────────

/// Sizes of a class box's parts at zoom 1.
struct ClassGeometry {
  float Width;
  float Header; ///< Stereotype and name.
  float Attributes, Methods; ///< Compartment heights; 0 when the class is a single box.
  bool Compartments;
};

ImFont* ItalicFont(const ImUmlDiagram* d) {
  return d->Style.ItalicFont;
}

ClassGeometry MeasureClass(const ImUmlDiagram* d, const ImUmlNodeRec& n) {
  const ImVec2 pad = d->Style.NodePadding;
  ClassGeometry g;
  char buf[256];
  const char* st = GuillemetText(d, n, buf, IM_ARRAYSIZE(buf));
  const bool abstract = n.Kind == ImUmlClassKind_Abstract;
  ImFont* name_font = abstract && ItalicFont(d) != nullptr ? ItalicFont(d) : nullptr;
  float w = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name), name_font).x;
  float h = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name), name_font).y;
  if (st[0] != 0) {
    const ImVec2 s = TextSize(n.Text, 1.0f, st, st + strlen(st));
    w = ImMax(w, s.x);
    h += s.y;
  }
  g.Header = h + pad.y * 2.0f;
  g.Attributes = g.Methods = 0.0f;
  g.Compartments = n.LineCount > 0 || n.Kind != ImUmlClassKind_Class;
  if (g.Compartments) {
    float sections[2] = {0.0f, 0.0f};
    for (int i = 0; i < n.LineCount; ++i) {
      const ImUmlLine& line = d->Lines[n.FirstLine + i];
      ImFont* f = line.Abstract && ItalicFont(d) != nullptr ? ItalicFont(d) : nullptr;
      ImVec2 s = TextSize(n.Text, 1.0f, StrBegin(d, line.Text), StrEnd(d, line.Text), f);
      if (line.Abstract && f == nullptr) {
        s.x += TextSize(n.Text, 1.0f, " {abstract}", nullptr).x;
      }
      w = ImMax(w, s.x);
      sections[line.Section] += ImMax(s.y, LineHeight(n.Text, 1.0f));
    }
    g.Attributes = sections[0] + pad.y * 2.0f;
    g.Methods = sections[1] + pad.y * 2.0f;
  }
  g.Width = ImMax(w + pad.x * 2.0f, 60.0f);
  return g;
}

bool IsBar(const ImUmlNodeRec& n) {
  return (n.Type == ImUmlNodeType_State && (n.Kind == ImUmlStateKind_Fork || n.Kind == ImUmlStateKind_Join)) ||
         (n.Type == ImUmlNodeType_Control && (n.Kind == ImUmlControlKind_Fork || n.Kind == ImUmlControlKind_Join));
}

bool Across(const ImUmlDiagram* d) {
  return d->Direction == ImUmlDirection_LeftRight || d->Direction == ImUmlDirection_RightLeft;
}

/// Size and shape of a node that is not a composite state, at zoom 1.
void MeasureNode(const ImUmlDiagram* d, ImUmlNodeRec& n) {
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 text = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name));
  n.Shape = ImUmlShape_Box;
  switch (n.Type) {
    case ImUmlNodeType_Class: {
      const ClassGeometry g = MeasureClass(d, n);
      n.Size = ImVec2(g.Width, g.Header + g.Attributes + g.Methods);
      break;
    }
    case ImUmlNodeType_State:
      switch (n.Kind) {
        case ImUmlStateKind_Initial:
          n.Size = ImVec2(16, 16);
          n.Shape = ImUmlShape_Circle;
          break;
        case ImUmlStateKind_Final:
          n.Size = ImVec2(20, 20);
          n.Shape = ImUmlShape_Circle;
          break;
        case ImUmlStateKind_Choice:
          n.Size = ImVec2(26, 26);
          n.Shape = ImUmlShape_Diamond;
          break;
        case ImUmlStateKind_Fork:
        case ImUmlStateKind_Join:
          n.Size = Across(d) ? ImVec2(7, 70) : ImVec2(70, 7);
          break;
        case ImUmlStateKind_History:
        case ImUmlStateKind_DeepHistory:
          n.Size = ImVec2(26, 26);
          n.Shape = ImUmlShape_Circle;
          break;
        default: {
          float w = text.x, h = text.y;
          if (n.LineCount > 0) {
            h += pad.y * 2.0f;
            for (int i = 0; i < n.LineCount; ++i) {
              const ImUmlLine& line = d->Lines[n.FirstLine + i];
              const ImVec2 s = TextSize(n.Text, 1.0f, StrBegin(d, line.Text), StrEnd(d, line.Text));
              w = ImMax(w, s.x);
              h += ImMax(s.y, LineHeight(n.Text, 1.0f));
            }
          }
          n.Size = ImVec2(ImMax(w + pad.x * 2.0f + d->Style.NodeRounding, 50.0f), h + pad.y * 2.0f);
          break;
        }
      }
      break;
    case ImUmlNodeType_Action: {
      const float point = n.Kind == ImUmlActionKind_Action ? 0.0f : (text.y + pad.y * 2.0f) * 0.5f;
      n.Size = ImVec2(text.x + pad.x * 2.0f + d->Style.NodeRounding + point, text.y + pad.y * 2.0f);
      break;
    }
    case ImUmlNodeType_Control:
      switch (n.Kind) {
        case ImUmlControlKind_Initial:
          n.Size = ImVec2(16, 16);
          n.Shape = ImUmlShape_Circle;
          break;
        case ImUmlControlKind_Final:
          n.Size = ImVec2(20, 20);
          n.Shape = ImUmlShape_Circle;
          break;
        case ImUmlControlKind_FlowFinal:
          n.Size = ImVec2(18, 18);
          n.Shape = ImUmlShape_Circle;
          break;
        case ImUmlControlKind_Fork:
        case ImUmlControlKind_Join:
          n.Size = Across(d) ? ImVec2(7, 70) : ImVec2(70, 7);
          break;
        default:
          n.Size = ImVec2(30, 22);
          n.Shape = ImUmlShape_Diamond;
          break;
      }
      break;
    case ImUmlNodeType_Actor:
      n.Size = ImVec2(ImMax(36.0f, text.x + 4.0f), 40.0f + 4.0f + text.y);
      break;
    case ImUmlNodeType_UseCase: {
      // The ellipse through the corners of the padded text box, widened a
      // little so short names do not look round.
      const float a = (text.x * 0.5f + pad.x) * 1.42f, b = (text.y * 0.5f + pad.y) * 1.42f;
      n.Size = ImVec2(ImMax(a * 2.0f, 60.0f), ImMax(b * 2.0f, 30.0f));
      n.Shape = ImUmlShape_Ellipse;
      break;
    }
    case ImUmlNodeType_Note:
      n.Size = ImVec2(text.x + pad.x * 2.0f + d->Style.NoteFold, text.y + pad.y * 2.0f);
      break;
    default:
      n.Size = text + pad * 2.0f;
      break;
  }
  n.Size = CeilPx(n.Size);
}

/// Composite state header: the name band.
float CompositeHeader(const ImUmlDiagram* d, const ImUmlNodeRec& n) {
  return ImCeil(TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name)).y + d->Style.NodePadding.y * 2.0f);
}

// ── Edge styles ──────────────────────────────────────────────────────────────

struct EdgeLook {
  bool Dashed;
  ImUmlMarker AtFrom, AtTo;
  bool RankToFirst; ///< The layout ranks `to` above `from`.
};

EdgeLook LookOf(const ImUmlDiagram* d, const ImUmlEdgeRec& e) {
  EdgeLook l = {false, ImUmlMarker_None, ImUmlMarker_None, false};
  switch (e.Type) {
    case ImUmlEdgeType_Relation:
      switch (e.Kind) {
        case ImUmlRelationKind_DirectedAssociation:
          l.AtTo = ImUmlMarker_Open;
          break;
        case ImUmlRelationKind_Inheritance:
          l.AtTo = ImUmlMarker_Hollow;
          l.RankToFirst = true;
          break;
        case ImUmlRelationKind_Realization:
          l.AtTo = ImUmlMarker_Hollow;
          l.Dashed = true;
          l.RankToFirst = true;
          break;
        case ImUmlRelationKind_Dependency:
          l.AtTo = ImUmlMarker_Open;
          l.Dashed = true;
          break;
        case ImUmlRelationKind_Aggregation:
          l.AtFrom = ImUmlMarker_HollowDiamond;
          break;
        case ImUmlRelationKind_Composition:
          l.AtFrom = ImUmlMarker_FilledDiamond;
          break;
        default:
          break;
      }
      break;
    case ImUmlEdgeType_Transition:
    case ImUmlEdgeType_Flow:
      l.AtTo = ImUmlMarker_Filled;
      break;
    case ImUmlEdgeType_UseCaseLink:
      switch (e.Kind) {
        case ImUmlUseCaseLinkKind_Include:
        case ImUmlUseCaseLinkKind_Extend:
          l.AtTo = ImUmlMarker_Open;
          l.Dashed = true;
          break;
        case ImUmlUseCaseLinkKind_Generalization:
          l.AtTo = ImUmlMarker_Hollow;
          l.RankToFirst = true;
          break;
        default:
          break;
      }
      break;
    case ImUmlEdgeType_NoteLink:
      l.Dashed = true;
      break;
    default:
      break;
  }
  (void)d;
  return l;
}

// ── Layout ───────────────────────────────────────────────────────────────────

ImGuiID HashItems(const ImUmlDiagram* d) {
  ImGuiID h = ImHashData(&d->Kind, sizeof(d->Kind), 0x5eed);
  h = ImHashData(&d->Direction, sizeof(d->Direction), h);
  h = ImHashData(&d->EdgeStyle, sizeof(d->EdgeStyle), h);
  h = ImHashData(&d->NodeSpacing, sizeof(d->NodeSpacing), h);
  h = ImHashData(&d->RankSpacing, sizeof(d->RankSpacing), h);
  h = ImHashData(&d->SequenceFlags, sizeof(d->SequenceFlags), h);
  h = ImHashData(&d->SystemLabel, sizeof(d->SystemLabel), h);
  h = ImHashData(&d->Style, sizeof(d->Style) - sizeof(d->Style.Colors) - sizeof(ImFont*), h);
  h = ImHashData(&d->Style.ItalicFont, sizeof(ImFont*), h);
  if (d->TextBuf.Size > 0) {
    h = ImHashData(d->TextBuf.Data, (size_t)d->TextBuf.Size, h);
  }
  if (d->Nodes.Size > 0) {
    h = ImHashData(d->Nodes.Data, sizeof(ImUmlNodeRec) * (size_t)d->Nodes.Size, h);
  }
  if (d->Lines.Size > 0) {
    h = ImHashData(d->Lines.Data, sizeof(ImUmlLine) * (size_t)d->Lines.Size, h);
  }
  if (d->Edges.Size > 0) {
    h = ImHashData(d->Edges.Data, sizeof(ImUmlEdgeRec) * (size_t)d->Edges.Size, h);
  }
  if (d->AttachIds.Size > 0) {
    h = ImHashData(d->AttachIds.Data, sizeof(ImGuiID) * (size_t)d->AttachIds.Size, h);
  }
  if (d->Events.Size > 0) {
    h = ImHashData(d->Events.Data, sizeof(ImUmlSeqEventRec) * (size_t)d->Events.Size, h);
  }
  return h;
}

/// Node index of every id; the first node wins on a duplicate.
void IndexNodes(const ImUmlDiagram* d, ImGuiStorage* map) {
  map->Clear();
  for (int i = 0; i < d->Nodes.Size; ++i) {
    if (map->GetInt(d->Nodes[i].Id, 0) == 0) {
      map->SetInt(d->Nodes[i].Id, i + 1);
    }
  }
}

bool InScope(const ImUmlDiagram* d, int node, int scope) {
  if (scope < 0) {
    return true;
  }
  for (int p = d->Nodes[node].Parent; p >= 0; p = d->Nodes[p].Parent) {
    if (p == scope) {
      return true;
    }
  }
  return false;
}

/// The ancestor of `node` (or itself) whose parent is `scope`.
int ChildOfScope(const ImUmlDiagram* d, int node, int scope) {
  int v = node;
  while (v >= 0 && d->Nodes[v].Parent != scope) {
    v = d->Nodes[v].Parent;
  }
  return v;
}

int EdgeScope(const ImUmlDiagram* d, int from, int to) {
  for (int a = d->Nodes[from].Parent;; a = d->Nodes[a].Parent) {
    if (InScope(d, to, a)) {
      return a;
    }
    if (a < 0) {
      return -1;
    }
  }
}

/// A note link edge per attached node (appended to d->Edges; recorded
/// with Type NoteLink so they are drawn dashed).
void AddNoteLinks(ImUmlDiagram* d, const ImGuiStorage& ids) {
  const int count = d->Nodes.Size;
  for (int i = 0; i < count; ++i) {
    const ImUmlNodeRec& n = d->Nodes[i];
    if (n.Type != ImUmlNodeType_Note) {
      continue;
    }
    for (int k = 0; k < n.AttachCount; ++k) {
      const int target = ids.GetInt(d->AttachIds[n.FirstAttach + k], 0) - 1;
      if (target < 0) {
        d->SkippedItems++;
        continue;
      }
      ImUmlEdgeRec e;
      memset((void*)&e, 0, sizeof(e));
      e.Type = ImUmlEdgeType_NoteLink;
      e.Label = e.FromLabel = e.ToLabel = ImUmlStr();
      e.From = i;
      e.To = target;
      e.Weight = 1;
      e.MinLen = 0;
      e.Constraint = false;
      e.Col = 0;
      e.Text = n.Text;
      d->Edges.push_back(e);
    }
  }
}

struct GraphScope {
  ImVector<int> Nodes; ///< Diagram node indices.
  ImVector<int> Edges; ///< Diagram edge indices.
};

/// Lays out the nodes whose parent is `scope` (after their composites'
/// insides), in coordinates relative to the scope's content origin.
/// Returns the scope's content size.
ImVec2 LayoutScope(ImUmlDiagram* d, int scope, const ImVector<int>& edge_scope, ImVector<ImVec2>* local_pos,
                   ImVector<ImVec2>* points, ImVector<ImUmlGraphEdgeOut>* edge_out) {
  GraphScope s;
  for (int i = 0; i < d->Nodes.Size; ++i) {
    if (d->Nodes[i].Parent == scope) {
      s.Nodes.push_back(i);
    }
  }
  for (int e = 0; e < d->Edges.Size; ++e) {
    if (edge_scope[e] == scope && d->Edges[e].From >= 0) {
      s.Edges.push_back(e);
    }
  }
  const ImVec2 pad = d->Style.NodePadding;
  for (int k = 0; k < s.Nodes.Size; ++k) {
    ImUmlNodeRec& n = d->Nodes[s.Nodes[k]];
    if (n.Composite) {
      const ImVec2 inner = LayoutScope(d, s.Nodes[k], edge_scope, local_pos, points, edge_out);
      n.HeaderHeight = CompositeHeader(d, n);
      const float name_w = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name)).x + pad.x * 2.0f;
      n.Size = CeilPx(ImVec2(ImMax(inner.x + pad.x * 4.0f, name_w), inner.y + n.HeaderHeight + pad.y * 3.0f));
      n.Shape = ImUmlShape_Box;
    }
  }
  ImUmlGraphInput in;
  in.Direction = d->Direction;
  in.EdgeStyle = d->EdgeStyle;
  in.NodeSpacing = d->NodeSpacing;
  in.RankSpacing = d->RankSpacing;
  ImGuiStorage local;
  int next_group = d->RankGroups.Data.Size;
  for (int k = 0; k < s.Nodes.Size; ++k) {
    const ImUmlNodeRec& n = d->Nodes[s.Nodes[k]];
    ImUmlGraphNode gn;
    gn.Size = n.Size;
    gn.Shape = n.Shape;
    gn.RankGroup = n.RankGroup;
    if (n.Type == ImUmlNodeType_Actor) {
      gn.Pin = n.Side == ImUmlSide_Right ? 2 : 1;
    }
    local.SetInt((ImGuiID)s.Nodes[k] + 1, k + 1);
    in.Nodes.push_back(gn);
  }
  // A note shares the rank of the first node it is attached to.
  for (int k = 0; k < s.Edges.Size; ++k) {
    const ImUmlEdgeRec& e = d->Edges[s.Edges[k]];
    if (e.Type != ImUmlEdgeType_NoteLink) {
      continue;
    }
    const int note = local.GetInt((ImGuiID)e.From + 1, 0) - 1;
    const int target = local.GetInt((ImGuiID)e.To + 1, 0) - 1;
    if (note < 0 || target < 0 || in.Nodes[note].RankGroup >= 0 || in.Nodes[note].Pin != 0) {
      continue;
    }
    if (in.Nodes[target].RankGroup < 0) {
      in.Nodes[target].RankGroup = next_group++;
    }
    in.Nodes[note].RankGroup = in.Nodes[target].RankGroup;
  }
  ImVector<bool> swapped;
  for (int k = 0; k < s.Edges.Size; ++k) {
    const ImUmlEdgeRec& e = d->Edges[s.Edges[k]];
    const EdgeLook look = LookOf(d, e);
    ImUmlGraphEdge ge;
    int from = local.GetInt((ImGuiID)ChildOfScope(d, e.From, scope) + 1, 0) - 1;
    int to = local.GetInt((ImGuiID)ChildOfScope(d, e.To, scope) + 1, 0) - 1;
    bool swap = look.RankToFirst;
    // Use case associations: from the left actor in, out to the right one.
    if (e.Type == ImUmlEdgeType_UseCaseLink) {
      const ImUmlNodeRec& a = d->Nodes[e.From];
      const ImUmlNodeRec& b = d->Nodes[e.To];
      if (a.Type == ImUmlNodeType_Actor && b.Type == ImUmlNodeType_Actor) {
        ge.Constraint = false;
      } else if (a.Type == ImUmlNodeType_Actor) {
        swap = a.Side == ImUmlSide_Right;
      } else if (b.Type == ImUmlNodeType_Actor) {
        swap = b.Side != ImUmlSide_Right;
      }
    }
    if (swap) {
      const int t = from;
      from = to;
      to = t;
    }
    swapped.push_back(swap);
    ge.From = from;
    ge.To = to;
    ge.Weight = e.Weight;
    ge.MinLen = e.MinLen;
    ge.Constraint = ge.Constraint && e.Constraint;
    if (!e.Label.Empty()) {
      ge.LabelSize = CeilPx(TextSize(e.Text, 1.0f, StrBegin(d, e.Label), StrEnd(d, e.Label)) + ImVec2(6.0f, 2.0f));
    }
    d->Edges[s.Edges[k]].LabelSize = ge.LabelSize;
    in.Edges.push_back(ge);
  }
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  for (int k = 0; k < s.Nodes.Size; ++k) {
    (*local_pos)[s.Nodes[k]] = out.NodePos[k];
  }
  for (int k = 0; k < s.Edges.Size; ++k) {
    const ImUmlGraphEdgeOut& eo = out.Edges[k];
    ImUmlGraphEdgeOut& dst = (*edge_out)[s.Edges[k]];
    dst = eo;
    dst.PointStart = points->Size;
    if (swapped[k]) {
      for (int i = eo.PointCount - 1; i >= 0; --i) {
        points->push_back(out.Points[eo.PointStart + i]);
      }
    } else {
      for (int i = 0; i < eo.PointCount; ++i) {
        points->push_back(out.Points[eo.PointStart + i]);
      }
    }
  }
  return out.Size;
}

/// Moves a scope's nodes (and, recursively, its composites' insides) from
/// local to diagram coordinates.
void PlaceScope(ImUmlDiagram* d, int scope, const ImVec2& origin, const ImVector<ImVec2>& local_pos,
                const ImVector<int>& edge_scope, ImVector<ImUmlGraphEdgeOut>* edge_out, ImVector<ImVec2>* points) {
  const ImVec2 pad = d->Style.NodePadding;
  for (int i = 0; i < d->Nodes.Size; ++i) {
    ImUmlNodeRec& n = d->Nodes[i];
    if (n.Parent != scope) {
      continue;
    }
    n.Pos = origin + local_pos[i];
    if (n.Composite) {
      const ImVec2 inner(n.Pos.x - n.Size.x * 0.5f + pad.x * 2.0f, n.Pos.y - n.Size.y * 0.5f + n.HeaderHeight + pad.y);
      PlaceScope(d, i, inner, local_pos, edge_scope, edge_out, points);
    }
  }
  for (int e = 0; e < d->Edges.Size; ++e) {
    if (edge_scope[e] != scope || d->Edges[e].From < 0) {
      continue;
    }
    ImUmlGraphEdgeOut& eo = (*edge_out)[e];
    for (int i = 0; i < eo.PointCount; ++i) {
      (*points)[eo.PointStart + i] += origin;
    }
    eo.LabelPos += origin;
  }
}

/// An edge drawn to a node inside a composite ends at the composite in its
/// scope's layout. Its path is rebuilt to end on the inner node instead:
/// the end point on the composite's border is replaced by the inner node's
/// border and the whole path is smoothed again, so it has no corner where
/// it enters the composite.
void ExtendIntoComposites(ImUmlDiagram* d, const ImVector<int>& edge_scope, ImVector<ImUmlGraphEdgeOut>* edge_out,
                          ImVector<ImVec2>* points) {
  ImVector<ImVec2> line, smooth;
  for (int e = 0; e < d->Edges.Size; ++e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    if (er.From < 0) {
      continue;
    }
    const int scope = edge_scope[e];
    const bool deep_from = ChildOfScope(d, er.From, scope) != er.From;
    const bool deep_to = ChildOfScope(d, er.To, scope) != er.To;
    ImUmlGraphEdgeOut& eo = (*edge_out)[e];
    if ((!deep_from && !deep_to) || eo.PointCount < 2) {
      continue;
    }
    // The points the path passes through (a Bézier chain's every third).
    line.resize(0);
    const int step = eo.Bezier ? 3 : 1;
    for (int i = 0; i < eo.PointCount; i += step) {
      line.push_back((*points)[eo.PointStart + i]);
    }
    if (line.Size < 2) {
      continue;
    }
    if (deep_to) {
      const ImUmlNodeRec& n = d->Nodes[er.To];
      const ImVec2 q = ImUml::ClipToShape(n.Shape, n.Pos, n.Size, line[line.Size - 2]);
      line.back() = ImVec2(RoundPx(q.x), RoundPx(q.y));
    }
    if (deep_from) {
      const ImUmlNodeRec& n = d->Nodes[er.From];
      const ImVec2 q = ImUml::ClipToShape(n.Shape, n.Pos, n.Size, line[1]);
      line[0] = ImVec2(RoundPx(q.x), RoundPx(q.y));
    }
    const ImVector<ImVec2>* out = &line;
    if (eo.Bezier) {
      ImUml::SmoothPath(line, &smooth, !Across(d));
      out = &smooth;
    }
    eo.PointStart = points->Size;
    eo.PointCount = out->Size;
    for (int i = 0; i < out->Size; ++i) {
      points->push_back((*out)[i]);
    }
  }
}

/// The graph diagrams' layout; fills node Pos/Size and d->CacheEdges /
/// d->Points, and returns the content size.
ImVec2 LayoutGraphDiagram(ImUmlDiagram* d) {
  // A note attached to a node inside a composite state is laid out next to
  // it, in the composite.
  for (int e = 0; e < d->Edges.Size; ++e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    if (er.Type != ImUmlEdgeType_NoteLink) {
      continue;
    }
    ImUmlNodeRec& note = d->Nodes[er.From];
    const ImUmlNodeRec& target = d->Nodes[er.To];
    if (note.FirstAttach >= 0 && d->AttachIds[note.FirstAttach] == target.Id && note.Parent != target.Parent &&
        InScope(d, er.To, note.Parent)) {
      note.Parent = target.Parent;
    }
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    d->Nodes[i].Composite = false;
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    if (d->Nodes[i].Parent >= 0) {
      d->Nodes[d->Nodes[i].Parent].Composite = true;
    }
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    if (!d->Nodes[i].Composite) {
      MeasureNode(d, d->Nodes[i]);
    }
  }
  ImVector<int> edge_scope;
  edge_scope.resize(d->Edges.Size);
  for (int e = 0; e < d->Edges.Size; ++e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    edge_scope[e] = er.From >= 0 ? EdgeScope(d, er.From, er.To) : -2;
  }
  ImVector<ImVec2> local_pos;
  local_pos.resize(d->Nodes.Size);
  d->CacheEdges.resize(d->Edges.Size);
  for (int e = 0; e < d->Edges.Size; ++e) {
    ImUmlGraphEdgeOut& eo = d->CacheEdges[e];
    eo.PointStart = eo.PointCount = 0;
    eo.Bezier = false;
    eo.LabelPos = ImVec2(0, 0);
  }
  d->Points.resize(0);
  ImVec2 size = LayoutScope(d, -1, edge_scope, &local_pos, &d->Points, &d->CacheEdges);

  // The use case boundary: room around the use cases, between the actors.
  ImVec2 origin(kMargin, kMargin);
  const bool boundary = d->Kind == ImUmlDiagramKind_UseCase && !d->SystemLabel.Empty();
  const ImUmlTextStyle& ts = d->TitleStyle;
  const float boundary_header =
      boundary ? ImCeil(TextSize(ts, 1.0f, StrBegin(d, d->SystemLabel), StrEnd(d, d->SystemLabel)).y) + 10.0f : 0.0f;
  if (boundary) {
    origin.y += boundary_header;
  }
  PlaceScope(d, -1, origin, local_pos, edge_scope, &d->CacheEdges, &d->Points);
  ExtendIntoComposites(d, edge_scope, &d->CacheEdges, &d->Points);
  size.y += boundary_header;
  d->Boundary = ImRect(0, 0, 0, 0);
  if (boundary) {
    bool any = false;
    ImRect r;
    for (int i = 0; i < d->Nodes.Size; ++i) {
      const ImUmlNodeRec& n = d->Nodes[i];
      if (n.Type == ImUmlNodeType_UseCase) {
        const ImRect nr(n.Pos - n.Size * 0.5f, n.Pos + n.Size * 0.5f);
        if (!any) {
          r = nr;
          any = true;
        } else {
          r.Add(nr);
        }
      }
    }
    if (any) {
      r.Expand(ImMax(8.0f, d->NodeSpacing * 0.35f));
      r.Min.y -= boundary_header;
      const float label_w = TextSize(ts, 1.0f, StrBegin(d, d->SystemLabel), StrEnd(d, d->SystemLabel)).x + 20.0f;
      if (r.GetWidth() < label_w) {
        r.Max.x = r.Min.x + label_w;
      }
      d->Boundary = ImRect(ImFloor(r.Min), ImFloor(r.Max));
    }
  }
  // Content bounds, including edge labels and the boundary.
  ImRect bounds(ImVec2(kMargin, kMargin), ImVec2(kMargin, kMargin) + size);
  for (int i = 0; i < d->Nodes.Size; ++i) {
    const ImUmlNodeRec& n = d->Nodes[i];
    bounds.Add(ImRect(n.Pos - n.Size * 0.5f, n.Pos + n.Size * 0.5f));
  }
  for (int e = 0; e < d->Edges.Size; ++e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    const ImUmlGraphEdgeOut& eo = d->CacheEdges[e];
    for (int i = 0; i < eo.PointCount; ++i) {
      bounds.Add(d->Points[eo.PointStart + i]);
    }
    if (!er.Label.Empty()) {
      bounds.Add(ImRect(eo.LabelPos - er.LabelSize * 0.5f, eo.LabelPos + er.LabelSize * 0.5f));
    }
  }
  if (d->Boundary.GetWidth() > 0) {
    bounds.Add(d->Boundary);
  }
  // Shift so the bounds start at the margin.
  const ImVec2 shift = ImVec2(kMargin, kMargin) - bounds.Min;
  if (shift.x != 0.0f || shift.y != 0.0f) {
    for (int i = 0; i < d->Nodes.Size; ++i) {
      d->Nodes[i].Pos += shift;
    }
    for (int i = 0; i < d->Points.Size; ++i) {
      d->Points[i] += shift;
    }
    for (int e = 0; e < d->CacheEdges.Size; ++e) {
      d->CacheEdges[e].LabelPos += shift;
    }
    d->Boundary.Translate(shift);
  }
  return CeilPx(bounds.GetSize() + ImVec2(kMargin, kMargin) * 2.0f);
}

// ── Sequence layout ──────────────────────────────────────────────────────────

const char* FragmentKeyword(int kind) {
  switch (kind) {
    case ImUmlFragmentKind_Loop:
      return "loop";
    case ImUmlFragmentKind_Alt:
      return "alt";
    case ImUmlFragmentKind_Opt:
      return "opt";
    case ImUmlFragmentKind_Par:
      return "par";
    case ImUmlFragmentKind_Critical:
      return "critical";
    case ImUmlFragmentKind_Break:
      return "break";
    default:
      return "";
  }
}

ImVec2 ParticipantBox(const ImUmlDiagram* d, const ImUmlNodeRec& n) {
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 text = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name));
  switch (n.Kind) {
    case ImUmlParticipantKind_Actor:
      return CeilPx(ImVec2(ImMax(text.x + 4.0f, 30.0f), 36.0f + 4.0f + text.y));
    case ImUmlParticipantKind_Boundary:
    case ImUmlParticipantKind_Control:
    case ImUmlParticipantKind_Entity:
      return CeilPx(ImVec2(ImMax(text.x + 4.0f, 40.0f), 28.0f + 4.0f + text.y));
    case ImUmlParticipantKind_Database:
      return CeilPx(ImVec2(ImMax(text.x + pad.x * 2.0f, 50.0f), text.y + pad.y * 2.0f + 16.0f));
    default:
      return CeilPx(ImVec2(ImMax(text.x + pad.x * 2.0f, 50.0f), text.y + pad.y * 2.0f));
  }
}

/// Message text as drawn: "N. text" when numbered.
void MessageText(const ImUmlDiagram* d, const ImUmlSeqEventRec& ev, int number, char* buf, int size) {
  if (d->SequenceFlags & ImUmlSequenceFlags_Autonumber) {
    ImFormatString(buf, (size_t)size, "%d. %.*s", number, ev.Text.Len, StrBegin(d, ev.Text));
  } else {
    ImFormatString(buf, (size_t)size, "%.*s", ev.Text.Len, StrBegin(d, ev.Text));
  }
}

ImVec2 LayoutSequenceDiagram(ImUmlDiagram* d, const ImGuiStorage& ids) {
  const ImVec2 pad = d->Style.NodePadding;
  ImUmlSeqInput in;
  in.ParticipantSpacing = d->NodeSpacing;
  in.MessageSpacing = d->Style.MessageSpacing;
  in.ActivationWidth = d->Style.ActivationWidth;
  in.Mirror = (d->SequenceFlags & ImUmlSequenceFlags_MirrorParticipants) != 0;
  d->SeqParticipants.resize(0);
  ImGuiStorage column;
  for (int i = 0; i < d->Nodes.Size; ++i) {
    ImUmlNodeRec& n = d->Nodes[i];
    if (n.Type != ImUmlNodeType_Participant) {
      continue;
    }
    ImUmlSeqParticipantIn p;
    p.BoxSize = ParticipantBox(d, n);
    p.Created = (n.Side & ImUmlParticipantFlags_Created) != 0;
    n.Size = p.BoxSize;
    column.SetInt(n.Id, in.Participants.Size + 1);
    in.Participants.push_back(p);
    d->SeqParticipants.push_back(i);
  }
  (void)ids;
  int number = 0;
  char buf[1024];
  for (int i = 0; i < d->Events.Size; ++i) {
    const ImUmlSeqEventRec& ev = d->Events[i];
    ImUmlSeqEventIn e;
    memset((void*)&e, 0, sizeof(e));
    e.Type = ev.Type;
    e.Kind = ev.Kind;
    e.Flags = ev.Flags;
    e.Side = ev.Side;
    e.A = ev.A != 0 ? column.GetInt(ev.A, 0) - 1 : -1;
    e.B = ev.B != 0 ? column.GetInt(ev.B, 0) - 1 : -1;
    switch (ev.Type) {
      case ImUmlSeqEventType_Message:
        if (e.A < 0 || e.B < 0) {
          d->SkippedItems++;
          e.A = e.B = -1;
          break;
        }
        MessageText(d, ev, ++number, buf, IM_ARRAYSIZE(buf));
        e.TextSize = CeilPx(TextSize(ev.TextStyle, 1.0f, buf, buf + strlen(buf)));
        if (e.TextSize.y == 0.0f) {
          e.TextSize.y = 0.0f;
        }
        break;
      case ImUmlSeqEventType_NoteOver:
      case ImUmlSeqEventType_NoteSide:
        if (e.A < 0) {
          d->SkippedItems++;
          break;
        }
        e.TextSize =
            CeilPx(TextSize(ev.TextStyle, 1.0f, StrBegin(d, ev.Text), StrEnd(d, ev.Text)) + pad * 2.0f +
                   ImVec2(d->Style.NoteFold, 0.0f));
        break;
      case ImUmlSeqEventType_FragmentBegin: {
        if (ev.Flags == 0) {
          e.A = e.B = -1;
        }
        const char* kw = FragmentKeyword(ev.Kind);
        const ImVec2 tab = TextSize(ev.TextStyle, 1.0f, kw, kw + strlen(kw)) + pad * 2.0f;
        ImFormatString(buf, IM_ARRAYSIZE(buf), "[%.*s]", ev.Text.Len, StrBegin(d, ev.Text));
        const ImVec2 label = ev.Text.Empty() ? ImVec2(0, 0) : TextSize(ev.TextStyle, 1.0f, buf, buf + strlen(buf));
        e.TextSize = CeilPx(ImVec2(tab.x + label.x + 10.0f, ImMax(tab.y, label.y + pad.y * 2.0f)));
        break;
      }
      case ImUmlSeqEventType_FragmentElse: {
        ImFormatString(buf, IM_ARRAYSIZE(buf), "[%.*s]", ev.Text.Len, StrBegin(d, ev.Text));
        e.TextSize = ev.Text.Empty() ? ImVec2(0, 0) : CeilPx(TextSize(ev.TextStyle, 1.0f, buf, buf + strlen(buf)));
        break;
      }
      default:
        break;
    }
    in.Events.push_back(e);
  }
  ImUml::LayoutSequence(in, &d->Seq);
  // Offset by the margin.
  const ImVec2 m(kMargin, kMargin);
  for (int i = 0; i < d->Seq.Participants.Size; ++i) {
    ImUmlSeqParticipantOut& p = d->Seq.Participants[i];
    p.X += m.x;
    p.TopY += m.y;
    p.LifelineStart += m.y;
    p.LifelineEnd += m.y;
    if (p.BottomY >= 0.0f) {
      p.BottomY += m.y;
    }
  }
  for (int i = 0; i < d->Seq.Events.Size; ++i) {
    ImUmlSeqEventOut& e = d->Seq.Events[i];
    e.Y += m.y;
    e.Y2 += m.y;
    e.X1 += m.x;
    e.X2 += m.x;
    e.Rect.Translate(m);
    e.TextPos += m;
  }
  for (int i = 0; i < d->Seq.Activations.Size; ++i) {
    d->Seq.Activations[i].Rect.Translate(m);
  }
  return CeilPx(d->Seq.Size + m * 2.0f);
}

// ── Drawing ──────────────────────────────────────────────────────────────────

struct Painter {
  ImDrawList* DL;
  const ImUmlDiagram* D;
  ImVec2 Origin;
  float Zoom;
  float LW;

  ImVec2 P(const ImVec2& p) const {
    return ImVec2(Origin.x + p.x * Zoom, Origin.y + p.y * Zoom);
  }
  float S(float v) const {
    return v * Zoom;
  }
  ImU32 Col(ImUmlCol idx) const {
    return D->Colors[idx];
  }
};

void DrawClass(const Painter& pt, const ImUmlNodeRec& n, ImU32 fill, ImU32 border) {
  const ImUmlDiagram* d = pt.D;
  const ClassGeometry g = MeasureClass(d, n);
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
  pt.DL->AddRectFilled(a, b, fill);
  pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
  const float cx = (a.x + b.x) * 0.5f;
  float y = a.y + pt.S(pad.y);
  char buf[256];
  const char* st = GuillemetText(d, n, buf, IM_ARRAYSIZE(buf));
  if (st[0] != 0) {
    TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, st, st + strlen(st), n.Text.Col);
    y += pt.S(LineHeight(n.Text, 1.0f));
  }
  ImFont* name_font = n.Kind == ImUmlClassKind_Abstract ? ItalicFont(d) : nullptr;
  TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, StrBegin(d, n.Name), StrEnd(d, n.Name), n.Text.Col, name_font);
  if (!n.Generic.Empty()) {
    const ImVec2 gs = TextSize(n.Text, pt.Zoom, StrBegin(d, n.Generic), StrEnd(d, n.Generic));
    const ImVec2 ga(b.x - gs.x * 0.5f - pt.S(6.0f), a.y - gs.y * 0.5f - pt.S(3.0f));
    const ImVec2 gb(b.x + gs.x * 0.5f + pt.S(6.0f), a.y + gs.y * 0.5f + pt.S(3.0f));
    pt.DL->AddRectFilled(ga, gb, fill);
    ImUml::RenderDashedRect(pt.DL, ga, gb, border, pt.LW, pt.S(d->Style.DashLength * 0.6f),
                            pt.S(d->Style.GapLength * 0.6f));
    TextAt(pt.DL, n.Text, pt.Zoom, ga + ImVec2(pt.S(6.0f), pt.S(3.0f)), StrBegin(d, n.Generic), StrEnd(d, n.Generic),
           n.Text.Col);
  }
  if (!g.Compartments) {
    return;
  }
  const float y1 = a.y + pt.S(g.Header), y2 = y1 + pt.S(g.Attributes);
  pt.DL->AddLine(ImVec2(a.x, y1), ImVec2(b.x, y1), border, pt.LW);
  pt.DL->AddLine(ImVec2(a.x, y2), ImVec2(b.x, y2), border, pt.LW);
  float ys[2] = {y1 + pt.S(pad.y), y2 + pt.S(pad.y)};
  for (int i = 0; i < n.LineCount; ++i) {
    const ImUmlLine& line = d->Lines[n.FirstLine + i];
    ImFont* f = line.Abstract ? ItalicFont(d) : nullptr;
    const char* tb = StrBegin(d, line.Text);
    const char* te = StrEnd(d, line.Text);
    const ImVec2 p(a.x + pt.S(pad.x), ys[line.Section]);
    TextAt(pt.DL, n.Text, pt.Zoom, p, tb, te, n.Text.Col, f);
    const ImVec2 s = TextSize(n.Text, pt.Zoom, tb, te, f);
    if (line.Abstract && f == nullptr) {
      const char* suffix = " {abstract}";
      TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(p.x + s.x, p.y), suffix, suffix + strlen(suffix), n.Text.Col);
    }
    if (line.Static) {
      const float uy = ImFloor(p.y + s.y) - 0.5f;
      pt.DL->AddLine(ImVec2(p.x, uy), ImVec2(p.x + s.x, uy), n.Text.Col, ImMax(1.0f, pt.Zoom));
    }
    ys[line.Section] += pt.S(ImMax(s.y / pt.Zoom, LineHeight(n.Text, 1.0f)));
  }
}

void DrawBar(const Painter& pt, const ImUmlNodeRec& n, ImU32 col) {
  pt.DL->AddRectFilled(pt.P(n.Pos - n.Size * 0.5f), pt.P(n.Pos + n.Size * 0.5f), col, pt.S(2.0f));
}

void DrawPseudo(const Painter& pt, const ImUmlNodeRec& n, bool initial, bool final_state, bool flow_final,
                ImU32 line, ImU32 fill) {
  const ImVec2 c = pt.P(n.Pos);
  const float r = pt.S(ImMin(n.Size.x, n.Size.y) * 0.5f);
  const int seg = ImUml::CircleSegments(r);
  if (initial) {
    pt.DL->AddCircleFilled(c, r, line, seg);
  } else if (final_state) {
    pt.DL->AddCircleFilled(c, r, pt.Col(ImUmlCol_Bg), seg);
    pt.DL->AddCircle(c, r - pt.LW * 0.5f, line, seg, pt.LW);
    pt.DL->AddCircleFilled(c, r * 0.55f, line, seg);
  } else if (flow_final) {
    pt.DL->AddCircleFilled(c, r, pt.Col(ImUmlCol_Bg), seg);
    pt.DL->AddCircle(c, r - pt.LW * 0.5f, line, seg, pt.LW);
    const float k = r * 0.7071f;
    pt.DL->AddLine(c + ImVec2(-k, -k), c + ImVec2(k, k), line, pt.LW);
    pt.DL->AddLine(c + ImVec2(-k, k), c + ImVec2(k, -k), line, pt.LW);
  } else {
    pt.DL->AddCircleFilled(c, r, fill, seg);
    pt.DL->AddCircle(c, r - pt.LW * 0.5f, line, seg, pt.LW);
  }
}

void DrawNode(const Painter& pt, const ImUmlNodeRec& n) {
  const ImUmlDiagram* d = pt.D;
  const ImU32 fill = n.FillCol != 0 ? n.FillCol : pt.Col(ImUmlCol_NodeFill);
  const ImU32 border = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NodeBorder);
  const ImU32 line = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_Line);
  const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
  const ImVec2 pad = d->Style.NodePadding;
  const char* nb = StrBegin(d, n.Name);
  const char* ne = StrEnd(d, n.Name);
  const ImVec2 ts = TextSize(n.Text, pt.Zoom, nb, ne);
  const float cx = (a.x + b.x) * 0.5f;
  switch (n.Type) {
    case ImUmlNodeType_Class:
      DrawClass(pt, n, fill, border);
      break;
    case ImUmlNodeType_State:
      switch (n.Kind) {
        case ImUmlStateKind_Initial:
          DrawPseudo(pt, n, true, false, false, line, fill);
          break;
        case ImUmlStateKind_Final:
          DrawPseudo(pt, n, false, true, false, line, fill);
          break;
        case ImUmlStateKind_Choice:
          ImUml::RenderDiamond(pt.DL, pt.P(n.Pos), n.Size * 0.5f * pt.Zoom, fill, border, pt.LW);
          break;
        case ImUmlStateKind_Fork:
        case ImUmlStateKind_Join:
          DrawBar(pt, n, line);
          break;
        case ImUmlStateKind_History:
        case ImUmlStateKind_DeepHistory: {
          DrawPseudo(pt, n, false, false, false, border, fill);
          const char* h = n.Kind == ImUmlStateKind_History ? "H" : "H*";
          const ImVec2 hs = TextSize(n.Text, pt.Zoom, h, h + strlen(h));
          TextAt(pt.DL, n.Text, pt.Zoom, pt.P(n.Pos) - hs * 0.5f, h, h + strlen(h), n.Text.Col);
          break;
        }
        default: {
          const float rounding = pt.S(d->Style.NodeRounding);
          if (n.Composite) {
            pt.DL->AddRectFilled(a, b, pt.Col(ImUmlCol_Bg), rounding);
            pt.DL->AddRectFilled(a, ImVec2(b.x, a.y + pt.S(n.HeaderHeight)), fill, rounding,
                                 ImDrawFlags_RoundCornersTop);
            pt.DL->AddRect(a, b, border, rounding, ImDrawFlags_None, pt.LW);
            pt.DL->AddLine(ImVec2(a.x, a.y + pt.S(n.HeaderHeight)), ImVec2(b.x, a.y + pt.S(n.HeaderHeight)), border,
                           pt.LW);
            TextCentered(pt.DL, n.Text, pt.Zoom, cx, a.y + pt.S(pad.y), nb, ne, n.Text.Col);
            break;
          }
          pt.DL->AddRectFilled(a, b, fill, rounding);
          pt.DL->AddRect(a, b, border, rounding, ImDrawFlags_None, pt.LW);
          if (n.LineCount == 0) {
            TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
            break;
          }
          float y = a.y + pt.S(pad.y);
          TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, nb, ne, n.Text.Col);
          y += ts.y + pt.S(pad.y);
          pt.DL->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), border, pt.LW);
          y += pt.S(pad.y);
          for (int i = 0; i < n.LineCount; ++i) {
            const ImUmlLine& l = d->Lines[n.FirstLine + i];
            TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(a.x + pt.S(pad.x + d->Style.NodeRounding * 0.5f), y),
                   StrBegin(d, l.Text), StrEnd(d, l.Text), n.Text.Col);
            y += pt.S(LineHeight(n.Text, 1.0f));
          }
          break;
        }
      }
      break;
    case ImUmlNodeType_Action: {
      const float h = b.y - a.y;
      if (n.Kind == ImUmlActionKind_Send) {
        const ImVec2 pts[5] = {a, ImVec2(b.x - h * 0.5f, a.y), ImVec2(b.x, (a.y + b.y) * 0.5f),
                               ImVec2(b.x - h * 0.5f, b.y), ImVec2(a.x, b.y)};
        pt.DL->AddConvexPolyFilled(pts, 5, fill);
        pt.DL->AddPolyline(pts, 5, border, ImDrawFlags_Closed, pt.LW);
        TextCentered(pt.DL, n.Text, pt.Zoom, cx - h * 0.25f, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
      } else if (n.Kind == ImUmlActionKind_Receive) {
        const ImVec2 notch(a.x + h * 0.5f, (a.y + b.y) * 0.5f);
        pt.DL->AddRectFilled(ImVec2(notch.x, a.y), b, fill);
        pt.DL->AddTriangleFilled(a, ImVec2(notch.x, a.y), notch, fill);
        pt.DL->AddTriangleFilled(ImVec2(a.x, b.y), notch, ImVec2(notch.x, b.y), fill);
        const ImVec2 pts[5] = {a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y), notch};
        pt.DL->AddPolyline(pts, 5, border, ImDrawFlags_Closed, pt.LW);
        TextCentered(pt.DL, n.Text, pt.Zoom, cx + h * 0.25f, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
      } else {
        const float rounding = pt.S(d->Style.NodeRounding);
        pt.DL->AddRectFilled(a, b, fill, rounding);
        pt.DL->AddRect(a, b, border, rounding, ImDrawFlags_None, pt.LW);
        TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
      }
      break;
    }
    case ImUmlNodeType_Control:
      switch (n.Kind) {
        case ImUmlControlKind_Initial:
          DrawPseudo(pt, n, true, false, false, line, fill);
          break;
        case ImUmlControlKind_Final:
          DrawPseudo(pt, n, false, true, false, line, fill);
          break;
        case ImUmlControlKind_FlowFinal:
          DrawPseudo(pt, n, false, false, true, line, fill);
          break;
        case ImUmlControlKind_Fork:
        case ImUmlControlKind_Join:
          DrawBar(pt, n, line);
          break;
        default:
          ImUml::RenderDiamond(pt.DL, pt.P(n.Pos), n.Size * 0.5f * pt.Zoom, fill, border, pt.LW);
          break;
      }
      if (nb != ne) {
        // The label sits beside the node, away from the flow.
        const ImVec2 p = Across(d) ? ImVec2(cx - ts.x * 0.5f, b.y + pt.S(2.0f)) : ImVec2(b.x + pt.S(4.0f), a.y - ts.y);
        TextAt(pt.DL, n.Text, pt.Zoom, p, nb, ne, n.Text.Col);
      }
      break;
    case ImUmlNodeType_Actor: {
      const float fig_w = pt.S(24.0f), fig_h = pt.S(40.0f);
      ImUml::RenderActorFigure(pt.DL, ImVec2(cx - fig_w * 0.5f, a.y), ImVec2(cx + fig_w * 0.5f, a.y + fig_h), border,
                               pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, a.y + fig_h + pt.S(4.0f), nb, ne, n.Text.Col);
      break;
    }
    case ImUmlNodeType_UseCase: {
      const ImVec2 r = (b - a) * 0.5f;
      const int seg = ImUml::CircleSegments(ImMax(r.x, r.y));
      pt.DL->AddEllipseFilled(pt.P(n.Pos), r, fill, 0.0f, seg);
      pt.DL->AddEllipse(pt.P(n.Pos), r, border, 0.0f, seg, pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
      break;
    }
    case ImUmlNodeType_Note: {
      const ImU32 nf = n.FillCol != 0 ? n.FillCol : pt.Col(ImUmlCol_NoteFill);
      const ImU32 nbd = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NoteBorder);
      ImUml::RenderNoteShape(pt.DL, a, b, pt.S(d->Style.NoteFold), nf, nbd, pt.LW);
      TextAt(pt.DL, n.Text, pt.Zoom, a + pad * pt.Zoom, nb, ne, n.Text.Col);
      break;
    }
    default:
      break;
  }
}

/// Removes `len` of arc length from the start (or end) of a polyline.
void TrimPath(ImVector<ImVec2>* pts, float len, bool at_end) {
  if (len <= 0.0f || pts->Size < 2) {
    return;
  }
  if (at_end) {
    for (int i = 0, j = pts->Size - 1; i < j; ++i, --j) {
      const ImVec2 t = (*pts)[i];
      (*pts)[i] = (*pts)[j];
      (*pts)[j] = t;
    }
  }
  while (pts->Size >= 2) {
    const ImVec2 a = (*pts)[0], b = (*pts)[1];
    const float seg = ImSqrt(ImLengthSqr(b - a));
    if (seg > len) {
      (*pts)[0] = a + (b - a) * (len / seg);
      break;
    }
    len -= seg;
    pts->erase(pts->Data);
  }
  if (at_end) {
    for (int i = 0, j = pts->Size - 1; i < j; ++i, --j) {
      const ImVec2 t = (*pts)[i];
      (*pts)[i] = (*pts)[j];
      (*pts)[j] = t;
    }
  }
}

/// Unit direction at the end of a polyline, measured over the last `reach`.
ImVec2 EndDirection(const ImVector<ImVec2>& pts, bool at_end, float reach) {
  const int n = pts.Size;
  const ImVec2 tip = at_end ? pts[n - 1] : pts[0];
  ImVec2 from = tip;
  float travelled = 0.0f;
  for (int k = 1; k < n; ++k) {
    const ImVec2 p = at_end ? pts[n - 1 - k] : pts[k];
    travelled += ImSqrt(ImLengthSqr(p - from));
    from = p;
    if (travelled >= reach) {
      break;
    }
  }
  ImVec2 d = tip - from;
  const float l = ImSqrt(ImLengthSqr(d));
  return l > 0.0f ? d * (1.0f / l) : ImVec2(0.0f, 1.0f);
}

void DrawEdgeLabel(const Painter& pt, const ImUmlTextStyle& ts, const ImVec2& centre, const char* b, const char* e) {
  const ImVec2 s = TextSize(ts, pt.Zoom, b, e);
  const ImVec2 c = pt.P(centre);
  const ImVec2 a(c.x - s.x * 0.5f - pt.S(2.0f), c.y - s.y * 0.5f);
  pt.DL->AddRectFilled(a, ImVec2(c.x + s.x * 0.5f + pt.S(2.0f), c.y + s.y * 0.5f), pt.Col(ImUmlCol_EdgeLabelBg));
  TextCentered(pt.DL, ts, pt.Zoom, c.x, a.y, b, e, ts.Col);
}

void DrawEdge(const Painter& pt, const ImUmlEdgeRec& e, const ImUmlGraphEdgeOut& eo, ImVector<ImVec2>* scratch) {
  const ImUmlDiagram* d = pt.D;
  if (eo.PointCount < 2) {
    return;
  }
  const EdgeLook look = LookOf(d, e);
  const ImU32 col = e.Col != 0 ? e.Col : (e.Type == ImUmlEdgeType_NoteLink ? pt.Col(ImUmlCol_NoteBorder)
                                                                           : pt.Col(ImUmlCol_Line));
  ImVector<ImVec2> screen;
  for (int i = 0; i < eo.PointCount; ++i) {
    screen.push_back(pt.P(d->Points[eo.PointStart + i]));
  }
  ImUml::FlattenPath(screen.Data, screen.Size, eo.Bezier, scratch);
  ImVector<ImVec2>& path = *scratch;
  const float size = pt.S(d->Style.ArrowSize);
  const ImVec2 to_dir = EndDirection(path, true, size);
  const ImVec2 from_dir = EndDirection(path, false, size);
  const ImVec2 to_tip = path.back(), from_tip = path[0];
  TrimPath(&path, ImUml::MarkerLength(look.AtTo, size), true);
  TrimPath(&path, ImUml::MarkerLength(look.AtFrom, size), false);
  ImUml::RenderPath(pt.DL, path.Data, path.Size, col, pt.LW, look.Dashed, pt.S(d->Style.DashLength),
                    pt.S(d->Style.GapLength));
  ImUml::RenderMarker(pt.DL, look.AtTo, to_tip, to_dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
  ImUml::RenderMarker(pt.DL, look.AtFrom, from_tip, from_dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
  if (!e.Label.Empty()) {
    DrawEdgeLabel(pt, e.Text, eo.LabelPos, StrBegin(d, e.Label), StrEnd(d, e.Label));
  }
  // Multiplicities, beside each end.
  for (int end = 0; end < 2; ++end) {
    const ImUmlStr& s = end == 0 ? e.FromLabel : e.ToLabel;
    if (s.Empty()) {
      continue;
    }
    const ImVec2 tip = end == 0 ? from_tip : to_tip;
    const ImVec2 dir = end == 0 ? from_dir : to_dir; // pointing out of the line at that end
    const ImVec2 normal(-dir.y, dir.x);
    const ImVec2 ts = TextSize(e.Text, pt.Zoom, StrBegin(d, s), StrEnd(d, s));
    const float back = ImUml::MarkerLength(end == 0 ? look.AtFrom : look.AtTo, size) + pt.S(6.0f) + ts.y * 0.5f;
    const ImVec2 c = tip - dir * back + normal * (pt.S(6.0f) + ts.x * 0.5f);
    TextAt(pt.DL, e.Text, pt.Zoom, c - ts * 0.5f, StrBegin(d, s), StrEnd(d, s), e.Text.Col);
  }
}

void DrawGraphDiagram(const Painter& pt) {
  const ImUmlDiagram* d = pt.D;
  if (d->Boundary.GetWidth() > 0.0f) {
    const ImVec2 a = pt.P(d->Boundary.Min), b = pt.P(d->Boundary.Max);
    pt.DL->AddRect(a, b, pt.Col(ImUmlCol_Boundary), 0.0f, ImDrawFlags_None, pt.LW);
    const ImUmlTextStyle& ts = d->TitleStyle;
    TextCentered(pt.DL, ts, pt.Zoom, (a.x + b.x) * 0.5f, a.y + pt.S(5.0f), StrBegin(d, d->SystemLabel),
                 StrEnd(d, d->SystemLabel), ts.Col);
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    if (d->Nodes[i].Composite) {
      DrawNode(pt, d->Nodes[i]);
    }
  }
  ImVector<ImVec2> scratch;
  for (int e = 0; e < d->Edges.Size; ++e) {
    if (d->Edges[e].From >= 0) {
      DrawEdge(pt, d->Edges[e], d->CacheEdges[e], &scratch);
    }
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    if (!d->Nodes[i].Composite) {
      DrawNode(pt, d->Nodes[i]);
    }
  }
}

void DrawParticipantBox(const Painter& pt, const ImUmlNodeRec& n, float x, float top) {
  const ImUmlDiagram* d = pt.D;
  const ImU32 fill = n.FillCol != 0 ? n.FillCol : pt.Col(ImUmlCol_NodeFill);
  const ImU32 border = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NodeBorder);
  const ImVec2 a = pt.P(ImVec2(x - n.Size.x * 0.5f, top)), b = pt.P(ImVec2(x + n.Size.x * 0.5f, top + n.Size.y));
  const char* nb = StrBegin(d, n.Name);
  const char* ne = StrEnd(d, n.Name);
  const ImVec2 ts = TextSize(n.Text, pt.Zoom, nb, ne);
  const float cx = (a.x + b.x) * 0.5f;
  switch (n.Kind) {
    case ImUmlParticipantKind_Actor: {
      const float w = pt.S(22.0f), h = pt.S(36.0f);
      ImUml::RenderActorFigure(pt.DL, ImVec2(cx - w * 0.5f, a.y), ImVec2(cx + w * 0.5f, a.y + h), border, pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, b.y - ts.y, nb, ne, n.Text.Col);
      break;
    }
    case ImUmlParticipantKind_Boundary:
    case ImUmlParticipantKind_Control:
    case ImUmlParticipantKind_Entity: {
      const float r = pt.S(12.0f);
      const ImVec2 c(cx + (n.Kind == ImUmlParticipantKind_Boundary ? pt.S(4.0f) : 0.0f), a.y + pt.S(14.0f));
      const int seg = ImUml::CircleSegments(r);
      pt.DL->AddCircleFilled(c, r, fill, seg);
      pt.DL->AddCircle(c, r, border, seg, pt.LW);
      if (n.Kind == ImUmlParticipantKind_Boundary) {
        pt.DL->AddLine(ImVec2(c.x - r - pt.S(8.0f), c.y), ImVec2(c.x - r, c.y), border, pt.LW);
        pt.DL->AddLine(ImVec2(c.x - r - pt.S(8.0f), c.y - r), ImVec2(c.x - r - pt.S(8.0f), c.y + r), border, pt.LW);
      } else if (n.Kind == ImUmlParticipantKind_Entity) {
        pt.DL->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y + r), border, pt.LW);
      } else {
        ImUml::RenderMarker(pt.DL, ImUmlMarker_Open, ImVec2(c.x + pt.S(1.0f), c.y - r), ImVec2(-1.0f, 0.0f),
                            pt.S(6.0f), border, fill, pt.LW);
      }
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, b.y - ts.y, nb, ne, n.Text.Col);
      break;
    }
    case ImUmlParticipantKind_Database:
      ImUml::RenderCylinder(pt.DL, a, b, fill, border, pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f + pt.S(4.0f), nb, ne, n.Text.Col);
      break;
    default:
      pt.DL->AddRectFilled(a, b, fill, pt.S(2.0f));
      pt.DL->AddRect(a, b, border, pt.S(2.0f), ImDrawFlags_None, pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
      break;
  }
}

void DrawSequenceDiagram(const Painter& pt) {
  const ImUmlDiagram* d = pt.D;
  const ImUmlSeqOutput& s = d->Seq;
  const ImVec2 pad = d->Style.NodePadding;
  const float dash = pt.S(d->Style.DashLength), gap = pt.S(d->Style.GapLength);
  char buf[1024];
  // Group fragments: filled backgrounds first.
  for (int i = 0; i < d->Events.Size; ++i) {
    const ImUmlSeqEventRec& ev = d->Events[i];
    if (ev.Type == ImUmlSeqEventType_FragmentBegin && ev.Kind == ImUmlFragmentKind_Group) {
      const ImRect& r = s.Events[i].Rect;
      const ImU32 fill = ev.Col != 0 ? ev.Col : pt.Col(ImUmlCol_FragmentTab);
      pt.DL->AddRectFilled(pt.P(r.Min), pt.P(r.Max), fill);
    }
  }
  // Lifelines.
  for (int k = 0; k < d->SeqParticipants.Size; ++k) {
    const ImUmlSeqParticipantOut& p = s.Participants[k];
    const ImVec2 pts[2] = {pt.P(ImVec2(p.X, p.LifelineStart)), pt.P(ImVec2(p.X, p.LifelineEnd))};
    ImUml::RenderPath(pt.DL, pts, 2, pt.Col(ImUmlCol_Lifeline), pt.LW, true, dash, gap);
  }
  // Activations.
  for (int i = 0; i < s.Activations.Size; ++i) {
    const ImRect& r = s.Activations[i].Rect;
    pt.DL->AddRectFilled(pt.P(r.Min), pt.P(r.Max), pt.Col(ImUmlCol_Activation));
    pt.DL->AddRect(pt.P(r.Min), pt.P(r.Max), pt.Col(ImUmlCol_NodeBorder), 0.0f, ImDrawFlags_None, pt.LW);
  }
  // Events.
  int number = 0;
  const float size = pt.S(d->Style.ArrowSize);
  for (int i = 0; i < d->Events.Size; ++i) {
    const ImUmlSeqEventRec& ev = d->Events[i];
    const ImUmlSeqEventOut& eo = s.Events[i];
    const ImUmlTextStyle& ts = ev.TextStyle;
    switch (ev.Type) {
      case ImUmlSeqEventType_Message: {
        if (eo.Number == 0) {
          break;
        }
        ++number;
        const ImU32 col = ev.Col != 0 ? ev.Col : pt.Col(ImUmlCol_Line);
        const bool dashed = ev.Kind == ImUmlMessageKind_Reply || ev.Kind == ImUmlMessageKind_Create;
        const ImUmlMarker marker =
            ev.Kind == ImUmlMessageKind_Sync || ev.Kind == ImUmlMessageKind_Destroy ? ImUmlMarker_Filled
                                                                                    : ImUmlMarker_Open;
        ImVector<ImVec2> path;
        if (ev.A == ev.B) {
          const float reach = 30.0f;
          path.push_back(pt.P(ImVec2(eo.X1, eo.Y)));
          path.push_back(pt.P(ImVec2(eo.X1 + reach, eo.Y)));
          path.push_back(pt.P(ImVec2(eo.X1 + reach, eo.Y2)));
          path.push_back(pt.P(ImVec2(eo.X2, eo.Y2)));
        } else {
          path.push_back(pt.P(ImVec2(eo.X1, eo.Y)));
          path.push_back(pt.P(ImVec2(eo.X2, eo.Y)));
        }
        const ImVec2 tip = path.back();
        const ImVec2 dir = EndDirection(path, true, size);
        TrimPath(&path, ImUml::MarkerLength(marker, size), true);
        ImUml::RenderPath(pt.DL, path.Data, path.Size, col, pt.LW, dashed, dash, gap);
        ImUml::RenderMarker(pt.DL, marker, tip, dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
        if (ev.Kind == ImUmlMessageKind_Destroy) {
          // The cross sits on the target's lifeline at this row.
          for (int k = 0; k < d->SeqParticipants.Size; ++k) {
            if (d->Nodes[d->SeqParticipants[k]].Id == ev.B) {
              ImUml::RenderMarker(pt.DL, ImUmlMarker_Cross, pt.P(ImVec2(s.Participants[k].X, eo.Y)), ImVec2(1, 0),
                                  pt.S(d->Style.ArrowSize * 1.2f), col, col, pt.LW);
            }
          }
        }
        MessageText(d, ev, number, buf, IM_ARRAYSIZE(buf));
        TextAt(pt.DL, ts, pt.Zoom, pt.P(eo.TextPos), buf, buf + strlen(buf), ts.Col);
        break;
      }
      case ImUmlSeqEventType_NoteOver:
      case ImUmlSeqEventType_NoteSide: {
        if (eo.Rect.GetWidth() <= 0.0f) {
          break;
        }
        const ImU32 fill = ev.Col != 0 ? ev.Col : pt.Col(ImUmlCol_NoteFill);
        ImUml::RenderNoteShape(pt.DL, pt.P(eo.Rect.Min), pt.P(eo.Rect.Max), pt.S(d->Style.NoteFold), fill,
                               pt.Col(ImUmlCol_NoteBorder), pt.LW);
        TextAt(pt.DL, ts, pt.Zoom, pt.P(eo.Rect.Min + pad), StrBegin(d, ev.Text), StrEnd(d, ev.Text), ts.Col);
        break;
      }
      case ImUmlSeqEventType_FragmentBegin: {
        const char* kw = FragmentKeyword(ev.Kind);
        const ImVec2 a = pt.P(eo.Rect.Min), b = pt.P(eo.Rect.Max);
        const ImU32 border = pt.Col(ImUmlCol_FragmentBorder);
        float label_x = a.x + pt.S(pad.x);
        const float tab_h = TextSize(ts, pt.Zoom, "A", nullptr).y + pt.S(pad.y * 2.0f);
        if (kw[0] != 0) {
          const ImVec2 kws = TextSize(ts, pt.Zoom, kw, kw + strlen(kw));
          const ImVec2 tab(kws.x + pt.S(pad.x * 2.0f), tab_h);
          ImUml::RenderFragmentFrame(pt.DL, a, b, tab, border, pt.Col(ImUmlCol_FragmentTab), pt.LW);
          TextAt(pt.DL, ts, pt.Zoom, ImVec2(a.x + pt.S(pad.x), a.y + pt.S(pad.y)), kw, kw + strlen(kw), ts.Col);
          label_x = a.x + tab.x + pt.S(6.0f);
        } else {
          pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
        }
        if (!ev.Text.Empty()) {
          ImFormatString(buf, IM_ARRAYSIZE(buf), kw[0] != 0 ? "[%.*s]" : "%.*s", ev.Text.Len, StrBegin(d, ev.Text));
          TextAt(pt.DL, ts, pt.Zoom, ImVec2(label_x, a.y + pt.S(pad.y)), buf, buf + strlen(buf), ts.Col);
        }
        break;
      }
      case ImUmlSeqEventType_FragmentElse: {
        const ImVec2 pts[2] = {pt.P(ImVec2(eo.Rect.Min.x, eo.Rect.Min.y)), pt.P(ImVec2(eo.Rect.Max.x, eo.Rect.Min.y))};
        ImUml::RenderPath(pt.DL, pts, 2, pt.Col(ImUmlCol_FragmentBorder), pt.LW, true, dash, gap);
        if (!ev.Text.Empty()) {
          ImFormatString(buf, IM_ARRAYSIZE(buf), "[%.*s]", ev.Text.Len, StrBegin(d, ev.Text));
          TextAt(pt.DL, ts, pt.Zoom, pt.P(ImVec2(eo.TextPos.x + pad.x, eo.TextPos.y)), buf, buf + strlen(buf),
                 ts.Col);
        }
        break;
      }
      default:
        break;
    }
  }
  // Participant boxes, on top of their lifelines.
  for (int k = 0; k < d->SeqParticipants.Size; ++k) {
    const ImUmlNodeRec& n = d->Nodes[d->SeqParticipants[k]];
    const ImUmlSeqParticipantOut& p = s.Participants[k];
    DrawParticipantBox(pt, n, p.X, p.TopY);
    if (p.BottomY >= 0.0f) {
      DrawParticipantBox(pt, n, p.X, p.BottomY);
    }
  }
}

// ── End ──────────────────────────────────────────────────────────────────────

void EndDiagram(int kind) {
  IM_ASSERT(GImUml != nullptr);
  ImUmlContext& g = *GImUml;
  ImUmlDiagram* d = g.Current;
  IM_ASSERT(d != nullptr && d->Kind == kind && "Mismatched ImUml Begin*/End*");
  if (d == nullptr) {
    return;
  }
  (void)kind;
  d->OpenClass = -1;
  d->OpenStates.resize(0);
  // Close fragments left open, so the layout sees balanced scopes.
  for (; d->OpenFragments > 0; --d->OpenFragments) {
    ImUmlSeqEventRec ev;
    memset((void*)&ev, 0, sizeof(ev));
    ev.Type = ImUmlSeqEventType_FragmentEnd;
    ev.Text = ImUmlStr();
    d->Events.push_back(ev);
  }

  // Layout, unless the same items were laid out before.
  const ImGuiID hash = HashItems(d);
  ImGuiStorage ids;
  IndexNodes(d, &ids);
  int edge_count = 0;
  if (kind != ImUmlDiagramKind_Sequence) {
    for (int e = 0; e < d->Edges.Size; ++e) {
      ImUmlEdgeRec& er = d->Edges[e];
      er.From = ids.GetInt(er.FromId, 0) - 1;
      er.To = ids.GetInt(er.ToId, 0) - 1;
      if (er.From < 0 || er.To < 0) {
        er.From = er.To = -1;
        d->SkippedItems++;
      }
    }
    edge_count = d->Edges.Size;
    AddNoteLinks(d, ids);
  }
  if (!(d->LayoutValid && d->LayoutHash == hash)) {
    const int skipped_before = d->SkippedItems;
    d->ContentSize = kind == ImUmlDiagramKind_Sequence ? LayoutSequenceDiagram(d, ids) : LayoutGraphDiagram(d);
    d->CacheNodePos.resize(d->Nodes.Size);
    d->CacheNodeSize.resize(d->Nodes.Size);
    d->CacheNodeHeader.resize(d->Nodes.Size);
    for (int i = 0; i < d->Nodes.Size; ++i) {
      d->CacheNodePos[i] = d->Nodes[i].Pos;
      d->CacheNodeSize[i] = d->Nodes[i].Size;
      d->CacheNodeHeader[i] = d->Nodes[i].HeaderHeight;
    }
    d->LayoutHash = hash;
    d->LayoutValid = true;
    // Skips found while laying out (sequence references) are remembered
    // with the cache.
    d->CacheSkipped = d->SkippedItems - skipped_before;
  } else {
    for (int i = 0; i < d->Nodes.Size; ++i) {
      d->Nodes[i].Pos = d->CacheNodePos[i];
      d->Nodes[i].Size = d->CacheNodeSize[i];
      d->Nodes[i].HeaderHeight = d->CacheNodeHeader[i];
    }
    if (kind != ImUmlDiagramKind_Sequence) {
      for (int i = 0; i < d->Nodes.Size; ++i) {
        d->Nodes[i].Composite = false;
      }
      for (int i = 0; i < d->Nodes.Size; ++i) {
        if (d->Nodes[i].Parent >= 0) {
          d->Nodes[d->Nodes[i].Parent].Composite = true;
        }
      }
      for (int i = 0; i < d->Nodes.Size; ++i) {
        if (!d->Nodes[i].Composite) {
          ImVec2 keep = d->Nodes[i].Size;
          MeasureNode(d, d->Nodes[i]); // restores Shape
          d->Nodes[i].Size = keep;
        }
      }
      for (int e = 0; e < d->Edges.Size; ++e) {
        ImUmlEdgeRec& er = d->Edges[e];
        if (!er.Label.Empty()) {
          er.LabelSize =
              CeilPx(TextSize(er.Text, 1.0f, StrBegin(d, er.Label), StrEnd(d, er.Label)) + ImVec2(6.0f, 2.0f));
        }
      }
    }
    d->SkippedItems += d->CacheSkipped;
  }

  // Size and zoom.
  const ImUmlTextStyle& tts = d->TitleStyle;
  const float title_h =
      d->Title.Empty() ? 0.0f : ImCeil(TextSize(tts, 1.0f, StrBegin(d, d->Title), StrEnd(d, d->Title)).y + 6.0f);
  const float title_w = d->Title.Empty() ? 0.0f : TextSize(tts, 1.0f, StrBegin(d, d->Title), StrEnd(d, d->Title)).x;
  const ImVec2 content = d->ContentSize;
  const ImVec2 natural(ImCeil(ImMax(content.x, title_w + 2.0f * kMargin)), content.y + title_h);
  ImUmlDiagramInfo& info = g.LastInfo;
  info = ImUmlDiagramInfo();
  info.NaturalSize = natural;
  info.ContentSize = content;
  info.TitleHeight = title_h;
  info.NodeCount = d->Nodes.Size;
  info.EdgeCount = kind == ImUmlDiagramKind_Sequence ? d->Events.Size : edge_count;
  info.SkippedItems = d->SkippedItems;
  g.Current = nullptr;
  if (d->Flags & ImUmlDiagramFlags_MeasureOnly) {
    d->Edges.resize(edge_count); // drop the note links added for this frame
    return;
  }
  ImGui::SetCursorScreenPos(d->CursorPos);
  const ImVec2 size = ImGui::CalcItemSize(d->SizeRequest, natural.x, natural.y);
  const ImRect bb(d->CursorPos, d->CursorPos + size);
  ImGui::ItemSize(bb);
  info.Size = size;
  if (!ImGui::ItemAdd(bb, d->ID)) {
    d->Edges.resize(edge_count);
    return;
  }
  float zoom = 1.0f;
  if (!(d->Flags & ImUmlDiagramFlags_NoFit) && content.x > 0.0f && content.y > 0.0f) {
    zoom = ImMin(d->Style.MaxZoom, ImMin(size.x / content.x, (size.y - title_h) / content.y));
    zoom = ImMax(zoom, 0.01f);
  }
  info.Zoom = zoom;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->PushClipRect(bb.Min, bb.Max, true);
  if (!d->Title.Empty()) {
    TextCentered(dl, tts, 1.0f, (bb.Min.x + bb.Max.x) * 0.5f, bb.Min.y + 2.0f, StrBegin(d, d->Title),
                 StrEnd(d, d->Title), tts.Col);
  }
  Painter pt;
  pt.DL = dl;
  pt.D = d;
  pt.Zoom = zoom;
  pt.LW = ImMax(1.0f, d->Style.LineWeight * zoom);
  pt.Origin = ImVec2(ImFloor(bb.Min.x + (size.x - content.x * zoom) * 0.5f),
                     ImFloor(bb.Min.y + title_h + (size.y - title_h - content.y * zoom) * 0.5f));
  if (kind == ImUmlDiagramKind_Sequence) {
    DrawSequenceDiagram(pt);
  } else {
    DrawGraphDiagram(pt);
  }
  dl->PopClipRect();
  d->Edges.resize(edge_count);
}

} // namespace

void ImUml::EndClassDiagram() {
  EndDiagram(ImUmlDiagramKind_Class);
}

void ImUml::EndSequenceDiagram() {
  EndDiagram(ImUmlDiagramKind_Sequence);
}

void ImUml::EndStateDiagram() {
  EndDiagram(ImUmlDiagramKind_State);
}

void ImUml::EndActivityDiagram() {
  EndDiagram(ImUmlDiagramKind_Activity);
}

void ImUml::EndUseCaseDiagram() {
  EndDiagram(ImUmlDiagramKind_UseCase);
}
