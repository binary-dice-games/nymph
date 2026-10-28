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

/// Notes a text drawn, when the diagram captures them and an item is being
/// drawn (ImUmlDiagramFlags_CaptureText).
void CaptureRun(const ImVec2& min, const ImVec2& max, const char* b, const char* e) {
  ImUmlContext* g = GImUml;
  if (g == nullptr || !g->CaptureText || g->RunKind == ImUmlTextRunKind_None || b >= e) {
    return;
  }
  ImUmlTextRun run;
  run.Kind = g->RunKind;
  run.Item = g->RunItem;
  run.Min = min;
  run.Max = max;
  run.Text = nullptr; // set when the diagram ends, once the text buffer stops growing
  g->RunTextOffsets.push_back(g->RunText.Size);
  const int old = g->RunText.Size;
  g->RunText.resize(old + (int)(e - b) + 1);
  memcpy(g->RunText.Data + old, b, (size_t)(e - b));
  g->RunText[old + (int)(e - b)] = 0;
  g->Runs.push_back(run);
}

/// The texts and shapes drawn from here on belong to item `item` of `kind`.
void BeginRuns(int kind, int item) {
  if (GImUml != nullptr && (GImUml->CaptureText || GImUml->CaptureItems)) {
    GImUml->RunKind = kind;
    GImUml->RunItem = item;
  }
}

/// Notes that the item being drawn covers [min, max]
/// (ImUmlDiagramFlags_CaptureItems).
void CaptureArea(const ImVec2& min, const ImVec2& max) {
  ImUmlContext* g = GImUml;
  if (g == nullptr || !g->CaptureItems || g->RunKind == ImUmlTextRunKind_None ||
      g->RunKind == ImUmlTextRunKind_Title) {
    return;
  }
  ImUmlItemArea area;
  area.Kind = g->RunKind;
  area.Item = g->RunItem;
  area.Min = ImMin(min, max);
  area.Max = ImMax(min, max);
  g->Areas.push_back(area);
}

/// Notes the screen path the item being drawn strokes, cut into pieces no
/// longer than a few pixels, each padded by `pad`, so a diagonal line is not
/// one large rectangle.
void CapturePath(const ImVec2* pts, int count, float pad) {
  if (GImUml == nullptr || !GImUml->CaptureItems) {
    return;
  }
  const float piece = ImMax(pad * 3.0f, 8.0f);
  const ImVec2 grow(pad, pad);
  for (int i = 0; i + 1 < count; ++i) {
    const ImVec2 a = pts[i], b = pts[i + 1];
    const float length = ImSqrt(ImLengthSqr(b - a));
    const bool straight = ImFabs(a.x - b.x) < 0.5f || ImFabs(a.y - b.y) < 0.5f;
    const int pieces = straight ? 1 : ImMax(1, (int)ImCeil(length / piece));
    for (int k = 0; k < pieces; ++k) {
      const ImVec2 p = ImLerp(a, b, (float)k / (float)pieces);
      const ImVec2 q = ImLerp(a, b, (float)(k + 1) / (float)pieces);
      CaptureArea(ImMin(p, q) - grow, ImMax(p, q) + grow);
    }
  }
}

/// Stops capturing and publishes the runs and areas in `info`.
void EndRuns(ImUmlDiagramInfo* info) {
  ImUmlContext& g = *GImUml;
  g.RunKind = ImUmlTextRunKind_None;
  info->ItemAreaCount = g.CaptureItems ? g.Areas.Size : 0;
  info->ItemAreas = g.CaptureItems ? g.Areas.Data : nullptr;
  g.CaptureItems = false;
  if (!g.CaptureText) {
    info->TextRunCount = 0;
    info->TextRuns = nullptr;
    return;
  }
  g.CaptureText = false;
  for (int i = 0; i < g.Runs.Size; ++i) {
    g.Runs[i].Text = g.RunText.Data + g.RunTextOffsets[i];
  }
  info->TextRunCount = g.Runs.Size;
  info->TextRuns = g.Runs.Data;
}

/// How far a captured path reaches on each side of the line (screen pixels),
/// so a thin line is easy to click.
constexpr float kPathPad = 4.0f;

/// Draws each line of [b, e) centred on `cx`, the first line's top at `y`.
void TextCentered(ImDrawList* dl, const ImUmlTextStyle& ts, float zoom, float cx, float y, const char* b, const char* e,
                  ImU32 col, ImFont* font = nullptr) {
  ImFont* f = font != nullptr ? font : ts.Font;
  const float size = ts.Size * zoom;
  if (GImUml != nullptr && GImUml->CaptureText && b < e) {
    float width = 0.0f;
    int lines = 0;
    for (const char* p = b; p < e; ++lines) {
      const char* nl = (const char*)memchr(p, '\n', (size_t)(e - p));
      const char* line_end = nl != nullptr ? nl : e;
      width = ImMax(width, f->CalcTextSizeA(size, FLT_MAX, 0.0f, p, line_end).x);
      p = nl != nullptr ? nl + 1 : e;
    }
    CaptureRun(ImVec2(ImFloor(cx - width * 0.5f + 0.5f), ImFloor(y + 0.5f)),
               ImVec2(ImFloor(cx + width * 0.5f + 0.5f), ImFloor(y + 0.5f) + size * (float)lines), b, e);
  }
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
  if (GImUml != nullptr && GImUml->CaptureText) {
    const ImVec2 at(ImFloor(p.x + 0.5f), ImFloor(p.y + 0.5f));
    const ImVec2 s = (font != nullptr ? font : ts.Font)->CalcTextSizeA(ts.Size * zoom, FLT_MAX, 0.0f, b, e);
    CaptureRun(at, ImVec2(at.x + s.x, at.y + s.y), b, e);
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

/// Text of an ER attribute's key column ("PK, FK").
void KeysText(int keys, char* buf, int size) {
  buf[0] = 0;
  const char* names[3] = {"PK", "FK", "UK"};
  for (int i = 0; i < 3; ++i) {
    if (keys & (1 << i)) {
      ImStrncpy(buf + strlen(buf), buf[0] != 0 ? ", " : "", (size_t)size - strlen(buf));
      ImStrncpy(buf + strlen(buf), names[i], (size_t)size - strlen(buf));
    }
  }
}

/// Sizes of an entity table's parts at zoom 1: the header, one row per
/// attribute, and the columns type | name | keys | comment (a column with
/// no text in any row has no width).
struct EntityGeometry {
  float Header;
  float Row;
  float Col[4];
  float Width;
};

EntityGeometry MeasureEntity(const ImUmlDiagram* d, const ImUmlNodeRec& n) {
  const ImVec2 pad = d->Style.NodePadding;
  EntityGeometry g;
  const ImVec2 name = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name));
  g.Header = name.y + pad.y * 2.0f;
  g.Row = LineHeight(n.Text, 1.0f) + pad.y;
  float text[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  char keys[32];
  for (int i = 0; i < n.LineCount; ++i) {
    const ImUmlLine& line = d->Lines[n.FirstLine + i];
    KeysText(line.Keys, keys, IM_ARRAYSIZE(keys));
    text[0] = ImMax(text[0], TextSize(n.Text, 1.0f, StrBegin(d, line.Type), StrEnd(d, line.Type)).x);
    text[1] = ImMax(text[1], TextSize(n.Text, 1.0f, StrBegin(d, line.Text), StrEnd(d, line.Text)).x);
    text[2] = ImMax(text[2], TextSize(n.Text, 1.0f, keys, keys + strlen(keys)).x);
    text[3] = ImMax(text[3], TextSize(n.Text, 1.0f, StrBegin(d, line.Comment), StrEnd(d, line.Comment)).x);
  }
  float sum = 0.0f;
  for (int c = 0; c < 4; ++c) {
    g.Col[c] = text[c] > 0.0f ? ImCeil(text[c] + pad.x * 2.0f) : 0.0f;
    sum += g.Col[c];
  }
  g.Width = ImMax(ImMax(sum, name.x + pad.x * 2.0f), 80.0f);
  g.Col[1] += g.Width - sum; // the name column takes what is left
  return g;
}

bool IsBar(const ImUmlNodeRec& n) {
  return (n.Type == ImUmlNodeType_State && (n.Kind == ImUmlStateKind_Fork || n.Kind == ImUmlStateKind_Join)) ||
         (n.Type == ImUmlNodeType_Control && (n.Kind == ImUmlControlKind_Fork || n.Kind == ImUmlControlKind_Join));
}

bool Across(const ImUmlDiagram* d) {
  return d->Direction == ImUmlDirection_LeftRight || d->Direction == ImUmlDirection_RightLeft;
}

const float kSubroutineInset = 8.0f; // the inner sides of a subroutine box

/// A flowchart node's size at zoom 1 for `shape`, around `text`.
ImVec2 FlowNodeSize(const ImUmlDiagram* d, int shape, const ImVec2& text) {
  const ImVec2 pad = d->Style.NodePadding;
  const float w = text.x + pad.x * 2.0f, h = text.y + pad.y * 2.0f;
  switch (shape) {
    case ImUmlFlowShape_Rounded:
      return ImVec2(ImMax(w + d->Style.NodeRounding, 50.0f), h);
    case ImUmlFlowShape_Stadium:
      return ImVec2(ImMax(w + h * 0.5f, 50.0f), h);
    case ImUmlFlowShape_Diamond: {
      // A diamond twice as wide as tall holding the text box (tw, th):
      // half-diagonals A = tw / 2 + th and A / 2.
      const float tw = text.x + pad.x, th = text.y + pad.y;
      const float half = ImMax(tw * 0.5f + th, 24.0f);
      return ImVec2(half * 2.0f, half);
    }
    case ImUmlFlowShape_Circle: {
      const float tw = text.x + pad.x, th = text.y + pad.y;
      const float diameter = ImMax(ImSqrt(tw * tw + th * th), 30.0f);
      return ImVec2(diameter, diameter);
    }
    case ImUmlFlowShape_Parallelogram:
      return ImVec2(ImMax(w + h * 0.5f, 50.0f), h);
    case ImUmlFlowShape_Hexagon:
      return ImVec2(ImMax(w + h, 50.0f), h);
    case ImUmlFlowShape_Cylinder:
      return ImVec2(ImMax(w, 50.0f), h + 12.0f);
    case ImUmlFlowShape_Subroutine:
      return ImVec2(ImMax(w + kSubroutineInset * 2.0f, 50.0f), h);
    default:
      return ImVec2(ImMax(w, 50.0f), h);
  }
}

/// "«text»" for a shape node's stereotype into `buf`; false when it has none.
bool StereotypeText(const ImUmlDiagram* d, const ImUmlNodeRec& n, char* buf, int size) {
  if (n.Stereotype.Empty()) {
    buf[0] = 0;
    return false;
  }
  ImFormatString(buf, (size_t)size, "\xC2\xAB%.*s\xC2\xBB", n.Stereotype.Len, StrBegin(d, n.Stereotype));
  return true;
}

/// The label of a shape node at zoom `zoom`: the stereotype line over the name.
ImVec2 ShapeLabelSize(const ImUmlDiagram* d, const ImUmlNodeRec& n, float zoom) {
  const ImVec2 name = TextSize(n.Text, zoom, StrBegin(d, n.Name), StrEnd(d, n.Name));
  char buf[256];
  if (!StereotypeText(d, n, buf, IM_ARRAYSIZE(buf))) {
    return name;
  }
  const ImVec2 st = TextSize(n.Text, zoom, buf, buf + strlen(buf));
  return ImVec2(ImMax(name.x, st.x), name.y + st.y);
}

/// A node drawn with a library shape, at zoom 1.
ImVec2 MeasureShapeNode(const ImUmlDiagram* d, const ImUmlNodeRec& n, const char* shape) {
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 label = ShapeLabelSize(d, n, 1.0f);
  const ImUmlShapeArt* art = ImUml::FindShapeArt(shape);
  if (art == nullptr) {
    return label + pad * 2.0f;
  }
  if (!art->Resizable) {
    return ImVec2(ImMax(art->ViewSize.x, label.x) + 4.0f, art->ViewSize.y + 4.0f + label.y);
  }
  return ImUml::GetShapeNodeSize(shape, label + pad * 0.5f);
}

/// The header band of a shape group: its shape's top margin (a tab, a cube's
/// top face) over the title.
float ShapeGroupHeader(const ImUmlDiagram* d, const ImUmlNodeRec& n, float* width) {
  const ImVec2 label = ShapeLabelSize(d, n, 1.0f);
  const ImUmlShapeArt* art = ImUml::FindShapeArt(StrBegin(d, n.Generic));
  const float frame_sides = art != nullptr && art->HasText ? art->ViewSize.x - (art->TextMax.x - art->TextMin.x) : 0.0f;
  *width = label.x + d->Style.NodePadding.x * 2.0f + frame_sides;
  return ImCeil((art != nullptr ? art->Slice[1] : 0.0f) + label.y + d->Style.NodePadding.y * 2.0f);
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
    case ImUmlNodeType_FlowNode:
      n.Size = FlowNodeSize(d, n.Kind, text);
      n.Shape = n.Kind == ImUmlFlowShape_Diamond ? ImUmlShape_Diamond
          : n.Kind == ImUmlFlowShape_Circle      ? ImUmlShape_Circle
                                                 : ImUmlShape_Box;
      break;
    case ImUmlNodeType_ShapeNode:
    case ImUmlNodeType_ShapeGroup: // a group with nothing in it is a node
      n.Size = MeasureShapeNode(d, n, StrBegin(d, n.Generic));
      break;
    case ImUmlNodeType_FlowGroup: // a group with nothing in it: its title in a box
      n.Size = ImVec2(ImMax(text.x + pad.x * 2.0f, 60.0f), text.y + pad.y * 2.0f);
      break;
    case ImUmlNodeType_Entity: {
      const EntityGeometry g = MeasureEntity(d, n);
      n.Size = ImVec2(g.Width, g.Header + g.Row * (float)n.LineCount);
      break;
    }
    case ImUmlNodeType_ExternalEntity:
      n.Size = ImVec2(ImMax(text.x + pad.x * 3.0f, 70.0f), ImMax(text.y + pad.y * 3.0f, 34.0f));
      break;
    case ImUmlNodeType_Process: {
      const ImVec2 number = TextSize(n.Text, 1.0f, StrBegin(d, n.Stereotype), StrEnd(d, n.Stereotype));
      const float body_h = text.y + (number.y > 0.0f ? number.y + pad.y : 0.0f);
      // The circle through the corners of the text block.
      const float body_w = ImMax(text.x, number.x);
      const float diameter = ImSqrt(body_w * body_w + body_h * body_h) + pad.x * 2.0f;
      n.Size = ImVec2(ImMax(diameter, 56.0f), ImMax(diameter, 56.0f));
      n.Shape = ImUmlShape_Circle;
      break;
    }
    case ImUmlNodeType_DataStore: {
      const ImVec2 number = TextSize(n.Text, 1.0f, StrBegin(d, n.Stereotype), StrEnd(d, n.Stereotype));
      const float cell = number.x > 0.0f ? number.x + pad.x * 2.0f : 0.0f;
      n.Size = ImVec2(ImMax(cell + text.x + pad.x * 2.0f, 70.0f), text.y + pad.y * 3.0f);
      break;
    }
    case ImUmlNodeType_Device: {
      const ImVec2 address = TextSize(n.Text, 1.0f, StrBegin(d, n.Stereotype), StrEnd(d, n.Stereotype));
      n.Size = ImVec2(
          ImMax(ImMax(44.0f, text.x), address.x) + 4.0f,
          36.0f + 4.0f + text.y + (address.y > 0.0f ? address.y + 1.0f : 0.0f));
      break;
    }
    case ImUmlNodeType_Group:
      n.Size = ImVec2(text.x + pad.x * 4.0f, text.y + pad.y * 6.0f);
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
  float DashScale; ///< Dash and gap lengths times this.
  float WeightScale; ///< Line weight times this.
};

EdgeLook LookOf(const ImUmlDiagram* d, const ImUmlEdgeRec& e) {
  EdgeLook l = {false, ImUmlMarker_None, ImUmlMarker_None, false, 1.0f, 1.0f};
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
    case ImUmlEdgeType_FlowLink:
      l.Dashed = (e.Kind & ImUmlFlowLinkFlags_Dotted) != 0;
      l.WeightScale = (e.Kind & ImUmlFlowLinkFlags_Thick) != 0 ? 2.0f : 1.0f;
      l.AtTo = (e.Kind & ImUmlFlowLinkFlags_NoArrow) ? ImUmlMarker_None : ImUmlMarker_Filled;
      l.AtFrom = (e.Kind & ImUmlFlowLinkFlags_BothArrows) && l.AtTo != ImUmlMarker_None ? ImUmlMarker_Filled
                                                                                        : ImUmlMarker_None;
      break;
    case ImUmlEdgeType_Relationship:
      l.Dashed = ((e.Kind >> 8) & ImUmlRelationshipFlags_NonIdentifying) != 0;
      break;
    case ImUmlEdgeType_ShapeLink: {
      const int f = e.Kind;
      l.Dashed = (f & ImUmlShapeLinkFlags_Dashed) != 0;
      l.WeightScale = (f & ImUmlShapeLinkFlags_Thick) != 0 ? 2.0f : 1.0f;
      const ImUmlMarker m = (f & ImUmlShapeLinkFlags_ArrowHollow) ? ImUmlMarker_Hollow
          : (f & ImUmlShapeLinkFlags_ArrowFilled)                 ? ImUmlMarker_Filled
          : (f & ImUmlShapeLinkFlags_ArrowOpen)                   ? ImUmlMarker_Open
                                                                  : ImUmlMarker_None;
      l.AtTo = m;
      l.AtFrom = (f & ImUmlShapeLinkFlags_BothEnds) ? m : ImUmlMarker_None;
      l.RankToFirst = (f & ImUmlShapeLinkFlags_RankToFirst) != 0;
      break;
    }
    case ImUmlEdgeType_TreeLink:
      l.Dashed = true;
      l.AtTo = ImUmlMarker_Open;
      break;
    case ImUmlEdgeType_DataFlow:
      l.AtTo = ImUmlMarker_Filled;
      if (e.Kind & ImUmlDataFlowFlags_Bidirectional) {
        l.AtFrom = ImUmlMarker_Filled;
      }
      break;
    case ImUmlEdgeType_Link:
      if ((e.Kind >> 8) & ImUmlLinkFlags_Directed) {
        l.AtTo = ImUmlMarker_Filled;
      }
      if ((e.Kind & 255) == ImUmlLinkKind_Wireless) {
        l.Dashed = true;
      } else if ((e.Kind & 255) == ImUmlLinkKind_Vpn) {
        l.Dashed = true;
        l.DashScale = 2.4f;
        l.WeightScale = 2.0f;
      }
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
  h = ImHashData(&d->TreeMode, sizeof(d->TreeMode), h);
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

/// Lays out every node and edge of a graph diagram as one graph, composite
/// states as clusters (spec/03_uml_diagrams/DESIGN.md section 4): an edge
/// leaving a composite is routed around it like any other. Node positions
/// and edge points are relative to the content's top-left corner.
/// Returns the content size.
ImVec2 LayoutAll(ImUmlDiagram* d) {
  const ImVec2 pad = d->Style.NodePadding;
  ImUmlGraphInput in;
  in.Direction = d->Direction;
  in.EdgeStyle = d->EdgeStyle;
  in.NodeSpacing = d->NodeSpacing;
  in.RankSpacing = d->RankSpacing;
  ImVector<int> layout_of, cluster_of;
  layout_of.resize(d->Nodes.Size);
  cluster_of.resize(d->Nodes.Size);
  for (int i = 0; i < d->Nodes.Size; ++i) {
    layout_of[i] = cluster_of[i] = -1;
  }
  // Composites become clusters; a parent is always submitted before its
  // children, so cluster parents come first as LayoutGraph() expects.
  for (int i = 0; i < d->Nodes.Size; ++i) {
    ImUmlNodeRec& n = d->Nodes[i];
    if (!n.Composite) {
      continue;
    }
    float header_w = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name)).x + pad.x * 2.0f;
    n.HeaderHeight = n.Type == ImUmlNodeType_ShapeGroup ? ShapeGroupHeader(d, n, &header_w) : CompositeHeader(d, n);
    n.Shape = ImUmlShape_Box;
    ImUmlGraphCluster gc;
    gc.Parent = n.Parent >= 0 ? cluster_of[n.Parent] : -1;
    gc.HeaderSize = ImVec2(header_w, n.HeaderHeight);
    gc.Padding = pad.x * 2.0f;
    cluster_of[i] = in.Clusters.Size;
    in.Clusters.push_back(gc);
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    const ImUmlNodeRec& n = d->Nodes[i];
    if (n.Composite) {
      continue;
    }
    ImUmlGraphNode gn;
    gn.Size = n.Size;
    gn.Shape = n.Shape;
    gn.RankGroup = n.RankGroup;
    gn.Cluster = n.Parent >= 0 ? cluster_of[n.Parent] : -1;
    if (n.Type == ImUmlNodeType_Actor) {
      gn.Pin = n.Side == ImUmlSide_Right ? 2 : 1;
    }
    layout_of[i] = in.Nodes.Size;
    in.Nodes.push_back(gn);
  }
  // A note shares the rank of the first node it is attached to, in its
  // cluster.
  int next_group = d->RankGroups.Data.Size;
  for (int e = 0; e < d->Edges.Size; ++e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    if (er.Type != ImUmlEdgeType_NoteLink || er.From < 0) {
      continue;
    }
    const int note = layout_of[er.From], target = layout_of[er.To];
    if (note < 0 || target < 0 || in.Nodes[note].RankGroup >= 0 || in.Nodes[note].Pin != 0 ||
        in.Nodes[note].Cluster != in.Nodes[target].Cluster) {
      continue;
    }
    if (in.Nodes[target].RankGroup < 0) {
      in.Nodes[target].RankGroup = next_group++;
    }
    in.Nodes[note].RankGroup = in.Nodes[target].RankGroup;
  }
  ImVector<int> edge_of; // diagram edge -> layout edge, or -1
  ImVector<bool> swapped;
  edge_of.resize(d->Edges.Size);
  for (int e = 0; e < d->Edges.Size; ++e) {
    ImUmlEdgeRec& er = d->Edges[e];
    edge_of[e] = -1;
    if (er.From < 0) {
      continue;
    }
    const EdgeLook look = LookOf(d, er);
    ImUmlGraphEdge ge;
    bool swap = look.RankToFirst;
    // Use case associations: from the left actor in, out to the right one.
    if (er.Type == ImUmlEdgeType_UseCaseLink) {
      const ImUmlNodeRec& a = d->Nodes[er.From];
      const ImUmlNodeRec& b = d->Nodes[er.To];
      if (a.Type == ImUmlNodeType_Actor && b.Type == ImUmlNodeType_Actor) {
        ge.Constraint = false;
      } else if (a.Type == ImUmlNodeType_Actor) {
        swap = a.Side == ImUmlSide_Right;
      } else if (b.Type == ImUmlNodeType_Actor) {
        swap = b.Side != ImUmlSide_Right;
      }
    }
    const int from = swap ? er.To : er.From, to = swap ? er.From : er.To;
    ge.From = ImMax(0, layout_of[from]);
    ge.To = ImMax(0, layout_of[to]);
    ge.FromCluster = cluster_of[from];
    ge.ToCluster = cluster_of[to];
    ge.Weight = er.Weight;
    ge.MinLen = er.MinLen;
    ge.Constraint = ge.Constraint && er.Constraint;
    if (!er.Label.Empty()) {
      ge.LabelSize = CeilPx(TextSize(er.Text, 1.0f, StrBegin(d, er.Label), StrEnd(d, er.Label)) + ImVec2(6.0f, 2.0f));
    }
    er.LabelSize = ge.LabelSize;
    edge_of[e] = in.Edges.Size;
    swapped.push_back(swap);
    in.Edges.push_back(ge);
  }
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  for (int i = 0; i < d->Nodes.Size; ++i) {
    ImUmlNodeRec& n = d->Nodes[i];
    if (n.Composite) {
      const ImRect& r = out.ClusterRects[cluster_of[i]];
      n.Pos = r.GetCenter();
      n.Size = r.GetSize();
    } else {
      n.Pos = out.NodePos[layout_of[i]];
    }
  }
  for (int e = 0; e < d->Edges.Size; ++e) {
    if (edge_of[e] < 0) {
      continue;
    }
    const ImUmlGraphEdgeOut& eo = out.Edges[edge_of[e]];
    ImUmlGraphEdgeOut& dst = d->CacheEdges[e];
    dst = eo;
    dst.PointStart = d->Points.Size;
    for (int i = 0; i < eo.PointCount; ++i) {
      const int k = swapped[edge_of[e]] ? eo.PointCount - 1 - i : i;
      d->Points.push_back(out.Points[eo.PointStart + k]);
    }
  }
  return out.Size;
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
  d->CacheEdges.resize(d->Edges.Size);
  for (int e = 0; e < d->Edges.Size; ++e) {
    ImUmlGraphEdgeOut& eo = d->CacheEdges[e];
    eo.PointStart = eo.PointCount = 0;
    eo.Bezier = false;
    eo.LabelPos = ImVec2(0, 0);
  }
  d->Points.resize(0);
  ImVec2 size = LayoutAll(d);

  // The use case boundary: room around the use cases, between the actors.
  ImVec2 origin(kMargin, kMargin);
  const bool boundary = d->Kind == ImUmlDiagramKind_UseCase && !d->SystemLabel.Empty();
  const ImUmlTextStyle& ts = d->TitleStyle;
  const float boundary_header =
      boundary ? ImCeil(TextSize(ts, 1.0f, StrBegin(d, d->SystemLabel), StrEnd(d, d->SystemLabel)).y) + 10.0f : 0.0f;
  if (boundary) {
    origin.y += boundary_header;
  }
  for (int i = 0; i < d->Nodes.Size; ++i) {
    d->Nodes[i].Pos += origin;
  }
  for (int i = 0; i < d->Points.Size; ++i) {
    d->Points[i] += origin;
  }
  for (int e = 0; e < d->CacheEdges.Size; ++e) {
    d->CacheEdges[e].LabelPos += origin;
  }
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

// ── Tree layout ──────────────────────────────────────────────────────────────

const float kOrgPortrait = 40.0f; // org chart portrait, zoom 1

/// A tree node's size at zoom 1 for the diagram's mode.
ImVec2 MeasureTreeNode(const ImUmlDiagram* d, const ImUmlNodeRec& n, int depth) {
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 name = TextSize(n.Text, 1.0f, StrBegin(d, n.Name), StrEnd(d, n.Name));
  switch (d->TreeMode) {
    case ImUmlTreeMode_OrgChart: {
      const ImVec2 title = TextSize(n.Text, 1.0f, StrBegin(d, n.Stereotype), StrEnd(d, n.Stereotype));
      const ImVec2 detail = TextSize(n.Text, 1.0f, StrBegin(d, n.Detail), StrEnd(d, n.Detail));
      const bool portrait = !n.Image.Empty() && !(n.TreeFlags & ImUmlTreeFlags_Vacant);
      const float text_w = ImMax(name.x, ImMax(title.x, detail.x));
      const float text_h = name.y + title.y + detail.y;
      // The card's content (portrait beside the text lines), padded; the
      // shape grows around it so it fits inside the shape's text area.
      const ImVec2 content((portrait ? kOrgPortrait + pad.x : 0.0f) + text_w + pad.x,
                           ImMax(text_h, portrait ? kOrgPortrait : 0.0f) + pad.y);
      const bool vacant = (n.TreeFlags & ImUmlTreeFlags_Vacant) != 0;
      const char* shape = vacant ? "dashed_region" : (n.Generic.Empty() ? "rounded_rectangle" : StrBegin(d, n.Generic));
      ImVec2 size = ImUml::FindShapeArt(shape) != nullptr && ImUml::FindShapeArt(shape)->Resizable
          ? ImUml::GetShapeNodeSize(shape, content)
          : content + pad;
      size.x = ImMax(size.x, 110.0f);
      return size;
    }
    case ImUmlTreeMode_MindMap:
      if (depth == 0) {
        return ImVec2(name.x + pad.x * 4.0f, name.y + pad.y * 3.0f);
      }
      if (depth == 1) {
        return ImVec2(name.x + pad.x * 3.0f, name.y + pad.y * 2.0f);
      }
      return ImVec2(name.x + pad.x, name.y + pad.y + 3.0f);
    default:
      return MeasureShapeNode(d, n, n.Generic.Empty() ? "rounded_rectangle" : StrBegin(d, n.Generic));
  }
}

/// Lays out a tree diagram; node Pos / Size at zoom 1, with the margin.
ImVec2 LayoutTreeDiagram(ImUmlDiagram* d) {
  const int count = d->Nodes.Size;
  ImUmlTreeInput in;
  in.Mode = d->TreeMode;
  in.Direction = d->TreeMode == ImUmlTreeMode_MindMap ? ImUmlDirection_LeftRight : d->Direction;
  in.NodeSpacing = d->NodeSpacing;
  in.RankSpacing = d->RankSpacing;
  ImVector<int> depth;
  depth.resize(count);
  for (int i = 0; i < count; ++i) {
    ImUmlNodeRec& n = d->Nodes[i];
    n.Composite = false;
    depth[i] = n.Parent >= 0 ? depth[n.Parent] + 1 : 0;
    n.Size = CeilPx(MeasureTreeNode(d, n, depth[i]));
    ImUmlTreeNodeIn tn;
    tn.Size = n.Size;
    tn.Parent = n.Parent;
    tn.Side = n.Parent == 0 ? n.Side : -1;
    tn.Hanging = (n.TreeFlags & ImUmlTreeFlags_Hanging) != 0;
    tn.Staff = (n.TreeFlags & ImUmlTreeFlags_Staff) != 0;
    in.Nodes.push_back(tn);
  }
  ImUmlTreeOutput out;
  ImUml::LayoutTree(in, &out);
  const ImVec2 origin(kMargin, kMargin);
  for (int i = 0; i < count; ++i) {
    d->Nodes[i].Pos = (i < out.Pos.Size ? out.Pos[i] : ImVec2(0, 0)) + origin;
  }
  d->TreeDepth = out.Depth;
  d->TreeBranch = out.Branch;
  d->TreeHang = out.HangRoot;
  d->TreeRight = out.OnRight;
  d->CacheEdges.resize(d->Edges.Size);
  for (int e = 0; e < d->Edges.Size; ++e) {
    d->CacheEdges[e].PointStart = d->CacheEdges[e].PointCount = 0;
    d->CacheEdges[e].Bezier = false;
    d->CacheEdges[e].LabelPos = ImVec2(0, 0);
  }
  d->Points.resize(0);
  d->Boundary = ImRect(0, 0, 0, 0);
  return CeilPx(out.Size + origin * 2.0f);
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

/// Draws library shape `name` into [a, b); false when it is not registered
/// (ImUml without nymph's shape library), so the caller draws its fallback.
bool DrawLibraryShape(const Painter& pt, const char* name, const ImVec2& a, const ImVec2& b, ImU32 fill, ImU32 border) {
  return ImUml::RenderShape(pt.DL, name, a, b, border, fill, pt.LW, pt.Zoom);
}

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
  if (n.Kind == ImUmlClassKind_Object) {
    // An instance: its "name : class" is underlined.
    const ImVec2 s = TextSize(n.Text, pt.Zoom, StrBegin(d, n.Name), StrEnd(d, n.Name));
    const float uy = ImFloor(y + s.y) + 0.5f;
    pt.DL->AddLine(ImVec2(cx - s.x * 0.5f, uy), ImVec2(cx + s.x * 0.5f, uy), n.Text.Col, ImMax(1.0f, pt.Zoom));
  }
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

/// Inside of a flowchart group: its fill colour, else halfway between the
/// background and the node fill, so nodes stand out on it.
ImU32 GroupFill(const Painter& pt, const ImUmlNodeRec& n) {
  if (n.FillCol != 0) {
    return n.FillCol;
  }
  const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(pt.Col(ImUmlCol_Bg));
  const ImVec4 node = ImGui::ColorConvertU32ToFloat4(pt.Col(ImUmlCol_NodeFill));
  return ImGui::ColorConvertFloat4ToU32(ImLerp(bg, node, 0.5f));
}

void DrawFlowNode(const Painter& pt, const ImUmlNodeRec& n, ImU32 fill, ImU32 border) {
  const ImUmlDiagram* d = pt.D;
  const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
  const ImVec2 c = pt.P(n.Pos);
  const float h = b.y - a.y;
  const char* nb = StrBegin(d, n.Name);
  const char* ne = StrEnd(d, n.Name);
  const ImVec2 ts = TextSize(n.Text, pt.Zoom, nb, ne);
  float text_y = (a.y + b.y - ts.y) * 0.5f;
  switch (n.Kind) {
    case ImUmlFlowShape_Rounded:
    case ImUmlFlowShape_Stadium: {
      const float r = n.Kind == ImUmlFlowShape_Stadium ? h * 0.5f : pt.S(d->Style.NodeRounding);
      pt.DL->AddRectFilled(a, b, fill, r);
      pt.DL->AddRect(a, b, border, r, ImDrawFlags_None, pt.LW);
      break;
    }
    case ImUmlFlowShape_Diamond:
      ImUml::RenderDiamond(pt.DL, c, n.Size * 0.5f * pt.Zoom, fill, border, pt.LW);
      break;
    case ImUmlFlowShape_Circle: {
      const float r = (b.x - a.x) * 0.5f;
      const int seg = ImUml::CircleSegments(r);
      pt.DL->AddCircleFilled(c, r, fill, seg);
      pt.DL->AddCircle(c, r - pt.LW * 0.5f, border, seg, pt.LW);
      break;
    }
    case ImUmlFlowShape_Parallelogram:
    case ImUmlFlowShape_Hexagon: {
      ImVec2 pts[6];
      int count = 0;
      if (n.Kind == ImUmlFlowShape_Parallelogram) {
        const float s = h * 0.5f;
        pts[count++] = ImVec2(a.x + s, a.y);
        pts[count++] = ImVec2(b.x, a.y);
        pts[count++] = ImVec2(b.x - s, b.y);
        pts[count++] = ImVec2(a.x, b.y);
      } else {
        const float s = h * 0.5f;
        pts[count++] = ImVec2(a.x + s, a.y);
        pts[count++] = ImVec2(b.x - s, a.y);
        pts[count++] = ImVec2(b.x, c.y);
        pts[count++] = ImVec2(b.x - s, b.y);
        pts[count++] = ImVec2(a.x + s, b.y);
        pts[count++] = ImVec2(a.x, c.y);
      }
      pt.DL->AddConvexPolyFilled(pts, count, fill);
      pt.DL->AddPolyline(pts, count, border, ImDrawFlags_Closed, pt.LW);
      break;
    }
    case ImUmlFlowShape_Cylinder:
      if (!DrawLibraryShape(pt, "database", a, b, fill, border)) {
        ImUml::RenderCylinder(pt.DL, a, b, fill, border, pt.LW);
      }
      text_y += ImMin(h * 0.15f, 8.0f) * 0.5f; // below the top rim
      break;
    case ImUmlFlowShape_Subroutine: {
      if (DrawLibraryShape(pt, "predefined_process", a, b, fill, border)) {
        break;
      }
      pt.DL->AddRectFilled(a, b, fill);
      pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
      const float inset = pt.S(kSubroutineInset);
      pt.DL->AddLine(ImVec2(a.x + inset, a.y), ImVec2(a.x + inset, b.y), border, pt.LW);
      pt.DL->AddLine(ImVec2(b.x - inset, a.y), ImVec2(b.x - inset, b.y), border, pt.LW);
      break;
    }
    default:
      pt.DL->AddRectFilled(a, b, fill);
      pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
      break;
  }
  TextCentered(pt.DL, n.Text, pt.Zoom, c.x, text_y, nb, ne, n.Text.Col);
}

/// The library shape that draws a network device of `kind`.
const char* DeviceShape(ImUmlDeviceKind kind) {
  static const char* const names[] = {"server", "database", "storage", "router", "switch", "firewall", "load_balancer",
                                      "access_point", "cloud", "internet", "workstation", "laptop", "mobile", "user"};
  return kind >= 0 && kind < (int)IM_ARRAYSIZE(names) ? names[kind] : "server";
}

/// A node or group drawn with library shape `shape`.
void DrawShapeNodeAs(const Painter& pt, const ImUmlNodeRec& n, const char* shape, ImU32 fill, ImU32 border) {
  const ImUmlDiagram* d = pt.D;
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
  const ImUmlShapeArt* art = ImUml::FindShapeArt(shape);
  char sbuf[256];
  const bool has_stereotype = StereotypeText(d, n, sbuf, IM_ARRAYSIZE(sbuf));
  const char* nb = StrBegin(d, n.Name);
  const char* ne = StrEnd(d, n.Name);
  const float stereotype_h = has_stereotype ? TextSize(n.Text, pt.Zoom, sbuf, sbuf + strlen(sbuf)).y : 0.0f;
  const float block_h = stereotype_h + TextSize(n.Text, pt.Zoom, nb, ne).y;
  // The label: stereotype over name, centred on `cx` from `y`.
  const auto label = [&](float cx, float y) {
    if (has_stereotype) {
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, sbuf, sbuf + strlen(sbuf), n.Text.Col);
      y += stereotype_h;
    }
    TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, nb, ne, n.Text.Col);
  };
  const float cx = (a.x + b.x) * 0.5f;
  if (n.Composite) {
    const ImU32 inside = GroupFill(pt, n);
    if (art != nullptr) {
      ImUml::RenderShape(pt.DL, shape, a, b, border, inside, pt.LW, pt.Zoom);
    } else {
      pt.DL->AddRectFilled(a, b, inside);
      pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
    }
    label(cx, a.y + pt.S(art != nullptr ? art->Slice[1] : 0.0f) + pt.S(pad.y));
    return;
  }
  if (art == nullptr) {
    pt.DL->AddRectFilled(a, b, fill);
    pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
    label(cx, (a.y + b.y - block_h) * 0.5f);
    return;
  }
  if (!art->Resizable) {
    const ImVec2 view = art->ViewSize * pt.Zoom;
    ImUml::RenderShape(pt.DL, shape, ImVec2(cx - view.x * 0.5f, a.y), ImVec2(cx + view.x * 0.5f, a.y + view.y), border,
                       fill, pt.LW, pt.Zoom);
    label(cx, a.y + view.y + pt.S(4.0f));
    return;
  }
  ImUml::RenderShape(pt.DL, shape, a, b, border, fill, pt.LW, pt.Zoom);
  ImVec2 tmin, tmax;
  ImUml::GetShapeTextRect(shape, a, b, pt.Zoom, &tmin, &tmax);
  label((tmin.x + tmax.x) * 0.5f, (tmin.y + tmax.y - block_h) * 0.5f);
}

void DrawNode(const Painter& pt, const ImUmlNodeRec& n) {
  const ImUmlDiagram* d = pt.D;
  BeginRuns(ImUmlTextRunKind_Node, (int)(&n - d->Nodes.Data));
  const ImU32 fill = n.FillCol != 0 ? n.FillCol : pt.Col(ImUmlCol_NodeFill);
  const ImU32 border = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NodeBorder);
  const ImU32 line = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_Line);
  const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
  CaptureArea(a, b);
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
      if (!DrawLibraryShape(pt, "actor", ImVec2(cx - fig_w * 0.5f, a.y), ImVec2(cx + fig_w * 0.5f, a.y + fig_h), fill,
                            border)) {
        ImUml::RenderActorFigure(pt.DL, ImVec2(cx - fig_w * 0.5f, a.y), ImVec2(cx + fig_w * 0.5f, a.y + fig_h), border,
                                 pt.LW);
      }
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
    case ImUmlNodeType_FlowNode:
      DrawFlowNode(pt, n, fill, border);
      break;
    case ImUmlNodeType_ShapeNode:
    case ImUmlNodeType_ShapeGroup:
      DrawShapeNodeAs(pt, n, StrBegin(d, n.Generic), fill, border);
      break;
    case ImUmlNodeType_FlowGroup: {
      if (!n.Composite) {
        pt.DL->AddRectFilled(a, b, GroupFill(pt, n));
        pt.DL->AddRect(a, b, n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_Boundary), 0.0f, ImDrawFlags_None, pt.LW);
        TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
        break;
      }
      pt.DL->AddRectFilled(a, b, GroupFill(pt, n));
      pt.DL->AddRect(a, b, n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_Boundary), 0.0f, ImDrawFlags_None, pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, a.y + pt.S(pad.y), nb, ne, n.Text.Col);
      break;
    }
    case ImUmlNodeType_Group: {
      // A dashed box, its label in the top-left corner.
      const float rounding = pt.S(d->Style.NodeRounding);
      const ImU32 edge = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_Boundary);
      if (n.FillCol != 0) {
        pt.DL->AddRectFilled(a, b, n.FillCol, rounding);
      }
      const ImVec2 outline[5] = {a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y), a};
      ImUml::RenderPath(pt.DL, outline, 5, edge, pt.LW, true, pt.S(d->Style.DashLength * 1.4f), pt.S(d->Style.GapLength));
      TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(a.x + pt.S(pad.x), a.y + pt.S(pad.y)), nb, ne, n.Text.Col);
      break;
    }
    case ImUmlNodeType_ExternalEntity:
      pt.DL->AddRectFilled(a, b, fill);
      pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW * 2.0f);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, (a.y + b.y - ts.y) * 0.5f, nb, ne, n.Text.Col);
      break;
    case ImUmlNodeType_Process: {
      const ImVec2 c = pt.P(n.Pos);
      const float r = (b.x - a.x) * 0.5f;
      pt.DL->AddCircleFilled(c, r, fill, ImUml::CircleSegments(r));
      pt.DL->AddCircle(c, r, border, ImUml::CircleSegments(r), pt.LW);
      const char* sb = StrBegin(d, n.Stereotype);
      const char* se = StrEnd(d, n.Stereotype);
      const ImVec2 number = TextSize(n.Text, pt.Zoom, sb, se);
      const float gap = number.y > 0.0f ? pt.S(pad.y) : 0.0f;
      float y = c.y - (number.y + gap + ts.y) * 0.5f;
      if (number.y > 0.0f) {
        TextCentered(pt.DL, n.Text, pt.Zoom, c.x, y, sb, se, n.Text.Col);
        y += number.y;
        pt.DL->AddLine(ImVec2(c.x - r * 0.6f, y + gap * 0.5f), ImVec2(c.x + r * 0.6f, y + gap * 0.5f), border, pt.LW);
        y += gap;
      }
      TextCentered(pt.DL, n.Text, pt.Zoom, c.x, y, nb, ne, n.Text.Col);
      break;
    }
    case ImUmlNodeType_DataStore: {
      // Open on the right: top and bottom lines, the left side, and the
      // number's cell.
      pt.DL->AddRectFilled(a, b, fill);
      pt.DL->AddLine(a, ImVec2(b.x, a.y), border, pt.LW);
      pt.DL->AddLine(ImVec2(a.x, b.y), b, border, pt.LW);
      pt.DL->AddLine(a, ImVec2(a.x, b.y), border, pt.LW);
      const char* sb = StrBegin(d, n.Stereotype);
      const char* se = StrEnd(d, n.Stereotype);
      float x = a.x;
      if (sb != se) {
        const float cell = TextSize(n.Text, pt.Zoom, sb, se).x + pt.S(pad.x * 2.0f);
        TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(a.x + pt.S(pad.x), (a.y + b.y - ts.y) * 0.5f), sb, se, n.Text.Col);
        x += cell;
        pt.DL->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), border, pt.LW);
      }
      TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(x + pt.S(pad.x), (a.y + b.y - ts.y) * 0.5f), nb, ne, n.Text.Col);
      break;
    }
    case ImUmlNodeType_Device: {
      const float icon_w = pt.S(44.0f), icon_h = pt.S(36.0f);
      if (!DrawLibraryShape(pt, DeviceShape(n.Kind), ImVec2(cx - icon_w * 0.5f, a.y),
                            ImVec2(cx + icon_w * 0.5f, a.y + icon_h), fill, border)) {
        ImUml::RenderDeviceIcon(pt.DL, n.Kind, ImVec2(cx - icon_w * 0.5f, a.y), ImVec2(cx + icon_w * 0.5f, a.y + icon_h),
                                border, fill, pt.LW);
      }
      float y = a.y + icon_h + pt.S(4.0f);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, nb, ne, n.Text.Col);
      y += ts.y + pt.S(1.0f);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, y, StrBegin(d, n.Stereotype), StrEnd(d, n.Stereotype),
                   pt.Col(ImUmlCol_Lifeline));
      break;
    }
    case ImUmlNodeType_Entity: {
      const EntityGeometry g = MeasureEntity(d, n);
      const float header_bottom = a.y + pt.S(g.Header);
      pt.DL->AddRectFilled(a, b, fill);
      pt.DL->AddRectFilled(a, ImVec2(b.x, header_bottom), pt.Col(ImUmlCol_FragmentTab));
      pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, a.y + pt.S(pad.y), nb, ne, n.Text.Col);
      if (n.LineCount == 0) {
        break;
      }
      pt.DL->AddLine(ImVec2(a.x, header_bottom), ImVec2(b.x, header_bottom), border, pt.LW);
      const ImU32 grid = pt.Col(ImUmlCol_Lifeline);
      char keys[32];
      for (int i = 0; i < n.LineCount; ++i) {
        const ImUmlLine& line = d->Lines[n.FirstLine + i];
        const float y = header_bottom + pt.S(g.Row * (float)i);
        if (i > 0) {
          pt.DL->AddLine(ImVec2(a.x + pt.LW, y), ImVec2(b.x - pt.LW, y), grid, 1.0f);
        }
        KeysText(line.Keys, keys, IM_ARRAYSIZE(keys));
        const char* cells[4][2] = {
            {StrBegin(d, line.Type), StrEnd(d, line.Type)},
            {StrBegin(d, line.Text), StrEnd(d, line.Text)},
            {keys, keys + strlen(keys)},
            {StrBegin(d, line.Comment), StrEnd(d, line.Comment)}};
        float x = a.x;
        for (int c = 0; c < 4; ++c) {
          if (g.Col[c] <= 0.0f) {
            continue;
          }
          TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(x + pt.S(pad.x), y + pt.S(pad.y * 0.5f)), cells[c][0], cells[c][1],
                 n.Text.Col);
          x += pt.S(g.Col[c]);
        }
      }
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

/// The part of polyline `pts` between arc lengths `from` and `to`.
void PathSlice(const ImVector<ImVec2>& pts, float from, float to, ImVector<ImVec2>* out) {
  out->resize(0);
  float at = 0.0f;
  for (int i = 0; i + 1 < pts.Size; ++i) {
    const ImVec2 a = pts[i], b = pts[i + 1];
    const float seg = ImSqrt(ImLengthSqr(b - a));
    if (seg <= 0.0f) {
      continue;
    }
    const float s0 = ImMax(from, at), s1 = ImMin(to, at + seg);
    if (s1 > s0) {
      const ImVec2 p0 = a + (b - a) * ((s0 - at) / seg), p1 = a + (b - a) * ((s1 - at) / seg);
      if (out->Size == 0) {
        out->push_back(p0);
      }
      out->push_back(p1);
    }
    at += seg;
  }
}

/// A ball-and-socket connector along `path`: a ball on the provider (start)
/// side cupped by a socket on the consumer side, at the middle of the path.
void DrawAssembly(const Painter& pt, ImU32 col, const ImVector<ImVec2>& path) {
  float total = 0.0f;
  for (int i = 0; i + 1 < path.Size; ++i) {
    total += ImSqrt(ImLengthSqr(path[i + 1] - path[i]));
  }
  const float rb = pt.S(5.0f), rs = pt.S(8.5f);
  if (path.Size < 2 || total < 2.0f * rs + 2.0f) {
    ImUml::RenderPath(pt.DL, path.Data, path.Size, col, pt.LW, false, 0.0f, 0.0f);
    return;
  }
  const float mid = total * 0.5f;
  ImVector<ImVec2> tail, head, around;
  PathSlice(path, mid - 1.0f, mid + 1.0f, &around);
  ImVec2 m = path[0], t(0.0f, 1.0f);
  if (around.Size >= 2) {
    m = (around[0] + around.back()) * 0.5f;
    const ImVec2 dir = around.back() - around[0];
    const float len = ImSqrt(ImLengthSqr(dir));
    t = len > 0.0f ? dir * (1.0f / len) : t;
  }
  PathSlice(path, 0.0f, mid - rb, &tail);
  PathSlice(path, mid + rs, total, &head);
  if (tail.Size >= 2) {
    ImUml::RenderPath(pt.DL, tail.Data, tail.Size, col, pt.LW, false, 0.0f, 0.0f);
  }
  if (head.Size >= 2) {
    ImUml::RenderPath(pt.DL, head.Data, head.Size, col, pt.LW, false, 0.0f, 0.0f);
  }
  const float theta = ImAtan2(t.y, t.x);
  const int seg = ImUml::CircleSegments(rs);
  pt.DL->PathArcTo(m, rs, theta - IM_PI * 0.5f, theta + IM_PI * 0.5f, seg / 2);
  pt.DL->PathStroke(col, ImDrawFlags_None, pt.LW);
  pt.DL->AddCircleFilled(m, rb, pt.Col(ImUmlCol_NodeFill), ImUml::CircleSegments(rb));
  pt.DL->AddCircle(m, rb, col, ImUml::CircleSegments(rb), pt.LW);
}

void DrawEdge(const Painter& pt, const ImUmlEdgeRec& e, const ImUmlGraphEdgeOut& eo, ImVector<ImVec2>* scratch) {
  const ImUmlDiagram* d = pt.D;
  BeginRuns(ImUmlTextRunKind_Edge, (int)(&e - d->Edges.Data));
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
  CapturePath(path.Data, path.Size, kPathPad);
  const float size = pt.S(d->Style.ArrowSize);
  const ImVec2 to_dir = EndDirection(path, true, size);
  const ImVec2 from_dir = EndDirection(path, false, size);
  const ImVec2 to_tip = path.back(), from_tip = path[0];
  TrimPath(&path, ImUml::MarkerLength(look.AtTo, size), true);
  TrimPath(&path, ImUml::MarkerLength(look.AtFrom, size), false);
  if (e.Type == ImUmlEdgeType_ShapeLink && (e.Kind & ImUmlShapeLinkFlags_Assembly)) {
    DrawAssembly(pt, col, path);
  } else {
    ImUml::RenderPath(pt.DL, path.Data, path.Size, col, pt.LW * look.WeightScale, look.Dashed,
                      pt.S(d->Style.DashLength * look.DashScale), pt.S(d->Style.GapLength * look.DashScale));
  }
  ImUml::RenderMarker(pt.DL, look.AtTo, to_tip, to_dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
  ImUml::RenderMarker(pt.DL, look.AtFrom, from_tip, from_dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
  if (e.Type == ImUmlEdgeType_Relationship) {
    ImUml::RenderCardinality(pt.DL, e.Kind & 15, from_tip, from_dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
    ImUml::RenderCardinality(pt.DL, (e.Kind >> 4) & 15, to_tip, to_dir, size, col, pt.Col(ImUmlCol_Bg), pt.LW);
  }
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

// ── Tree drawing ─────────────────────────────────────────────────────────────

const ImU32 kBranchColors[8] = {
    IM_COL32(78, 121, 167, 255), IM_COL32(242, 142, 43, 255), IM_COL32(89, 161, 79, 255), IM_COL32(225, 87, 89, 255),
    IM_COL32(176, 122, 161, 255), IM_COL32(118, 183, 178, 255), IM_COL32(237, 201, 72, 255), IM_COL32(255, 157, 167, 255)};

/// Black or white, whichever reads on `col`.
ImU32 TextOn(ImU32 col) {
  const ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
  return 0.299f * c.x + 0.587f * c.y + 0.114f * c.z > 0.6f ? IM_COL32(20, 20, 20, 255) : IM_COL32(255, 255, 255, 255);
}

/// The colour of a mind map node's branch: the nearest colour set on it or
/// an ancestor below the root, else the palette entry of its branch.
ImU32 BranchColor(const ImUmlDiagram* d, int i) {
  for (int a = i; a > 0; a = d->Nodes[a].Parent) {
    if (d->Nodes[a].LineCol != 0) {
      return d->Nodes[a].LineCol;
    }
  }
  if (i == 0) {
    return d->Nodes[0].LineCol != 0 ? d->Nodes[0].LineCol : d->Colors[ImUmlCol_NodeBorder];
  }
  return kBranchColors[d->TreeBranch[i] & 7];
}

/// Strokes the polyline or curve of one tree connector.
void StrokeConnector(const Painter& pt, const ImVec2* pts, int count, bool bezier, ImU32 col, float lw) {
  ImVector<ImVec2> flat;
  ImUml::FlattenPath(pts, count, bezier, &flat);
  ImUml::RenderPath(pt.DL, flat.Data, flat.Size, col, lw, false, 0.0f, 0.0f);
}

/// Whether the connector of node `i` is drawn with its parent's other
/// elbow connectors (one stem and one bus per parent).
bool IsBusChild(const ImUmlDiagram* d, int i) {
  const ImUmlNodeRec& c = d->Nodes[i];
  return d->TreeMode != ImUmlTreeMode_MindMap && d->EdgeStyle == ImUmlEdgeStyle_Elbow && c.Parent >= 0 &&
         d->TreeHang[i] < 0 && !(c.TreeFlags & ImUmlTreeFlags_Staff);
}

void DrawElbowBuses(const Painter& pt) {
  const ImUmlDiagram* d = pt.D;
  const bool vertical = d->Direction == ImUmlDirection_TopDown || d->Direction == ImUmlDirection_BottomUp;
  for (int p = 0; p < d->Nodes.Size; ++p) {
    float lo = FLT_MAX, hi = -FLT_MAX;
    int kids = 0;
    for (int i = p + 1; i < d->Nodes.Size; ++i) {
      if (d->Nodes[i].Parent != p || !IsBusChild(d, i)) {
        continue;
      }
      const ImVec2 c = pt.P(d->Nodes[i].Pos);
      lo = ImMin(lo, vertical ? c.x : c.y);
      hi = ImMax(hi, vertical ? c.x : c.y);
      ++kids;
    }
    if (kids == 0) {
      continue;
    }
    const ImUmlNodeRec& pa = d->Nodes[p];
    const ImVec2 pmin = pt.P(pa.Pos - pa.Size * 0.5f), pmax = pt.P(pa.Pos + pa.Size * 0.5f), pc = pt.P(pa.Pos);
    const ImU32 col = pa.LineCol != 0 && false ? pa.LineCol : pt.Col(ImUmlCol_Line);
    // The bus runs halfway between the parent and its nearest child.
    float near_edge = FLT_MAX;
    for (int i = p + 1; i < d->Nodes.Size; ++i) {
      if (d->Nodes[i].Parent != p || !IsBusChild(d, i)) {
        continue;
      }
      const ImVec2 cmin = pt.P(d->Nodes[i].Pos - d->Nodes[i].Size * 0.5f), cmax = pt.P(d->Nodes[i].Pos + d->Nodes[i].Size * 0.5f);
      switch (d->Direction) {
        case ImUmlDirection_BottomUp:
          near_edge = ImMin(near_edge, pmin.y - cmax.y);
          break;
        case ImUmlDirection_LeftRight:
          near_edge = ImMin(near_edge, cmin.x - pmax.x);
          break;
        case ImUmlDirection_RightLeft:
          near_edge = ImMin(near_edge, pmin.x - cmax.x);
          break;
        default:
          near_edge = ImMin(near_edge, cmin.y - pmax.y);
          break;
      }
    }
    const float bus = ImMin(near_edge * 0.5f, pt.S(d->RankSpacing * 0.5f));
    ImVec2 stem_from, stem_to, bus_a, bus_b;
    switch (d->Direction) {
      case ImUmlDirection_BottomUp:
        stem_from = ImVec2(pc.x, pmin.y);
        stem_to = ImVec2(pc.x, pmin.y - bus);
        bus_a = ImVec2(lo, stem_to.y);
        bus_b = ImVec2(hi, stem_to.y);
        break;
      case ImUmlDirection_LeftRight:
        stem_from = ImVec2(pmax.x, pc.y);
        stem_to = ImVec2(pmax.x + bus, pc.y);
        bus_a = ImVec2(stem_to.x, lo);
        bus_b = ImVec2(stem_to.x, hi);
        break;
      case ImUmlDirection_RightLeft:
        stem_from = ImVec2(pmin.x, pc.y);
        stem_to = ImVec2(pmin.x - bus, pc.y);
        bus_a = ImVec2(stem_to.x, lo);
        bus_b = ImVec2(stem_to.x, hi);
        break;
      default:
        stem_from = ImVec2(pc.x, pmax.y);
        stem_to = ImVec2(pc.x, pmax.y + bus);
        bus_a = ImVec2(lo, stem_to.y);
        bus_b = ImVec2(hi, stem_to.y);
        break;
    }
    const ImVec2 stem[2] = {stem_from, stem_to};
    StrokeConnector(pt, stem, 2, false, col, pt.LW);
    if (kids > 1 || (vertical ? ImFabs(bus_a.x - stem_to.x) : ImFabs(bus_a.y - stem_to.y)) > 0.5f) {
      const float a = vertical ? ImMin(lo, stem_to.x) : ImMin(lo, stem_to.y);
      const float b = vertical ? ImMax(hi, stem_to.x) : ImMax(hi, stem_to.y);
      const ImVec2 bar[2] = {vertical ? ImVec2(a, stem_to.y) : ImVec2(stem_to.x, a),
                             vertical ? ImVec2(b, stem_to.y) : ImVec2(stem_to.x, b)};
      StrokeConnector(pt, bar, 2, false, col, pt.LW);
    }
    for (int i = p + 1; i < d->Nodes.Size; ++i) {
      if (d->Nodes[i].Parent != p || !IsBusChild(d, i)) {
        continue;
      }
      const ImUmlNodeRec& c = d->Nodes[i];
      const ImVec2 cmin = pt.P(c.Pos - c.Size * 0.5f), cmax = pt.P(c.Pos + c.Size * 0.5f), cc = pt.P(c.Pos);
      ImVec2 on_bus, at_child;
      switch (d->Direction) {
        case ImUmlDirection_BottomUp:
          on_bus = ImVec2(cc.x, stem_to.y);
          at_child = ImVec2(cc.x, cmax.y);
          break;
        case ImUmlDirection_LeftRight:
          on_bus = ImVec2(stem_to.x, cc.y);
          at_child = ImVec2(cmin.x, cc.y);
          break;
        case ImUmlDirection_RightLeft:
          on_bus = ImVec2(stem_to.x, cc.y);
          at_child = ImVec2(cmax.x, cc.y);
          break;
        default:
          on_bus = ImVec2(cc.x, stem_to.y);
          at_child = ImVec2(cc.x, cmin.y);
          break;
      }
      const ImVec2 drop[2] = {on_bus, at_child};
      StrokeConnector(pt, drop, 2, false, c.LineCol != 0 ? c.LineCol : col, pt.LW);
    }
  }
}

void DrawTreeEdges(const Painter& pt) {
  const ImUmlDiagram* d = pt.D;
  const bool mind = d->TreeMode == ImUmlTreeMode_MindMap;
  DrawElbowBuses(pt);
  for (int i = 1; i < d->Nodes.Size; ++i) {
    const ImUmlNodeRec& c = d->Nodes[i];
    if (c.Parent < 0 || IsBusChild(d, i)) {
      continue;
    }
    const ImUmlNodeRec& pa = d->Nodes[c.Parent];
    const ImVec2 pmin = pt.P(pa.Pos - pa.Size * 0.5f), pmax = pt.P(pa.Pos + pa.Size * 0.5f);
    const ImVec2 cmin = pt.P(c.Pos - c.Size * 0.5f), cmax = pt.P(c.Pos + c.Size * 0.5f);
    const ImVec2 pc = (pmin + pmax) * 0.5f, cc = (cmin + cmax) * 0.5f;
    ImUmlDirection dir = d->Direction;
    if (mind) {
      dir = d->TreeRight[i] ? ImUmlDirection_LeftRight : ImUmlDirection_RightLeft;
    }
    ImU32 col = pt.Col(ImUmlCol_Line);
    float lw = pt.LW;
    if (mind) {
      col = BranchColor(d, i);
      const int depth = d->TreeDepth[i];
      lw = pt.LW * (depth <= 1 ? 2.4f : depth == 2 ? 1.7f : 1.2f);
    } else if (c.LineCol != 0) {
      col = c.LineCol;
    }
    const bool staff = (c.TreeFlags & ImUmlTreeFlags_Staff) != 0 && !mind;
    if (staff) {
      const ImVec2 pts[2] = {ImVec2(pmax.x, pc.y), ImVec2(cmin.x, cc.y)};
      StrokeConnector(pt, pts, 2, false, col, lw);
      continue;
    }
    if (d->TreeHang[i] >= 0 && d->Direction == ImUmlDirection_TopDown) {
      // The spine runs down the parent's left side; a tick to each child.
      const float spine = pmin.x + pt.S(12.0f);
      const ImVec2 pts[3] = {ImVec2(spine, pmax.y), ImVec2(spine, cc.y), ImVec2(cmin.x, cc.y)};
      StrokeConnector(pt, pts, 3, false, col, lw);
      continue;
    }
    ImVec2 from, to;
    switch (dir) {
      case ImUmlDirection_BottomUp:
        from = ImVec2(pc.x, pmin.y);
        to = ImVec2(cc.x, cmax.y);
        break;
      case ImUmlDirection_LeftRight:
        from = ImVec2(pmax.x, pc.y);
        to = ImVec2(cmin.x, cc.y);
        break;
      case ImUmlDirection_RightLeft:
        from = ImVec2(pmin.x, pc.y);
        to = ImVec2(cmax.x, cc.y);
        break;
      default:
        from = ImVec2(pc.x, pmax.y);
        to = ImVec2(cc.x, cmin.y);
        break;
    }
    const bool vertical = dir == ImUmlDirection_TopDown || dir == ImUmlDirection_BottomUp;
    const float delta = vertical ? to.y - from.y : to.x - from.x;
    const float sign = delta < 0.0f ? -1.0f : 1.0f;
    if (d->EdgeStyle == ImUmlEdgeStyle_Straight) {
      const ImVec2 pts[2] = {from, to};
      StrokeConnector(pt, pts, 2, false, col, lw);
    } else if (d->EdgeStyle == ImUmlEdgeStyle_Curve) {
      const float k = ImFabs(delta) * 0.5f * sign;
      const ImVec2 pts[4] = {
          from,
          vertical ? ImVec2(from.x, from.y + k) : ImVec2(from.x + k, from.y),
          vertical ? ImVec2(to.x, to.y - k) : ImVec2(to.x - k, to.y),
          to};
      StrokeConnector(pt, pts, 4, true, col, lw);
    } else {
      const float bus_at = ImMin(ImFabs(delta) * 0.5f, pt.S(d->RankSpacing * 0.5f)) * sign;
      ImVec2 pts[4];
      if (vertical) {
        pts[0] = from;
        pts[1] = ImVec2(from.x, from.y + bus_at);
        pts[2] = ImVec2(to.x, from.y + bus_at);
        pts[3] = to;
      } else {
        pts[0] = from;
        pts[1] = ImVec2(from.x + bus_at, from.y);
        pts[2] = ImVec2(from.x + bus_at, to.y);
        pts[3] = to;
      }
      StrokeConnector(pt, pts, 4, false, col, lw);
    }
  }
  // Cross links: dashed curves that move nothing.
  ImVector<ImVec2> scratch;
  for (int e = 0; e < d->Edges.Size; ++e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    if (er.Type != ImUmlEdgeType_TreeLink || er.From < 0 || er.To < 0) {
      continue;
    }
    BeginRuns(ImUmlTextRunKind_Edge, e);
    const ImUmlNodeRec& a = d->Nodes[er.From];
    const ImUmlNodeRec& b = d->Nodes[er.To];
    const ImVec2 p0 = ImUml::ClipToShape(ImUmlShape_Box, a.Pos, a.Size, b.Pos);
    const ImVec2 p3 = ImUml::ClipToShape(ImUmlShape_Box, b.Pos, b.Size, a.Pos);
    const ImVec2 mid = (p0 + p3) * 0.5f, delta = p3 - p0;
    const ImVec2 bulge = ImVec2(-delta.y, delta.x) * 0.25f;
    const ImVec2 pts[4] = {pt.P(p0), pt.P(p0 + delta * 0.33f + bulge), pt.P(p0 + delta * 0.66f + bulge), pt.P(p3)};
    ImUml::FlattenPath(pts, 4, true, &scratch);
    const ImU32 col = er.Col != 0 ? er.Col : pt.Col(ImUmlCol_Lifeline);
    CapturePath(scratch.Data, scratch.Size, kPathPad);
    const ImVec2 tip = scratch.back();
    const ImVec2 dir = EndDirection(scratch, true, pt.S(d->Style.ArrowSize));
    TrimPath(&scratch, ImUml::MarkerLength(ImUmlMarker_Open, pt.S(d->Style.ArrowSize)), true);
    ImUml::RenderPath(pt.DL, scratch.Data, scratch.Size, col, pt.LW, true, pt.S(d->Style.DashLength),
                      pt.S(d->Style.GapLength));
    ImUml::RenderMarker(pt.DL, ImUmlMarker_Open, tip, dir, pt.S(d->Style.ArrowSize), col, pt.Col(ImUmlCol_Bg), pt.LW);
    if (!er.Label.Empty()) {
      DrawEdgeLabel(pt, er.Text, mid + bulge * 0.75f, StrBegin(d, er.Label), StrEnd(d, er.Label));
    }
  }
}

/// "+N" in the bottom-right corner of a node with collapsed descendants.
void DrawHiddenBadge(const Painter& pt, const ImUmlNodeRec& n, const ImVec2& max) {
  if (n.Hidden <= 0) {
    return;
  }
  char buf[24];
  ImFormatString(buf, IM_ARRAYSIZE(buf), "+%d", n.Hidden);
  const ImVec2 s = TextSize(n.Text, pt.Zoom * 0.8f, buf, buf + strlen(buf));
  const ImVec2 a(max.x - s.x - pt.S(8.0f), max.y - s.y * 0.5f - pt.S(2.0f));
  pt.DL->AddRectFilled(a, ImVec2(a.x + s.x + pt.S(6.0f), a.y + s.y + pt.S(2.0f)), pt.Col(ImUmlCol_Bg), pt.S(4.0f));
  pt.DL->AddRect(a, ImVec2(a.x + s.x + pt.S(6.0f), a.y + s.y + pt.S(2.0f)), pt.Col(ImUmlCol_NodeBorder), pt.S(4.0f),
                 ImDrawFlags_None, pt.LW);
  pt.DL->AddText(n.Text.Font, n.Text.Size * pt.Zoom * 0.8f, ImVec2(a.x + pt.S(3.0f), a.y + pt.S(1.0f)), n.Text.Col, buf,
                 buf + strlen(buf));
}

void DrawTreeNode(const Painter& pt, int i) {
  const ImUmlDiagram* d = pt.D;
  const ImUmlNodeRec& n = d->Nodes[i];
  BeginRuns(ImUmlTextRunKind_Node, i);
  const ImVec2 pad = d->Style.NodePadding;
  const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
  CaptureArea(a, b);
  const float cx = (a.x + b.x) * 0.5f, cy = (a.y + b.y) * 0.5f;
  const char* nb = StrBegin(d, n.Name);
  const char* ne = StrEnd(d, n.Name);
  const ImVec2 ts = TextSize(n.Text, pt.Zoom, nb, ne);
  const ImU32 fill = n.FillCol != 0 ? n.FillCol : pt.Col(ImUmlCol_NodeFill);
  switch (d->TreeMode) {
    case ImUmlTreeMode_MindMap: {
      const int depth = d->TreeDepth[i];
      const ImU32 col = BranchColor(d, i);
      if (depth <= 1) {
        const ImU32 box = n.FillCol != 0 ? n.FillCol : col;
        pt.DL->AddRectFilled(a, b, box, (b.y - a.y) * 0.5f);
        TextCentered(pt.DL, n.Text, pt.Zoom, cx, cy - ts.y * 0.5f, nb, ne, TextOn(box));
      } else {
        TextCentered(pt.DL, n.Text, pt.Zoom, cx, a.y + pt.S(pad.y * 0.5f), nb, ne, n.Text.Col);
        pt.DL->AddLine(ImVec2(a.x, b.y - pt.S(1.0f)), ImVec2(b.x, b.y - pt.S(1.0f)), col, pt.LW * 1.4f);
      }
      DrawHiddenBadge(pt, n, b);
      break;
    }
    case ImUmlTreeMode_OrgChart: {
      const bool vacant = (n.TreeFlags & ImUmlTreeFlags_Vacant) != 0;
      const char* shape = vacant ? "dashed_region" : (n.Generic.Empty() ? "rounded_rectangle" : StrBegin(d, n.Generic));
      const ImU32 border = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NodeBorder);
      if (!ImUml::RenderShape(pt.DL, shape, a, b, border, fill, pt.LW, pt.Zoom)) {
        pt.DL->AddRectFilled(a, b, fill);
        pt.DL->AddRect(a, b, border, 0.0f, ImDrawFlags_None, pt.LW);
      }
      // The content lies inside the shape's text area, centred in it.
      ImVec2 tmin = a, tmax = b;
      const ImUmlShapeArt* art = ImUml::FindShapeArt(shape);
      if (art != nullptr && art->Resizable) {
        ImUml::GetShapeTextRect(shape, a, b, pt.Zoom, &tmin, &tmax);
      }
      const float ccy = (tmin.y + tmax.y) * 0.5f;
      float x = tmin.x + pt.S(pad.x * 0.5f);
      if (!n.Image.Empty() && !vacant) {
        const float p = pt.S(kOrgPortrait);
        ImUml::RenderShape(pt.DL, StrBegin(d, n.Image), ImVec2(x, ccy - p * 0.5f), ImVec2(x + p, ccy + p * 0.5f), border,
                           fill, pt.LW, pt.Zoom);
        x += p + pt.S(pad.x);
      }
      const char* tb = StrBegin(d, n.Stereotype);
      const char* te = StrEnd(d, n.Stereotype);
      const char* db = StrBegin(d, n.Detail);
      const char* de = StrEnd(d, n.Detail);
      const ImVec2 title = TextSize(n.Text, pt.Zoom, tb, te), detail = TextSize(n.Text, pt.Zoom, db, de);
      float y = ccy - (ts.y + title.y + detail.y) * 0.5f;
      TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(x, y), nb, ne, n.Text.Col);
      y += ts.y;
      TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(x, y), tb, te, pt.Col(ImUmlCol_Lifeline));
      y += title.y;
      TextAt(pt.DL, n.Text, pt.Zoom, ImVec2(x, y), db, de, pt.Col(ImUmlCol_Lifeline));
      DrawHiddenBadge(pt, n, b);
      break;
    }
    default: {
      const ImU32 border = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NodeBorder);
      DrawShapeNodeAs(pt, n, n.Generic.Empty() ? "rounded_rectangle" : StrBegin(d, n.Generic), fill, border);
      DrawHiddenBadge(pt, n, b);
      break;
    }
  }
}

void DrawTreeDiagram(const Painter& pt) {
  const ImUmlDiagram* d = pt.D;
  DrawTreeEdges(pt);
  for (int i = 0; i < d->Nodes.Size; ++i) {
    DrawTreeNode(pt, i);
  }
}

void DrawParticipantBox(const Painter& pt, const ImUmlNodeRec& n, float x, float top) {
  const ImUmlDiagram* d = pt.D;
  const ImU32 fill = n.FillCol != 0 ? n.FillCol : pt.Col(ImUmlCol_NodeFill);
  const ImU32 border = n.LineCol != 0 ? n.LineCol : pt.Col(ImUmlCol_NodeBorder);
  const ImVec2 a = pt.P(ImVec2(x - n.Size.x * 0.5f, top)), b = pt.P(ImVec2(x + n.Size.x * 0.5f, top + n.Size.y));
  CaptureArea(a, b);
  const char* nb = StrBegin(d, n.Name);
  const char* ne = StrEnd(d, n.Name);
  const ImVec2 ts = TextSize(n.Text, pt.Zoom, nb, ne);
  const float cx = (a.x + b.x) * 0.5f;
  switch (n.Kind) {
    case ImUmlParticipantKind_Actor: {
      const float w = pt.S(22.0f), h = pt.S(36.0f);
      if (!DrawLibraryShape(pt, "actor", ImVec2(cx - w * 0.5f, a.y), ImVec2(cx + w * 0.5f, a.y + h), fill, border)) {
        ImUml::RenderActorFigure(pt.DL, ImVec2(cx - w * 0.5f, a.y), ImVec2(cx + w * 0.5f, a.y + h), border, pt.LW);
      }
      TextCentered(pt.DL, n.Text, pt.Zoom, cx, b.y - ts.y, nb, ne, n.Text.Col);
      break;
    }
    case ImUmlParticipantKind_Boundary:
    case ImUmlParticipantKind_Control:
    case ImUmlParticipantKind_Entity: {
      const char* icon = n.Kind == ImUmlParticipantKind_Boundary ? "boundary"
          : n.Kind == ImUmlParticipantKind_Entity                 ? "entity"
                                                                  : "control";
      const float iw = pt.S(36.0f), ih = pt.S(28.0f);
      if (DrawLibraryShape(pt, icon, ImVec2(cx - iw * 0.5f, a.y), ImVec2(cx + iw * 0.5f, a.y + ih), fill, border)) {
        TextCentered(pt.DL, n.Text, pt.Zoom, cx, b.y - ts.y, nb, ne, n.Text.Col);
        break;
      }
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
      if (!DrawLibraryShape(pt, "database", a, b, fill, border)) {
        ImUml::RenderCylinder(pt.DL, a, b, fill, border, pt.LW);
      }
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
    BeginRuns(ImUmlTextRunKind_Node, d->SeqParticipants[k]);
    CapturePath(pts, 2, kPathPad);
    ImUml::RenderPath(pt.DL, pts, 2, pt.Col(ImUmlCol_Lifeline), pt.LW, true, dash, gap);
  }
  BeginRuns(ImUmlTextRunKind_None, 0);
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
    BeginRuns(ImUmlTextRunKind_Event, i);
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
        CapturePath(path.Data, path.Size, kPathPad);
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
        CaptureArea(pt.P(eo.Rect.Min), pt.P(eo.Rect.Max));
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
        CaptureArea(a, b);
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
        CapturePath(pts, 2, kPathPad);
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
    BeginRuns(ImUmlTextRunKind_Node, d->SeqParticipants[k]);
    DrawParticipantBox(pt, n, p.X, p.TopY);
    if (p.BottomY >= 0.0f) {
      DrawParticipantBox(pt, n, p.X, p.BottomY);
    }
  }
}

// ── End ──────────────────────────────────────────────────────────────────────

/// The node or edge of a graph diagram at `mouse`, as drawn: plain nodes
/// over edges over composite nodes (inner composites first).
void FindHovered(const Painter& pt, int edge_count, const ImVec2& mouse, ImUmlDiagramInfo* info) {
  const ImUmlDiagram* d = pt.D;
  const auto node_at = [&](bool composite) {
    for (int i = d->Nodes.Size - 1; i >= 0; --i) {
      const ImUmlNodeRec& n = d->Nodes[i];
      if (n.Composite != composite) {
        continue;
      }
      const ImVec2 a = pt.P(n.Pos - n.Size * 0.5f), b = pt.P(n.Pos + n.Size * 0.5f);
      if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) {
        info->HoveredNode = i;
        info->HoveredNodeId = n.Id;
        info->HoveredMin = a;
        info->HoveredMax = b;
        return true;
      }
    }
    return false;
  };
  if (node_at(false)) {
    return;
  }
  const float reach = ImMax(5.0f, pt.LW * 2.0f);
  ImVector<ImVec2> screen, path;
  for (int e = ImMin(edge_count, d->Edges.Size) - 1; e >= 0; --e) {
    const ImUmlEdgeRec& er = d->Edges[e];
    const ImUmlGraphEdgeOut& eo = d->CacheEdges[e];
    if (er.From < 0 || eo.PointCount < 2) {
      continue;
    }
    screen.resize(0);
    for (int i = 0; i < eo.PointCount; ++i) {
      screen.push_back(pt.P(d->Points[eo.PointStart + i]));
    }
    ImUml::FlattenPath(screen.Data, screen.Size, eo.Bezier, &path);
    for (int i = 0; i + 1 < path.Size; ++i) {
      const ImVec2 c = ImLineClosestPoint(path[i], path[i + 1], mouse);
      if (ImLengthSqr(c - mouse) <= reach * reach) {
        info->HoveredEdge = e;
        info->HoveredEdgeFrom = er.FromId;
        info->HoveredEdgeTo = er.ToId;
        info->HoveredMin = c - ImVec2(reach, reach);
        info->HoveredMax = c + ImVec2(reach, reach);
        return;
      }
    }
  }
  node_at(true);
}

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
    d->ContentSize = kind == ImUmlDiagramKind_Sequence ? LayoutSequenceDiagram(d, ids)
        : kind == ImUmlDiagramKind_Tree                    ? LayoutTreeDiagram(d)
                                                           : LayoutGraphDiagram(d);
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
  const bool pinch = g.HasNextPinch;
  g.HasNextPinch = false;
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
  // The fitted content is centred in the space below the title.
  const auto origin_at = [&](float z) {
    return ImVec2(
        bb.Min.x + (size.x - content.x * z) * 0.5f, bb.Min.y + title_h + (size.y - title_h - content.y * z) * 0.5f);
  };
  const bool interactive = (d->Flags & ImUmlDiagramFlags_Interactive) != 0;
  bool hovered = false;
  bool held = false;
  ImVec2 pan(0.0f, 0.0f);
  const bool next_view = d->HasNextView;
  if (next_view) {
    // The content point NextCenter at the centre of the space below the
    // title: origin = centre - NextCenter * zoom, as an offset from the fit.
    d->HasNextView = false;
    d->ViewZoom = d->NextZoom;
    const float z = zoom * d->ViewZoom;
    const ImVec2 c(
        (d->NextCenter.x != d->NextCenter.x) ? content.x * 0.5f : d->NextCenter.x,
        (d->NextCenter.y != d->NextCenter.y) ? content.y * 0.5f : d->NextCenter.y);
    d->ViewPan = ImVec2((content.x * 0.5f - c.x) * z, (content.y * 0.5f - c.y) * z);
  }
  if (interactive) {
    ImGui::ButtonBehavior(bb, d->ID, &hovered, &held, ImGuiButtonFlags_MouseButtonLeft);
    ImGuiIO& io = ImGui::GetIO();
    const float fit = zoom;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      d->ViewPan = ImVec2(0, 0);
      d->ViewZoom = 1.0f;
    }
    if (hovered && io.MouseWheel != 0.0f) {
      ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, d->ID);
      // Keep the content point under the mouse where it is.
      const float old_zoom = fit * d->ViewZoom;
      const ImVec2 old_origin = origin_at(old_zoom) + d->ViewPan;
      const ImVec2 at = (io.MousePos - old_origin) / old_zoom;
      d->ViewZoom = ImClamp(d->ViewZoom * ImPow(1.2f, io.MouseWheel), 0.25f, 8.0f);
      const float new_zoom = fit * d->ViewZoom;
      d->ViewPan = io.MousePos - at * new_zoom - origin_at(new_zoom);
    }
    if (pinch) {
      // The content point under the fingers' old midpoint goes under the new one.
      const float old_zoom = fit * d->ViewZoom;
      const ImVec2 old_origin = origin_at(old_zoom) + d->ViewPan;
      const ImVec2 at = (g.NextPinchFrom - old_origin) / old_zoom;
      d->ViewZoom = ImClamp(d->ViewZoom * g.NextPinchScale, 0.25f, 8.0f);
      const float new_zoom = fit * d->ViewZoom;
      d->ViewPan = g.NextPinchTo - at * new_zoom - origin_at(new_zoom);
    }
    if (held && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      d->ViewPan += io.MouseDelta;
    }
    zoom = fit * d->ViewZoom;
    pan = d->ViewPan;
  } else if (next_view) {
    zoom *= d->ViewZoom;
    pan = d->ViewPan;
  }
  if (interactive || next_view) {
    info.ViewZoom = d->ViewZoom;
    info.ViewCenter = ImVec2(content.x * 0.5f - pan.x / zoom, content.y * 0.5f - pan.y / zoom);
  }
  info.Zoom = zoom;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  // The origin of the content, and whether it leaves the rectangle (an
  // editor shows what is outside, ImUmlDiagramFlags_ShowOutside).
  const ImVec2 fitted = origin_at(zoom);
  const ImVec2 content_min(ImFloor(fitted.x + pan.x), ImFloor(fitted.y + pan.y));
  const ImVec2 content_max = content_min + content * zoom;
  const bool outside = (d->Flags & ImUmlDiagramFlags_ShowOutside) != 0 &&
      (content_min.x < bb.Min.x - 0.5f || content_min.y < bb.Min.y + title_h - 0.5f || content_max.x > bb.Max.x + 0.5f ||
       content_max.y > bb.Max.y + 0.5f);
  const ImVec2 window_min = dl->GetClipRectMin();
  const ImVec2 window_max = dl->GetClipRectMax();
  if (outside) {
    dl->PushClipRect(window_min, window_max, false);
  } else {
    dl->PushClipRect(bb.Min, bb.Max, true);
  }
  GImUml->CaptureText = (d->Flags & ImUmlDiagramFlags_CaptureText) != 0;
  GImUml->CaptureItems = (d->Flags & ImUmlDiagramFlags_CaptureItems) != 0;
  GImUml->Areas.resize(0);
  GImUml->Runs.resize(0);
  GImUml->RunTextOffsets.resize(0);
  GImUml->RunText.resize(0);
  if (!d->Title.Empty()) {
    BeginRuns(ImUmlTextRunKind_Title, 0);
    TextCentered(dl, tts, 1.0f, (bb.Min.x + bb.Max.x) * 0.5f, bb.Min.y + 2.0f, StrBegin(d, d->Title),
                 StrEnd(d, d->Title), tts.Col);
  }
  // Content panned or zoomed away from the fit passes under the title, not
  // over it (at the fit it stays below it: images are unchanged).
  const bool moved = title_h > 0.0f && (pan.x != 0.0f || pan.y != 0.0f || d->ViewZoom != 1.0f);
  if (moved && !outside) {
    dl->PushClipRect(ImVec2(bb.Min.x, bb.Min.y + title_h), bb.Max, true);
  }
  Painter pt;
  pt.DL = dl;
  pt.D = d;
  pt.Zoom = zoom;
  pt.LW = ImMax(1.0f, d->Style.LineWeight * zoom);
  pt.Origin = content_min;
  if (kind == ImUmlDiagramKind_Sequence) {
    DrawSequenceDiagram(pt);
  } else if (kind == ImUmlDiagramKind_Tree) {
    DrawTreeDiagram(pt);
  } else {
    DrawGraphDiagram(pt);
    if (interactive && hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      FindHovered(pt, edge_count, ImGui::GetIO().MousePos, &info);
    }
  }
  if (moved && !outside) {
    dl->PopClipRect();
  }
  if (outside) {
    // Dim what the image leaves out and outline its limit; the title band
    // goes back over the content that passed under it.
    ImVec4 veil = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    veil.w = 0.65f;
    const ImU32 shade = ImGui::ColorConvertFloat4ToU32(veil);
    dl->AddRectFilled(window_min, ImVec2(window_max.x, bb.Min.y), shade);
    dl->AddRectFilled(ImVec2(window_min.x, bb.Max.y), window_max, shade);
    dl->AddRectFilled(ImVec2(window_min.x, bb.Min.y), ImVec2(bb.Min.x, bb.Max.y), shade);
    dl->AddRectFilled(ImVec2(bb.Max.x, bb.Min.y), ImVec2(window_max.x, bb.Max.y), shade);
    if (title_h > 0.0f && moved) {
      ImVec4 opaque = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
      opaque.w = 1.0f;
      dl->AddRectFilled(bb.Min, ImVec2(bb.Max.x, bb.Min.y + title_h), ImGui::ColorConvertFloat4ToU32(opaque));
      GImUml->CaptureText = false; // the title was recorded when first drawn
      TextCentered(dl, tts, 1.0f, (bb.Min.x + bb.Max.x) * 0.5f, bb.Min.y + 2.0f, StrBegin(d, d->Title),
                   StrEnd(d, d->Title), tts.Col);
    }
  }
  dl->PopClipRect();
  if (d->Flags & ImUmlDiagramFlags_ShowOutside) {
    // The image's limit, always visible while editing, so the user sees
    // whether the diagram fits inside it.
    ImVec4 edge = ImGui::GetStyle().Colors[ImGuiCol_Text];
    edge.w = 0.8f;
    // Just inside the rectangle: a root diagram's rectangle is the edge of
    // the drawing surface, which clips anything outside it.
    dl->PushClipRect(window_min, window_max, false);
    dl->AddRect(bb.Min + ImVec2(1, 1), bb.Max - ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(edge), 0.0f, 0, 2.0f);
    dl->PopClipRect();
  }
  d->Edges.resize(edge_count);
  EndRuns(&info);
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

void ImUml::EndFlowchart() {
  EndDiagram(ImUmlDiagramKind_Flowchart);
}

void ImUml::EndShapeDiagram() {
  EndDiagram(ImUmlDiagramKind_Shapes);
}

void ImUml::EndTreeDiagram() {
  EndDiagram(ImUmlDiagramKind_Tree);
}

void ImUml::EndERDiagram() {
  EndDiagram(ImUmlDiagramKind_ER);
}

void ImUml::EndDataFlowDiagram() {
  EndDiagram(ImUmlDiagramKind_DataFlow);
}

void ImUml::EndNetworkDiagram() {
  EndDiagram(ImUmlDiagramKind_Network);
}
