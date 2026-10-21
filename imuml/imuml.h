// MIT License © 2026 Binary Dice Games
/// @file imuml.h
/// @brief ImUml: UML diagrams for Dear ImGui, in the style of ImPlot.
///
/// A diagram is a Begin/End scope. Between them, the program submits the
/// diagram's nodes and edges every frame; End lays them out automatically
/// (a layered layout after Graphviz's dot for class, state, activity and
/// use case diagrams and flowcharts, a grid for sequence diagrams), scales
/// the result to fit the space reserved, and draws it on the window's draw
/// list.
///
///     if (ImUml::BeginClassDiagram("Animals")) {
///       ImUml::BeginClass("Animal", nullptr, ImUmlClassKind_Abstract);
///       ImUml::ClassAttribute("+name: string");
///       ImUml::ClassMethod("+makeSound()*");
///       ImUml::EndClass();
///       ImUml::Class("Duck");
///       ImUml::Relation("Duck", "Animal", ImUmlRelationKind_Inheritance);
///       ImUml::EndClassDiagram();
///     }
///
/// Items are recorded, not drawn, until End: positions are known only once
/// every node and edge is in. Text is copied, so the strings passed need
/// not outlive the call. An edge naming an id that was never submitted is
/// skipped (counted in ImUmlDiagramInfo::SkippedItems); ImUml never fails.
///
/// Design: spec/03_uml_diagrams/DESIGN.md (nymph repository).
#pragma once

#include "imgui.h"

#define IMUML_VERSION "0.1.0"
/// Colour resolved from the current ImGui style at Begin*.
#define IMUML_AUTO_COL ImVec4(0, 0, 0, -1)

struct ImUmlContext;

typedef int ImUmlCol; // -> enum ImUmlCol_
typedef int ImUmlStyleVar; // -> enum ImUmlStyleVar_
typedef int ImUmlDiagramFlags; // -> enum ImUmlDiagramFlags_
typedef int ImUmlDirection; // -> enum ImUmlDirection_
typedef int ImUmlEdgeStyle; // -> enum ImUmlEdgeStyle_
typedef int ImUmlClassKind; // -> enum ImUmlClassKind_
typedef int ImUmlRelationKind; // -> enum ImUmlRelationKind_
typedef int ImUmlParticipantKind; // -> enum ImUmlParticipantKind_
typedef int ImUmlParticipantFlags; // -> enum ImUmlParticipantFlags_
typedef int ImUmlMessageKind; // -> enum ImUmlMessageKind_
typedef int ImUmlMessageFlags; // -> enum ImUmlMessageFlags_
typedef int ImUmlSequenceFlags; // -> enum ImUmlSequenceFlags_
typedef int ImUmlFragmentKind; // -> enum ImUmlFragmentKind_
typedef int ImUmlStateKind; // -> enum ImUmlStateKind_
typedef int ImUmlActionKind; // -> enum ImUmlActionKind_
typedef int ImUmlControlKind; // -> enum ImUmlControlKind_
typedef int ImUmlSide; // -> enum ImUmlSide_
typedef int ImUmlUseCaseLinkKind; // -> enum ImUmlUseCaseLinkKind_
typedef int ImUmlFlowShape; // -> enum ImUmlFlowShape_
typedef int ImUmlFlowLinkFlags; // -> enum ImUmlFlowLinkFlags_
typedef int ImUmlCardinality; // -> enum ImUmlCardinality_
typedef int ImUmlAttributeKeys; // -> enum ImUmlAttributeKeys_
typedef int ImUmlRelationshipFlags; // -> enum ImUmlRelationshipFlags_
typedef int ImUmlDataFlowFlags; // -> enum ImUmlDataFlowFlags_
typedef int ImUmlDeviceKind; // -> enum ImUmlDeviceKind_
typedef int ImUmlLinkKind; // -> enum ImUmlLinkKind_
typedef int ImUmlLinkFlags; // -> enum ImUmlLinkFlags_
typedef int ImUmlShapeLinkFlags; // -> enum ImUmlShapeLinkFlags_
typedef int ImUmlTreeMode; // -> enum ImUmlTreeMode_

// ── Enums ────────────────────────────────────────────────────────────────────

enum ImUmlCol_ {
  ImUmlCol_Line, ///< Edges, arrows, lifelines' default.
  ImUmlCol_NodeFill, ///< Inside of boxes, ellipses and compartments.
  ImUmlCol_NodeBorder, ///< Outline of nodes.
  ImUmlCol_Text, ///< Node and edge text.
  ImUmlCol_NoteFill,
  ImUmlCol_NoteBorder,
  ImUmlCol_FragmentBorder, ///< Sequence fragments.
  ImUmlCol_FragmentTab, ///< Fill of a fragment's name tab.
  ImUmlCol_Activation, ///< Fill of activation bars.
  ImUmlCol_Lifeline,
  ImUmlCol_EdgeLabelBg, ///< Background behind edge labels.
  ImUmlCol_Boundary, ///< Composite state and system boundary outline.
  ImUmlCol_Title,
  ImUmlCol_Bg, ///< Inside of hollow markers (inheritance triangles).
  ImUmlCol_COUNT
};

enum ImUmlStyleVar_ {
  ImUmlStyleVar_LineWeight, ///< float
  ImUmlStyleVar_NodePadding, ///< ImVec2
  ImUmlStyleVar_NodeRounding, ///< float
  ImUmlStyleVar_NodeSpacing, ///< float, dot's nodesep
  ImUmlStyleVar_RankSpacing, ///< float, dot's ranksep
  ImUmlStyleVar_ArrowSize, ///< float
  ImUmlStyleVar_DashLength, ///< float
  ImUmlStyleVar_GapLength, ///< float
  ImUmlStyleVar_ParticipantSpacing, ///< float
  ImUmlStyleVar_MessageSpacing, ///< float
  ImUmlStyleVar_ActivationWidth, ///< float
  ImUmlStyleVar_NoteFold, ///< float
  ImUmlStyleVar_MaxZoom, ///< float
  ImUmlStyleVar_COUNT
};

enum ImUmlDiagramFlags_ {
  ImUmlDiagramFlags_None = 0,
  ImUmlDiagramFlags_NoTitle = 1 << 0, ///< Hide the title (the ID after "##" is never shown).
  ImUmlDiagramFlags_MeasureOnly = 1 << 1, ///< Lay out but draw nothing and reserve no space; see GetLastDiagramInfo().
  ImUmlDiagramFlags_NoFit = 1 << 2, ///< Draw at zoom 1, clipped, instead of scaling to fit.
  /// The mouse wheel zooms around the mouse (0.25x to 8x of the fit),
  /// dragging pans, a double-click fits again; the view is kept per diagram
  /// ID. ImUmlDiagramInfo reports the hovered node or edge (graph diagrams).
  ImUmlDiagramFlags_Interactive = 1 << 3,
  /// Records every text the diagram draws (ImUmlDiagramInfo::TextRuns), with
  /// the item it belongs to, so a host can tell what is under the pointer.
  /// Drawing is unchanged.
  ImUmlDiagramFlags_CaptureText = 1 << 4,
};

/// What an ImUmlTextRun belongs to.
enum ImUmlTextRunKind_ {
  ImUmlTextRunKind_None,
  ImUmlTextRunKind_Node, ///< Item: index of the node in submission order.
  ImUmlTextRunKind_Edge, ///< Item: index of the edge in submission order.
  ImUmlTextRunKind_Event, ///< Sequence diagrams: message, note, fragment; item: index of the event.
  ImUmlTextRunKind_Title, ///< The diagram's title; item 0.
};

/// @brief One text a diagram drew, in screen coordinates.
struct ImUmlTextRun {
  int Kind; ///< ImUmlTextRunKind_
  int Item;
  ImVec2 Min, Max; ///< Screen rectangle of the text (all its lines).
  const char* Text; ///< The text as drawn, zero-terminated; valid until the next diagram is drawn.
};

enum ImUmlDirection_ {
  ImUmlDirection_TopDown,
  ImUmlDirection_BottomUp,
  ImUmlDirection_LeftRight,
  ImUmlDirection_RightLeft,
};

enum ImUmlEdgeStyle_ {
  ImUmlEdgeStyle_Spline,
  ImUmlEdgeStyle_Polyline,
  ImUmlEdgeStyle_Elbow, ///< Tree diagrams: orthogonal connectors.
  ImUmlEdgeStyle_Curve, ///< Tree diagrams: smooth curves.
  ImUmlEdgeStyle_Straight, ///< Tree diagrams: straight lines.
};

enum ImUmlClassKind_ {
  ImUmlClassKind_Class,
  ImUmlClassKind_Abstract, ///< «abstract», italic name.
  ImUmlClassKind_Interface, ///< «interface».
  ImUmlClassKind_Enum, ///< «enumeration».
  ImUmlClassKind_Object, ///< An object diagram instance: the name ("order1 : Order") is underlined.
};

enum ImUmlRelationKind_ {
  ImUmlRelationKind_Association,
  ImUmlRelationKind_DirectedAssociation,
  ImUmlRelationKind_Inheritance,
  ImUmlRelationKind_Realization,
  ImUmlRelationKind_Dependency,
  ImUmlRelationKind_Aggregation,
  ImUmlRelationKind_Composition,
};

enum ImUmlParticipantKind_ {
  ImUmlParticipantKind_Participant,
  ImUmlParticipantKind_Actor,
  ImUmlParticipantKind_Boundary,
  ImUmlParticipantKind_Control,
  ImUmlParticipantKind_Entity,
  ImUmlParticipantKind_Database,
};

enum ImUmlParticipantFlags_ {
  ImUmlParticipantFlags_None = 0,
  ImUmlParticipantFlags_Created = 1 << 0, ///< The box appears at the first message to it (a Create message).
};

enum ImUmlMessageKind_ {
  ImUmlMessageKind_Sync, ///< Solid line, filled arrow.
  ImUmlMessageKind_Async, ///< Solid line, open arrow.
  ImUmlMessageKind_Reply, ///< Dashed line, open arrow.
  ImUmlMessageKind_Create, ///< Dashed line to the new participant's box.
  ImUmlMessageKind_Destroy, ///< Solid line; the target's lifeline ends in a cross.
};

enum ImUmlMessageFlags_ {
  ImUmlMessageFlags_None = 0,
  ImUmlMessageFlags_Activate = 1 << 0, ///< Start an activation on the target (mermaid's +).
  ImUmlMessageFlags_Deactivate = 1 << 1, ///< End the source's activation (mermaid's -).
};

enum ImUmlSequenceFlags_ {
  ImUmlSequenceFlags_None = 0,
  ImUmlSequenceFlags_Autonumber = 1 << 0,
  ImUmlSequenceFlags_MirrorParticipants = 1 << 1, ///< Repeat the participant boxes at the bottom.
};

enum ImUmlFragmentKind_ {
  ImUmlFragmentKind_Loop,
  ImUmlFragmentKind_Alt,
  ImUmlFragmentKind_Opt,
  ImUmlFragmentKind_Par,
  ImUmlFragmentKind_Critical,
  ImUmlFragmentKind_Break,
  ImUmlFragmentKind_Group, ///< A filled box with no tab (mermaid's rect).
};

enum ImUmlStateKind_ {
  ImUmlStateKind_Simple,
  ImUmlStateKind_Initial,
  ImUmlStateKind_Final,
  ImUmlStateKind_Choice,
  ImUmlStateKind_Fork,
  ImUmlStateKind_Join,
  ImUmlStateKind_History,
  ImUmlStateKind_DeepHistory,
};

enum ImUmlActionKind_ {
  ImUmlActionKind_Action,
  ImUmlActionKind_Send,
  ImUmlActionKind_Receive,
};

enum ImUmlControlKind_ {
  ImUmlControlKind_Initial,
  ImUmlControlKind_Final,
  ImUmlControlKind_FlowFinal,
  ImUmlControlKind_Decision,
  ImUmlControlKind_Merge,
  ImUmlControlKind_Fork,
  ImUmlControlKind_Join,
};

enum ImUmlSide_ {
  ImUmlSide_Left,
  ImUmlSide_Right,
};

enum ImUmlUseCaseLinkKind_ {
  ImUmlUseCaseLinkKind_Association,
  ImUmlUseCaseLinkKind_Include,
  ImUmlUseCaseLinkKind_Extend,
  ImUmlUseCaseLinkKind_Generalization,
};

/// @brief Flowchart node shapes (mermaid's flowchart shapes).
enum ImUmlFlowShape_ {
  ImUmlFlowShape_Box, ///< Rectangle: a process step.
  ImUmlFlowShape_Rounded, ///< Rounded rectangle.
  ImUmlFlowShape_Stadium, ///< Pill: start or end.
  ImUmlFlowShape_Diamond, ///< Decision.
  ImUmlFlowShape_Circle,
  ImUmlFlowShape_Parallelogram, ///< Input or output.
  ImUmlFlowShape_Hexagon, ///< Preparation.
  ImUmlFlowShape_Cylinder, ///< Database or storage.
  ImUmlFlowShape_Subroutine, ///< Rectangle with double sides: a predefined process.
};

/// @brief Flowchart link look (mermaid's -->, ---, -.->, ==>, <-->).
enum ImUmlFlowLinkFlags_ {
  ImUmlFlowLinkFlags_None = 0, ///< Solid line, arrow at `to`.
  ImUmlFlowLinkFlags_Dotted = 1 << 0, ///< Dashed line.
  ImUmlFlowLinkFlags_Thick = 1 << 1, ///< Twice the line weight.
  ImUmlFlowLinkFlags_NoArrow = 1 << 2, ///< No arrowhead.
  ImUmlFlowLinkFlags_BothArrows = 1 << 3, ///< Arrowheads at both ends.
};

/// How many of an entity an ER relationship end allows (crow's foot).
enum ImUmlCardinality_ {
  ImUmlCardinality_ExactlyOne, ///< Two bars.
  ImUmlCardinality_ZeroOrOne, ///< A circle and a bar.
  ImUmlCardinality_ZeroOrMore, ///< A circle and a crow's foot.
  ImUmlCardinality_OneOrMore, ///< A bar and a crow's foot.
};

enum ImUmlAttributeKeys_ {
  ImUmlAttributeKeys_None = 0,
  ImUmlAttributeKeys_PK = 1 << 0, ///< Primary key.
  ImUmlAttributeKeys_FK = 1 << 1, ///< Foreign key.
  ImUmlAttributeKeys_UK = 1 << 2, ///< Unique key.
};

enum ImUmlRelationshipFlags_ {
  ImUmlRelationshipFlags_None = 0,
  ImUmlRelationshipFlags_NonIdentifying = 1 << 0, ///< Dashed line (mermaid's `..`); identifying is solid (`--`).
};

enum ImUmlDataFlowFlags_ {
  ImUmlDataFlowFlags_None = 0,
  ImUmlDataFlowFlags_Bidirectional = 1 << 0, ///< Arrowheads at both ends.
};

/// The icon a network device is drawn with.
enum ImUmlDeviceKind_ {
  ImUmlDeviceKind_Server,
  ImUmlDeviceKind_Database,
  ImUmlDeviceKind_Storage,
  ImUmlDeviceKind_Router,
  ImUmlDeviceKind_Switch,
  ImUmlDeviceKind_Firewall,
  ImUmlDeviceKind_LoadBalancer,
  ImUmlDeviceKind_AccessPoint,
  ImUmlDeviceKind_Cloud,
  ImUmlDeviceKind_Internet,
  ImUmlDeviceKind_Workstation,
  ImUmlDeviceKind_Laptop,
  ImUmlDeviceKind_Mobile,
  ImUmlDeviceKind_User,
};

enum ImUmlLinkKind_ {
  ImUmlLinkKind_Wired, ///< Solid line.
  ImUmlLinkKind_Wireless, ///< Short dashes.
  ImUmlLinkKind_Vpn, ///< Long dashes, a thicker line.
};

enum ImUmlLinkFlags_ {
  ImUmlLinkFlags_None = 0,
  ImUmlLinkFlags_Directed = 1 << 0, ///< An arrowhead at the `to` end.
};

// ── Style ────────────────────────────────────────────────────────────────────

struct ImUmlStyle {
  float LineWeight; ///< Edge and outline thickness at zoom 1.
  ImVec2 NodePadding; ///< Inside boxes, around text.
  float NodeRounding; ///< Corner radius of rounded boxes (states, actions).
  float NodeSpacing; ///< Between nodes in a rank (dot's nodesep).
  float RankSpacing; ///< Between ranks (dot's ranksep).
  float ArrowSize; ///< Length of arrowheads and markers.
  float DashLength;
  float GapLength;
  float ParticipantSpacing; ///< Least gap between sequence participant boxes.
  float MessageSpacing; ///< Vertical space between sequence rows.
  float ActivationWidth;
  float NoteFold; ///< Size of a note's folded corner.
  float MaxZoom; ///< Largest scale when fitting a diagram to its space.
  ImVec4 Colors[ImUmlCol_COUNT];
  ImFont* ItalicFont; ///< For abstract names and members; null draws them upright with " {abstract}".
  IMGUI_API ImUmlStyle();
};

/// @brief What the last End*() computed.
struct ImUmlDiagramInfo {
  ImVec2 NaturalSize; ///< Size at zoom 1, title included.
  ImVec2 ContentSize; ///< The scalable part of NaturalSize: the diagram without its title.
  float TitleHeight; ///< The part that is never scaled.
  ImVec2 Size; ///< Size reserved (0 with MeasureOnly).
  float Zoom; ///< Scale the content was drawn at.
  int NodeCount;
  int EdgeCount;
  int SkippedItems; ///< Edges or messages naming an unknown id.
  /// ImUmlDiagramFlags_CaptureText only: the texts drawn, in drawing order.
  int TextRunCount;
  const ImUmlTextRun* TextRuns;
  // ImUmlDiagramFlags_Interactive only:
  float ViewZoom; ///< The user's zoom on top of the fit (1 = fitted).
  ImVec2 ViewCenter; ///< Content point (zoom-1 units) at the centre of the view; also with SetNextDiagramView().
  int HoveredNode; ///< Node under the mouse, in submission order; -1 for none.
  ImGuiID HoveredNodeId; ///< Its id hashed with ImHashStr; 0 for none.
  int HoveredEdge; ///< Edge under the mouse (not a node), in submission order; -1 for none.
  ImGuiID HoveredEdgeFrom, HoveredEdgeTo; ///< Its ends' ids, hashed.
  ImVec2 HoveredMin, HoveredMax; ///< Screen rectangle of the hovered node or edge.
  ImUmlDiagramInfo() {
    NaturalSize = ContentSize = Size = ImVec2(0, 0);
    TitleHeight = 0.0f;
    Zoom = 1.0f;
    NodeCount = EdgeCount = SkippedItems = 0;
    TextRunCount = 0;
    TextRuns = nullptr;
    ViewZoom = 1.0f;
    ViewCenter = ImVec2(0, 0);
    HoveredNode = HoveredEdge = -1;
    HoveredNodeId = HoveredEdgeFrom = HoveredEdgeTo = 0;
    HoveredMin = HoveredMax = ImVec2(0, 0);
  }
};

/// Link styles of ShapeLink().
enum ImUmlShapeLinkFlags_ {
  ImUmlShapeLinkFlags_None = 0,
  ImUmlShapeLinkFlags_Dashed = 1 << 0,
  ImUmlShapeLinkFlags_Thick = 1 << 1,
  ImUmlShapeLinkFlags_ArrowOpen = 1 << 2, ///< Open arrow at `to`.
  ImUmlShapeLinkFlags_ArrowFilled = 1 << 3, ///< Filled arrow at `to`.
  ImUmlShapeLinkFlags_ArrowHollow = 1 << 4, ///< Hollow triangle at `to` (generalisation, realisation).
  ImUmlShapeLinkFlags_BothEnds = 1 << 5, ///< The same arrow at `from` too.
  /// A ball-and-socket connector: a ball on the `from` side (the provider),
  /// a socket cupping it on the `to` side (the consumer), the label beside.
  ImUmlShapeLinkFlags_Assembly = 1 << 6,
  ImUmlShapeLinkFlags_RankToFirst = 1 << 7, ///< The layout ranks `to` above `from`.
};

/// Layout and look of a tree diagram.
enum ImUmlTreeMode_ {
  ImUmlTreeMode_Tidy, ///< A plain tree: each parent centred over its children.
  ImUmlTreeMode_MindMap, ///< The root in the middle, branches to both sides, coloured by branch.
  ImUmlTreeMode_OrgChart, ///< Cards with a portrait, name, title; hanging teams; assistants beside the parent.
};

/// @brief One node of a tree diagram (BeginTreeNode()).
struct ImUmlTreeNodeDesc {
  const char* Id; ///< Optional; needed to be named by TreeLink().
  const char* Text; ///< The label; an org chart's name.
  const char* Subtext; ///< Org chart: the job title.
  const char* Detail; ///< Org chart: a small third line (department).
  const char* Shape; ///< A registered shape; null: the mode's default.
  const char* Image; ///< Org chart: a registered shape or placeholder image ("image:person") for a portrait.
  int Side; ///< Mind map, root children: ImUmlSide_Left or Right; -1 balances.
  bool Hanging; ///< Org chart: this node's descendants hang below it in an indented list.
  bool Staff; ///< Org chart: an assistant, drawn beside its parent.
  bool Vacant; ///< Org chart: a dashed card with no portrait.
  int Hidden; ///< Descendants not submitted (collapsed): a "+N" badge when above 0.
  ImU32 Color; ///< Branch or outline colour; 0 for the mode's.
  ImUmlTreeNodeDesc() {
    Id = Text = Subtext = Detail = Shape = Image = nullptr;
    Side = -1;
    Hanging = Staff = Vacant = false;
    Hidden = 0;
    Color = 0;
  }
};

// ── Shapes ───────────────────────────────────────────────────────────────────
// A shape is vector artwork registered by name (nymph registers the SVG files
// of resources/shapes/; any ImGui program can register its own). ImUml knows
// geometry only: no files, no parser. Spec: spec/13_more_diagrams_and_shapes.

/// Colour a shape path takes.
enum ImUmlShapeRole_ {
  ImUmlShapeRole_Line, ///< The line colour (SVG #000000).
  ImUmlShapeRole_Half, ///< The line colour at half alpha (SVG #808080).
  ImUmlShapeRole_Fill, ///< The node fill colour (SVG #FFFFFF).
};

/// @brief One filled or stroked polyline of a shape, in viewBox units.
struct ImUmlShapePath {
  int Role; ///< ImUmlShapeRole_
  bool Stroke; ///< Stroked (else filled).
  bool Closed;
  float Width; ///< Stroke width, a multiple of the line weight.
  float Dash, Gap; ///< Dashed stroke, viewBox units (0: solid).
  int PointStart, PointCount; ///< Range in ImUmlShapeDesc::Points.
};

/// @brief What AddShape() takes; copied, so the arrays need not outlive the call.
struct ImUmlShapeDesc {
  const char* Name;
  ImVec2 ViewSize; ///< Nominal size: one unit is one pixel at zoom 1.
  bool Resizable; ///< Fills any size (else an icon: keeps its aspect ratio).
  float Slice[4]; ///< Left, top, right, bottom margins that keep their size when resized.
  bool GrowProportional; ///< Node size grows with the label's ratio (else by margins).
  bool HasText; ///< Label rect (else the whole shape; icons put the label below).
  ImVec2 TextMin, TextMax;
  bool HasContent; ///< Where a container's children go.
  ImVec2 ContentMin, ContentMax;
  const ImVec2* Points; ///< All paths' flattened points.
  int PointCount;
  const ImUmlShapePath* Paths; ///< In painter's order.
  int PathCount;
  const ImVec2* Outline; ///< Closed border for clipping; null: the viewBox.
  int OutlineCount;
};

namespace ImUml {

// ── Context ──────────────────────────────────────────────────────────────────

IMGUI_API ImUmlContext* CreateContext();
/// @brief Destroys `ctx`, or the current context when null.
IMGUI_API void DestroyContext(ImUmlContext* ctx = nullptr);
IMGUI_API ImUmlContext* GetCurrentContext();
IMGUI_API void SetCurrentContext(ImUmlContext* ctx);

// ── Style ────────────────────────────────────────────────────────────────────

IMGUI_API ImUmlStyle& GetStyle();
/// @brief Every colour IMUML_AUTO_COL: follows the ImGui style (the default).
IMGUI_API void StyleColorsAuto(ImUmlStyle* dst = nullptr);
IMGUI_API void StyleColorsLight(ImUmlStyle* dst = nullptr);
IMGUI_API void StyleColorsDark(ImUmlStyle* dst = nullptr);
IMGUI_API void StyleColorsClassic(ImUmlStyle* dst = nullptr);
IMGUI_API void PushStyleColor(ImUmlCol idx, ImU32 col);
IMGUI_API void PushStyleColor(ImUmlCol idx, const ImVec4& col);
IMGUI_API void PopStyleColor(int count = 1);
IMGUI_API void PushStyleVar(ImUmlStyleVar idx, float val);
IMGUI_API void PushStyleVar(ImUmlStyleVar idx, const ImVec2& val);
IMGUI_API void PopStyleVar(int count = 1);
/// @brief The resolved colour `idx` of the diagram being submitted, else of
///        the style (auto colours resolved from the current ImGui style).
IMGUI_API ImU32 GetColorU32(ImUmlCol idx);

// ── Next item ────────────────────────────────────────────────────────────────

/// @brief Colours of the next node or edge: `line` its outline or line,
///        `fill` its inside; 0 keeps the diagram's colour.
IMGUI_API void SetNextItemColors(ImU32 line, ImU32 fill = 0);
/// @brief Places the next node in one rank with every other node of the
///        same `group` (dot's rank=same). Graph diagrams only.
IMGUI_API void SetNextNodeRank(const char* group);
/// @brief Layout hints of the next edge, after dot's attributes: `weight`
///        keeps it short and straight (0-100, default 1), `min_length` is
///        the least number of ranks it spans (dot's minlen, default 1), and
///        `constraint` false leaves it out of ranking.
IMGUI_API void SetNextEdgeLayout(float weight, int min_length = 1, bool constraint = true);

// ── Diagram scopes ───────────────────────────────────────────────────────────
// `size`: a positive value is pixels, 0 the natural size, a negative value
// the available space minus its magnitude (as ImGui's item sizes). Return
// false when the window is collapsed or clipped; then submit nothing and do
// not call End*.

IMGUI_API bool BeginClassDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndClassDiagram();
IMGUI_API bool
BeginSequenceDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndSequenceDiagram();
IMGUI_API bool BeginStateDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndStateDiagram();
IMGUI_API bool
BeginActivityDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndActivityDiagram();
IMGUI_API bool
BeginUseCaseDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndUseCaseDiagram();
IMGUI_API bool BeginFlowchart(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndFlowchart();
/// @brief An entity-relationship diagram (database structure, mermaid's erDiagram).
IMGUI_API bool BeginERDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndERDiagram();
/// @brief A data flow diagram (DFD): external entities, processes, data
///        stores and the data flowing between them.
IMGUI_API bool
BeginDataFlowDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndDataFlowDiagram();
/// @brief A network diagram: devices drawn as icons, links, and zones.
IMGUI_API bool
BeginNetworkDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndNetworkDiagram();

// ── Setup (after Begin*, before the first item) ──────────────────────────────

/// @brief Graph diagrams: rank direction (dot's rankdir) and edge shape.
IMGUI_API void SetupLayout(ImUmlDirection direction, ImUmlEdgeStyle edge_style = ImUmlEdgeStyle_Spline);
/// @brief Graph diagrams: node and rank spacing; a value <= 0 keeps the style's.
///        Sequence diagrams: `node_spacing` is the least gap between participants.
IMGUI_API void SetupSpacing(float node_spacing, float rank_spacing = 0.0f);
IMGUI_API void SetupSequence(ImUmlSequenceFlags flags);
/// @brief Use case diagrams: draw a system boundary titled `label` around
///        the use cases.
IMGUI_API void SetupSystemBoundary(const char* label);

// ── Class diagrams ───────────────────────────────────────────────────────────

/// @brief A class box. `name` defaults to `id`; `stereotype` replaces the
///        kind's («interface»); `generic` (type parameters) is drawn in a
///        dashed box on the corner. Members end with EndClass().
IMGUI_API void BeginClass(
    const char* id,
    const char* name = nullptr,
    ImUmlClassKind kind = ImUmlClassKind_Class,
    const char* stereotype = nullptr,
    const char* generic = nullptr);
/// @brief A member line, drawn as written, except a trailing `$` (static:
///        underlined) or `*` (abstract: italic), which is not drawn.
IMGUI_API void ClassAttribute(const char* text);
IMGUI_API void ClassMethod(const char* text);
IMGUI_API void EndClass();
/// @brief BeginClass() + EndClass() with no members.
IMGUI_API void Class(const char* id, const char* name = nullptr, ImUmlClassKind kind = ImUmlClassKind_Class);
/// @brief A relation from `from` to `to`; `from_label` / `to_label` are
///        drawn next to each end (multiplicities).
IMGUI_API void Relation(
    const char* from,
    const char* to,
    ImUmlRelationKind kind = ImUmlRelationKind_Association,
    const char* label = nullptr,
    const char* from_label = nullptr,
    const char* to_label = nullptr);

// ── Notes (graph diagrams) ───────────────────────────────────────────────────

/// @brief A note, in the rank of the first attached node and joined to each
///        by a dashed line; with no ids, a free note.
IMGUI_API void Note(const char* text, const char* const* attach_ids = nullptr, int attach_count = 0);

// ── Sequence diagrams ────────────────────────────────────────────────────────

IMGUI_API void Participant(
    const char* id,
    const char* name = nullptr,
    ImUmlParticipantKind kind = ImUmlParticipantKind_Participant,
    ImUmlParticipantFlags flags = 0);
IMGUI_API void Message(
    const char* from,
    const char* to,
    const char* text,
    ImUmlMessageKind kind = ImUmlMessageKind_Sync,
    ImUmlMessageFlags flags = 0);
/// @brief A combined fragment, ended by EndFragment(); `over_from` /
///        `over_to` name its leftmost and rightmost participants (null: the
///        ones its messages touch).
IMGUI_API void BeginFragment(
    ImUmlFragmentKind kind,
    const char* label,
    const char* over_from = nullptr,
    const char* over_to = nullptr);
/// @brief Starts the next operand of an Alt, Par or Critical fragment.
IMGUI_API void FragmentElse(const char* label);
IMGUI_API void EndFragment();
/// @brief A note over one participant, or spanning `a` to `b`.
IMGUI_API void NoteOver(const char* text, const char* a, const char* b = nullptr);
IMGUI_API void NoteSide(const char* text, const char* id, ImUmlSide side);

// ── State diagrams ───────────────────────────────────────────────────────────

/// @brief A state; states and transitions submitted before EndState() are
///        inside it (a composite state).
IMGUI_API void BeginState(const char* id, const char* name = nullptr, ImUmlStateKind kind = ImUmlStateKind_Simple);
/// @brief A line of the state's activity compartment ("entry / start").
IMGUI_API void StateActivity(const char* text);
IMGUI_API void EndState();
IMGUI_API void State(const char* id, const char* name = nullptr, ImUmlStateKind kind = ImUmlStateKind_Simple);
IMGUI_API void Transition(const char* from, const char* to, const char* label = nullptr);

// ── Activity diagrams ────────────────────────────────────────────────────────

IMGUI_API void Action(const char* id, const char* text, ImUmlActionKind kind = ImUmlActionKind_Action);
IMGUI_API void ControlNode(const char* id, ImUmlControlKind kind, const char* label = nullptr);
/// @brief A control flow; `guard` is drawn as "[guard]".
IMGUI_API void Flow(const char* from, const char* to, const char* guard = nullptr);

// ── Use case diagrams ────────────────────────────────────────────────────────

/// @brief An actor, in the first (Left) or last (Right) rank, outside the
///        system boundary.
IMGUI_API void Actor(const char* id, const char* name = nullptr, ImUmlSide side = ImUmlSide_Left);
IMGUI_API void UseCase(const char* id, const char* name = nullptr);
IMGUI_API void
UseCaseLink(const char* from, const char* to, ImUmlUseCaseLinkKind kind = ImUmlUseCaseLinkKind_Association);

// ── Flowcharts ───────────────────────────────────────────────────────────────

/// @brief A node: `text` (lines split at '\n') in `shape`; `text` null
///        shows the id.
IMGUI_API void FlowNode(const char* id, const char* text = nullptr, ImUmlFlowShape shape = ImUmlFlowShape_Box);
/// @brief A group (mermaid's subgraph): a titled box around the nodes,
///        groups and notes submitted before EndFlowGroup(). Links may name
///        a group: they end at its box.
IMGUI_API void BeginFlowGroup(const char* id, const char* title = nullptr);
IMGUI_API void EndFlowGroup();
/// @brief A link from `from` to `to` (nodes or groups) with an optional
///        `label` in the middle.
IMGUI_API void FlowLink(const char* from, const char* to, const char* label = nullptr, ImUmlFlowLinkFlags flags = 0);
// ── ER diagrams ──────────────────────────────────────────────────────────────

/// @brief An entity (a table): a header with its name (defaults to `id`)
///        over one row per attribute. Attributes end with EndEntity().
IMGUI_API void BeginEntity(const char* id, const char* name = nullptr);
/// @brief An attribute row: its type, name, keys (PK / FK / UK) and an
///        optional comment, drawn in aligned columns.
IMGUI_API void
EntityAttribute(const char* type, const char* name, ImUmlAttributeKeys keys = 0, const char* comment = nullptr);
IMGUI_API void EndEntity();
/// @brief BeginEntity() + EndEntity() with no attributes.
IMGUI_API void Entity(const char* id, const char* name = nullptr);
/// @brief A relationship from `from` to `to`, with the cardinality allowed
///        at each end (drawn at that end, crow's foot notation).
IMGUI_API void Relationship(
    const char* from,
    const char* to,
    ImUmlCardinality from_cardinality,
    ImUmlCardinality to_cardinality,
    const char* label = nullptr,
    ImUmlRelationshipFlags flags = 0);

// ── Groups (data flow and network diagrams) ──────────────────────────────────

/// @brief A dashed box around the nodes submitted before EndGroup(): a
///        data flow diagram's trust boundary, a network zone (a subnet, a
///        DMZ). Groups nest; edges never end at a group.
IMGUI_API void BeginGroup(const char* id, const char* label = nullptr);
IMGUI_API void EndGroup();

// ── Data flow diagrams ───────────────────────────────────────────────────────

/// @brief A source or sink outside the system: a box.
IMGUI_API void ExternalEntity(const char* id, const char* name = nullptr);
/// @brief A process transforming data: a circle, `number` ("1.2") on top.
IMGUI_API void Process(const char* id, const char* name = nullptr, const char* number = nullptr);
/// @brief Data at rest: an open-ended box, `number` ("D1") in its left cell.
IMGUI_API void DataStore(const char* id, const char* name = nullptr, const char* number = nullptr);
/// @brief Data moving from `from` to `to`, labelled with what flows.
IMGUI_API void DataFlow(const char* from, const char* to, const char* label = nullptr, ImUmlDataFlowFlags flags = 0);

// ── Network diagrams ─────────────────────────────────────────────────────────

/// @brief A device: an icon of `kind`, its name below, and `address` (an IP,
///        a host name) under the name.
IMGUI_API void Device(
    const char* id,
    const char* name = nullptr,
    ImUmlDeviceKind kind = ImUmlDeviceKind_Server,
    const char* address = nullptr);
/// @brief A link between two devices; `from_label` / `to_label` (ports,
///        interfaces) are drawn next to each end.
IMGUI_API void Link(
    const char* from,
    const char* to,
    ImUmlLinkKind kind = ImUmlLinkKind_Wired,
    const char* label = nullptr,
    const char* from_label = nullptr,
    const char* to_label = nullptr,
    ImUmlLinkFlags flags = 0);

// ── Shape diagrams ───────────────────────────────────────────────────────────
// Component, deployment and package diagrams, and flowcharts with library
// shapes: nodes and groups drawn with registered shapes (AddShape), on the
// layered layout.

IMGUI_API bool BeginShapeDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndShapeDiagram();
/// @brief A node drawn with shape `shape`: `text` (lines separated by newlines) in
///        the shape's text area, `stereotype` ("component": drawn as
///        guillemets) above it. Also valid in a Flowchart.
IMGUI_API void ShapeNode(const char* id, const char* text = nullptr, const char* shape = "rectangle", const char* stereotype = nullptr);
/// @brief A group: a frame drawn with `shape`, its title in the header band,
///        the nodes submitted until EndShapeGroup() inside it. A group with
///        nothing in it is a node. Also valid in a Flowchart.
IMGUI_API void BeginShapeGroup(const char* id, const char* title = nullptr, const char* shape = "rectangle", const char* stereotype = nullptr);
IMGUI_API void EndShapeGroup();
/// @brief A link between two nodes or groups; `from_label` / `to_label` are
///        drawn next to each end.
IMGUI_API void ShapeLink(
    const char* from, const char* to, const char* label = nullptr, ImUmlShapeLinkFlags flags = 0,
    const char* from_label = nullptr, const char* to_label = nullptr);

// ── Tree diagrams ────────────────────────────────────────────────────────────
// Trees, mind maps and org charts: the hierarchy is the nesting of
// BeginTreeNode() / EndTreeNode(); positions come from a tidy-tree layout.

IMGUI_API bool BeginTreeDiagram(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImUmlDiagramFlags flags = 0);
IMGUI_API void EndTreeDiagram();
/// @brief Sets the mode; call after Begin and before the first node. The
///        direction and edge style come from SetupLayout() (Elbow, Curve or
///        Straight edges).
IMGUI_API void SetupTree(ImUmlTreeMode mode);
IMGUI_API void BeginTreeNode(const ImUmlTreeNodeDesc& desc);
IMGUI_API void EndTreeNode();
/// @brief A dashed curve between two nodes with an id; it does not move
///        anything.
IMGUI_API void TreeLink(const char* from, const char* to, const char* label = nullptr);

// ── Shapes (imuml_shape.cpp) ─────────────────────────────────────────────────

/// @brief Registers (or replaces) a shape in the current context.
IMGUI_API void AddShape(const ImUmlShapeDesc& desc);
/// @brief Whether a shape of that name is registered.
IMGUI_API bool HasShape(const char* name);
/// @brief Draws shape `name` into [`min`, `max`): a resizable shape fills it
///        (margins of its slice scaled by `zoom`), an icon is centred at its
///        aspect ratio. Returns false (drawing nothing) for an unknown name.
IMGUI_API bool RenderShape(
    ImDrawList* dl, const char* name, const ImVec2& min, const ImVec2& max, ImU32 line, ImU32 fill, float line_weight,
    float zoom = 1.0f);
/// @brief The label rectangle of shape `name` once drawn into [`min`, `max`)
///        (the whole box for a shape without one). False for an unknown name.
IMGUI_API bool GetShapeTextRect(const char* name, const ImVec2& min, const ImVec2& max, float zoom, ImVec2* text_min, ImVec2* text_max);
/// @brief The size a node of shape `name` needs around a label of
///        `label_size` (zoom 1): never below the shape's nominal size. An icon
///        needs only its nominal size (the label goes below it).
IMGUI_API ImVec2 GetShapeNodeSize(const char* name, const ImVec2& label_size);
/// @brief The shape's nominal size; (0, 0) for an unknown name.
IMGUI_API ImVec2 GetShapeSize(const char* name);
/// @brief Whether shape `name` is an icon (keeps its aspect ratio).
IMGUI_API bool IsShapeIcon(const char* name);

// ── Queries ──────────────────────────────────────────────────────────────────

IMGUI_API const ImUmlDiagramInfo& GetLastDiagramInfo();

/// How many nodes, edges and sequence events the open diagram has had
/// submitted so far; false (and nothing set) outside a diagram. A host
/// compares the counts before and after drawing one of its items to learn
/// which nodes, edges and events the item made.
IMGUI_API bool GetItemCounts(int* nodes, int* edges, int* events);
/// Sets the view of the next diagram drawn (not measured): `zoom` on top of
/// the fit (0.25 to 8) and the content point (zoom-1 units, as
/// ImUmlDiagramInfo::ViewCenter) at the centre; a NaN component centres the
/// content on that axis. Applies with or without ImUmlDiagramFlags_Interactive
/// (a saved view drawn into an image); with it, the user moves on from there.
IMGUI_API void SetNextDiagramView(float zoom, const ImVec2& center);

} // namespace ImUml
