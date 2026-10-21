// MIT License © 2026 Binary Dice Games
/// @file element_icons.hpp
/// @brief Small vector icons for element types, drawn before their names in
///        the editor's element tree. Types of one kind (plot series, diagram
///        nodes, links, widgets...) share a picture; the colour tells the
///        family (plots, diagrams, user interface, layout).
#pragma once

#include <imgui.h>
#include <string_view>

namespace bdg::nymph {

/// @brief The pictures an element type can have.
enum class element_icon {
  generic, ///< anything not listed: a small square
  image, ///< the image options, Image
  layout_h, ///< HorizontalLayout
  layout_v, ///< VerticalLayout
  spring, ///< Spring, Spacing, Dummy, NewLine, SameLine
  text, ///< Label, Text..., PlotText
  separator, ///< Separator, SeparatorText
  plot, ///< Plot
  plot3d, ///< Plot3D
  line, ///< line-like series
  scatter, ///< scatter series
  bars, ///< vertical bars, histograms, stems
  bars_h, ///< horizontal bars, Gantt charts and tasks
  pie, ///< PlotPieChart
  heatmap, ///< heat maps, surfaces, ColorScale
  tag, ///< annotations, tags, infinite lines
  sankey, ///< Sankey diagrams, nodes and flows
  diagram, ///< graph diagrams and flowcharts
  sequence, ///< SequenceDiagram
  tree, ///< tree diagrams, mind maps, org charts
  node, ///< a box in a diagram
  ellipse, ///< UseCase
  actor, ///< Actor, Person
  participant, ///< Participant
  link, ///< an arrow between nodes
  group, ///< a dashed frame: groups, zones, fragments
  note, ///< Note
  shape, ///< Shape, ActivityControl, State-like diamonds
  window, ///< Screen, Window, Child, popups, tooltips
  button, ///< buttons and selectables
  checkbox, ///< Checkbox
  radio, ///< RadioButton
  slider, ///< sliders, drags, progress bars
  input, ///< text and number inputs, combos, list boxes, colour edits
  menu, ///< menus and menu bars
  table, ///< tables, rows and columns
  tabs, ///< tab bars, tabs, collapsing headers, tree nodes
  container, ///< Group, Indent, Disabled
};

/// @brief The picture for element type `type` ("PlotLine"), or "Image" for
///        the image options.
element_icon icon_for(std::string_view type);

/// @brief Draws the icon of `type` in the square at `min` with side `size`.
void draw_element_icon(ImDrawList* dl, ImVec2 min, float size, std::string_view type);

} // namespace bdg::nymph
