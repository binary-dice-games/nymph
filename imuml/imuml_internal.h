// MIT License © 2026 Binary Dice Games
/// @file imuml_internal.h
/// @brief ImUml internals: recorded items, the layout engines (pure
///        functions, testable without an ImGui context) and the rendering
///        primitives. Not a stable API.
#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"
#include "imuml.h"

// ── Graph layout (imuml_graph_layout.cpp) ────────────────────────────────────
// dot's four phases (Gansner, Koutsofios, North, Vo, "A Technique for
// Drawing Directed Graphs", 1993): rank, order, position, route. Every
// coordinate is a whole number; the result depends only on the input.

enum ImUmlShape_ {
  ImUmlShape_Box,
  ImUmlShape_Ellipse,
  ImUmlShape_Circle,
  ImUmlShape_Diamond,
};
typedef int ImUmlShape;

struct ImUmlGraphNode {
  ImVec2 Size; ///< Whole pixels, in the final orientation.
  ImUmlShape Shape;
  int RankGroup; ///< Nodes with the same value >= 0 share a rank.
  int Pin; ///< 0 none, 1 first rank, 2 last rank.
  int Cluster; ///< The innermost cluster containing the node, or -1.
  ImUmlGraphNode() {
    Size = ImVec2(0, 0);
    Shape = ImUmlShape_Box;
    RankGroup = -1;
    Pin = 0;
    Cluster = -1;
  }
};

/// @brief A cluster (dot's subgraph cluster): a box around its nodes and
///        nested clusters, laid out as part of the whole graph and kept
///        together in every rank, so edges from outside are routed around it.
struct ImUmlGraphCluster {
  int Parent; ///< Enclosing cluster, or -1. Parents come before children.
  ImVec2 HeaderSize; ///< The title band, in the output orientation (drawn at the top).
  float Padding; ///< Between the box and its contents.
  ImUmlGraphCluster() {
    Parent = -1;
    HeaderSize = ImVec2(0, 0);
    Padding = 10.0f;
  }
};

struct ImUmlGraphEdge {
  int From, To; ///< Node indices; From == To is a self-loop.
  int Weight; ///< 0-100 (dot's weight).
  int MinLen; ///< dot's minlen, >= 0.
  bool Constraint; ///< false: drawn but not ranked.
  ImVec2 LabelSize; ///< (0, 0) for none.
  int FromCluster, ToCluster; ///< >= 0: the end is that cluster's box (From / To ignored).
  ImUmlGraphEdge() {
    From = To = 0;
    FromCluster = ToCluster = -1;
    Weight = 1;
    MinLen = 1;
    Constraint = true;
    LabelSize = ImVec2(0, 0);
  }
};

struct ImUmlGraphInput {
  ImVector<ImUmlGraphNode> Nodes;
  ImVector<ImUmlGraphEdge> Edges;
  ImVector<ImUmlGraphCluster> Clusters;
  ImUmlDirection Direction;
  ImUmlEdgeStyle EdgeStyle;
  float NodeSpacing;
  float RankSpacing;
  int MaxIterations; ///< Network simplex pivots per solve (dot's nslimit).
  ImUmlGraphInput() {
    Direction = ImUmlDirection_TopDown;
    EdgeStyle = ImUmlEdgeStyle_Spline;
    NodeSpacing = 30.0f;
    RankSpacing = 50.0f;
    MaxIterations = 4000;
  }
};

struct ImUmlGraphEdgeOut {
  int PointStart, PointCount; ///< Into ImUmlGraphOutput::Points, from the edge's From to its To.
  bool Bezier; ///< Points are p0, c1, c2, p1, c1, c2, p2, ... (3n + 1); else a polyline.
  ImVec2 LabelPos; ///< Centre of the label.
};

struct ImUmlGraphOutput {
  ImVector<ImVec2> NodePos; ///< Centres.
  ImVector<int> NodeRank;
  ImVector<int> NodeOrder; ///< Position within its rank.
  ImVector<ImUmlGraphEdgeOut> Edges;
  ImVector<ImVec2> Points;
  ImVector<ImRect> ClusterRects; ///< One box per cluster, the header band at its top.
  ImVec2 Size;
  int Crossings; ///< Edge crossings between ranks after ordering (with clusters, edge-border crossings count too).
  int ReversedEdges; ///< Edges reversed to break cycles.
  bool HitIterationCap;
};

/// @brief One network simplex problem: minimise the sum of
///        Weight * (Rank[To] - Rank[From]) subject to
///        Rank[To] - Rank[From] >= MinLen. The graph must be acyclic.
struct ImUmlNsEdge {
  int From, To;
  int MinLen;
  int Weight;
};

namespace ImUml {
/// @brief Solves the problem for `node_count` nodes; `ranks` receives one
///        rank per node, the smallest 0. Disconnected parts are solved
///        independently. Deterministic: every choice follows edge order.
/// @return false if `max_iterations` pivots were reached (the ranks are
///         still feasible).
bool NetworkSimplex(int node_count, const ImVector<ImUmlNsEdge>& edges, ImVector<int>* ranks, int max_iterations);
/// @brief dot's phase 1 alone: ranks of the input's nodes (smallest 0),
///        and which edges were reversed to break cycles.
bool RankGraph(const ImUmlGraphInput& in, ImVector<int>* ranks, ImVector<bool>* reversed);
/// @brief The whole layout.
void LayoutGraph(const ImUmlGraphInput& in, ImUmlGraphOutput* out);
/// @brief A smooth cubic Bézier chain (p0, c1, c2, p1, ...) through the
///        polyline `pts` (at least 2 points): a monotone cubic of the rank
///        coordinate (y when `along_y`, else x), so it never swings past its
///        points sideways; control points rounded to whole pixels.
void SmoothPath(const ImVector<ImVec2>& pts, ImVector<ImVec2>* out, bool along_y);
/// @brief The point where the segment from `centre` towards `towards`
///        leaves a node of `shape` and `size` centred at `centre`.
ImVec2 ClipToShape(ImUmlShape shape, const ImVec2& centre, const ImVec2& size, const ImVec2& towards);
} // namespace ImUml

// ── Tree layout (imuml_tree_layout.cpp) ──────────────────────────────────────

struct ImUmlTreeNodeIn {
  ImVec2 Size; ///< Whole pixels.
  int Parent; ///< Index of the parent (always before the node), or -1 for the root.
  int Side; ///< Mind map, root children: ImUmlSide_Left / Right, else -1.
  bool Hanging; ///< Its descendants hang below it in an indented list.
  bool Staff; ///< An assistant, placed beside its parent.
  ImUmlTreeNodeIn() {
    Size = ImVec2(0, 0);
    Parent = -1;
    Side = -1;
    Hanging = Staff = false;
  }
};

struct ImUmlTreeInput {
  ImVector<ImUmlTreeNodeIn> Nodes; ///< The root is node 0; parents come before children.
  ImUmlTreeMode Mode;
  ImUmlDirection Direction;
  float NodeSpacing; ///< Between sibling subtrees.
  float RankSpacing; ///< Between levels.
  float Indent; ///< Hanging lists: the indent per level.
  float StaffGap; ///< Between a node and its assistants.
  ImUmlTreeInput() {
    Mode = ImUmlTreeMode_Tidy;
    Direction = ImUmlDirection_TopDown;
    NodeSpacing = 20.0f;
    RankSpacing = 40.0f;
    Indent = 24.0f;
    StaffGap = 24.0f;
  }
};

struct ImUmlTreeOutput {
  ImVector<ImVec2> Pos; ///< Centres.
  ImVector<int> Depth;
  ImVector<int> Branch; ///< Mind map: the index of the root's child the node descends from.
  ImVector<unsigned char> OnRight; ///< Mind map: on the root's right (else left).
  ImVector<int> HangRoot; ///< Members of a hanging list: the node it hangs from, else -1.
  ImVec2 Size;
};

namespace ImUml {
/// @brief Lays out a tree: parents centred over their children, subtrees
///        packed by contour, siblings in written order (DESIGN.md section 4,
///        "Tree layout"). Coordinates of node top-lefts are whole pixels.
void LayoutTree(const ImUmlTreeInput& in, ImUmlTreeOutput* out);
} // namespace ImUml

// ── Sequence layout (imuml_sequence_layout.cpp) ──────────────────────────────

enum ImUmlSeqEventType_ {
  ImUmlSeqEventType_Message,
  ImUmlSeqEventType_NoteOver,
  ImUmlSeqEventType_NoteSide,
  ImUmlSeqEventType_FragmentBegin,
  ImUmlSeqEventType_FragmentElse,
  ImUmlSeqEventType_FragmentEnd,
};

struct ImUmlSeqParticipantIn {
  ImVec2 BoxSize;
  bool Created;
};

struct ImUmlSeqEventIn {
  int Type;
  int A, B; ///< Participants: message from / to, note over A..B (B may equal A), side note A, fragment over A..B (-1 auto).
  int Kind; ///< Message kind, fragment kind.
  int Flags; ///< Message flags.
  int Side; ///< NoteSide.
  ImVec2 TextSize; ///< Message text, note box, fragment label, else label.
};

struct ImUmlSeqInput {
  ImVector<ImUmlSeqParticipantIn> Participants;
  ImVector<ImUmlSeqEventIn> Events;
  float ParticipantSpacing;
  float MessageSpacing;
  float ActivationWidth;
  float SelfLoopWidth;
  bool Mirror;
  ImUmlSeqInput() {
    ParticipantSpacing = 40.0f;
    MessageSpacing = 14.0f;
    ActivationWidth = 10.0f;
    SelfLoopWidth = 30.0f;
    Mirror = true;
  }
};

struct ImUmlSeqParticipantOut {
  float X; ///< Centre of the lifeline.
  float TopY; ///< Top of the head box.
  float LifelineStart, LifelineEnd;
  float BottomY; ///< Top of the mirrored box, or -1.
  bool Destroyed;
};

struct ImUmlSeqEventOut {
  float Y; ///< Messages: the arrow (for self messages, where it leaves).
  float Y2; ///< Self messages: where it returns.
  float X1, X2; ///< Messages: arrow start and end (after activation offsets).
  ImRect Rect; ///< Notes, fragments (frame), else separators (the line's extent).
  ImVec2 TextPos; ///< Top-left of the text.
  int Number; ///< 1-based message number.
};

struct ImUmlSeqActivation {
  int Participant;
  ImRect Rect;
};

struct ImUmlSeqOutput {
  ImVector<ImUmlSeqParticipantOut> Participants;
  ImVector<ImUmlSeqEventOut> Events;
  ImVector<ImUmlSeqActivation> Activations;
  ImVec2 Size;
};

namespace ImUml {
/// @brief Places participants in columns and events in rows (DESIGN.md
///        section 4, "Sequence layout"). Coordinates are whole pixels.
void LayoutSequence(const ImUmlSeqInput& in, ImUmlSeqOutput* out);
} // namespace ImUml

// ── Recorded items (imuml.cpp) ───────────────────────────────────────────────

enum ImUmlDiagramKind_ {
  ImUmlDiagramKind_Class,
  ImUmlDiagramKind_Sequence,
  ImUmlDiagramKind_State,
  ImUmlDiagramKind_Activity,
  ImUmlDiagramKind_UseCase,
  ImUmlDiagramKind_Flowchart,
  ImUmlDiagramKind_ER,
  ImUmlDiagramKind_DataFlow,
  ImUmlDiagramKind_Network,
  ImUmlDiagramKind_Shapes,
  ImUmlDiagramKind_Tree,
};

enum ImUmlNodeType_ {
  ImUmlNodeType_Class,
  ImUmlNodeType_State,
  ImUmlNodeType_Action,
  ImUmlNodeType_Control,
  ImUmlNodeType_Actor,
  ImUmlNodeType_UseCase,
  ImUmlNodeType_Entity,
  ImUmlNodeType_Group,
  ImUmlNodeType_ExternalEntity,
  ImUmlNodeType_Process,
  ImUmlNodeType_DataStore,
  ImUmlNodeType_Device,
  ImUmlNodeType_Note,
  ImUmlNodeType_Participant,
  ImUmlNodeType_FlowNode, ///< Kind: ImUmlFlowShape_.
  ImUmlNodeType_FlowGroup, ///< Composite (a cluster) when it has children.
  ImUmlNodeType_ShapeNode, ///< Shape name in Generic, stereotype in Stereotype.
  ImUmlNodeType_ShapeGroup, ///< Composite (a cluster) when it has children.
  ImUmlNodeType_TreeNode,
};

enum ImUmlEdgeType_ {
  ImUmlEdgeType_Relation,
  ImUmlEdgeType_Transition,
  ImUmlEdgeType_Flow,
  ImUmlEdgeType_UseCaseLink,
  ImUmlEdgeType_Relationship,
  ImUmlEdgeType_DataFlow,
  ImUmlEdgeType_Link,
  ImUmlEdgeType_NoteLink,
  ImUmlEdgeType_FlowLink, ///< Kind: ImUmlFlowLinkFlags_.
  ImUmlEdgeType_ShapeLink, ///< Kind: ImUmlShapeLinkFlags_.
  ImUmlEdgeType_TreeLink,
};

/// @brief A string in the diagram's text buffer (offsets survive growth).
struct ImUmlStr {
  int Offset;
  int Len;
  ImUmlStr() {
    Offset = -1;
    Len = 0;
  }
  bool Empty() const {
    return Len <= 0;
  }
};

/// @brief The text style current when an item was submitted.
struct ImUmlTextStyle {
  ImFont* Font;
  float Size;
  ImU32 Col;
};

struct ImUmlLine {
  ImUmlStr Text; ///< Member, activity, or ER attribute name.
  int Section; ///< 0 attributes / activities, 1 methods.
  bool Static, Abstract;
  ImUmlStr Type, Comment; ///< ER attributes.
  int Keys; ///< ER attributes: ImUmlAttributeKeys_.
};

enum ImUmlTreeFlags_ {
  ImUmlTreeFlags_Hanging = 1 << 0,
  ImUmlTreeFlags_Staff = 1 << 1,
  ImUmlTreeFlags_Vacant = 1 << 2,
};

struct ImUmlNodeRec {
  ImGuiID Id;
  int Type; ///< ImUmlNodeType_
  int Kind;
  ImUmlStr Name, Stereotype, Generic; ///< Stereotype also holds a process / store number, a device address.
  ImUmlStr Detail, Image; ///< Tree nodes: an org chart's third line and portrait.
  int TreeFlags; ///< ImUmlTreeFlags_
  int Hidden; ///< Tree nodes: collapsed descendants.
  int FirstLine, LineCount;
  int FirstAttach, AttachCount; ///< Notes: ids in AttachIds.
  int Parent; ///< Composite state containing it, or -1.
  int RankGroup;
  int Side;
  ImU32 LineCol, FillCol; ///< 0: the diagram's.
  ImUmlTextStyle Text;
  // Layout results (zoom 1, diagram coordinates).
  ImVec2 Size;
  ImVec2 Pos; ///< Centre.
  ImUmlShape Shape;
  bool Composite;
  float HeaderHeight; ///< Composite states: the name band.
};

struct ImUmlEdgeRec {
  int Type; ///< ImUmlEdgeType_
  int Kind;
  ImGuiID FromId, ToId;
  ImUmlStr Label, FromLabel, ToLabel;
  ImU32 Col;
  ImUmlTextStyle Text;
  int Weight, MinLen;
  bool Constraint;
  // Layout results.
  int From, To; ///< Node indices, -1 if unknown.
  int PointStart, PointCount;
  bool Bezier;
  ImVec2 LabelPos;
  ImVec2 LabelSize;
};

struct ImUmlSeqEventRec {
  int Type; ///< ImUmlSeqEventType_
  ImGuiID A, B;
  int Kind, Flags, Side;
  ImUmlStr Text;
  ImU32 Col;
  ImUmlTextStyle TextStyle;
};

struct ImUmlDiagram {
  ImGuiID ID;
  int Kind;
  ImUmlDiagramFlags Flags;
  ImVec2 SizeRequest;
  ImVec2 CursorPos;
  ImUmlStr Title;
  ImUmlTextStyle TitleStyle;
  bool SetupLocked; ///< An item was submitted: Setup* is too late.
  ImUmlDirection Direction;
  ImUmlEdgeStyle EdgeStyle;
  float NodeSpacing, RankSpacing;
  ImUmlSequenceFlags SequenceFlags;
  ImUmlStr SystemLabel;
  ImU32 Colors[ImUmlCol_COUNT];
  bool TextColorAuto; ///< Item text follows ImGuiCol_Text at submission.
  ImUmlStyle Style; ///< Copy at Begin, so pushes inside the scope do not move it.

  ImVector<char> TextBuf;
  ImVector<ImUmlNodeRec> Nodes;
  ImVector<ImUmlLine> Lines;
  ImVector<ImUmlEdgeRec> Edges;
  ImVector<ImGuiID> AttachIds;
  ImVector<ImUmlSeqEventRec> Events;
  ImVector<int> OpenStates; ///< BeginState stack.
  int OpenClass; ///< Node index of the class between BeginClass and EndClass, or -1.
  int OpenFragments;
  ImGuiStorage RankGroups; ///< Group name hash -> group index + 1.
  int SkippedItems;

  // Layout cache, kept across frames.
  ImGuiID LayoutHash;
  bool LayoutValid;
  int CacheSkipped; ///< Items skipped while laying out (sequence references), replayed on a cache hit.
  ImVector<ImVec2> CacheNodePos, CacheNodeSize;
  ImVector<float> CacheNodeHeader;
  ImVector<ImUmlGraphEdgeOut> CacheEdges;
  ImVector<ImVec2> Points;
  ImUmlSeqOutput Seq;
  ImVector<int> SeqParticipants; ///< Node indices of participants, in column order.
  ImVec2 ContentSize; ///< Laid out content at zoom 1, without the title.
  // View of ImUmlDiagramFlags_Interactive, kept across frames.
  ImVec2 ViewPan; ///< Screen offset added to the fitted origin.
  float ViewZoom; ///< Factor on the fitted zoom.
  bool HasNextView; ///< SetNextDiagramView() was called before this diagram's Begin.
  float NextZoom;
  ImVec2 NextCenter;
  ImRect Boundary; ///< Use case system boundary (diagram coordinates).
  ImUmlTreeMode TreeMode;
  ImVector<unsigned char> TreeRight; ///< Mind map: node on the root's right.
  ImVector<int> TreeDepth, TreeBranch, TreeHang;

  ImUmlDiagram() {
    ID = 0;
    Kind = 0;
    Flags = 0;
    TreeMode = ImUmlTreeMode_Tidy;
    OpenClass = -1;
    OpenFragments = 0;
    SkippedItems = 0;
    LayoutHash = 0;
    LayoutValid = false;
    CacheSkipped = 0;
    SetupLocked = false;
    ViewPan = ImVec2(0, 0);
    ViewZoom = 1.0f;
    HasNextView = false;
    NextZoom = 1.0f;
    NextCenter = ImVec2(0, 0);
  }
};

struct ImUmlColorMod {
  ImUmlCol Col;
  ImVec4 Backup;
};

struct ImUmlStyleMod {
  ImUmlStyleVar Var;
  ImVec2 Backup;
};

/// A registered shape artwork (deep copy of an ImUmlShapeDesc).
struct ImUmlShapeArt {
  ImVector<char> Name;
  ImVec2 ViewSize;
  bool Resizable;
  float Slice[4];
  bool GrowProportional;
  bool HasText, HasContent;
  ImVec2 TextMin, TextMax, ContentMin, ContentMax;
  ImVector<ImVec2> Points;
  ImVector<ImUmlShapePath> Paths;
  ImVector<ImVec2> Outline;
};

struct ImUmlContext {
  ImUmlStyle Style;
  ImVector<ImUmlShapeArt*> Shapes; ///< Sorted by name.
  ImVector<ImUmlColorMod> ColorModifiers;
  ImVector<ImUmlStyleMod> StyleModifiers;
  ImVector<ImUmlDiagram*> Diagrams;
  ImGuiStorage DiagramMap; ///< ID -> index + 1.
  ImUmlDiagram* Current;
  ImUmlDiagramInfo LastInfo;
  // Text capture (ImUmlDiagramFlags_CaptureText), while drawing.
  bool CaptureText = false;
  int RunKind = 0; ///< ImUmlTextRunKind_ of the item being drawn.
  int RunItem = 0;
  ImVector<ImUmlTextRun> Runs;
  ImVector<int> RunTextOffsets;
  ImVector<char> RunText;
  bool CaptureItems = false; ///< ImUmlDiagramFlags_CaptureItems.
  ImVector<ImUmlItemArea> Areas;
  bool HasNextView; ///< SetNextDiagramView(), taken by the next Begin.
  float NextZoom;
  ImVec2 NextCenter;
  // Next item.
  ImU32 NextLineCol, NextFillCol;
  bool HasNextRank;
  char NextRank[64];
  int NextWeight, NextMinLen;
  bool NextConstraint;
  ~ImUmlContext() {
    for (int i = 0; i < Shapes.Size; ++i) {
      IM_DELETE(Shapes[i]);
    }
  }
  ImUmlContext() {
    HasNextView = false;
    NextZoom = 1.0f;
    NextCenter = ImVec2(0, 0);
    Current = nullptr;
    NextLineCol = NextFillCol = 0;
    HasNextRank = false;
    NextRank[0] = 0;
    NextWeight = 1;
    NextMinLen = 1;
    NextConstraint = true;
  }
};

// ── Rendering primitives (imuml_render.cpp) ──────────────────────────────────
// Every function draws in screen coordinates already scaled by the zoom;
// `lw` is the line weight. Curves and circles use explicit segment counts.

enum ImUmlMarker_ {
  ImUmlMarker_None,
  ImUmlMarker_Open, ///< Two strokes: an open arrow.
  ImUmlMarker_Filled, ///< Filled triangle.
  ImUmlMarker_Hollow, ///< Hollow triangle (inheritance).
  ImUmlMarker_HollowDiamond,
  ImUmlMarker_FilledDiamond,
  ImUmlMarker_Cross,
};
typedef int ImUmlMarker;

namespace ImUml {
/// @brief Number of segments used for a circle of radius `r` (screen px).
int CircleSegments(float r);
/// @brief Flattens a path (polyline, or a cubic Bézier chain when `bezier`)
///        into `out`.
void FlattenPath(const ImVec2* points, int count, bool bezier, ImVector<ImVec2>* out);
/// @brief Strokes a flattened path, solid or dashed by arc length.
void RenderPath(ImDrawList* dl, const ImVec2* pts, int count, ImU32 col, float lw, bool dashed, float dash, float gap);
/// @brief A marker with its tip at `tip`, pointing along `dir` (unit).
void RenderMarker(
    ImDrawList* dl,
    ImUmlMarker marker,
    const ImVec2& tip,
    const ImVec2& dir,
    float size,
    ImU32 line,
    ImU32 bg,
    float lw);
/// @brief How far back from the tip a marker reaches (where the line stops).
float MarkerLength(ImUmlMarker marker, float size);
/// @brief An ER cardinality end (crow's foot notation) at `tip`, the line
///        arriving along `dir` (unit, pointing at the tip). The line runs
///        through it to the tip; circles are filled with `bg`.
/// @brief The icon of a network device, filling `a`..`b`.
void RenderDeviceIcon(ImDrawList* dl, ImUmlDeviceKind kind, const ImVec2& a, const ImVec2& b, ImU32 line, ImU32 fill, float lw);
void RenderCardinality(
    ImDrawList* dl,
    ImUmlCardinality cardinality,
    const ImVec2& tip,
    const ImVec2& dir,
    float size,
    ImU32 line,
    ImU32 bg,
    float lw);
void RenderDashedRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float lw, float dash, float gap);
void RenderNoteShape(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float fold, ImU32 fill, ImU32 border, float lw);
/// @brief A stick figure filling `a`..`b`.
void RenderActorFigure(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float lw);
void RenderDiamond(ImDrawList* dl, const ImVec2& c, const ImVec2& half, ImU32 fill, ImU32 border, float lw);
/// @brief Fragment frame with its pentagon name tab of `tab` size.
void RenderFragmentFrame(
    ImDrawList* dl,
    const ImVec2& a,
    const ImVec2& b,
    const ImVec2& tab,
    ImU32 border,
    ImU32 tab_fill,
    float lw);
/// @brief The registered shape artwork called `name`, or null.
const ImUmlShapeArt* FindShapeArt(const char* name);
/// @brief Database cylinder filling `a`..`b`.
void RenderCylinder(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 fill, ImU32 border, float lw);
} // namespace ImUml
