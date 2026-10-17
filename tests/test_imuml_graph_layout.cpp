// MIT License © 2026 Binary Dice Games
/// @file test_imuml_graph_layout.cpp
/// @brief UML plan steps 4-5 and 12: network simplex, ranking, ordering,
///        positions and routes of the dot-style graph layout. Pure: no ImGui
///        context.
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "imuml_internal.h"

namespace {

// ── Helpers ──────────────────────────────────────────────────────────────────

struct E {
  int from, to, minlen, weight;
};

ImVector<ImUmlNsEdge> ns_edges(const std::vector<E>& es) {
  ImVector<ImUmlNsEdge> out;
  for (const E& e : es) {
    ImUmlNsEdge n = {e.from, e.to, e.minlen, e.weight};
    out.push_back(n);
  }
  return out;
}

int64_t cost(const std::vector<E>& es, const ImVector<int>& r) {
  int64_t c = 0;
  for (const E& e : es) {
    c += static_cast<int64_t>(e.weight) * (r[e.to] - r[e.from]);
  }
  return c;
}

bool feasible(const std::vector<E>& es, const ImVector<int>& r) {
  for (const E& e : es) {
    if (r[e.to] - r[e.from] < e.minlen) {
      return false;
    }
  }
  return true;
}

/// The least cost over every rank assignment in [0, k] (brute force).
int64_t brute_force(int n, const std::vector<E>& es, int k) {
  ImVector<int> r;
  r.resize(n);
  int64_t best = INT64_MAX;
  std::vector<int> digits(static_cast<size_t>(n), 0);
  while (true) {
    for (int i = 0; i < n; ++i) {
      r[i] = digits[static_cast<size_t>(i)];
    }
    if (feasible(es, r)) {
      best = std::min(best, cost(es, r));
    }
    int i = 0;
    while (i < n && ++digits[static_cast<size_t>(i)] > k) {
      digits[static_cast<size_t>(i)] = 0;
      ++i;
    }
    if (i == n) {
      break;
    }
  }
  return best;
}

uint32_t lcg(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state >> 8;
}

ImUmlGraphInput graph(int nodes, const std::vector<std::pair<int, int>>& edges, float w = 40, float h = 20) {
  ImUmlGraphInput in;
  for (int i = 0; i < nodes; ++i) {
    ImUmlGraphNode n;
    n.Size = ImVec2(w, h);
    in.Nodes.push_back(n);
  }
  for (const auto& [a, b] : edges) {
    ImUmlGraphEdge e;
    e.From = a;
    e.To = b;
    in.Edges.push_back(e);
  }
  return in;
}

ImRect node_rect(const ImUmlGraphInput& in, const ImUmlGraphOutput& out, int i) {
  return ImRect(out.NodePos[i] - in.Nodes[i].Size * 0.5f, out.NodePos[i] + in.Nodes[i].Size * 0.5f);
}

void expect_no_overlap(const ImUmlGraphInput& in, const ImUmlGraphOutput& out) {
  for (int i = 0; i < in.Nodes.Size; ++i) {
    for (int j = i + 1; j < in.Nodes.Size; ++j) {
      ImRect a = node_rect(in, out, i);
      const ImRect b = node_rect(in, out, j);
      a.Expand(-0.5f);
      EXPECT_FALSE(a.Overlaps(b)) << "nodes " << i << " and " << j << " overlap";
    }
  }
}

// ── Network simplex ──────────────────────────────────────────────────────────

TEST(imuml_network_simplex, ChainAndDiamond) {
  ImVector<int> r;
  const std::vector<E> chain = {{0, 1, 1, 1}, {1, 2, 1, 1}};
  ASSERT_TRUE(ImUml::NetworkSimplex(3, ns_edges(chain), &r, 100));
  EXPECT_EQ(r[0], 0);
  EXPECT_EQ(r[1], 1);
  EXPECT_EQ(r[2], 2);
  const std::vector<E> diamond = {{0, 1, 1, 1}, {0, 2, 1, 1}, {1, 3, 1, 1}, {2, 3, 1, 1}};
  ASSERT_TRUE(ImUml::NetworkSimplex(4, ns_edges(diamond), &r, 100));
  EXPECT_EQ(cost(diamond, r), 4);
}

TEST(imuml_network_simplex, BeatsLongestPath) {
  // v -> a -> b -> c, and v -> t1, t2, t3: longest path puts the t's at the
  // bottom (cost 12); the optimum lifts them next to v (cost 6).
  const std::vector<E> es = {{0, 1, 1, 1}, {1, 2, 1, 1}, {2, 3, 1, 1}, {0, 4, 1, 1}, {0, 5, 1, 1}, {0, 6, 1, 1}};
  ImVector<int> r;
  EXPECT_TRUE(ImUml::NetworkSimplex(7, ns_edges(es), &r, 100));
  EXPECT_EQ(cost(es, r), 6);
  // With no pivots allowed the ranks are still feasible.
  ImUml::NetworkSimplex(7, ns_edges(es), &r, 0);
  EXPECT_TRUE(feasible(es, r));
}

TEST(imuml_network_simplex, MatchesBruteForceOnRandomDags) {
  uint32_t seed = 12345;
  for (int round = 0; round < 60; ++round) {
    const int n = 3 + static_cast<int>(lcg(seed) % 3); // 3-5 nodes
    std::vector<E> es;
    for (int a = 0; a < n; ++a) {
      for (int b = a + 1; b < n; ++b) {
        if (lcg(seed) % 3 == 0) {
          es.push_back({a, b, static_cast<int>(lcg(seed) % 2), 1 + static_cast<int>(lcg(seed) % 4)});
        }
      }
    }
    // Keep it connected: a spanning chain.
    for (int a = 0; a + 1 < n; ++a) {
      es.push_back({a, a + 1, 1, 1});
    }
    ImVector<int> r;
    ASSERT_TRUE(ImUml::NetworkSimplex(n, ns_edges(es), &r, 1000));
    ASSERT_TRUE(feasible(es, r)) << "round " << round;
    EXPECT_EQ(cost(es, r), brute_force(n, es, 2 * n)) << "round " << round;
  }
}

TEST(imuml_network_simplex, IsDeterministic) {
  const std::vector<E> es = {{0, 1, 1, 2}, {0, 2, 1, 1}, {1, 3, 2, 1}, {2, 3, 1, 3}, {3, 4, 1, 1}, {0, 4, 1, 1}};
  ImVector<int> a, b;
  ImUml::NetworkSimplex(5, ns_edges(es), &a, 100);
  ImUml::NetworkSimplex(5, ns_edges(es), &b, 100);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(a[i], b[i]);
  }
}

// ── Ranking ──────────────────────────────────────────────────────────────────

TEST(imuml_rank, MinLengthConstraintAndRankGroups) {
  ImUmlGraphInput in = graph(4, {{0, 1}, {1, 2}, {0, 3}});
  ImVector<int> r;
  ImVector<bool> rev;
  in.Edges[1].MinLen = 0; // 1 and 2 may share a rank
  ImUml::RankGraph(in, &r, &rev);
  EXPECT_EQ(r[1], r[2]);

  in = graph(4, {{0, 1}, {1, 2}, {0, 3}});
  in.Edges[0].MinLen = 3;
  ImUml::RankGraph(in, &r, &rev);
  EXPECT_GE(r[1] - r[0], 3);

  // A constraint: false edge does not change the ranks.
  ImUmlGraphInput base = graph(3, {{0, 1}});
  ImVector<int> r0;
  ImUml::RankGraph(base, &r0, &rev);
  ImUmlGraphEdge loose;
  loose.From = 0;
  loose.To = 2;
  loose.MinLen = 4;
  loose.Constraint = false;
  base.Edges.push_back(loose);
  ImUml::RankGraph(base, &r, &rev);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(r[i], r0[i]);
  }

  // Rank groups share a rank.
  in = graph(4, {{0, 1}, {1, 2}, {0, 3}});
  in.Nodes[2].RankGroup = 7;
  in.Nodes[3].RankGroup = 7;
  ImUml::RankGraph(in, &r, &rev);
  EXPECT_EQ(r[2], r[3]);
}

TEST(imuml_rank, PinsGoFirstAndLast) {
  ImUmlGraphInput in = graph(4, {{0, 1}, {1, 2}, {2, 3}});
  in.Nodes[1].Pin = 1;
  in.Nodes[2].Pin = 2;
  ImVector<int> r;
  ImVector<bool> rev;
  ImUml::RankGraph(in, &r, &rev);
  for (int i = 0; i < 4; ++i) {
    EXPECT_LE(r[1], r[i]);
    EXPECT_GE(r[2], r[i]);
  }
  EXPECT_LT(r[1], r[0]);
  EXPECT_GT(r[2], r[3]);
}

TEST(imuml_rank, CyclesSelfLoopsAndComponents) {
  ImUmlGraphInput in = graph(5, {{0, 1}, {1, 2}, {2, 0}, {3, 3}, {3, 4}});
  ImVector<int> r;
  ImVector<bool> rev;
  ImUml::RankGraph(in, &r, &rev);
  int reversed = 0;
  for (int e = 0; e < rev.Size; ++e) {
    reversed += rev[e] ? 1 : 0;
  }
  EXPECT_EQ(reversed, 1);
  EXPECT_FALSE(rev[3]); // the self-loop is ignored, not reversed
  EXPECT_EQ(r[0], 0); // each component starts at rank 0
  EXPECT_EQ(r[3], 0);
  EXPECT_EQ(r[4], 1);
}

// ── Whole layout ─────────────────────────────────────────────────────────────

TEST(imuml_layout, NoOverlapsOnAssortedGraphs) {
  const std::vector<std::vector<std::pair<int, int>>> graphs = {
      {{0, 1}, {1, 2}, {2, 3}},
      {{0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}},
      {{0, 1}, {0, 2}, {1, 3}, {2, 3}},
      {{0, 3}, {0, 4}, {0, 5}, {1, 3}, {1, 4}, {1, 5}, {2, 3}, {2, 4}, {2, 5}},
      {{0, 1}, {1, 2}, {2, 0}},
      {{0, 0}, {0, 1}},
      {{0, 1}, {0, 1}, {0, 1}},
      {{0, 1}, {2, 3}},
  };
  for (const auto& edges : graphs) {
    int n = 0;
    for (const auto& [a, b] : edges) {
      n = std::max({n, a + 1, b + 1});
    }
    const ImUmlGraphInput in = graph(n, edges);
    ImUmlGraphOutput out;
    ImUml::LayoutGraph(in, &out);
    expect_no_overlap(in, out);
  }
}

TEST(imuml_layout, PlanarGraphsHaveNoCrossings) {
  for (const auto& edges : std::vector<std::vector<std::pair<int, int>>>{
           {{0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {2, 6}}, // tree
           {{0, 3}, {1, 2}, {0, 2}, {2, 4}, {3, 5}}, // given in a crossing order
           {{0, 3}, {1, 2}, {2, 4}, {3, 5}}, // two chains given crossed
       }) {
    int n = 0;
    for (const auto& [a, b] : edges) {
      n = std::max({n, a + 1, b + 1});
    }
    ImUmlGraphOutput out;
    ImUml::LayoutGraph(graph(n, edges), &out);
    EXPECT_EQ(out.Crossings, 0);
  }
}

TEST(imuml_layout, LongEdgesStraightenAndParentsCentre) {
  // a -> b -> c -> d and a long edge a -> d through two virtual nodes.
  ImUmlGraphInput in = graph(4, {{0, 1}, {1, 2}, {2, 3}, {0, 3}});
  in.EdgeStyle = ImUmlEdgeStyle_Polyline;
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  const ImUmlGraphEdgeOut& longe = out.Edges[3];
  ASSERT_EQ(longe.PointCount, 4);
  EXPECT_EQ(out.Points[longe.PointStart + 1].x, out.Points[longe.PointStart + 2].x);

  ImUmlGraphInput tree = graph(3, {{0, 1}, {0, 2}});
  ImUml::LayoutGraph(tree, &out);
  EXPECT_NEAR(out.NodePos[0].x, (out.NodePos[1].x + out.NodePos[2].x) * 0.5f, 1.0f);
  EXPECT_LT(out.NodePos[0].y, out.NodePos[1].y);
}

TEST(imuml_layout, EdgeEndsLieOnTheShape) {
  for (ImUmlShape shape : {ImUmlShape_Box, ImUmlShape_Ellipse, ImUmlShape_Diamond, ImUmlShape_Circle}) {
    ImUmlGraphInput in = graph(3, {{0, 1}, {0, 2}}, 60, 30);
    for (int i = 0; i < 3; ++i) {
      in.Nodes[i].Shape = shape;
    }
    in.EdgeStyle = ImUmlEdgeStyle_Polyline;
    ImUmlGraphOutput out;
    ImUml::LayoutGraph(in, &out);
    for (int e = 0; e < 2; ++e) {
      const ImUmlGraphEdgeOut& eo = out.Edges[e];
      const ImVec2 start = out.Points[eo.PointStart];
      const ImVec2 c = out.NodePos[0];
      const ImVec2 half = in.Nodes[0].Size * 0.5f;
      const ImVec2 d = start - c;
      float value = 0.0f; // 1 on the boundary
      switch (shape) {
        case ImUmlShape_Ellipse:
          value = std::sqrt((d.x / half.x) * (d.x / half.x) + (d.y / half.y) * (d.y / half.y));
          break;
        case ImUmlShape_Circle:
          value = std::sqrt(d.x * d.x + d.y * d.y) / std::min(half.x, half.y);
          break;
        case ImUmlShape_Diamond:
          value = std::fabs(d.x) / half.x + std::fabs(d.y) / half.y;
          break;
        default:
          value = std::max(std::fabs(d.x) / half.x, std::fabs(d.y) / half.y);
          break;
      }
      EXPECT_NEAR(value, 1.0f, 0.08f) << "shape " << shape;
    }
  }
}

TEST(imuml_layout, LabelsGetTheirOwnSpace) {
  ImUmlGraphInput in = graph(3, {{0, 1}, {0, 2}});
  in.Edges[0].LabelSize = ImVec2(80, 16);
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  const ImRect label(out.Edges[0].LabelPos - ImVec2(40, 8), out.Edges[0].LabelPos + ImVec2(40, 8));
  for (int i = 0; i < 3; ++i) {
    ImRect r = node_rect(in, out, i);
    r.Expand(-0.5f);
    EXPECT_FALSE(r.Overlaps(label)) << "label overlaps node " << i;
  }
}

TEST(imuml_layout, SplinesBendOnlyWherePathsDo) {
  // a -> b -> c -> d, a long labelled edge a -> d, a short edge a -> b.
  ImUmlGraphInput in = graph(5, {{0, 1}, {1, 2}, {2, 3}, {0, 3}, {0, 4}});
  in.Edges[3].LabelSize = ImVec2(60, 14);
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  for (int e = 0; e < in.Edges.Size; ++e) {
    const ImUmlGraphEdgeOut& eo = out.Edges[e];
    ASSERT_TRUE(eo.Bezier);
    const ImVec2* p = &out.Points[eo.PointStart];
    // Never doubling back along the rank axis.
    for (int i = 1; i < eo.PointCount; ++i) {
      EXPECT_GE(p[i].y, p[i - 1].y) << "edge " << e << " point " << i;
    }
    // Sideways, the curve turns at most once (no S-shapes).
    ImVector<ImVec2> flat;
    ImUml::FlattenPath(p, eo.PointCount, true, &flat);
    int turns = 0, sign = 0;
    for (int i = 1; i < flat.Size; ++i) {
      const float dx = flat[i].x - flat[i - 1].x;
      const int s = dx > 0.5f ? 1 : (dx < -0.5f ? -1 : 0);
      if (s != 0 && sign != 0 && s != sign) {
        ++turns;
      }
      sign = s != 0 ? s : sign;
    }
    EXPECT_LE(turns, 1) << "edge " << e;
  }
  // A two-point edge is a straight line: its control points lie on it.
  ImUml::LayoutGraph(graph(3, {{0, 1}, {0, 2}}), &out);
  const ImUmlGraphEdgeOut& shrt = out.Edges[1];
  ASSERT_EQ(shrt.PointCount, 4);
  const ImVec2 a = out.Points[shrt.PointStart], b = out.Points[shrt.PointStart + 3];
  for (int k = 1; k <= 2; ++k) {
    const ImVec2 c = out.Points[shrt.PointStart + k];
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    EXPECT_NEAR(cross / std::sqrt(ImLengthSqr(b - a)), 0.0f, 1.0f);
  }
}

// ── Clusters ─────────────────────────────────────────────────────────────────

/// The media player of examples/uml_state.yaml: start -> stopped,
/// stopped -> playing (inside Active), paused (inside) -> stopped,
/// stopped -> end, and inside Active: a0 -> playing <-> paused.
ImUmlGraphInput media_player(ImUmlDirection dir = ImUmlDirection_TopDown) {
  // 0 start, 1 stopped, 2 end, 3 a0, 4 playing, 5 paused
  ImUmlGraphInput in = graph(6, {{0, 1}, {1, 4}, {5, 1}, {1, 2}, {3, 4}, {4, 5}, {5, 4}}, 60, 24);
  in.Direction = dir;
  ImUmlGraphCluster active;
  active.HeaderSize = ImVec2(80, 20);
  in.Clusters.push_back(active);
  for (int v = 3; v <= 5; ++v) {
    in.Nodes[v].Cluster = 0;
  }
  return in;
}

TEST(imuml_clusters, MembersInsideOthersOutside) {
  for (ImUmlDirection dir : {ImUmlDirection_TopDown, ImUmlDirection_LeftRight, ImUmlDirection_BottomUp}) {
    const ImUmlGraphInput in = media_player(dir);
    ImUmlGraphOutput out;
    ImUml::LayoutGraph(in, &out);
    ASSERT_EQ(out.ClusterRects.Size, 1);
    const ImRect box = out.ClusterRects[0];
    for (int v = 0; v < 6; ++v) {
      const ImRect r = node_rect(in, out, v);
      if (in.Nodes[v].Cluster == 0) {
        EXPECT_TRUE(box.Contains(r)) << "member " << v << " direction " << dir;
      } else {
        EXPECT_FALSE(box.Overlaps(r)) << "outsider " << v << " direction " << dir;
      }
    }
    expect_no_overlap(in, out);
  }
}

TEST(imuml_clusters, EdgesLeavingABoxGoAroundIt) {
  ImUmlGraphInput in = media_player();
  in.EdgeStyle = ImUmlEdgeStyle_Polyline;
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  EXPECT_LE(out.Crossings, 2); // only the two edges that enter the box cross its border
  ImRect box = out.ClusterRects[0];
  box.Expand(-2.0f);
  // paused -> stopped: every point after the start (on paused, inside the
  // box) lies outside the box, and no segment after the first crosses it.
  const ImUmlGraphEdgeOut& stop = out.Edges[2];
  for (int i = 1; i < stop.PointCount; ++i) {
    EXPECT_FALSE(box.Contains(out.Points[stop.PointStart + i])) << "point " << i;
  }
  // stopped -> end never enters the box.
  const ImUmlGraphEdgeOut& eject = out.Edges[3];
  for (int i = 0; i < eject.PointCount; ++i) {
    EXPECT_FALSE(box.Contains(out.Points[eject.PointStart + i]));
  }
}

TEST(imuml_clusters, NestedBoxesAndEdgesToABox) {
  // 0 outside, 1 in A, 2 in B (B inside A); edges: 0 -> box A, 2 -> 0.
  ImUmlGraphInput in = graph(3, {{0, 0}, {2, 0}, {1, 2}}, 50, 20);
  in.Edges[0].ToCluster = 0;
  in.Edges[0].From = 0;
  in.Edges[0].To = 0;
  ImUmlGraphCluster a, b;
  b.Parent = 0;
  in.Clusters.push_back(a);
  in.Clusters.push_back(b);
  in.Nodes[1].Cluster = 0;
  in.Nodes[2].Cluster = 1;
  in.EdgeStyle = ImUmlEdgeStyle_Polyline;
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  ASSERT_EQ(out.ClusterRects.Size, 2);
  EXPECT_TRUE(out.ClusterRects[0].Contains(out.ClusterRects[1]));
  EXPECT_TRUE(out.ClusterRects[1].Contains(node_rect(in, out, 2)));
  EXPECT_FALSE(out.ClusterRects[1].Overlaps(node_rect(in, out, 1)));
  // The edge to box A ends on its border.
  const ImUmlGraphEdgeOut& to_box = out.Edges[0];
  ASSERT_GE(to_box.PointCount, 2);
  const ImVec2 end = out.Points[to_box.PointStart + to_box.PointCount - 1];
  const ImRect a_box = out.ClusterRects[0];
  const float d = std::min({std::fabs(end.x - a_box.Min.x), std::fabs(end.x - a_box.Max.x),
                            std::fabs(end.y - a_box.Min.y), std::fabs(end.y - a_box.Max.y)});
  EXPECT_LE(d, 1.0f);
}

TEST(imuml_clusters, SiblingBoxesNeverOverlap) {
  // Two sibling clusters fed from one node, with edges between them, the
  // shape of the network example (a DMZ and a LAN behind a firewall).
  // 0 fw; A: 1 lb, 2 web1, 3 web2; B: 4 sw, 5 db, 6 pc, 7 ap, 8 phone.
  ImUmlGraphInput in = graph(9, {{0, 1}, {1, 2}, {1, 3}, {0, 4}, {4, 5}, {4, 6}, {4, 7}, {7, 8}, {2, 5}}, 60, 50);
  in.Edges[8].Constraint = false;
  ImUmlGraphCluster a, b;
  in.Clusters.push_back(a);
  in.Clusters.push_back(b);
  for (int v = 1; v <= 3; ++v) {
    in.Nodes[v].Cluster = 0;
  }
  for (int v = 4; v <= 8; ++v) {
    in.Nodes[v].Cluster = 1;
  }
  ImUmlGraphOutput out;
  ImUml::LayoutGraph(in, &out);
  EXPECT_FALSE(out.ClusterRects[0].Overlaps(out.ClusterRects[1]));
  EXPECT_FALSE(out.ClusterRects[0].Overlaps(node_rect(in, out, 0)));
  EXPECT_FALSE(out.ClusterRects[1].Overlaps(node_rect(in, out, 0)));
  for (int v = 1; v <= 8; ++v) {
    EXPECT_TRUE(out.ClusterRects[in.Nodes[v].Cluster].Contains(node_rect(in, out, v))) << v;
  }
}

TEST(imuml_clusters, Deterministic) {
  ImUmlGraphOutput a, b;
  ImUml::LayoutGraph(media_player(), &a);
  ImUml::LayoutGraph(media_player(), &b);
  ASSERT_EQ(a.Points.Size, b.Points.Size);
  for (int i = 0; i < a.Points.Size; ++i) {
    EXPECT_EQ(a.Points[i].x, b.Points[i].x);
    EXPECT_EQ(a.Points[i].y, b.Points[i].y);
  }
  EXPECT_EQ(a.ClusterRects[0].Min.x, b.ClusterRects[0].Min.x);
}

TEST(imuml_layout, LeftRightIsTopDownTransposed) {
  ImUmlGraphInput td = graph(4, {{0, 1}, {0, 2}, {1, 3}}, 50, 20);
  ImUmlGraphInput lr = graph(4, {{0, 1}, {0, 2}, {1, 3}}, 20, 50);
  lr.Direction = ImUmlDirection_LeftRight;
  ImUmlGraphOutput a, b;
  ImUml::LayoutGraph(td, &a);
  ImUml::LayoutGraph(lr, &b);
  EXPECT_EQ(a.Size.x, b.Size.y);
  EXPECT_EQ(a.Size.y, b.Size.x);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(a.NodePos[i].x, b.NodePos[i].y);
    EXPECT_EQ(a.NodePos[i].y, b.NodePos[i].x);
  }
}

TEST(imuml_layout, DeterministicWholePixels) {
  ImUmlGraphInput in = graph(6, {{0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {4, 0}, {1, 5}, {5, 5}}, 47, 23);
  in.Edges[2].LabelSize = ImVec2(31, 13);
  ImUmlGraphOutput a, b;
  ImUml::LayoutGraph(in, &a);
  ImUml::LayoutGraph(in, &b);
  ASSERT_EQ(a.Points.Size, b.Points.Size);
  for (int i = 0; i < a.NodePos.Size; ++i) {
    EXPECT_EQ(a.NodePos[i].x, b.NodePos[i].x);
    EXPECT_EQ(a.NodePos[i].y, b.NodePos[i].y);
    EXPECT_EQ(a.NodePos[i].y, std::floor(a.NodePos[i].y));
  }
  for (int i = 0; i < a.Points.Size; ++i) {
    EXPECT_EQ(a.Points[i].x, b.Points[i].x);
    EXPECT_EQ(a.Points[i].x, std::floor(a.Points[i].x));
    EXPECT_EQ(a.Points[i].y, std::floor(a.Points[i].y));
  }
}

} // namespace
