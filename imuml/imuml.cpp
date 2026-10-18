// MIT License © 2026 Binary Dice Games
/// @file imuml.cpp
/// @brief ImUml context, style, diagram scopes and item recording.
///
/// Items are recorded between Begin* and End* (imuml.h); the diagram
/// kinds' End* functions are in imuml_items.cpp.
#include "imuml_internal.h"

ImUmlContext* GImUml = nullptr;

// ── Style ────────────────────────────────────────────────────────────────────

ImUmlStyle::ImUmlStyle() {
  LineWeight = 1.0f;
  NodePadding = ImVec2(8.0f, 5.0f);
  NodeRounding = 6.0f;
  NodeSpacing = 30.0f;
  RankSpacing = 46.0f;
  ArrowSize = 10.0f;
  DashLength = 5.0f;
  GapLength = 3.0f;
  ParticipantSpacing = 40.0f;
  MessageSpacing = 12.0f;
  ActivationWidth = 10.0f;
  NoteFold = 8.0f;
  MaxZoom = 1.0f;
  for (int i = 0; i < ImUmlCol_COUNT; ++i) {
    Colors[i] = IMUML_AUTO_COL;
  }
  ItalicFont = nullptr;
}

namespace {

ImUmlContext& Ctx() {
  IM_ASSERT(GImUml != nullptr && "No current ImUml context: call ImUml::CreateContext()");
  return *GImUml;
}

ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
  return ImVec4(ImLerp(a.x, b.x, t), ImLerp(a.y, b.y, t), ImLerp(a.z, b.z, t), 1.0f);
}

ImVec4 Opaque(const ImVec4& c, const ImVec4& over) {
  return Mix(over, ImVec4(c.x, c.y, c.z, 1.0f), c.w);
}

/// The automatic value of colour `idx` under ImGui style `st`.
ImVec4 AutoColor(ImUmlCol idx, const ImGuiStyle& st) {
  const ImVec4 bg = Opaque(st.Colors[ImGuiCol_WindowBg], ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
  const ImVec4 text = Opaque(st.Colors[ImGuiCol_Text], bg);
  const ImVec4 accent = Opaque(st.Colors[ImGuiCol_ButtonActive], bg);
  const float light = 0.299f * bg.x + 0.587f * bg.y + 0.114f * bg.z;
  const ImVec4 yellow = light > 0.5f ? ImVec4(1.0f, 0.93f, 0.55f, 1.0f) : ImVec4(0.45f, 0.40f, 0.18f, 1.0f);
  switch (idx) {
    case ImUmlCol_Line:
      return Mix(text, bg, 0.12f);
    case ImUmlCol_NodeFill:
      return Mix(bg, accent, light > 0.5f ? 0.12f : 0.22f);
    case ImUmlCol_NodeBorder:
      return Mix(accent, text, 0.45f);
    case ImUmlCol_Text:
      return text;
    case ImUmlCol_NoteFill:
      return Mix(bg, yellow, 0.75f);
    case ImUmlCol_NoteBorder:
      return Mix(yellow, text, 0.5f);
    case ImUmlCol_FragmentBorder:
      return Mix(text, bg, 0.25f);
    case ImUmlCol_FragmentTab:
      return Mix(bg, accent, 0.2f);
    case ImUmlCol_Activation:
      return Mix(bg, accent, 0.3f);
    case ImUmlCol_Lifeline:
      return Mix(text, bg, 0.45f);
    case ImUmlCol_EdgeLabelBg:
      return bg;
    case ImUmlCol_Boundary:
      return Mix(text, bg, 0.3f);
    case ImUmlCol_Title:
      return text;
    case ImUmlCol_Bg:
      return bg;
    default:
      return text;
  }
}

bool IsAuto(const ImVec4& c) {
  return c.w < 0.0f;
}

void StyleColorsFrom(ImUmlStyle* dst, void (*imgui_theme)(ImGuiStyle*)) {
  ImGuiStyle st;
  imgui_theme(&st);
  for (int i = 0; i < ImUmlCol_COUNT; ++i) {
    dst->Colors[i] = AutoColor(i, st);
  }
}

ImUmlStyle& StyleOrCurrent(ImUmlStyle* dst) {
  return dst != nullptr ? *dst : Ctx().Style;
}

float* StyleVarFloat(ImUmlStyle& s, ImUmlStyleVar idx) {
  switch (idx) {
    case ImUmlStyleVar_LineWeight:
      return &s.LineWeight;
    case ImUmlStyleVar_NodeRounding:
      return &s.NodeRounding;
    case ImUmlStyleVar_NodeSpacing:
      return &s.NodeSpacing;
    case ImUmlStyleVar_RankSpacing:
      return &s.RankSpacing;
    case ImUmlStyleVar_ArrowSize:
      return &s.ArrowSize;
    case ImUmlStyleVar_DashLength:
      return &s.DashLength;
    case ImUmlStyleVar_GapLength:
      return &s.GapLength;
    case ImUmlStyleVar_ParticipantSpacing:
      return &s.ParticipantSpacing;
    case ImUmlStyleVar_MessageSpacing:
      return &s.MessageSpacing;
    case ImUmlStyleVar_ActivationWidth:
      return &s.ActivationWidth;
    case ImUmlStyleVar_NoteFold:
      return &s.NoteFold;
    case ImUmlStyleVar_MaxZoom:
      return &s.MaxZoom;
    default:
      return nullptr;
  }
}

ImUmlDiagram* CurrentDiagram() {
  ImUmlDiagram* d = Ctx().Current;
  IM_ASSERT(d != nullptr && "Call this between ImUml::Begin*Diagram() and End*Diagram()");
  return d;
}

} // namespace

// ── Context ──────────────────────────────────────────────────────────────────

ImUmlContext* ImUml::CreateContext() {
  ImUmlContext* ctx = IM_NEW(ImUmlContext)();
  if (GImUml == nullptr) {
    GImUml = ctx;
  }
  return ctx;
}

void ImUml::DestroyContext(ImUmlContext* ctx) {
  if (ctx == nullptr) {
    ctx = GImUml;
  }
  if (ctx == nullptr) {
    return;
  }
  for (int i = 0; i < ctx->Diagrams.Size; ++i) {
    IM_DELETE(ctx->Diagrams[i]);
  }
  if (GImUml == ctx) {
    GImUml = nullptr;
  }
  IM_DELETE(ctx);
}

ImUmlContext* ImUml::GetCurrentContext() {
  return GImUml;
}

void ImUml::SetCurrentContext(ImUmlContext* ctx) {
  GImUml = ctx;
}

// ── Style ────────────────────────────────────────────────────────────────────

ImUmlStyle& ImUml::GetStyle() {
  return Ctx().Style;
}

void ImUml::StyleColorsAuto(ImUmlStyle* dst) {
  ImUmlStyle& s = StyleOrCurrent(dst);
  for (int i = 0; i < ImUmlCol_COUNT; ++i) {
    s.Colors[i] = IMUML_AUTO_COL;
  }
}

void ImUml::StyleColorsLight(ImUmlStyle* dst) {
  StyleColorsFrom(&StyleOrCurrent(dst), [](ImGuiStyle* s) { ImGui::StyleColorsLight(s); });
}

void ImUml::StyleColorsDark(ImUmlStyle* dst) {
  StyleColorsFrom(&StyleOrCurrent(dst), [](ImGuiStyle* s) { ImGui::StyleColorsDark(s); });
}

void ImUml::StyleColorsClassic(ImUmlStyle* dst) {
  StyleColorsFrom(&StyleOrCurrent(dst), [](ImGuiStyle* s) { ImGui::StyleColorsClassic(s); });
}

void ImUml::PushStyleColor(ImUmlCol idx, ImU32 col) {
  PushStyleColor(idx, ImGui::ColorConvertU32ToFloat4(col));
}

void ImUml::PushStyleColor(ImUmlCol idx, const ImVec4& col) {
  ImUmlContext& g = Ctx();
  IM_ASSERT(idx >= 0 && idx < ImUmlCol_COUNT);
  ImUmlColorMod mod;
  mod.Col = idx;
  mod.Backup = g.Style.Colors[idx];
  g.ColorModifiers.push_back(mod);
  g.Style.Colors[idx] = col;
}

void ImUml::PopStyleColor(int count) {
  ImUmlContext& g = Ctx();
  IM_ASSERT(count <= g.ColorModifiers.Size && "PopStyleColor() called more times than PushStyleColor()");
  while (count-- > 0 && !g.ColorModifiers.empty()) {
    const ImUmlColorMod& mod = g.ColorModifiers.back();
    g.Style.Colors[mod.Col] = mod.Backup;
    g.ColorModifiers.pop_back();
  }
}

void ImUml::PushStyleVar(ImUmlStyleVar idx, float val) {
  ImUmlContext& g = Ctx();
  float* f = StyleVarFloat(g.Style, idx);
  IM_ASSERT(f != nullptr && "This style variable is not a float");
  if (f == nullptr) {
    return;
  }
  ImUmlStyleMod mod;
  mod.Var = idx;
  mod.Backup = ImVec2(*f, 0.0f);
  g.StyleModifiers.push_back(mod);
  *f = val;
}

void ImUml::PushStyleVar(ImUmlStyleVar idx, const ImVec2& val) {
  ImUmlContext& g = Ctx();
  IM_ASSERT(idx == ImUmlStyleVar_NodePadding && "This style variable is not an ImVec2");
  if (idx != ImUmlStyleVar_NodePadding) {
    return;
  }
  ImUmlStyleMod mod;
  mod.Var = idx;
  mod.Backup = g.Style.NodePadding;
  g.StyleModifiers.push_back(mod);
  g.Style.NodePadding = val;
}

void ImUml::PopStyleVar(int count) {
  ImUmlContext& g = Ctx();
  IM_ASSERT(count <= g.StyleModifiers.Size && "PopStyleVar() called more times than PushStyleVar()");
  while (count-- > 0 && !g.StyleModifiers.empty()) {
    const ImUmlStyleMod& mod = g.StyleModifiers.back();
    if (mod.Var == ImUmlStyleVar_NodePadding) {
      g.Style.NodePadding = mod.Backup;
    } else if (float* f = StyleVarFloat(g.Style, mod.Var)) {
      *f = mod.Backup.x;
    }
    g.StyleModifiers.pop_back();
  }
}

ImU32 ImUml::GetColorU32(ImUmlCol idx) {
  ImUmlContext& g = Ctx();
  if (g.Current != nullptr) {
    return g.Current->Colors[idx];
  }
  const ImVec4 c = g.Style.Colors[idx];
  return ImGui::ColorConvertFloat4ToU32(IsAuto(c) ? AutoColor(idx, ImGui::GetStyle()) : c);
}

// ── Next item ────────────────────────────────────────────────────────────────

void ImUml::SetNextItemColors(ImU32 line, ImU32 fill) {
  ImUmlContext& g = Ctx();
  g.NextLineCol = line;
  g.NextFillCol = fill;
}

void ImUml::SetNextNodeRank(const char* group) {
  ImUmlContext& g = Ctx();
  g.HasNextRank = group != nullptr && group[0] != 0;
  if (g.HasNextRank) {
    ImStrncpy(g.NextRank, group, IM_ARRAYSIZE(g.NextRank));
  }
}

void ImUml::SetNextEdgeLayout(float weight, int min_length, bool constraint) {
  ImUmlContext& g = Ctx();
  g.NextWeight = (int)ImClamp(weight + 0.5f, 0.0f, 100.0f);
  g.NextMinLen = ImMax(0, min_length);
  g.NextConstraint = constraint;
}

// ── Recording helpers (shared with imuml_items.cpp) ──────────────────────────

namespace ImUml {

ImUmlStr AddString(ImUmlDiagram* d, const char* text, const char* end) {
  ImUmlStr s;
  if (text == nullptr) {
    return s;
  }
  if (end == nullptr) {
    end = text + strlen(text);
  }
  s.Offset = d->TextBuf.Size;
  s.Len = (int)(end - text);
  d->TextBuf.resize(d->TextBuf.Size + s.Len + 1);
  memcpy(d->TextBuf.Data + s.Offset, text, (size_t)s.Len);
  d->TextBuf[s.Offset + s.Len] = 0;
  return s;
}

const char* StrBegin(const ImUmlDiagram* d, const ImUmlStr& s) {
  return s.Offset < 0 ? "" : d->TextBuf.Data + s.Offset;
}

const char* StrEnd(const ImUmlDiagram* d, const ImUmlStr& s) {
  return StrBegin(d, s) + (s.Offset < 0 ? 0 : s.Len);
}

ImUmlTextStyle CurrentTextStyle(const ImUmlDiagram* d) {
  ImUmlTextStyle ts;
  ts.Font = ImGui::GetFont();
  ts.Size = ImGui::GetFontSize();
  ts.Col = d->TextColorAuto ? ImGui::GetColorU32(ImGuiCol_Text) : d->Colors[ImUmlCol_Text];
  return ts;
}

ImUmlDiagram* GetCurrentDiagram() {
  return CurrentDiagram();
}

/// A new node of the current diagram, every field zeroed (so the layout
/// hash sees only what was submitted), with the next-item settings applied.
ImUmlNodeRec& AddNode(ImUmlDiagram* d, int type, int kind, const char* id, const char* name) {
  ImUmlContext& g = Ctx();
  d->SetupLocked = true;
  ImUmlNodeRec rec;
  memset((void*)&rec, 0, sizeof(rec));
  rec.Id = ImHashStr(id != nullptr ? id : "");
  rec.Type = type;
  rec.Kind = kind;
  rec.Name = AddString(d, name != nullptr ? name : (id != nullptr ? id : ""), nullptr);
  rec.Stereotype = ImUmlStr();
  rec.Generic = ImUmlStr();
  rec.FirstLine = d->Lines.Size;
  rec.Parent = d->OpenStates.empty() ? -1 : d->OpenStates.back();
  rec.RankGroup = -1;
  if (g.HasNextRank) {
    const ImGuiID key = ImHashStr(g.NextRank);
    int index = d->RankGroups.GetInt(key, 0);
    if (index == 0) {
      index = d->RankGroups.Data.Size + 1;
      d->RankGroups.SetInt(key, index);
    }
    rec.RankGroup = index - 1;
  }
  rec.LineCol = g.NextLineCol;
  rec.FillCol = g.NextFillCol;
  rec.Text = CurrentTextStyle(d);
  g.HasNextRank = false;
  g.NextLineCol = g.NextFillCol = 0;
  d->Nodes.push_back(rec);
  return d->Nodes.back();
}

ImUmlEdgeRec& AddEdge(ImUmlDiagram* d, int type, int kind, const char* from, const char* to, const char* label) {
  ImUmlContext& g = Ctx();
  d->SetupLocked = true;
  ImUmlEdgeRec rec;
  memset((void*)&rec, 0, sizeof(rec));
  rec.Type = type;
  rec.Kind = kind;
  rec.FromId = ImHashStr(from != nullptr ? from : "");
  rec.ToId = ImHashStr(to != nullptr ? to : "");
  rec.Label = label != nullptr && label[0] != 0 ? AddString(d, label, nullptr) : ImUmlStr();
  rec.FromLabel = ImUmlStr();
  rec.ToLabel = ImUmlStr();
  rec.Col = g.NextLineCol;
  rec.Text = CurrentTextStyle(d);
  rec.Weight = g.NextWeight;
  rec.MinLen = g.NextMinLen;
  rec.Constraint = g.NextConstraint;
  rec.From = rec.To = -1;
  g.NextLineCol = g.NextFillCol = 0;
  g.NextWeight = 1;
  g.NextMinLen = 1;
  g.NextConstraint = true;
  d->Edges.push_back(rec);
  return d->Edges.back();
}

} // namespace ImUml

// ── Diagram scopes ───────────────────────────────────────────────────────────

namespace ImUml {

bool BeginDiagram(int kind, const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  ImUmlContext& g = Ctx();
  IM_ASSERT(g.Current == nullptr && "Mismatched ImUml Begin*/End*: a diagram is already open");
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems && !(flags & ImUmlDiagramFlags_MeasureOnly)) {
    return false;
  }
  const ImGuiID id = window->GetID(title_id);
  const int slot = g.DiagramMap.GetInt(id, 0);
  ImUmlDiagram* d;
  if (slot == 0) {
    d = IM_NEW(ImUmlDiagram)();
    g.Diagrams.push_back(d);
    g.DiagramMap.SetInt(id, g.Diagrams.Size);
  } else {
    d = g.Diagrams[slot - 1];
  }
  d->ID = id;
  d->Kind = kind;
  d->Flags = flags;
  // A measuring pass leaves the next view to the drawing one.
  if (g.HasNextView && !(flags & ImUmlDiagramFlags_MeasureOnly)) {
    d->HasNextView = true;
    d->NextZoom = g.NextZoom;
    d->NextCenter = g.NextCenter;
    g.HasNextView = false;
  }
  d->SizeRequest = size;
  d->CursorPos = ImGui::GetCursorScreenPos();
  d->TextBuf.resize(0);
  d->Nodes.resize(0);
  d->Lines.resize(0);
  d->Edges.resize(0);
  d->AttachIds.resize(0);
  d->Events.resize(0);
  d->OpenStates.resize(0);
  d->OpenClass = -1;
  d->OpenFragments = 0;
  d->RankGroups.Clear();
  d->SkippedItems = 0;
  d->SetupLocked = false;
  d->Style = g.Style;
  const ImGuiStyle& st = ImGui::GetStyle();
  for (int i = 0; i < ImUmlCol_COUNT; ++i) {
    const ImVec4 c = g.Style.Colors[i];
    d->Colors[i] = ImGui::ColorConvertFloat4ToU32(IsAuto(c) ? AutoColor(i, st) : c);
  }
  d->TextColorAuto = IsAuto(g.Style.Colors[ImUmlCol_Text]);
  const char* title_end = ImGui::FindRenderedTextEnd(title_id);
  d->Title = (flags & ImUmlDiagramFlags_NoTitle) || title_end == title_id ? ImUmlStr()
                                                                            : AddString(d, title_id, title_end);
  d->TitleStyle = CurrentTextStyle(d);
  if (IsAuto(g.Style.Colors[ImUmlCol_Title])) {
    d->TitleStyle.Col = ImGui::GetColorU32(ImGuiCol_Text);
  } else {
    d->TitleStyle.Col = d->Colors[ImUmlCol_Title];
  }
  // Use case and data flow diagrams read left to right; the rest top down.
  d->Direction = kind == ImUmlDiagramKind_UseCase || kind == ImUmlDiagramKind_DataFlow ? ImUmlDirection_LeftRight
                                                                                         : ImUmlDirection_TopDown;
  d->EdgeStyle = ImUmlEdgeStyle_Spline;
  d->NodeSpacing = kind == ImUmlDiagramKind_Sequence ? g.Style.ParticipantSpacing : g.Style.NodeSpacing;
  d->RankSpacing = g.Style.RankSpacing;
  d->SequenceFlags = ImUmlSequenceFlags_MirrorParticipants;
  d->SystemLabel = ImUmlStr();
  g.Current = d;
  return true;
}

} // namespace ImUml

bool ImUml::BeginClassDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_Class, title_id, size, flags);
}

bool ImUml::BeginSequenceDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_Sequence, title_id, size, flags);
}

bool ImUml::BeginStateDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_State, title_id, size, flags);
}

bool ImUml::BeginActivityDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_Activity, title_id, size, flags);
}

bool ImUml::BeginUseCaseDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_UseCase, title_id, size, flags);
}

bool ImUml::BeginFlowchart(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_Flowchart, title_id, size, flags);
}

// ── Setup ────────────────────────────────────────────────────────────────────

void ImUml::SetupLayout(ImUmlDirection direction, ImUmlEdgeStyle edge_style) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(!d->SetupLocked && "Setup*() must be called before the first item");
  d->Direction = direction;
  d->EdgeStyle = edge_style;
}

void ImUml::SetupSpacing(float node_spacing, float rank_spacing) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(!d->SetupLocked && "Setup*() must be called before the first item");
  if (node_spacing > 0.0f) {
    d->NodeSpacing = node_spacing;
  }
  if (rank_spacing > 0.0f) {
    d->RankSpacing = rank_spacing;
  }
}

void ImUml::SetupSequence(ImUmlSequenceFlags flags) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Sequence && !d->SetupLocked);
  d->SequenceFlags = flags;
}

void ImUml::SetupSystemBoundary(const char* label) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_UseCase && !d->SetupLocked);
  d->SystemLabel = label != nullptr && label[0] != 0 ? AddString(d, label, nullptr) : ImUmlStr();
}

const ImUmlDiagramInfo& ImUml::GetLastDiagramInfo() {
  return Ctx().LastInfo;
}

void ImUml::SetNextDiagramView(float zoom, const ImVec2& center) {
  ImUmlContext& g = Ctx();
  g.HasNextView = true;
  g.NextZoom = ImClamp(zoom, 0.25f, 8.0f);
  g.NextCenter = center;
}

// ── Items ────────────────────────────────────────────────────────────────────

namespace {

/// A member line; strips mermaid's `$` (static) and `*` (abstract) suffix.
void AddMember(ImUmlDiagram* d, const char* text, int section) {
  IM_ASSERT(d->OpenClass >= 0 && "ClassAttribute()/ClassMethod() must be between BeginClass() and EndClass()");
  if (d->OpenClass < 0) {
    return;
  }
  ImUmlLine line;
  memset((void*)&line, 0, sizeof(line));
  const char* end = text + strlen(text);
  if (end > text && (end[-1] == '$' || end[-1] == '*')) {
    line.Static = end[-1] == '$';
    line.Abstract = end[-1] == '*';
    --end;
  }
  line.Text = ImUml::AddString(d, text, end);
  line.Section = section;
  d->Lines.push_back(line);
  d->Nodes[d->OpenClass].LineCount++;
}

} // namespace

void ImUml::BeginClass(
    const char* id,
    const char* name,
    ImUmlClassKind kind,
    const char* stereotype,
    const char* generic) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Class && d->OpenClass < 0);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Class, kind, id, name);
  if (stereotype != nullptr && stereotype[0] != 0) {
    n.Stereotype = AddString(d, stereotype, nullptr);
  }
  if (generic != nullptr && generic[0] != 0) {
    n.Generic = AddString(d, generic, nullptr);
  }
  n.Shape = ImUmlShape_Box;
  d->OpenClass = d->Nodes.Size - 1;
}

void ImUml::ClassAttribute(const char* text) {
  AddMember(CurrentDiagram(), text, 0);
}

void ImUml::ClassMethod(const char* text) {
  AddMember(CurrentDiagram(), text, 1);
}

void ImUml::EndClass() {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->OpenClass >= 0 && "EndClass() without BeginClass()");
  d->OpenClass = -1;
}

void ImUml::Class(const char* id, const char* name, ImUmlClassKind kind) {
  BeginClass(id, name, kind);
  EndClass();
}

void ImUml::Relation(
    const char* from,
    const char* to,
    ImUmlRelationKind kind,
    const char* label,
    const char* from_label,
    const char* to_label) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Class);
  ImUmlEdgeRec& e = AddEdge(d, ImUmlEdgeType_Relation, kind, from, to, label);
  if (from_label != nullptr && from_label[0] != 0) {
    e.FromLabel = AddString(d, from_label, nullptr);
  }
  if (to_label != nullptr && to_label[0] != 0) {
    e.ToLabel = AddString(d, to_label, nullptr);
  }
}

void ImUml::Note(const char* text, const char* const* attach_ids, int attach_count) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind != ImUmlDiagramKind_Sequence && "Use NoteOver()/NoteSide() in sequence diagrams");
  char id[32];
  ImFormatString(id, IM_ARRAYSIZE(id), "##note%d", d->Nodes.Size);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Note, 0, id, text != nullptr ? text : "");
  n.FirstAttach = d->AttachIds.Size;
  n.AttachCount = ImMax(0, attach_count);
  for (int i = 0; i < attach_count; ++i) {
    d->AttachIds.push_back(ImHashStr(attach_ids[i] != nullptr ? attach_ids[i] : ""));
  }
}

void ImUml::Participant(const char* id, const char* name, ImUmlParticipantKind kind, ImUmlParticipantFlags flags) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Sequence);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Participant, kind, id, name);
  n.Side = flags;
}

namespace {

ImUmlSeqEventRec& AddEvent(ImUmlDiagram* d, int type, const char* a, const char* b, const char* text) {
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Sequence);
  ImUmlContext& g = Ctx();
  d->SetupLocked = true;
  ImUmlSeqEventRec ev;
  memset((void*)&ev, 0, sizeof(ev));
  ev.Type = type;
  ev.A = a != nullptr ? ImHashStr(a) : 0;
  ev.B = b != nullptr ? ImHashStr(b) : 0;
  ev.Text = text != nullptr ? ImUml::AddString(d, text, nullptr) : ImUmlStr();
  ev.Col = g.NextLineCol;
  ev.TextStyle = ImUml::CurrentTextStyle(d);
  g.NextLineCol = g.NextFillCol = 0;
  d->Events.push_back(ev);
  return d->Events.back();
}

} // namespace

void ImUml::Message(const char* from, const char* to, const char* text, ImUmlMessageKind kind, ImUmlMessageFlags flags) {
  ImUmlSeqEventRec& ev = AddEvent(CurrentDiagram(), ImUmlSeqEventType_Message, from, to, text);
  ev.Kind = kind;
  ev.Flags = flags;
}

void ImUml::BeginFragment(ImUmlFragmentKind kind, const char* label, const char* over_from, const char* over_to) {
  ImUmlDiagram* d = CurrentDiagram();
  ImUmlSeqEventRec& ev = AddEvent(d, ImUmlSeqEventType_FragmentBegin, over_from, over_to, label);
  ev.Kind = kind;
  ev.Flags = over_from != nullptr && over_to != nullptr ? 1 : 0;
  d->OpenFragments++;
}

void ImUml::FragmentElse(const char* label) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->OpenFragments > 0 && "FragmentElse() outside a fragment");
  AddEvent(d, ImUmlSeqEventType_FragmentElse, nullptr, nullptr, label);
}

void ImUml::EndFragment() {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->OpenFragments > 0 && "EndFragment() without BeginFragment()");
  if (d->OpenFragments <= 0) {
    return;
  }
  d->OpenFragments--;
  AddEvent(d, ImUmlSeqEventType_FragmentEnd, nullptr, nullptr, nullptr);
}

void ImUml::NoteOver(const char* text, const char* a, const char* b) {
  AddEvent(CurrentDiagram(), ImUmlSeqEventType_NoteOver, a, b != nullptr ? b : a, text);
}

void ImUml::NoteSide(const char* text, const char* id, ImUmlSide side) {
  ImUmlSeqEventRec& ev = AddEvent(CurrentDiagram(), ImUmlSeqEventType_NoteSide, id, nullptr, text);
  ev.Side = side;
}

void ImUml::BeginState(const char* id, const char* name, ImUmlStateKind kind) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_State);
  AddNode(d, ImUmlNodeType_State, kind, id, name);
  d->OpenStates.push_back(d->Nodes.Size - 1);
}

void ImUml::StateActivity(const char* text) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(!d->OpenStates.empty() && "StateActivity() must be between BeginState() and EndState()");
  if (d->OpenStates.empty()) {
    return;
  }
  ImUmlNodeRec& n = d->Nodes[d->OpenStates.back()];
  IM_ASSERT(n.FirstLine + n.LineCount == d->Lines.Size && "Submit a state's activities before its children");
  ImUmlLine line;
  memset((void*)&line, 0, sizeof(line));
  line.Text = AddString(d, text, nullptr);
  d->Lines.push_back(line);
  n.LineCount++;
}

void ImUml::EndState() {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(!d->OpenStates.empty() && "EndState() without BeginState()");
  if (!d->OpenStates.empty()) {
    d->OpenStates.pop_back();
  }
}

void ImUml::State(const char* id, const char* name, ImUmlStateKind kind) {
  BeginState(id, name, kind);
  EndState();
}

void ImUml::Transition(const char* from, const char* to, const char* label) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_State);
  AddEdge(d, ImUmlEdgeType_Transition, 0, from, to, label);
}

void ImUml::Action(const char* id, const char* text, ImUmlActionKind kind) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Activity);
  AddNode(d, ImUmlNodeType_Action, kind, id, text);
}

void ImUml::ControlNode(const char* id, ImUmlControlKind kind, const char* label) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Activity);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Control, kind, id, label != nullptr ? label : "");
  (void)n;
}

void ImUml::Flow(const char* from, const char* to, const char* guard) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Activity);
  char buf[512];
  const char* label = nullptr;
  if (guard != nullptr && guard[0] != 0) {
    ImFormatString(buf, IM_ARRAYSIZE(buf), "[%s]", guard);
    label = buf;
  }
  AddEdge(d, ImUmlEdgeType_Flow, 0, from, to, label);
}

void ImUml::Actor(const char* id, const char* name, ImUmlSide side) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_UseCase);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Actor, 0, id, name);
  n.Side = side;
}

void ImUml::UseCase(const char* id, const char* name) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_UseCase);
  AddNode(d, ImUmlNodeType_UseCase, 0, id, name);
}

void ImUml::UseCaseLink(const char* from, const char* to, ImUmlUseCaseLinkKind kind) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_UseCase);
  const char* label = kind == ImUmlUseCaseLinkKind_Include ? "\xC2\xABinclude\xC2\xBB"
                      : kind == ImUmlUseCaseLinkKind_Extend ? "\xC2\xAB" "extend\xC2\xBB"
                                                             : nullptr;
  AddEdge(d, ImUmlEdgeType_UseCaseLink, kind, from, to, label);
}

// ── Flowcharts ───────────────────────────────────────────────────────────────

void ImUml::FlowNode(const char* id, const char* text, ImUmlFlowShape shape) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Flowchart);
  AddNode(d, ImUmlNodeType_FlowNode, shape, id, text);
}

void ImUml::BeginFlowGroup(const char* id, const char* title) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Flowchart);
  AddNode(d, ImUmlNodeType_FlowGroup, 0, id, title);
  d->OpenStates.push_back(d->Nodes.Size - 1); // the parent of what follows, as BeginState()
}

void ImUml::EndFlowGroup() {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(!d->OpenStates.empty() && "EndFlowGroup() without BeginFlowGroup()");
  if (!d->OpenStates.empty()) {
    d->OpenStates.pop_back();
  }
}

void ImUml::FlowLink(const char* from, const char* to, const char* label, ImUmlFlowLinkFlags flags) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Flowchart);
  AddEdge(d, ImUmlEdgeType_FlowLink, flags, from, to, label);
}

// ── ER diagrams ──────────────────────────────────────────────────────────────

bool ImUml::BeginERDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_ER, title_id, size, flags);
}

void ImUml::BeginEntity(const char* id, const char* name) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_ER && d->OpenClass < 0);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Entity, 0, id, name);
  n.Shape = ImUmlShape_Box;
  d->OpenClass = d->Nodes.Size - 1;
}

void ImUml::EntityAttribute(const char* type, const char* name, ImUmlAttributeKeys keys, const char* comment) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->OpenClass >= 0 && "EntityAttribute() must be between BeginEntity() and EndEntity()");
  if (d->OpenClass < 0) {
    return;
  }
  ImUmlLine line;
  memset((void*)&line, 0, sizeof(line));
  line.Text = AddString(d, name != nullptr ? name : "", nullptr);
  line.Type = AddString(d, type != nullptr ? type : "", nullptr);
  line.Comment = comment != nullptr && comment[0] != 0 ? AddString(d, comment, nullptr) : ImUmlStr();
  line.Keys = keys;
  d->Lines.push_back(line);
  d->Nodes[d->OpenClass].LineCount++;
}

void ImUml::EndEntity() {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->OpenClass >= 0 && "EndEntity() without BeginEntity()");
  d->OpenClass = -1;
}

void ImUml::Entity(const char* id, const char* name) {
  BeginEntity(id, name);
  EndEntity();
}

void ImUml::Relationship(
    const char* from,
    const char* to,
    ImUmlCardinality from_cardinality,
    ImUmlCardinality to_cardinality,
    const char* label,
    ImUmlRelationshipFlags flags) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_ER);
  // Kind packs both cardinalities and the flags: from | to << 4 | flags << 8.
  const int kind = (from_cardinality & 15) | (to_cardinality & 15) << 4 | (flags & 255) << 8;
  AddEdge(d, ImUmlEdgeType_Relationship, kind, from, to, label);
}

// ── Groups, data flow and network diagrams ───────────────────────────────────

bool ImUml::BeginDataFlowDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_DataFlow, title_id, size, flags);
}

bool ImUml::BeginNetworkDiagram(const char* title_id, const ImVec2& size, ImUmlDiagramFlags flags) {
  return BeginDiagram(ImUmlDiagramKind_Network, title_id, size, flags);
}

void ImUml::BeginGroup(const char* id, const char* label) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_DataFlow || d->Kind == ImUmlDiagramKind_Network);
  AddNode(d, ImUmlNodeType_Group, 0, id, label);
  d->OpenStates.push_back(d->Nodes.Size - 1);
}

void ImUml::EndGroup() {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(!d->OpenStates.empty() && "EndGroup() without BeginGroup()");
  if (!d->OpenStates.empty()) {
    d->OpenStates.pop_back();
  }
}

namespace {

void DataFlowNode(int type, const char* id, const char* name, const char* number) {
  ImUmlDiagram* d = ImUml::GetCurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_DataFlow);
  ImUmlNodeRec& n = ImUml::AddNode(d, type, 0, id, name);
  if (number != nullptr && number[0] != 0) {
    n.Stereotype = ImUml::AddString(d, number, nullptr);
  }
}

} // namespace

void ImUml::ExternalEntity(const char* id, const char* name) {
  DataFlowNode(ImUmlNodeType_ExternalEntity, id, name, nullptr);
}

void ImUml::Process(const char* id, const char* name, const char* number) {
  DataFlowNode(ImUmlNodeType_Process, id, name, number);
}

void ImUml::DataStore(const char* id, const char* name, const char* number) {
  DataFlowNode(ImUmlNodeType_DataStore, id, name, number);
}

void ImUml::DataFlow(const char* from, const char* to, const char* label, ImUmlDataFlowFlags flags) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_DataFlow);
  AddEdge(d, ImUmlEdgeType_DataFlow, flags, from, to, label);
}

void ImUml::Device(const char* id, const char* name, ImUmlDeviceKind kind, const char* address) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Network);
  ImUmlNodeRec& n = AddNode(d, ImUmlNodeType_Device, kind, id, name);
  if (address != nullptr && address[0] != 0) {
    n.Stereotype = AddString(d, address, nullptr);
  }
}

void ImUml::Link(
    const char* from,
    const char* to,
    ImUmlLinkKind kind,
    const char* label,
    const char* from_label,
    const char* to_label,
    ImUmlLinkFlags flags) {
  ImUmlDiagram* d = CurrentDiagram();
  IM_ASSERT(d->Kind == ImUmlDiagramKind_Network);
  // Kind packs the link kind and its flags: kind | flags << 8.
  ImUmlEdgeRec& e = AddEdge(d, ImUmlEdgeType_Link, (kind & 255) | (flags & 255) << 8, from, to, label);
  if (from_label != nullptr && from_label[0] != 0) {
    e.FromLabel = AddString(d, from_label, nullptr);
  }
  if (to_label != nullptr && to_label[0] != 0) {
    e.ToLabel = AddString(d, to_label, nullptr);
  }
}
