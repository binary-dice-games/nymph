// MIT License © 2026 Binary Dice Games
/// @file suggest.hpp
/// @brief "Did you mean" suggestions for error messages.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

/// @brief Levenshtein distance between `a` and `b`.
inline size_t edit_distance(std::string_view a, std::string_view b) {
  std::vector<size_t> row(b.size() + 1);
  for (size_t j = 0; j <= b.size(); ++j) {
    row[j] = j;
  }
  for (size_t i = 1; i <= a.size(); ++i) {
    size_t diagonal = row[0];
    row[0] = i;
    for (size_t j = 1; j <= b.size(); ++j) {
      const size_t above = row[j];
      row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0u : 1u)});
      diagonal = above;
    }
  }
  return row[b.size()];
}

/// @brief " (did you mean 'x'?)" for the closest candidate within two edits
///        (and closer than its own length), else "".
template <typename Range>
std::string did_you_mean(std::string_view word, const Range& candidates) {
  std::string best;
  size_t best_distance = 3;
  for (const auto& candidate : candidates) {
    const std::string_view c(candidate);
    const size_t d = edit_distance(word, c);
    if (d < best_distance && d < c.size()) {
      best_distance = d;
      best = std::string(c);
    }
  }
  return best.empty() ? "" : " (did you mean '" + best + "'?)";
}

} // namespace bdg::nymph
