// MIT License © 2026 Binary Dice Games
/// @file value_widgets.cpp
/// @brief Widgets that edit a value: sliders, drags, numeric and text
///        inputs, colour editors (spec/09_ui_mockups/DESIGN.md section 3,
///        "Widgets"). One class per family, registered once per ImGui name
///        (`SliderFloat` .. `SliderInt4`).
#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <memory>
#include "elements/ui/ui_common.hpp"
#include "elements/ui/ui_flags.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

/// Float limits ImGui accepts for slider ranges (it asserts beyond
/// FLT_MAX / 2).
constexpr double kFloatLimit = 1e30;
/// Integer limits that keep ImGui's range arithmetic from overflowing.
constexpr double kIntLimit = 1e9;

field_desc label_field() {
  return text_field("label", "", "Text shown beside the widget; \"##\" hides the rest (ImGui labels).");
}

/// "1, 2.5, 3" for a pick.
template <typename T>
std::string join_numbers(const T* v, int count) {
  std::string out;
  for (int i = 0; i < count; ++i) {
    out += (i == 0 ? "" : ", ") + pick_number(static_cast<double>(v[i]));
  }
  return out;
}

/// "#RRGGBB" or "#RRGGBBAA" of a float colour.
std::string hex_color(const float* c, bool alpha) {
  const auto byte = [](float v) {
    return static_cast<int>(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255.0f));
  };
  char buf[16];
  if (alpha) {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", byte(c[0]), byte(c[1]), byte(c[2]), byte(c[3]));
  } else {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", byte(c[0]), byte(c[1]), byte(c[2]));
  }
  return buf;
}

/// Grows `text` as ImGui types into it (ImGuiInputTextFlags_CallbackResize).
int resize_text(ImGuiInputTextCallbackData* data) {
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    auto* text = static_cast<std::string*>(data->UserData);
    text->resize(static_cast<size_t>(data->BufTextLen));
    data->Buf = text->data();
  }
  return 0;
}

/// An InputText over `text`, which grows up to kMaxTextBuffer bytes.
bool input_text(
    const char* label,
    std::string& text,
    ImGuiInputTextFlags flags,
    const char* hint,
    bool multiline,
    ImVec2 size) {
  // At the limit, the buffer stops growing: ImGui then refuses more input.
  const bool can_grow = text.size() + 1 < kMaxTextBuffer;
  if (can_grow) {
    flags |= ImGuiInputTextFlags_CallbackResize;
  }
  const size_t buffer_size = can_grow ? text.capacity() + 1 : text.size() + 1;
  if (!can_grow) {
    text.reserve(buffer_size);
  }
  ImGuiInputTextCallback callback = can_grow ? &resize_text : nullptr;
  void* user = can_grow ? &text : nullptr;
  if (multiline) {
    return ImGui::InputTextMultiline(label, text.data(), buffer_size, size, flags, callback, user);
  }
  if (hint != nullptr) {
    return ImGui::InputTextWithHint(label, hint, text.data(), buffer_size, flags, callback, user);
  }
  return ImGui::InputText(label, text.data(), buffer_size, flags, callback, user);
}

/// A `size` pair as an ImVec2, `fallback` when empty.
ImVec2 pair_of(const node& n, size_t index, ImVec2 fallback) {
  const auto& v = n.get<std::vector<double>>(index);
  return v.size() == 2 ? ImVec2(static_cast<float>(v[0]), static_cast<float>(v[1])) : fallback;
}

void check_pair(const element_class& cls, const node& n, size_t index, double min, double max) {
  const auto& v = n.get<std::vector<double>>(index);
  if (v.empty()) {
    return;
  }
  if (v.size() != 2) {
    cls.fail_at(n, index, "must be two numbers [width, height], not " + std::to_string(v.size()));
  }
  for (const double d : v) {
    if (!(d >= min && d <= max)) {
      cls.fail_at(n, index, "values must be between " + pick_number(min) + " and " + pick_number(max));
    }
  }
}

// ── Sliders, drags and numeric inputs ────────────────────────────────────────

enum class numeric_kind { slider, drag, input };

/// `SliderFloat`, `SliderInt3`, `DragFloat2`, `InputInt`, `InputDouble`, ...
class numeric_class : public widget_class {
 public:
  numeric_class(std::string name, numeric_kind kind, ImGuiDataType type, int count)
      : widget_class(std::move(name), {}, fields(kind, type, count), description(kind, type, count)),
        kind_(kind),
        type_(type),
        count_(count) {
    value_ = index("value");
    min_ = field_index("min");
    max_ = field_index("max");
    speed_ = field_index("speed");
    step_ = field_index("step");
    step_fast_ = field_index("step_fast");
    format_ = field_index("format");
    flags_ = index("flags");
    set_view_kind(view_kind::ui_number);
  }

  void check(const node& n) const override {
    if (count_ > 1) {
      const size_t size = integer() ? n.get<std::vector<int64_t>>(value_).size() : n.get<std::vector<double>>(value_).size();
      if (size != static_cast<size_t>(count_)) {
        fail_at(n, value_, "must be " + std::to_string(count_) + " numbers, not " + std::to_string(size));
      }
      if (!integer()) {
        for (const double d : n.get<std::vector<double>>(value_)) {
          if (!std::isfinite(d) || std::fabs(d) > kFloatLimit) {
            fail_at(n, value_, "values must be finite numbers within +-1e30");
          }
        }
      } else {
        for (const int64_t i : n.get<std::vector<int64_t>>(value_)) {
          if (std::fabs(static_cast<double>(i)) > kIntLimit) {
            fail_at(n, value_, "values must be within +-1e9");
          }
        }
      }
    }
    if (kind_ == numeric_kind::slider && bound(n, max_) < bound(n, min_)) {
      fail_at(n, static_cast<size_t>(max_), "must not be below min");
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[value_], apply);
    if (apply) {
      load(n, e, saved(n, dc));
    }
    void* data = type_ == ImGuiDataType_Float ? static_cast<void*>(e.floats.data())
        : type_ == ImGuiDataType_Double     ? static_cast<void*>(e.doubles.data())
                                            : static_cast<void*>(e.ints.data());
    const std::string& label = n.get<std::string>(0);
    const ImGuiSliderFlags slider_flags = static_cast<ImGuiSliderFlags>(n.get<int64_t>(flags_));
    const std::string format = format_ >= 0 ? n.get<std::string>(static_cast<size_t>(format_)) : std::string("%d");
    // Ranges in the widget's own type.
    float fmin = static_cast<float>(bound(n, min_)), fmax = static_cast<float>(bound(n, max_));
    int imin = static_cast<int>(bound(n, min_)), imax = static_cast<int>(bound(n, max_));
    const void* pmin = integer() ? static_cast<const void*>(&imin) : static_cast<const void*>(&fmin);
    const void* pmax = integer() ? static_cast<const void*>(&imax) : static_cast<const void*>(&fmax);

    bool changed = false;
    switch (kind_) {
      case numeric_kind::slider:
        changed = ImGui::SliderScalarN(label.c_str(), type_, data, count_, pmin, pmax, format.c_str(), slider_flags);
        break;
      case numeric_kind::drag: {
        const float speed = static_cast<float>(n.get<double>(static_cast<size_t>(speed_)));
        changed = ImGui::DragScalarN(label.c_str(), type_, data, count_, speed, pmin, pmax, format.c_str(), slider_flags);
        break;
      }
      case numeric_kind::input: {
        const ImGuiInputTextFlags input_flags = static_cast<ImGuiInputTextFlags>(n.get<int64_t>(flags_));
        float fstep = 0, fstep_fast = 0;
        double dstep = 0, dstep_fast = 0;
        int istep = 0, istep_fast = 0;
        const void* step = nullptr;
        const void* step_fast = nullptr;
        if (step_ >= 0) {
          const double s = step_value(n, step_), sf = step_value(n, step_fast_);
          fstep = static_cast<float>(s);
          fstep_fast = static_cast<float>(sf);
          dstep = s;
          dstep_fast = sf;
          istep = static_cast<int>(s);
          istep_fast = static_cast<int>(sf);
          if (s > 0) {
            step = type_ == ImGuiDataType_Float ? static_cast<const void*>(&fstep)
                : type_ == ImGuiDataType_Double ? static_cast<const void*>(&dstep)
                                                : static_cast<const void*>(&istep);
          }
          if (sf > 0) {
            step_fast = type_ == ImGuiDataType_Float ? static_cast<const void*>(&fstep_fast)
                : type_ == ImGuiDataType_Double     ? static_cast<const void*>(&dstep_fast)
                                                    : static_cast<const void*>(&istep_fast);
          }
        }
        changed = ImGui::InputScalarN(label.c_str(), type_, data, count_, step, step_fast, format.c_str(), input_flags);
        break;
      }
    }
    item_result r;
    if (changed) {
      e.touched = true;
      r.clicked = true;
    }
    e.numbers.clear();
    for (int i = 0; i < count_; ++i) {
      const size_t k = static_cast<size_t>(i);
      e.numbers.push_back(
          type_ == ImGuiDataType_Float    ? static_cast<double>(e.floats[k])
              : type_ == ImGuiDataType_Double ? e.doubles[k]
                                              : static_cast<double>(e.ints[k]));
    }
    r.values = {{"value", values(e)}};
    return r;
  }

 private:
  bool integer() const {
    return type_ == ImGuiDataType_S32;
  }

  size_t index(std::string_view name) const {
    return static_cast<size_t>(field_index(name));
  }

  /// A `min` / `max` field as a double (0 when the kind has none).
  static double bound(const node& n, int index) {
    if (index < 0) {
      return 0.0;
    }
    const value& v = n.values[static_cast<size_t>(index)];
    return std::holds_alternative<int64_t>(v) ? static_cast<double>(std::get<int64_t>(v)) : std::get<double>(v);
  }

  static double step_value(const node& n, int index) {
    return bound(n, index);
  }

  /// Copies the saved value, else the source value, into the entry.
  void load(const node& n, ui_entry& e, const element_view* saved_view) const {
    const value& v = n.values[value_];
    const bool use_saved =
        saved_view != nullptr && saved_view->numbers && saved_view->numbers->size() == static_cast<size_t>(count_);
    for (int i = 0; i < count_; ++i) {
      double d = 0.0;
      if (use_saved) {
        d = (*saved_view->numbers)[static_cast<size_t>(i)];
      } else if (const auto* one = std::get_if<double>(&v)) {
        d = *one;
      } else if (const auto* one_int = std::get_if<int64_t>(&v)) {
        d = static_cast<double>(*one_int);
      } else if (const auto* many = std::get_if<std::vector<double>>(&v)) {
        d = (*many)[static_cast<size_t>(i)];
      } else if (const auto* many_int = std::get_if<std::vector<int64_t>>(&v)) {
        d = static_cast<double>((*many_int)[static_cast<size_t>(i)]);
      }
      e.floats[static_cast<size_t>(i)] = static_cast<float>(d);
      e.doubles[static_cast<size_t>(i)] = d;
      e.ints[static_cast<size_t>(i)] = static_cast<int>(d);
    }
  }

  std::string values(const ui_entry& e) const {
    if (type_ == ImGuiDataType_Float) {
      return join_numbers(e.floats.data(), count_);
    }
    if (type_ == ImGuiDataType_Double) {
      return join_numbers(e.doubles.data(), count_);
    }
    return join_numbers(e.ints.data(), count_);
  }

  static std::vector<field_desc> fields(numeric_kind kind, ImGuiDataType type, int count) {
    const bool integer = type == ImGuiDataType_S32;
    std::vector<field_desc> f{label_field()};
    if (count == 1) {
      f.push_back(
          integer ? int_field("value", 0, "The value.", value_range{-kIntLimit, kIntLimit})
                  : number_field("value", 0.0, "The value.", value_range{-kFloatLimit, kFloatLimit}));
    } else {
      f.push_back(
          integer ? integers_field("value", "The " + std::to_string(count) + " values.")
                  : numbers_field("value", "The " + std::to_string(count) + " values."));
    }
    if (kind == numeric_kind::slider || kind == numeric_kind::drag) {
      const bool slider = kind == numeric_kind::slider;
      const std::string what = slider ? "Smallest value of the slider." : "Smallest value; min = max = 0 is unbounded.";
      if (integer) {
        f.push_back(int_field("min", 0, what, value_range{-kIntLimit, kIntLimit}));
        f.push_back(int_field(
            "max", slider ? 100 : 0, slider ? "Largest value." : "Largest value.", value_range{-kIntLimit, kIntLimit}));
      } else {
        f.push_back(number_field("min", 0.0, what, value_range{-kFloatLimit, kFloatLimit}));
        f.push_back(number_field("max", slider ? 1.0 : 0.0, "Largest value.", value_range{-kFloatLimit, kFloatLimit}));
      }
      if (!slider) {
        f.push_back(number_field("speed", 1.0, "Value change per pixel of mouse movement.", value_range{0, 1e6}));
      }
    }
    if (kind == numeric_kind::input && count == 1) {
      const double step = integer ? 1.0 : 0.0, fast = integer ? 100.0 : 0.0;
      f.push_back(number_field("step", step, "Step of the - and + buttons; 0 hides them.", value_range{0, 1e9}));
      f.push_back(number_field("step_fast", fast, "Step with Ctrl held; 0 for none.", value_range{0, 1e9}));
    }
    if (integer) {
      if (kind != numeric_kind::input) {
        f.push_back(int_format_field("format", "%d", "printf format of the shown value: one %d (like \"%d px\")."));
      }
    } else {
      f.push_back(double_format_field(
          "format",
          type == ImGuiDataType_Double ? "%.6f" : "%.3f",
          "printf format of the shown value: one %f, %e or %g (like \"%.1f ms\")."));
    }
    if (kind == numeric_kind::input) {
      f.push_back(flags_field("flags", 0, input_text_flag_names(), "ImGuiInputTextFlags without the prefix."));
    } else {
      f.push_back(flags_field("flags", 0, slider_flag_names(), "ImGuiSliderFlags without the prefix, like AlwaysClamp."));
    }
    return f;
  }

  static std::string description(numeric_kind kind, ImGuiDataType type, int count) {
    const char* what = kind == numeric_kind::slider ? "a slider" : kind == numeric_kind::drag ? "a drag box" : "an input box";
    const char* of = type == ImGuiDataType_S32 ? "integer" : "number";
    return std::string("ImGui ") + what + " for " + (count == 1 ? std::string("one ") + of
                                                                : std::to_string(count) + " " + of + "s") +
        ".";
  }

  numeric_kind kind_;
  ImGuiDataType type_;
  int count_;
  size_t value_ = 0;
  int min_ = -1, max_ = -1, speed_ = -1, step_ = -1, step_fast_ = -1, format_ = -1;
  size_t flags_ = 0;
};

// ── SliderAngle, VSlider, ranges ─────────────────────────────────────────────

class slider_angle_class : public widget_class {
 public:
  slider_angle_class()
      : widget_class(
            "SliderAngle",
            {},
            {label_field(),
             number_field("value", 0.0, "The angle in degrees.", value_range{-1e6, 1e6}),
             number_field("min", -360.0, "Smallest angle, degrees.", value_range{-1e6, 1e6}),
             number_field("max", 360.0, "Largest angle, degrees.", value_range{-1e6, 1e6}),
             double_format_field("format", "%.0f deg", "printf format of the shown degrees."),
             flags_field("flags", 0, slider_flag_names(), "ImGuiSliderFlags without the prefix.")},
            "ImGui::SliderAngle: a slider for an angle, shown in degrees.") {
    set_view_kind(view_kind::ui_number);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[1], apply);
    constexpr float kRadians = 3.14159265358979f / 180.0f;
    if (apply) {
      const element_view* v = saved(n, dc);
      const double degrees = v != nullptr && v->numbers && v->numbers->size() == 1 ? v->numbers->front() : n.get<double>(1);
      e.floats[0] = static_cast<float>(degrees) * kRadians;
    }
    item_result r;
    if (ImGui::SliderAngle(
            n.get<std::string>(0).c_str(),
            &e.floats[0],
            static_cast<float>(n.get<double>(2)),
            static_cast<float>(n.get<double>(3)),
            n.get<std::string>(4).c_str(),
            static_cast<ImGuiSliderFlags>(n.get<int64_t>(5)))) {
      e.touched = true;
      r.clicked = true;
    }
    const double degrees = std::round(e.floats[0] / kRadians * 100.0f) / 100.0f;
    e.numbers = {degrees};
    r.values = {{"value", pick_number(degrees)}};
    return r;
  }
};

/// VSliderFloat / VSliderInt.
class vslider_class : public widget_class {
 public:
  explicit vslider_class(bool integer)
      : widget_class(
            integer ? "VSliderInt" : "VSliderFloat",
            {},
            {label_field(),
             numbers_field("size", "Size [width, height] in pixels; empty is [18, 160]."),
             integer ? int_field("value", 0, "The value.", value_range{-kIntLimit, kIntLimit})
                     : number_field("value", 0.0, "The value.", value_range{-kFloatLimit, kFloatLimit}),
             integer ? int_field("min", 0, "Smallest value.", value_range{-kIntLimit, kIntLimit})
                     : number_field("min", 0.0, "Smallest value.", value_range{-kFloatLimit, kFloatLimit}),
             integer ? int_field("max", 100, "Largest value.", value_range{-kIntLimit, kIntLimit})
                     : number_field("max", 1.0, "Largest value.", value_range{-kFloatLimit, kFloatLimit}),
             integer ? int_format_field("format", "%d", "printf format of the shown value.")
                     : double_format_field("format", "%.3f", "printf format of the shown value."),
             flags_field("flags", 0, slider_flag_names(), "ImGuiSliderFlags without the prefix.")},
            integer ? "ImGui::VSliderInt: a vertical slider for an integer."
                    : "ImGui::VSliderFloat: a vertical slider for a number.",
            false),
        integer_(integer) {
    set_view_kind(view_kind::ui_number);
  }

  void check(const node& n) const override {
    check_pair(*this, n, 1, 1, 4096);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[2], apply);
    const element_view* v = apply ? saved(n, dc) : nullptr;
    const bool use_saved = v != nullptr && v->numbers && v->numbers->size() == 1;
    const ImVec2 size = pair_of(n, 1, ImVec2(18.0f, 160.0f));
    const char* label = n.get<std::string>(0).c_str();
    const char* format = n.get<std::string>(5).c_str();
    const auto flags = static_cast<ImGuiSliderFlags>(n.get<int64_t>(6));
    item_result r;
    bool changed = false;
    if (integer_) {
      if (apply) {
        e.ints[0] = use_saved ? static_cast<int>(v->numbers->front()) : static_cast<int>(n.get<int64_t>(2));
      }
      changed = ImGui::VSliderInt(
          label, size, &e.ints[0], static_cast<int>(n.get<int64_t>(3)), static_cast<int>(n.get<int64_t>(4)), format, flags);
      r.values = {{"value", pick_number(e.ints[0])}};
      e.numbers = {static_cast<double>(e.ints[0])};
    } else {
      if (apply) {
        e.floats[0] = static_cast<float>(use_saved ? v->numbers->front() : n.get<double>(2));
      }
      changed = ImGui::VSliderFloat(
          label,
          size,
          &e.floats[0],
          static_cast<float>(n.get<double>(3)),
          static_cast<float>(n.get<double>(4)),
          format,
          flags);
      r.values = {{"value", pick_number(e.floats[0])}};
      e.numbers = {static_cast<double>(e.floats[0])};
    }
    if (changed) {
      e.touched = true;
      r.clicked = true;
    }
    return r;
  }

 private:
  bool integer_;
};

/// DragFloatRange2 / DragIntRange2.
class drag_range_class : public widget_class {
 public:
  explicit drag_range_class(bool integer)
      : widget_class(
            integer ? "DragIntRange2" : "DragFloatRange2",
            {},
            {label_field(),
             integer ? integers_field("value", "The range [low, high].") : numbers_field("value", "The range [low, high]."),
             number_field("speed", 1.0, "Value change per pixel of mouse movement.", value_range{0, 1e6}),
             number_field("min", 0.0, "Smallest value; min = max = 0 is unbounded.", value_range{-kIntLimit, kIntLimit}),
             number_field("max", 0.0, "Largest value.", value_range{-kIntLimit, kIntLimit}),
             integer ? int_format_field("format", "%d", "printf format of the low value.")
                     : double_format_field("format", "%.3f", "printf format of the low value."),
             integer ? int_format_field("format_max", "", "printf format of the high value; empty is format.")
                     : double_format_field("format_max", "", "printf format of the high value; empty is format."),
             flags_field("flags", 0, slider_flag_names(), "ImGuiSliderFlags without the prefix.")},
            integer ? "ImGui::DragIntRange2: two drag boxes for a range of integers."
                    : "ImGui::DragFloatRange2: two drag boxes for a range of numbers."),
        integer_(integer) {
    set_view_kind(view_kind::ui_number);
  }

  void check(const node& n) const override {
    const size_t size =
        integer_ ? n.get<std::vector<int64_t>>(1).size() : n.get<std::vector<double>>(1).size();
    if (size != 2) {
      fail_at(n, 1, "must be two numbers [low, high], not " + std::to_string(size));
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[1], apply);
    const element_view* v = apply ? saved(n, dc) : nullptr;
    const bool use_saved = v != nullptr && v->numbers && v->numbers->size() == 2;
    const char* label = n.get<std::string>(0).c_str();
    const float speed = static_cast<float>(n.get<double>(2));
    const char* format = n.get<std::string>(5).c_str();
    const std::string& format_max = n.get<std::string>(6);
    const auto flags = static_cast<ImGuiSliderFlags>(n.get<int64_t>(7));
    item_result r;
    bool changed = false;
    if (integer_) {
      if (apply) {
        for (size_t i = 0; i < 2; ++i) {
          e.ints[i] = static_cast<int>(use_saved ? (*v->numbers)[i] : n.get<std::vector<int64_t>>(1)[i]);
        }
      }
      changed = ImGui::DragIntRange2(
          label,
          &e.ints[0],
          &e.ints[1],
          speed,
          static_cast<int>(n.get<double>(3)),
          static_cast<int>(n.get<double>(4)),
          format,
          format_max.empty() ? nullptr : format_max.c_str(),
          flags);
      r.values = {{"value", join_numbers(e.ints.data(), 2)}};
      e.numbers = {static_cast<double>(e.ints[0]), static_cast<double>(e.ints[1])};
    } else {
      if (apply) {
        for (size_t i = 0; i < 2; ++i) {
          e.floats[i] = static_cast<float>(use_saved ? (*v->numbers)[i] : n.get<std::vector<double>>(1)[i]);
        }
      }
      changed = ImGui::DragFloatRange2(
          label,
          &e.floats[0],
          &e.floats[1],
          speed,
          static_cast<float>(n.get<double>(3)),
          static_cast<float>(n.get<double>(4)),
          format,
          format_max.empty() ? nullptr : format_max.c_str(),
          flags);
      r.values = {{"value", join_numbers(e.floats.data(), 2)}};
      e.numbers = {static_cast<double>(e.floats[0]), static_cast<double>(e.floats[1])};
    }
    if (changed) {
      e.touched = true;
      r.clicked = true;
    }
    return r;
  }

 private:
  bool integer_;
};

// ── Text inputs ──────────────────────────────────────────────────────────────

enum class text_kind { plain, hint, multiline };

class input_text_class : public widget_class {
 public:
  explicit input_text_class(text_kind kind)
      : widget_class(
            kind == text_kind::plain  ? "InputText"
                : kind == text_kind::hint ? "InputTextWithHint"
                                          : "InputTextMultiline",
            {},
            fields(kind),
            kind == text_kind::plain  ? "ImGui::InputText: a one-line text box."
                : kind == text_kind::hint ? "ImGui::InputTextWithHint: a one-line text box showing a hint while empty."
                                          : "ImGui::InputTextMultiline: a text box of several lines."),
        kind_(kind) {
    size_ = field_index("size");
    hint_ = field_index("hint");
    set_view_kind(view_kind::ui_text);
  }

  void check(const node& n) const override {
    if (n.get<std::string>(1).size() >= kMaxTextBuffer) {
      fail_at(n, 1, "is longer than " + std::to_string(kMaxTextBuffer - 1) + " bytes");
    }
    if (size_ >= 0) {
      check_pair(*this, n, static_cast<size_t>(size_), 0, 4096);
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[1], apply);
    if (apply) {
      const element_view* v = saved(n, dc);
      e.text = v != nullptr && v->text && v->text->size() < kMaxTextBuffer ? *v->text : n.get<std::string>(1);
    }
    const auto flags = static_cast<ImGuiInputTextFlags>(n.get<int64_t>(static_cast<size_t>(field_index("flags"))));
    const ImVec2 size = size_ >= 0 ? pair_of(n, static_cast<size_t>(size_), ImVec2(0.0f, 0.0f)) : ImVec2(0.0f, 0.0f);
    item_result r;
    if (input_text(
            n.get<std::string>(0).c_str(),
            e.text,
            flags,
            hint_ >= 0 ? n.get<std::string>(static_cast<size_t>(hint_)).c_str() : nullptr,
            kind_ == text_kind::multiline,
            size)) {
      e.touched = true;
      r.clicked = true;
    }
    r.values = {{"value", (flags & ImGuiInputTextFlags_Password) != 0 ? std::string() : e.text}};
    return r;
  }

 private:
  static std::vector<field_desc> fields(text_kind kind) {
    std::vector<field_desc> f{label_field(), text_field("value", "", "The text in the box.")};
    if (kind == text_kind::hint) {
      f.push_back(text_field("hint", "", "Grey text shown while the box is empty."));
    }
    if (kind == text_kind::multiline) {
      f.push_back(numbers_field("size", "Size [width, height] in pixels; 0 on an axis is ImGui's default."));
    }
    f.push_back(flags_field("flags", 0, input_text_flag_names(), "ImGuiInputTextFlags without the prefix."));
    return f;
  }

  text_kind kind_;
  int size_ = -1;
  int hint_ = -1;
};

// ── Colours ──────────────────────────────────────────────────────────────────

enum class color_kind { edit3, edit4, picker3, picker4, button };

class color_class : public widget_class {
 public:
  explicit color_class(color_kind kind)
      : widget_class(
            name_of(kind),
            {},
            fields(kind),
            kind == color_kind::button ? "ImGui::ColorButton: a square of colour."
                : kind == color_kind::edit3 || kind == color_kind::edit4
                ? "ImGui colour editor: numeric fields and a colour square that opens a picker."
                : "ImGui colour picker: a saturation / value square and a hue bar.",
            kind != color_kind::button),
        kind_(kind) {
    if (kind != color_kind::button) {
      set_view_kind(view_kind::ui_color);
    }
  }

  void check(const node& n) const override {
    if (kind_ == color_kind::button) {
      check_pair(*this, n, 3, 0, 4096);
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[1], apply);
    if (apply) {
      const element_view* v = saved(n, dc);
      const std::string& text = v != nullptr && v->text ? *v->text : n.get<std::string>(1);
      const ImVec4 c = text.empty() ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : color_of(text);
      e.floats = {c.x, c.y, c.z, c.w};
    }
    const char* label = n.get<std::string>(0).c_str();
    const auto flags = static_cast<ImGuiColorEditFlags>(n.get<int64_t>(2));
    item_result r;
    bool changed = false;
    switch (kind_) {
      case color_kind::edit3:
        changed = ImGui::ColorEdit3(label, e.floats.data(), flags);
        break;
      case color_kind::edit4:
        changed = ImGui::ColorEdit4(label, e.floats.data(), flags);
        break;
      case color_kind::picker3:
        changed = ImGui::ColorPicker3(label, e.floats.data(), flags);
        break;
      case color_kind::picker4:
        changed = ImGui::ColorPicker4(label, e.floats.data(), flags);
        break;
      case color_kind::button:
        r.clicked = ImGui::ColorButton(
            label, ImVec4(e.floats[0], e.floats[1], e.floats[2], e.floats[3]), flags, pair_of(n, 3, ImVec2(0.0f, 0.0f)));
        break;
    }
    if (changed) {
      e.touched = true;
      r.clicked = true;
    }
    const bool alpha = kind_ == color_kind::edit4 || kind_ == color_kind::picker4 || e.floats[3] < 1.0f;
    const std::string hex = hex_color(e.floats.data(), alpha);
    r.values = {{"value", hex}};
    if (kind_ != color_kind::button) {
      e.text = hex;
    }
    return r;
  }

 private:
  static std::string name_of(color_kind kind) {
    switch (kind) {
      case color_kind::edit3:
        return "ColorEdit3";
      case color_kind::edit4:
        return "ColorEdit4";
      case color_kind::picker3:
        return "ColorPicker3";
      case color_kind::picker4:
        return "ColorPicker4";
      case color_kind::button:
        return "ColorButton";
    }
    return {};
  }

  static std::vector<field_desc> fields(color_kind kind) {
    std::vector<field_desc> f{
        kind == color_kind::button
            ? text_field("label", "", "ID of the button, shown in its tooltip (ImGui's desc_id).")
            : label_field(),
        color_field("value", "The colour, \"#RRGGBB\" or \"#RRGGBBAA\"; empty is white."),
        flags_field("flags", 0, color_edit_flag_names(), "ImGuiColorEditFlags without the prefix."),
    };
    if (kind == color_kind::button) {
      f.push_back(numbers_field("size", "Size [width, height] in pixels; empty is a square of the frame height."));
    }
    return f;
  }

  color_kind kind_;
};

} // namespace

void register_value_widgets(registry& reg) {
  const struct {
    const char* base;
    numeric_kind kind;
  } families[] = {{"Slider", numeric_kind::slider}, {"Drag", numeric_kind::drag}, {"Input", numeric_kind::input}};
  for (const auto& family : families) {
    for (const ImGuiDataType type : {ImGuiDataType_Float, ImGuiDataType_S32}) {
      for (int count = 1; count <= 4; ++count) {
        std::string name = std::string(family.base) + (type == ImGuiDataType_Float ? "Float" : "Int");
        if (count > 1) {
          name += std::to_string(count);
        }
        reg.add(std::make_unique<numeric_class>(std::move(name), family.kind, type, count));
      }
    }
  }
  reg.add(std::make_unique<numeric_class>("InputDouble", numeric_kind::input, ImGuiDataType_Double, 1));
  reg.add(std::make_unique<slider_angle_class>());
  reg.add(std::make_unique<vslider_class>(false));
  reg.add(std::make_unique<vslider_class>(true));
  reg.add(std::make_unique<drag_range_class>(false));
  reg.add(std::make_unique<drag_range_class>(true));
  reg.add(std::make_unique<input_text_class>(text_kind::plain));
  reg.add(std::make_unique<input_text_class>(text_kind::hint));
  reg.add(std::make_unique<input_text_class>(text_kind::multiline));
  for (const color_kind kind :
       {color_kind::edit3, color_kind::edit4, color_kind::picker3, color_kind::picker4, color_kind::button}) {
    reg.add(std::make_unique<color_class>(kind));
  }
}

} // namespace bdg::nymph::ui
