
#include "optimize.h"
#include <cmath>

namespace {
  Box calculate_mean_page_box(const std::vector<Box>& boxes, bool even) {
    auto sum = Box{ };
    auto samples = 0;
    for (auto i = (even ? 0u : 1u); i < boxes.size(); i += 2, ++samples) {
      sum.llx += boxes[i].llx;
      sum.lly += boxes[i].lly;
      sum.urx += boxes[i].urx;
      sum.ury += boxes[i].ury;
    }
    if (samples < 1)
      return { };
    return {
      sum.llx / samples,
      sum.lly / samples,
      sum.urx / samples,
      sum.ury / samples,
    };
  }

  Box calculate_standard_deviation(const std::vector<Box>& boxes,
      const Box& mean_page_box, bool even) {
    auto square_sum = Box{ };
    auto samples = 0;
    const auto square = [](auto v) { return v * v; };
    for (auto i = (even ? 0u : 1u); i < boxes.size(); i += 2, ++samples) {
      const auto& box = boxes[i];
      square_sum.llx += square(mean_page_box.llx - box.llx);
      square_sum.lly += square(mean_page_box.lly - box.lly);
      square_sum.urx += square(mean_page_box.urx - box.urx);
      square_sum.ury += square(mean_page_box.ury - box.ury);
    }
    if (samples < 2)
      return { };
    return {
      std::sqrt(square_sum.llx / (samples - 1)),
      std::sqrt(square_sum.lly / (samples - 1)),
      std::sqrt(square_sum.urx / (samples - 1)),
      std::sqrt(square_sum.ury / (samples - 1))
    };
  }

  Box calculate_full_page_box(const Settings& settings,
      const std::vector<Box>& boxes, bool even) {

    const auto mean_page_box = calculate_mean_page_box(boxes, even);
    const auto standard_deviation = calculate_standard_deviation(
        boxes, mean_page_box, even);
    const auto outlier_limit = Box{
        standard_deviation.llx * settings.outlier_deviation,
        standard_deviation.lly * settings.outlier_deviation,
        standard_deviation.urx * settings.outlier_deviation,
        standard_deviation.ury * settings.outlier_deviation,
    };

    auto normal_box = calculate_mean_page_box(boxes, even);
    for (auto i = (even ? 0u : 1u); i < boxes.size(); i += 2) {
      const auto& box = boxes[i];
      if (std::abs(box.llx - mean_page_box.llx) < outlier_limit.llx)
        normal_box.llx = std::min(normal_box.llx, box.llx);
      if (std::abs(box.lly - mean_page_box.lly) < outlier_limit.lly)
        normal_box.lly = std::min(normal_box.lly, box.lly);
      if (std::abs(box.urx - mean_page_box.urx) < outlier_limit.urx)
        normal_box.urx = std::max(normal_box.urx, box.urx);
      if (std::abs(box.ury - mean_page_box.ury) < outlier_limit.ury)
        normal_box.ury = std::max(normal_box.ury, box.ury);
    }
    return normal_box;
  }

  void crop_expand_outliers(const Settings& settings,
      std::vector<Box>& boxes, bool even) {

    const auto full_page_box = calculate_full_page_box(settings, boxes, even);
    for (auto i = (even ? 0u : 1u); i < boxes.size(); i += 2) {
      auto& box = boxes[i];
      if (settings.crop_outlier) {
        box.llx = std::max(box.llx, full_page_box.llx);
        box.lly = std::max(box.lly, full_page_box.lly);
        box.urx = std::min(box.urx, full_page_box.urx);
        box.ury = std::min(box.ury, full_page_box.ury);
      }
      if (settings.expand_outlier) {
        box.llx = std::min(box.llx, full_page_box.llx);
        box.lly = std::min(box.lly, full_page_box.lly);
        box.urx = std::max(box.urx, full_page_box.urx);
        box.ury = std::max(box.ury, full_page_box.ury);
      }
    }
  }

  void apply_margins(const Settings& settings,
      std::vector<Box>& boxes, bool even) {

    for (auto i = (even ? 0u : 1u); i < boxes.size(); i += 2) {
      auto& box = boxes[i];
      box.llx -= settings.margin_left;
      box.lly -= settings.margin_bottom;
      box.urx += settings.margin_right;
      box.ury += settings.margin_top;
      if (even) {
        box.llx -= settings.margin_inner;
        box.urx += settings.margin_outer;
      }
      else {
        box.llx -= settings.margin_outer;
        box.urx += settings.margin_inner;
      }
    }
  }
} // namespace

void optimize_boxes(const Settings& settings, std::vector<Box>& boxes) {
  if (settings.crop_outlier &&
      settings.expand_outlier) {
    crop_expand_outliers(settings, boxes, true);
    crop_expand_outliers(settings, boxes, false);
  }

  apply_margins(settings, boxes, true);
  apply_margins(settings, boxes, false);
}
