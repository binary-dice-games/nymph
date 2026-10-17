// MIT License © 2026 Binary Dice Games
/// @file imuml_graph_layout.cpp
/// @brief The layered graph layout of class, state, activity and use case
///        diagrams: dot's four phases, implemented from the paper (Gansner,
///        Koutsofios, North, Vo, "A Technique for Drawing Directed Graphs",
///        IEEE TSE 19(3), 1993). No Graphviz code is used.
///
/// 1. Rank: network simplex on the edges' weights and minimum lengths.
/// 2. Order: weighted median sweeps plus transpose, best order kept.
/// 3. Position: network simplex on dot's auxiliary graph for x; ranks
///    stacked for y.
/// 4. Route: paths through the virtual nodes, clipped to node shapes,
///    optionally turned into cubic Béziers.
/// Everything is computed top-down in whole pixels and transformed to the
/// requested direction at the end. Every loop runs in index order and every
/// tie is broken by index, so the result depends only on the input.
#include "imuml_internal.h"

namespace {

// ── Small helpers ────────────────────────────────────────────────────────────

/// Stable merge sort; `less(a, b)` must be a strict weak order.
template <typename T, typename LESS>
void StableSort(T* data, int count, LESS less) {
  if (count < 2) {
    return;
  }
  if (count <= 16) {
    for (int i = 1; i < count; ++i) {
      T v = data[i];
      int j = i - 1;
      while (j >= 0 && less(v, data[j])) {
        data[j + 1] = data[j];
        --j;
      }
      data[j + 1] = v;
    }
    return;
  }
  const int half = count / 2;
  StableSort(data, half, less);
  StableSort(data + half, count - half, less);
  ImVector<T> tmp;
  tmp.resize(count);
  int a = 0, b = half, o = 0;
  while (a < half && b < count) {
    tmp[o++] = less(data[b], data[a]) ? data[b++] : data[a++];
  }
  while (a < half) {
    tmp[o++] = data[a++];
  }
  while (b < count) {
    tmp[o++] = data[b++];
  }
  for (int i = 0; i < count; ++i) {
    data[i] = tmp[i];
  }
}

float RoundPx(float v) {
  return ImFloor(v + 0.5f);
}

ImVec2 RoundPx(const ImVec2& v) {
  return ImVec2(RoundPx(v.x), RoundPx(v.y));
}

/// Union-find over node indices.
struct DisjointSets {
  ImVector<int> Parent;
  explicit DisjointSets(int n) {
    Parent.resize(n);
    for (int i = 0; i < n; ++i) {
      Parent[i] = i;
    }
  }
  int Find(int v) {
    while (Parent[v] != v) {
      Parent[v] = Parent[Parent[v]];
      v = Parent[v];
    }
    return v;
  }
  /// Joins the sets; the smaller root index becomes the root, so the result
  /// does not depend on the order of joins.
  void Join(int a, int b) {
    a = Find(a);
    b = Find(b);
    if (a == b) {
      return;
    }
    if (a < b) {
      Parent[b] = a;
    } else {
      Parent[a] = b;
    }
  }
};

// ── Network simplex ──────────────────────────────────────────────────────────

/// Network simplex on one connected, acyclic graph (the paper's section 2.3,
/// with dot's cyclic search for the leaving edge).
struct Simplex {
  int N;
  ImVector<ImUmlNsEdge> E;
  ImVector<int> AdjStart, Adj; // incident edges per node, in edge order
  ImVector<int> Rank;
  ImVector<bool> InTree, TreeEdge;
  ImVector<int> CutValue;
  ImVector<int> Low, Lim, Parent, ParentEdge, PostOrder;
  ImVector<int> TreeAdjStart, TreeAdj;

  int Slack(int e) const {
    return Rank[E[e].To] - Rank[E[e].From] - E[e].MinLen;
  }

  int Other(int e, int v) const {
    return E[e].From == v ? E[e].To : E[e].From;
  }

  void BuildAdjacency() {
    AdjStart.resize(N + 1);
    for (int i = 0; i <= N; ++i) {
      AdjStart[i] = 0;
    }
    for (int e = 0; e < E.Size; ++e) {
      AdjStart[E[e].From + 1]++;
      AdjStart[E[e].To + 1]++;
    }
    for (int i = 0; i < N; ++i) {
      AdjStart[i + 1] += AdjStart[i];
    }
    Adj.resize(AdjStart[N]);
    ImVector<int> fill;
    fill.resize(N);
    for (int i = 0; i < N; ++i) {
      fill[i] = AdjStart[i];
    }
    for (int e = 0; e < E.Size; ++e) {
      Adj[fill[E[e].From]++] = e;
      Adj[fill[E[e].To]++] = e;
    }
  }

  /// Feasible initial ranks: longest path from the sinks (Kahn's order).
  void InitRanks() {
    ImVector<int> indegree, order;
    indegree.resize(N);
    for (int v = 0; v < N; ++v) {
      indegree[v] = 0;
    }
    for (int e = 0; e < E.Size; ++e) {
      indegree[E[e].To]++;
    }
    for (int v = 0; v < N; ++v) {
      if (indegree[v] == 0) {
        order.push_back(v);
      }
    }
    for (int head = 0; head < order.Size; ++head) {
      const int v = order[head];
      for (int i = AdjStart[v]; i < AdjStart[v + 1]; ++i) {
        const int e = Adj[i];
        if (E[e].From == v && --indegree[E[e].To] == 0) {
          order.push_back(E[e].To);
        }
      }
    }
    Rank.resize(N);
    for (int v = 0; v < N; ++v) {
      Rank[v] = 0;
    }
    for (int k = order.Size - 1; k >= 0; --k) {
      const int v = order[k];
      bool any = false;
      int r = 0;
      for (int i = AdjStart[v]; i < AdjStart[v + 1]; ++i) {
        const int e = Adj[i];
        if (E[e].From == v) {
          const int cand = Rank[E[e].To] - E[e].MinLen;
          r = any ? ImMin(r, cand) : cand;
          any = true;
        }
      }
      Rank[v] = r;
    }
  }

  /// Grows the tree along tight edges; returns the number of tree nodes.
  int GrowTightTree() {
    ImVector<int> stack;
    int count = 0;
    for (int v = 0; v < N; ++v) {
      if (InTree[v]) {
        stack.push_back(v);
        ++count;
      }
    }
    while (!stack.empty()) {
      const int v = stack.back();
      stack.pop_back();
      for (int i = AdjStart[v]; i < AdjStart[v + 1]; ++i) {
        const int e = Adj[i];
        const int w = Other(e, v);
        if (!InTree[w] && Slack(e) == 0) {
          InTree[w] = true;
          TreeEdge[e] = true;
          stack.push_back(w);
          ++count;
        }
      }
    }
    return count;
  }

  void FeasibleTree() {
    InTree.resize(N);
    TreeEdge.resize(E.Size);
    for (int v = 0; v < N; ++v) {
      InTree[v] = false;
    }
    for (int e = 0; e < E.Size; ++e) {
      TreeEdge[e] = false;
    }
    InTree[0] = true;
    while (GrowTightTree() < N) {
      int best = -1;
      for (int e = 0; e < E.Size; ++e) {
        if (InTree[E[e].From] != InTree[E[e].To] && (best < 0 || Slack(e) < Slack(best))) {
          best = e;
        }
      }
      const int delta = InTree[E[best].From] ? Slack(best) : -Slack(best);
      for (int v = 0; v < N; ++v) {
        if (InTree[v]) {
          Rank[v] += delta;
        }
      }
    }
  }

  void BuildTreeAdjacency() {
    TreeAdjStart.resize(N + 1);
    for (int i = 0; i <= N; ++i) {
      TreeAdjStart[i] = 0;
    }
    for (int e = 0; e < E.Size; ++e) {
      if (TreeEdge[e]) {
        TreeAdjStart[E[e].From + 1]++;
        TreeAdjStart[E[e].To + 1]++;
      }
    }
    for (int i = 0; i < N; ++i) {
      TreeAdjStart[i + 1] += TreeAdjStart[i];
    }
    TreeAdj.resize(TreeAdjStart[N]);
    ImVector<int> fill;
    fill.resize(N);
    for (int i = 0; i < N; ++i) {
      fill[i] = TreeAdjStart[i];
    }
    for (int e = 0; e < E.Size; ++e) {
      if (TreeEdge[e]) {
        TreeAdj[fill[E[e].From]++] = e;
        TreeAdj[fill[E[e].To]++] = e;
      }
    }
  }

  /// Post-order numbers (Lim), subtree minimum (Low), parents, from node 0.
  void LowLim() {
    Low.resize(N);
    Lim.resize(N);
    Parent.resize(N);
    ParentEdge.resize(N);
    PostOrder.resize(0);
    ImVector<int> stack, next;
    next.resize(N);
    for (int v = 0; v < N; ++v) {
      next[v] = TreeAdjStart[v];
    }
    int lim = 1;
    Parent[0] = -1;
    ParentEdge[0] = -1;
    Low[0] = lim;
    stack.push_back(0);
    while (!stack.empty()) {
      const int v = stack.back();
      if (next[v] < TreeAdjStart[v + 1]) {
        const int e = TreeAdj[next[v]++];
        const int w = Other(e, v);
        if (e == ParentEdge[v]) {
          continue;
        }
        Parent[w] = v;
        ParentEdge[w] = e;
        Low[w] = lim;
        stack.push_back(w);
      } else {
        Lim[v] = lim++;
        PostOrder.push_back(v);
        stack.pop_back();
      }
    }
  }

  void CutValues() {
    CutValue.resize(E.Size);
    for (int k = 0; k < PostOrder.Size; ++k) {
      const int child = PostOrder[k];
      const int pe = ParentEdge[child];
      if (pe < 0) {
        continue;
      }
      const bool child_is_tail = E[pe].From == child;
      int cut = E[pe].Weight;
      for (int i = AdjStart[child]; i < AdjStart[child + 1]; ++i) {
        const int e = Adj[i];
        if (e == pe) {
          continue;
        }
        const bool is_out = E[e].From == child;
        const bool points_to_head = is_out == child_is_tail;
        cut += points_to_head ? E[e].Weight : -E[e].Weight;
        if (TreeEdge[e]) {
          cut += points_to_head ? -CutValue[e] : CutValue[e];
        }
      }
      CutValue[pe] = cut;
    }
  }

  bool IsDescendant(int v, int root) const {
    return Low[root] <= Lim[v] && Lim[v] <= Lim[root];
  }

  int EnterEdge(int leaving) const {
    int tail = E[leaving].From;
    bool flip = false;
    if (Lim[E[leaving].From] > Lim[E[leaving].To]) {
      tail = E[leaving].To;
      flip = true;
    }
    int best = -1;
    for (int e = 0; e < E.Size; ++e) {
      if (flip == IsDescendant(E[e].From, tail) && flip != IsDescendant(E[e].To, tail) &&
          (best < 0 || Slack(e) < Slack(best))) {
        best = e;
      }
    }
    return best;
  }

  void UpdateRanks() {
    // Pre-order is the reverse of post-order's parent-before-child relation:
    // walk post-order backwards.
    for (int k = PostOrder.Size - 1; k >= 0; --k) {
      const int v = PostOrder[k];
      const int pe = ParentEdge[v];
      if (pe < 0) {
        continue;
      }
      const int p = Parent[v];
      Rank[v] = E[pe].From == v ? Rank[p] - E[pe].MinLen : Rank[p] + E[pe].MinLen;
    }
  }

  bool Solve(int max_iterations) {
    BuildAdjacency();
    InitRanks();
    if (N > 1) {
      FeasibleTree();
    }
    BuildTreeAdjacency();
    LowLim();
    CutValues();
    bool completed = true;
    int start = 0;
    for (int iteration = 0;; ++iteration) {
      // dot's search: the most negative of the next 30 negative tree edges,
      // scanning cyclically from where the last search stopped.
      int leaving = -1, found = 0;
      for (int k = 0; k < E.Size && found < 30; ++k) {
        const int e = (start + k) % E.Size;
        if (TreeEdge[e] && CutValue[e] < 0) {
          ++found;
          if (leaving < 0 || CutValue[e] < CutValue[leaving]) {
            leaving = e;
          }
        }
      }
      if (leaving < 0) {
        break;
      }
      if (iteration >= max_iterations) {
        completed = false;
        break;
      }
      start = (leaving + 1) % E.Size;
      const int entering = EnterEdge(leaving);
      if (entering < 0) {
        break;
      }
      TreeEdge[leaving] = false;
      TreeEdge[entering] = true;
      BuildTreeAdjacency();
      LowLim();
      CutValues();
      UpdateRanks();
    }
    int min_rank = 0;
    for (int v = 0; v < N; ++v) {
      min_rank = v == 0 ? Rank[v] : ImMin(min_rank, Rank[v]);
    }
    for (int v = 0; v < N; ++v) {
      Rank[v] -= min_rank;
    }
    return completed;
  }
};

} // namespace

bool ImUml::NetworkSimplex(int node_count, const ImVector<ImUmlNsEdge>& edges, ImVector<int>* ranks, int max_iterations) {
  ranks->resize(node_count);
  for (int v = 0; v < node_count; ++v) {
    (*ranks)[v] = 0;
  }
  if (node_count == 0) {
    return true;
  }
  // Merge parallel edges (weights add, the largest minimum length wins) and
  // drop self-loops: the solver needs a simple graph.
  ImVector<ImUmlNsEdge> merged;
  ImGuiStorage index_of;
  for (int e = 0; e < edges.Size; ++e) {
    const ImUmlNsEdge& in = edges[e];
    if (in.From == in.To) {
      continue;
    }
    const ImGuiID key = (ImGuiID)(in.From * node_count + in.To) + 1;
    const int at = index_of.GetInt(key, 0);
    if (at == 0) {
      merged.push_back(in);
      index_of.SetInt(key, merged.Size);
    } else {
      merged[at - 1].Weight += in.Weight;
      merged[at - 1].MinLen = ImMax(merged[at - 1].MinLen, in.MinLen);
    }
  }
  // Solve each connected part on its own.
  DisjointSets sets(node_count);
  for (int e = 0; e < merged.Size; ++e) {
    sets.Join(merged[e].From, merged[e].To);
  }
  ImVector<int> local;
  local.resize(node_count);
  bool completed = true;
  for (int root = 0; root < node_count; ++root) {
    if (sets.Find(root) != root) {
      continue;
    }
    Simplex s;
    s.N = 0;
    ImVector<int> nodes;
    for (int v = 0; v < node_count; ++v) {
      if (sets.Find(v) == root) {
        local[v] = s.N++;
        nodes.push_back(v);
      }
    }
    for (int e = 0; e < merged.Size; ++e) {
      if (sets.Find(merged[e].From) == root) {
        ImUmlNsEdge le = merged[e];
        le.From = local[le.From];
        le.To = local[le.To];
        s.E.push_back(le);
      }
    }
    completed = s.Solve(max_iterations) && completed;
    for (int i = 0; i < nodes.Size; ++i) {
      (*ranks)[nodes[i]] = s.Rank[i];
    }
  }
  return completed;
}

// ── Phase 1: rank ────────────────────────────────────────────────────────────

namespace {

struct RankResult {
  ImVector<int> Ranks;
  ImVector<bool> Reversed;
  bool Doubled; ///< Every minimum length was doubled to make room for labels.
  bool Completed;
};

void ComputeRanks(const ImUmlGraphInput& in, RankResult* out) {
  const int n = in.Nodes.Size;
  out->Ranks.resize(n);
  out->Reversed.resize(in.Edges.Size);
  for (int e = 0; e < in.Edges.Size; ++e) {
    out->Reversed[e] = false;
  }
  out->Completed = true;
  out->Doubled = false;
  for (int e = 0; e < in.Edges.Size; ++e) {
    const ImUmlGraphEdge& ed = in.Edges[e];
    if (ed.Constraint && ed.From != ed.To && ed.LabelSize.x > 0 && ed.LabelSize.y > 0) {
      out->Doubled = true;
    }
  }
  const int scale = out->Doubled ? 2 : 1;

  // Nodes of one rank group are ranked as one node: its first member.
  ImVector<int> rep;
  rep.resize(n);
  ImGuiStorage first_of_group;
  for (int v = 0; v < n; ++v) {
    rep[v] = v;
    if (in.Nodes[v].RankGroup >= 0) {
      const ImGuiID key = (ImGuiID)in.Nodes[v].RankGroup + 1;
      const int first = first_of_group.GetInt(key, -1);
      if (first < 0) {
        first_of_group.SetInt(key, v);
      } else {
        rep[v] = first;
      }
    }
  }

  // Edges between representatives, then cycles broken by a depth-first
  // search in node order: an edge to a node on the stack is reversed.
  struct REdge {
    int From, To, Edge;
  };
  ImVector<REdge> redges;
  for (int e = 0; e < in.Edges.Size; ++e) {
    const ImUmlGraphEdge& ed = in.Edges[e];
    if (!ed.Constraint) {
      continue;
    }
    const int a = rep[ed.From], b = rep[ed.To];
    if (a == b) {
      continue;
    }
    REdge r = {a, b, e};
    redges.push_back(r);
  }
  ImVector<int> out_start, out_list;
  out_start.resize(n + 1);
  for (int i = 0; i <= n; ++i) {
    out_start[i] = 0;
  }
  for (int i = 0; i < redges.Size; ++i) {
    out_start[redges[i].From + 1]++;
  }
  for (int i = 0; i < n; ++i) {
    out_start[i + 1] += out_start[i];
  }
  out_list.resize(redges.Size);
  {
    ImVector<int> fill;
    fill.resize(n);
    for (int i = 0; i < n; ++i) {
      fill[i] = out_start[i];
    }
    for (int i = 0; i < redges.Size; ++i) {
      out_list[fill[redges[i].From]++] = i;
    }
  }
  ImVector<int> state; // 0 unvisited, 1 on stack, 2 done
  state.resize(n);
  for (int v = 0; v < n; ++v) {
    state[v] = 0;
  }
  ImVector<bool> reversed;
  reversed.resize(redges.Size);
  for (int i = 0; i < redges.Size; ++i) {
    reversed[i] = false;
  }
  ImVector<int> stack, next;
  next.resize(n);
  for (int start = 0; start < n; ++start) {
    if (rep[start] != start || state[start] != 0) {
      continue;
    }
    state[start] = 1;
    next[start] = out_start[start];
    stack.push_back(start);
    while (!stack.empty()) {
      const int v = stack.back();
      if (next[v] < out_start[v + 1]) {
        const int i = out_list[next[v]++];
        const int w = redges[i].To;
        if (state[w] == 1) {
          reversed[i] = true;
        } else if (state[w] == 0) {
          state[w] = 1;
          next[w] = out_start[w];
          stack.push_back(w);
        }
      } else {
        state[v] = 2;
        stack.pop_back();
      }
    }
  }

  ImVector<ImUmlNsEdge> ns;
  for (int i = 0; i < redges.Size; ++i) {
    const ImUmlGraphEdge& ed = in.Edges[redges[i].Edge];
    ImUmlNsEdge e;
    e.From = reversed[i] ? redges[i].To : redges[i].From;
    e.To = reversed[i] ? redges[i].From : redges[i].To;
    e.MinLen = ed.MinLen * scale;
    e.Weight = ed.Weight;
    ns.push_back(e);
    if (reversed[i]) {
      out->Reversed[redges[i].Edge] = true;
    }
  }
  ImVector<int> ranks;
  out->Completed = ImUml::NetworkSimplex(n, ns, &ranks, in.MaxIterations);

  // Balance (the paper's final step): a node whose in- and out-weights are
  // equal may sit anywhere in its feasible range; move it to the least
  // crowded rank there.
  {
    int max_rank = 0;
    for (int v = 0; v < n; ++v) {
      max_rank = ImMax(max_rank, ranks[v]);
    }
    ImVector<int> count;
    count.resize(max_rank + 1);
    for (int r = 0; r <= max_rank; ++r) {
      count[r] = 0;
    }
    for (int v = 0; v < n; ++v) {
      if (rep[v] == v) {
        count[ranks[v]]++;
      }
    }
    for (int v = 0; v < n; ++v) {
      if (rep[v] != v) {
        continue;
      }
      int in_w = 0, out_w = 0, lo = 0, hi = max_rank;
      bool has_in = false, has_out = false;
      for (int i = 0; i < ns.Size; ++i) {
        if (ns[i].To == v) {
          in_w += ns[i].Weight;
          lo = has_in ? ImMax(lo, ranks[ns[i].From] + ns[i].MinLen) : ranks[ns[i].From] + ns[i].MinLen;
          has_in = true;
        }
        if (ns[i].From == v) {
          out_w += ns[i].Weight;
          hi = has_out ? ImMin(hi, ranks[ns[i].To] - ns[i].MinLen) : ranks[ns[i].To] - ns[i].MinLen;
          has_out = true;
        }
      }
      if (!has_in || !has_out || in_w != out_w) {
        continue;
      }
      int best = ranks[v];
      for (int r = lo; r <= hi; r += scale) {
        if (count[r] < count[best]) {
          best = r;
        }
      }
      if (best != ranks[v]) {
        count[ranks[v]]--;
        count[best]++;
        ranks[v] = best;
      }
    }
  }

  // Pins: first / last rank, outside every unpinned node.
  bool has_first = false, any_unpinned = false;
  int min_unpinned = 0, max_unpinned = 0;
  for (int v = 0; v < n; ++v) {
    const int r = ranks[rep[v]];
    if (in.Nodes[v].Pin == 1) {
      has_first = true;
    } else if (in.Nodes[v].Pin == 0) {
      min_unpinned = any_unpinned ? ImMin(min_unpinned, r) : r;
      max_unpinned = any_unpinned ? ImMax(max_unpinned, r) : r;
      any_unpinned = true;
    }
  }
  const int shift = (has_first ? scale : 0) - min_unpinned;
  for (int v = 0; v < n; ++v) {
    const int pin = in.Nodes[v].Pin;
    if (pin == 1) {
      out->Ranks[v] = 0;
    } else if (pin == 2) {
      out->Ranks[v] = any_unpinned ? max_unpinned + shift + scale : (has_first ? scale : 0);
    } else {
      out->Ranks[v] = ranks[rep[v]] + shift;
    }
  }
  int min_rank = 0;
  for (int v = 0; v < n; ++v) {
    min_rank = v == 0 ? out->Ranks[v] : ImMin(min_rank, out->Ranks[v]);
  }
  for (int v = 0; v < n; ++v) {
    out->Ranks[v] -= min_rank;
  }
}

// ── Phases 2-4 on one connected component ────────────────────────────────────

struct LNode {
  int Orig; ///< Input node, or -1 for a virtual node.
  int Rank;
  float W, H; ///< Across / along the rank axis (top-down orientation).
  bool IsLabel;
  int Edge; ///< Virtual nodes: the input edge.
};

struct LSeg {
  int Upper, Lower; ///< LNode indices, ranks r and r + 1.
  int Weight; ///< The edge's weight.
};

struct Component {
  ImVector<int> Nodes; ///< Input node indices.
  ImVector<int> Edges; ///< Input edge indices.
};

/// The edge's layered path, top to bottom, as LNode indices.
struct Chain {
  ImVector<int> Nodes;
  int LabelNode; ///< LNode index, or -1.
  bool Flipped; ///< The input edge runs bottom to top.
  bool Flat;
  bool Loop;
};

struct ComponentLayout {
  ImVector<LNode> L;
  ImVector<LSeg> S;
  ImVector<ImVector<int>*> Layers;
  ImVector<int> Pos;
  ImVector<int> UpStart, Up, DownStart, Down;
  ImVector<int> X; ///< Centre x per LNode.
  ImVector<float> RankTop, RankHeight;
  int Crossings;
  bool Completed;
  ~ComponentLayout() {
    for (int i = 0; i < Layers.Size; ++i) {
      IM_DELETE(Layers[i]);
    }
  }
};

int CountCrossings(ComponentLayout& c) {
  int total = 0;
  struct Pair {
    int A, B;
  };
  ImVector<Pair> pairs;
  ImVector<int> tree;
  for (int r = 0; r + 1 < c.Layers.Size; ++r) {
    pairs.resize(0);
    const ImVector<int>& layer = *c.Layers[r];
    for (int i = 0; i < layer.Size; ++i) {
      const int v = layer[i];
      for (int k = c.DownStart[v]; k < c.DownStart[v + 1]; ++k) {
        Pair p = {c.Pos[v], c.Pos[c.Down[k]]};
        pairs.push_back(p);
      }
    }
    StableSort(pairs.Data, pairs.Size, [](const Pair& x, const Pair& y) {
      return x.A < y.A || (x.A == y.A && x.B < y.B);
    });
    // Inversions of B, counted with a Fenwick tree.
    const int width = c.Layers[r + 1]->Size + 1;
    tree.resize(width + 1);
    for (int i = 0; i <= width; ++i) {
      tree[i] = 0;
    }
    for (int i = 0; i < pairs.Size; ++i) {
      const int b = pairs[i].B + 1;
      int greater = i; // seen so far minus those <= b
      for (int k = b; k > 0; k -= k & -k) {
        greater -= tree[k];
      }
      total += greater;
      for (int k = b; k <= width; k += k & -k) {
        tree[k]++;
      }
    }
  }
  return total;
}

/// Crossings between the edges of v and w (v left of w), against both
/// neighbouring ranks.
int PairCrossings(const ComponentLayout& c, int v, int w) {
  int count = 0;
  for (int i = c.UpStart[v]; i < c.UpStart[v + 1]; ++i) {
    for (int j = c.UpStart[w]; j < c.UpStart[w + 1]; ++j) {
      count += c.Pos[c.Up[i]] > c.Pos[c.Up[j]] ? 1 : 0;
    }
  }
  for (int i = c.DownStart[v]; i < c.DownStart[v + 1]; ++i) {
    for (int j = c.DownStart[w]; j < c.DownStart[w + 1]; ++j) {
      count += c.Pos[c.Down[i]] > c.Pos[c.Down[j]] ? 1 : 0;
    }
  }
  return count;
}

/// The paper's weighted median of the neighbours' positions; -1 for none.
float MedianValue(const ComponentLayout& c, int v, bool upward) {
  const ImVector<int>& start = upward ? c.UpStart : c.DownStart;
  const ImVector<int>& list = upward ? c.Up : c.Down;
  const int n = start[v + 1] - start[v];
  if (n == 0) {
    return -1.0f;
  }
  ImVector<int> p;
  p.resize(n);
  for (int i = 0; i < n; ++i) {
    p[i] = c.Pos[list[start[v] + i]];
  }
  StableSort(p.Data, n, [](int a, int b) { return a < b; });
  const int m = n / 2;
  if (n % 2 == 1) {
    return (float)p[m];
  }
  if (n == 2) {
    return (p[0] + p[1]) * 0.5f;
  }
  const float left = (float)(p[m - 1] - p[0]);
  const float right = (float)(p[n - 1] - p[m]);
  if (left + right == 0.0f) {
    return (p[m - 1] + p[m]) * 0.5f;
  }
  return (p[m - 1] * right + p[m] * left) / (left + right);
}

void ReorderLayer(ComponentLayout& c, int r, bool upward) {
  ImVector<int>& layer = *c.Layers[r];
  struct Item {
    float Median;
    int Node;
    int Pos;
  };
  ImVector<Item> movable;
  ImVector<bool> fixed;
  fixed.resize(layer.Size);
  for (int i = 0; i < layer.Size; ++i) {
    const float m = MedianValue(c, layer[i], upward);
    fixed[i] = m < 0.0f;
    if (!fixed[i]) {
      Item it = {m, layer[i], i};
      movable.push_back(it);
    }
  }
  StableSort(movable.Data, movable.Size, [](const Item& a, const Item& b) {
    return a.Median < b.Median || (a.Median == b.Median && a.Pos < b.Pos);
  });
  int k = 0;
  for (int i = 0; i < layer.Size; ++i) {
    if (!fixed[i]) {
      layer[i] = movable[k++].Node;
    }
  }
  for (int i = 0; i < layer.Size; ++i) {
    c.Pos[layer[i]] = i;
  }
}

void Transpose(ComponentLayout& c) {
  for (int pass = 0; pass < 32; ++pass) {
    bool improved = false;
    for (int r = 0; r < c.Layers.Size; ++r) {
      ImVector<int>& layer = *c.Layers[r];
      for (int i = 0; i + 1 < layer.Size; ++i) {
        const int v = layer[i], w = layer[i + 1];
        if (PairCrossings(c, w, v) < PairCrossings(c, v, w)) {
          layer[i] = w;
          layer[i + 1] = v;
          c.Pos[w] = i;
          c.Pos[v] = i + 1;
          improved = true;
        }
      }
    }
    if (!improved) {
      break;
    }
  }
}

void BuildNeighbours(ComponentLayout& c) {
  const int n = c.L.Size;
  c.UpStart.resize(n + 1);
  c.DownStart.resize(n + 1);
  for (int i = 0; i <= n; ++i) {
    c.UpStart[i] = c.DownStart[i] = 0;
  }
  for (int s = 0; s < c.S.Size; ++s) {
    c.DownStart[c.S[s].Upper + 1]++;
    c.UpStart[c.S[s].Lower + 1]++;
  }
  for (int i = 0; i < n; ++i) {
    c.UpStart[i + 1] += c.UpStart[i];
    c.DownStart[i + 1] += c.DownStart[i];
  }
  c.Up.resize(c.S.Size);
  c.Down.resize(c.S.Size);
  ImVector<int> fu, fd;
  fu.resize(n);
  fd.resize(n);
  for (int i = 0; i < n; ++i) {
    fu[i] = c.UpStart[i];
    fd[i] = c.DownStart[i];
  }
  for (int s = 0; s < c.S.Size; ++s) {
    c.Down[fd[c.S[s].Upper]++] = c.S[s].Lower;
    c.Up[fu[c.S[s].Lower]++] = c.S[s].Upper;
  }
}

void Order(ComponentLayout& c) {
  const int n = c.L.Size;
  c.Pos.resize(n);
  // Initial order: depth-first from the nodes in rank order (then index),
  // following downward edges in order.
  ImVector<int> start_order;
  for (int i = 0; i < n; ++i) {
    start_order.push_back(i);
  }
  StableSort(start_order.Data, start_order.Size, [&c](int a, int b) { return c.L[a].Rank < c.L[b].Rank; });
  ImVector<bool> seen;
  seen.resize(n);
  for (int i = 0; i < n; ++i) {
    seen[i] = false;
  }
  ImVector<int> stack;
  for (int k = 0; k < start_order.Size; ++k) {
    const int s = start_order[k];
    if (seen[s]) {
      continue;
    }
    stack.push_back(s);
    while (!stack.empty()) {
      const int v = stack.back();
      stack.pop_back();
      if (seen[v]) {
        continue;
      }
      seen[v] = true;
      c.Pos[v] = c.Layers[c.L[v].Rank]->Size;
      c.Layers[c.L[v].Rank]->push_back(v);
      for (int i = c.DownStart[v + 1] - 1; i >= c.DownStart[v]; --i) {
        if (!seen[c.Down[i]]) {
          stack.push_back(c.Down[i]);
        }
      }
    }
  }
  Transpose(c);
  // The best order so far, all layers concatenated (layer sizes never change).
  ImVector<int> best;
  auto save = [&c, &best]() {
    best.resize(0);
    for (int r = 0; r < c.Layers.Size; ++r) {
      for (int i = 0; i < c.Layers[r]->Size; ++i) {
        best.push_back((*c.Layers[r])[i]);
      }
    }
  };
  save();
  int best_crossings = CountCrossings(c);
  for (int iteration = 0; iteration < 24 && best_crossings > 0; ++iteration) {
    if (iteration % 2 == 0) {
      for (int r = 1; r < c.Layers.Size; ++r) {
        ReorderLayer(c, r, true);
      }
    } else {
      for (int r = c.Layers.Size - 2; r >= 0; --r) {
        ReorderLayer(c, r, false);
      }
    }
    Transpose(c);
    const int crossings = CountCrossings(c);
    if (crossings < best_crossings) {
      best_crossings = crossings;
      save();
    }
  }
  int k = 0;
  for (int r = 0; r < c.Layers.Size; ++r) {
    for (int i = 0; i < c.Layers[r]->Size; ++i) {
      (*c.Layers[r])[i] = best[k++];
      c.Pos[(*c.Layers[r])[i]] = i;
    }
  }
  c.Crossings = best_crossings;
}

/// x by network simplex on the auxiliary graph (the paper's section 4.2).
void PositionX(ComponentLayout& c, const ImVector<int>& extra_right, const ImUmlGraphInput& in) {
  const int n = c.L.Size;
  ImVector<ImUmlNsEdge> aux;
  int count = n;
  for (int s = 0; s < c.S.Size; ++s) {
    const LNode& a = c.L[c.S[s].Upper];
    const LNode& b = c.L[c.S[s].Lower];
    const bool va = a.Orig < 0, vb = b.Orig < 0;
    const int omega = va && vb ? 8 : (va || vb ? 2 : 1);
    const int weight = omega * c.S[s].Weight;
    const int node = count++;
    ImUmlNsEdge e1 = {node, c.S[s].Upper, 0, weight};
    ImUmlNsEdge e2 = {node, c.S[s].Lower, 0, weight};
    aux.push_back(e1);
    aux.push_back(e2);
  }
  const float sep = in.NodeSpacing;
  // Least distance between the centres of neighbours a (left) and b.
  auto separation = [&](int ia, int ib) -> int {
    const LNode& a = c.L[ia];
    const LNode& b = c.L[ib];
    const bool both_virtual = a.Orig < 0 && !a.IsLabel && b.Orig < 0 && !b.IsLabel;
    const float gap = both_virtual ? sep * 0.5f : sep;
    return (int)ImCeil(a.W * 0.5f + (float)extra_right[ia] + b.W * 0.5f + gap);
  };
  for (int r = 0; r < c.Layers.Size; ++r) {
    const ImVector<int>& layer = *c.Layers[r];
    for (int i = 0; i + 1 < layer.Size; ++i) {
      ImUmlNsEdge e = {layer[i], layer[i + 1], separation(layer[i], layer[i + 1]), 0};
      aux.push_back(e);
    }
  }
  ImVector<int> x;
  c.Completed = ImUml::NetworkSimplex(count, aux, &x, in.MaxIterations) && c.Completed;
  c.X.resize(n);
  for (int i = 0; i < n; ++i) {
    c.X[i] = x[i];
  }

  // Balance (dot's final x step): where a node's cost is flat between its
  // neighbours, the solver leaves it at one end; move it to the middle of
  // its neighbours when that costs no more.
  ImVector<int> nb_start, nb, nb_w;
  nb_start.resize(n + 1);
  for (int i = 0; i <= n; ++i) {
    nb_start[i] = 0;
  }
  for (int s = 0; s < c.S.Size; ++s) {
    nb_start[c.S[s].Upper + 1]++;
    nb_start[c.S[s].Lower + 1]++;
  }
  for (int i = 0; i < n; ++i) {
    nb_start[i + 1] += nb_start[i];
  }
  nb.resize(nb_start[n]);
  nb_w.resize(nb_start[n]);
  {
    ImVector<int> fill;
    fill.resize(n);
    for (int i = 0; i < n; ++i) {
      fill[i] = nb_start[i];
    }
    for (int s = 0; s < c.S.Size; ++s) {
      const int a = c.S[s].Upper, b = c.S[s].Lower;
      const bool va = c.L[a].Orig < 0, vb = c.L[b].Orig < 0;
      const int w = (va && vb ? 8 : (va || vb ? 2 : 1)) * c.S[s].Weight;
      nb[fill[a]] = b;
      nb_w[fill[a]++] = w;
      nb[fill[b]] = a;
      nb_w[fill[b]++] = w;
    }
  }
  auto local_cost = [&](int v, int xv) {
    long long cost = 0;
    for (int k = nb_start[v]; k < nb_start[v + 1]; ++k) {
      cost += (long long)nb_w[k] * ImAbs(xv - c.X[nb[k]]);
    }
    return cost;
  };
  for (int pass = 0; pass < 2; ++pass) {
    for (int k = 0; k < c.Layers.Size; ++k) {
      const ImVector<int>& layer = *c.Layers[pass == 0 ? k : c.Layers.Size - 1 - k];
      for (int i = 0; i < layer.Size; ++i) {
        const int v = layer[i];
        if (nb_start[v] == nb_start[v + 1]) {
          continue;
        }
        int lo = -0x3fffffff, hi = 0x3fffffff, mn = 0x3fffffff, mx = -0x3fffffff;
        if (i > 0) {
          lo = c.X[layer[i - 1]] + separation(layer[i - 1], v);
        }
        if (i + 1 < layer.Size) {
          hi = c.X[layer[i + 1]] - separation(v, layer[i + 1]);
        }
        for (int q = nb_start[v]; q < nb_start[v + 1]; ++q) {
          mn = ImMin(mn, c.X[nb[q]]);
          mx = ImMax(mx, c.X[nb[q]]);
        }
        const int target = ImClamp((mn + mx) / 2, lo, ImMax(lo, hi));
        if (target != c.X[v] && local_cost(v, target) <= local_cost(v, c.X[v])) {
          c.X[v] = target;
        }
      }
    }
  }
}

ImVec2 ShapeSizeTd(const ImUmlGraphInput& in, int node) {
  const ImVec2 s = in.Nodes[node].Size;
  const bool across = in.Direction == ImUmlDirection_LeftRight || in.Direction == ImUmlDirection_RightLeft;
  return across ? ImVec2(s.y, s.x) : s;
}

ImVec2 LabelSizeTd(const ImUmlGraphInput& in, const ImVec2& s) {
  const bool across = in.Direction == ImUmlDirection_LeftRight || in.Direction == ImUmlDirection_RightLeft;
  return across ? ImVec2(s.y, s.x) : s;
}

/// Turns a polyline into a smooth chain of cubic Béziers through the same
/// points. The cross coordinate is interpolated as a monotone cubic of the
/// rank coordinate (Fritsch-Carlson, as d3's monotone curves): where the
/// path turns, the tangent runs along the rank axis, elsewhere it is the
/// harmonic mean of the two segments' slopes. So the curve never swings
/// past its neighbouring points sideways: straight runs stay straight, a
/// two-point edge is a straight line, and each turn is a single bend.
/// `along_y`: the rank axis is y (else x).
void ToBezier(const ImVector<ImVec2>& pts, ImVector<ImVec2>* out, bool along_y) {
  out->resize(0);
  const int n = pts.Size;
  out->push_back(pts[0]);
  auto u = [&](int i) { return along_y ? pts[i].y : pts[i].x; };
  auto v = [&](int i) { return along_y ? pts[i].x : pts[i].y; };
  auto make = [&](float uu, float vv) { return RoundPx(along_y ? ImVec2(vv, uu) : ImVec2(uu, vv)); };
  ImVector<float> slope, tangent;
  slope.resize(ImMax(0, n - 1));
  bool monotone = true;
  for (int i = 0; i + 1 < n; ++i) {
    const float du = u(i + 1) - u(i);
    if (ImFabs(du) < 0.5f || (i > 0 && (du > 0.0f) != (u(i) - u(i - 1) > 0.0f))) {
      monotone = false; // runs across the rank axis: no function of it
      break;
    }
    slope[i] = (v(i + 1) - v(i)) / du;
  }
  if (!monotone) {
    for (int i = 0; i + 1 < n; ++i) {
      const ImVec2 d = (pts[i + 1] - pts[i]) * (1.0f / 3.0f);
      out->push_back(RoundPx(pts[i] + d));
      out->push_back(RoundPx(pts[i + 1] - d));
      out->push_back(pts[i + 1]);
    }
    return;
  }
  tangent.resize(n);
  tangent[0] = slope[0];
  tangent[n - 1] = slope[n - 2];
  for (int i = 1; i + 1 < n; ++i) {
    const float a = slope[i - 1], b = slope[i];
    tangent[i] = a * b <= 0.0f ? 0.0f : 2.0f * a * b / (a + b);
  }
  for (int i = 0; i + 1 < n; ++i) {
    const float du = (u(i + 1) - u(i)) / 3.0f;
    out->push_back(make(u(i) + du, v(i) + tangent[i] * du));
    out->push_back(make(u(i + 1) - du, v(i + 1) - tangent[i + 1] * du));
    out->push_back(pts[i + 1]);
  }
}

} // namespace

void ImUml::SmoothPath(const ImVector<ImVec2>& pts, ImVector<ImVec2>* out, bool along_y) {
  ToBezier(pts, out, along_y);
}

bool ImUml::RankGraph(const ImUmlGraphInput& in, ImVector<int>* ranks, ImVector<bool>* reversed) {
  RankResult r;
  ComputeRanks(in, &r);
  *ranks = r.Ranks;
  *reversed = r.Reversed;
  return r.Completed;
}

ImVec2 ImUml::ClipToShape(ImUmlShape shape, const ImVec2& centre, const ImVec2& size, const ImVec2& towards) {
  const ImVec2 d = towards - centre;
  const float ax = ImFabs(d.x), ay = ImFabs(d.y);
  if (ax < 1e-6f && ay < 1e-6f) {
    return centre;
  }
  const float hw = ImMax(size.x * 0.5f, 0.5f), hh = ImMax(size.y * 0.5f, 0.5f);
  float t;
  switch (shape) {
    case ImUmlShape_Ellipse:
      t = 1.0f / ImSqrt((d.x / hw) * (d.x / hw) + (d.y / hh) * (d.y / hh));
      break;
    case ImUmlShape_Circle: {
      const float r = ImMin(hw, hh);
      t = r / ImSqrt(d.x * d.x + d.y * d.y);
      break;
    }
    case ImUmlShape_Diamond:
      t = 1.0f / (ax / hw + ay / hh);
      break;
    default:
      t = ImMin(ax > 0.0f ? hw / ax : 1e30f, ay > 0.0f ? hh / ay : 1e30f);
      break;
  }
  return centre + d * t;
}

void ImUml::LayoutGraph(const ImUmlGraphInput& in, ImUmlGraphOutput* out) {
  const int n = in.Nodes.Size;
  out->NodePos.resize(n);
  out->NodeRank.resize(n);
  out->NodeOrder.resize(n);
  out->Edges.resize(in.Edges.Size);
  out->Points.resize(0);
  out->Size = ImVec2(0, 0);
  out->Crossings = 0;
  out->ReversedEdges = 0;
  out->HitIterationCap = false;
  if (n == 0) {
    for (int e = 0; e < in.Edges.Size; ++e) {
      out->Edges[e].PointStart = out->Edges[e].PointCount = 0;
      out->Edges[e].Bezier = false;
      out->Edges[e].LabelPos = ImVec2(0, 0);
    }
    return;
  }

  RankResult rr;
  ComputeRanks(in, &rr);
  for (int e = 0; e < in.Edges.Size; ++e) {
    out->ReversedEdges += rr.Reversed[e] ? 1 : 0;
  }
  out->HitIterationCap = !rr.Completed;
  const bool doubled = rr.Doubled;
  const float rank_gap = RoundPx(doubled ? in.RankSpacing * 0.5f : in.RankSpacing);
  const bool across = in.Direction == ImUmlDirection_LeftRight || in.Direction == ImUmlDirection_RightLeft;

  // Components: joined by any edge or a shared rank group.
  DisjointSets sets(n);
  for (int e = 0; e < in.Edges.Size; ++e) {
    sets.Join(in.Edges[e].From, in.Edges[e].To);
  }
  {
    ImGuiStorage first_of_group;
    for (int v = 0; v < n; ++v) {
      if (in.Nodes[v].RankGroup >= 0) {
        const ImGuiID key = (ImGuiID)in.Nodes[v].RankGroup + 1;
        const int first = first_of_group.GetInt(key, -1);
        if (first < 0) {
          first_of_group.SetInt(key, v);
        } else {
          sets.Join(first, v);
        }
      }
    }
  }

  // Top-down positions of every node and edge point, filled per component.
  ImVector<ImVec2> td_pos;
  td_pos.resize(n);
  struct EdgePath {
    ImVector<ImVec2> Pts;
    bool Bezier;
    ImVec2 Label;
  };
  ImVector<EdgePath*> paths;
  paths.resize(in.Edges.Size);
  for (int e = 0; e < in.Edges.Size; ++e) {
    paths[e] = IM_NEW(EdgePath)();
    paths[e]->Bezier = false;
    paths[e]->Label = ImVec2(0, 0);
  }

  float offset_x = 0.0f, total_h = 0.0f;
  struct Packed {
    ImVector<int> Nodes, Edges;
    float X0, W, H;
  };
  ImVector<Packed*> packed;
  for (int root = 0; root < n; ++root) {
    if (sets.Find(root) != root) {
      continue;
    }
    Component comp;
    for (int v = 0; v < n; ++v) {
      if (sets.Find(v) == root) {
        comp.Nodes.push_back(v);
      }
    }
    for (int e = 0; e < in.Edges.Size; ++e) {
      if (sets.Find(in.Edges[e].From) == root) {
        comp.Edges.push_back(e);
      }
    }

    ComponentLayout c;
    c.Completed = true;
    ImVector<int> lnode_of;
    lnode_of.resize(n);
    int min_rank = 0x7fffffff, max_rank = 0;
    for (int i = 0; i < comp.Nodes.Size; ++i) {
      min_rank = ImMin(min_rank, rr.Ranks[comp.Nodes[i]]);
      max_rank = ImMax(max_rank, rr.Ranks[comp.Nodes[i]]);
    }
    for (int i = 0; i < comp.Nodes.Size; ++i) {
      const int v = comp.Nodes[i];
      const ImVec2 s = ShapeSizeTd(in, v);
      LNode ln = {v, rr.Ranks[v] - min_rank, ImCeil(s.x), ImCeil(s.y), false, -1};
      lnode_of[v] = c.L.Size;
      c.L.push_back(ln);
    }
    const int ranks = max_rank - min_rank + 1;
    for (int r = 0; r < ranks; ++r) {
      c.Layers.push_back(IM_NEW(ImVector<int>)());
    }

    // Virtual nodes and segments.
    ImVector<Chain*> chains;
    ImVector<int> extra_right;
    for (int k = 0; k < comp.Edges.Size; ++k) {
      const int e = comp.Edges[k];
      const ImUmlGraphEdge& ed = in.Edges[e];
      Chain* ch = IM_NEW(Chain)();
      ch->LabelNode = -1;
      ch->Flipped = false;
      ch->Flat = false;
      ch->Loop = ed.From == ed.To;
      chains.push_back(ch);
      if (ch->Loop) {
        continue;
      }
      int u = lnode_of[ed.From], v = lnode_of[ed.To];
      if (c.L[u].Rank == c.L[v].Rank) {
        ch->Flat = true;
        ch->Nodes.push_back(u);
        ch->Nodes.push_back(v);
        continue;
      }
      if (c.L[u].Rank > c.L[v].Rank) {
        const int t = u;
        u = v;
        v = t;
        ch->Flipped = true;
      }
      const bool has_label = ed.LabelSize.x > 0 && ed.LabelSize.y > 0;
      const int ru = c.L[u].Rank, rv = c.L[v].Rank;
      const int label_rank = has_label && rv - ru >= 2 ? (ru + rv) / 2 : -1;
      ch->Nodes.push_back(u);
      int prev = u;
      for (int r = ru + 1; r < rv; ++r) {
        LNode vn = {-1, r, 0.0f, 0.0f, false, e};
        if (r == label_rank) {
          const ImVec2 ls = LabelSizeTd(in, ed.LabelSize);
          vn.W = ImCeil(ls.x) + 8.0f;
          vn.H = ImCeil(ls.y);
          vn.IsLabel = true;
          ch->LabelNode = c.L.Size;
        }
        const int idx = c.L.Size;
        c.L.push_back(vn);
        LSeg sg = {prev, idx, ed.Weight};
        c.S.push_back(sg);
        ch->Nodes.push_back(idx);
        prev = idx;
      }
      LSeg sg = {prev, v, ed.Weight};
      c.S.push_back(sg);
      ch->Nodes.push_back(v);
    }
    extra_right.resize(c.L.Size);
    for (int i = 0; i < c.L.Size; ++i) {
      extra_right[i] = 0;
      // A label's room is reserved right of its edge, so the edge runs
      // through the label node's centre and stays straight (dot draws its
      // edge labels beside the edge the same way).
      if (c.L[i].IsLabel) {
        extra_right[i] = (int)c.L[i].W;
        c.L[i].W = 0.0f;
      }
    }
    const float loop_size = RoundPx(ImMax(16.0f, in.NodeSpacing * 0.6f));
    for (int k = 0; k < comp.Edges.Size; ++k) {
      if (chains[k]->Loop) {
        extra_right[lnode_of[in.Edges[comp.Edges[k]].From]] = (int)loop_size;
      }
    }
    // Flat edges join their ends in the auxiliary graph too, so they stay
    // short: modelled as a segment between the two (same-rank) nodes.
    BuildNeighbours(c);
    Order(c);
    out->Crossings += c.Crossings;
    // A node joined only by same-rank edges (a note beside its node) goes
    // right after the node of its first such edge.
    {
      ImVector<bool> moved;
      moved.resize(c.L.Size);
      for (int i = 0; i < c.L.Size; ++i) {
        moved[i] = false;
      }
      for (int k = 0; k < comp.Edges.Size; ++k) {
        const Chain& ch = *chains[k];
        if (!ch.Flat) {
          continue;
        }
        for (int side = 0; side < 2; ++side) {
          const int a = ch.Nodes[side], b = ch.Nodes[1 - side];
          const bool isolated = c.UpStart[a] == c.UpStart[a + 1] && c.DownStart[a] == c.DownStart[a + 1];
          if (!isolated || moved[a] || extra_right[a] != 0) {
            continue;
          }
          moved[a] = true;
          ImVector<int>& layer = *c.Layers[c.L[a].Rank];
          layer.erase(layer.Data + c.Pos[a]);
          int at = 0;
          for (int i = 0; i < layer.Size; ++i) {
            if (layer[i] == b) {
              at = i + 1;
            }
          }
          layer.insert(layer.Data + at, a);
          for (int i = 0; i < layer.Size; ++i) {
            c.Pos[layer[i]] = i;
          }
          break;
        }
      }
    }
    for (int k = 0; k < comp.Edges.Size; ++k) {
      if (chains[k]->Flat) {
        LSeg sg = {chains[k]->Nodes[0], chains[k]->Nodes[1], in.Edges[comp.Edges[k]].Weight};
        c.S.push_back(sg);
      }
    }
    PositionX(c, extra_right, in);
    if (!c.Completed) {
      out->HitIterationCap = true;
    }

    // y: ranks stacked.
    c.RankTop.resize(ranks);
    c.RankHeight.resize(ranks);
    float y = 0.0f;
    for (int r = 0; r < ranks; ++r) {
      float h = 0.0f;
      for (int i = 0; i < c.Layers[r]->Size; ++i) {
        h = ImMax(h, c.L[(*c.Layers[r])[i]].H);
      }
      c.RankTop[r] = y;
      c.RankHeight[r] = h;
      y += h + rank_gap;
    }
    const float height = ImMax(0.0f, y - rank_gap);

    // Bounding box across.
    float min_x = 1e30f, max_x = -1e30f;
    for (int i = 0; i < c.L.Size; ++i) {
      min_x = ImMin(min_x, (float)c.X[i] - c.L[i].W * 0.5f);
      max_x = ImMax(max_x, (float)c.X[i] + c.L[i].W * 0.5f + (float)extra_right[i]);
    }
    const float dx = RoundPx(offset_x - min_x);
    ImVector<ImVec2> lpos;
    lpos.resize(c.L.Size);
    for (int i = 0; i < c.L.Size; ++i) {
      const int r = c.L[i].Rank;
      lpos[i] = ImVec2((float)c.X[i] + dx, RoundPx(c.RankTop[r] + c.RankHeight[r] * 0.5f));
    }
    for (int i = 0; i < comp.Nodes.Size; ++i) {
      const int v = comp.Nodes[i];
      td_pos[v] = lpos[lnode_of[v]];
      out->NodeRank[v] = c.L[lnode_of[v]].Rank;
      out->NodeOrder[v] = c.Pos[lnode_of[v]];
    }

    // Phase 4: routes.
    ImVector<ImVec2> pts;
    for (int k = 0; k < comp.Edges.Size; ++k) {
      const int e = comp.Edges[k];
      const ImUmlGraphEdge& ed = in.Edges[e];
      const Chain& ch = *chains[k];
      EdgePath& path = *paths[e];
      const ImVec2 label_td = LabelSizeTd(in, ed.LabelSize);
      const bool has_label = ed.LabelSize.x > 0 && ed.LabelSize.y > 0;
      if (ch.Loop) {
        const int v = ed.From;
        const ImVec2 cp = td_pos[v];
        const ImVec2 s = ShapeSizeTd(in, v);
        const ImVec2 a = RoundPx(ImVec2(cp.x + s.x * 0.5f, cp.y - s.y * 0.25f));
        const ImVec2 b = RoundPx(ImVec2(cp.x + s.x * 0.5f, cp.y + s.y * 0.25f));
        const float reach = loop_size;
        path.Pts.push_back(a);
        path.Pts.push_back(RoundPx(ImVec2(a.x + reach, a.y - reach * 0.5f)));
        path.Pts.push_back(RoundPx(ImVec2(b.x + reach, b.y + reach * 0.5f)));
        path.Pts.push_back(b);
        path.Bezier = true;
        path.Label = RoundPx(ImVec2(a.x + reach + 4.0f + label_td.x * 0.5f, cp.y));
        continue;
      }
      const int u = ch.Nodes[0], w = ch.Nodes[ch.Nodes.Size - 1];
      const int ou = c.L[u].Orig, ow = c.L[w].Orig;
      if (ch.Flat) {
        const ImVec2 pu = lpos[u], pw = lpos[w];
        const ImVec2 su = ShapeSizeTd(in, ou), sw = ShapeSizeTd(in, ow);
        if (ImAbs(c.Pos[u] - c.Pos[w]) <= 1) {
          path.Pts.push_back(RoundPx(ClipToShape(in.Nodes[ou].Shape, pu, su, pw)));
          path.Pts.push_back(RoundPx(ClipToShape(in.Nodes[ow].Shape, pw, sw, pu)));
          path.Bezier = false;
          path.Label = RoundPx(ImVec2((pu.x + pw.x) * 0.5f, (pu.y + pw.y) * 0.5f - label_td.y * 0.5f - 4.0f));
        } else {
          // An arc over the rank, from the top of one node to the other.
          const float lift = RoundPx(ImMax(rank_gap, 20.0f) * 0.8f);
          const ImVec2 a = RoundPx(ImVec2(pu.x, pu.y - su.y * 0.5f));
          const ImVec2 b = RoundPx(ImVec2(pw.x, pw.y - sw.y * 0.5f));
          path.Pts.push_back(a);
          path.Pts.push_back(ImVec2(a.x, a.y - lift));
          path.Pts.push_back(ImVec2(b.x, b.y - lift));
          path.Pts.push_back(b);
          path.Bezier = true;
          path.Label = RoundPx(ImVec2((a.x + b.x) * 0.5f, ImMin(a.y, b.y) - lift * 0.75f - label_td.y * 0.5f));
        }
        continue;
      }
      pts.resize(0);
      for (int i = 0; i < ch.Nodes.Size; ++i) {
        const int ln = ch.Nodes[i];
        pts.push_back(lpos[ln]);
      }
      // Parallel edges with no virtual node between the same two nodes are
      // fanned apart through an offset midpoint.
      if (ch.Nodes.Size == 2) {
        int count = 0, index = 0;
        for (int j = 0; j < comp.Edges.Size; ++j) {
          const Chain& other = *chains[j];
          if (other.Loop || other.Flat || other.Nodes.Size != 2) {
            continue;
          }
          if (other.Nodes[0] == u && other.Nodes[1] == w) {
            if (j < k) {
              ++index;
            }
            ++count;
          }
        }
        if (count > 1) {
          const float offset = (index - (count - 1) * 0.5f) * ImMax(12.0f, in.NodeSpacing * 0.5f);
          const ImVec2 mid = RoundPx(ImVec2((pts[0].x + pts[1].x) * 0.5f + offset, (pts[0].y + pts[1].y) * 0.5f));
          pts.push_back(pts[1]);
          pts[1] = mid;
        }
      }
      pts[0] = RoundPx(ClipToShape(in.Nodes[ou].Shape, lpos[u], ShapeSizeTd(in, ou), pts[1]));
      pts[pts.Size - 1] =
          RoundPx(ClipToShape(in.Nodes[ow].Shape, lpos[w], ShapeSizeTd(in, ow), pts[pts.Size - 2]));
      if (ch.LabelNode >= 0) {
        const ImVec2 lp = lpos[ch.LabelNode];
        path.Label = RoundPx(ImVec2(lp.x + 6.0f + label_td.x * 0.5f, lp.y));
      } else if (has_label) {
        const ImVec2 a = pts[(pts.Size - 1) / 2], b = pts[pts.Size / 2];
        const ImVec2 m = (a + b) * 0.5f;
        path.Label = RoundPx(ImVec2(m.x + 6.0f + label_td.x * 0.5f, m.y));
      }
      if (in.EdgeStyle == ImUmlEdgeStyle_Spline) {
        ToBezier(pts, &path.Pts, true); // top-down: ranks along y
        path.Bezier = true;
      } else {
        path.Pts = pts;
        path.Bezier = false;
      }
      if (ch.Flipped) {
        for (int i = 0, j = path.Pts.Size - 1; i < j; ++i, --j) {
          const ImVec2 t = path.Pts[i];
          path.Pts[i] = path.Pts[j];
          path.Pts[j] = t;
        }
      }
    }
    for (int k = 0; k < chains.Size; ++k) {
      IM_DELETE(chains[k]);
    }
    Packed* pk = IM_NEW(Packed)();
    pk->Nodes = comp.Nodes;
    pk->Edges = comp.Edges;
    pk->X0 = offset_x;
    pk->W = RoundPx(max_x - min_x);
    pk->H = height;
    packed.push_back(pk);
    offset_x += RoundPx(max_x - min_x) + in.NodeSpacing;
    total_h = ImMax(total_h, height);
  }
  float total_w = ImMax(0.0f, offset_x - in.NodeSpacing);

  // Components are packed into rows (dot's pack) so that unconnected nodes
  // make a block of about 1.6:1 in the output, not one long row.
  if (packed.Size > 1) {
    float area = 0.0f, widest = 0.0f;
    for (int i = 0; i < packed.Size; ++i) {
      area += packed[i]->W * packed[i]->H;
      widest = ImMax(widest, packed[i]->W);
    }
    const float aspect = across ? 1.0f / 1.6f : 1.6f;
    const float row_limit = ImMax(widest, RoundPx(ImSqrt(area) * aspect));
    float x = 0.0f, y = 0.0f, row_h = 0.0f;
    total_w = 0.0f;
    for (int i = 0; i < packed.Size; ++i) {
      const Packed& pk = *packed[i];
      if (x > 0.0f && x + pk.W > row_limit) {
        y += row_h + in.RankSpacing;
        x = 0.0f;
        row_h = 0.0f;
      }
      const ImVec2 shift(x - pk.X0, y);
      for (int k = 0; k < pk.Nodes.Size; ++k) {
        td_pos[pk.Nodes[k]] += shift;
      }
      for (int k = 0; k < pk.Edges.Size; ++k) {
        EdgePath& path = *paths[pk.Edges[k]];
        for (int j = 0; j < path.Pts.Size; ++j) {
          path.Pts[j] += shift;
        }
        path.Label += shift;
      }
      x += pk.W + in.NodeSpacing;
      row_h = ImMax(row_h, pk.H);
      total_w = ImMax(total_w, x - in.NodeSpacing);
    }
    total_h = y + row_h;
  }
  for (int i = 0; i < packed.Size; ++i) {
    IM_DELETE(packed[i]);
  }

  // Transform to the requested direction.
  auto transform = [&](const ImVec2& p) -> ImVec2 {
    switch (in.Direction) {
      case ImUmlDirection_BottomUp:
        return ImVec2(p.x, total_h - p.y);
      case ImUmlDirection_LeftRight:
        return ImVec2(p.y, p.x);
      case ImUmlDirection_RightLeft:
        return ImVec2(total_h - p.y, p.x);
      default:
        return p;
    }
  };
  for (int v = 0; v < n; ++v) {
    out->NodePos[v] = transform(td_pos[v]);
  }
  for (int e = 0; e < in.Edges.Size; ++e) {
    ImUmlGraphEdgeOut& eo = out->Edges[e];
    eo.PointStart = out->Points.Size;
    eo.PointCount = paths[e]->Pts.Size;
    eo.Bezier = paths[e]->Bezier;
    eo.LabelPos = transform(paths[e]->Label);
    for (int i = 0; i < paths[e]->Pts.Size; ++i) {
      out->Points.push_back(transform(paths[e]->Pts[i]));
    }
    IM_DELETE(paths[e]);
  }
  out->Size = across ? ImVec2(total_h, total_w) : ImVec2(total_w, total_h);
}
