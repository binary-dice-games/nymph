// MIT License © 2026 Binary Dice Games
/// @file imuml_tree_layout.cpp
/// @brief The tree layout of trees, mind maps and org charts
///        (spec/13_more_diagrams_and_shapes/DESIGN.md section 4).
///
/// A tidy tree after Reingold and Tilford, in the contour form of Walker and
/// Buchheim, Juenger and Leipert (2002): subtrees are packed as close as
/// their contours allow, siblings keep their written order and a parent is
/// centred over its first and last child. Extensions: nodes of any size,
/// assistants ("staff") that widen their parent's right side, org chart
/// "hanging" blocks (an indented list under a node), and a two-sided mind
/// map. A pure function: node sizes in, centres out, no ImGui context.
#include <initializer_list>
#include "imuml_internal.h"

namespace {

struct TreeWork {
  const ImUmlTreeInput* In;
  int N;
  bool Across; ///< Levels run along x (LeftRight / RightLeft).
  // Extents along the sibling axis (s) and the level axis (l).
  ImVector<float> Es, El, Ls, Rs, Ext;
  ImVector<int> Depth, HangRoot, KidStart, KidCount, Kids;
  ImVector<float> Rel; ///< s relative to the parent's centre (tidy children).
  ImVector<float> LocalS, LocalL; ///< Hanging members: s from the block root's centre, l from its card top.
  ImVector<float> StaffRel;
  ImVector<float> S, L; ///< Final centres along s and l.
  ImVector<int> CStart, CLen;
  ImVector<float> Pool; ///< Contours: for each node CLen lefts then CLen rights.
  ImVector<int> Branch;
  ImVector<unsigned char> Right; ///< Mind map: on the right of the root.
};

float Round(float v) {
  return ImFloor(v + 0.5f);
}

/// Places the children `kids[0..count)` side by side and returns their centre;
/// `Rel` of each child is set relative to it.
float PlaceKids(TreeWork& w, const int* kids, int count) {
  const float sep = w.In->NodeSpacing;
  ImVector<float> acc; // the accumulated right contour, by depth below the kids
  ImVector<float> pos;
  pos.resize(count);
  for (int k = 0; k < count; ++k) {
    const int c = kids[k];
    const int len = w.CLen[c];
    const float* cl = w.Pool.Data + w.CStart[c];
    const float* cr = cl + len;
    float x = 0.0f;
    if (k > 0) {
      x = -FLT_MAX;
      const int shared = ImMin(len, acc.Size);
      for (int j = 0; j < shared; ++j) {
        x = ImMax(x, acc[j] + sep - cl[j]);
      }
      if (shared == 0) {
        x = pos[k - 1];
      }
    }
    pos[k] = x;
    for (int j = 0; j < len; ++j) {
      if (j < acc.Size) {
        acc[j] = ImMax(acc[j], x + cr[j]);
      } else {
        acc.push_back(x + cr[j]);
      }
    }
  }
  const float centre = (pos[0] + pos[count - 1]) * 0.5f;
  for (int k = 0; k < count; ++k) {
    w.Rel[kids[k]] = pos[k] - centre;
  }
  return centre;
}

/// Stores node `v`'s contour from its children (already placed relative to v).
void StoreContour(TreeWork& w, int v, const int* kids, int count) {
  int len = 1;
  for (int k = 0; k < count; ++k) {
    len = ImMax(len, 1 + w.CLen[kids[k]]);
  }
  ImVector<float> lo, hi;
  lo.resize(len);
  hi.resize(len);
  lo[0] = -w.Ls[v];
  hi[0] = w.Rs[v];
  for (int j = 1; j < len; ++j) {
    lo[j] = FLT_MAX;
    hi[j] = -FLT_MAX;
  }
  for (int k = 0; k < count; ++k) {
    const int c = kids[k];
    const float* cl = w.Pool.Data + w.CStart[c];
    const float* cr = cl + w.CLen[c];
    for (int j = 0; j < w.CLen[c]; ++j) {
      lo[j + 1] = ImMin(lo[j + 1], w.Rel[c] + cl[j]);
      hi[j + 1] = ImMax(hi[j + 1], w.Rel[c] + cr[j]);
    }
  }
  w.CStart[v] = w.Pool.Size;
  w.CLen[v] = len;
  for (int j = 0; j < len; ++j) {
    w.Pool.push_back(lo[j]);
  }
  for (int j = 0; j < len; ++j) {
    w.Pool.push_back(hi[j]);
  }
}

} // namespace

void ImUml::LayoutTree(const ImUmlTreeInput& in, ImUmlTreeOutput* out) {
  out->Pos.resize(0);
  out->Depth.resize(0);
  out->Branch.resize(0);
  out->OnRight.resize(0);
  out->HangRoot.resize(0);
  out->Size = ImVec2(0, 0);
  const int n = in.Nodes.Size;
  if (n == 0) {
    return;
  }
  TreeWork w;
  w.In = &in;
  w.N = n;
  w.Across = in.Direction == ImUmlDirection_LeftRight || in.Direction == ImUmlDirection_RightLeft;
  for (ImVector<float>* v : {&w.Es, &w.El, &w.Ls, &w.Rs, &w.Ext, &w.Rel, &w.LocalS, &w.LocalL, &w.StaffRel, &w.S, &w.L}) {
    v->resize(n);
    for (int i = 0; i < n; ++i) {
      (*v)[i] = 0.0f;
    }
  }
  for (ImVector<int>* v : {&w.Depth, &w.HangRoot, &w.KidStart, &w.KidCount, &w.CStart, &w.CLen, &w.Branch}) {
    v->resize(n);
    for (int i = 0; i < n; ++i) {
      (*v)[i] = 0;
    }
  }
  w.Right.resize(n);
  for (int i = 0; i < n; ++i) {
    const ImVec2 sz = in.Nodes[i].Size;
    w.Es[i] = w.Across ? sz.y : sz.x;
    w.El[i] = w.Across ? sz.x : sz.y;
    w.Ls[i] = w.Rs[i] = w.Es[i] * 0.5f;
    w.Ext[i] = w.El[i];
    w.HangRoot[i] = -1;
    w.Right[i] = 1;
  }
  const int root = 0;
  const bool mind = in.Mode == ImUmlTreeMode_MindMap;

  // Roles: staff, hanging block members, tidy children.
  ImVector<int> kind; // 0 tidy child, 1 staff, 2 member
  kind.resize(n);
  for (int i = 0; i < n; ++i) {
    kind[i] = 0;
    const int p = in.Nodes[i].Parent;
    if (i == root || p < 0 || p >= i) {
      continue;
    }
    const int block = in.Nodes[p].Hanging && !mind ? p : w.HangRoot[p];
    if (block >= 0) {
      w.HangRoot[i] = block;
      kind[i] = 2;
    } else if (in.Nodes[i].Staff && !mind) {
      kind[i] = 1;
    }
  }
  // Children lists (tidy children only), in written order.
  for (int i = 0; i < n; ++i) {
    w.KidCount[i] = 0;
  }
  for (int i = 1; i < n; ++i) {
    const int p = in.Nodes[i].Parent;
    if (kind[i] == 0 && p >= 0 && p < i) {
      w.KidCount[p]++;
    }
  }
  int total = 0;
  for (int i = 0; i < n; ++i) {
    w.KidStart[i] = total;
    total += w.KidCount[i];
    w.KidCount[i] = 0;
  }
  w.Kids.resize(ImMax(total, 1));
  for (int i = 1; i < n; ++i) {
    const int p = in.Nodes[i].Parent;
    if (kind[i] == 0 && p >= 0 && p < i) {
      w.Kids[w.KidStart[p] + w.KidCount[p]++] = i;
    }
  }
  // Depth (members take their block root's, staff their parent's).
  for (int i = 1; i < n; ++i) {
    const int p = in.Nodes[i].Parent;
    if (p < 0 || p >= i) {
      continue;
    }
    w.Depth[i] = kind[i] == 2 ? w.Depth[w.HangRoot[i]] : (kind[i] == 1 ? w.Depth[p] : w.Depth[p] + 1);
  }
  // Mind map: the branch of each node, and the side.
  if (mind) {
    int branch = 0;
    for (int i = 1; i < n; ++i) {
      const int p = in.Nodes[i].Parent;
      if (p == root) {
        w.Branch[i] = branch++;
      } else if (p > 0) {
        w.Branch[i] = w.Branch[p];
      }
    }
  }
  // Hanging blocks: members stacked below the root card, indented.
  const float gap_l = ImMax(4.0f, in.RankSpacing * 0.3f), gap_s = ImMax(3.0f, in.NodeSpacing * 0.3f);
  for (int r = 0; r < n; ++r) {
    if (w.HangRoot[r] >= 0 || !in.Nodes[r].Hanging || mind) {
      continue;
    }
    float cursor = w.El[r] + gap_l, width = w.Es[r];
    bool any = false;
    // Hanging depth of a member: steps up to the block root.
    for (int j = r + 1; j < n; ++j) {
      if (w.HangRoot[j] != r) {
        continue;
      }
      int hd = 0;
      for (int a = j; a != r; a = in.Nodes[a].Parent) {
        ++hd;
      }
      const float s0 = in.Indent * (float)hd;
      w.LocalS[j] = s0 + w.Es[j] * 0.5f - w.Es[r] * 0.5f;
      w.LocalL[j] = cursor;
      width = ImMax(width, s0 + w.Es[j]);
      cursor += w.El[j] + gap_s;
      any = true;
    }
    if (any) {
      w.Rs[r] = width - w.Es[r] * 0.5f;
      w.Ext[r] = cursor - gap_s;
    }
  }
  // Staff: beside the parent, on its right; they widen its right extent.
  for (int p = 0; p < n; ++p) {
    float x = w.Es[p] * 0.5f;
    for (int i = p + 1; i < n; ++i) {
      if (kind[i] != 1 || in.Nodes[i].Parent != p) {
        continue;
      }
      w.StaffRel[i] = x + in.StaffGap + w.Es[i] * 0.5f;
      x += in.StaffGap + w.Es[i];
    }
    w.Rs[p] = ImMax(w.Rs[p], x);
  }
  w.Rel.resize(n);

  // Contours and relative positions, children before parents.
  for (int v = n - 1; v >= 0; --v) {
    if (kind[v] != 0 && v != root) {
      // Staff and members are placed from their parent / block root; they
      // are not part of the tidy tree.
      w.CStart[v] = 0;
      w.CLen[v] = 0;
      continue;
    }
    const int count = w.KidCount[v];
    const int* kids = w.Kids.Data + w.KidStart[v];
    if (v == root && mind && count > 0) {
      // Two sides: pinned children first, then a contiguous split of the
      // rest that balances the two sides' heights.
      ImVector<float> span;
      span.resize(count);
      float pinned_l = 0.0f, pinned_r = 0.0f, free_total = 0.0f;
      for (int k = 0; k < count; ++k) {
        const int c = kids[k];
        // The height of a branch: its widest level along s.
        float lo = 0.0f, hi = 0.0f;
        for (int j = 0; j < w.CLen[c]; ++j) {
          lo = ImMin(lo, w.Pool[w.CStart[c] + j]);
          hi = ImMax(hi, w.Pool[w.CStart[c] + w.CLen[c] + j]);
        }
        span[k] = hi - lo + in.NodeSpacing;
        if (in.Nodes[c].Side == ImUmlSide_Left) {
          pinned_l += span[k];
        } else if (in.Nodes[c].Side == ImUmlSide_Right) {
          pinned_r += span[k];
        } else {
          free_total += span[k];
        }
      }
      // The first `split` free children go right.
      int best_split = 0;
      float best_diff = FLT_MAX;
      for (int split = 0;; ++split) {
        float r = pinned_r, l = pinned_l;
        int seen = 0;
        for (int k = 0; k < count; ++k) {
          if (in.Nodes[kids[k]].Side >= 0) {
            continue;
          }
          (seen++ < split ? r : l) += span[k];
        }
        const float diff = ImFabs(r - l);
        if (diff < best_diff - 1e-3f) {
          best_diff = diff;
          best_split = split;
        }
        if (split >= count || (seen < split)) {
          break;
        }
      }
      (void)free_total;
      ImVector<int> right_kids, left_kids;
      int seen = 0;
      for (int k = 0; k < count; ++k) {
        const int c = kids[k];
        const int side = in.Nodes[c].Side;
        const bool right = side == ImUmlSide_Right || (side < 0 && seen++ < best_split);
        (right ? right_kids : left_kids).push_back(c);
        w.Right[c] = right ? 1 : 0;
      }
      if (right_kids.Size > 0) {
        PlaceKids(w, right_kids.Data, right_kids.Size);
      }
      if (left_kids.Size > 0) {
        PlaceKids(w, left_kids.Data, left_kids.Size);
      }
      w.CStart[v] = 0;
      w.CLen[v] = 1;
      continue;
    }
    if (count == 0) {
      w.CStart[v] = w.Pool.Size;
      w.CLen[v] = 1;
      w.Pool.push_back(-w.Ls[v]);
      w.Pool.push_back(w.Rs[v]);
      continue;
    }
    PlaceKids(w, kids, count);
    StoreContour(w, v, kids, count);
  }
  if (mind) {
    // Descendants inherit the side of their top-level branch.
    for (int i = 1; i < n; ++i) {
      const int p = in.Nodes[i].Parent;
      if (p > 0) {
        w.Right[i] = w.Right[p];
      }
    }
  }

  // Absolute positions along s.
  w.S[root] = 0.0f;
  for (int i = 1; i < n; ++i) {
    const int p = in.Nodes[i].Parent;
    if (p < 0 || p >= i) {
      continue;
    }
    if (kind[i] == 2) {
      w.S[i] = w.S[w.HangRoot[i]] + w.LocalS[i];
    } else if (kind[i] == 1) {
      w.S[i] = w.S[p] + w.StaffRel[i];
    } else {
      w.S[i] = w.S[p] + w.Rel[i];
    }
  }
  // Level bands: the tallest node of each depth.
  int depths = 0;
  for (int i = 0; i < n; ++i) {
    depths = ImMax(depths, w.Depth[i] + 1);
  }
  ImVector<float> thick, start;
  thick.resize(depths);
  start.resize(depths);
  for (int d = 0; d < depths; ++d) {
    thick[d] = 0.0f;
  }
  for (int i = 0; i < n; ++i) {
    if (kind[i] != 2) {
      thick[w.Depth[i]] = ImMax(thick[w.Depth[i]], w.Ext[i]);
    }
  }
  const bool centre_in_band = in.Mode != ImUmlTreeMode_OrgChart;
  if (mind) {
    float at = w.El[root] * 0.5f + in.RankSpacing;
    start[0] = 0.0f;
    for (int d = 1; d < depths; ++d) {
      start[d] = at;
      at += thick[d] + in.RankSpacing;
    }
  } else {
    float at = 0.0f;
    for (int d = 0; d < depths; ++d) {
      start[d] = at;
      at += thick[d] + in.RankSpacing;
    }
  }
  for (int i = 0; i < n; ++i) {
    if (kind[i] == 2) {
      continue;
    }
    const int d = w.Depth[i];
    if (mind) {
      const float l = i == root ? 0.0f : start[d] + thick[d] * 0.5f;
      w.L[i] = w.Right[i] ? l : -l;
    } else {
      const bool top = in.Nodes[i].Hanging || !centre_in_band;
      w.L[i] = start[d] + (top ? w.El[i] * 0.5f : thick[d] * 0.5f);
    }
  }
  for (int i = 0; i < n; ++i) {
    if (kind[i] == 2) {
      const int r = w.HangRoot[i];
      w.L[i] = w.L[r] - w.El[r] * 0.5f + w.LocalL[i] + w.El[i] * 0.5f;
    }
  }

  // To x, y by direction; the top-left of every node on a whole pixel.
  out->Pos.resize(n);
  ImVec2 lo(FLT_MAX, FLT_MAX), hi(-FLT_MAX, -FLT_MAX);
  for (int i = 0; i < n; ++i) {
    float x = w.S[i], y = w.L[i];
    switch (in.Direction) {
      case ImUmlDirection_BottomUp:
        y = -w.L[i];
        break;
      case ImUmlDirection_LeftRight:
        x = w.L[i];
        y = w.S[i];
        break;
      case ImUmlDirection_RightLeft:
        x = -w.L[i];
        y = w.S[i];
        break;
      default:
        break;
    }
    const ImVec2 half = in.Nodes[i].Size * 0.5f;
    const ImVec2 top_left(Round(x - half.x), Round(y - half.y));
    out->Pos[i] = top_left + half;
    lo.x = ImMin(lo.x, top_left.x);
    lo.y = ImMin(lo.y, top_left.y);
    hi.x = ImMax(hi.x, top_left.x + in.Nodes[i].Size.x);
    hi.y = ImMax(hi.y, top_left.y + in.Nodes[i].Size.y);
  }
  for (int i = 0; i < n; ++i) {
    out->Pos[i] -= lo;
  }
  out->Size = hi - lo;
  out->Depth = w.Depth;
  out->Branch = w.Branch;
  out->HangRoot = w.HangRoot;
  out->OnRight.resize(n);
  for (int i = 0; i < n; ++i) {
    out->OnRight[i] = w.Right[i];
  }
}
