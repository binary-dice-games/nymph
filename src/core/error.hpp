// MIT License © 2026 Binary Dice Games
/// @file error.hpp
/// @brief nymph::error: a problem in the source, with its position.
#pragma once

#include <stdexcept>
#include <string>

namespace bdg::nymph {

/// @brief A problem in a nymph source, reported to the user as
///        `<file>:<line>:<column>: <message>`.
///
/// Positions are 1-based and refer to the whole source file; columns count
/// characters, not bytes. A line of 0 means the problem has no position
/// (for example a size limit), and it is reported as `<file>: <message>`.
class error : public std::runtime_error {
 public:
  error(int line, int column, const std::string& message) : std::runtime_error(message), line_(line), column_(column) {}

  /// @brief 1-based line, or 0 when the error has no position.
  int line() const {
    return line_;
  }
  /// @brief 1-based column in characters, or 0.
  int column() const {
    return column_;
  }

 private:
  int line_;
  int column_;
};

} // namespace bdg::nymph
