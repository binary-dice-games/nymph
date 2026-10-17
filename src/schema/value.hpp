// MIT License © 2026 Binary Dice Games
/// @file value.hpp
/// @brief The value of one element field.
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace bdg::nymph {

/// @brief A field value. Which alternative a field holds is fixed by its
///        field_type: boolean -> bool; integer, enumeration, flags ->
///        int64_t; number -> double; text, color -> std::string;
///        number_array -> std::vector<double>; integer_array ->
///        std::vector<int64_t>.
using value = std::variant<bool, int64_t, double, std::string, std::vector<double>, std::vector<int64_t>>;

} // namespace bdg::nymph
