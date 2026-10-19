// MIT License © 2026 Binary Dice Games
/// @file charconv.hpp
/// @brief Locale-independent parsing of a `double`, as `std::from_chars`
///        does, for standard libraries without the floating-point overload
///        (Apple's libc++; spec/07_release_pipeline/DESIGN.md section 4.8).
#pragma once

#include <charconv>

namespace bdg::nymph {

/// @brief `std::from_chars(first, last, value)` for `double` (format
///        `general`): no leading whitespace or `+`; `inf`, `infinity` and
///        `nan` in any case. Uses the standard library's overload when it
///        has one (`NYMPH_HAS_FLOAT_FROM_CHARS`, a configure check),
///        else from_chars_double_fallback().
/// @return `ptr` past the number, `ec` `invalid_argument` when nothing
///         parses (`ptr == first`) or `result_out_of_range` when the value
///         is too large or too small for a `double`.
std::from_chars_result from_chars_double(const char* first, const char* last, double& value);

/// @brief The portable implementation behind from_chars_double(), built on
///        every platform so that its tests run everywhere.
std::from_chars_result from_chars_double_fallback(const char* first, const char* last, double& value);

} // namespace bdg::nymph
